//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "tf_gamestats.h"
#include "KeyValues.h"
#include "viewport_panel_names.h"
#include "client.h"
#include "team.h"
#include "tf_weaponbase.h"
#include "tf_client.h"
#include "tf_team.h"
#include "tf_viewmodel.h"
#include "tf_item.h"
#include "in_buttons.h"
#include "entity_capture_flag.h"
#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "game.h"
#include "tf_weapon_builder.h"
#include "tf_obj.h"
#include "tf_ammo_pack.h"
#include "datacache/imdlcache.h"
#include "particle_parse.h"
#include "props_shared.h"
#include "filesystem.h"
#include "toolframework_server.h"
#include "IEffects.h"
#include "func_respawnroom.h"
#include "networkstringtable_gamedll.h"
#include "team_control_point_master.h"
#include "tf_weapon_pda.h"
#include "sceneentity.h"
#include "fmtstr.h"
#include "tf_weapon_sniperrifle.h"
#include "tf_weapon_minigun.h"
#include "trigger_area_capture.h"
#include "triggers.h"
#include "tf_weapon_medigun.h"
#include "hl2orange.spa.h"
#include "te_tfblood.h"
#include "activitylist.h"
#include "steam/steam_api.h"
#include "cdll_int.h"
#include "tf_weaponbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DAMAGE_FORCE_SCALE_SELF				9

extern bool IsInCommentaryMode( void );

extern ConVar	sk_player_head;
extern ConVar	sk_player_chest;
extern ConVar	sk_player_stomach;
extern ConVar	sk_player_arm;
extern ConVar	sk_player_leg;

extern ConVar	tf_spy_invis_time;
extern ConVar	tf_spy_invis_unstealth_time;
extern ConVar	tf_stalematechangeclasstime;

EHANDLE g_pLastSpawnPoints[TF_TEAM_COUNT];

ConVar tf_playerstatetransitions( "tf_playerstatetransitions", "-2", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "tf_playerstatetransitions <ent index or -1 for all>. Show player state transitions." );
ConVar tf_playergib( "tf_playergib", "1", FCVAR_PROTECTED, "Allow player gibbing." );

ConVar tf_weapon_ragdoll_velocity_min( "tf_weapon_ragdoll_velocity_min", "100", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_weapon_ragdoll_velocity_max( "tf_weapon_ragdoll_velocity_max", "150", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_weapon_ragdoll_maxspeed( "tf_weapon_ragdoll_maxspeed", "300", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_damageforcescale_other( "tf_damageforcescale_other", "6.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_self_soldier( "tf_damageforcescale_self_soldier", "10.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damagescale_self_soldier( "tf_damagescale_self_soldier", "0.60", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_damage_lineardist( "tf_damage_lineardist", "0", FCVAR_DEVELOPMENTONLY );
ConVar tf_damage_range( "tf_damage_range", "0.5", FCVAR_DEVELOPMENTONLY );

ConVar tf_max_voice_speak_delay( "tf_max_voice_speak_delay", "1.5", FCVAR_DEVELOPMENTONLY, "Max time after a voice command until player can do another one" );

extern ConVar spec_freeze_time;
extern ConVar spec_freeze_traveltime;
extern ConVar sv_maxunlag;
extern ConVar tf_damage_disablespread;
extern ConVar tf_gravetalk;
extern ConVar tf_spectalk;

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
	{
		m_iPlayerIndex = TF_PLAYER_INDEX_NONE;
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
	// BUGBUG:  ywb  we assume this is either 0 or an animation sequence #, but it could also be an activity, which should fit within this limit, but we're not guaranteed.
	SendPropInt( SENDINFO( m_nData ), ANIMATION_SEQUENCE_BITS ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
    Vector vecEyePos = pPlayer->EyePosition();
	CPVSFilter filter( vecEyePos );
	if ( !IsCustomPlayerAnimEvent( event ) && ( event != PLAYERANIMEVENT_SNAP_YAW ) && ( event != PLAYERANIMEVENT_VOICE_COMMAND_GESTURE ) )
	{
		filter.RemoveRecipient( pPlayer );
	}

	Assert( pPlayer->entindex() >= 1 && pPlayer->entindex() <= MAX_PLAYERS );
	g_TEPlayerAnimEvent.m_iPlayerIndex = pPlayer->entindex();
	g_TEPlayerAnimEvent.m_iEvent = event;
	Assert( nData < (1<<ANIMATION_SEQUENCE_BITS) );
	Assert( (1<<ANIMATION_SEQUENCE_BITS) >= ActivityList_HighestIndex() );
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

//=================================================================================
//
// Ragdoll Entity
//
class CTFRagdoll : public CBaseAnimatingOverlay
{
public:

	DECLARE_CLASS( CTFRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	CTFRagdoll()
	{
		m_iPlayerIndex.Set( TF_PLAYER_INDEX_NONE );
		m_bGib = false;
		m_bBurning = false;
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar( bool, m_bGib );
	CNetworkVar( bool, m_bBurning );
	CNetworkVar( int, m_iTeam );
	CNetworkVar( int, m_iClass );
};

LINK_ENTITY_TO_CLASS( tf_ragdoll, CTFRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTFRagdoll, DT_TFRagdoll )
	SendPropVector( SENDINFO( m_vecRagdollOrigin ), -1,  SPROP_COORD ),
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ), 13, SPROP_ROUNDDOWN, -2048.0f, 2048.0f ),
	SendPropInt( SENDINFO( m_nForceBone ) ),
	SendPropBool( SENDINFO( m_bGib ) ),
	SendPropBool( SENDINFO( m_bBurning ) ),
	SendPropInt( SENDINFO( m_iTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iClass ), 4, SPROP_UNSIGNED ),	
END_SEND_TABLE()

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: Filters updates to a variable so that only non-local players see
// the changes.  This is so we can send a low-res origin to non-local players
// while sending a hi-res one to the local player.
// Input  : *pVarData - 
//			*pOut - 
//			objectID - 
//-----------------------------------------------------------------------------

void* SendProxy_SendNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	pRecipients->SetAllRecipients();
	pRecipients->ClearRecipient( objectID - 1 );
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendNonLocalDataTable );

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the UtlVector list of objects to entindexes, where it's reassembled on the client
//-----------------------------------------------------------------------------
void SendProxy_PlayerObjectList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;

	// If this fails, then SendProxyArrayLength_PlayerObjects didn't work.
	Assert( iElement < pPlayer->GetObjectCount() );

	CBaseObject *pObject = pPlayer->GetObject(iElement);

	EHANDLE hObject;
	hObject = pObject;

	SendProxy_EHandleToInt( pProp, pStruct, &hObject, pOut, iElement, objectID );
}

int SendProxyArrayLength_PlayerObjects( const void *pStruct, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;
	int iObjects = pPlayer->GetObjectCount();
	Assert( iObjects <= MAX_OBJECTS_PER_PLAYER );
	return iObjects;
}

BEGIN_DATADESC( CTFPlayer )
END_DATADESC()
extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropArray2( 
		SendProxyArrayLength_PlayerObjects,
		SendPropInt("player_object_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_PlayerObjectList), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"
		),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
//	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFNonLocalPlayerExclusive )
	// send a lo-res origin to other players
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

END_SEND_TABLE()


//============

LINK_ENTITY_TO_CLASS( player, CTFPlayer );
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST( CTFPlayer, DT_TFPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseEntity", "m_nModelIndex" ),
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),

	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),
	SendPropExclude( "DT_BaseFlex", "m_blinktoggle" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),

	SendPropBool(SENDINFO(m_bSaveMeParity)),

	// This will create a race condition will the local player, but the data will be the same so.....
	SendPropInt( SENDINFO( m_nWaterLevel ), 2, SPROP_UNSIGNED ),

	SendPropEHandle(SENDINFO(m_hItem)),

	// Ragdoll.
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),

	SendPropDataTable( SENDINFO_DT( m_PlayerClass ), &REFERENCE_SEND_TABLE( DT_TFPlayerClassShared ) ),
	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_TFPlayerShared ) ),

	// Data that only gets sent to the local player
	SendPropDataTable( "tflocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

	// Data that gets sent to all other players
	SendPropDataTable( "tfnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable ),

	SendPropBool( SENDINFO( m_iSpawnCounter ) ),

END_SEND_TABLE()



// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

// Hint callbacks
bool HintCallbackNeedsResources_Sentrygun( CBasePlayer *pPlayer )
{
	return ( pPlayer->GetAmmoCount( TF_AMMO_METAL ) > CalculateObjectCost( OBJ_SENTRYGUN ) );
}
bool HintCallbackNeedsResources_Dispenser( CBasePlayer *pPlayer )
{
	return ( pPlayer->GetAmmoCount( TF_AMMO_METAL ) > CalculateObjectCost( OBJ_DISPENSER ) );
}
bool HintCallbackNeedsResources_Teleporter( CBasePlayer *pPlayer )
{
	return ( pPlayer->GetAmmoCount( TF_AMMO_METAL ) > CalculateObjectCost( OBJ_TELEPORTER_ENTRANCE ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer::CTFPlayer()
{
	m_PlayerAnimState = CreateTFPlayerAnimState( this );
	item_list = 0;

	SetArmorValue( 10 );

	m_hItem = NULL;
	m_hTauntScene = NULL;

	UseClientSideAnimation();
	m_angEyeAngles.Init();
	m_pStateInfo = NULL;
	m_lifeState = LIFE_DEAD; // Start "dead".
	m_iMaxSentryKills = 0;
	m_flNextNameChangeTime = 0;

	m_flNextTimeCheck = gpGlobals->curtime;
	m_flSpawnTime = 0;

	SetViewOffset( TF_PLAYER_VIEW_OFFSET );

	m_Shared.Init( this );

	m_iLastSkin = -1;

	m_bHudClassAutoKill = false;
	m_bMedigunAutoHeal = false;

	m_vecLastDeathPosition = Vector( FLT_MAX, FLT_MAX, FLT_MAX );

	SetDesiredPlayerClassIndex( TF_CLASS_UNDEFINED );

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );

	ResetScores();

	m_flLastAction = gpGlobals->curtime;

	m_bInitTaunt = false;

	m_bSpeakingConceptAsDisguisedSpy = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFPlayerThink()
{
	if ( m_pStateInfo && m_pStateInfo->pfnThink )
	{
		(this->*m_pStateInfo->pfnThink)();
	}

	// Time to finish the current random expression? Or time to pick a new one?
	if ( IsAlive() && m_flNextRandomExpressionTime >= 0 && gpGlobals->curtime > m_flNextRandomExpressionTime )
	{
		// Random expressions need to be cleared, because they don't loop. So if we
		// pick the same one again, we want to restart it.
		ClearExpression();
		m_iszExpressionScene = NULL_STRING;
		UpdateExpression();
	}

	// Check to see if we are in the air and taunting.  Stop if so.
	if ( GetGroundEntity() == NULL && m_Shared.InCond( TF_COND_TAUNTING ) )
	{
		if( m_hTauntScene.Get() )
		{
			StopScriptedScene( this, m_hTauntScene );
			m_Shared.m_flTauntRemoveTime = 0.0f;
			m_hTauntScene = NULL;
		}
	}

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::MedicRegenThink( void )
{
	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		if ( IsAlive() )
		{
			// Heal faster if we haven't been in combat for a while
			float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageTime();
			float flScale = RemapValClamped( flTimeSinceDamage, 5, 10, 1.0, 3.0 );

			int iHealAmount = ceil(TF_MEDIC_REGEN_AMOUNT * flScale);
			TakeHealth( iHealAmount, DMG_GENERIC );
		}

		SetContextThink( &CTFPlayer::MedicRegenThink, gpGlobals->curtime + TF_MEDIC_REGEN_TIME, "MedicRegenThink" );
	}
}

CTFPlayer::~CTFPlayer()
{
	DestroyRagdoll();
	m_PlayerAnimState->Release();
}


CTFPlayer *CTFPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CTFPlayer::s_PlayerEdict = ed;
	return (CTFPlayer*)CreateEntityByName( className );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateTimers( void )
{
	m_Shared.ConditionThink();
	m_Shared.InvisibilityThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PreThink()
{
	// Update timers.
	UpdateTimers();

	// Pass through to the base class think.
	BaseClass::PreThink();

	// Reset bullet force accumulator, only lasts one frame, for ragdoll forces from multiple shots.
	m_vecTotalBulletForce = vec3_origin;

	CheckForIdle();
}

ConVar mp_idledealmethod( "mp_idledealmethod", "1", FCVAR_GAMEDLL, "Deals with Idle Players. 1 = Sends them into Spectator mode then kicks them if they're still idle, 2 = Kicks them out of the game;" );
ConVar mp_idlemaxtime( "mp_idlemaxtime", "3", FCVAR_GAMEDLL, "Maximum time a player is allowed to be idle (in minutes)" );

void CTFPlayer::CheckForIdle( void )
{
	if ( m_afButtonLast != m_nButtons )
		m_flLastAction = gpGlobals->curtime;

	if ( mp_idledealmethod.GetInt() )
	{
		if ( IsHLTV() )
			return;

		if ( IsFakeClient() )
			return;

		//Don't mess with the host on a listen server (probably one of us debugging something)
		if ( engine->IsDedicatedServer() == false && entindex() == 1 )
			return;

		if ( m_bIsIdle == false )
		{
			if ( StateGet() == TF_STATE_OBSERVER || StateGet() != TF_STATE_ACTIVE )
				return;
		}
		
		float flIdleTime = mp_idlemaxtime.GetFloat() * 60;

		if ( TFGameRules()->InStalemate() )
		{
			flIdleTime = mp_stalemate_timelimit.GetInt() * 0.5f;
		}
		
		if ( (gpGlobals->curtime - m_flLastAction) > flIdleTime  )
		{
			bool bKickPlayer = false;

			ConVarRef mp_allowspectators( "mp_allowspectators" );
			if ( mp_allowspectators.IsValid() && ( mp_allowspectators.GetBool() == false ) )
			{
				// just kick the player if this server doesn't allow spectators
				bKickPlayer = true;
			}
			else if ( mp_idledealmethod.GetInt() == 1 )
			{
				//First send them into spectator mode then kick him.
				if ( m_bIsIdle == false )
				{
					ForceChangeTeam( TEAM_SPECTATOR );
					m_flLastAction = gpGlobals->curtime;
					m_bIsIdle = true;
					return;
				}
				else
				{
					bKickPlayer = true;
				}
			}
			else if ( mp_idledealmethod.GetInt() == 2 )
			{
				bKickPlayer = true;
			}

			if ( bKickPlayer == true )
			{
				UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#game_idle_kick", GetPlayerName() );
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", GetUserID() ) );
				m_flLastAction = gpGlobals->curtime;
			}
		}
	}
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CTFPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOff( void )
{
	if( IsEffectActive(EF_DIMLIGHT) )
	{
		RemoveEffects( EF_DIMLIGHT );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Precache()
{
	// Precache the player models and gibs.
	PrecachePlayerModels();

	// Precache the player sounds.
	PrecacheScriptSound( "Player.Spawn" );
	PrecacheScriptSound( "TFPlayer.Pain" );
	PrecacheScriptSound( "TFPlayer.CritHit" );
	PrecacheScriptSound( "TFPlayer.CritPain" );
	PrecacheScriptSound( "TFPlayer.CritDeath" );
	PrecacheScriptSound( "TFPlayer.FreezeCam" );
	PrecacheScriptSound( "TFPlayer.Drown" );
	PrecacheScriptSound( "TFPlayer.AttackerPain" );
	PrecacheScriptSound( "TFPlayer.SaveMe" );
	PrecacheScriptSound( "Camera.SnapShot" );

	PrecacheScriptSound( "Game.YourTeamLost" );
	PrecacheScriptSound( "Game.YourTeamWon" );
	PrecacheScriptSound( "Game.SuddenDeath" );
	PrecacheScriptSound( "Game.Stalemate" );
	PrecacheScriptSound( "TV.Tune" );

	// Precache particle systems
	PrecacheParticleSystem( "crit_text" );
	PrecacheParticleSystem( "cig_smoke" );
	PrecacheParticleSystem( "speech_mediccall" );
	PrecacheParticleSystem( "player_recent_teleport_blue" );
	PrecacheParticleSystem( "player_recent_teleport_red" );
	PrecacheParticleSystem( "particle_nemesis_red" );
	PrecacheParticleSystem( "particle_nemesis_blue" );
	PrecacheParticleSystem( "spy_start_disguise_red" );
	PrecacheParticleSystem( "spy_start_disguise_blue" );
	PrecacheParticleSystem( "burningplayer_red" );
	PrecacheParticleSystem( "burningplayer_blue" );
	PrecacheParticleSystem( "blood_spray_red_01" );
	PrecacheParticleSystem( "blood_spray_red_01_far" );
	PrecacheParticleSystem( "water_blood_impact_red_01" );
	PrecacheParticleSystem( "blood_impact_red_01" );
	PrecacheParticleSystem( "water_playerdive" );
	PrecacheParticleSystem( "water_playeremerge" );
					 
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Precache the player models and player model gibs.
//-----------------------------------------------------------------------------
void CTFPlayer::PrecachePlayerModels( void )
{
	int i;
	for ( i = 0; i < TF_CLASS_COUNT_ALL; i++ )
	{
		const char *pszModel = GetPlayerClassData( i )->m_szModelName;
		if ( pszModel && pszModel[0] )
		{
			int iModel = PrecacheModel( pszModel );
			PrecacheGibsForModel( iModel );
		}

		if ( !IsX360() )
		{
			// Precache the hardware facial morphed models as well.
			const char *pszHWMModel = GetPlayerClassData( i )->m_szHWMModelName;
			if ( pszHWMModel && pszHWMModel[0] )
			{
				PrecacheModel( pszHWMModel );
			}
		}
	}
	
	if ( TFGameRules() && TFGameRules()->IsBirthday() )
	{
		for ( i = 1; i < ARRAYSIZE(g_pszBDayGibs); i++ )
		{
			PrecacheModel( g_pszBDayGibs[i] );
		}
		PrecacheModel( "models/effects/bday_hat.mdl" );
	}

	// Precache player class sounds
	for ( i = TF_FIRST_NORMAL_CLASS; i < TF_CLASS_COUNT_ALL; ++i )
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( i );

		PrecacheScriptSound( pData->m_szDeathSound );
		PrecacheScriptSound( pData->m_szCritDeathSound );
		PrecacheScriptSound( pData->m_szMeleeDeathSound );
		PrecacheScriptSound( pData->m_szExplosionDeathSound );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsReadyToPlay( void )
{
	return ( ( GetTeamNumber() > LAST_SHARED_TEAM ) &&
			 ( GetDesiredPlayerClassIndex() > TF_CLASS_UNDEFINED ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsReadyToSpawn( void )
{
	if ( IsClassMenuOpen() )
	{
		return false;
	}

	return ( StateGet() != TF_STATE_DYING );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player should be allowed to instantly spawn
//			when they next finish picking a class.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldGainInstantSpawn( void )
{
	return ( GetPlayerClass()->GetClassIndex() == TF_CLASS_UNDEFINED || IsClassMenuOpen() );
}

//-----------------------------------------------------------------------------
// Purpose: Resets player scores
//-----------------------------------------------------------------------------
void CTFPlayer::ResetScores( void )
{
	CTF_GameStats.ResetPlayerStats( this );
	RemoveNemesisRelationships();
	BaseClass::ResetScores();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	SetWeaponBuilder( NULL );

	m_iMaxSentryKills = 0;
	CTF_GameStats.Event_MaxSentryKills( this, 0 );

	StateEnter( TF_STATE_WELCOME );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Spawn()
{
	MDLCACHE_CRITICAL_SECTION();

	m_flSpawnTime = gpGlobals->curtime;
	UpdateModel();

	SetMoveType( MOVETYPE_WALK );
	BaseClass::Spawn();

	// Create our off hand viewmodel if necessary
	CreateViewModel( 1 );
	// Make sure it has no model set, in case it had one before
	GetViewModel(1)->SetModel( "" );

	// Kind of lame, but CBasePlayer::Spawn resets a lot of the state that we initially want on.
	// So if we're in the welcome state, call its enter function to reset 
	if ( m_Shared.InState( TF_STATE_WELCOME ) )
	{
		StateEnterWELCOME();
	}

	// If they were dead, then they're respawning. Put them in the active state.
	if ( m_Shared.InState( TF_STATE_DYING ) )
	{
		StateTransition( TF_STATE_ACTIVE );
	}

	// If they're spawning into the world as fresh meat, give them items and stuff.
	if ( m_Shared.InState( TF_STATE_ACTIVE ) )
	{
		// remove our disguise each time we spawn
		if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			m_Shared.RemoveDisguise();
		}

		EmitSound( "Player.Spawn" );
		InitClass();
		m_Shared.RemoveAllCond( NULL ); // Remove conc'd, burning, rotting, hallucinating, etc.

		UpdateSkin( GetTeamNumber() );
		TeamFortress_SetSpeed();

		// Prevent firing for a second so players don't blow their faces off
		SetNextAttack( gpGlobals->curtime + 1.0 );

		DoAnimationEvent( PLAYERANIMEVENT_SPAWN );

		// Force a taunt off, if we are still taunting, the condition should have been cleared above.
		if( m_hTauntScene.Get() )
		{
			StopScriptedScene( this, m_hTauntScene );
			m_Shared.m_flTauntRemoveTime = 0.0f;
			m_hTauntScene = NULL;
		}

		// turn on separation so players don't get stuck in each other when spawned
		m_Shared.SetSeparation( true );
		m_Shared.SetSeparationVelocity( vec3_origin );

		RemoveTeleportEffect();
	
		//If this is true it means I respawned without dying (changing class inside the spawn room) but doesn't necessarily mean that my healers have stopped healing me
		//This means that medics can still be linked to me but my health would not be affected since this condition is not set.
		//So instead of going and forcing every healer on me to stop healing we just set this condition back on. 
		//If the game decides I shouldn't be healed by someone (LOS, Distance, etc) they will break the link themselves like usual.
		if ( m_Shared.GetNumHealers() > 0 )
		{
			m_Shared.AddCond( TF_COND_HEALTH_BUFF );
		}

		if ( !m_bSeenRoundInfo )
		{
			TFGameRules()->ShowRoundInfoPanel( this );
			m_bSeenRoundInfo = true;
		}

		if ( IsInCommentaryMode() && !IsFakeClient() )
		{
			// Player is spawning in commentary mode. Tell the commentary system.
			CBaseEntity *pEnt = NULL;
			variant_t emptyVariant;
			while ( (pEnt = gEntList.FindEntityByClassname( pEnt, "commentary_auto" )) != NULL )
			{
				pEnt->AcceptInput( "MultiplayerSpawned", this, this, emptyVariant, 0 );
			}
		}
	}

	CTF_GameStats.Event_PlayerSpawned( this );

	m_iSpawnCounter = !m_iSpawnCounter;
	m_bAllowInstantSpawn = false;

	m_Shared.SetSpyCloakMeter( 100.0f );

	m_Shared.ClearDamageEvents();
	ClearDamagerHistory();

	m_flLastDamageTime = 0;

	m_flNextVoiceCommandTime = gpGlobals->curtime;

	ClearZoomOwner();
	SetFOV( this , 0 );

	SetViewOffset( GetClassEyeHeight() );

	ClearExpression();
	m_flNextSpeakWeaponFire = gpGlobals->curtime;

	m_bIsIdle = false;
	m_flPowerPlayTime = 0.0;

	// This makes the surrounding box always the same size as the standing collision box
	// helps with parts of the hitboxes that extend out of the crouching hitbox, eg with the
	// heavyweapons guy
	Vector mins = VEC_HULL_MIN;
	Vector maxs = VEC_HULL_MAX;
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &mins, &maxs );
}

//-----------------------------------------------------------------------------
// Purpose: Removes all nemesis relationships between this player and others
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveNemesisRelationships()
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CTFPlayer *pTemp = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pTemp && pTemp != this )
		{
			// set this player to be not dominating anyone else
			m_Shared.SetPlayerDominated( pTemp, false );

			// set no one else to be dominating this player
			pTemp->m_Shared.SetPlayerDominated( this, false );
		}
	}	
	// reset the matrix of who has killed whom with respect to this player
	CTF_GameStats.ResetKillHistory( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::Regenerate( void )
{
	// We may have been boosted over our max health. If we have, 
	// restore it after we reset out class values.
	int iCurrentHealth = GetHealth();
	m_bRegenerating = true;
	InitClass();
	m_bRegenerating = false;
	if ( iCurrentHealth > GetHealth() )
	{
		SetHealth( iCurrentHealth );
	}

	if ( m_Shared.InCond( TF_COND_BURNING ) )
	{
		m_Shared.RemoveCond( TF_COND_BURNING );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::InitClass( void )
{
	// Set initial health and armor based on class.
	SetMaxHealth( GetPlayerClass()->GetMaxHealth() );
	SetHealth( GetMaxHealth() );

	SetArmorValue( GetPlayerClass()->GetMaxArmor() );

	// Init the anim movement vars
	m_PlayerAnimState->SetRunSpeed( GetPlayerClass()->GetMaxSpeed() );
	m_PlayerAnimState->SetWalkSpeed( GetPlayerClass()->GetMaxSpeed() * 0.5 );

	// Give default items for class.
	GiveDefaultItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CreateViewModel( int iViewModel )
{
	Assert( iViewModel >= 0 && iViewModel < MAX_VIEWMODELS );

	if ( GetViewModel( iViewModel ) )
		return;

	CTFViewModel *pViewModel = ( CTFViewModel * )CreateEntityByName( "tf_viewmodel" );
	if ( pViewModel )
	{
		pViewModel->SetAbsOrigin( GetAbsOrigin() );
		pViewModel->SetOwner( this );
		pViewModel->SetIndex( iViewModel );
		DispatchSpawn( pViewModel );
		pViewModel->FollowEntity( this, false );
		m_hViewModel.Set( iViewModel, pViewModel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the view model for the player's off hand
//-----------------------------------------------------------------------------
CBaseViewModel *CTFPlayer::GetOffHandViewModel()
{
	// off hand model is slot 1
	return GetViewModel( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Sends the specified animation activity to the off hand view model
//-----------------------------------------------------------------------------
void CTFPlayer::SendOffHandViewModelActivity( Activity activity )
{
	CBaseViewModel *pViewModel = GetOffHandViewModel();
	if ( pViewModel )
	{
		int sequence = pViewModel->SelectWeightedSequence( activity );
		pViewModel->SendViewModelMatchingSequence( sequence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the player up with the default weapons, ammo, etc.
//-----------------------------------------------------------------------------
void CTFPlayer::GiveDefaultItems()
{
	// Get the player class data.
	TFPlayerClassData_t *pData = m_PlayerClass.GetData();

	RemoveAllAmmo();

	// Give ammo. Must be done before weapons, so weapons know the player has ammo for them.
	for ( int iAmmo = 0; iAmmo < TF_AMMO_COUNT; ++iAmmo )
	{
		GiveAmmo( pData->m_aAmmoMax[iAmmo], iAmmo );
	}
	
	// Give weapons.
	ManageRegularWeapons( pData );

	// Give a builder weapon for each object the playerclass is allowed to build
	ManageBuilderWeapons( pData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageBuilderWeapons( TFPlayerClassData_t *pData )
{
	if ( pData->m_aBuildable[0] != OBJ_LAST )
	{
		CTFWeaponBase *pBuilder = Weapon_OwnsThisID( TF_WEAPON_BUILDER );

		// Give the player a new builder weapon when they switch between engy and spy
		if ( pBuilder && !GetPlayerClass()->CanBuildObject( pBuilder->GetSubType() ) )
		{
			Weapon_Detach( pBuilder );
			UTIL_Remove( pBuilder );

			pBuilder = NULL;
		}
		
		if ( pBuilder )
		{
			pBuilder->GiveDefaultAmmo();
			pBuilder->ChangeTeam( GetTeamNumber() );

			if ( m_bRegenerating == false )
			{
				pBuilder->WeaponReset();
			}
		}
		else
		{
			pBuilder = (CTFWeaponBase *)GiveNamedItem( "tf_weapon_builder" );

			if ( pBuilder )
			{
				pBuilder->SetSubType( pData->m_aBuildable[0] );
				pBuilder->DefaultTouch( this );				
			}
		}

		if ( pBuilder )
		{
			pBuilder->m_nSkin = GetTeamNumber() - 2;	// color the w_model to the team
		}
	}
	else
	{
		//Not supposed to be holding a builder, nuke it from orbit
		CTFWeaponBase *pWpn = Weapon_OwnsThisID( TF_WEAPON_BUILDER );

		if ( pWpn == NULL )
			return;

		Weapon_Detach( pWpn );
		UTIL_Remove( pWpn );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageRegularWeapons( TFPlayerClassData_t *pData )
{
	for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		if ( pData->m_aWeapons[iWeapon] != TF_WEAPON_NONE )
		{
			int iWeaponID = pData->m_aWeapons[iWeapon];
			const char *pszWeaponName = WeaponIdToClassname( iWeaponID );

			CTFWeaponBase *pWeapon = (CTFWeaponBase *)GetWeapon( iWeapon );

			//If we already have a weapon in this slot but is not the same type then nuke it (changed classes)
			if ( pWeapon && pWeapon->GetWeaponID() != iWeaponID )
			{
				Weapon_Detach( pWeapon );
				UTIL_Remove( pWeapon );
			}

			pWeapon = (CTFWeaponBase *)Weapon_OwnsThisID( iWeaponID );

			if ( pWeapon )
			{
				pWeapon->ChangeTeam( GetTeamNumber() );
				pWeapon->GiveDefaultAmmo();
	
				if ( m_bRegenerating == false )
				{
					pWeapon->WeaponReset();
				}
			}
			else
			{
				pWeapon = (CTFWeaponBase *)GiveNamedItem( pszWeaponName );

				if ( pWeapon )
				{
					pWeapon->DefaultTouch( this );
				}
			}
		}
		else
		{
			//I shouldn't have any weapons in this slot, so get rid of it
			CTFWeaponBase *pCarriedWeapon = (CTFWeaponBase *)GetWeapon( iWeapon );

			//Don't nuke builders since they will be nuked if we don't need them later.
			if ( pCarriedWeapon && pCarriedWeapon->GetWeaponID() != TF_WEAPON_BUILDER )
			{
				Weapon_Detach( pCarriedWeapon );
				UTIL_Remove( pCarriedWeapon );
			}
		}
	}

	if ( m_bRegenerating == false )
	{
		SetActiveWeapon( NULL );
		Weapon_Switch( Weapon_GetSlot( 0 ) );
		Weapon_SetLast( Weapon_GetSlot( 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a spawn point for the player.
//-----------------------------------------------------------------------------
CBaseEntity* CTFPlayer::EntSelectSpawnPoint()
{
	CBaseEntity *pSpot = g_pLastSpawnPoints[ GetTeamNumber() ];
	const char *pSpawnPointName = "";

	switch( GetTeamNumber() )
	{
	case TF_TEAM_RED:
	case TF_TEAM_BLUE:
		{
			pSpawnPointName = "info_player_teamspawn";
			if ( SelectSpawnSpot( pSpawnPointName, pSpot ) )
			{
				g_pLastSpawnPoints[ GetTeamNumber() ] = pSpot;
			}

			// need to save this for later so we can apply and modifiers to the armor and grenades...after the call to InitClass()
			m_pSpawnPoint = dynamic_cast<CTFTeamSpawn*>( pSpot );
			break;
		}
	case TEAM_SPECTATOR:
	case TEAM_UNASSIGNED:
	default:
		{
			pSpot = CBaseEntity::Instance( INDEXENT(0) );
			break;		
		}
	}

	if ( !pSpot )
	{
		Warning( "PutClientInServer: no %s on level\n", pSpawnPointName );
		return CBaseEntity::Instance( INDEXENT(0) );
	}

	return pSpot;
} 

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot )
{
	// Get an initial spawn point.
	pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	if ( !pSpot )
	{
		// Sometimes the first spot can be NULL????
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	}

	// First we try to find a spawn point that is fully clear. If that fails,
	// we look for a spawnpoint that's clear except for another players. We
	// don't collide with our team members, so we should be fine.
	bool bIgnorePlayers = false;

	CBaseEntity *pFirstSpot = pSpot;
	do 
	{
		if ( pSpot )
		{
			// Check to see if this is a valid team spawn (player is on this team, etc.).
			if( TFGameRules()->IsSpawnPointValid( pSpot, this, bIgnorePlayers ) )
			{
				// Check for a bad spawn entity.
				if ( pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
				{
					pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
					continue;
				}

				// Found a valid spawn point.
				return true;
			}
		}

		// Get the next spawning point to check.
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );

		if ( pSpot == pFirstSpot && !bIgnorePlayers )
		{
			// Loop through again, ignoring players
			bIgnorePlayers = true;
			pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
		}
	} 
	// Continue until a valid spawn point is found or we hit the start.
	while ( pSpot != pFirstSpot ); 

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PhysObjectSleep()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Sleep();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PhysObjectWake()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Wake();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetAutoTeam( void )
{
	int iTeam = TEAM_SPECTATOR;

	CTFTeam *pBlue = TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	CTFTeam *pRed  = TFTeamMgr()->GetTeam( TF_TEAM_RED );

	if ( pBlue && pRed )
	{
		if ( pBlue->GetNumPlayers() < pRed->GetNumPlayers() )
		{
			iTeam = TF_TEAM_BLUE;
		}
		else if ( pRed->GetNumPlayers() < pBlue->GetNumPlayers() )
		{
			iTeam = TF_TEAM_RED;
		}
		else
		{
			iTeam = RandomInt( 0, 1 ) ? TF_TEAM_RED : TF_TEAM_BLUE;
		}
	}

	return iTeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinTeam( const char *pTeamName )
{
	int iTeam = TF_TEAM_RED;
	if ( stricmp( pTeamName, "auto" ) == 0 )
	{
		iTeam = GetAutoTeam();
	}
	else if ( stricmp( pTeamName, "spectate" ) == 0 )
	{
		iTeam = TEAM_SPECTATOR;
	}
	else
	{
		for ( int i = 0; i < TF_TEAM_COUNT; ++i )
		{
			if ( stricmp( pTeamName, g_aTeamNames[i] ) == 0 )
			{
				iTeam = i;
				break;
			}
		}
	}

	if ( iTeam == TEAM_SPECTATOR )
	{
		// Prevent this is the cvar is set
		if ( !mp_allowspectators.GetInt() && !IsHLTV() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			return;
		}
		
		if ( GetTeamNumber() != TEAM_UNASSIGNED && !IsDead() )
		{
			CommitSuicide( false, true );
		}

		ChangeTeam( TEAM_SPECTATOR );

		// do we have fadetoblack on? (need to fade their screen back in)
		if ( mp_fadetoblack.GetBool() )
		{
			color32_s clr = { 0,0,0,255 };
			UTIL_ScreenFade( this, clr, 0, 0, FFADE_IN | FFADE_PURGE );
		}
	}
	else
	{
		if ( iTeam == GetTeamNumber() )
		{
			return;	// we wouldn't change the team
		}

		// if this join would unbalance the teams, refuse
		// come up with a better way to tell the player they tried to join a full team!
		if ( TFGameRules()->WouldChangeUnbalanceTeams( iTeam, GetTeamNumber() ) )
		{
			ShowViewPortPanel( PANEL_TEAM );
			return;
		}

		ChangeTeam( iTeam );

		ShowViewPortPanel( ( iTeam == TF_TEAM_RED ) ? PANEL_CLASS_RED : PANEL_CLASS_BLUE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Join a team without using the game menus
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinTeam_NoMenus( const char *pTeamName )
{
	Assert( IsX360() );

	Msg( "Client command HandleCommand_JoinTeam_NoMenus: %s\n", pTeamName );

	// Only expected to be used on the 360 when players leave the lobby to start a new game
	if ( !IsInCommentaryMode() )
	{
		Assert( GetTeamNumber() == TEAM_UNASSIGNED );
		Assert( IsX360() );
	}

	int iTeam = TEAM_SPECTATOR;
	if ( Q_stricmp( pTeamName, "spectate" ) )
	{
		for ( int i = 0; i < TF_TEAM_COUNT; ++i )
		{
			if ( stricmp( pTeamName, g_aTeamNames[i] ) == 0 )
			{
				iTeam = i;
				break;
			}
		}
	}

	ForceChangeTeam( iTeam );
}

//-----------------------------------------------------------------------------
// Purpose: Player has been forcefully changed to another team
//-----------------------------------------------------------------------------
void CTFPlayer::ForceChangeTeam( int iTeamNum )
{
	int iNewTeam = iTeamNum;

	if ( iNewTeam == TF_TEAM_AUTOASSIGN )
	{
		iNewTeam = GetAutoTeam();
	}

	if ( !GetGlobalTeam( iNewTeam ) )
	{
		Warning( "CTFPlayer::ForceChangeTeam( %d ) - invalid team index.\n", iNewTeam );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iNewTeam == iOldTeam )
		return;

	RemoveAllObjects();
	RemoveNemesisRelationships();

	BaseClass::ChangeTeam( iNewTeam );

	if ( iNewTeam == TEAM_UNASSIGNED )
	{
		StateTransition( TF_STATE_OBSERVER );
	}
	else if ( iNewTeam == TEAM_SPECTATOR )
	{
		m_bIsIdle = false;
		StateTransition( TF_STATE_OBSERVER );

		RemoveAllWeapons();
		DestroyViewModels();
	}

	// Don't modify living players in any way
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleFadeToBlack( void )
{
	if ( mp_fadetoblack.GetBool() )
	{
		color32_s clr = { 0,0,0,255 };
		UTIL_ScreenFade( this, clr, 0.75, 0, FFADE_OUT | FFADE_STAYOUT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ChangeTeam( int iTeamNum )
{
	if ( !GetGlobalTeam( iTeamNum ) )
	{
		Warning( "CTFPlayer::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;

	RemoveAllObjects();
	RemoveNemesisRelationships();

	BaseClass::ChangeTeam( iTeamNum );

	if ( iTeamNum == TEAM_UNASSIGNED )
	{
		StateTransition( TF_STATE_OBSERVER );
	}
	else if ( iTeamNum == TEAM_SPECTATOR )
	{
		m_bIsIdle = false;

		StateTransition( TF_STATE_OBSERVER );

		RemoveAllWeapons();
		DestroyViewModels();
	}
	else // active player
	{
		if ( !IsDead() && (iOldTeam == TF_TEAM_RED || iOldTeam == TF_TEAM_BLUE) )
		{
			// Kill player if switching teams while alive
			CommitSuicide( false, true );
		}
		else if ( IsDead() && iOldTeam < FIRST_GAME_TEAM )
		{
			SetObserverMode( OBS_MODE_CHASE );
			HandleFadeToBlack();
		}

		// let any spies disguising as me know that I've changed teams
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CTFPlayer *pTemp = ToTFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pTemp && pTemp != this )
			{
				if ( ( pTemp->m_Shared.GetDisguiseTarget() == this ) || // they were disguising as me and I've changed teams
 					 ( !pTemp->m_Shared.GetDisguiseTarget() && pTemp->m_Shared.GetDisguiseTeam() == iTeamNum ) ) // they don't have a disguise and I'm joining the team they're disguising as
				{
					// choose someone else...
					pTemp->m_Shared.FindDisguiseTarget();
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinClass( const char *pClassName )
{
	// can only join a class after you join a valid team
	if ( GetTeamNumber() <= LAST_SHARED_TEAM )
		return;

	// In case we don't get the class menu message before the spawn timer
	// comes up, fake that we've closed the menu.
	SetClassMenuOpen( false );

	if ( TFGameRules()->InStalemate() )
	{
		if ( IsAlive() && !TFGameRules()->CanChangeClassInStalemate() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_stalemate_cant_change_class" );
			return;
		}
	}

	int iClass = TF_CLASS_UNDEFINED;
	bool bShouldNotRespawn = false;

	if ( ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN ) && ( TFGameRules()->GetWinningTeam() != GetTeamNumber() ) )
	{
		m_bAllowInstantSpawn = false;
		bShouldNotRespawn = true;
	}

	if ( stricmp( pClassName, "random" ) != 0 )
	{
		int i = 0;

		for ( i = TF_CLASS_SCOUT ; i < TF_CLASS_COUNT_ALL ; i++ )
		{
			if ( stricmp( pClassName, GetPlayerClassData( i )->m_szClassName ) == 0 )
			{
				iClass = i;
				break;
			}
		}
		if ( i > TF_LAST_NORMAL_CLASS )
		{
			Warning( "HandleCommand_JoinClass( %s ) - invalid class name.\n", pClassName );
			return;
		}
	}
	else
	{
		// The player has selected Random class...so let's pick one for them.
		do{
			// Don't let them be the same class twice in a row
			iClass = random->RandomInt( TF_FIRST_NORMAL_CLASS, TF_LAST_NORMAL_CLASS );
		} while( iClass == GetPlayerClass()->GetClassIndex() );
	}

	// joining the same class?
	if ( iClass != TF_CLASS_RANDOM && iClass == GetDesiredPlayerClassIndex() )
	{
		// If we're dead, and we have instant spawn, respawn us immediately. Catches the case
		// were a player misses respawn wave because they're at the class menu, and then changes
		// their mind and reselects their current class.
		if ( m_bAllowInstantSpawn && !IsAlive() )
		{
			ForceRespawn();
		}
		return;
	}

	SetDesiredPlayerClassIndex( iClass );
	IGameEvent * event = gameeventmanager->CreateEvent( "player_changeclass" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "class", iClass );

		gameeventmanager->FireEvent( event );
	}

	// are they TF_CLASS_RANDOM and trying to select the class they're currently playing as (so they can stay this class)?
	if ( iClass == GetPlayerClass()->GetClassIndex() )
	{
		// If we're dead, and we have instant spawn, respawn us immediately. Catches the case
		// were a player misses respawn wave because they're at the class menu, and then changes
		// their mind and reselects their current class.
		if ( m_bAllowInstantSpawn && !IsAlive() )
		{
			ForceRespawn();
		}
		return;
	}

	// We can respawn instantly if:
	//	- We're dead, and we're past the required post-death time
	//	- We're inside a respawn room
	//	- We're in the stalemate grace period
	bool bInRespawnRoom = PointInRespawnRoom( this, WorldSpaceCenter() );
	if ( bInRespawnRoom && !IsAlive() )
	{
		// If we're not spectating ourselves, ignore respawn rooms. Otherwise we'll get instant spawns
		// by spectating someone inside a respawn room.
		bInRespawnRoom = (GetObserverTarget() == this);
	}
	bool bDeadInstantSpawn = !IsAlive();
	if ( bDeadInstantSpawn && m_flDeathTime )
	{
		// In death mode, don't allow class changes to force respawns ahead of respawn waves
		float flWaveTime = TFGameRules()->GetNextRespawnWave( GetTeamNumber(), this );
		bDeadInstantSpawn = (gpGlobals->curtime > flWaveTime);
	}
	bool bInStalemateClassChangeTime = false;
	if ( TFGameRules()->InStalemate() )
	{
		// Stalemate overrides respawn rules. Only allow spawning if we're in the class change time.
		bInStalemateClassChangeTime = TFGameRules()->CanChangeClassInStalemate();
		bDeadInstantSpawn = false;
		bInRespawnRoom = false;
	}
	if ( bShouldNotRespawn == false && ( m_bAllowInstantSpawn || bDeadInstantSpawn || bInRespawnRoom || bInStalemateClassChangeTime ) )
	{
		ForceRespawn();
		return;
	}

	if( iClass == TF_CLASS_RANDOM )
	{
		if( IsAlive() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_respawn_asrandom" );
		}
		else
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_spawn_asrandom" );
		}
	}
	else
	{
		if( IsAlive() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_respawn_as", GetPlayerClassData( iClass )->m_szLocalizableName );
		}
		else
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_spawn_as", GetPlayerClassData( iClass )->m_szLocalizableName );
		}
	}

	if ( IsAlive() && ( GetHudClassAutoKill() == true ) && bShouldNotRespawn == false )
	{
		CommitSuicide( false, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];
	
	m_flLastAction = gpGlobals->curtime;

#ifdef _DEBUG
	if ( FStrEq( pcmd, "addcond" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			int iCond = clamp( atoi( args[1] ), 0, TF_COND_LAST-1 );

			CTFPlayer *pTargetPlayer = this;
			if ( args.ArgC() >= 4 )
			{
				// Find the matching netname
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex(i) );
					if ( pPlayer )
					{
						if ( Q_strstr( pPlayer->GetPlayerName(), args[3] ) )
						{
							pTargetPlayer = ToTFPlayer(pPlayer);
							break;
						}
					}
				}
			}

			if ( args.ArgC() >= 3 )
			{
				float flDuration = atof( args[2] );
				pTargetPlayer->m_Shared.AddCond( iCond, flDuration );
			}
			else
			{
				pTargetPlayer->m_Shared.AddCond( iCond );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "removecond" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			int iCond = clamp( atoi( args[1] ), 0, TF_COND_LAST-1 );
			m_Shared.RemoveCond( iCond );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "burn" ) ) 
	{
		m_Shared.Burn( this );
		return true;
	}
	else
#endif

	if ( FStrEq( pcmd, "jointeam" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			HandleCommand_JoinTeam( args[1] );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "jointeam_nomenus" ) )
	{
		if ( IsX360() )
		{
			if ( args.ArgC() >= 2 )
			{
				HandleCommand_JoinTeam_NoMenus( args[1] );
			}
			return true;
		}
		return false;
	}
	else if ( FStrEq( pcmd, "closedwelcomemenu" ) )
	{
		if ( GetTeamNumber() == TEAM_UNASSIGNED )
		{
			ShowViewPortPanel( PANEL_TEAM, true );
		}
		else if ( IsPlayerClass( TF_CLASS_UNDEFINED ) )
		{
			switch( GetTeamNumber() )
			{
			case TF_TEAM_RED:
				ShowViewPortPanel( PANEL_CLASS_RED, true );
				break;

			case TF_TEAM_BLUE:
				ShowViewPortPanel( PANEL_CLASS_BLUE, true );
				break;

			default:
				break;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "joinclass" ) ) 
	{
		if ( args.ArgC() >= 2 )
		{
			HandleCommand_JoinClass( args[1] );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "disguise" ) ) 
	{
		if ( args.ArgC() >= 3 )
		{
			if ( CanDisguise() )
			{
				int nClass = atoi( args[ 1 ] );
				int nTeam = atoi( args[ 2 ] );
				
				// intercepting the team value and reassigning what gets passed into Disguise()
				// because the team numbers in the client menu don't match the #define values for the teams
				m_Shared.Disguise( ( nTeam == 1 ) ? TF_TEAM_BLUE : TF_TEAM_RED, nClass );
			}
		}
		return true;
	}
	else if (FStrEq( pcmd, "lastdisguise" ) )
	{
		// disguise as our last known disguise. desired disguise will be initted to something sensible
		if ( CanDisguise() )
		{
			// disguise as the previous class, if one exists
			int nClass = m_Shared.GetDesiredDisguiseClass();

			//If we pass in "random" or whatever then just make it pick a random class.
			if ( args.ArgC() > 1 )
			{
				nClass = TF_CLASS_UNDEFINED;
			}

			if ( nClass == TF_CLASS_UNDEFINED )
			{
				// they haven't disguised yet, pick a nice one for them.
				// exclude some undesirable classes
				do
				{
					nClass = random->RandomInt( TF_FIRST_NORMAL_CLASS, TF_LAST_NORMAL_CLASS );
				} while( nClass == TF_CLASS_SCOUT || nClass == TF_CLASS_SPY );
			}

			m_Shared.Disguise( ( GetTeamNumber() == TF_TEAM_BLUE ) ? TF_TEAM_RED : TF_TEAM_BLUE, nClass );
		}

		return true;
	}
	else if ( FStrEq( pcmd, "mp_playgesture" ) )
	{
		if ( args.ArgC() == 1 )
		{
			Warning( "mp_playgesture: Gesture activity or sequence must be specified!\n" );
			return true;
		}

		if ( sv_cheats->GetBool() )
		{
			if ( !PlayGesture( args[1] ) )
			{
				Warning( "mp_playgesture: unknown sequence or activity name \"%s\"\n", args[1] );
				return true;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "mp_playanimation" ) )
	{
		if ( args.ArgC() == 1 )
		{
			Warning( "mp_playanimation: Activity or sequence must be specified!\n" );
			return true;
		}

		if ( sv_cheats->GetBool() )
		{
			if ( !PlaySpecificSequence( args[1] ) )
			{
				Warning( "mp_playanimation: Unknown sequence or activity name \"%s\"\n", args[1] );
				return true;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "menuopen" ) )
	{
		SetClassMenuOpen( true );
		return true;
	}
	else if ( FStrEq( pcmd, "menuclosed" ) )
	{
		SetClassMenuOpen( false );
		return true;
	}
	else if ( FStrEq( pcmd, "pda_click" ) )
	{
		// player clicked on the PDA, play attack animation

		CTFWeaponBase *pWpn = GetActiveTFWeapon();

		CTFWeaponPDA *pPDA = dynamic_cast<CTFWeaponPDA *>( pWpn );

		if ( pPDA && !m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
		}

		return true;
	}
	else if ( FStrEq( pcmd, "taunt" ) )
	{
		Taunt();
		return true;
	}
	else if ( FStrEq( pcmd, "build" ) )
	{
		if ( args.ArgC() == 2 )
		{
			// player wants to build something
			int iBuilding = atoi( args[ 1 ] );

			StartBuildingObjectOfType( iBuilding );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "destroy" ) )
	{
		if ( args.ArgC() == 2 )
		{
			// player wants to destroy something
			int iBuilding = atoi( args[ 1 ] );

			DetonateOwnedObjectsOfType( iBuilding );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "extendfreeze" ) )
	{
		m_flDeathTime += 2.0f;
		return true;
	}
	else if ( FStrEq( pcmd, "show_motd" ) )
	{
		KeyValues *data = new KeyValues( "data" );
		data->SetString( "title", "#TF_Welcome" );	// info panel title
		data->SetString( "type", "1" );				// show userdata from stringtable entry
		data->SetString( "msg",	"motd" );			// use this stringtable entry
		data->SetString( "cmd", "mapinfo" );		// exec this command if panel closed

		ShowViewPortPanel( PANEL_INFO, true, data );

		data->deleteThis();
	}
	else if ( FStrEq( pcmd, "condump_on" ) )
	{
		if ( !PlayerHasPowerplay() )
		{
			Msg("Console dumping on.\n");
			return true;
		}
		else 
		{
			if ( args.ArgC() == 2 && GetTeam() )
			{
				for ( int i = 0; i < GetTeam()->GetNumPlayers(); i++ )
				{
					CTFPlayer *pTeamPlayer = ToTFPlayer( GetTeam()->GetPlayer(i) );
					if ( pTeamPlayer )
					{
						pTeamPlayer->SetPowerplayEnabled( true );
					}
				}
				return true;
			}
			else
			{
				if ( SetPowerplayEnabled( true ) )
					return true;
			}
		}
	}
	else if ( FStrEq( pcmd, "condump_off" ) )
	{
		if ( !PlayerHasPowerplay() )
		{
			Msg("Console dumping off.\n");
			return true;
		}
		else
		{
			if ( args.ArgC() == 2 && GetTeam() )
			{
				for ( int i = 0; i < GetTeam()->GetNumPlayers(); i++ )
				{
					CTFPlayer *pTeamPlayer = ToTFPlayer( GetTeam()->GetPlayer(i) );
					if ( pTeamPlayer )
					{
						pTeamPlayer->SetPowerplayEnabled( false );
					}
				}
				return true;
			}
			else
			{
				if ( SetPowerplayEnabled( false ) )
					return true;
			}
		}
	}

	return BaseClass::ClientCommand( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetClassMenuOpen( bool bOpen )
{
	m_bIsClassMenuOpen = bOpen;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsClassMenuOpen( void )
{
	return m_bIsClassMenuOpen;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayGesture( const char *pGestureName )
{
	Activity nActivity = (Activity)LookupActivity( pGestureName );
	if ( nActivity != ACT_INVALID )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, nActivity );
		return true;
	}

	int nSequence = LookupSequence( pGestureName );
	if ( nSequence != -1 )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE, nSequence );
		return true;
	} 

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlaySpecificSequence( const char *pAnimationName )
{
	Activity nActivity = (Activity)LookupActivity( pAnimationName );
	if ( nActivity != ACT_INVALID )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM, nActivity );
		return true;
	}

	int nSequence = LookupSequence( pAnimationName );
	if ( nSequence != -1 )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_SEQUENCE, nSequence );
		return true;
	} 

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanDisguise( void )
{
	if ( !IsAlive() )
		return false;

	if ( GetPlayerClass()->GetClassIndex() != TF_CLASS_SPY )
		return false;

	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		HintMessage( HINT_CANNOT_DISGUISE_WITH_FLAG );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DetonateOwnedObjectsOfType( int iType )
{
	int i;
	int iNumObjects = GetObjectCount();
	for ( i=0;i<iNumObjects;i++ )
	{
		CBaseObject *pObj = GetObject(i);

		if ( pObj && pObj->GetType() == iType )
		{
			SpeakConceptIfAllowed( MP_CONCEPT_DETONATED_OBJECT, pObj->GetResponseRulesModifier() );
			pObj->DetonateObject();

			const CObjectInfo *pInfo = GetObjectInfo( iType );

			if ( pInfo )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"killedobject\" (object \"%s\") (weapon \"%s\") (objectowner \"%s<%i><%s><%s>\") (attacker_position \"%d %d %d\")\n",   
					GetPlayerName(),
					GetUserID(),
					GetNetworkIDString(),
					GetTeam()->GetName(),
					pInfo->m_pObjectName,
					"pda_engineer",
					GetPlayerName(),
					GetUserID(),
					GetNetworkIDString(),
					GetTeam()->GetName(),
					(int)GetAbsOrigin().x, 
					(int)GetAbsOrigin().y,
					(int)GetAbsOrigin().z );
			}

			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StartBuildingObjectOfType( int iType )
{
	// early out if we can't build this type of object
	if ( CanBuild( iType ) != CB_CAN_BUILD )
		return;

	for ( int i = 0; i < WeaponCount(); i++) 
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon(i);

		if ( pWpn == NULL )
			continue;

		if ( pWpn->GetWeaponID() != TF_WEAPON_BUILDER )
			continue;

		CTFWeaponBuilder *pBuilder = dynamic_cast< CTFWeaponBuilder * >( pWpn );

		// Is this the builder that builds the object we're looking for?
		if ( pBuilder )
		{
			pBuilder->SetSubType( iType );

			if ( GetActiveTFWeapon() == pBuilder )
			{
				SetActiveWeapon( NULL );
			}

			// try to switch to this weapon
			if ( Weapon_Switch( pBuilder ) )
			{
				break;
			}
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( m_takedamage != DAMAGE_YES )
		return;

	CTFPlayer *pAttacker = (CTFPlayer*)ToTFPlayer( info.GetAttacker() );
	if ( pAttacker )
	{
		// Prevent team damage here so blood doesn't appear
		if ( info.GetAttacker()->IsPlayer() )
		{
			if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
				return;
		}
	}

	// Save this bone for the ragdoll.
	m_nForceBone = ptr->physicsbone;

	SetLastHitGroup( ptr->hitgroup );

	// Ignore hitboxes for all weapons except the sniper rifle

	CTakeDamageInfo info_modified = info;

	if ( info_modified.GetDamageType() & DMG_USE_HITLOCATIONS )
	{
		switch ( ptr->hitgroup )
		{
		case HITGROUP_HEAD:
			{
				CTFWeaponBase *pWpn = pAttacker->GetActiveTFWeapon();
				bool bCritical = true;

				if ( pWpn && !pWpn->CanFireCriticalShot( true ) )
				{
					bCritical = false;
				}

				if ( bCritical )
				{
					info_modified.AddDamageType( DMG_CRITICAL );
					info_modified.SetDamageCustom( TF_DMG_CUSTOM_HEADSHOT );

					// play the critical shot sound to the shooter	
					if ( pWpn )
					{
						pWpn->WeaponSound( BURST );
					}
				}

				break;
			}
		default:
			break;
		}
	}

	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		// no impact effects
	}
	else if ( m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{ 
		// Make bullet impacts
		g_pEffects->Ricochet( ptr->endpos - (vecDir * 8), -vecDir );
	}
	else
	{	
		// Since this code only runs on the server, make sure it shows the tempents it creates.
		CDisablePredictionFiltering disabler;

		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( info_modified.GetDamage(), vecDir, ptr, info_modified.GetDamageType() );
	}

	AddMultiDamage( info_modified, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	int bResult = false;

	// If the bit's set, add over the max health
	if ( bitsDamageType & DMG_IGNORE_MAXHEALTH )
	{
		int iTimeBasedDamage = g_pGameRules->Damage_GetTimeBased();
		m_bitsDamageType &= ~(bitsDamageType & ~iTimeBasedDamage);
		m_iHealth += flHealth;
		bResult = true;
	}
	else
	{
		float flHealthToAdd = flHealth;
		float flMaxHealth = GetPlayerClass()->GetMaxHealth();
		
		// don't want to add more than we're allowed to have
		if ( flHealthToAdd > flMaxHealth - m_iHealth )
		{
			flHealthToAdd = flMaxHealth - m_iHealth;
		}

		if ( flHealthToAdd <= 0 )
		{
			bResult = false;
		}
		else
		{
			bResult = BaseClass::TakeHealth( flHealthToAdd, bitsDamageType );
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFWeaponRemove( int iWeaponID )
{
	// find the weapon that matches the id and remove it
	int i;
	for (i = 0; i < WeaponCount(); i++) 
	{
		CTFWeaponBase *pWeapon = ( CTFWeaponBase *)GetWeapon( i );
		if ( !pWeapon )
			continue;

		if ( pWeapon->GetWeaponID() != iWeaponID )
			continue;

		RemovePlayerItem( pWeapon );
		UTIL_Remove( pWeapon );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::DropCurrentWeapon( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DropFlag( void )
{
	if ( HasItem() )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( GetItem() );
		if ( pFlag )
		{
			pFlag->Drop( this, true, true );
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DROPPED );
				event->SetInt( "priority", 8 );

				gameeventmanager->FireEvent( event );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
EHANDLE CTFPlayer::TeamFortress_GetDisguiseTarget( int nTeam, int nClass )
{
	if ( nTeam == GetTeamNumber() || nTeam == TF_SPY_UNDEFINED )
	{
		// we're not disguised as the enemy team
		return NULL;
	}

	CBaseEntity *pLastTarget = m_Shared.GetDisguiseTarget(); // don't redisguise self as this person
	
	// Find a player on the team the spy is disguised as to pretend to be
	CTFPlayer *pPlayer = NULL;

	// Loop through players
	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			if ( pPlayer == pLastTarget )
			{
				// choose someone else, we're trying to rid ourselves of a disguise as this one
				continue;
			}

			// First, try to find a player with the same color AND skin
			if ( pPlayer->GetTeamNumber() == nTeam && pPlayer->GetPlayerClass()->GetClassIndex() == nClass )
			{
				return pPlayer;
			}
		}
	}

	// we didn't find someone with the same skin, so just find someone with the same color
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			if ( pPlayer->GetTeamNumber() == nTeam )
			{
				return pPlayer;
			}
		}
	}

	// we didn't find anyone
	return NULL;
}

static float DamageForce( const Vector &size, float damage, float scale )
{ 
	float force = damage * ((48 * 48 * 82.0) / (size.x * size.y * size.z)) * scale;
	
	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}

ConVar tf_debug_damage( "tf_debug_damage", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	if ( GetFlags() & FL_GODMODE )
		return 0;

	if ( IsInCommentaryMode() )
		return 0;

	if ( m_debugOverlays & OVERLAY_BUDDHA_MODE ) 
	{
		if ((m_iHealth - info.GetDamage()) <= 0)
		{
			m_iHealth = 1;
			return 0;
		}
	}

	// Early out if there's no damage
	if ( !info.GetDamage() )
		return 0;

	if ( !IsAlive() )
		return 0;

	int iHealthBefore = GetHealth();

	bool bDebug = tf_debug_damage.GetBool();
	if ( bDebug )
	{
		Warning( "%s taking damage from %s, via %s. Damage: %.2f\n", GetDebugName(), info.GetInflictor() ? info.GetInflictor()->GetDebugName() : "Unknown Inflictor", info.GetAttacker() ? info.GetAttacker()->GetDebugName() : "Unknown Attacker", info.GetDamage() );
	}

	// Make sure the player can take damage from the attacking entity
	if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
	{
		if ( bDebug )
		{
			Warning( "    ABORTED: Player can't take damage from that attacker.\n" );
		}
		return 0;
	}

	AddDamagerToHistory( info.GetAttacker() );

	// keep track of amount of damage last sustained
	m_lastDamageAmount = info.GetDamage();
	m_LastDamageType = info.GetDamageType();

	if ( IsPlayerClass( TF_CLASS_SPY ) && !( info.GetDamageType() & DMG_FALL ) )
	{
		m_Shared.NoteLastDamageTime( m_lastDamageAmount );
	}

	// if this is our own rocket, scale down the damage
	if ( IsPlayerClass( TF_CLASS_SOLDIER ) && info.GetAttacker() == this ) 
	{
		float flDamage = info.GetDamage() * tf_damagescale_self_soldier.GetFloat();
		info.SetDamage( flDamage );
	}

	// Save damage force for ragdolls.
	m_vecTotalBulletForce = info.GetDamageForce();
	m_vecTotalBulletForce.x = clamp( m_vecTotalBulletForce.x, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.y = clamp( m_vecTotalBulletForce.y, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.z = clamp( m_vecTotalBulletForce.z, -15000.0f, 15000.0f );

	int bTookDamage = 0;
 
	int bitsDamage = inputInfo.GetDamageType();

	// If we're invulnerable, force ourselves to only take damage events only, so we still get pushed
	if ( m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{
		bool bAllowDamage = false;

		// check to see if our attacker is a trigger_hurt entity (and allow it to kill us even if we're invuln)
		CBaseEntity *pAttacker = info.GetAttacker();
		if ( pAttacker && pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) )
		{
			CTriggerHurt *pTrigger = dynamic_cast<CTriggerHurt *>( pAttacker );
			if ( pTrigger )
			{
				bAllowDamage = true;
			}
		}

		if ( !bAllowDamage )
		{
			int iOldTakeDamage = m_takedamage;
			m_takedamage = DAMAGE_EVENTS_ONLY;
			// NOTE: Deliberately skip base player OnTakeDamage, because we don't want all the stuff it does re: suit voice
			CBaseCombatCharacter::OnTakeDamage( info );
			m_takedamage = iOldTakeDamage;

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) )
			{
				SpeakConceptIfAllowed( MP_CONCEPT_HURT );
			}
			return 0;
		}
	}

	// If we're not damaging ourselves, apply randomness
	if ( info.GetAttacker() != this && !(bitsDamage & (DMG_DROWN | DMG_FALL)) ) 
	{
		float flDamage = 0;
		if ( bitsDamage & DMG_CRITICAL )
		{
			if ( bDebug )
			{
				Warning( "    CRITICAL!\n");
			}

			flDamage = info.GetDamage() * TF_DAMAGE_CRIT_MULTIPLIER;

			// Show the attacker, unless the target is a disguised spy
			if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() && !m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				CEffectData	data;
				data.m_nHitBox = GetParticleSystemIndex( "crit_text" );
				data.m_vOrigin = WorldSpaceCenter() + Vector(0,0,32);
				data.m_vAngles = vec3_angle;
				data.m_nEntIndex = 0;

				CSingleUserRecipientFilter filter( (CBasePlayer*)info.GetAttacker() );
				te->DispatchEffect( filter, 0.0, data.m_vOrigin, "ParticleEffect", data );

				EmitSound_t params;
				params.m_flSoundTime = 0;
				params.m_pSoundName = "TFPlayer.CritHit";
				EmitSound( filter, info.GetAttacker()->entindex(), params );
			}

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) )
			{
				SpeakConceptIfAllowed( MP_CONCEPT_HURT, "damagecritical:1" );
			}
		}
		else
		{
			float flRandomDamage = info.GetDamage() * tf_damage_range.GetFloat();
			if ( tf_damage_lineardist.GetBool() )
			{
				float flBaseDamage = info.GetDamage() - flRandomDamage;
				flDamage = flBaseDamage + RandomFloat( 0, flRandomDamage * 2 );
			}
			else
			{
				float flMin = 0.25;
				float flMax = 0.75;
				float flCenter = 0.5;

				if ( bitsDamage & DMG_USEDISTANCEMOD )
				{
					float flDistance = MAX( 1.0, (WorldSpaceCenter() - info.GetAttacker()->WorldSpaceCenter()).Length() );
					float flOptimalDistance = 512.0;

					flCenter = RemapValClamped( flDistance / flOptimalDistance, 0.0, 2.0, 1.0, 0.0 );
					if ( bitsDamage & DMG_NOCLOSEDISTANCEMOD )
					{
						if ( flCenter > 0.5 )
						{
							// Reduce the damage bonus at close range
							flCenter = RemapVal( flCenter, 0.5, 1.0, 0.5, 0.65 );
						}
					}
					flMin = MAX( 0.0, flCenter - 0.25 );
					flMax = MIN( 1.0, flCenter + 0.25 );

					if ( bDebug )
					{
						Warning("    RANDOM: Dist %.2f, Ctr: %.2f, Min: %.2f, Max: %.2f\n", flDistance, flCenter, flMin, flMax );
					}
				}

				//Msg("Range: %.2f - %.2f\n", flMin, flMax );
				float flRandomVal = tf_damage_disablespread.GetBool() ? flCenter : RandomFloat( flMin, flMax );

				if ( flRandomVal > 0.5 )
				{
					// Rocket launcher & Scattergun have different short range bonuses
					if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
					{
						CTFWeaponBase *pWeapon = ToTFPlayer( info.GetAttacker() )->GetActiveTFWeapon();
						if ( pWeapon )
						{
							if ( pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER )
							{
								// Rocket launcher only has half the bonus of the other weapons at short range
								flRandomDamage *= 0.5;
							}
							else if ( pWeapon->GetWeaponID() == TF_WEAPON_SCATTERGUN )
							{
								// Scattergun gets 50% bonus of other weapons at short range
								flRandomDamage *= 1.5;
							}
						}
					}
				}

				float flOut = SimpleSplineRemapValClamped( flRandomVal, 0, 1, -flRandomDamage, flRandomDamage );
				flDamage = info.GetDamage() + flOut;

				/*
				for ( float flVal = flMin; flVal <= flMax; flVal += 0.05 )
				{
					float flOut = SimpleSplineRemapValClamped( flVal, 0, 1, -flRandomDamage, flRandomDamage );
					Msg("Val: %.2f, Out: %.2f, Dmg: %.2f\n", flVal, flOut, info.GetDamage() + flOut );
				}
				*/
			}

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) )
			{
				SpeakConceptIfAllowed( MP_CONCEPT_HURT );
			}
		}

		info.SetDamage( flDamage );
	}

	// NOTE: Deliberately skip base player OnTakeDamage, because we don't want all the stuff it does re: suit voice
	bTookDamage = CBaseCombatCharacter::OnTakeDamage( info );

	// Early out if the base class took no damage
	if ( !bTookDamage )
	{
		if ( bDebug )
		{
			Warning( "    ABORTED: Player failed to take the damage.\n" );
		}
		return 0;
	}

	if ( bDebug )
	{
		Warning( "    DEALT: Player took %.2f damage.\n", info.GetDamage() );
		Warning( "    HEALTH LEFT: %d\n", GetHealth() );
	}

	// Send the damage message to the client for the hud damage indicator
	// Don't do this for damage types that don't use the indicator
	if ( !(bitsDamage & (DMG_DROWN | DMG_FALL | DMG_BURN) ) )
	{
		// Try and figure out where the damage is coming from
		Vector vecDamageOrigin = info.GetReportedPosition();

		// If we didn't get an origin to use, try using the attacker's origin
		if ( vecDamageOrigin == vec3_origin && info.GetInflictor() )
		{
			vecDamageOrigin = info.GetInflictor()->GetAbsOrigin();
		}

		CSingleUserRecipientFilter user( this );
		UserMessageBegin( user, "Damage" );
			WRITE_BYTE( clamp( (int)info.GetDamage(), 0, 255 ) );
			WRITE_VEC3COORD( vecDamageOrigin );
		MessageEnd();
	}

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// todo: remove after combining shotgun blasts?
	if ( info.GetInflictor() && info.GetInflictor()->edict() )
	{
		m_DmgOrigin = info.GetInflictor()->GetAbsOrigin();
	}

	m_DmgTake += (int)info.GetDamage();

	// Reset damage time countdown for each type of time based damage player just sustained
	for (int i = 0; i < CDMG_TIMEBASED; i++)
	{
		// Make sure the damage type is really time-based.
		// This is kind of hacky but necessary until we setup DamageType as an enum.
		int iDamage = ( DMG_PARALYZE << i );
		if ( ( info.GetDamageType() & iDamage ) && g_pGameRules->Damage_IsTimeBased( iDamage ) )
		{
			m_rgbTimeBasedDamage[i] = 0;
		}
	}

	// Display any effect associate with this damage type
	DamageEffect( info.GetDamage(),bitsDamage );

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	m_Local.m_vecPunchAngle.SetX( -2 );

	// Do special explosion damage effect
	if ( bitsDamage & DMG_BLAST )
	{
		OnDamagedByExplosion( info );
	}

	PainSound( info );

	PlayFlinch( info );

	// Detect drops below 25% health and restart expression, so that characters look worried.
	int iHealthBoundary = (GetMaxHealth() * 0.25);
	if ( GetHealth() <= iHealthBoundary && iHealthBefore > iHealthBoundary )
	{
		ClearExpression();
	}

	CTF_GameStats.Event_PlayerDamage( this, info, iHealthBefore - GetHealth() );

	return bTookDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DamageEffect(float flDamage, int fDamageType)
{
	bool bDisguised = m_Shared.InCond( TF_COND_DISGUISED );

	if (fDamageType & DMG_CRUSH)
	{
		//Red damage indicator
		color32 red = {128,0,0,128};
		UTIL_ScreenFade( this, red, 1.0f, 0.1f, FFADE_IN );
	}
	else if (fDamageType & DMG_DROWN)
	{
		//Red damage indicator
		color32 blue = {0,0,128,128};
		UTIL_ScreenFade( this, blue, 1.0f, 0.1f, FFADE_IN );
	}
	else if (fDamageType & DMG_SLASH)
	{
		if ( !bDisguised )
		{
			// If slash damage shoot some blood
			SpawnBlood(EyePosition(), g_vecAttackDir, BloodColor(), flDamage);
		}
	}
	else if ( fDamageType & DMG_BULLET )
	{
		if ( !bDisguised )
		{
			EmitSound( "Flesh.BulletImpact" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( ( ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) && tf_avoidteammates.GetBool() ) ||
		collisionGroup == TFCOLLISION_GROUP_ROCKETS )
	{
		switch( GetTeamNumber() )
		{
		case TF_TEAM_RED:
			if ( !( contentsMask & CONTENTS_REDTEAM ) )
				return false;
			break;

		case TF_TEAM_BLUE:
			if ( !( contentsMask & CONTENTS_BLUETEAM ) )
				return false;
			break;
		}
	}
	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//---------------------------------------
// Is the player the passed player class?
//---------------------------------------
bool CTFPlayer::IsPlayerClass( int iClass ) const
{
	const CTFPlayerClass *pClass = &m_PlayerClass;

	if ( !pClass )
		return false;

	return ( pClass->IsClass( iClass ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CommitSuicide( bool bExplode /* = false */, bool bForce /*= false*/ )
{
	// Don't suicide if we haven't picked a class for the first time, or we're not in active state
	if ( IsPlayerClass( TF_CLASS_UNDEFINED ) || !m_Shared.InState( TF_STATE_ACTIVE ) )
		return;

	// Don't suicide during the "bonus time" if we're not on the winning team
	if ( !bForce && TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && 
		 GetTeamNumber() != TFGameRules()->GetWinningTeam() )
	{
		return;
	}
	
	m_iSuicideCustomKillFlags = TF_DMG_CUSTOM_SUICIDE;

	BaseClass::CommitSuicide( bExplode, bForce );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : int
//-----------------------------------------------------------------------------
int CTFPlayer::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Grab the vector of the incoming attack. 
	// (Pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	Vector vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0.0f, 0.0f, 10.0f ) - WorldSpaceCenter();
		VectorNormalize( vecDir );
	}
	g_vecAttackDir = vecDir;

	// Do the damage.
	m_bitsDamageType |= info.GetDamageType();

	bool bIgniting = false;

	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		// Start burning if we took ignition damage
		bIgniting = ( ( info.GetDamageType() & DMG_IGNITE ) && ( GetWaterLevel() < WL_Waist ) );

		if ( info.GetDamage() == 0.0f )
			return 0;

		// Take damage - round to the nearest integer.
		m_iHealth -= ( info.GetDamage() + 0.5f );
	}

	m_flLastDamageTime = gpGlobals->curtime;

	// Apply a damage force.
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( !pAttacker )
		return 0;

	if ( ( info.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE ) == 0 )
	{
		if ( info.GetInflictor() && ( GetMoveType() == MOVETYPE_WALK ) && 
		   ( !pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) ) && 
		   ( !m_Shared.InCond( TF_COND_DISGUISED ) ) )	
		{
			Vector vecForce;
			vecForce.Init();
			if ( info.GetAttacker() == this )
			{
				if ( IsPlayerClass( TF_CLASS_SOLDIER ) ) 
				{
					vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), tf_damageforcescale_self_soldier.GetFloat() );
				}
				else
				{
					vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), DAMAGE_FORCE_SCALE_SELF );
				}
			}
			else
			{
				// Sentryguns push a lot harder
				if ( (info.GetDamageType() & DMG_BULLET) && info.GetInflictor()->IsBaseObject() )
				{
					vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), 16 );
				}
				else
				{
					vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), tf_damageforcescale_other.GetFloat() );

					if ( IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) )
					{
						// Heavies take less push from non sentryguns
						vecForce *= 0.5;
					}
				}
			}

			ApplyAbsVelocityImpulse( vecForce );
		}
	}

	if ( bIgniting )
	{
		m_Shared.Burn( ToTFPlayer( pAttacker ) );
	}

	// Fire a global game event - "player_hurt"
	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "health", MAX( 0, m_iHealth ) );

		// HLTV event priority, not transmitted
		event->SetInt( "priority", 5 );	

		// Hurt by another player.
		if ( pAttacker->IsPlayer() )
		{
			CBasePlayer *pPlayer = ToBasePlayer( pAttacker );
			event->SetInt( "attacker", pPlayer->GetUserID() );
		}
		// Hurt by world.
		else
		{
			event->SetInt( "attacker", 0 );
		}

        gameeventmanager->FireEvent( event );
	}
	
	if ( pAttacker != this && pAttacker->IsPlayer() )
	{
		ToTFPlayer( pAttacker )->RecordDamageEvent( info, (m_iHealth <= 0) );
	}

	//No bleeding while invul or disguised.
	bool bBleed = ( m_Shared.InCond( TF_COND_DISGUISED ) == false && m_Shared.InCond( TF_COND_INVULNERABLE ) == false );
	if ( bBleed && pAttacker->IsPlayer() )
	{
		CTFWeaponBase *pWeapon = ToTFPlayer( pAttacker )->GetActiveTFWeapon();
		if ( pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER )
		{
			bBleed = false;
		}
	}

	if ( bBleed )
	{
		Vector vDamagePos = info.GetDamagePosition();

		if ( vDamagePos == vec3_origin )
		{
			vDamagePos = WorldSpaceCenter();
		}

		CPVSFilter filter( vDamagePos );
		TE_TFBlood( filter, 0.0, vDamagePos, -vecDir, entindex() );
	}

	// Done.
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Adds this damager to the history list of people who damaged player
//-----------------------------------------------------------------------------
void CTFPlayer::AddDamagerToHistory( EHANDLE hDamager )
{
	// sanity check: ignore damager if it is on our team.  (Catch-all for 
	// damaging self in rocket jumps, etc.)
	CTFPlayer *pDamager = ToTFPlayer( hDamager );
	if ( !pDamager || ( pDamager->GetTeam() == GetTeam() ) )
		return;

	// If this damager is different from the most recent damager, shift the
	// damagers down and drop the oldest damager.  (If this damager is already
	// the most recent, we will just update the damage time but not remove
	// other damagers from history.)
	if ( m_DamagerHistory[0].hDamager != hDamager )
	{
		for ( int i = 1; i < ARRAYSIZE( m_DamagerHistory ); i++ )
		{
			m_DamagerHistory[i] = m_DamagerHistory[i-1];
		}		
	}	
	// set this damager as most recent and note the time
	m_DamagerHistory[0].hDamager = hDamager;
	m_DamagerHistory[0].flTimeDamage = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Clears damager history
//-----------------------------------------------------------------------------
void CTFPlayer::ClearDamagerHistory()
{
	for ( int i = 0; i < ARRAYSIZE( m_DamagerHistory ); i++ )
	{
		m_DamagerHistory[i].Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldGib( const CTakeDamageInfo &info )
{
	// Check to see if we should allow players to gib.
	if ( !tf_playergib.GetBool() )
		return false;

	if ( ( ( info.GetDamageType() & DMG_BLAST ) != 0 ) || ( ( info.GetDamageType() & DMG_HALF_FALLOFF ) != 0 ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther( pVictim, info );

	if ( pVictim->IsPlayer() )
	{
		CTFPlayer *pTFVictim = ToTFPlayer(pVictim);

		// Custom death handlers
		const char *pszCustomDeath = "customdeath:none";
		if ( info.GetAttacker() && info.GetAttacker()->IsBaseObject() )
		{
			pszCustomDeath = "customdeath:sentrygun";
		}
		else if ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		{
			pszCustomDeath = "customdeath:sentrygun";
		}
		else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_HEADSHOT )
		{				
			pszCustomDeath = "customdeath:headshot";
		}
		else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BACKSTAB )
		{
			pszCustomDeath = "customdeath:backstab";
		}
		else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BURNING )
		{
			pszCustomDeath = "customdeath:burning";
		}

		// Revenge handler
		const char *pszDomination = "domination:none";
		if ( pTFVictim->GetDeathFlags() & (TF_DEATH_REVENGE|TF_DEATH_ASSISTER_REVENGE) )
		{
			pszDomination = "domination:revenge";
		}
		else if ( pTFVictim->GetDeathFlags() & TF_DEATH_DOMINATION )
		{
			pszDomination = "domination:dominated";
		}

		CFmtStrN<128> modifiers( "%s,%s,victimclass:%s", pszCustomDeath, pszDomination, g_aPlayerClassNames_NonLocalized[ pTFVictim->GetPlayerClass()->GetClassIndex() ] );
		SpeakConceptIfAllowed( MP_CONCEPT_KILLED_PLAYER, modifiers );
	}
	else
	{
		if ( pVictim->IsBaseObject() )
		{
			CBaseObject *pObject = dynamic_cast<CBaseObject *>( pVictim );
			SpeakConceptIfAllowed( MP_CONCEPT_KILLED_OBJECT, pObject->GetResponseRulesModifier() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	SpeakConceptIfAllowed( MP_CONCEPT_DIED );

	StateTransition( TF_STATE_DYING );	// Transition into the dying state.

	CTFPlayer *pPlayerAttacker = NULL;
	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		pPlayerAttacker = ToTFPlayer( info.GetAttacker() );
	}

	bool bDisguised = m_Shared.InCond( TF_COND_DISGUISED );
	// we want the rag doll to burn if the player was burning and was not a pryo (who only burns momentarily)
	bool bBurning = m_Shared.InCond( TF_COND_BURNING ) && ( TF_CLASS_PYRO != GetPlayerClass()->GetClassIndex() );

	// Remove all conditions...
	m_Shared.RemoveAllCond( NULL );

	// Reset our model if we were disguised
	if ( bDisguised )
	{
		UpdateModel();
	}

	RemoveTeleportEffect();

	// Stop being invisible
	m_Shared.RemoveCond( TF_COND_STEALTHED );

	// Drop a pack with their leftover ammo
	DropAmmoPack();

	// If the player has a capture flag and was killed by another player, award that player a defense
	if ( HasItem() && pPlayerAttacker && ( pPlayerAttacker != this ) )
	{
		CCaptureFlag *pCaptureFlag = dynamic_cast<CCaptureFlag *>( GetItem() );
		if ( pCaptureFlag )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", pPlayerAttacker->entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DEFEND );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}
			CTF_GameStats.Event_PlayerDefendedPoint( pPlayerAttacker );
		}
	}

	// Remove all items...
	RemoveAllItems( true );

	for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		CTFWeaponBase *pWeapon = (CTFWeaponBase *)GetWeapon( iWeapon );

		if ( pWeapon )
		{
			pWeapon->WeaponReset();
		}
	}

	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->SendViewModelAnim( ACT_IDLE );
		GetActiveWeapon()->Holster();
		SetActiveWeapon( NULL );
	}

	ClearZoomOwner();

	m_vecLastDeathPosition = GetAbsOrigin();

	CTakeDamageInfo info_modified = info;

	// Ragdoll, gib, or death animation.
	bool bRagdoll = true;
	bool bGib = false;

	// See if we should gib.
	if ( ShouldGib( info ) )
	{
		bGib = true;
		bRagdoll = false;
	}
	else
	// See if we should play a custom death animation.
	{
		if ( PlayDeathAnimation( info, info_modified ) )
		{
			bRagdoll = false;
		}
	}

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	if( pPlayerAttacker )
	{
		// See if we were killed by a sentrygun. If so, look at that instead of the player
		if ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		{
			// Catches the case where we're killed directly by the sentrygun (i.e. bullets)
			// Look at the sentrygun
			m_hObserverTarget.Set( info.GetInflictor() ); 
		}
		// See if we were killed by a projectile emitted from a base object. The attacker
		// will still be the owner of that object, but we want the deathcam to point to the 
		// object itself.
		else if ( info.GetInflictor() && info.GetInflictor()->GetOwnerEntity() && 
					info.GetInflictor()->GetOwnerEntity()->IsBaseObject() )
		{
			m_hObserverTarget.Set( info.GetInflictor()->GetOwnerEntity() );
		}
		else
		{
			// Look at the player
			m_hObserverTarget.Set( info.GetAttacker() ); 
		}

		// reset fov to default
		SetFOV( this, 0 );
	}
	else if ( info.GetAttacker() && info.GetAttacker()->IsBaseObject() )
	{
		// Catches the case where we're killed by entities spawned by the sentrygun (i.e. rockets)
		// Look at the sentrygun. 
		m_hObserverTarget.Set( info.GetAttacker() ); 
	}
	else
	{
		m_hObserverTarget.Set( NULL );
	}

	if ( info_modified.GetDamageCustom() == TF_DMG_CUSTOM_SUICIDE )
	{
		// if this was suicide, recalculate attacker to see if we want to award the kill to a recent damager
		info_modified.SetAttacker( TFGameRules()->GetDeathScorer( info.GetAttacker(), info.GetInflictor(), this ) );
	}

	BaseClass::Event_Killed( info_modified );

	CTFPlayer *pInflictor = ToTFPlayer( info.GetInflictor() );
	if ( ( TF_DMG_CUSTOM_HEADSHOT == info.GetDamageCustom() ) && pInflictor )
	{				
		CTF_GameStats.Event_Headshot( pInflictor );
	}
	else if ( ( TF_DMG_CUSTOM_BACKSTAB == info.GetDamageCustom() ) && pInflictor )
	{
		CTF_GameStats.Event_Backstab( pInflictor );
	}

	// Create the ragdoll entity.
	if ( bGib || bRagdoll )
	{
		CreateRagdollEntity( bGib, bBurning );
	}

	// Don't overflow the value for this.
	m_iHealth = 0;

	// If we died in sudden death and we're an engineer, explode our buildings
	if ( IsPlayerClass( TF_CLASS_ENGINEER ) && TFGameRules()->InStalemate() )
	{
		for (int i = GetObjectCount()-1; i >= 0; i--)
		{
			CBaseObject *obj = GetObject(i);
			Assert( obj );

			if ( obj )
			{
				obj->DetonateObject();
			}		
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayDeathAnimation( const CTakeDamageInfo &info, CTakeDamageInfo &info_modified )
{
	// No supporting this for the initial release.
	return false;

	if ( SelectWeightedSequence( ACT_DIESIMPLE ) == -1 )
		return false;

	// Get the attacking player.
	CTFPlayer *pAttacker = (CTFPlayer*)ToTFPlayer( info.GetAttacker() );
	if ( !pAttacker )
		return false;

	bool bPlayDeathAnim = false;

	// Check for a sniper headshot. (Currently only on Heavy.)
	if ( pAttacker->GetPlayerClass()->IsClass( TF_CLASS_SNIPER ) && ( info.GetDamageCustom() == TF_DMG_CUSTOM_HEADSHOT ) )
	{
		if ( GetPlayerClass()->IsClass( TF_CLASS_HEAVYWEAPONS ) )
		{
			bPlayDeathAnim = true;
		}
	}
	// Check for a spy backstab. (Currently only on Sniper.)
	else if ( pAttacker->GetPlayerClass()->IsClass( TF_CLASS_SPY ) && ( info.GetDamageCustom() == TF_DMG_CUSTOM_BACKSTAB ) )
	{
		if ( GetPlayerClass()->IsClass( TF_CLASS_SNIPER ) )
		{
			bPlayDeathAnim = true;
		}
	}

	// Play death animation?
	if ( bPlayDeathAnim )
	{
		info_modified.SetDamageType( info_modified.GetDamageType() | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );

		SetAbsVelocity( vec3_origin );
		DoAnimationEvent( PLAYERANIMEVENT_DIE );

		// No ragdoll yet.
		if ( m_hRagdoll.Get() )
		{
			UTIL_Remove( m_hRagdoll );
		}
	}

	return bPlayDeathAnim;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWeapon - 
//			&vecOrigin - 
//			&vecAngles - 
//-----------------------------------------------------------------------------
bool CTFPlayer::CalculateAmmoPackPositionAndAngles( CTFWeaponBase *pWeapon, Vector &vecOrigin, QAngle &vecAngles )
{
	// Look up the hand and weapon bones.
	int iHandBone = LookupBone( "weapon_bone" );
	if ( iHandBone == -1 )
		return false;

	GetBonePosition( iHandBone, vecOrigin, vecAngles );

	// Draw the position and angles.
	Vector vecDebugForward2, vecDebugRight2, vecDebugUp2;
	AngleVectors( vecAngles, &vecDebugForward2, &vecDebugRight2, &vecDebugUp2 );

	/*
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugForward2 * 25.0f ), 255, 0, 0, false, 30.0f );
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugRight2 * 25.0f ), 0, 255, 0, false, 30.0f );
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugUp2 * 25.0f ), 0, 0, 255, false, 30.0f ); 
	*/

	VectorAngles( vecDebugUp2, vecAngles );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// NOTE: If we don't let players drop ammo boxes, we don't need this code..
//-----------------------------------------------------------------------------
void CTFPlayer::AmmoPackCleanUp( void )
{
	// If we have more than 3 ammo packs out now, destroy the oldest one.
	int iNumPacks = 0;
	CTFAmmoPack *pOldestBox = NULL;

	// Cycle through all ammobox in the world and remove them
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "tf_ammo_pack" );
	while ( pEnt )
	{
		CBaseEntity *pOwner = pEnt->GetOwnerEntity();
		if (pOwner == this)
		{
			CTFAmmoPack *pThisBox = dynamic_cast<CTFAmmoPack *>( pEnt );
			Assert( pThisBox );
			if ( pThisBox )
			{
				iNumPacks++;

				// Find the oldest one
				if ( pOldestBox == NULL || pOldestBox->GetCreationTime() > pThisBox->GetCreationTime() )
				{
					pOldestBox = pThisBox;
				}
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "tf_ammo_pack" );
	}

	// If they have more than 3 packs active, remove the oldest one
	if ( iNumPacks > 3 && pOldestBox )
	{
		UTIL_Remove( pOldestBox );
	}
}		

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DropAmmoPack( void )
{
	// We want the ammo packs to look like the player's weapon model they were carrying.
	// except if they are melee or building weapons
	CTFWeaponBase *pWeapon = NULL;
	CTFWeaponBase *pActiveWeapon = m_Shared.GetActiveTFWeapon();

	if ( !pActiveWeapon || pActiveWeapon->GetTFWpnData().m_bDontDrop )
	{
		// Don't drop this one, find another one to drop

		int iWeight = -1;

		// find the highest weighted weapon
		for (int i = 0;i < WeaponCount(); i++) 
		{
			CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon(i);
			if ( !pWpn )
				continue;

			if ( pWpn->GetTFWpnData().m_bDontDrop )
				continue;

			int iThisWeight = pWpn->GetTFWpnData().iWeight;

			if ( iThisWeight > iWeight )
			{
				iWeight = iThisWeight;
				pWeapon = pWpn;
			}
		}
	}
	else
	{
		pWeapon = pActiveWeapon;
	}

	// If we didn't find one, bail
	if ( !pWeapon )
		return;

	// We need to find bones on the world model, so switch the weapon to it.
	const char *pszWorldModel = pWeapon->GetWorldModel();
	pWeapon->SetModel( pszWorldModel );


	// Find the position and angle of the weapons so the "ammo box" matches.
	Vector vecPackOrigin;
	QAngle vecPackAngles;
	if( !CalculateAmmoPackPositionAndAngles( pWeapon, vecPackOrigin, vecPackAngles ) )
		return;

	// Fill the ammo pack with unused player ammo, if out add a minimum amount.
	int iPrimary = MAX( 5, GetAmmoCount( TF_AMMO_PRIMARY ) );
	int iSecondary = MAX( 5, GetAmmoCount( TF_AMMO_SECONDARY ) );
	int iMetal = MAX( 5, GetAmmoCount( TF_AMMO_METAL ) );	

	// Create the ammo pack.
	CTFAmmoPack *pAmmoPack = CTFAmmoPack::Create( vecPackOrigin, vecPackAngles, this, pszWorldModel );
	Assert( pAmmoPack );
	if ( pAmmoPack )
	{
		// Remove all of the players ammo.
		RemoveAllAmmo();

		// Fill up the ammo pack.
		pAmmoPack->GiveAmmo( iPrimary, TF_AMMO_PRIMARY );
		pAmmoPack->GiveAmmo( iSecondary, TF_AMMO_SECONDARY );
		pAmmoPack->GiveAmmo( iMetal, TF_AMMO_METAL );

		Vector vecRight, vecUp;
		AngleVectors( EyeAngles(), NULL, &vecRight, &vecUp );

		// Calculate the initial impulse on the weapon.
		Vector vecImpulse( 0.0f, 0.0f, 0.0f );
		vecImpulse += vecUp * random->RandomFloat( -0.25, 0.25 );
		vecImpulse += vecRight * random->RandomFloat( -0.25, 0.25 );
		VectorNormalize( vecImpulse );
		vecImpulse *= random->RandomFloat( tf_weapon_ragdoll_velocity_min.GetFloat(), tf_weapon_ragdoll_velocity_max.GetFloat() );
		vecImpulse += GetAbsVelocity();

		// Cap the impulse.
		float flSpeed = vecImpulse.Length();
		if ( flSpeed > tf_weapon_ragdoll_maxspeed.GetFloat() )
		{
			VectorScale( vecImpulse, tf_weapon_ragdoll_maxspeed.GetFloat() / flSpeed, vecImpulse );
		}

		if ( pAmmoPack->VPhysicsGetObject() )
		{
			// We can probably remove this when the mass on the weapons is correct!
			pAmmoPack->VPhysicsGetObject()->SetMass( 25.0f );
			AngularImpulse angImpulse( 0, random->RandomFloat( 0, 100 ), 0 );
			pAmmoPack->VPhysicsGetObject()->SetVelocityInstantaneous( &vecImpulse, &angImpulse );
		}

		pAmmoPack->SetInitialVelocity( vecImpulse );

		pAmmoPack->m_nSkin = ( GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;

		// Give the ammo pack some health, so that trains can destroy it.
		pAmmoPack->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		pAmmoPack->m_takedamage = DAMAGE_YES;		
		pAmmoPack->SetHealth( 900 );
		
		pAmmoPack->SetBodygroup( 1, 1 );
	
		// Clean up old ammo packs if they exist in the world
		AmmoPackCleanUp();	
	}	
	pWeapon->SetModel( pWeapon->GetViewModel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PlayerDeathThink( void )
{
	//overridden, do nothing
}

//-----------------------------------------------------------------------------
// Purpose: Remove the tf items from the player then call into the base class
//          removal of items.
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveAllItems( bool removeSuit )
{
	// If the player has a capture flag, drop it.
	if ( HasItem() )
	{
		GetItem()->Drop( this, true );

		IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
		if ( event )
		{
			event->SetInt( "player", entindex() );
			event->SetInt( "eventtype", TF_FLAGEVENT_DROPPED );
			event->SetInt( "priority", 8 );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( m_hOffHandWeapon.Get() )
	{ 
		HolsterOffHandWeapon();

		// hide the weapon model
		// don't normally have to do this, unless we have a holster animation
		CBaseViewModel *vm = GetViewModel( 1 );
		if ( vm )
		{
			vm->SetWeaponModel( NULL, NULL );
		}

		m_hOffHandWeapon = NULL;
	}

	Weapon_SetLast( NULL );
	UpdateClientData();
}

void CTFPlayer::ClientHearVox( const char *pSentence )
{
	//TFTODO: implement this.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateModel( void )
{
	SetModel( GetPlayerClass()->GetModelName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSkin - 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateSkin( int iTeam )
{
	// The player's skin is team - 2.
	int iSkin = iTeam - 2;

	// Check to see if the skin actually changed.
	if ( iSkin != m_iLastSkin )
	{
		m_nSkin = iSkin;
		m_iLastSkin = iSkin;
	}
}

//=========================================================================
// Displays the state of the items specified by the Goal passed in
void CTFPlayer::DisplayLocalItemStatus( CTFGoal *pGoal )
{
#if 0
	for (int i = 0; i < 4; i++)
	{
		if (pGoal->display_item_status[i] != 0)
		{
			CTFGoalItem *pItem = Finditem(pGoal->display_item_status[i]);
			if (pItem)
				DisplayItemStatus(pGoal, this, pItem);
			else
				ClientPrint( this, HUD_PRINTTALK, "#Item_missing" );
		}
	}
#endif
}

//=========================================================================
// Called when the player disconnects from the server.
void CTFPlayer::TeamFortress_ClientDisconnected( void )
{
	TeamFortress_RemoveEverythingFromWorld();
	RemoveNemesisRelationships();

	RemoveAllWeapons();
}

//=========================================================================
// Removes everything this player has (buildings, grenades, etc.) from the world
void CTFPlayer::TeamFortress_RemoveEverythingFromWorld( void )
{
	TeamFortress_RemoveRockets();
	TeamFortress_RemovePipebombs();

	// Destroy any buildables - this should replace TeamFortress_RemoveBuildings
	RemoveAllObjects();
}

//=========================================================================
// Removes all rockets the player has fired into the world
// (this prevents a team kill cheat where players would fire rockets 
// then change teams to kill their own team)
void CTFPlayer::TeamFortress_RemoveRockets( void )
{
	RemoveOwnedEnt( "tf_weapon_rocket" );
	RemoveOwnedEnt( "tf_weapon_flamerocket" );
}

//=========================================================================
// Removes all pipebombs from the world
void CTFPlayer::TeamFortress_RemovePipebombs( void )
{
	CTFPlayerClass *pClass = GetPlayerClass();
	if ( pClass && pClass->GetClassIndex() == TF_CLASS_DEMOMAN )
	{
		RemoveOwnedEnt( "tf_projectile_pipe", true );
	}
}


//=========================================================================
// Remove all of an ent owned by this player
void CTFPlayer::RemoveOwnedEnt( char *pEntName, bool bGrenade )
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, pEntName );
	while ( pEnt )
	{
		// if the player owns this entity, remove it
		bool bOwner = (pEnt->GetOwnerEntity() == this);

		if ( !bOwner && bGrenade )
		{
			CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade*>(pEnt);
			Assert( pGrenade );
			if ( pGrenade )
			{
				bOwner = (pGrenade->GetThrower() == this);
			}
		}

		if ( bOwner )
		{
			pEnt->SetThink( &BaseClass::SUB_Remove );
			pEnt->SetNextThink( gpGlobals->curtime );
			pEnt->SetTouch( NULL );
			pEnt->AddEffects( EF_NODRAW );
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, pEntName );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::NoteWeaponFired()
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

//=============================================================================
//
// Player state functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CPlayerStateInfo *CTFPlayer::StateLookupInfo( int nState )
{
	// This table MUST match the 
	static CPlayerStateInfo playerStateInfos[] =
	{
		{ TF_STATE_ACTIVE,				"TF_STATE_ACTIVE",				&CTFPlayer::StateEnterACTIVE,				NULL,	NULL },
		{ TF_STATE_WELCOME,				"TF_STATE_WELCOME",				&CTFPlayer::StateEnterWELCOME,				NULL,	&CTFPlayer::StateThinkWELCOME },
		{ TF_STATE_OBSERVER,			"TF_STATE_OBSERVER",			&CTFPlayer::StateEnterOBSERVER,				NULL,	&CTFPlayer::StateThinkOBSERVER },
		{ TF_STATE_DYING,				"TF_STATE_DYING",				&CTFPlayer::StateEnterDYING,				NULL,	&CTFPlayer::StateThinkDYING },
	};

	for ( int iState = 0; iState < ARRAYSIZE( playerStateInfos ); ++iState )
	{
		if ( playerStateInfos[iState].m_nPlayerState == nState )
			return &playerStateInfos[iState];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnter( int nState )
{
	m_Shared.m_nPlayerState = nState;
	m_pStateInfo = StateLookupInfo( nState );

	if ( tf_playerstatetransitions.GetInt() == -1 || tf_playerstatetransitions.GetInt() == entindex() )
	{
		if ( m_pStateInfo )
			Msg( "ShowStateTransitions: entering '%s'\n", m_pStateInfo->m_pStateName );
		else
			Msg( "ShowStateTransitions: entering #%d\n", nState );
	}

	// Initialize the new state.
	if ( m_pStateInfo && m_pStateInfo->pfnEnterState )
	{
		(this->*m_pStateInfo->pfnEnterState)();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateLeave( void )
{
	if ( m_pStateInfo && m_pStateInfo->pfnLeaveState )
	{
		(this->*m_pStateInfo->pfnLeaveState)();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateTransition( int nState )
{
	StateLeave();
	StateEnter( nState );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterWELCOME( void )
{
	PickWelcomeObserverPoint();  
	
	StartObserverMode( OBS_MODE_FIXED );

	// Important to set MOVETYPE_NONE or our physics object will fall while we're sitting at one of the intro cameras.
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );		

	PhysObjectSleep();

	if ( gpGlobals->eLoadType == MapLoad_Background )
	{
		m_bSeenRoundInfo = true;

		ChangeTeam( TEAM_SPECTATOR );
	}
	else if ( (TFGameRules() && TFGameRules()->IsLoadingBugBaitReport()) )
	{
		m_bSeenRoundInfo = true;
		
		ChangeTeam( TF_TEAM_BLUE );
		SetDesiredPlayerClassIndex( TF_CLASS_SCOUT );
		ForceRespawn();
	}
	else if ( IsInCommentaryMode() )
	{
		m_bSeenRoundInfo = true;
	}
	else
	{
		if ( !IsX360() )
		{
			KeyValues *data = new KeyValues( "data" );
			data->SetString( "title", "#TF_Welcome" );	// info panel title
			data->SetString( "type", "1" );				// show userdata from stringtable entry
			data->SetString( "msg",	"motd" );			// use this stringtable entry
			data->SetString( "cmd", "mapinfo" );		// exec this command if panel closed

			ShowViewPortPanel( PANEL_INFO, true, data );

			data->deleteThis();
		}
		else
		{
			ShowViewPortPanel( PANEL_MAPINFO, true );
		}

		m_bSeenRoundInfo = false;
	}

	m_bIsIdle = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkWELCOME( void )
{
	if ( IsInCommentaryMode() && !IsFakeClient() )
	{
		ChangeTeam( TF_TEAM_BLUE );
		SetDesiredPlayerClassIndex( TF_CLASS_SCOUT );
		ForceRespawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveEffects( EF_NODRAW | EF_NOSHADOW );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_Local.m_iHideHUD = 0;
	PhysObjectWake();

	m_flLastAction = gpGlobals->curtime;
	m_bIsIdle = false;

	// If we're a Medic, start thinking to regen myself
	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		SetContextThink( &CTFPlayer::MedicRegenThink, gpGlobals->curtime + TF_MEDIC_REGEN_TIME, "MedicRegenThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetObserverMode(int mode)
{
	if ( mode < OBS_MODE_NONE || mode >= NUM_OBSERVER_MODES )
		return false;

	// Skip OBS_MODE_POI as we're not using that.
	// no POI mode in src2013, remove
	//if ( mode == OBS_MODE_POI )
	//{
	//	mode++;
	//}

	// Skip over OBS_MODE_ROAMING for dead players
	if( GetTeamNumber() > TEAM_SPECTATOR )
	{
		if ( IsDead() && ( mode > OBS_MODE_FIXED ) && mp_fadetoblack.GetBool() )
		{
			mode = OBS_MODE_CHASE;
		}
		else if ( mode == OBS_MODE_ROAMING )
		{
			mode = OBS_MODE_IN_EYE;
		}
	}

	if ( m_iObserverMode > OBS_MODE_DEATHCAM )
	{
		// remember mode if we were really spectating before
		m_iObserverLastMode = m_iObserverMode;
	}

	m_iObserverMode = mode;
	m_flLastAction = gpGlobals->curtime;

	switch ( mode )
	{
	case OBS_MODE_NONE:
	case OBS_MODE_FIXED :
	case OBS_MODE_DEATHCAM :
		SetFOV( this, 0 );	// Reset FOV
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_NONE );
		break;

	case OBS_MODE_CHASE :
	case OBS_MODE_IN_EYE :	
		// udpate FOV and viewmodels
		SetObserverTarget( m_hObserverTarget );	
		SetMoveType( MOVETYPE_OBSERVER );
		break;

	case OBS_MODE_ROAMING :
		SetFOV( this, 0 );	// Reset FOV
		SetObserverTarget( m_hObserverTarget );
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_OBSERVER );
		break;
		
	case OBS_MODE_FREEZECAM:
		SetFOV( this, 0 );	// Reset FOV
		SetObserverTarget( m_hObserverTarget );
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_OBSERVER );
		break;
	}

	CheckObserverSettings();

	return true;	
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterOBSERVER( void )
{
	// Always start a spectator session in chase mode
	m_iObserverLastMode = OBS_MODE_CHASE;

	if( m_hObserverTarget == NULL )
	{
		// find a new observer target
		CheckObserverSettings();
	}

	if ( !m_bAbortFreezeCam )
	{
		FindInitialObserverTarget();
	}

	StartObserverMode( m_iObserverLastMode );

	PhysObjectSleep();

	m_bIsIdle = false;

	if ( GetTeamNumber() != TEAM_SPECTATOR )
	{
		HandleFadeToBlack();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkOBSERVER()
{
	// Make sure nobody has changed any of our state.
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );

	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterDYING( void )
{
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_bPlayedFreezeCamSound = false;
	m_bAbortFreezeCam = false;
}

//-----------------------------------------------------------------------------
// Purpose: Move the player to observer mode once the dying process is over
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkDYING( void )
{
	// If we have a ragdoll, it's time to go to deathcam
	if ( !m_bAbortFreezeCam && m_hRagdoll && 
		(m_lifeState == LIFE_DYING || m_lifeState == LIFE_DEAD) && 
		GetObserverMode() != OBS_MODE_FREEZECAM )
	{
		if ( GetObserverMode() != OBS_MODE_DEATHCAM )
		{
			StartObserverMode( OBS_MODE_DEATHCAM );	// go to observer mode
		}
		RemoveEffects( EF_NODRAW | EF_NOSHADOW );	// still draw player body
	}

	float flTimeInFreeze = spec_freeze_traveltime.GetFloat() + spec_freeze_time.GetFloat();
	float flFreezeEnd = (m_flDeathTime + TF_DEATH_ANIMATION_TIME + flTimeInFreeze );
	if ( !m_bPlayedFreezeCamSound  && GetObserverTarget() && GetObserverTarget() != this )
	{
		// Start the sound so that it ends at the freezecam lock on time
		float flFreezeSoundLength = 0.3;
		float flFreezeSoundTime = (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) + spec_freeze_traveltime.GetFloat() - flFreezeSoundLength;
		if ( gpGlobals->curtime >= flFreezeSoundTime )
		{
			CSingleUserRecipientFilter filter( this );
			EmitSound_t params;
			params.m_flSoundTime = 0;
			params.m_pSoundName = "TFPlayer.FreezeCam";
			EmitSound( filter, entindex(), params );

			m_bPlayedFreezeCamSound = true;
		}
	}

	if ( gpGlobals->curtime >= (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) )	// allow x seconds death animation / death cam
	{
		if ( GetObserverTarget() && GetObserverTarget() != this )
		{
			if ( !m_bAbortFreezeCam && gpGlobals->curtime < flFreezeEnd )
			{
				if ( GetObserverMode() != OBS_MODE_FREEZECAM )
				{
					StartObserverMode( OBS_MODE_FREEZECAM );
					PhysObjectSleep();
				}
				return;
			}
		}

		if ( GetObserverMode() == OBS_MODE_FREEZECAM )
		{
			// If we're in freezecam, and we want out, abort.  (only if server is not using mp_fadetoblack)
			if ( m_bAbortFreezeCam && !mp_fadetoblack.GetBool() )
			{
				if ( m_hObserverTarget == NULL )
				{
					// find a new observer target
					CheckObserverSettings();
				}

				FindInitialObserverTarget();
				SetObserverMode( OBS_MODE_CHASE );
				ShowViewPortPanel( "specgui" , ModeWantsSpectatorGUI(OBS_MODE_CHASE) );
			}
		}

		// Don't allow anyone to respawn until freeze time is over, even if they're not
		// in freezecam. This prevents players skipping freezecam to spawn faster.
		if ( gpGlobals->curtime < flFreezeEnd )
			return;

		m_lifeState = LIFE_RESPAWNABLE;

		StopAnimation();

		AddEffects( EF_NOINTERP );

		if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND) )
			SetMoveType( MOVETYPE_NONE );

		StateTransition( TF_STATE_OBSERVER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::AttemptToExitFreezeCam( void )
{
	float flFreezeTravelTime = (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) + spec_freeze_traveltime.GetFloat() + 0.5;
	if ( gpGlobals->curtime < flFreezeTravelTime )
		return;

	m_bAbortFreezeCam = true;
}

class CIntroViewpoint : public CPointEntity
{
	DECLARE_CLASS( CIntroViewpoint, CPointEntity );
public:
	DECLARE_DATADESC();

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	int			m_iIntroStep;
	float		m_flStepDelay;
	string_t	m_iszMessage;
	string_t	m_iszGameEvent;
	float		m_flEventDelay;
	int			m_iGameEventData;
	float		m_flFOV;
};

BEGIN_DATADESC( CIntroViewpoint )
	DEFINE_KEYFIELD( m_iIntroStep,	FIELD_INTEGER,	"step_number" ),
	DEFINE_KEYFIELD( m_flStepDelay,	FIELD_FLOAT,	"time_delay" ),
	DEFINE_KEYFIELD( m_iszMessage,	FIELD_STRING,	"hint_message" ),
	DEFINE_KEYFIELD( m_iszGameEvent,	FIELD_STRING,	"event_to_fire" ),
	DEFINE_KEYFIELD( m_flEventDelay,	FIELD_FLOAT,	"event_delay" ),
	DEFINE_KEYFIELD( m_iGameEventData,	FIELD_INTEGER,	"event_data_int" ),
	DEFINE_KEYFIELD( m_flFOV,	FIELD_FLOAT,	"fov" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( game_intro_viewpoint, CIntroViewpoint );

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
// Input  : iCount - Amount of ammo to give.
//			iAmmoIndex - Index of the ammo into the AmmoInfoArray
//			iMax - Max carrying capability of the player
// Output : Amount of ammo actually given
//-----------------------------------------------------------------------------
int CTFPlayer::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound )
{
	if ( iCount <= 0 )
	{
        return 0;
	}

	if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	{
		// game rules say I can't have any more of this ammo type.
		return 0;
	}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
	{
		return 0;
	}

	int iMax = m_PlayerClass.GetData()->m_aAmmoMax[iAmmoIndex];
	int iAdd = MIN( iCount, iMax - GetAmmoCount(iAmmoIndex) );
	if ( iAdd < 1 )
	{
		return 0;
	}

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	CBaseCombatCharacter::GiveAmmo( iAdd, iAmmoIndex );
	return iAdd;
}


//-----------------------------------------------------------------------------
// Purpose: Reset player's information and force him to spawn
//-----------------------------------------------------------------------------
void CTFPlayer::ForceRespawn( void )
{
	CTF_GameStats.Event_PlayerForceRespawn( this );

	m_flSpawnTime = gpGlobals->curtime;

	int iDesiredClass = GetDesiredPlayerClassIndex();

	if ( iDesiredClass == TF_CLASS_UNDEFINED )
	{
		return;
	}

	if ( iDesiredClass == TF_CLASS_RANDOM )
	{
		// Don't let them be the same class twice in a row
		do{
			iDesiredClass = random->RandomInt( TF_FIRST_NORMAL_CLASS, TF_LAST_NORMAL_CLASS );
		} while( iDesiredClass == GetPlayerClass()->GetClassIndex() );
	}

	if ( HasTheFlag() )
	{
		DropFlag();
	}

	if ( GetPlayerClass()->GetClassIndex() != iDesiredClass )
	{
		// clean up any pipebombs/buildings in the world (no explosions)
		TeamFortress_RemoveEverythingFromWorld();

		GetPlayerClass()->Init( iDesiredClass );

		CTF_GameStats.Event_PlayerChangedClass( this );
	}

	m_Shared.RemoveAllCond( NULL );

	RemoveAllItems( true );

	// Reset ground state for airwalk animations
	SetGroundEntity( NULL );

	// TODO: move this into conditions
	RemoveTeleportEffect();

	// remove invisibility very quickly	
	m_Shared.FadeInvis( 0.1 );

	// Stop any firing that was taking place before respawn.
	m_nButtons = 0;

	StateTransition( TF_STATE_ACTIVE );
	Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Do nothing multiplayer_animstate takes care of animation.
// Input  : playerAnim - 
//-----------------------------------------------------------------------------
void CTFPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Handle cheat commands
// Input  : iImpulse - 
//-----------------------------------------------------------------------------
void CTFPlayer::CheatImpulseCommands( int iImpulse )
{
	switch( iImpulse )
	{
	case 101:
		{
			if( sv_cheats->GetBool() )
			{
				extern int gEvilImpulse101;
				gEvilImpulse101 = true;

				GiveAmmo( 1000, TF_AMMO_PRIMARY );
				GiveAmmo( 1000, TF_AMMO_SECONDARY );
				GiveAmmo( 1000, TF_AMMO_METAL );
				TakeHealth( 999, DMG_GENERIC );

				gEvilImpulse101 = false;
			}
		}
		break;

	default:
		{
			BaseClass::CheatImpulseCommands( iImpulse );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetWeaponBuilder( CTFWeaponBuilder *pBuilder )
{
	m_hWeaponBuilder = pBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBuilder *CTFPlayer::GetWeaponBuilder( void )
{
	Assert( 0 );
	return m_hWeaponBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this player is building something
//-----------------------------------------------------------------------------
bool CTFPlayer::IsBuilding( void )
{
	/*
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
		return pBuilder->IsBuilding();
		*/

	return false;
}

void CTFPlayer::RemoveBuildResources( int iAmount )
{
	RemoveAmmo( iAmount, TF_AMMO_METAL );
}

void CTFPlayer::AddBuildResources( int iAmount )
{
	GiveAmmo( iAmount, TF_AMMO_METAL );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject	*CTFPlayer::GetObject( int index )
{
	return (CBaseObject *)( m_aObjects[index].Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayer::GetObjectCount( void )
{
	return m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all the player's objects
//			If bForceAll is set, remove all of them immediately.
//			Otherwise, make them all deteriorate over time.
//			If iClass is passed in, don't remove any objects that can be built 
//			by that class. If bReturnResources is set, the cost of any destroyed 
//			objects will be returned to the player.
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveAllObjects( void )
{
	// Remove all the player's objects
	for (int i = GetObjectCount()-1; i >= 0; i--)
	{
		CBaseObject *obj = GetObject(i);
		Assert( obj );

		if ( obj )
		{
			UTIL_Remove( obj );
		}		
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StopPlacement( void )
{
	/*
	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StopPlacement();
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Player has started building an object
//-----------------------------------------------------------------------------
int	CTFPlayer::StartedBuildingObject( int iObjectType )
{
	// Deduct the cost of the object
	int iCost = CalculateObjectCost( iObjectType );
	if ( iCost > GetBuildResources() )
	{
		// Player must have lost resources since he started placing
		return 0;
	}

	RemoveBuildResources( iCost );

	// If the object costs 0, we need to return non-0 to mean success
	if ( !iCost )
		return 1;

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: Player has aborted building something
//-----------------------------------------------------------------------------
void CTFPlayer::StoppedBuilding( int iObjectType )
{
	/*
	int iCost = CalculateObjectCost( iObjectType );

	AddBuildResources( iCost );

	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StoppedBuilding( iObjectType );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CTFPlayer::FinishedObject( CBaseObject *pObject )
{
	AddObject( pObject );
	CTF_GameStats.Event_PlayerCreatedBuilding( this, pObject );

	/*
	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->FinishedObject();
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified object to this player's object list.
//-----------------------------------------------------------------------------
void CTFPlayer::AddObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::AddObject adding object %p:%s to player %s\n", gpGlobals->curtime, pObject, pObject->GetClassname(), GetPlayerName() ) );

	// Make a handle out of it
	CHandle<CBaseObject> hObject;
	hObject = pObject;

	bool alreadyInList = PlayerOwnsObject( pObject );
	Assert( !alreadyInList );
	if ( !alreadyInList )
	{
		m_aObjects.AddToTail( hObject );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Object built by this player has been destroyed
//-----------------------------------------------------------------------------
void CTFPlayer::OwnedObjectDestroyed( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::OwnedObjectDestroyed player %s object %p:%s\n", gpGlobals->curtime, 
		GetPlayerName(),
		pObject,
		pObject->GetClassname() ) );

	RemoveObject( pObject );

	// Tell our builder weapon so it recalculates the state of the build icons
	/*
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->RecalcState();
	}
	*/
}


//-----------------------------------------------------------------------------
// Removes an object from the player
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::RemoveObject %p:%s from player %s\n", gpGlobals->curtime, 
		pObject,
		pObject->GetClassname(),
		GetPlayerName() ) );

	Assert( pObject );

	int i;
	for ( i = m_aObjects.Count(); --i >= 0; )
	{
		// Also, while we're at it, remove all other bogus ones too...
		if ( (!m_aObjects[i].Get()) || (m_aObjects[i] == pObject))
		{
			m_aObjects.FastRemove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// See if the player owns this object
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayerOwnsObject( CBaseObject *pObject )
{
	return ( m_aObjects.Find( pObject ) != -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PlayFlinch( const CTakeDamageInfo &info )
{
	// Don't play flinches if we just died. 
	if ( !IsAlive() )
		return;

	// No pain flinches while disguised, our man has supreme discipline
	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		return;

	PlayerAnimEvent_t flinchEvent;

	switch ( LastHitGroup() )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchEvent = PLAYERANIMEVENT_FLINCH_HEAD;
		break;
	case HITGROUP_LEFTARM:
		flinchEvent = PLAYERANIMEVENT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
		flinchEvent = PLAYERANIMEVENT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
		flinchEvent = PLAYERANIMEVENT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
		flinchEvent = PLAYERANIMEVENT_FLINCH_RIGHTLEG;
		break;
	case HITGROUP_STOMACH:
	case HITGROUP_CHEST:
	case HITGROUP_GEAR:
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		flinchEvent = PLAYERANIMEVENT_FLINCH_CHEST;
		break;
	}

	DoAnimationEvent( flinchEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Plays the crit sound that players that get crit hear
//-----------------------------------------------------------------------------
float CTFPlayer::PlayCritReceivedSound( void )
{
	float flCritPainLength = 0;
	// Play a custom pain sound to the guy taking the damage
	CSingleUserRecipientFilter receiverfilter( this );
	EmitSound_t params;
	params.m_flSoundTime = 0;
	params.m_pSoundName = "TFPlayer.CritPain";
	params.m_pflSoundDuration = &flCritPainLength;
	EmitSound( receiverfilter, entindex(), params );

	return flCritPainLength;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PainSound( const CTakeDamageInfo &info )
{
	// Don't make sounds if we just died. DeathSound will handle that.
	if ( !IsAlive() )
		return;

	// no pain sounds while disguised, our man has supreme discipline
	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		return;

	if ( m_flNextPainSoundTime > gpGlobals->curtime )
		return;

	// Don't play falling pain sounds, they have their own system
	if ( info.GetDamageType() & DMG_FALL )
		return;

	if ( info.GetDamageType() & DMG_DROWN )
	{
		EmitSound( "TFPlayer.Drown" );
		return;
	}

	if ( info.GetDamageType() & DMG_BURN )
	{
		// Looping fire pain sound is done in CTFPlayerShared::ConditionThink
		return;
	}

	float flPainLength = 0;

	bool bAttackerIsPlayer = ( info.GetAttacker() && info.GetAttacker()->IsPlayer() );

	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );

	pExpresser->AllowMultipleScenes();

	// speak a pain concept here, send to everyone but the attacker
	CPASFilter filter( GetAbsOrigin() );

	if ( bAttackerIsPlayer )
	{
		filter.RemoveRecipient( ToBasePlayer( info.GetAttacker() ) );
	}

	// play a crit sound to the victim ( us )
	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		flPainLength = PlayCritReceivedSound();

		// remove us from hearing our own pain sound if we hear the crit sound
		filter.RemoveRecipient( this );
	}

	char szResponse[AI_Response::MAX_RESPONSE_NAME];

	if ( SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_PAIN, "damagecritical:1", szResponse, AI_Response::MAX_RESPONSE_NAME, &filter ) )
	{
		flPainLength = MAX( GetSceneDuration( szResponse ), flPainLength );
	}

	// speak a louder pain concept to just the attacker
	if ( bAttackerIsPlayer )
	{
		CSingleUserRecipientFilter attackerFilter( ToBasePlayer( info.GetAttacker() ) );
		SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_ATTACKER_PAIN, "damagecritical:1", szResponse, AI_Response::MAX_RESPONSE_NAME, &attackerFilter );
	}

	pExpresser->DisallowMultipleScenes();

	m_flNextPainSoundTime = gpGlobals->curtime + flPainLength;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DeathSound( const CTakeDamageInfo &info )
{
	// Don't make death sounds when choosing a class
	if ( IsPlayerClass( TF_CLASS_UNDEFINED ) )
		return;

	TFPlayerClassData_t *pData = GetPlayerClass()->GetData();
	if ( !pData )
		return;

	if ( m_LastDamageType & DMG_FALL ) // Did we die from falling?
	{
		// They died in the fall. Play a splat sound.
		EmitSound( "Player.FallGib" );
	}
	else if ( m_LastDamageType & DMG_BLAST )
	{
		EmitSound( pData->m_szExplosionDeathSound );
	}
	else if ( m_LastDamageType & DMG_CRITICAL )
	{
		EmitSound( pData->m_szCritDeathSound );

		PlayCritReceivedSound();
	}
	else if ( m_LastDamageType & DMG_CLUB )
	{
		EmitSound( pData->m_szMeleeDeathSound );
	}
	else
	{
		EmitSound( pData->m_szDeathSound );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when this player burns another player
//-----------------------------------------------------------------------------
void CTFPlayer::OnBurnOther( CTFPlayer *pTFPlayerVictim )
{
#define ACHIEVEMENT_BURN_TIME_WINDOW	30.0f
#define ACHIEVEMENT_BURN_VICTIMS	5
	// add current time we burned another player to head of vector
	m_aBurnOtherTimes.AddToHead( gpGlobals->curtime );

	// remove any burn times that are older than the burn window from the list
	float flTimeDiscard = gpGlobals->curtime - ACHIEVEMENT_BURN_TIME_WINDOW;
	for ( int i = 1; i < m_aBurnOtherTimes.Count(); i++ )
	{
		if ( m_aBurnOtherTimes[i] < flTimeDiscard )
		{
			m_aBurnOtherTimes.RemoveMultiple( i, m_aBurnOtherTimes.Count() - i );
			break;
		}
	}

	// see if we've burned enough players in time window to satisfy achievement
	if ( m_aBurnOtherTimes.Count() >= ACHIEVEMENT_BURN_VICTIMS )
	{
		CSingleUserRecipientFilter filter( this );
		UserMessageBegin( filter, "AchievementEvent" );
		WRITE_BYTE( ACHIEVEMENT_TF_BURN_PLAYERSINMINIMIMTIME );
		MessageEnd();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CTFPlayer::GetTFTeam( void )
{
	CTFTeam *pTeam = dynamic_cast<CTFTeam *>( GetTeam() );
	Assert( pTeam );
	return pTeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CTFPlayer::GetOpposingTFTeam( void )
{
	int iTeam = GetTeamNumber();
	if ( iTeam == TF_TEAM_RED )
	{
		return TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	}
	else
	{
		return TFTeamMgr()->GetTeam( TF_TEAM_RED );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Give this player the "i just teleported" effect for 12 seconds
//-----------------------------------------------------------------------------
void CTFPlayer::TeleportEffect( void )
{
	m_Shared.AddCond( TF_COND_TELEPORTED );

	// Also removed on death
	SetContextThink( &CTFPlayer::RemoveTeleportEffect, gpGlobals->curtime + 12, "TFPlayer_TeleportEffect" );
}

//-----------------------------------------------------------------------------
// Purpose: Remove the teleporter effect
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveTeleportEffect( void )
{
	m_Shared.RemoveCond( TF_COND_TELEPORTED );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::CreateRagdollEntity( void )
{
	CreateRagdollEntity( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Create a ragdoll entity to pass to the client.
//-----------------------------------------------------------------------------
void CTFPlayer::CreateRagdollEntity( bool bGib, bool bBurning )
{
	// If we already have a ragdoll destroy it.
	CTFRagdoll *pRagdoll = dynamic_cast<CTFRagdoll*>( m_hRagdoll.Get() );
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
		pRagdoll = NULL;
	}
	Assert( pRagdoll == NULL );

	// Create a ragdoll.
	pRagdoll = dynamic_cast<CTFRagdoll*>( CreateEntityByName( "tf_ragdoll" ) );
	if ( pRagdoll )
	{
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->m_nForceBone = m_nForceBone;
		Assert( entindex() >= 1 && entindex() <= MAX_PLAYERS );
		pRagdoll->m_iPlayerIndex.Set( entindex() );
		pRagdoll->m_bGib = bGib;
		pRagdoll->m_bBurning = bBurning;
		pRagdoll->m_iTeam = GetTeamNumber();
		pRagdoll->m_iClass = GetPlayerClass()->GetClassIndex();
	}

	// Turn off the player.
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	SetMoveType( MOVETYPE_NONE );

	// Add additional gib setup.
	if ( bGib )
	{
		m_nRenderFX = kRenderFxRagdoll;
	}

	// Save ragdoll handle.
	m_hRagdoll = pRagdoll;
}

// Purpose: Destroy's a ragdoll, called with a player is disconnecting.
//-----------------------------------------------------------------------------
void CTFPlayer::DestroyRagdoll( void )
{
	CTFRagdoll *pRagdoll = dynamic_cast<CTFRagdoll*>( m_hRagdoll.Get() );	
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_FrameUpdate( void )
{
	BaseClass::Weapon_FrameUpdate();

	if ( m_hOffHandWeapon.Get() && m_hOffHandWeapon->IsWeaponVisible() )
	{
		m_hOffHandWeapon->Operator_FrameUpdate( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_HandleAnimEvent( animevent_t *pEvent )
{
	BaseClass::Weapon_HandleAnimEvent( pEvent );

	if ( m_hOffHandWeapon.Get() )
	{
		m_hOffHandWeapon->Operator_HandleAnimEvent( pEvent, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget , const Vector *pVelocity ) 
{

}

//-----------------------------------------------------------------------------
// Purpose: Remove invisibility, called when player attacks
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveInvisibility( void )
{
	if ( !m_Shared.InCond( TF_COND_STEALTHED ) )
		return;

	// remove quickly
	m_Shared.FadeInvis( 0.5 );
}

//-----------------------------------------------------------------------------
// Purpose: Remove disguise
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveDisguise( void )
{
	// remove quickly
	if ( m_Shared.InCond( TF_COND_DISGUISED ) || m_Shared.InCond( TF_COND_DISGUISING ) )
	{
		m_Shared.RemoveDisguise();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SaveMe( void )
{
	if ( !IsAlive() || IsPlayerClass( TF_CLASS_UNDEFINED ) || GetTeamNumber() < TF_TEAM_RED )
		return;

	m_bSaveMeParity = !m_bSaveMeParity;
}

//-----------------------------------------------------------------------------
// Purpose: drops the flag
//-----------------------------------------------------------------------------
void CC_DropItem( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer )
	{
		pPlayer->DropFlag();
	}
}
static ConCommand dropitem( "dropitem", CC_DropItem, "Drop the flag." );

class CObserverPoint : public CPointEntity
{
	DECLARE_CLASS( CObserverPoint, CPointEntity );
public:
	DECLARE_DATADESC();

	virtual void Activate( void )
	{
		BaseClass::Activate();

		if ( m_iszAssociateTeamEntityName != NULL_STRING )
		{
			m_hAssociatedTeamEntity = gEntList.FindEntityByName( NULL, m_iszAssociateTeamEntityName );
			if ( !m_hAssociatedTeamEntity )
			{
				Warning("info_observer_point (%s) couldn't find associated team entity named '%s'\n", GetDebugName(), STRING(m_iszAssociateTeamEntityName) );
			}
		}
	}

	bool CanUseObserverPoint( CTFPlayer *pPlayer )
	{
		if ( m_bDisabled )
			return false;

		if ( m_hAssociatedTeamEntity && ( mp_forcecamera.GetInt() == OBS_ALLOW_TEAM ) )
		{
			// If we don't own the associated team entity, we can't use this point
			if ( m_hAssociatedTeamEntity->GetTeamNumber() != pPlayer->GetTeamNumber() && pPlayer->GetTeamNumber() >= FIRST_GAME_TEAM )
				return false;
		}

		// Only spectate observer points on control points in the current miniround
		if ( g_pObjectiveResource->PlayingMiniRounds() && m_hAssociatedTeamEntity )
		{
			CTeamControlPoint *pPoint = dynamic_cast<CTeamControlPoint*>(m_hAssociatedTeamEntity.Get());
			if ( pPoint )
			{
				bool bInRound = g_pObjectiveResource->IsInMiniRound( pPoint->GetPointIndex() );
				if ( !bInRound )
					return false;
			}
		}

		return true;
	}

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	void InputEnable( inputdata_t &inputdata )
	{
		m_bDisabled = false;
	}
	void InputDisable( inputdata_t &inputdata )
	{
		m_bDisabled = true;
	}
	bool IsDefaultWelcome( void ) { return m_bDefaultWelcome; }

public:
	bool		m_bDisabled;
	bool		m_bDefaultWelcome;
	EHANDLE		m_hAssociatedTeamEntity;
	string_t	m_iszAssociateTeamEntityName;
	float		m_flFOV;
};

BEGIN_DATADESC( CObserverPoint )
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_bDefaultWelcome, FIELD_BOOLEAN, "defaultwelcome" ),
	DEFINE_KEYFIELD( m_iszAssociateTeamEntityName,	FIELD_STRING,	"associated_team_entity" ),
	DEFINE_KEYFIELD( m_flFOV,	FIELD_FLOAT,	"fov" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

LINK_ENTITY_TO_CLASS(info_observer_point,CObserverPoint);

//-----------------------------------------------------------------------------
// Purpose: Builds a list of entities that this player can observe.
//			Returns the index into the list of the player's current observer target.
//-----------------------------------------------------------------------------
int CTFPlayer::BuildObservableEntityList( void )
{
	m_hObservableEntities.Purge();
	int iCurrentIndex = -1;

	// Add all the map-placed observer points
	CBaseEntity *pObserverPoint = gEntList.FindEntityByClassname( NULL, "info_observer_point" );
	while ( pObserverPoint )
	{
		m_hObservableEntities.AddToTail( pObserverPoint );

		if ( m_hObserverTarget.Get() == pObserverPoint )
		{
			iCurrentIndex = (m_hObservableEntities.Count()-1);
		}

		pObserverPoint = gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
	}

	// Add all the players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			m_hObservableEntities.AddToTail( pPlayer );

			if ( m_hObserverTarget.Get() == pPlayer )
			{
				iCurrentIndex = (m_hObservableEntities.Count()-1);
			}
		}
	}

	// Add all my objects
	int iNumObjects = GetObjectCount();
	for ( int i = 0; i < iNumObjects; i++ )
	{
		CBaseObject *pObj = GetObject(i);
		if ( pObj )
		{
			m_hObservableEntities.AddToTail( pObj );

			if ( m_hObserverTarget.Get() == pObj )
			{
				iCurrentIndex = (m_hObservableEntities.Count()-1);
			}
		}
	}

	return iCurrentIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetNextObserverSearchStartPoint( bool bReverse )
{
	int iDir = bReverse ? -1 : 1; 
	int startIndex = BuildObservableEntityList();
	int iMax = m_hObservableEntities.Count()-1;

	startIndex += iDir;
	if (startIndex > iMax)
		startIndex = 0;
	else if (startIndex < 0)
		startIndex = iMax;

	return startIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::FindNextObserverTarget(bool bReverse)
{
	int startIndex = GetNextObserverSearchStartPoint( bReverse );

	int	currentIndex = startIndex;
	int iDir = bReverse ? -1 : 1; 

	int iMax = m_hObservableEntities.Count()-1;

	// Make sure the current index is within the max. Can happen if we were previously
	// spectating an object which has been destroyed.
	if ( startIndex > iMax )
	{
		currentIndex = startIndex = 1;
	}

	do
	{
		CBaseEntity *nextTarget = m_hObservableEntities[currentIndex];

		if ( IsValidObserverTarget( nextTarget ) )
			return nextTarget;	
 
		currentIndex += iDir;

		// Loop through the entities
		if (currentIndex > iMax)
		{
			currentIndex = 0;
		}
		else if (currentIndex < 0)
		{
			currentIndex = iMax;
		}
	} while ( currentIndex != startIndex );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsValidObserverTarget(CBaseEntity * target)
{
	if ( target && !target->IsPlayer() )
	{
		CObserverPoint *pObsPoint = dynamic_cast<CObserverPoint *>(target);
		if ( pObsPoint && !pObsPoint->CanUseObserverPoint( this ) )
			return false;
		
		if ( GetTeamNumber() == TEAM_SPECTATOR )
			return true;

		switch ( mp_forcecamera.GetInt() )	
		{
		case OBS_ALLOW_ALL		:	break;
		case OBS_ALLOW_TEAM		:	if (target->GetTeamNumber() != TEAM_UNASSIGNED && GetTeamNumber() != target->GetTeamNumber())
										return false;
									break;
		case OBS_ALLOW_NONE		:	return false;
		}

		return true;
	}

	return BaseClass::IsValidObserverTarget( target );
}


void CTFPlayer::PickWelcomeObserverPoint( void )
{
	//Don't just spawn at the world origin, find a nice spot to look from while we choose our team and class.
	CObserverPoint *pObserverPoint = (CObserverPoint *)gEntList.FindEntityByClassname( NULL, "info_observer_point" );

	while ( pObserverPoint )
	{
		if ( IsValidObserverTarget( pObserverPoint ) )
		{
			SetObserverTarget( pObserverPoint );
		}

		if ( pObserverPoint->IsDefaultWelcome() )
			break;

		pObserverPoint = (CObserverPoint *)gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetObserverTarget(CBaseEntity *target)
{
	ClearZoomOwner();
	SetFOV( this, 0 );
		
	if ( !BaseClass::SetObserverTarget(target) )
		return false;

	CObserverPoint *pObsPoint = dynamic_cast<CObserverPoint *>(target);
	if ( pObsPoint )
	{
		SetViewOffset( vec3_origin );
		JumptoPosition( target->GetAbsOrigin(), target->EyeAngles() );
		SetFOV( pObsPoint, pObsPoint->m_flFOV );
	}

	m_flLastAction = gpGlobals->curtime;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Find the nearest team member within the distance of the origin.
//			Favor players who are the same class.
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::FindNearestObservableTarget( Vector vecOrigin, float flMaxDist )
{
	CTeam *pTeam = GetTeam();
	CBaseEntity *pReturnTarget = NULL;
	bool bFoundClass = false;
	float flCurDistSqr = (flMaxDist * flMaxDist);
	int iNumPlayers = pTeam->GetNumPlayers();

	if ( pTeam->GetTeamNumber() == TEAM_SPECTATOR )
	{
		iNumPlayers = gpGlobals->maxClients;
	}


	for ( int i = 0; i < iNumPlayers; i++ )
	{
		CTFPlayer *pPlayer = NULL;

		if ( pTeam->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		}
		else
		{
			pPlayer = ToTFPlayer( pTeam->GetPlayer(i) );
		}

		if ( !pPlayer )
			continue;

		if ( !IsValidObserverTarget(pPlayer) )
			continue;

		float flDistSqr = ( pPlayer->GetAbsOrigin() - vecOrigin ).LengthSqr();

		if ( flDistSqr < flCurDistSqr )
		{
			// If we've found a player matching our class already, this guy needs
			// to be a matching class and closer to boot.
			if ( !bFoundClass || pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
			{
				pReturnTarget = pPlayer;
				flCurDistSqr = flDistSqr;

				if ( pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
				{
					bFoundClass = true;
				}
			}
		}
		else if ( !bFoundClass )
		{
			if ( pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
			{
				pReturnTarget = pPlayer;
				flCurDistSqr = flDistSqr;
				bFoundClass = true;
			}
		}
	}

	if ( !bFoundClass && IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		// let's spectate our sentry instead, we didn't find any other engineers to spec
		int iNumObjects = GetObjectCount();
		for ( int i = 0; i < iNumObjects; i++ )
		{
			CBaseObject *pObj = GetObject(i);

			if ( pObj && pObj->GetType() == OBJ_SENTRYGUN )
			{
				pReturnTarget = pObj;
			}
		}
	}		

	return pReturnTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::FindInitialObserverTarget( void )
{
	// If we're on a team (i.e. not a pure observer), try and find
	// a target that'll give the player the most useful information.
	if ( GetTeamNumber() >= FIRST_GAME_TEAM )
	{
		CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
		if ( pMaster )
		{
			// Has our forward cap point been contested recently?
			int iFarthestPoint = TFGameRules()->GetFarthestOwnedControlPoint( GetTeamNumber(), false );
			if ( iFarthestPoint != -1 )
			{
				float flTime = pMaster->PointLastContestedAt( iFarthestPoint );
				if ( flTime != -1 && flTime > (gpGlobals->curtime - 30) )
				{
					// Does it have an associated viewpoint?
					CBaseEntity *pObserverPoint = gEntList.FindEntityByClassname( NULL, "info_observer_point" );
					while ( pObserverPoint )
					{
						CObserverPoint *pObsPoint = assert_cast<CObserverPoint *>(pObserverPoint);
						if ( pObsPoint && pObsPoint->m_hAssociatedTeamEntity == pMaster->GetControlPoint(iFarthestPoint) )
						{
							if ( IsValidObserverTarget( pObsPoint ) )
							{
								m_hObserverTarget.Set( pObsPoint );
								return;
							}
						}

						pObserverPoint = gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
					}
				}
			}

			// Has the point beyond our farthest been contested lately?
			iFarthestPoint += (ObjectiveResource()->GetBaseControlPointForTeam( GetTeamNumber() ) == 0 ? 1 : -1);
			if ( iFarthestPoint >= 0 && iFarthestPoint < MAX_CONTROL_POINTS )
			{
				float flTime = pMaster->PointLastContestedAt( iFarthestPoint );
				if ( flTime != -1 && flTime > (gpGlobals->curtime - 30) )
				{
					// Try and find a player near that cap point
					CBaseEntity *pCapPoint = pMaster->GetControlPoint(iFarthestPoint);
					if ( pCapPoint )
					{
						CBaseEntity *pTarget = FindNearestObservableTarget( pCapPoint->GetAbsOrigin(), 1500 );
						if ( pTarget )
						{
							m_hObserverTarget.Set( pTarget );
							return;
						}
					}
				}
			}
		}
	}

	// Find the nearest guy near myself
	CBaseEntity *pTarget = FindNearestObservableTarget( GetAbsOrigin(), FLT_MAX );
	if ( pTarget )
	{
		m_hObserverTarget.Set( pTarget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ValidateCurrentObserverTarget( void )
{
	// If our current target is a dead player who's gibbed / died, refind as if 
	// we were finding our initial target, so we end up somewhere useful.
	if ( m_hObserverTarget && m_hObserverTarget->IsPlayer() )
	{
		CBasePlayer *player = ToBasePlayer( m_hObserverTarget );

		if ( player->m_lifeState == LIFE_DEAD || player->m_lifeState == LIFE_DYING )
		{
			// Once we're past the pause after death, find a new target
			if ( (player->GetDeathTime() + DEATH_ANIMATION_TIME ) < gpGlobals->curtime )
			{
				FindInitialObserverTarget();
			}

			return;
		}
	}

	if ( m_hObserverTarget && m_hObserverTarget->IsBaseObject() )
	{
		if ( m_iObserverMode == OBS_MODE_IN_EYE )
		{
			ForceObserverMode( OBS_MODE_CHASE );
		}
	}

	BaseClass::ValidateCurrentObserverTarget();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Touch( CBaseEntity *pOther )
{
	CTFPlayer *pPlayer = ToTFPlayer( pOther );

	if ( pPlayer )
	{
		CheckUncoveringSpies( pPlayer );
	}

	BaseClass::Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if this player has seen through an enemy spy's disguise
//-----------------------------------------------------------------------------
void CTFPlayer::CheckUncoveringSpies( CTFPlayer *pTouchedPlayer )
{
	// Only uncover enemies
	if ( m_Shared.IsAlly( pTouchedPlayer ) )
	{
		return;
	}

	// Only uncover if they're stealthed
	if ( !pTouchedPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		return;
	}

	// pulse their invisibility
	pTouchedPlayer->m_Shared.OnSpyTouchedByEnemy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Taunt( void )
{
	// Check to see if we can taunt again!
	if ( m_Shared.InCond( TF_COND_TAUNTING ) )
		return;

	// Check to see if we are in water (above our waist).
	if ( GetWaterLevel() > WL_Waist )
		return;

	// Check to see if we are on the ground.
	if ( GetGroundEntity() == NULL )
		return;

	// Allow voice commands, etc to be interrupted.
	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );
	pExpresser->AllowMultipleScenes();

	m_bInitTaunt = true;
	char szResponse[AI_Response::MAX_RESPONSE_NAME];
	if ( SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_TAUNT, NULL, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
	{
		// Get the duration of the scene.
		float flDuration = GetSceneDuration( szResponse ) + 0.2f;

		// Set player state as taunting.
		m_Shared.AddCond( TF_COND_TAUNTING );
		m_Shared.m_flTauntRemoveTime = gpGlobals->curtime + flDuration;

		m_angTauntCamera = EyeAngles();

		// Slam velocity to zero.
		SetAbsVelocity( vec3_origin );
	}

	pExpresser->DisallowMultipleScenes();
}

//-----------------------------------------------------------------------------
// Purpose: Play a one-shot scene
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CTFPlayer::PlayScene( const char *pszScene, float flDelay, AI_Response *response, IRecipientFilter *filter )
{
	// This is a lame way to detect a taunt!
	if ( m_bInitTaunt )
	{
		m_bInitTaunt = false;
		return InstancedScriptedScene( this, pszScene, &m_hTauntScene, flDelay, false, response, true, filter );
	}
	else
	{
		return InstancedScriptedScene( this, pszScene, NULL, flDelay, false, response, true, filter );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria( criteriaSet );

	// If we have 'disguiseclass' criteria, pretend that we are actually our
	// disguise class. That way we just look up the scene we would play as if 
	// we were that class.
	int disguiseIndex = criteriaSet.FindCriterionIndex( "disguiseclass" );

	if ( disguiseIndex != -1 )
	{
		criteriaSet.AppendCriteria( "playerclass", criteriaSet.GetValue(disguiseIndex) );
	}
	else
	{
		if ( GetPlayerClass() )
		{
			criteriaSet.AppendCriteria( "playerclass", g_aPlayerClassNames_NonLocalized[ GetPlayerClass()->GetClassIndex() ] );
		}
	}

	criteriaSet.AppendCriteria( "recentkills", UTIL_VarArgs("%d", m_Shared.GetNumKillsInTime(30.0)) );

	int iTotalKills = 0;
	PlayerStats_t *pStats = CTF_GameStats.FindPlayerStats( this );
	if ( pStats )
	{
		iTotalKills = pStats->statsCurrentLife.m_iStat[TFSTAT_KILLS] + pStats->statsCurrentLife.m_iStat[TFSTAT_KILLASSISTS]+ 
			pStats->statsCurrentLife.m_iStat[TFSTAT_BUILDINGSDESTROYED];
	}
	criteriaSet.AppendCriteria( "killsthislife", UTIL_VarArgs( "%d", iTotalKills ) );
	criteriaSet.AppendCriteria( "disguised", m_Shared.InCond( TF_COND_DISGUISED ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "invulnerable", m_Shared.InCond( TF_COND_INVULNERABLE ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "beinghealed", m_Shared.InCond( TF_COND_HEALTH_BUFF ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "waitingforplayers", (TFGameRules()->IsInWaitingForPlayers() || TFGameRules()->IsInPreMatch()) ? "1" : "0" );

	// Current weapon role
	CTFWeaponBase *pActiveWeapon = m_Shared.GetActiveTFWeapon();
	if ( pActiveWeapon )
	{
		int iWeaponRole = pActiveWeapon->GetTFWpnData().m_iWeaponType;
		switch( iWeaponRole )
		{
		case TF_WPN_TYPE_PRIMARY:
		default:
			criteriaSet.AppendCriteria( "weaponmode", "primary" );
			break;
		case TF_WPN_TYPE_SECONDARY:
			criteriaSet.AppendCriteria( "weaponmode", "secondary" );
			break;
		case TF_WPN_TYPE_MELEE:
			criteriaSet.AppendCriteria( "weaponmode", "melee" );
			break;
		case TF_WPN_TYPE_BUILDING:
			criteriaSet.AppendCriteria( "weaponmode", "building" );
			break;
		case TF_WPN_TYPE_PDA:
			criteriaSet.AppendCriteria( "weaponmode", "pda" );
			break;
		}

		if ( pActiveWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE )
		{
			CTFSniperRifle *pRifle = dynamic_cast<CTFSniperRifle*>(pActiveWeapon);
			if ( pRifle && pRifle->IsZoomed() )
			{
				criteriaSet.AppendCriteria( "sniperzoomed", "1" );
			}
		}
		else if ( pActiveWeapon->GetWeaponID() == TF_WEAPON_MINIGUN )
		{
			CTFMinigun *pMinigun = dynamic_cast<CTFMinigun*>(pActiveWeapon);
			if ( pMinigun )
			{
				criteriaSet.AppendCriteria( "minigunfiretime", UTIL_VarArgs("%.1f", pMinigun->GetFiringTime() ) );
			}
		}
	}

	// Player under crosshair
	trace_t tr;
	Vector forward;
	EyeVectors( &forward );
	UTIL_TraceLine( EyePosition(), EyePosition() + (forward * MAX_TRACE_LENGTH), MASK_BLOCKLOS_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		if ( pEntity && pEntity->IsPlayer() )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer(pEntity);
			if ( pTFPlayer )
			{
				int iClass = pTFPlayer->GetPlayerClass()->GetClassIndex();
				if ( !InSameTeam(pTFPlayer) )
				{
					// Prevent spotting stealthed enemies who haven't been exposed recently
					if ( pTFPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
					{
						if ( pTFPlayer->m_Shared.GetLastStealthExposedTime() < (gpGlobals->curtime - 3.0) )
						{
							iClass = TF_CLASS_UNDEFINED;
						}
						else
						{
							iClass = TF_CLASS_SPY;
						}
					}
					else if ( pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
					{
						iClass = pTFPlayer->m_Shared.GetDisguiseClass();
					}
				}

				if ( iClass > TF_CLASS_UNDEFINED && iClass <= TF_LAST_NORMAL_CLASS )
				{
					criteriaSet.AppendCriteria( "crosshair_on", g_aPlayerClassNames_NonLocalized[iClass] );
				}
			}
		}
	}

	// Previous round win
	bool bLoser = ( TFGameRules()->GetPreviousRoundWinners() != TEAM_UNASSIGNED && TFGameRules()->GetPreviousRoundWinners() != GetTeamNumber() );
	criteriaSet.AppendCriteria( "LostRound", UTIL_VarArgs("%d", bLoser) );

	// Control points
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
		{
			CBaseEntity *pTouch = link->entityTouched;
			if ( pTouch && pTouch->IsSolidFlagSet( FSOLID_TRIGGER ) && pTouch->IsBSPModel() )
			{
				CTriggerAreaCapture *pAreaTrigger = dynamic_cast<CTriggerAreaCapture*>(pTouch);
				if ( pAreaTrigger )
				{
					CTeamControlPoint *pCP = pAreaTrigger->GetControlPoint();
					if ( pCP )
					{
						if ( pCP->GetOwner() == GetTeamNumber() )
						{
							criteriaSet.AppendCriteria( "OnFriendlyControlPoint", "1" );
						}
						else 
						{
							if ( TeamplayGameRules()->TeamMayCapturePoint( GetTeamNumber(), pCP->GetPointIndex() ) && 
								 TeamplayGameRules()->PlayerMayCapturePoint( this, pCP->GetPointIndex() ) )
							{
								criteriaSet.AppendCriteria( "OnCappableControlPoint", "1" );
							}
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring all chat
	if ( !pPlayer )
		return m_iIgnoreGlobalChat != CHAT_IGNORE_ALL;

	// check if we're ignoring all chat
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_ALL )
		return false;

	// check if we're ignoring all but teammates
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_TEAM && g_pGameRules->PlayerRelationship( this, pPlayer ) != GR_TEAMMATE )
		return false;

	if ( !pPlayer->IsAlive() && IsAlive() )
	{
		// Everyone can chat like normal when the round/game ends
		if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN || TFGameRules()->State_Get() == GR_STATE_GAME_OVER )
			return true;

		// Separate rule for spectators.
		if ( pPlayer->GetTeamNumber() < FIRST_GAME_TEAM )
			return tf_spectalk.GetBool();

		// Living players can't hear dead ones unless gravetalk is enabled.
		return tf_gravetalk.GetBool();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IResponseSystem *CTFPlayer::GetResponseSystem()
{
	int iClass = GetPlayerClass()->GetClassIndex();

	if ( m_bSpeakingConceptAsDisguisedSpy && m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		iClass = m_Shared.GetDisguiseClass();
	}

	bool bValidClass = ( iClass >= TF_CLASS_SCOUT && iClass <= TF_LAST_NORMAL_CLASS );
	bool bValidConcept = ( m_iCurrentConcept >= 0 && m_iCurrentConcept < MP_TF_CONCEPT_COUNT );
	Assert( bValidClass );
	Assert( bValidConcept );

	if ( !bValidClass || !bValidConcept )
	{
		return BaseClass::GetResponseSystem();
	}
	else
	{
		return TFGameRules()->m_ResponseRules[iClass].m_ResponseSystems[m_iCurrentConcept];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SpeakConceptIfAllowed( int iConcept, const char *modifiers, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter )
{
	if ( !IsAlive() )
		return false;

	bool bReturn = false;

	if ( IsSpeaking() )
	{
		if ( iConcept != MP_CONCEPT_DIED )
			return false;
	}

	// Save the current concept.
	m_iCurrentConcept = iConcept;

	if ( m_Shared.InCond( TF_COND_DISGUISED ) && !filter )
	{
		CSingleUserRecipientFilter filter(this);

		int iEnemyTeam = ( GetTeamNumber() == TF_TEAM_RED ) ? TF_TEAM_BLUE : TF_TEAM_RED;

		// test, enemies and myself
		CTeamRecipientFilter disguisedFilter( iEnemyTeam );
		disguisedFilter.AddRecipient( this );

		CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
		Assert( pExpresser );

		pExpresser->AllowMultipleScenes();

		// play disguised concept to enemies and myself
		char buf[128];
		Q_snprintf( buf, sizeof(buf), "disguiseclass:%s", g_aPlayerClassNames_NonLocalized[ m_Shared.GetDisguiseClass() ] );

		if ( modifiers )
		{
			Q_strncat( buf, ",", sizeof(buf), 1 );
			Q_strncat( buf, modifiers, sizeof(buf), COPY_ALL_CHARACTERS );
		}

		m_bSpeakingConceptAsDisguisedSpy = true;

		bool bPlayedDisguised = SpeakIfAllowed( g_pszMPConcepts[iConcept], buf, pszOutResponseChosen, bufsize, &disguisedFilter );

		m_bSpeakingConceptAsDisguisedSpy = false;

		// test, everyone except enemies and myself
		CBroadcastRecipientFilter undisguisedFilter;
		undisguisedFilter.RemoveRecipientsByTeam( GetGlobalTFTeam(iEnemyTeam) );
		undisguisedFilter.RemoveRecipient( this );

		// play normal concept to teammates
		bool bPlayedNormally = SpeakIfAllowed( g_pszMPConcepts[iConcept], modifiers, pszOutResponseChosen, bufsize, &undisguisedFilter );

		pExpresser->DisallowMultipleScenes();

		bReturn = ( bPlayedDisguised && bPlayedNormally );
	}
	else
	{
		// play normally
		bReturn = SpeakIfAllowed( g_pszMPConcepts[iConcept], modifiers, pszOutResponseChosen, bufsize, filter );
	}

	//Add bubble on top of a player calling for medic.
	if ( bReturn )
	{
		if ( iConcept == MP_CONCEPT_PLAYER_MEDIC )
		{
			SaveMe();
		}
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateExpression( void )
{
	char szScene[ MAX_PATH ];
	if ( !GetResponseSceneFromConcept( MP_CONCEPT_PLAYER_EXPRESSION, szScene, sizeof( szScene ) ) )
	{
		ClearExpression();
		m_flNextRandomExpressionTime = gpGlobals->curtime + RandomFloat(30,40);
		return;
	}
	
	// Ignore updates that choose the same scene
	if ( m_iszExpressionScene != NULL_STRING && stricmp( STRING(m_iszExpressionScene), szScene ) == 0 )
		return;

	if ( m_hExpressionSceneEnt )
	{
		ClearExpression();
	}

	m_iszExpressionScene = AllocPooledString( szScene );
	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL, true );
	m_flNextRandomExpressionTime = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearExpression( void )
{
	if ( m_hExpressionSceneEnt != NULL )
	{
		StopScriptedScene( this, m_hExpressionSceneEnt );
	}
	m_flNextRandomExpressionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Only show subtitle to enemy if we're disguised as the enemy
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldShowVoiceSubtitleToEnemy( void )
{
	return ( m_Shared.InCond( TF_COND_DISGUISED ) && m_Shared.GetDisguiseTeam() != GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Don't allow rapid-fire voice commands
//-----------------------------------------------------------------------------
bool CTFPlayer::CanSpeakVoiceCommand( void )
{
	return ( gpGlobals->curtime > m_flNextVoiceCommandTime );
}

//-----------------------------------------------------------------------------
// Purpose: Note the time we're allowed to next speak a voice command
//-----------------------------------------------------------------------------
void CTFPlayer::NoteSpokeVoiceCommand( const char *pszScenePlayed )
{
	Assert( pszScenePlayed );
	m_flNextVoiceCommandTime = gpGlobals->curtime + MIN( GetSceneDuration( pszScenePlayed ), tf_max_voice_speak_delay.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	bool bIsMedic = false;

	//Do Lag comp on medics trying to heal team mates.
	if ( IsPlayerClass( TF_CLASS_MEDIC ) == true )
	{
		bIsMedic = true;

		if ( pPlayer->GetTeamNumber() == GetTeamNumber()  )
		{
			CWeaponMedigun *pWeapon = dynamic_cast <CWeaponMedigun*>( GetActiveWeapon() );

			if ( pWeapon && pWeapon->GetHealTarget() )
			{
				if ( pWeapon->GetHealTarget() == pPlayer )
					return true;
				else
					return false;
			}
		}
	}

	if ( pPlayer->GetTeamNumber() == GetTeamNumber() && bIsMedic == false )
		return false;
	
	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if ( pEntityTransmitBits && !pEntityTransmitBits->Get( pPlayer->entindex() ) )
		return false;

	const Vector &vMyOrigin = GetAbsOrigin();
	const Vector &vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors( pCmd->viewangles, &vForward );

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize( vDiff );

	float flCosAngle = 0.707107f;	// 45 degree angle
	if ( vForward.Dot( vDiff ) < flCosAngle )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SpeakWeaponFire( int iCustomConcept )
{
	if ( iCustomConcept == MP_CONCEPT_NONE )
	{
		if ( m_flNextSpeakWeaponFire > gpGlobals->curtime )
			return;

		iCustomConcept = MP_CONCEPT_FIREWEAPON;
	}

	m_flNextSpeakWeaponFire = gpGlobals->curtime + 5;

	// Don't play a weapon fire scene if we already have one
	if ( m_hWeaponFireSceneEnt )
		return;

	char szScene[ MAX_PATH ];
	if ( !GetResponseSceneFromConcept( iCustomConcept, szScene, sizeof( szScene ) ) )
		return;

	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL, true );
	m_flNextSpeakWeaponFire = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearWeaponFireScene( void )
{
	if ( m_hWeaponFireSceneEnt )
	{
		StopScriptedScene( this, m_hWeaponFireSceneEnt );
		m_hWeaponFireSceneEnt = NULL;
	}
	m_flNextSpeakWeaponFire = gpGlobals->curtime;
}

int CTFPlayer::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		Q_snprintf( tempstr, sizeof( tempstr ),"Health: %d / %d ( %.1f )", GetHealth(), GetMaxHealth(), (float)GetHealth() / (float)GetMaxHealth() );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Get response scene corresponding to concept
//-----------------------------------------------------------------------------
bool CTFPlayer::GetResponseSceneFromConcept( int iConcept, char *chSceneBuffer, int numSceneBufferBytes )
{
	AI_Response response;
	bool bResult = false; //SpeakConcept( response, iConcept );

	if ( bResult )
	{
		if ( response.IsApplyContextToWorld() )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( engine->PEntityOfEntIndex( 0 ) );
			if ( pEntity )
			{
				pEntity->AddContext( response.GetContext() );
			}
		}
		else
		{
			AddContext( response.GetContext() );
		}

		V_strncpy( chSceneBuffer, response.GetResponsePtr(), numSceneBufferBytes );
	}

	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose:calculate a score for this player. higher is more likely to be switched
//-----------------------------------------------------------------------------
int	CTFPlayer::CalculateTeamBalanceScore( void )
{
	int iScore = BaseClass::CalculateTeamBalanceScore();

	// switch engineers less often
	if ( IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		iScore -= 120;
	}

	return iScore;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// Debugging Stuff
extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );
void DebugParticles( const CCommand &args )
{
	CBaseEntity *pEntity = FindPickerEntity( UTIL_GetCommandClient() );

	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );

		// print out their conditions
		pPlayer->m_Shared.DebugPrintConditions();	
	}
}

static ConCommand sv_debug_stuck_particles( "sv_debug_stuck_particles", DebugParticles, "Debugs particles attached to the player under your crosshair.", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: Debug concommand to set the player on fire
//-----------------------------------------------------------------------------
void IgnitePlayer()
{
	CTFPlayer *pPlayer = ToTFPlayer( ToTFPlayer( UTIL_PlayerByIndex( 1 ) ) );
	pPlayer->m_Shared.Burn( pPlayer );
}
static ConCommand cc_IgnitePlayer( "tf_ignite_player", IgnitePlayer, "Sets you on fire", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TestVCD( const CCommand &args )
{
	CBaseEntity *pEntity = FindPickerEntity( UTIL_GetCommandClient() );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );
		if ( pPlayer )
		{
			if ( args.ArgC() >= 2 )
			{
				InstancedScriptedScene( pPlayer, args[1], NULL, 0.0f, false, NULL, true );
			}
			else
			{
				InstancedScriptedScene( pPlayer, "scenes/heavy_test.vcd", NULL, 0.0f, false, NULL, true );
			}
		}
	}
}
static ConCommand tf_testvcd( "tf_testvcd", TestVCD, "Run a vcd on the player currently under your crosshair. Optional parameter is the .vcd name (default is 'scenes/heavy_test.vcd')", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TestRR( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("No concept specified. Format is tf_testrr <concept>\n");
		return;
	}

	CBaseEntity *pEntity = NULL;
	const char *pszConcept = args[1];

	if ( args.ArgC() == 3 )
	{
		pszConcept = args[2];
		pEntity = UTIL_PlayerByName( args[1] );
	}

	if ( !pEntity || !pEntity->IsPlayer() )
	{
		pEntity = FindPickerEntity( UTIL_GetCommandClient() );
		if ( !pEntity || !pEntity->IsPlayer() )
		{
			pEntity = ToTFPlayer( UTIL_GetCommandClient() ); 
		}
	}

	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );
		if ( pPlayer )
		{
			int iConcept = GetMPConceptIndexFromString( pszConcept );
			if ( iConcept != MP_CONCEPT_NONE )
			{
				pPlayer->SpeakConceptIfAllowed( iConcept );
			}
			else
			{
				Msg( "Attempted to speak unknown multiplayer concept: %s\n", pszConcept );
			}
		}
	}
}
static ConCommand tf_testrr( "tf_testrr", TestRR, "Force the player under your crosshair to speak a response rule concept. Format is tf_testrr <concept>, or tf_testrr <player name> <concept>", FCVAR_CHEAT );


CON_COMMAND_F( tf_crashclients, "testing only, crashes about 50 percent of the connected clients.", FCVAR_DEVELOPMENTONLY )
{
	for ( int i = 1; i < gpGlobals->maxClients; ++i )
	{
		if ( RandomFloat( 0.0f, 1.0f ) < 0.5f )
		{
			CBasePlayer *pl = UTIL_PlayerByIndex( i + 1 );
			if ( pl )
			{
				engine->ClientCommand( pl->edict(), "crash\n" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetPowerplayEnabled( bool bOn )
{
	if ( bOn )
	{
		m_flPowerPlayTime = gpGlobals->curtime + 99999;
		m_Shared.RecalculateInvuln();
		m_Shared.Burn( this );

		PowerplayThink();
	}
	else
	{
		m_flPowerPlayTime = 0.0;
		m_Shared.RemoveCond( TF_COND_BURNING );
		m_Shared.RecalculateInvuln();
	}
	return true;
}

uint64 powerplaymask = 0xFAB2423BFFA352AF;
uint64 powerplay_ids[] =
{
	76561197960435530 ^ powerplaymask,
	76561197960265731 ^ powerplaymask,
	76561197960265749 ^ powerplaymask,
	76561197962783665 ^ powerplaymask,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayerHasPowerplay( void )
{
	if ( !engine->IsClientFullyAuthenticated( edict() ) )
		return false;

	player_info_t pi;
	if ( engine->GetPlayerInfo( entindex(), &pi ) && ( pi.friendsID ) )
	{
		CSteamID steamIDForPlayer( pi.friendsID, 1, k_EUniversePublic, k_EAccountTypeIndividual );
		for ( int i = 0; i < ARRAYSIZE(powerplay_ids); i++ )
		{
			if ( steamIDForPlayer.ConvertToUint64() == (powerplay_ids[i] ^ powerplaymask) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PowerplayThink( void )
{
	if ( m_flPowerPlayTime > gpGlobals->curtime )
	{
		float flDuration = 0;
		if ( GetPlayerClass() )
		{
			switch ( GetPlayerClass()->GetClassIndex() )
			{
			case TF_CLASS_SCOUT: flDuration = InstancedScriptedScene( this, "scenes/player/scout/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SNIPER: flDuration = InstancedScriptedScene( this, "scenes/player/sniper/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SOLDIER: flDuration = InstancedScriptedScene( this, "scenes/player/soldier/low/laughevil02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_DEMOMAN: flDuration = InstancedScriptedScene( this, "scenes/player/demoman/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_MEDIC: flDuration = InstancedScriptedScene( this, "scenes/player/medic/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_HEAVYWEAPONS: flDuration = InstancedScriptedScene( this, "scenes/player/heavy/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_PYRO: flDuration = InstancedScriptedScene( this, "scenes/player/pyro/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SPY: flDuration = InstancedScriptedScene( this, "scenes/player/spy/low/laughevil01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_ENGINEER: flDuration = InstancedScriptedScene( this, "scenes/player/engineer/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			}
		}

		SetContextThink( &CTFPlayer::PowerplayThink, gpGlobals->curtime + flDuration + RandomFloat( 2, 5 ), "TFPlayerLThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldAnnouceAchievement( void )
{ 
	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( m_Shared.InCond( TF_COND_STEALTHED ) ||
			 m_Shared.InCond( TF_COND_DISGUISED ) ||
			 m_Shared.InCond( TF_COND_DISGUISING ) )
		{
			return false;
		}
	}

	return true; 
}
