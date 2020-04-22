//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for Portal.
//
//=============================================================================//

#include "cbase.h"
#include "portal_player.h"
#include "globalstate.h"
#include "trains.h"
#include "game.h"
#include "portal_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "portal_gamerules.h"
#include "weapon_portalgun.h"
#include "portal/weapon_physcannon.h"
#include "KeyValues.h"
#include "team.h"
#include "eventqueue.h"
#include "weapon_portalbase.h"
#include "engine/IEngineSound.h"
#include "ai_basenpc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "prop_portal_shared.h"
#include "player_pickup.h"	// for player pickup code
#include "vphysics/player_controller.h"
#include "datacache/imdlcache.h"
#include "bone_setup.h"
#include "portal_gamestats.h"
#include "physicsshadowclone.h"
#include "physics_prop_ragdoll.h"
#include "soundenvelope.h"
#include "ai_speech.h"		// For expressors, vcd playing
#include "sceneentity.h"	// has the VCD precache function

// Max mass the player can lift with +use
#define PORTAL_PLAYER_MAX_LIFT_MASS 85
#define PORTAL_PLAYER_MAX_LIFT_SIZE 128

extern CBaseEntity	*g_pLastSpawn;

extern void respawn(CBaseEntity *pEdict, bool fCopyCorpse);


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
	}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
SendPropEHandle( SENDINFO( m_hPlayer ) ),
SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_nData ), 32 ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
	CPVSFilter filter( (const Vector&)pPlayer->EyePosition() );

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}



//=================================================================================
//
// Ragdoll Entity
//
class CPortalRagdoll : public CBaseAnimatingOverlay, public CDefaultPlayerPickupVPhysics
{
public:

	DECLARE_CLASS( CPortalRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPortalRagdoll()
	{
		m_hPlayer.Set( NULL );
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle 
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( portal_ragdoll, CPortalRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CPortalRagdoll, DT_PortalRagdoll )
SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
SendPropEHandle( SENDINFO( m_hPlayer ) ),
SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
SendPropVector( SENDINFO( m_vecRagdollVelocity ) ),
END_SEND_TABLE()


BEGIN_DATADESC( CPortalRagdoll )

	DEFINE_FIELD( m_vecRagdollOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecRagdollVelocity, FIELD_VECTOR ),

END_DATADESC()





LINK_ENTITY_TO_CLASS( player, CPortal_Player );

IMPLEMENT_SERVERCLASS_ST(CPortal_Player, DT_Portal_Player)
SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),
SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),
SendPropExclude( "DT_BaseFlex", "m_blinktoggle" ),

// portal_playeranimstate and clientside animation takes care of these on the client
SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),


SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN ),
SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN ),
SendPropEHandle( SENDINFO( m_hRagdoll ) ),
SendPropInt( SENDINFO( m_iSpawnInterpCounter), 4 ),
SendPropInt( SENDINFO( m_iPlayerSoundType), 3 ),
SendPropBool( SENDINFO( m_bHeldObjectOnOppositeSideOfPortal) ),
SendPropEHandle( SENDINFO( m_pHeldObjectPortal ) ),
SendPropBool( SENDINFO( m_bPitchReorientation ) ),
SendPropEHandle( SENDINFO( m_hPortalEnvironment ) ),
SendPropEHandle( SENDINFO( m_hSurroundingLiquidPortal ) ),
SendPropBool( SENDINFO( m_bSuppressingCrosshair ) ),
SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),

END_SEND_TABLE()

BEGIN_DATADESC( CPortal_Player )

	DEFINE_SOUNDPATCH( m_pWooshSound ),

	DEFINE_FIELD( m_bHeldObjectOnOppositeSideOfPortal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_pHeldObjectPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bIntersectingPortalPlane, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bStuckOnPortalCollisionObject, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fTimeLastHurt, FIELD_TIME ),
	DEFINE_FIELD( m_StatsThisLevel.iNumPortalsPlaced, FIELD_INTEGER ),
	DEFINE_FIELD( m_StatsThisLevel.iNumStepsTaken, FIELD_INTEGER ),
	DEFINE_FIELD( m_StatsThisLevel.fNumSecondsTaken, FIELD_FLOAT ),
	DEFINE_FIELD( m_fTimeLastNumSecondsUpdate, FIELD_TIME ),
	DEFINE_FIELD( m_iNumCamerasDetatched, FIELD_INTEGER ),
	DEFINE_FIELD( m_bPitchReorientation, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsRegenerating, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fNeuroToxinDamageTime, FIELD_TIME ),
	DEFINE_FIELD( m_hPortalEnvironment, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flExpressionLoopTime, FIELD_TIME ),
	DEFINE_FIELD( m_iszExpressionScene, FIELD_STRING ),
	DEFINE_FIELD( m_hExpressionSceneEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecTotalBulletForce, FIELD_VECTOR ),
	DEFINE_FIELD( m_bSilentDropAndPickup, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hRagdoll, FIELD_EHANDLE ),
	DEFINE_FIELD( m_angEyeAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_iPlayerSoundType, FIELD_INTEGER ),
	DEFINE_FIELD( m_qPrePortalledViewAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_bFixEyeAnglesFromPortalling, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_matLastPortalled, FIELD_VMATRIX_WORLDSPACE ),
	DEFINE_FIELD( m_vWorldSpaceCenterHolder, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hSurroundingLiquidPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bSuppressingCrosshair, FIELD_BOOLEAN ),
	//DEFINE_FIELD ( m_PlayerAnimState, CPortalPlayerAnimState ),
	//DEFINE_FIELD ( m_StatsThisLevel, PortalPlayerStatistics_t ),

	DEFINE_EMBEDDEDBYREF( m_pExpresser ),

END_DATADESC()

ConVar sv_regeneration_wait_time ("sv_regeneration_wait_time", "1.0", FCVAR_REPLICATED );

const char *g_pszChellModel = "models/player/chell.mdl";
const char *g_pszPlayerModel = g_pszChellModel;


#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define PORTALPLAYER_PHYSDAMAGE_SCALE 4.0f

extern ConVar sv_turbophysics;

//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
#define VPHYS_MAX_DISTANCE		2.0
#define VPHYS_MAX_VEL			10
#define VPHYS_MAX_DISTSQR		(VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE)
#define VPHYS_MAX_VELSQR		(VPHYS_MAX_VEL*VPHYS_MAX_VEL)


extern float IntervalDistance( float x, float x0, float x1 );

//disable 'this' : used in base member initializer list
#pragma warning( disable : 4355 )

CPortal_Player::CPortal_Player()
{

	m_PlayerAnimState = CreatePortalPlayerAnimState( this );
	CreateExpresser();

	UseClientSideAnimation();

	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_iSpawnInterpCounter = 0;

	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_pHeldObjectPortal = 0;

	m_bIntersectingPortalPlane = false;

	m_bPitchReorientation = false;

	m_bSilentDropAndPickup = false;

	m_iszExpressionScene = NULL_STRING;
	m_hExpressionSceneEnt = NULL;
	m_flExpressionLoopTime = 0.0f;
	m_bSuppressingCrosshair = false;
}

CPortal_Player::~CPortal_Player( void )
{
	ClearSceneEvents( NULL, true );

	if ( m_PlayerAnimState )
		m_PlayerAnimState->Release();

	CPortalRagdoll *pRagdoll = dynamic_cast<CPortalRagdoll*>( m_hRagdoll.Get() );	
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
	}
}

void CPortal_Player::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}

void CPortal_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "PortalPlayer.EnterPortal" );
	PrecacheScriptSound( "PortalPlayer.ExitPortal" );

	PrecacheScriptSound( "PortalPlayer.Woosh" );
	PrecacheScriptSound( "PortalPlayer.FallRecover" );

	PrecacheModel ( "sprites/glow01.vmt" );

	//Precache Citizen models
	PrecacheModel( g_pszPlayerModel );
	PrecacheModel( g_pszChellModel );

	PrecacheScriptSound( "NPC_Citizen.die" );
}

void CPortal_Player::CreateSounds()
{
	if ( !m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pWooshSound = controller.SoundCreate( filter, entindex(), "PortalPlayer.Woosh" );
		controller.Play( m_pWooshSound, 0, 100 );
	}
}

void CPortal_Player::StopLoopingSounds()
{
	if ( m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy( m_pWooshSound );
		m_pWooshSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CPortal_Player::GiveAllItems( void )
{
	EquipSuit();

	CBasePlayer::GiveAmmo( 255,	"Pistol");
	CBasePlayer::GiveAmmo( 32,	"357" );

	CBasePlayer::GiveAmmo( 255,	"AR2" );
	CBasePlayer::GiveAmmo( 3,	"AR2AltFire" );
	CBasePlayer::GiveAmmo( 255,	"SMG1");
	CBasePlayer::GiveAmmo( 3,	"smg1_grenade");

	CBasePlayer::GiveAmmo( 255,	"Buckshot");
	CBasePlayer::GiveAmmo( 16,	"XBowBolt" );

	CBasePlayer::GiveAmmo( 3,	"rpg_round");
	CBasePlayer::GiveAmmo( 6,	"grenade" );

	GiveNamedItem( "weapon_crowbar" );
	GiveNamedItem( "weapon_physcannon" );

	GiveNamedItem( "weapon_pistol" );
	GiveNamedItem( "weapon_357" );

	GiveNamedItem( "weapon_smg1" );
	GiveNamedItem( "weapon_ar2" );

	GiveNamedItem( "weapon_shotgun" );
	GiveNamedItem( "weapon_crossbow" );

	GiveNamedItem( "weapon_rpg" );
	GiveNamedItem( "weapon_frag" );

	GiveNamedItem( "weapon_bugbait" );

	//GiveNamedItem( "weapon_physcannon" );
	CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun*>( GiveNamedItem( "weapon_portalgun" ) );

	if ( !pPortalGun )
	{
		pPortalGun = static_cast<CWeaponPortalgun*>( Weapon_OwnsThisType( "weapon_portalgun" ) );
	}

	if ( pPortalGun )
	{
		pPortalGun->SetCanFirePortal1();
		pPortalGun->SetCanFirePortal2();
	}
}

void CPortal_Player::GiveDefaultItems( void )
{
	castable_string_t st( "suit_no_sprint" );
	GlobalEntity_SetState( st, GLOBAL_OFF );
	inputdata_t in;
	InputDisableFlashlight( in );
}


//-----------------------------------------------------------------------------
// Purpose: Sets  specific defaults.
//-----------------------------------------------------------------------------
void CPortal_Player::Spawn(void)
{
	SetPlayerModel();

	BaseClass::Spawn();

	CreateSounds();

	pl.deadflag = false;
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	RemoveEffects( EF_NODRAW );
	StopObserverMode();

	GiveDefaultItems();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = PORTALPLAYER_PHYSDAMAGE_SCALE;

	RemoveFlag( FL_FROZEN );

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

#ifdef PORTAL_MP
	PickTeam();
#endif
}

void CPortal_Player::Activate( void )
{
	BaseClass::Activate();
	m_fTimeLastNumSecondsUpdate = gpGlobals->curtime;
}

void CPortal_Player::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	// On teleport, we send event for tracking fling achievements
	if ( eventType == NOTIFY_EVENT_TELEPORT )
	{
		CProp_Portal *pEnteredPortal = dynamic_cast<CProp_Portal*>( pNotify );
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_portaled" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "portal2", pEnteredPortal->m_bIsPortal2 );
			gameeventmanager->FireEvent( event );
		}
	}

	BaseClass::NotifySystemEvent( pNotify, eventType, params );
}

void CPortal_Player::OnRestore( void )
{
	BaseClass::OnRestore();
	if ( m_pExpresser )
	{
		m_pExpresser->SetOuter ( this );
	}
}

//bool CPortal_Player::StartObserverMode( int mode )
//{
//	//Do nothing.
//
//	return false;
//}

bool CPortal_Player::ValidatePlayerModel( const char *pModel )
{
	if ( !Q_stricmp( g_pszPlayerModel, pModel ) )
	{
		return true;
	}

	if ( !Q_stricmp( g_pszChellModel, pModel ) )
	{
		return true;
	}

	return false;
}

void CPortal_Player::SetPlayerModel( void )
{
	const char *szModelName = NULL;
	const char *pszCurrentModelName = modelinfo->GetModelName( GetModel());

	szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( edict() ), "cl_playermodel" );

	if ( ValidatePlayerModel( szModelName ) == false )
	{
		char szReturnString[512];

		if ( ValidatePlayerModel( pszCurrentModelName ) == false )
		{
			pszCurrentModelName = g_pszPlayerModel;
		}

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", pszCurrentModelName );
		engine->ClientCommand ( edict(), szReturnString );

		szModelName = pszCurrentModelName;
	}

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 )
	{
		szModelName = g_pszPlayerModel;

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	SetModel( szModelName );
	m_iPlayerSoundType = (int)PLAYER_SOUNDS_CITIZEN;
}


bool CPortal_Player::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	bool bRet = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortal_Player::UpdateExpression( void )
{
	if ( !m_pExpresser )
		return;

	int iConcept = CONCEPT_CHELL_IDLE;
	if ( GetHealth() <= 0 )
	{
		iConcept = CONCEPT_CHELL_DEAD;
	}

	GetExpresser()->SetOuter( this );

	ClearExpression();
	AI_Response response;
	bool result = SpeakFindResponse( response, g_pszChellConcepts[iConcept] );
	if ( !result )
	{
		m_flExpressionLoopTime = gpGlobals->curtime + RandomFloat(30,40);
		return;
	}

	char const *szScene = response.GetResponsePtr();

	// Ignore updates that choose the same scene
	if ( m_iszExpressionScene != NULL_STRING && stricmp( STRING(m_iszExpressionScene), szScene ) == 0 )
		return;

	if ( m_hExpressionSceneEnt )
	{
		ClearExpression();
	}

	m_iszExpressionScene = AllocPooledString( szScene );
	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL );
	m_flExpressionLoopTime = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortal_Player::ClearExpression( void )
{
	if ( m_hExpressionSceneEnt != NULL )
	{ 
		StopScriptedScene( this, m_hExpressionSceneEnt );
	}
	m_flExpressionLoopTime = gpGlobals->curtime;
}


void CPortal_Player::PreThink( void )
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if ( vTempAngles[PITCH] > 180.0f )
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();

	if( (m_afButtonPressed & IN_JUMP) )
	{
		Jump();	
	}

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;

	SetLocalAngles( vOldAngles );
}

void CPortal_Player::PostThink( void )
{
	BaseClass::PostThink();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );

	// Regenerate heath after 3 seconds
	if ( IsAlive() && GetHealth() < GetMaxHealth() )
	{
		// Color to overlay on the screen while the player is taking damage
		color32 hurtScreenOverlay = {64,0,0,64};

		if ( gpGlobals->curtime > m_fTimeLastHurt + sv_regeneration_wait_time.GetFloat() )
		{
			TakeHealth( 1, DMG_GENERIC );
			m_bIsRegenerating = true;

			if ( GetHealth() >= GetMaxHealth() )
			{
				m_bIsRegenerating = false;
			}
		}
		else
		{
			m_bIsRegenerating = false;
			UTIL_ScreenFade( this, hurtScreenOverlay, 1.0f, 0.1f, FFADE_IN|FFADE_PURGE );
		}
	}

	UpdatePortalPlaneSounds();
	UpdateWooshSounds();

	m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	if ( IsAlive() && m_flExpressionLoopTime >= 0 && gpGlobals->curtime > m_flExpressionLoopTime )
	{
		// Random expressions need to be cleared, because they don't loop. So if we
		// pick the same one again, we want to restart it.
		ClearExpression();
		m_iszExpressionScene = NULL_STRING;
		UpdateExpression();
	}

	UpdateSecondsTaken();

	// Try to fix the player if they're stuck
	if ( m_bStuckOnPortalCollisionObject )
	{
		Vector vForward = ((CProp_Portal*)m_hPortalEnvironment.Get())->m_vPrevForward;
		Vector vNewPos = GetAbsOrigin() + vForward * gpGlobals->frametime * -1000.0f;
		Teleport( &vNewPos, NULL, &vForward );
		m_bStuckOnPortalCollisionObject = false;
	}
}

void CPortal_Player::PlayerDeathThink(void)
{
	float flForward;

	SetNextThink( gpGlobals->curtime + 0.1f );

	if (GetFlags() & FL_ONGROUND)
	{
		flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vecNewVelocity = GetAbsVelocity();
			VectorNormalize( vecNewVelocity );
			vecNewVelocity *= flForward;
			SetAbsVelocity( vecNewVelocity );
		}
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	if (GetModelIndex() && (!IsSequenceFinished()) && (m_lifeState == LIFE_DYING))
	{
		StudioFrameAdvance( );

		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 60 )  // animations should be no longer than this
			return;
	}

	if (m_lifeState == LIFE_DYING)
		m_lifeState = LIFE_DEAD;

	StopAnimation();

	IncrementInterpolationFrame();
	m_flPlaybackRate = 0.0;

	int fAnyButtonDown = (m_nButtons & ~IN_SCORE);

	// Strip out the duck key from this check if it's toggled
	if ( (fAnyButtonDown & IN_DUCK) && GetToggledDuckState())
	{
		fAnyButtonDown &= ~IN_DUCK;
	}

	// wait for all buttons released
	if ( m_lifeState == LIFE_DEAD )
	{
		if ( fAnyButtonDown || gpGlobals->curtime < m_flDeathTime + DEATH_ANIMATION_TIME )
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_lifeState = LIFE_RESPAWNABLE;
		}

		return;
	}

	// if the player has been dead for one second longer than allowed by forcerespawn, 
	// forcerespawn isn't on. Send the player off to an intermission camera until they 
	// choose to respawn.
	if ( g_pGameRules->IsMultiplayer() && ( gpGlobals->curtime > (m_flDeathTime + DEATH_ANIMATION_TIME) ) && !IsObserver() )
	{
		// go to dead camera. 
		StartObserverMode( m_iObserverLastMode );
	}

	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown 
		&& !( g_pGameRules->IsMultiplayer() && forcerespawn.GetInt() > 0 && (gpGlobals->curtime > (m_flDeathTime + 5))) )
		return;

	m_nButtons = 0;
	m_iRespawnFrames = 0;

	//Msg( "Respawn\n");

	respawn( this, !IsObserver() );// don't copy a corpse if we're in deathcam.
	SetNextThink( TICK_NEVER_THINK );
}

void CPortal_Player::UpdatePortalPlaneSounds( void )
{
	CProp_Portal *pPortal = m_hPortalEnvironment;
	if ( pPortal && pPortal->m_bActivated )
	{
		Vector vVelocity;
		GetVelocity( &vVelocity, NULL );

		if ( !vVelocity.IsZero() )
		{
			Vector vMin, vMax;
			CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

			Vector vEarCenter = ( vMax + vMin ) / 2.0f;
			Vector vDiagonal = vMax - vMin;

			if ( !m_bIntersectingPortalPlane )
			{
				vDiagonal *= 0.25f;

				if ( UTIL_IsBoxIntersectingPortal( vEarCenter, vDiagonal, pPortal ) )
				{
					m_bIntersectingPortalPlane = true;

					CPASAttenuationFilter filter( this );
					CSoundParameters params;
					if ( GetParametersForSound( "PortalPlayer.EnterPortal", params, NULL ) )
					{
						EmitSound_t ep( params );
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

						EmitSound( filter, entindex(), ep );
					}
				}
			}
			else
			{
				vDiagonal *= 0.30f;

				if ( !UTIL_IsBoxIntersectingPortal( vEarCenter, vDiagonal, pPortal ) )
				{
					m_bIntersectingPortalPlane = false;

					CPASAttenuationFilter filter( this );
					CSoundParameters params;
					if ( GetParametersForSound( "PortalPlayer.ExitPortal", params, NULL ) )
					{
						EmitSound_t ep( params );
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

						EmitSound( filter, entindex(), ep );
					}
				}
			}
		}
	}
	else if ( m_bIntersectingPortalPlane )
	{
		m_bIntersectingPortalPlane = false;

		CPASAttenuationFilter filter( this );
		CSoundParameters params;
		if ( GetParametersForSound( "PortalPlayer.ExitPortal", params, NULL ) )
		{
			EmitSound_t ep( params );
			Vector vVelocity;
			GetVelocity( &vVelocity, NULL );
			ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
			ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

			EmitSound( filter, entindex(), ep );
		}
	}
}

void CPortal_Player::UpdateWooshSounds( void )
{
	if ( m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		float fWooshVolume = GetAbsVelocity().Length() - MIN_FLING_SPEED;

		if ( fWooshVolume < 0.0f )
		{
			controller.SoundChangeVolume( m_pWooshSound, 0.0f, 0.1f );
			return;
		}

		fWooshVolume /= 2000.0f;
		if ( fWooshVolume > 1.0f )
			fWooshVolume = 1.0f;

		controller.SoundChangeVolume( m_pWooshSound, fWooshVolume, 0.1f );
		//		controller.SoundChangePitch( m_pWooshSound, fWooshVolume + 0.5f, 0.1f );
	}
}

void CPortal_Player::FireBullets ( const FireBulletsInfo_t &info )
{
	NoteWeaponFired();

	BaseClass::FireBullets( info );
}

void CPortal_Player::NoteWeaponFired( void )
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CPortal_Player::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if ( !( pCmd->buttons & IN_ATTACK ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5) )
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


void CPortal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: Override setup bones so that is uses the render angles from
//			the Portal animation state to setup the hitboxes.
//-----------------------------------------------------------------------------
void CPortal_Player::SetupBones( matrix3x4_t *pBoneToWorld, int boneMask )
{
	VPROF_BUDGET( "CBaseAnimating::SetupBones", VPROF_BUDGETGROUP_SERVER_ANIM );

	// Set the mdl cache semaphore.
	MDLCACHE_CRITICAL_SECTION();

	// Get the studio header.
	Assert( GetModelPtr() );
	CStudioHdr *pStudioHdr = GetModelPtr( );

	Vector pos[MAXSTUDIOBONES];
	Quaternion q[MAXSTUDIOBONES];

	// Adjust hit boxes based on IK driven offset.
	Vector adjOrigin = GetAbsOrigin() + Vector( 0, 0, m_flEstIkOffset );

	// FIXME: pass this into Studio_BuildMatrices to skip transforms
	CBoneBitList boneComputed;
	if ( m_pIk )
	{
		m_iIKCounter++;
		m_pIk->Init( pStudioHdr, GetAbsAngles(), adjOrigin, gpGlobals->curtime, m_iIKCounter, boneMask );
		GetSkeleton( pStudioHdr, pos, q, boneMask );

		m_pIk->UpdateTargets( pos, q, pBoneToWorld, boneComputed );
		CalculateIKLocks( gpGlobals->curtime );
		m_pIk->SolveDependencies( pos, q, pBoneToWorld, boneComputed );
	}
	else
	{
		GetSkeleton( pStudioHdr, pos, q, boneMask );
	}

	CBaseAnimating *pParent = dynamic_cast< CBaseAnimating* >( GetMoveParent() );
	if ( pParent )
	{
		// We're doing bone merging, so do special stuff here.
		CBoneCache *pParentCache = pParent->GetBoneCache();
		if ( pParentCache )
		{
			BuildMatricesWithBoneMerge( 
				pStudioHdr, 
				m_PlayerAnimState->GetRenderAngles(),
				adjOrigin, 
				pos, 
				q, 
				pBoneToWorld, 
				pParent, 
				pParentCache );

			return;
		}
	}

	Studio_BuildMatrices( 
		pStudioHdr, 
		m_PlayerAnimState->GetRenderAngles(),
		adjOrigin, 
		pos, 
		q, 
		-1,
		GetModelScale(), // Scaling
		pBoneToWorld,
		boneMask );
}


// Set the activity based on an event or current state
void CPortal_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

CAI_Expresser *CPortal_Player::CreateExpresser()
{
	Assert( !m_pExpresser );

	if ( m_pExpresser )
	{
		delete m_pExpresser;
	}

	m_pExpresser = new CAI_Expresser(this);
	if ( !m_pExpresser)
	{
		return NULL;
	}
	m_pExpresser->Connect(this);

	return m_pExpresser;
}

//-----------------------------------------------------------------------------

CAI_Expresser *CPortal_Player::GetExpresser() 
{ 
	if ( m_pExpresser )
	{
		m_pExpresser->Connect(this);
	}
	return m_pExpresser; 
}


extern int	gEvilImpulse101;
//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CPortal_Player::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( !IsAllowedToPickupWeapons() )
		return false;

	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if( !pWeapon->FVisible( this, MASK_SOLID ) && !(GetFlags() & FL_NOTARGET) )
	{
		return false;
	}

	CWeaponPortalgun *pPickupPortalgun = dynamic_cast<CWeaponPortalgun*>( pWeapon );

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType());

	if ( bOwnsWeaponAlready == true ) 
	{
		// If we picked up a second portal gun set the bool to alow secondary fire
		if ( pPickupPortalgun )
		{
			CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun*>( Weapon_OwnsThisType( pWeapon->GetClassname() ) );

			if ( pPickupPortalgun->CanFirePortal1() )
				pPortalGun->SetCanFirePortal1();

			if ( pPickupPortalgun->CanFirePortal2() )
				pPortalGun->SetCanFirePortal2();

			UTIL_Remove( pWeapon );
			return true;
		}

		//If we have room for the ammo, then "take" the weapon too.
		if ( Weapon_EquipAmmoOnly( pWeapon ) )
		{
			pWeapon->CheckRespawn();

			UTIL_Remove( pWeapon );
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();
	Weapon_Equip( pWeapon );

	// If we're holding and object before picking up portalgun, drop it
	if ( pPickupPortalgun )
	{
		ForceDropOfCarriedPhysObjects( GetPlayerHeldEntity( this ) );
	}

	return true;
}

void CPortal_Player::ShutdownUseEntity( void )
{
	ShutdownPickupController( m_hUseEntity );
}

const Vector& CPortal_Player::WorldSpaceCenter( ) const
{
	m_vWorldSpaceCenterHolder = GetAbsOrigin();
	m_vWorldSpaceCenterHolder.z += ( (IsDucked()) ? (VEC_DUCK_HULL_MAX_SCALED( this ).z) : (VEC_HULL_MAX_SCALED( this ).z) ) * 0.5f;
	return m_vWorldSpaceCenterHolder;
}

void CPortal_Player::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity )
{
	Vector oldOrigin = GetLocalOrigin();
	QAngle oldAngles = GetLocalAngles();
	BaseClass::Teleport( newPosition, newAngles, newVelocity );
	m_angEyeAngles = pl.v_angle;

	m_PlayerAnimState->Teleport( newPosition, newAngles, this );
}

void CPortal_Player::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
	if( m_hPortalEnvironment.Get() == NULL )
		return BaseClass::VPhysicsShadowUpdate( pPhysics );


	//below is mostly a cut/paste of existing CBasePlayer::VPhysicsShadowUpdate code with some minor tweaks to avoid getting stuck in stuff when in a portal environment
	if ( sv_turbophysics.GetBool() )
		return;

	Vector newPosition;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition( &newPosition, NULL ) > 0 ? true : false;

	// UNDONE: If the player is penetrating, but the player's game collisions are not stuck, teleport the physics shadow to the game position
	if ( pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING )
	{
		CUtlVector<CBaseEntity *> list;
		PhysGetListOfPenetratingEntities( this, list );
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			// filter out anything that isn't simulated by vphysics
			// UNDONE: Filter out motion disabled objects?
			if ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				// I'm currently stuck inside a moving object, so allow vphysics to 
				// apply velocity to the player in order to separate these objects
				m_touchedPhysObject = true;
			}
		}
	}

	if ( m_pPhysicsController->IsInContact() || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) )
	{
		m_touchedPhysObject = true;
	}

	if ( IsFollowingPhysics() )
	{
		m_touchedPhysObject = true;
	}

	if ( GetMoveType() == MOVETYPE_NOCLIP )
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if ( phys_timescale.GetFloat() == 0.0f )
	{
		physicsUpdated = false;
	}

	if ( !physicsUpdated )
		return;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition( &newPosition, 0 );
	m_pPhysicsController->GetShadowVelocity( &newVelocity );



	Vector tmp = GetAbsOrigin() - newPosition;
	if ( !m_touchedPhysObject && !(GetFlags() & FL_ONGROUND) )
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if ( IsRideablePhysics(pPhysGround) )
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	if ( dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_touchedPhysObject) )
	{
		if ( m_touchedPhysObject || pPhysGround )
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if ( deltaV >= maxVelErrorSqr )
			{
				Vector dir = GetAbsVelocity();
				float len = VectorNormalize(dir);
				float dot = DotProduct( newVelocity, dir );
				if ( dot > len )
				{
					dot = len;
				}
				else if ( dot < -len )
				{
					dot = -len;
				}

				VectorMA( newVelocity, -dot, dir, newVelocity );

				if ( m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER )
				{
					float val = Lerp( 0.1f, len, dot );
					VectorMA( newVelocity, val - len, dir, newVelocity );
				}

				if ( !IsRideablePhysics(pPhysGround) )
				{
					if ( !(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER ) && IsSimulatingOnAlternateTicks() )
					{
						newVelocity *= 0.5f;
					}
					ApplyAbsVelocityImpulse( newVelocity );
				}
			}

			trace_t trace;
			UTIL_TraceEntity( this, newPosition, newPosition, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( !trace.allsolid && !trace.startsolid )
			{
				SetAbsOrigin( newPosition );
			}
		}
		else
		{
			trace_t trace;

			Ray_t ray;
			ray.Init( GetAbsOrigin(), GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs() );

			CTraceFilterSimple OriginalTraceFilter( this, COLLISION_GROUP_PLAYER_MOVEMENT );
			CTraceFilterTranslateClones traceFilter( &OriginalTraceFilter );
			UTIL_Portal_TraceRay_With( m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace );

			// current position is not ok, fixup
			if ( trace.allsolid || trace.startsolid )
			{
				//try again with new position
				ray.Init( newPosition, newPosition, WorldAlignMins(), WorldAlignMaxs() );
				UTIL_Portal_TraceRay_With( m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace );

				if( trace.startsolid == false )
				{
					SetAbsOrigin( newPosition );
				}
				else
				{
					if( !FindClosestPassableSpace( this, newPosition - GetAbsOrigin(), MASK_PLAYERSOLID ) )
					{
						// Try moving the player closer to the center of the portal
						CProp_Portal *pPortal = m_hPortalEnvironment.Get();
						newPosition += ( pPortal->GetAbsOrigin() - WorldSpaceCenter() ) * 0.1f;
						SetAbsOrigin( newPosition );

						DevMsg( "Hurting the player for FindClosestPassableSpaceFailure!" );

						// Deal 1 damage per frame... this will kill a player very fast, but allow for the above correction to fix some cases
						CTakeDamageInfo info( this, this, vec3_origin, vec3_origin, 1, DMG_CRUSH );
						OnTakeDamage( info );
					}
				}
			}
		}
	}
	else
	{
		if ( m_touchedPhysObject )
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
				MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

			// is current position ok?
			if ( trace.allsolid || trace.startsolid )
			{
				// stuck????!?!?
				//Msg("Stuck on %s\n", trace.m_pEnt->GetClassname());
				SetAbsOrigin( newPosition );
				UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
					MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
				if ( trace.allsolid || trace.startsolid )
				{
					//Msg("Double Stuck\n");
					SetAbsOrigin( m_oldOrigin );
				}
			}
		}
	}
	m_oldOrigin = GetAbsOrigin();
}

bool CPortal_Player::UseFoundEntity( CBaseEntity *pUseEntity )
{
	bool usedSomething = false;

	//!!!UNDONE: traceline here to prevent +USEing buttons through walls			
	int caps = pUseEntity->ObjectCaps();
	variant_t emptyVariant;

	if ( m_afButtonPressed & IN_USE )
	{
		// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
		if ( !pUseEntity->MyNPCPointer() )
		{
			EmitSound( "HL2Player.Use" );
		}
	}

	if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
		( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
	{
		if ( caps & FCAP_CONTINUOUS_USE )
			m_afPhysicsFlags |= PFLAG_USING;

		pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

		usedSomething = true;
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	else if ( (m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
	{
		pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

		usedSomething = true;
	}

#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	//Check for weapon pick-up
	if ( m_afButtonPressed & IN_USE )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(pUseEntity);

		if ( ( pWeapon != NULL ) && ( Weapon_CanSwitchTo( pWeapon ) ) )
		{
			//Try to take ammo or swap the weapon
			if ( Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType() ) )
			{
				Weapon_EquipAmmoOnly( pWeapon );
			}
			else
			{
				Weapon_DropSlot( pWeapon->GetSlot() );
				Weapon_Equip( pWeapon );
			}

			usedSomething = true;
		}
	}
#endif

	return usedSomething;
}

//bool CPortal_Player::StartReplayMode( float fDelay, float fDuration, int iEntity )
//{
//	if ( !BaseClass::StartReplayMode( fDelay, fDuration, 1 ) )
//		return false;
//
//	CSingleUserRecipientFilter filter( this );
//	filter.MakeReliable();
//
//	UserMessageBegin( filter, "KillCam" );
//
//	EHANDLE hPlayer = this;
//
//	if ( m_hObserverTarget.Get() )
//	{
//		WRITE_EHANDLE( m_hObserverTarget );	// first target
//		WRITE_EHANDLE( hPlayer );	//second target
//	}
//	else
//	{
//		WRITE_EHANDLE( hPlayer );	// first target
//		WRITE_EHANDLE( 0 );			//second target
//	}
//	MessageEnd();
//
//	return true;
//}
//
//void CPortal_Player::StopReplayMode()
//{
//	BaseClass::StopReplayMode();
//
//	CSingleUserRecipientFilter filter( this );
//	filter.MakeReliable();
//
//	UserMessageBegin( filter, "KillCam" );
//	WRITE_EHANDLE( 0 );
//	WRITE_EHANDLE( 0 );
//	MessageEnd();
//}

void CPortal_Player::PlayerUse( void )
{
	// Was use pressed or released?
	if ( ! ((m_nButtons | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	if ( m_afButtonPressed & IN_USE )
	{
		// Currently using a latched entity?
		if ( ClearUseEntity() )
		{
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_DIROVERRIDE )
			{
				m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = GetGroundEntity();
				if ( pTrain && !(m_nButtons & IN_JUMP) && (GetFlags() & FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(this) )
				{
					m_afPhysicsFlags |= PFLAG_DIROVERRIDE;
					m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
					m_iTrain |= TRAIN_NEW;
					EmitSound( "HL2Player.TrainUse" );
					return;
				}
			}
		}

		// Tracker 3926:  We can't +USE something if we're climbing a ladder
		if ( GetMoveType() == MOVETYPE_LADDER )
		{
			return;
		}
	}

	CBaseEntity *pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if ( pUseEntity )
	{
		SetHeldObjectOnOppositeSideOfPortal( false );

		// TODO: Removed because we no longer have ghost animatings. May need to rework this code.
		//// If we found a ghost animating then it needs to be held across a portal
		//CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( pUseEntity );
		//if ( pGhostAnimating )
		//{
		//	CProp_Portal *pPortal = NULL;

		//	CPortalSimulator *pPortalSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pGhostAnimating->GetSourceEntity() );

		//	//HACKHACK: This assumes all portal simulators are a member of a prop_portal
		//	pPortal = (CProp_Portal *)(((char *)pPortalSimulator) - ((int)&(((CProp_Portal *)0)->m_PortalSimulator)));
		//	Assert( (&(pPortal->m_PortalSimulator)) == pPortalSimulator ); //doublechecking the hack

		//	if ( pPortal )
		//	{
		//		SetHeldObjectPortal( pPortal->m_hLinkedPortal );
		//		SetHeldObjectOnOppositeSideOfPortal( true );
		//	}
		//}
		usedSomething = UseFoundEntity( pUseEntity );
	}
	else 
	{
		Vector forward;
		EyeVectors( &forward, NULL, NULL );
		Vector start = EyePosition();

		Ray_t rayPortalTest;
		rayPortalTest.Init( start, start + forward * PLAYER_USE_RADIUS );

		float fMustBeCloserThan = 2.0f;

		CProp_Portal *pPortal = UTIL_Portal_FirstAlongRay( rayPortalTest, fMustBeCloserThan );

		if ( pPortal )
		{
			SetHeldObjectPortal( pPortal );
			pUseEntity = FindUseEntityThroughPortal();
		}

		if ( pUseEntity )
		{
			SetHeldObjectOnOppositeSideOfPortal( true );
			usedSomething = UseFoundEntity( pUseEntity );
		}
		else if ( m_afButtonPressed & IN_USE )
		{
			// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
			// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
			// lets the ladder code unset this flag.
			m_bPlayUseDenySound = true;
		}
	}

	// Debounce the use key
	if ( usedSomething && pUseEntity )
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}
}

void CPortal_Player::PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper)
{
	if( m_bFixEyeAnglesFromPortalling )
	{
		//the idea here is to handle the notion that the player has portalled, but they sent us an angle update before receiving that message.
		//If we don't handle this here, we end up sending back their old angles which makes them hiccup their angles for a frame
		float fOldAngleDiff = fabs( AngleDistance( ucmd->viewangles.x, m_qPrePortalledViewAngles.x ) );
		fOldAngleDiff += fabs( AngleDistance( ucmd->viewangles.y, m_qPrePortalledViewAngles.y ) );
		fOldAngleDiff += fabs( AngleDistance( ucmd->viewangles.z, m_qPrePortalledViewAngles.z ) );

		float fCurrentAngleDiff = fabs( AngleDistance( ucmd->viewangles.x, pl.v_angle.x ) );
		fCurrentAngleDiff += fabs( AngleDistance( ucmd->viewangles.y, pl.v_angle.y ) );
		fCurrentAngleDiff += fabs( AngleDistance( ucmd->viewangles.z, pl.v_angle.z ) );

		if( fCurrentAngleDiff > fOldAngleDiff )
			ucmd->viewangles = TransformAnglesToWorldSpace( ucmd->viewangles, m_matLastPortalled.As3x4() );

		m_bFixEyeAnglesFromPortalling = false;
	}

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}


bool CPortal_Player::ClientCommand( const CCommand &args )
{
	if ( FStrEq( args[0], "spectate" ) )
	{
		// do nothing.
		return true;
	}

	return BaseClass::ClientCommand( args );
}

void CPortal_Player::CheatImpulseCommands( int iImpulse )
{
	switch ( iImpulse )
	{
	case 101:
		{
			if( sv_cheats->GetBool() )
			{
				GiveAllItems();
			}
		}
		break;

	default:
		BaseClass::CheatImpulseCommands( iImpulse );
	}
}

void CPortal_Player::CreateViewModel( int index /*=0*/ )
{
	BaseClass::CreateViewModel( index );
	return;
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}

bool CPortal_Player::BecomeRagdollOnClient( const Vector &force )
{
	return true;//BaseClass::BecomeRagdollOnClient( force );
}

void CPortal_Player::CreateRagdollEntity( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

#if PORTAL_HIDE_PLAYER_RAGDOLL
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	AddEFlags( EFL_NO_DISSOLVE );
#endif // PORTAL_HIDE_PLAYER_RAGDOLL
	CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
	pRagdoll->m_takedamage = DAMAGE_NO;
	m_hRagdoll = pRagdoll;

/*
	// If we already have a ragdoll destroy it.
	CPortalRagdoll *pRagdoll = dynamic_cast<CPortalRagdoll*>( m_hRagdoll.Get() );
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
		pRagdoll = NULL;
	}
	Assert( pRagdoll == NULL );

	// Create a ragdoll.
	pRagdoll = dynamic_cast<CPortalRagdoll*>( CreateEntityByName( "portal_ragdoll" ) );
	if ( pRagdoll )
	{
		

		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->CopyAnimationDataFrom( this );
		pRagdoll->SetOwnerEntity( this );
		pRagdoll->m_flAnimTime = gpGlobals->curtime;
		pRagdoll->m_flPlaybackRate = 0.0;
		pRagdoll->SetCycle( 0 );
		pRagdoll->ResetSequence( 0 );

		float fSequenceDuration = SequenceDuration( GetSequence() );
		float fPreviousCycle = clamp(GetCycle()-( 0.1 * ( 1 / fSequenceDuration ) ),0.f,1.f);
		float fCurCycle = GetCycle();
		matrix3x4_t pBoneToWorld[MAXSTUDIOBONES], pBoneToWorldNext[MAXSTUDIOBONES];
		SetupBones( pBoneToWorldNext, BONE_USED_BY_ANYTHING );
		SetCycle( fPreviousCycle );
		SetupBones( pBoneToWorld, BONE_USED_BY_ANYTHING );
		SetCycle( fCurCycle );

		pRagdoll->InitRagdoll( info.GetDamageForce(), m_nForceBone, info.GetDamagePosition(), pBoneToWorld, pBoneToWorldNext, 0.1f, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		pRagdoll->SetMoveType( MOVETYPE_VPHYSICS );
		pRagdoll->SetSolid( SOLID_VPHYSICS );
		if ( IsDissolving() )
		{
			pRagdoll->TransferDissolveFrom( this );
		}

		Vector mins, maxs;
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
		pRagdoll->CollisionProp()->SetCollisionBounds( mins, maxs );
		pRagdoll->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	// Turn off the player.
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	SetMoveType( MOVETYPE_NONE );

	// Save ragdoll handle.
	m_hRagdoll = pRagdoll;
*/
}

void CPortal_Player::Jump( void )
{
	g_PortalGameStats.Event_PlayerJump( GetAbsOrigin(), GetAbsVelocity() );
	BaseClass::Jump();
}

void CPortal_Player::Event_Killed( const CTakeDamageInfo &info )
{
	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	//if( info.GetAttacker() )
	//{
	//	// set new target
	//	m_hObserverTarget.Set( info.GetAttacker() ); 
	//}
	//else
	//	m_hObserverTarget.Set( NULL );

	UpdateExpression();

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity( info );

	BaseClass::Event_Killed( subinfo );

#if PORTAL_HIDE_PLAYER_RAGDOLL
	// Fizzle all portals so they don't see the player disappear
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal *pTempPortal = pPortals[i];

		if( pTempPortal && pTempPortal->m_bActivated )
		{
			pTempPortal->Fizzle();
		}
	}
#endif // PORTAL_HIDE_PLAYER_RAGDOLL

	if ( (info.GetDamageType() & DMG_DISSOLVE) && !(m_hRagdoll.Get()->GetEFlags() & EFL_NO_DISSOLVE) )
	{
		if ( m_hRagdoll )
		{
			m_hRagdoll->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
	}

	m_lifeState = LIFE_DYING;
	StopZooming();

	if ( GetObserverTarget() )
	{
		//StartReplayMode( 3, 3, GetObserverTarget()->entindex() );
		//StartObserverMode( OBS_MODE_DEATHCAM );
	}
}

int CPortal_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo inputInfoCopy( inputInfo );

	// If you shoot yourself, make it hurt but push you less
	if ( inputInfoCopy.GetAttacker() == this && inputInfoCopy.GetDamageType() == DMG_BULLET )
	{
		inputInfoCopy.ScaleDamage( 5.0f );
		inputInfoCopy.ScaleDamageForce( 0.05f );
	}

	CBaseEntity *pAttacker = inputInfoCopy.GetAttacker();
	CBaseEntity *pInflictor = inputInfoCopy.GetInflictor();

	bool bIsTurret = false;

	if ( pAttacker && FClassnameIs( pAttacker, "npc_portal_turret_floor" ) )
		bIsTurret = true;

	// Refuse damage from prop_glados_core.
	if ( (pAttacker && FClassnameIs( pAttacker, "prop_glados_core" )) ||
		(pInflictor && FClassnameIs( pInflictor, "prop_glados_core" ))  )
	{
		inputInfoCopy.SetDamage(0.0f);
	}

	if ( bIsTurret && ( inputInfoCopy.GetDamageType() & DMG_BULLET ) )
	{
		Vector vLateralForce = inputInfoCopy.GetDamageForce();
		vLateralForce.z = 0.0f;

		// Push if the player is moving against the force direction
		if ( GetAbsVelocity().Dot( vLateralForce ) < 0.0f )
			ApplyAbsVelocityImpulse( vLateralForce );
	}
	else if ( ( inputInfoCopy.GetDamageType() & DMG_CRUSH ) )
	{
		if ( bIsTurret )
		{
			inputInfoCopy.SetDamage( inputInfoCopy.GetDamage() * 0.5f );
		}

		if ( inputInfoCopy.GetDamage() >= 10.0f )
		{
			EmitSound( "PortalPlayer.BonkYelp" );
		}
	}
	else if ( ( inputInfoCopy.GetDamageType() & DMG_SHOCK ) || ( inputInfoCopy.GetDamageType() & DMG_BURN ) )
	{
		EmitSound( "PortalPortal.PainYelp" );
	}

	int ret = BaseClass::OnTakeDamage( inputInfoCopy );

	// Copy the multidamage damage origin over what the base class wrote, because
	// that gets translated correctly though portals.
	m_DmgOrigin = inputInfo.GetDamagePosition();

	if ( GetHealth() < 100 )
	{
		m_fTimeLastHurt = gpGlobals->curtime;
	}

	return ret;
}

int CPortal_Player::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// set damage type sustained
	m_bitsDamageType |= info.GetDamageType();

	if ( !CBaseCombatCharacter::OnTakeDamage_Alive( info ) )
		return 0;

	CBaseEntity * attacker = info.GetAttacker();

	if ( !attacker )
		return 0;

	Vector vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize( vecDir );
	}

	if ( info.GetInflictor() && (GetMoveType() == MOVETYPE_WALK) && 
		( !attacker->IsSolidFlagSet(FSOLID_TRIGGER)) )
	{
		Vector force = vecDir;// * -DamageForce( WorldAlignSize(), info.GetBaseDamage() );
		if ( force.z > 250.0f )
		{
			force.z = 250.0f;
		}
		ApplyAbsVelocityImpulse( force );
	}

	// fire global game event

	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("health", MAX(0, m_iHealth) );
		event->SetInt("priority", 5 );	// HLTV event priority, not transmitted

		if ( attacker->IsPlayer() )
		{
			CBasePlayer *player = ToBasePlayer( attacker );
			event->SetInt("attacker", player->GetUserID() ); // hurt by other player
		}
		else
		{
			event->SetInt("attacker", 0 ); // hurt by "world"
		}

		gameeventmanager->FireEvent( event );
	}

	// Insert a combat sound so that nearby NPCs hear battle
	if ( attacker->IsNPC() )
	{
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 512, 0.5, this );//<<TODO>>//magic number
	}

	return 1;
}


void CPortal_Player::ForceDuckThisFrame( void )
{
	if( m_Local.m_bDucked != true )
	{
		//m_Local.m_bDucking = false;
		m_Local.m_bDucked = true;
		ForceButtons( IN_DUCK );
		AddFlag( FL_DUCKING );
		SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), VPHYS_CROUCH );
	}
}

void CPortal_Player::UnDuck( void )
{
	if( m_Local.m_bDucked != false )
	{
		m_Local.m_bDucked = false;
		UnforceButtons( IN_DUCK );
		RemoveFlag( FL_DUCKING );
		SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), VPHYS_WALK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Overload for portal-- Our player can lift his own mass.
// Input  : *pObject - The object to lift
//			bLimitMassAndSize - check for mass/size limits
//-----------------------------------------------------------------------------
void CPortal_Player::PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize )
{
	// can't pick up what you're standing on
	if ( GetGroundEntity() == pObject )
		return;

	if ( bLimitMassAndSize == true )
	{
		if ( CBasePlayer::CanPickupObject( pObject, PORTAL_PLAYER_MAX_LIFT_MASS, PORTAL_PLAYER_MAX_LIFT_SIZE ) == false )
			return;
	}

	// Can't be picked up if NPCs are on me
	if ( pObject->HasNPCsOnIt() )
		return;

	PlayerPickupObject( this, pObject );
}

void CPortal_Player::ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis )
{
	m_bHeldObjectOnOppositeSideOfPortal = false;
	BaseClass::ForceDropOfCarriedPhysObjects( pOnlyIfHoldingThis );
}

void CPortal_Player::IncrementPortalsPlaced( void )
{
	m_StatsThisLevel.iNumPortalsPlaced++;

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_PORTALS )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.iNumPortalsPlaced ) );
}

void CPortal_Player::IncrementStepsTaken( void )
{
	m_StatsThisLevel.iNumStepsTaken++;

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_STEPS )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.iNumStepsTaken ) );
}

void CPortal_Player::UpdateSecondsTaken( void )
{
	float fSecondsSinceLastUpdate = ( gpGlobals->curtime - m_fTimeLastNumSecondsUpdate );
	m_StatsThisLevel.fNumSecondsTaken += fSecondsSinceLastUpdate;
	m_fTimeLastNumSecondsUpdate = gpGlobals->curtime;

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_TIME )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.fNumSecondsTaken ) );

	if ( m_fNeuroToxinDamageTime > 0.0f )
	{
		float fTimeRemaining = m_fNeuroToxinDamageTime - gpGlobals->curtime;

		if ( fTimeRemaining < 0.0f )
		{
			CTakeDamageInfo info;
			info.SetDamage( gpGlobals->frametime * 50.0f );
			info.SetDamageType( DMG_NERVEGAS );
			TakeDamage( info );
			fTimeRemaining = 0.0f;
		}

		PauseBonusProgress( false );
		SetBonusProgress( static_cast<int>( fTimeRemaining ) );
	}
}

void CPortal_Player::ResetThisLevelStats( void )
{
	m_StatsThisLevel.iNumPortalsPlaced = 0;
	m_StatsThisLevel.iNumStepsTaken = 0;
	m_StatsThisLevel.fNumSecondsTaken = 0.0f;

	if ( m_iBonusChallenge != PORTAL_CHALLENGE_NONE )
		SetBonusProgress( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Update the area bits variable which is networked down to the client to determine
//			which area portals should be closed based on visibility.
// Input  : *pvs - pvs to be used to determine visibility of the portals
//-----------------------------------------------------------------------------
void CPortal_Player::UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize )
{
	Assert ( pvs );

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	int *portalArea = (int *)stackalloc( sizeof( int ) * iPortalCount );
	bool *bUsePortalForVis = (bool *)stackalloc( sizeof( bool ) * iPortalCount );

	unsigned char *portalTempBits = (unsigned char *)stackalloc( sizeof( unsigned char ) * 32 * iPortalCount );
	COMPILE_TIME_ASSERT( (sizeof( unsigned char ) * 32) >= sizeof( ((CPlayerLocalData*)0)->m_chAreaBits ) );

	// setup area bits for these portals
	for ( int i = 0; i < iPortalCount; ++i )
	{
		CProp_Portal* pLocalPortal = pPortals[ i ];
		// Make sure this portal is active before adding it's location to the pvs
		if ( pLocalPortal && pLocalPortal->m_bActivated )
		{
			CProp_Portal* pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();

			// Make sure this portal's linked portal is in the PVS before we add what it can see
			if ( pRemotePortal && pRemotePortal->m_bActivated && pRemotePortal->NetworkProp() && 
				pRemotePortal->NetworkProp()->IsInPVS( edict(), pvs, pvssize ) )
			{
				portalArea[ i ] = engine->GetArea( pPortals[ i ]->GetAbsOrigin() );

				if ( portalArea [ i ] >= 0 )
				{
					bUsePortalForVis[ i ] = true;
				}

				engine->GetAreaBits( portalArea[ i ], &portalTempBits[ i * 32 ], sizeof( unsigned char ) * 32 );
			}
		}
	}

	// Use the union of player-view area bits and the portal-view area bits of each portal
	for ( int i = 0; i < m_Local.m_chAreaBits.Count(); i++ )
	{
		for ( int j = 0; j < iPortalCount; ++j )
		{
			// If this portal is active, in PVS and it's location is valid
			if ( bUsePortalForVis[ j ]  )
			{
				m_Local.m_chAreaBits.Set( i, m_Local.m_chAreaBits[ i ] | portalTempBits[ (j * 32) + i ] );
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// AddPortalCornersToEnginePVS
// Subroutine to wrap the adding of portal corners to the PVS which is called once for the setup of each portal.
// input - pPortal: the portal we are viewing 'out of' which needs it's corners added to the PVS
//////////////////////////////////////////////////////////////////////////
void AddPortalCornersToEnginePVS( CProp_Portal* pPortal )
{
	Assert ( pPortal );

	if ( !pPortal )
		return;

	Vector vForward, vRight, vUp;
	pPortal->GetVectors( &vForward, &vRight, &vUp );

	// Center of the remote portal
	Vector ptOrigin			= pPortal->GetAbsOrigin();

	// Distance offsets to the different edges of the portal... Used in the placement checks
	Vector vToTopEdge = vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS );
	Vector vToBottomEdge = -vToTopEdge;
	Vector vToRightEdge = vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS );
	Vector vToLeftEdge = -vToRightEdge;

	// Distance to place PVS points away from portal, to avoid being in solid
	Vector vForwardBump		= vForward * 1.0f;

	// Add center and edges to the engine PVS
	engine->AddOriginToPVS( ptOrigin + vForwardBump);
	engine->AddOriginToPVS( ptOrigin + vToTopEdge + vToLeftEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToTopEdge + vToRightEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToBottomEdge + vToLeftEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToBottomEdge + vToRightEdge + vForwardBump );
}

void PortalSetupVisibility( CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal *pPortal = pPortals[i];

		if ( pPortal && pPortal->m_bActivated )
		{
			if ( pPortal->NetworkProp()->IsInPVS( pPlayer->edict(), pvs, pvssize ) )
			{
				if ( engine->CheckAreasConnected( area, pPortal->NetworkProp()->AreaNum() ) )
				{
					CProp_Portal *pLinkedPortal = static_cast<CProp_Portal*>( pPortal->m_hLinkedPortal.Get() );
					if ( pLinkedPortal )
					{
						AddPortalCornersToEnginePVS ( pLinkedPortal );
					}
				}
			}
		}
	}
}

void CPortal_Player::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();

	// At this point the EyePosition has been added as a view origin, but if we are currently stuck
	// in a portal, our EyePosition may return a point in solid. Find the reflected eye position
	// and use that as a vis origin instead.
	if ( m_hPortalEnvironment )
	{
		CProp_Portal *pPortal = NULL, *pRemotePortal = NULL;
		pPortal = m_hPortalEnvironment;
		pRemotePortal = pPortal->m_hLinkedPortal;

		if ( pPortal && pRemotePortal && pPortal->m_bActivated && pRemotePortal->m_bActivated )
		{		
			Vector ptPortalCenter = pPortal->GetAbsOrigin();
			Vector vPortalForward;
			pPortal->GetVectors( &vPortalForward, NULL, NULL );

			Vector eyeOrigin = EyePosition();
			Vector vEyeToPortalCenter = ptPortalCenter - eyeOrigin;

			float fPortalDist = vPortalForward.Dot( vEyeToPortalCenter );
			if( fPortalDist > 0.0f ) //eye point is behind portal
			{
				// Move eye origin to it's transformed position on the other side of the portal
				UTIL_Portal_PointTransform( pPortal->MatrixThisToLinked(), eyeOrigin, eyeOrigin );

				// Use this as our view origin (as this is where the client will be displaying from)
				engine->AddOriginToPVS( eyeOrigin );
				if ( !pViewEntity || pViewEntity->IsPlayer() )
				{
					area = engine->GetArea( eyeOrigin );
				}	
			}
		}
	}

	PortalSetupVisibility( this, area, pvs, pvssize );
}


#ifdef PORTAL_MP

CBaseEntity* CPortal_Player::EntSelectSpawnPoint( void )
{
	CBaseEntity *pSpot = NULL;
	CBaseEntity *pLastSpawnPoint = g_pLastSpawn;
	edict_t		*player = edict();
	const char *pSpawnpointName = "info_player_start";

	/*if ( HL2MPRules()->IsTeamplay() == true )
	{
	if ( GetTeamNumber() == TEAM_COMBINE )
	{
	pSpawnpointName = "info_player_combine";
	pLastSpawnPoint = g_pLastCombineSpawn;
	}
	else if ( GetTeamNumber() == TEAM_REBELS )
	{
	pSpawnpointName = "info_player_rebel";
	pLastSpawnPoint = g_pLastRebelSpawn;
	}

	if ( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL )
	{
	pSpawnpointName = "info_player_deathmatch";
	pLastSpawnPoint = g_pLastSpawn;
	}
	}*/

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for ( int i = random->RandomInt(1,5); i > 0; i-- )
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	if ( !pSpot )  // skip over the null point
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );

	CBaseEntity *pFirstSpot = pSpot;

	do 
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( g_pGameRules->IsSpawnPointValid( pSpot, this ) )
			{
				if ( pSpot->GetLocalOrigin() == vec3_origin )
				{
					pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
					continue;
				}

				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	if ( pSpot )
	{
		CBaseEntity *ent = NULL;
		for ( CEntitySphereQuery sphere( pSpot->GetAbsOrigin(), 128 ); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			// if ent is a client, kill em (unless they are ourselves)
			if ( ent->IsPlayer() && !(ent->edict() == player) )
				ent->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 300, DMG_GENERIC ) );
		}
		goto ReturnSpot;
	}

	if ( !pSpot  )
	{
		pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_start" );

		if ( pSpot )
			goto ReturnSpot;
	}

ReturnSpot:

	/*if ( HL2MPRules()->IsTeamplay() == true )
	{
	if ( GetTeamNumber() == TEAM_COMBINE )
	{
	g_pLastCombineSpawn = pSpot;
	}
	else if ( GetTeamNumber() == TEAM_REBELS ) 
	{
	g_pLastRebelSpawn = pSpot;
	}
	}*/

	g_pLastSpawn = pSpot;

	//m_flSlamProtectTime = gpGlobals->curtime + 0.5;

	return pSpot;
}

void CPortal_Player::PickTeam( void )
{
	//picks lowest or random
	CTeam *pCombine = g_Teams[TEAM_COMBINE];
	CTeam *pRebels = g_Teams[TEAM_REBELS];
	if ( pCombine->GetNumPlayers() > pRebels->GetNumPlayers() )
	{
		ChangeTeam( TEAM_REBELS );
	}
	else if ( pCombine->GetNumPlayers() < pRebels->GetNumPlayers() )
	{
		ChangeTeam( TEAM_COMBINE );
	}
	else
	{
		ChangeTeam( random->RandomInt( TEAM_COMBINE, TEAM_REBELS ) );
	}
}

#endif

CON_COMMAND( startadmiregloves, "Starts the admire gloves animation." )
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();
	if( pPlayer == NULL )
		pPlayer = GetPortalPlayer( 1 ); //last ditch effort

	if( pPlayer )
		pPlayer->StartAdmireGlovesAnimation();
}

CON_COMMAND( displayportalplayerstats, "Displays current level stats for portals placed, steps taken, and seconds taken." )
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();
	if( pPlayer == NULL )
		pPlayer = GetPortalPlayer( 1 ); //last ditch effort

	if( pPlayer )
	{
		int iMinutes = static_cast<int>( pPlayer->NumSecondsTaken() / 60.0f );
		int iSeconds = static_cast<int>( pPlayer->NumSecondsTaken() ) % 60;

		CFmtStr msg;
		NDebugOverlay::ScreenText( 0.5f, 0.5f, msg.sprintf( "Portals Placed: %d\nSteps Taken: %d\nTime: %d:%d", pPlayer->NumPortalsPlaced(), pPlayer->NumStepsTaken(), iMinutes, iSeconds ), 255, 255, 255, 150, 5.0f );
	}
}

CON_COMMAND( startneurotoxins, "Starts the nerve gas timer." )
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();
	if( pPlayer == NULL )
		pPlayer = GetPortalPlayer( 1 ); //last ditch effort

	float fCoundownTime = 180.0f;

	if ( args.ArgC() > 1 )
		fCoundownTime = atof( args[ 1 ] );

	if( pPlayer )
		pPlayer->SetNeuroToxinDamageTime( fCoundownTime );
}
