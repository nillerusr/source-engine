//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Player for Swarm.  This is an invisible entity that doesn't move, representing the commander.
//                  The player drives movement of CASW_Marine NPC entities
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "asw_player.h"
#include "in_buttons.h"
#include "asw_gamerules.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "asw_marine_speech.h"
#include "asw_marine_profile.h"
#include "asw_spawner.h"
#include "asw_alien.h"
#include "asw_simple_alien.h"
#include "asw_pickup.h"
#include "asw_use_area.h"
#include "asw_button_area.h"
#include "asw_weapon.h"
#include "asw_ammo.h"
#include "asw_weapon_parse.h"
#include "asw_computer_area.h"
#include "asw_hack.h"
#include "asw_point_camera.h"
#include "ai_waypoint.h"
#include "inetchannelinfo.h"
#include "asw_sentry_base.h"
#include "asw_shareddefs.h"
#include "iasw_vehicle.h"
#include "obstacle_pushaway.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "asw_door.h"
#include "asw_remote_turret_shared.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "Sprite.h"
#include "physics_prop_ragdoll.h"
#include "asw_util_shared.h"
#include "asw_voting_missions.h"
#include "asw_campaign_save.h"
#include "gib.h"
#include "asw_intro_control.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "ai_network.h"
#include "ai_navigator.h"
#include "ai_node.h"
#include "datacache/imdlcache.h"
#include "asw_spawn_manager.h"
#include "asw_campaign_info.h"
#include "sendprop_priorities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_PLAYER_MODEL "models/swarm/marine/Marine.mdl"

//#define MARINE_ORDER_DISTANCE 500
#define MARINE_ORDER_DISTANCE 32000


ConVar asw_blend_test_scale("asw_blend_test_scale", "0.1f", FCVAR_CHEAT);
ConVar asw_debug_pvs("asw_debug_pvs", "0", FCVAR_CHEAT);
extern ConVar asw_rts_controls;
extern ConVar asw_DebugAutoAim;
extern ConVar asw_debug_marine_damage;

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
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event )
{
	CPVSFilter filter( (const Vector&) pPlayer->EyePosition() );
	
	// The player himself doesn't need to be sent his animation events 
	// unless cs_showanimstate wants to show them.
	//if ( asw_showanimstate.GetInt() == pPlayer->entindex() )
	//{
		//filter.RemoveRecipient( pPlayer );
	//}

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

// -------------------------------------------------------------------------------- //
// Marine animation event.
// -------------------------------------------------------------------------------- //

class CTEMarineAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEMarineAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEMarineAnimEvent( const char *name ) : CBaseTempEntity( name )
	{
	}

	CNetworkHandle( CBasePlayer, m_hExcludePlayer );
	CNetworkHandle( CASW_Marine, m_hMarine );
	CNetworkVar( int, m_iEvent );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEMarineAnimEvent, DT_TEMarineAnimEvent )
	SendPropEHandle( SENDINFO( m_hMarine ) ),
	SendPropEHandle( SENDINFO( m_hExcludePlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

static CTEMarineAnimEvent g_TEMarineAnimEvent( "MarineAnimEvent" );

void TE_MarineAnimEvent( CASW_Marine *pMarine, PlayerAnimEvent_t event )
{
	CPVSFilter filter( (const Vector&) pMarine->EyePosition() );

	g_TEMarineAnimEvent.m_hMarine = pMarine;
	g_TEMarineAnimEvent.m_hExcludePlayer = NULL;
	g_TEMarineAnimEvent.m_iEvent = event;
	g_TEMarineAnimEvent.Create( filter, 0 );
}

void TE_MarineAnimEventExceptCommander( CASW_Marine *pMarine, PlayerAnimEvent_t event )
{
	if (!pMarine)
		return;
	CPVSFilter filter( (const Vector&) pMarine->EyePosition() );
	
	if (pMarine->GetCommander() && pMarine->IsInhabited())
		g_TEMarineAnimEvent.m_hExcludePlayer = pMarine->GetCommander();
	else
		g_TEMarineAnimEvent.m_hExcludePlayer = NULL;
		//filter.RemoveRecipient(pMarine->GetCommander());
	g_TEMarineAnimEvent.m_hMarine = pMarine;
	g_TEMarineAnimEvent.m_iEvent = event;
	g_TEMarineAnimEvent.Create( filter, 0 );
}

// NOTE: This animevent won't get recorded in demos properly, since it's not sent to everyone!
void TE_MarineAnimEventJustCommander( CASW_Marine *pMarine, PlayerAnimEvent_t event )
{	
	if (!pMarine || !pMarine->IsInhabited())
		return;
	
	if (!pMarine->GetCommander())
		return;

	CRecipientFilter filter;
	filter.RemoveAllRecipients();
	filter.AddRecipient(pMarine->GetCommander());

	g_TEMarineAnimEvent.m_hExcludePlayer = NULL;
	g_TEMarineAnimEvent.m_hMarine = pMarine;
	g_TEMarineAnimEvent.m_iEvent = event;
	g_TEMarineAnimEvent.Create( filter, 0 );
}

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( player, CASW_Player );
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST( CASW_Player, DT_ASW_Player )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	//SendPropInt		(SENDINFO(m_iHealth), 10 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 10, 0, SendProxy_AngleToFloat, SENDPROP_PLAYER_EYE_ANGLES_PRIORITY ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, 0, SendProxy_AngleToFloat, SENDPROP_PLAYER_EYE_ANGLES_PRIORITY ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 2), 10, 0, SendProxy_AngleToFloat, SENDPROP_PLAYER_EYE_ANGLES_PRIORITY ),
	SendPropEHandle( SENDINFO ( m_hMarine ) ),
	SendPropEHandle( SENDINFO( m_hSpectatingMarine ) ),
	SendPropEHandle( SENDINFO( m_hOrderingMarine ) ),
	SendPropEHandle( SENDINFO ( m_pCurrentInfoMessage ) ),
	SendPropEHandle( SENDINFO ( m_hVotingMissions ) ),
	
	SendPropInt(SENDINFO(m_iLeaderVoteIndex) ),
	SendPropInt(SENDINFO(m_iKickVoteIndex) ),
	SendPropFloat( SENDINFO( m_fMapGenerationProgress ) ),

	SendPropTime( SENDINFO( m_flUseKeyDownTime ) ),
	SendPropEHandle( SENDINFO ( m_hUseKeyDownEnt ) ),
	SendPropInt		(SENDINFO(m_nChangingSlot)),
	SendPropInt	(SENDINFO( m_iMapVoted ) ),
	SendPropInt		(SENDINFO( m_iNetworkedXP ) ),
	SendPropInt		(SENDINFO( m_iNetworkedPromotion ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Player )
	DEFINE_FIELD( m_fIsWalking, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecLastMarineOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_hMarine, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpectatingMarine, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecStoredPosition, FIELD_VECTOR ),	
	DEFINE_FIELD( m_pCurrentInfoMessage, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_fClearInfoMessageTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_bLastAttackButton, FIELD_BOOLEAN ),		// keep this at no click after restore
	DEFINE_FIELD( m_iUseEntities, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_hUseEntities, FIELD_EHANDLE ),
	DEFINE_FIELD( m_fBlendAmount, FIELD_FLOAT ),
	DEFINE_FIELD( m_angEyeAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_iLeaderVoteIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_iKickVoteIndex, FIELD_INTEGER ),	
	DEFINE_FIELD( m_hVotingMissions, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iKLVotesStarted, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastKLVoteTime, FIELD_TIME ),
	DEFINE_FIELD( m_hOrderingMarine, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecFreeCamOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_bUsedFreeCam, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSentJoinedMessage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iMapVoted, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastControlledMarineTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecCrosshairTracePos, FIELD_VECTOR ),
END_DATADESC()

// -------------------------------------------------------------------------------- //

void ASW_GetNumAIAwake(const char* szClass, int &iAwake, int &iAsleep, int &iNormal, int &iEfficient, int &iVEfficient, int &iSEfficient, int &iDormant)
{
	CBaseEntity* pEntity = NULL;
	iAwake = 0;
	iAsleep = 0;
	iNormal = iEfficient = iVEfficient = iSEfficient = iDormant = 0;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szClass )) != NULL)
	{
		CAI_BaseNPC* pAI = dynamic_cast<CAI_BaseNPC*>(pEntity);			
		if (pAI)
		{
			if (pAI->GetEfficiency() == AIE_NORMAL)
				iNormal++;
			else if (pAI->GetEfficiency() == AIE_EFFICIENT)
				iEfficient++;
			else if (pAI->GetEfficiency() == AIE_VERY_EFFICIENT)
				iVEfficient++;
			else if (pAI->GetEfficiency() == AIE_SUPER_EFFICIENT)
				iSEfficient++;
			else if (pAI->GetEfficiency() == AIE_DORMANT)
				iDormant++;

			if (pAI->GetSleepState() == AISS_AWAKE)
				iAwake++;
			else
				iAsleep++;
		}
		else
		{
			CASW_Simple_Alien *pSimple = dynamic_cast<CASW_Simple_Alien*>(pEntity);
			if (pSimple)
			{
				if (!pSimple->m_bSleeping)
					iAwake++;
				else
					iAsleep++;
			}
		}
	}
}

void ASW_DrawAwakeAI()
{
	int iAwake = 0;
	int iAsleep = 0;
	int iDormant = 0;
	int iEfficient = 0;
	int iVEfficient = 0;
	int iSEfficient = 0;
	int iNormal = 0;
	int nprintIndex = 18;
	engine->Con_NPrintf( nprintIndex, "AI (awake/asleep) (normal/efficient/very efficient/super efficient/dormant)");
	nprintIndex++;
	engine->Con_NPrintf( nprintIndex, "================================");
	nprintIndex++;
	for ( int i = 0; i < ASWSpawnManager()->GetNumAlienClasses(); i++ )
	{
		ASW_GetNumAIAwake( ASWSpawnManager()->GetAlienClass( i )->m_pszAlienClass, iAwake, iAsleep, iNormal, iEfficient, iVEfficient, iSEfficient, iDormant);
		engine->Con_NPrintf( nprintIndex, "%s: (%d / %d) (%d / %d / %d / %d / %d)\n", ASWSpawnManager()->GetAlienClass( i )->m_pszAlienClass, iAwake, iAsleep, iNormal, iEfficient, iVEfficient, iSEfficient, iDormant );
		nprintIndex++;
	}
}
ConVar asw_draw_awake_ai( "asw_draw_awake_ai", "0", FCVAR_CHEAT, "Lists how many of each AI are awake");

CASW_Player::CASW_Player()
{
	m_PlayerAnimState = CreatePlayerAnimState(this, this, LEGANIM_9WAY, false);
	UseClientSideAnimation();
	m_angEyeAngles.Init();

	SetViewOffset( ASW_PLAYER_VIEW_OFFSET );

	m_hMarine = NULL;
	m_pCurrentInfoMessage = NULL;
	m_vecLastMarineOrigin = vec3_origin;

	m_bLastAttackButton= false;
	m_fLastAICountTime = 0;
	m_bAutoReload = true;	
	m_bRequestedSpectator = false;
	m_bPrintedWantStartMessage = false;
	m_bPrintedWantsContinueMessage = false;
	m_bFirstInhabit = false;
	m_hUseKeyDownEnt = NULL;
	m_flUseKeyDownTime = 0.0f;
	
	m_Local.m_vecPunchAngle.Set( ROLL, 15 );
	m_Local.m_vecPunchAngle.Set( PITCH, 8 );
	m_Local.m_vecPunchAngle.Set( YAW, 0 );
	m_angMarineAutoAim = vec3_angle;

	m_bPendingSteamStats = false;
	m_flPendingSteamStatsStart = 0.0f;
	m_bHasAwardedXP = false;
	m_bSentPromotedMessage = false;

	m_nChangingSlot = 0;

	if (ASWGameRules())
	{
		ASWGameRules()->ClearLeaderKickVotes(this);

		if (ASWGameRules()->GetGameState() == ASW_GS_INGAME)
		{
			SpectateNextMarine();
		}		
	}
}


CASW_Player::~CASW_Player()
{
	m_PlayerAnimState->Release();
	if (ASWGameRules())
		ASWGameRules()->SetMaxMarines(this);
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CASW_Player::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


CASW_Player *CASW_Player::CreatePlayer( const char *className, edict_t *ed )
{
	CASW_Player::s_PlayerEdict = ed;
	return (CASW_Player*)CreateEntityByName( className );
}

void CASW_Player::PreThink( void )
{
	BaseClass::PreThink();
	HandleSpeedChanges();
}


void CASW_Player::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	if (GetMarine())
		angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	// find nearby usable items
	FindUseEntities();

	if (m_pCurrentInfoMessage.Get() && gpGlobals->curtime >= m_fClearInfoMessageTime)
	{
		m_pCurrentInfoMessage = NULL;
	}	

	// clicking while ingame on mission with no marine makes us spectate the next marine
	if ((!GetMarine() || GetMarine()->GetHealth()<=0)
		&& !HasLiveMarines() && ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
	{
		//Msg("m_nButtons & IN_ATTACK = %d (m_Local.m_nOldButtons & IN_ATTACK) = %d\n", (m_nButtons & IN_ATTACK), (m_Local.m_nOldButtons & IN_ATTACK));
		bool bClicked = (!m_bLastAttackButton && (m_nButtons & IN_ATTACK));

		if ( bClicked || ( !GetSpectatingMarine() && gpGlobals->curtime > m_fLastControlledMarineTime + 6.0f ) )
		{
			SpectateNextMarine();
			m_fLastControlledMarineTime = gpGlobals->curtime;		// set this again so we don't spam SpectateNextMarine
		}
	}
	else
	{
		m_fLastControlledMarineTime = gpGlobals->curtime;
	}

	m_bLastAttackButton = (m_nButtons & IN_ATTACK);

	RagdollBlendTest();

	if (asw_draw_awake_ai.GetBool() && gpGlobals->curtime - m_fLastAICountTime > 2.0f)
	{
		ASW_DrawAwakeAI();
		m_fLastAICountTime = gpGlobals->curtime;
	}
}

CBaseEntity* CASW_Player::GetUseEntity(int i) 
{
	return m_hUseEntities[i];
}

void CASW_Player::Precache()
{
	PrecacheModel( ASW_PLAYER_MODEL );
	PrecacheModel( "models/swarm/OrderArrow/OrderArrow.mdl" );
	// ASWTODO - precache the actual model set in asw_client_corpse.cpp rather than assuming this one
	PrecacheModel( "models/swarm/colonist/male/malecolonist.mdl" );

	PrecacheScriptSound( "noslow.BulletTimeIn" );
	PrecacheScriptSound( "noslow.BulletTimeOut" );
	PrecacheScriptSound( "noslow.SingleBreath" );
	PrecacheScriptSound( "Game.ObjectiveComplete" );
	PrecacheScriptSound( "Game.MissionWon" );
	PrecacheScriptSound( "Game.MissionLost" );
	PrecacheScriptSound( "asw_song.stims" );
	//PrecacheScriptSound( "Holdout.GetReadySlide" );
	//PrecacheScriptSound( "Holdout.GetReadySlam" );
	PrecacheScriptSound( "asw_song.statsSuccess" );
	PrecacheScriptSound( "asw_song.statsFail" );

	if (MarineProfileList())
	{
		MarineProfileList()->PrecacheSpeech(this);
	}
	else
	{
		Msg("Couldn't precache marine speech as profile list isn't created yet\n");
	}

	BaseClass::Precache();
}

void CASW_Player::Spawn()
{
	SetModel( ASW_PLAYER_MODEL );

	BaseClass::Spawn();
	
	SetMoveType( MOVETYPE_WALK );	
	m_takedamage = DAMAGE_NO;
	m_iKickVoteIndex = -1;
	m_iLeaderVoteIndex = -1;
	BecomeNonSolid();

	if (ASWGameRules())
	{
		ASWGameRules()->SetMaxMarines();

		if (ASWGameRules()->IsOutroMap())
		{
			CASW_Intro_Control* pIntro = dynamic_cast<CASW_Intro_Control*>(gEntList.FindEntityByClassname( NULL, "asw_intro_control" ));
			if (pIntro)
			{
				pIntro->PlayerSpawned(this);
			}
		}
	}
}

void CASW_Player::DoAnimationEvent( PlayerAnimEvent_t event )
{
	TE_PlayerAnimEvent( this, event );	// Send to any clients who can see this guy.
}

CBaseCombatWeapon* CASW_Player::ASWAnim_GetActiveWeapon()
{
	return GetActiveWeapon();
}

void CASW_Player::EmitPrivateSound( const char *soundName, bool bFromMarine )
{
	CSoundParameters params;
	if (!GetParametersForSound( soundName, params, NULL ))
		return;

	CSingleUserRecipientFilter filter( this );
	if ( bFromMarine && GetMarine() )
	{
		EmitSound( filter, GetMarine()->entindex(), soundName );
	}
	else
	{
		EmitSound( filter, entindex(), soundName );
	}
}


bool CASW_Player::ASWAnim_CanMove()
{
	return true;
}

bool CASW_Player::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];

	switch ( ASWGameRules()->GetGameState() )
	{
		case ASW_GS_BRIEFING:
		{
			if ( FStrEq( pcmd, "cl_selectm" ) )			// selecting a marine from the roster
			{
				CASW_Game_Resource *pGameResource = ASWGameResource();
				if (!pGameResource)
					return true;

				if ( args.ArgC() < 3 )
				{
					Warning( "Player sent bad cl_selectm command\n" );
				}

				int iRosterIndex = clamp(atoi( args[1] ), 0, ASW_NUM_MARINE_PROFILES-1);
				int nPreferredSlot = atoi( args[2] );
				if (ASWGameRules()->RosterSelect( this, iRosterIndex, nPreferredSlot ) )
				{
					// did they specify a previous inventory selection too?
					if ( args.ArgC() == 6 )
					{
						int iMarineResource = -1;
						for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
						{
							CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
							if (pMR && pMR->GetProfileIndex() == iRosterIndex)
							{
								iMarineResource = i;
								break;
							}
						}
						if (iMarineResource == -1)
							return true;

						m_bRequestedSpectator = false;
						int primary = atoi( args[3] );
						int secondary = atoi( args[4] );
						int extra = atoi( args[5] );

						if (primary != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 0, primary);
						if (secondary != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 1, secondary);
						if (extra != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 2, extra);
					}
				}

				return true;
			}
			else if ( FStrEq( pcmd, "cl_dselectm" ) )			// selecting a marine from the roster
			{
				if ( args.ArgC() < 2 )
				{
					Warning( "Player sent bad cl_dselectm command\n" );
					return true;
				}

				if (!ASWGameRules())
					return true;

				int iRosterIndex = clamp(atoi( args[1] ), 0, ASW_NUM_MARINE_PROFILES-1);
				ASWGameRules()->RosterDeselect(this, iRosterIndex);

				return true;
			}
			if ( FStrEq( pcmd, "cl_selectsinglem" ) )			// selecting a marine from the roster, deselecting our current
			{
				CASW_Game_Resource *pGameResource = ASWGameResource();
				if (!pGameResource)
					return true;

				if ( args.ArgC() < 2 )
				{
					Warning( "Player sent bad cl_selectsinglem command\n" );
				}

				// deselect any current marines
				for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
				{
					CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
					if ( !pMR )
						break;

					if ( pMR && pMR->GetCommander() == this )
					{	
						ASWGameRules()->RosterDeselect( this, pMR->GetProfileIndex() );
					}
				}

				// now select the new marine
				int iRosterIndex = clamp(atoi( args[1] ), 0, ASW_NUM_MARINE_PROFILES-1);
				if (ASWGameRules()->RosterSelect(this, iRosterIndex))
				{

					// did they specify a previous inventory selection too?
					if (args.ArgC() == 5)
					{
						int iMarineResource = -1;
						for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
						{
							CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
							if (pMR && pMR->GetProfileIndex() == iRosterIndex)
							{
								iMarineResource = i;
								break;
							}
						}
						if (iMarineResource == -1)
							return true;

						m_bRequestedSpectator = false;
						int primary = atoi( args[2] );
						int secondary = atoi( args[3] );
						int extra = atoi( args[4] );

						if (primary != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 0, primary);
						if (secondary != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 1, secondary);
						if (extra != -1)
							ASWGameRules()->LoadoutSelect(this, iRosterIndex, 2, extra);
					}
				}

				return true;
			}
			else if ( FStrEq( pcmd, "cl_revive" ) )			// revive all dead marines, leader only
			{
				if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() && ASWGameRules())
				{
					ASWGameRules()->ReviveDeadMarines();
				}

				return true;
			}
			else if ( FStrEq( pcmd, "cl_wants_start" ) )			// print a message telling the other players that you want to start
			{
				if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() && ASWGameRules())
				{
					if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING)
					{
						if (!m_bPrintedWantStartMessage)
						{
							UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_wants_start", GetPlayerName() );
							m_bPrintedWantStartMessage = true;
						}
					}
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_loadout" ) )			// selecting equipment
			{
				if ( args.ArgC() < 4 )
				{
					Warning( "Player sent bad loadout command\n" );
				}

				int iProfileIndex = clamp(atoi( args[1] ), 0, ASW_NUM_MARINE_PROFILES-1);
				int iInvSlot = atoi( args[2] );
				int iEquipIndex = atoi( args[3] );

				ASWGameRules()->LoadoutSelect(this, iProfileIndex, iInvSlot, iEquipIndex);

				return true;

			}
			else if ( FStrEq( pcmd, "cl_loadouta" ) )			// selecting equipment
			{
				if ( args.ArgC() < 5 )
				{
					Warning( "Player sent bad loadouta command\n" );
				}

				int iProfileIndex = clamp(atoi( args[1] ), 0, ASW_NUM_MARINE_PROFILES-1);
				int iPrimary = atoi( args[2] );
				int iSecondary = atoi( args[3] );
				int iExtra = atoi( args[4] );

				if (iPrimary >=0)
					ASWGameRules()->LoadoutSelect(this, iProfileIndex, 0, iPrimary);
				if (iSecondary >=0)
					ASWGameRules()->LoadoutSelect(this, iProfileIndex, 1, iSecondary);
				if (iExtra >=0)
					ASWGameRules()->LoadoutSelect(this, iProfileIndex, 2, iExtra);

				return true;

			}
			else if ( FStrEq( pcmd, "cl_start" ) )			// done selecting, go ingame
			{
				// check if all players are ready, etc
				BecomeNonSolid();
				// send a message to client telling him to close the briefing
				if (ASWGameRules())
				{			
					ASWGameRules()->RequestStartMission(this);
				}		

				return true;
			}
			else if ( FStrEq( pcmd, "cl_spendskill") )
			{	
				if ( args.ArgC() < 3 )
				{
					Warning("Player sent a bad cl_spendskill command\n");
					return false;
				}

				int iProfileIndex = atoi(args[1]);
				int nSkillSlot = atoi(args[2]);

				if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES )
					return false;

				if (nSkillSlot < 0 || nSkillSlot >= ASW_SKILL_SLOT_SPARE )
					return false;

				if (ASWGameRules() && ASWGameRules()->CanSpendPoint(this, iProfileIndex, nSkillSlot))
					ASWGameRules()->SpendSkill(iProfileIndex, nSkillSlot);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_undoskill" ) )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_undoskill command\n");
					return false;
				}
				int iProfileIndex = atoi(args[1]);
				if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES )
					return false;
				if (ASWGameRules())
					ASWGameRules()->SkillsUndo(this, iProfileIndex);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_skill_up") )
			{
				if (ASWGameRules())
					ASWGameRules()->RequestSkillUp(this);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_skill_down") )
			{
				if (ASWGameRules())
					ASWGameRules()->RequestSkillDown(this);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_hardcore_ff") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_hardcore_ff command\n");
					return false;
				}

				if ( ASWGameResource() && ASWGameResource()->GetLeader() == this )
				{
					bool bOldHardcoreMode = CAlienSwarm::IsHardcoreFF();
					int nHardcore = atoi( args[1] );
					nHardcore = clamp<int>( nHardcore, 0, 1 );

					extern ConVar asw_sentry_friendly_fire_scale;
					extern ConVar asw_marine_ff_absorption;
					asw_sentry_friendly_fire_scale.SetValue( nHardcore );
					asw_marine_ff_absorption.SetValue( 1 - nHardcore );

					if ( CAlienSwarm::IsHardcoreFF() != bOldHardcoreMode )
					{
						CReliableBroadcastRecipientFilter filter;
						filter.RemoveRecipient( this );		// notify everyone except the player changing the setting
						if ( nHardcore > 0 )
						{
							UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_enabled_hardcoreff", GetPlayerName() );
						}
						else
						{
							UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_disabled_hardcoreff", GetPlayerName() );
						}
					}
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_onslaught") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_onslaught command\n");
					return false;
				}

				if ( ASWGameResource() && ASWGameResource()->GetLeader() == this )
				{
					bool bOldOnslaughtMode = CAlienSwarm::IsOnslaught();
					int nOnslaught = atoi( args[1] );
					nOnslaught = clamp<int>( nOnslaught, 0, 1 );

					extern ConVar asw_horde_override;
					extern ConVar asw_wanderer_override;
					asw_horde_override.SetValue( nOnslaught );
					asw_wanderer_override.SetValue( nOnslaught );

					if ( CAlienSwarm::IsOnslaught() != bOldOnslaughtMode )
					{
						CReliableBroadcastRecipientFilter filter;
						filter.RemoveRecipient( this );		// notify everyone except the player changing the setting
						if ( nOnslaught > 0 )
						{
							UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_enabled_onslaught", GetPlayerName() );
						}
						else
						{
							UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_disabled_onslaught", GetPlayerName() );
						}
					}
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_fixedskills") )
			{
				/*
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_fixedskills command\n");
					return false;
				}
				if ( ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetGameState() == ASW_GS_BRIEFING
					&& ASWGameResource()->m_Leader.Get() == this && ASWGameResource() && ASWGameRules()->GetCampaignSave() )
				{
					ASWGameRules()->GetCampaignSave()->m_bFixedSkillPoints = ( atoi( args[1] ) == 1 );
					ASWGameResource()->UpdateMarineSkills( ASWGameRules()->GetCampaignSave() );
					ASWGameRules()->GetCampaignSave()->SaveGameToFile();
				}
				*/
				return true;
			}
			else if ( FStrEq( pcmd, "cl_carnage") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_carnage command\n");
					return false;
				}		
				if (ASWGameRules() && ASWGameResource() && ASWGameResource()->m_Leader.Get() == this)
					ASWGameRules()->SetCarnageMode(atoi(args[1]) == 1);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_uber") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_uber command\n");
					return false;
				}		
				if (ASWGameRules() && ASWGameResource() && ASWGameResource()->m_Leader.Get() == this)
					ASWGameRules()->SetUberMode(atoi(args[1]) == 1);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_hardcore") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_hardcore command\n");
					return false;
				}		
				if (ASWGameRules() && ASWGameResource() && ASWGameResource()->m_Leader.Get() == this)
					ASWGameRules()->SetHardcoreMode(atoi(args[1]) == 1);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_needtech") )
			{
				if (ASWGameResource()->GetLeader() != this)
					return true;
				if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_BRIEFING)
					UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_need_tech" );
				return true;
			}
			else if ( FStrEq( pcmd, "cl_needequip") )
			{
				if (ASWGameResource()->GetLeader() != this)
					return true;
				if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_BRIEFING)
					ASWGameRules()->ReportMissingEquipment();
				return true;
			}
			else if ( FStrEq( pcmd, "cl_needtwoplayers") )
			{
				if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_BRIEFING)
					ASWGameRules()->ReportNeedTwoPlayers();
				return true;
			}
			else if ( FStrEq( pcmd, "cl_editing_slot") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_editing_slot command\n");
					return false;
				}
				m_nChangingSlot = atoi( args[1] );

				return true;
			}
			else if ( FStrEq( pcmd, "cl_promoted" ) )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_promoted command\n");
					return false;
				}
				if ( !m_bSentPromotedMessage )
				{
					m_bSentPromotedMessage = true;
					int nPromote = atoi( args[1] );
					switch ( nPromote )
					{
						case 1: UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_promoted_1", GetPlayerName() ); break;
						case 2: UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_promoted_2", GetPlayerName() ); break;
						case 3: UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_promoted_3", GetPlayerName() ); break;
					}
				}
				return true;
			}

			break;
		}

		case ASW_GS_INGAME:
		{
			if ( FStrEq( pcmd, "cl_marineface" ) )			// ordering a marine to a certain spot + yaw
			{
				if ( args.ArgC() < 6 )
				{
					Warning( "Player sent bad marine face command\n" );
				}

				int iMarineEntIndex = atoi( args[1] );		
				float fYaw = atof( args[2] );
				float x = atof(args[3]);
				float y = atof(args[4]);
				float z = atof(args[5]);
				//Msg("cl_marineface %f, %f, %f\n", x, y, z);
				Vector vecOrderPos(x, y, z);

				OrderMarineFace(iMarineEntIndex, fYaw, vecOrderPos);

				return true;

			}
			else if ( FStrEq( pcmd, "cl_orderfollow" ) )			// ordering a marine to a follow your current marine
			{
				if ( args.ArgC() < 1 )
				{
					Warning( "Player sent bad cl_orderfollow command\n" );
				}

				OrderNearbyMarines(this, ASW_ORDER_FOLLOW);

				return true;
			}
			else if ( FStrEq( pcmd, "cl_orderhold" ) )
			{
				if ( args.ArgC() < 1 )
				{
					Warning( "Player sent bad cl_orderhold command\n" );
				}

				OrderNearbyMarines(this, ASW_ORDER_HOLD_POSITION);

				return true;
			}
			else if ( FStrEq( pcmd, "cl_mread" ) )			// telling the server you've read an info message
			{
				if ( args.ArgC() < 2 )
				{
					Warning( "Player sent bad cl_mread command\n" );
				}

				int iMessageEntIndex = atoi( args[1] );
				CASW_Info_Message *pMessage = dynamic_cast<CASW_Info_Message*>(CBaseEntity::Instance(iMessageEntIndex));
				if (pMessage)
				{
					pMessage->OnMessageRead(this);
				}

				return true;

			}
			else if ( FStrEq( pcmd, "cl_emote" ) )			// activating special speech+icon emote
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_emote command\n");
					return false;
				}
				int iEmote = atoi( args[1] );
				CASW_Marine* pMarine = GetMarine();
				if (pMarine && pMarine->GetHealth() > 0)
				{
					pMarine->DoEmote(iEmote);
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_chatter" ) )			// activate a specific chatter
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_chatter command\n");
					return false;
				}
				int iChatter = atoi( args[1] );
				int iSubChatter = -1;
				if (args.ArgC() == 3)
				{
					iSubChatter = atoi( args[2] );
				}
				CASW_Marine* pMarine = GetMarine();
				if (pMarine && pMarine->GetMarineSpeech())
				{
					pMarine->GetMarineSpeech()->ClientRequestChatter(iChatter, iSubChatter);
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_freecam") )		// player is telling server where his freecam is
			{
				if ( args.ArgC() < 4 )
				{
					Warning("Player sent a bad cl_freecam command\n");
					return false;
				}

				m_bUsedFreeCam = true;
				m_vecFreeCamOrigin.x = atoi( args[1] );
				m_vecFreeCamOrigin.y = atoi( args[2] );
				m_vecFreeCamOrigin.z = atoi( args[3] );

				return true;
			}
			else if ( FStrEq( pcmd, "cl_selecthack") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_selecthack command\n");
					return false;
				}

				int iHackOption = atoi(args[1]);
				if (GetMarine() && GetMarine()->m_hCurrentHack.Get())
				{
					CASW_Hack *pHack = dynamic_cast<CASW_Hack*>(GetMarine()->m_hCurrentHack.Get());
					if (pHack)
					{
						pHack->SelectHackOption(iHackOption);				
						return true;
					}
				}
				return false;
			}
			else if ( FStrEq( pcmd, "cl_stopusing") )
			{
				if (GetMarine() && GetMarine()->m_hUsingEntity.Get())
					GetMarine()->StopUsing();
				return true;
			}
			else if ( FStrEq( pcmd, "cl_blipspeech") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_blipspeech command\n");
					return false;
				}
				int iTargetMarine = atoi( args[1] );
				if (ASWGameRules())
					ASWGameRules()->BlipSpeech(iTargetMarine);
				return true;
			}
			else if ( FStrEq( pcmd, "cl_viewmail") )
			{
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_viewmail command\n");
					return false;
				}		
				if (GetMarine() && GetMarine()->m_hUsingEntity.Get())
				{
					CASW_Computer_Area *pComputer = dynamic_cast<CASW_Computer_Area*>(GetMarine()->m_hUsingEntity.Get());
					if (pComputer)
					{
						int iMail = clamp(atoi(args[1]), 0, 3);
						pComputer->OnViewMail(GetMarine(), iMail);
					}			
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_offhand") )
			{			
				if ( args.ArgC() < 2 )
				{
					Warning("Player sent a bad cl_offhand command\n");
					return false;
				}
				int slot = clamp(atoi(args[1]), 0, 2);
				CASW_Marine* pMarine = GetMarine();
				if (pMarine && pMarine->GetHealth()>0 && !(pMarine->GetFlags() & FL_FROZEN))
				{

					// check we have an item in that slot
					CASW_Weapon* pWeapon = pMarine->GetASWWeapon(slot);
					if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->m_bOffhandActivate)				
					{				
						pWeapon->OffhandActivate();				
					}
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_ai_offhand") )
			{			
				if ( args.ArgC() < 3 )
				{
					Warning("Player sent a bad cl_ai_offhand command\n");
					return false;
				}
				int slot = clamp( atoi( args[1] ), 0, 2 );
				int marine_index = atoi( args[2] );
				CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( CBaseEntity::Instance( marine_index ) );
				if ( pMarine && pMarine->GetCommander() == this )
				{
					pMarine->OrderUseOffhandItem( slot, GetCrosshairTracePos() );
				}
				return true;
			}

			break;
		}

		case ASW_GS_DEBRIEF:
		{
			if ( FStrEq( pcmd, "cl_wants_continue" ) )			// print a message telling the other players that you want to start
			{
				if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() && ASWGameRules())
				{
					if (ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
					{
						if (!m_bPrintedWantsContinueMessage)
						{
							UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_wants_continue", GetPlayerName() );
							m_bPrintedWantsContinueMessage = true;
						}
					}
				}
				return true;
			}
			else if ( FStrEq( pcmd, "cl_wants_returnmap" ) )			// print a message telling the other players that you want to start
			{
				if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() && ASWGameRules())
				{
					if (ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
					{
						if (!m_bPrintedWantsContinueMessage)
						{
							UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_wants_returnmap", GetPlayerName() );
							m_bPrintedWantsContinueMessage = true;
						}
					}
				}
				return true;
			}

			break;
		}
	}
	
	if ( FStrEq( pcmd, "cl_wants_restart" ) )			// print a message telling the other players that you want to start
	{
		if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() && ASWGameRules())
		{
			if (ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
			{
				if (!m_bPrintedWantsContinueMessage)
				{
					UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_wants_restart", GetPlayerName() );
					m_bPrintedWantsContinueMessage = true;
				}
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "cl_autoreload" ) )			// telling the server you've read an info message
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad cl_autoreload command\n" );
		}

		m_bAutoReload = (atoi(args[1]) == 1);
		return true;

	}
	else if ( FStrEq( pcmd, "cl_switchm" ) )			// selecting a marine from the roster
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad switch marine command\n" );
		}

		int iMarineIndex = atoi( args[1] ) - 1;
		SwitchMarine(iMarineIndex);

		return true;

	}
	else if ( FStrEq( pcmd, "cl_mapline") )		// player is drawing a line on the minimap
	{
		if ( args.ArgC() < 4 )
		{
			Warning("Player sent a bad cl_mapline command\n");
			return false;
		}
		int linetype = clamp(atoi( args[1] ), 0, 1);
		int world_x = atoi( args[2] );
		int world_y = atoi( args[3] );

		// todo: throttle messages here
		// send user messages to all other clients
		if (ASWGameRules())
			ASWGameRules()->BroadcastMapLine(this, linetype, world_x, world_y);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_campaignnext") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_campaignnext command\n");
			return false;
		}
		// make sure we're leader
		if ( ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex() )
		{
			int iTargetMission = atoi( args[1] );
			CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();
			if ( pCampaign && pCampaign->GetMission( iTargetMission ) )
			{
				ASWGameResource()->m_iNextCampaignMission = iTargetMission;
			}
// 			if (ASWGameRules() && ASWGameRules()->GetCampaignSave())
// 			{
// 				ASWGameRules()->GetCampaignSave()->PlayerVote(this, iTargetMission);
// 			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "cl_skill") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_skill command\n");
			return false;
		}
		if ( ASWGameRules() )
		{
			ASWGameRules()->RequestSkill( this, atoi(args[1]) );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "cl_forcelaunch") )
	{
		// make sure we're leader
		if (ASWGameResource() && ASWGameResource()->m_iLeaderIndex == entindex())
		{
			if (ASWGameRules() && ASWGameRules()->GetCampaignSave())
			{
				ASWGameRules()->RequestCampaignMove( ASWGameResource()->m_iNextCampaignMission.Get() );
				//ASWGameRules()->GetCampaignSave()->ForceNextMissionLaunch();
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "cl_campaignsas") )
	{
		if (ASWGameRules() && ASWGameRules()->GetGameState() >= ASW_GS_DEBRIEF
			&& ASWGameResource() && ASWGameResource()->GetLeader() == this)
		{
			ASWGameRules()->CampaignSaveAndShowCampaignMap(this, false);
		}
		return true;
	}
	else if ( FStrEq( pcmd, "cl_forceready") )
	{
		if ( args.ArgC() < 2 || !ASWGameResource() || ASWGameResource()->GetLeader() != this || !ASWGameRules())
		{
			Warning("Player sent a bad cl_forceready command\n");
			return false;
		}

		int iReadyType = atoi(args[1]);		
		if (iReadyType == ASW_FR_BRIEFING && !ASWGameResource()->AtLeastOneMarine())	// don't allow force ready start in briefing if no marines are selected
			return false;

		ASWGameRules()->SetForceReady(iReadyType);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_restart_mission") )
	{
		// check we're leader and request the restart
		if (ASWGameResource() && ASWGameResource()->GetLeader() == this && ASWGameRules())
			ASWGameRules()->RestartMission(this);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_ready") )
	{	
		if ( args.ArgC() < 1 )
		{
			Warning("Player sent a bad cl_ready command\n");
			return false;
		}

		if (!ASWGameResource() || !ASWGameRules())
			return false;

		// only allow readiness in briefing and debrief
		if (ASWGameRules()->GetGameState() != ASW_GS_BRIEFING && ASWGameRules()->GetGameState() != ASW_GS_DEBRIEF)
			return false;

		int iPlayerIndex = entindex() - 1;

		// player index is out of range
		if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
			return false;

		// mark us as ready or not
		bool bReady = ASWGameResource()->m_bPlayerReady[iPlayerIndex];
		// toggle our ready status
		ASWGameResource()->m_bPlayerReady.Set(iPlayerIndex, !bReady);

		
		ASWGameRules()->SetMaxMarines();

		// if we have no marines, that means we want to be a spectator
		if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && ASWGameResource()->GetNumMarines(this) <= 0)
			m_bRequestedSpectator = true;
		
		return true;
	}
	else if ( FStrEq( pcmd, "cl_spectating") )
	{	
		if ( args.ArgC() < 1 )
		{
			Warning("Player sent a bad cl_spectating command\n");
			return false;
		}
		Msg("cl_spectating get game resource=%d\n", ASWGameResource());
		Msg(" gamerules = %d\n", ASWGameRules());
		Msg(" this is leader = %d\n", ASWGameResource() && ASWGameResource()->GetLeader() == this);

		// remember that this guy wants to spectate (so we can try not to make him leader, ready him up after leaving leader, etc)
		m_bRequestedSpectator = true;

		if (!ASWGameResource() || !ASWGameRules())
		{
			Msg("  cl_spectating returning false as we don't have game resource or gamerules\n");
			return false;
		}

		int iPlayerIndex = entindex() - 1;
		Msg(" playerindex = %d\n", iPlayerIndex);

		// player index is out of range
		if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
		{
			Warning("Spectating player index out of range!");
			Msg("  cl_spectating returning false as it's out of range\n");
			return false;
		}

		Msg(" gamestate = %d (briefing=%d debrief=%d map=%d\n", ASWGameRules()->GetGameState(), ASW_GS_BRIEFING, ASW_GS_DEBRIEF, ASW_GS_CAMPAIGNMAP);

		if (ASWGameResource()->GetLeader() == this)		// leader can't be made ready here (but spectator will be made autoready when he's removed from being leader, later)
		{
			Msg("Spectator is leader, so we're deferring auto ready until he stops.  returning true\n");
			if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING
				|| ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
			{
				ASWGameRules()->SetMaxMarines();
			}
			return true;
		}
		
		// flag us as ready in the briefing/debrief
		if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING
			|| ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
		{
			Msg(" Setting player %d ready\n", iPlayerIndex);
			ASWGameResource()->m_bPlayerReady.Set(iPlayerIndex, true);

			ASWGameRules()->SetMaxMarines();
		}
		else if (ASWGameRules()->GetGameState() == ASW_GS_CAMPAIGNMAP && ASWGameRules()->GetCampaignSave())
		{
			Msg(" telling campaign save that we're spectating\n", iPlayerIndex);
			ASWGameRules()->GetCampaignSave()->PlayerSpectating(this);
		}

		Msg("  cl_spectating returning true\n");		
		return true;
	}
	else if ( FStrEq( pcmd, "cl_leadervote") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_leadervote command\n");
			return false;
		}
		int iTargetPlayer = clamp(atoi(args[1]), -1, gpGlobals->maxClients);
		ASWGameRules()->SetLeaderVote(this, iTargetPlayer);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_kickvote") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_kickvote command\n");
			return false;
		}
		int iTargetPlayer = clamp(atoi(args[1]), -1, gpGlobals->maxClients);
		ASWGameRules()->SetKickVote(this, iTargetPlayer);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_vmaplist") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_vmaplist command\n");
			return false;
		}
		int nMissionOffset = atoi(args[1]);
		if (nMissionOffset < 0)
			nMissionOffset = 0;
		VoteMissionList(nMissionOffset, ASW_MISSIONS_PER_PAGE);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_vcampmaplist") )
	{
		if ( args.ArgC() < 3 )
		{
			Warning("Player sent a bad cl_vcampmaplist command\n");
			return false;
		}
		int nCampaignIndex = atoi(args[1]);
		if (nCampaignIndex < 0)
			nCampaignIndex = 0;
		int nMissionOffset = atoi(args[2]);
		if (nMissionOffset < 0)
			nMissionOffset = 0;
		VoteCampaignMissionList(nCampaignIndex, nMissionOffset, ASW_MISSIONS_PER_PAGE);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_vcamplist") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_vcamplist command\n");
			return false;
		}
		int nCampaignOffset = atoi(args[1]);
		if (nCampaignOffset < 0)
			nCampaignOffset = 0;
		VoteCampaignList(nCampaignOffset, ASW_CAMPAIGNS_PER_PAGE);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_vsaveslist") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_vsaveslist command\n");
			return false;
		}
		int nSaveOffset = atoi(args[1]);
		if (nSaveOffset < 0)
			nSaveOffset = 0;
		VoteSavedCampaignList(nSaveOffset, ASW_SAVES_PER_PAGE);
		return true;
	}
	else if ( FStrEq( pcmd, "asw_vote_saved_campaign") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad asw_vote_saved_campaign command\n");
			return false;
		}		
		
		if (ASWGameRules())
			ASWGameRules()->StartVote(this, ASW_VOTE_SAVED_CAMPAIGN, args[1]);
		return true;
	}
	else if ( FStrEq( pcmd, "asw_vote_campaign") )
	{
		if ( args.ArgC() < 3 )
		{
			Warning("Player sent a bad asw_vote_campaign command\n");
			return false;
		}	
		int nCampaignIndex = atoi( args[1] );
		if (ASWGameRules())
			ASWGameRules()->StartVote(this, ASW_VOTE_CHANGE_MISSION, args[2], nCampaignIndex );
		return true;
	}
	else if ( FStrEq( pcmd, "asw_vote_mission") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad asw_vote_mission command\n");
			return false;
		}		
		if (ASWGameRules())
			ASWGameRules()->StartVote(this, ASW_VOTE_CHANGE_MISSION, args[1]);
		return true;
	}
	else if ( FStrEq( pcmd, "vote_yes") )
	{
		if (ASWGameRules())
			ASWGameRules()->CastVote(this, true);
		return true;
	}
	else if ( FStrEq( pcmd, "vote_no") )
	{
		if (ASWGameRules())
			ASWGameRules()->CastVote(this, false);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_ccounts") )
	{
		if ( args.ArgC() < 4 )
		{
			Warning("Player sent a bad cl_ccounts command\n");
			return false;
		}
		m_iClientMissionsCompleted = atoi(args[1]);
		m_iClientCampaignsCompleted = atoi(args[2]);
		m_iClientKills = atoi(args[3]);
		return true;
	}
	else if ( FStrEq( pcmd, "cl_skip_intro") )
	{
		if (ASWGameRules())
		{
			if (ASWGameRules()->IsIntroMap())
			{
				CASW_Intro_Control* pIntro = dynamic_cast<CASW_Intro_Control*>(gEntList.FindEntityByClassname( NULL, "asw_intro_control" ));
				if (pIntro)
				{
					pIntro->LaunchCampaignMap();
				}
			}
			else if (ASWGameRules()->IsOutroMap())
			{
				CASW_Intro_Control* pIntro = dynamic_cast<CASW_Intro_Control*>(gEntList.FindEntityByClassname( NULL, "asw_intro_control" ));
				if (pIntro)
				{
					pIntro->CheckReconnect();
				}
			}
		}
		return true;
	}	
	else if ( FStrEq( pcmd, "cl_fullyjoined") )
	{
		if (!m_bSentJoinedMessage)
		{
			if (gpGlobals->maxClients > 1)
			{
				//char text[256];
				//Q_snprintf( text,sizeof(text), "%s has joined the game.\n", GetPlayerName() );
				//UTIL_ClientPrintAll( HUD_PRINTTALK, text );
				IGameEvent * event = gameeventmanager->CreateEvent( "player_fullyjoined" );
				if ( event )
				{
					event->SetInt("userid", GetUserID() );
					event->SetString( "name", GetPlayerName() );

					gameeventmanager->FireEvent( event );
				}

				// if we're meant to be leader then make it so
				if (ASWGameResource() && !Q_strcmp(GetASWNetworkID(), ASWGameResource()->GetLastLeaderNetworkID()) && ASWGameResource()->GetLeader() != this)
				{
					ASWGameResource()->SetLeader(this);
					UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_made_leader", GetPlayerName());
					Msg("Network ID=%s LastLeaderNetworkID=%s\n", GetASWNetworkID(), ASWGameResource()->GetLastLeaderNetworkID());
				}

				UTIL_RestartAmbientSounds();
			}
			m_bSentJoinedMessage = true;
		}
		// check for getting back any marines we lost
		bool bReturnedMarines = false;
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if ( pGameResource )
		{
			const char *szNetworkID = GetASWNetworkID();
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource* pMR = pGameResource->GetMarineResource( i );
				if ( !pMR )
					continue;
				CASW_Marine *pMarine = pMR->GetMarineEntity();
				
				if ( pMarine && !Q_strcmp( pMarine->m_szInitialCommanderNetworkID, szNetworkID ) )
				{
					bool bWasInhabited = pMarine->IsInhabited();
					CASW_Player *pTempCommander = pMarine->GetCommander();
					if ( bWasInhabited && pTempCommander )
					{
						// if another player is currently controlling one of our old marines, have him switch out
						//  this will likely be confusing for them!  need some message?
						pTempCommander->LeaveMarines();
					}
					Msg("Fully joined, marine %d previous ID = %s my ID = %s\n", i, pMarine->m_szInitialCommanderNetworkID, szNetworkID );
					pMarine->SetCommander( this );
					pMR->SetCommander( this );
					bReturnedMarines = true;
					
					// if another player was controlling one of our marines, make sure he switches to one of his own if he can
					if ( bWasInhabited && pTempCommander )
					{
						pTempCommander->SwitchMarine(0);
					}
				}
			}
		}
		if ( bReturnedMarines )
		{
			Msg(" Fully joined player switching to marine 0\n");
			SwitchMarine(0);
		}
		ASWGameRules()->OnPlayerFullyJoined( this );
		return true;
	}
	else if ( FStrEq( pcmd, "cl_gen_progress") )
	{
		if ( args.ArgC() < 2 )
		{
			Warning("Player sent a bad cl_gen_progress command\n");
			return false;
		}
		m_fMapGenerationProgress = clamp(atof(args[1]), 0.0f, 1.0f);
		return true;
	}	
	
	return BaseClass::ClientCommand( args );
}

void CASW_Player::BecomeNonSolid()
{
	m_afPhysicsFlags |= PFLAG_OBSERVER;

	SetGroundEntity( (CBaseEntity *)NULL );

    AddSolidFlags( FSOLID_NOT_SOLID );
	RemoveFlag( FL_AIMTARGET ); // don't attract autoaim
	AddFlag( FL_DONTTOUCH );	// stop it touching anything
	AddFlag( FL_NOTARGET );	// stop NPCs noticing it

	SetMoveType( MOVETYPE_NOCLIP );

	return;
}

void CASW_Player::OnMarineCommanded( const CASW_Marine *pMarine )
{
	if ( !ASWGameResource() )
	{
		return;
	}

	int nNumMarines = 0;
	int nNewMarine = 0;

	const int max_marines = ASWGameResource()->GetMaxMarineResources();
	for ( int i = 0; i < max_marines; i++ )
	{		
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource( i );
		if ( pMR )
		{
			if ( pMR->m_Commander.Get() == this)
			{
				if ( pMR->GetMarineEntity() == pMarine )
				{
					nNewMarine = nNumMarines;
				}

				nNumMarines++;
			}
		}
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_commanding" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "new_marine", pMarine->entindex() );
		event->SetInt( "new_index", nNewMarine );
		event->SetInt( "count", nNumMarines );
		gameeventmanager->FireEvent( event );
	}
}

void CASW_Player::SetMarine( CASW_Marine *pMarine )
{
	if ( pMarine && pMarine != GetMarine() )
	{
		if ( !ASWGameResource() )
		{
			return;
		}

		int nNumMarines = 0;
		int nOldMarine = 0;

		const int max_marines = ASWGameResource()->GetMaxMarineResources();
		for ( int i = 0; i < max_marines; ++i )
		{		
			CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( pMR )
			{
				if ( pMR->m_Commander.Get() == this )
				{
					if ( pMR->GetMarineEntity() == GetMarine() )
					{
						nOldMarine = nNumMarines;
					}

					nNumMarines++;
				}
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "marine_selected" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetInt( "new_marine", pMarine->entindex() );
			event->SetInt( "old_marine", ( GetMarine() ? GetMarine()->entindex() : -1 ) );
			event->SetInt( "old_index", nOldMarine );
			event->SetInt( "count", nNumMarines );
			gameeventmanager->FireEvent( event );
		}

		m_hMarine = pMarine;
		// make sure our list of usable entities is refreshed
		FindUseEntities();
	}
}

CASW_Marine* CASW_Player::GetMarine()
{
	return m_hMarine.Get();
}

CASW_Marine* CASW_Player::GetMarine() const
{
	return m_hMarine.Get();
}

void CASW_Player::SpectateNextMarine()
{
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	CASW_Marine *pFirst = NULL;
	//Msg("CASW_Player::SpectateNextMarine\n");

	if (GetSpectatingMarine() && GetSpectatingMarine()->GetHealth() <= 0)
	{
		//Msg("clearing initial spectating marine as he's dead\n");
		SetSpectatingMarine(NULL);
	}
	// loop through all marines
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		//Msg("Checking pMR %d\n", i);
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;
		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if (!pMarine || !pMarine->IsAlive() || pMarine->GetHealth() <= 0)
		{
			//Msg(" but he's dead\n");
			continue;
		}
		if (!pFirst)
		{
			pFirst = pMarine;
			//Msg("  set this guy as our first\n");
		}
		if (GetSpectatingMarine() == NULL)		// if we're not spectating anything yet, then spectate the first one we find
		{
			//Msg("  We're not spectating anyone, so we're gonna spec this dude\n");
			SetSpectatingMarine(pMarine);
			break;
		}
		if (GetSpectatingMarine() == pMarine)	// if we're spectating this one, then clear it, so the next one we find will get set
		{
			//Msg("  we're spectating this dude, so clearing our current spectator\n");
			SetSpectatingMarine(NULL);
		}				
	}
	//Msg("end\n");
	// if we're still not spectating anything but we found at least marine, then that means we were spectating the last one in the list and need to set this
	if (GetSpectatingMarine() == NULL && pFirst)
	{
		//Msg("  but we're still not speccing anyone and we have a first set, so speccing that dude\n");
		SetSpectatingMarine(pFirst);
	}
}

void CASW_Player::SetSpectatingMarine(CASW_Marine *pMarine)
{
	if (pMarine)
	{
		//Msg("Starting spectating marine %s\n", pMarine->GetEntityName());
		m_hSpectatingMarine = pMarine;
	}
	else
	{
		//Msg("Clearing spectating marine\n");
		m_hSpectatingMarine = NULL;
	}
}

CASW_Marine* CASW_Player::GetSpectatingMarine()
{
	return m_hSpectatingMarine.Get();
}

void CASW_Player::SelectNextMarine(bool bReverse)
{
	// find index of current marine and our total number of live marines
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	Msg("CASW_Player::SelectNextMarine reverse=%d\n", bReverse);

	int iMarines = 0;
	int iCurrent = -1;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetCommander() == this)
		{
			iMarines++;
			if (pMR->GetMarineEntity() && pMR->GetMarineEntity() == GetMarine())
				iCurrent = iMarines;
		}
	}
	Msg("Marines = %d Current = %d\n", iMarines, iCurrent);
	// if we don't have any of our marines selected (maybe we died), then put us in at the start of the list
	if (iCurrent == -1 && iMarines > 1)
		iCurrent = 1;
	if (iCurrent != -1 && iMarines > 1)
	{
		int iTarget = iCurrent + (bReverse ? -1 : 1);
		if (iTarget <= 0)
			iTarget = iMarines;
		if (iTarget > iMarines)
			iTarget = 1;

		int k = 6;
		while (!CanSwitchToMarine(iTarget-1) && k >= 0)
		{
			iTarget += (bReverse ? -1 : 1);
			if (iTarget <= 0)
				iTarget = iMarines;
			if (iTarget > iMarines)
				iTarget = 1;
			k--;
		}		
		Msg(" wrapped to %d and sent as \n", iTarget, iTarget-1);
		SwitchMarine(iTarget-1);
		// todo: this currently doesn't work properly if we have dead marines! (it'll try to select a dead one and just end up doing nothing)
		//&& pMR->GetHealthPercent() > 0 
	}
}

bool CASW_Player::CanSwitchToMarine(int num)
{
	if (!ASWGameResource())
		return false;
	int max_marines = ASWGameResource()->GetMaxMarineResources();
	for (int i=0;i<max_marines;i++)
	{		
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR)
		{
			if ((CASW_Player*) pMR->m_Commander == this)
			{
				num--;
				if (num < 0 && pMR->GetMarineEntity() )
				{
					// abort if we're trying to switch to a dead marine
					if (pMR->GetMarineEntity()->GetHealth() <= 0)
					{
						return false;
					}
					return true;
				}
			}
		}
	}
	return false;
}

// select the nth marine in the marine info list owned by this player
void CASW_Player::SwitchMarine(int num)
{	
	if (!ASWGameResource())
		return;
	int max_marines = ASWGameResource()->GetMaxMarineResources();
	for (int i=0;i<max_marines;i++)
	{		
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR)
		{
			if ((CASW_Player*) pMR->m_Commander == this)
			{
				num--;
				if (num < 0 && pMR->GetMarineEntity() )
				{
					CASW_Marine *pOldMarine = GetMarine();
					CASW_Marine *pNewMarine = pMR->GetMarineEntity();

					// abort if we're trying to switch to a dead marine
					if (pNewMarine->GetHealth() <= 0)
					{
						return;
					}

					if ( pOldMarine )
					{
						if ( pNewMarine == pOldMarine )
							return;
						pOldMarine->UninhabitedBy(this);
					}

					if (asw_rts_controls.GetBool())
					{
						Msg("RTS controls moving above marine %d\n", num);
						Msg("Marine is at: %f, %f, %f\n", pMR->GetMarineEntity()->GetAbsOrigin().x, pMR->GetMarineEntity()->GetAbsOrigin().y, pMR->GetMarineEntity()->GetAbsOrigin().z);
						Vector vecNewOrigin = pMR->GetMarineEntity()->GetAbsOrigin() + Vector(0, -200, 400);
						SetAbsOrigin( vecNewOrigin );
						Msg("Moved cam to: %f, %f, %f\n", vecNewOrigin.x, vecNewOrigin.y, vecNewOrigin.z);
						return;
					}

					m_ASWLocal.m_hAutoAimTarget.Set(NULL);

					SetMarine( pNewMarine );
					SetSpectatingMarine(NULL);
					pNewMarine->SetCommander(this);
					pNewMarine->InhabitedBy(this);

					if ( gpGlobals->curtime > ASWGameRules()->m_fMissionStartedTime + 5.0f )
					{
						// If it's not the very beginning of the level... go ahead and say it
						pNewMarine->GetMarineSpeech()->Chatter(CHATTER_SELECTION);
					}

					CASW_SquadFormation *pSquad = pNewMarine->GetSquadFormation();
					if ( pSquad )
					{
						pSquad->ChangeLeader( pNewMarine, false );
					}

					if ( pOldMarine )
					{
						if ( pOldMarine->GetASWOrders() == ASW_ORDER_HOLD_POSITION )
						{
							pOldMarine->OrdersFromPlayer( this, ASW_ORDER_HOLD_POSITION, pNewMarine, false, GetLocalAngles().y );
						}
						else
						{
							pOldMarine->OrdersFromPlayer( this, ASW_ORDER_FOLLOW, pNewMarine, false );
						}
					}

					if ( !m_bFirstInhabit )
					{
						//OrderNearbyMarines( this, ASW_ORDER_FOLLOW );
						m_bFirstInhabit = true;
					}
					return;
				}
			}
		}
	}
	// if we got here, it means we pushed a marine number greater than the number of marines we have
	// check again, this time counting up other player's marines, to see if we're trying to shout out to them (or trying to spectate them, if we're all dead)
	CASW_Marine *pMarine = GetMarine();
	bool bSpectating = (!pMarine || pMarine->GetHealth() <= 0) && !HasLiveMarines();
	for (int i=0;i<max_marines;i++)
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR)
		{
			if ((CASW_Player*) pMR->m_Commander != this)
			{
				num--;
				if (num < 0)
				{		
					// do a chatter call to this marine
					CASW_Marine_Profile *pProfile = pMR->GetProfile();
					if (pProfile)
					{
						if ( bSpectating && pMR->GetMarineEntity() )
						{
							SetSpectatingMarine( pMR->GetMarineEntity() );
							return;
						}
						else if ( pMarine )
						{
							if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_sarge"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_SARGE);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_jaeger"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_JAEGER);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_wildcat"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_WILDCAT);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_wolfe"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_WOLFE);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_faith"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_FAITH);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_bastille"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_BASTILLE);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_crash"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_CRASH);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_flynn"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_FLYNN);
							else if (!Q_stricmp(pProfile->m_ShortName, "#asw_name_vegas"))
								pMarine->GetMarineSpeech()->Chatter(CHATTER_VEGAS);
							return;
						}
					}
				}
			}
		}
	}
	
}

void CASW_Player::OrderMarineFace(int iMarine, float fYaw, Vector &vecOrderPos)
{
	//Msg("Ordering marine %d ", iMarine);
	// check if we were passed an ent index of the marine we're ordering
	CASW_Marine *pTarget = (iMarine == -1) ? NULL : dynamic_cast<CASW_Marine*>(CBaseEntity::Instance(iMarine));

	CASW_Marine *pMyMarine = GetMarine();
		if (!pMyMarine)
			return;

	// if we don't have a specific marine to order, find the best one
	if (!pTarget)
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if (!pGameResource)
			return;
		
		// if we didn't specify a marine, we'll order the one nearest to the order pos

		// check if we preselected a specific marine to order
		pTarget = m_hOrderingMarine.Get();

		// find the nearest marine
		if (!pTarget)
		{
			float nearest_dist = 9999;
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
				if (!pMR)
					continue;

				CASW_Marine* pMarine = pMR->GetMarineEntity();
				if (!pMarine || pMarine == pMyMarine || pMarine->GetHealth() <= 0		// skip if dead
						|| pMarine->GetCommander() != this)
					continue;
			
				float distance = vecOrderPos.DistTo(pMarine->GetAbsOrigin());
				if (pMarine->GetASWOrders() != ASW_ORDER_FOLLOW)		// bias against marines that are already holding position somewhere
					distance += 5000;
				if (distance < nearest_dist)
				{
					nearest_dist = distance;
					pTarget = pMarine;
				}
			}
		}
	}

	// do an emote
	//pMyMarine->DoEmote(3);	// stop

	if (!pTarget)
		return;

	pTarget->OrdersFromPlayer(this, ASW_ORDER_MOVE_TO, GetMarine(), true, fYaw, &vecOrderPos);
}

// makes the player uninhabit marines and become free in control of his
//   player entity as normal (this is just for debugging)
void CASW_Player::LeaveMarines()
{
	if (GetMarine())
	{
		GetMarine()->UninhabitedBy(this);
	}
	m_hMarine = NULL;	
}

void CASW_Player::ChangeName( const char *pszNewName )
{
	// make sure name is not too long
	char trimmedName[ASW_MAX_PLAYER_NAME_LENGTH];
	Q_strncpy( trimmedName, pszNewName, sizeof( trimmedName ) );

	const char *pszOldName = GetPlayerName();

	//char text[256];
	//Q_snprintf( text,sizeof(text), "%s changed name (CASW_Player::ChangeName) to %s\n", pszOldName, trimmedName );
	//UTIL_ClientPrintAll( HUD_PRINTTALK, text );

	// broadcast event
	IGameEvent * event = gameeventmanager->CreateEvent( "player_changename" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetString( "oldname", pszOldName );
		event->SetString( "newname", trimmedName );
		gameeventmanager->FireEvent( event );
	}

	// change shared player name
	SetPlayerName( trimmedName );

	// tell engine to use new name
	engine->ClientCommand( edict(), "name \"%s\"", trimmedName );
}

#define ASW_NO_SERVERSIDE_AUTOAIM

Vector CASW_Player::GetAutoaimVectorForMarine(CASW_Marine* marine, float flDelta, float flNearMissDelta)
{
#ifdef ASW_NO_SERVERSIDE_AUTOAIM
	// test of no serverside autoaim
	Vector	forward;
	AngleVectors( EyeAngles(), &forward );	//  + m_Local.m_vecPunchAngle
	return	forward;
#else
	//if ( ( ShouldAutoaim() == false ) || ( flDelta == 0 ) )	
	if (GetMarine() == NULL)
	{
		Vector	forward;
		AngleVectors( EyeAngles(), &forward );	//  + m_Local.m_vecPunchAngle
		//Msg("Not autoaiming\n");
		return	forward;	
	}

	Vector vecSrc	= GetMarine()->Weapon_ShootPosition( );
	float flDist	= MAX_COORD_RANGE;

	QAngle angles = MarineAutoaimDeflection( vecSrc, flDist, flDelta, flNearMissDelta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = false;

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	Vector	forward;
	AngleVectors( EyeAngles() + angles, &forward );	// + m_Local.m_vecPunchAngle

	m_angMarineAutoAim = angles;
	
	return forward;
#endif
}

QAngle CASW_Player::MarineAutoaimDeflection( Vector &vecSrc, float flDist, float flDelta, float flNearMissDelta )
{
	float		bestdot;
	Vector		bestdir;
	CBaseEntity	*bestent;
	trace_t		tr;
	Vector		v_forward, v_right, v_up;

	if (ASWGameRules() && ASWGameRules()->IsHardcoreMode())
		return vec3_angle;

	//if ( ShouldAutoaim() == false )
	//{
		//m_fOnTarget = false;
		//return vec3_angle;
	//}

	AngleVectors( EyeAngles() + m_angMarineAutoAim, &v_forward, &v_right, &v_up );  //  + m_Local.m_vecPunchAngle

	// try all possible entities
	bestdir = v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	// near misses
	Vector nearmissdir = v_forward;
	float nearmissdot = flNearMissDelta;
	CBaseEntity *nearmissent = NULL;

	//Reset this data
	m_fOnTarget			= false;
	CASW_Weapon* pWeapon = GetMarine() ? GetMarine()->GetActiveASWWeapon() : NULL;
	bool bDoAutoaimEnt = pWeapon && pWeapon->GetAutoAimAmount() >= 0.25f;	// only the prifle + autogun show up their autoaim target

	if (bDoAutoaimEnt || m_ASWLocal.m_hAutoAimTarget.Get())
		m_ASWLocal.m_hAutoAimTarget.Set(NULL);
	//Msg("marine aa def clearing aa ent\n");

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, MASK_SHOT, GetMarine(), COLLISION_GROUP_NONE, &tr );

	CBaseEntity *pEntHit = tr.m_pEnt;
	CBaseEntity* pFlareEnt = NULL;

	if ( pEntHit && pEntHit->m_takedamage != DAMAGE_NO)
	{
		//m_hAutoAimTarget = pEntHit;

		// don't look through water
		if (!((GetWaterLevel() != 3 && pEntHit->GetWaterLevel() == 3) || (GetWaterLevel() == 3 && pEntHit->GetWaterLevel() == 0)))
		{
			if ( pEntHit->GetFlags() & FL_AIMTARGET )
			{
				m_fOnTarget = true;
				if (bDoAutoaimEnt)
					m_ASWLocal.m_hAutoAimTarget.Set(pEntHit);
			}

			//Already on target, don't autoaim
			//Msg("Already on target (%s), not autoaiming\n", pEntHit->GetClassname());
			
			return vec3_angle;
		}
	}

	int count = AimTarget_ListCount();
	//Msg("checking autoaim over %d targets\n", count);
	if ( count )
	{
		CBaseEntity **pList = (CBaseEntity **)stackalloc( sizeof(CBaseEntity *) * count );
		AimTarget_ListCopy( pList, count );

		for ( int i = 0; i < count; i++ )
		{
			Vector center, center_flat;
			Vector dir, dir_flat;
			float dot, dot_flat;
			CBaseEntity *pEntity = pList[i];

			//Msg("Checking autoaim vs %d:%s", pEntity->entindex, pEntity->GetClassname());

			// Don't shoot yourself
			if ( pEntity == this || pEntity == GetMarine())
			{
				//Msg("this is you!, skipping\n");
				continue;
			}

			if (!pEntity->IsAlive() || !pEntity->edict() )
			{
				//Msg("not alive or not an edict, skipping\n");
				continue;
			}
	
			// don't autoaim onto marines
			if (pEntity->Classify() == CLASS_ASW_MARINE)
				continue;

			//if ( !g_pGameRules->ShouldAutoAim( this, pEntity->edict() ) )
				//continue;

			// don't look through water
			if ((GetWaterLevel() != 3 && pEntity->GetWaterLevel() == 3) || (GetWaterLevel() == 3 && pEntity->GetWaterLevel() == 0))
			{
				//Msg("not looking through water, skipping\n");
				continue;
			}

			// Only shoot enemies!
			//if ( IRelationType( pEntity ) != D_HT )
			//{
				//if ( !pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
					// Msg( "friend\n");
					//continue;
			//}

			center = pEntity->BodyTarget( vecSrc );
			center_flat = center;
			center_flat.z = vecSrc.z;

			dir = (center - vecSrc);
			VectorNormalize( dir );

			dir_flat = (center_flat - vecSrc);
			VectorNormalize( dir_flat );

			// make sure it's in front of the player
			if (DotProduct (dir, v_forward ) < 0)
			{
				//Msg("not in front of you, skipping\n");
				continue;
			}

			dot = fabs( DotProduct (dir, v_right ) ) 
				+ fabs( DotProduct (dir, v_up ) ) * 0.5;

			dot_flat = fabs( DotProduct (dir_flat, v_right ) ) 
				+ fabs( DotProduct (dir_flat, v_up ) ) * 0.5;

			// tweak for distance
			dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);
			dot_flat *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

			// asw: we're only using the flat dot here
			//  should really fix this to give priority to aliens that are nearer your z, instead of just nearer on the x and y

			// asw temp lose the z autoaim
			dot_flat = dot;

			// check for 'near misses' that change the Z aim only
			if (dot_flat < nearmissdot)
			{
				// todo: check if we have LOS trace?
				nearmissdot = dot_flat;
				nearmissent = pEntity;
				nearmissdir = dir;
			}
			
			// check for actual autoaim
			if (dot_flat > bestdot)
			{
				//Msg("too far to turn, skipping\n");
				if (bestent==NULL && dot_flat <= ASW_FLARE_AUTOAIM_DOT
					&& ASWGameRules()->CanFlareAutoaimAt(GetMarine(), pEntity))
				{
					pFlareEnt = pEntity;
					// allow autoaim at this entity because it's inside a flare radius
					if (asw_debug_marine_damage.GetBool())
						Msg("Flare autoaiming!");
				}
				else
				{
					continue;	// too far to turn
				}
			}

			UTIL_TraceLine( vecSrc, center, MASK_SHOT, GetMarine(), COLLISION_GROUP_NONE, &tr );

			if (tr.fraction != 1.0 && tr.m_pEnt != pEntity )
			{
				//Msg( "hit %s, can't see %s, skipping\n", STRING( tr.u.ent->classname ), STRING( pEdict->classname ) );
				//Msg("Can't see, skipping\n");
				continue;
			}

			// can shoot at this one
			bestdot = dot_flat;
			bestent = pEntity;
			bestdir = dir;
			//Msg("can see, storing!\n");
		}
		if ( bestent )
		{
			if (asw_DebugAutoAim.GetBool())
			{
				Msg("Autoaiming at a %s dot=%f\n", bestent->GetClassname(), bestdot);
				NDebugOverlay::EntityBounds(bestent, 255,0,0, 255, 1.0f);
			}
			QAngle bestang;
			VectorAngles( bestdir, bestang );

			bestang -= EyeAngles();	 // - m_Local.m_vecPunchAngle

			if (bDoAutoaimEnt || (pFlareEnt == bestent))
				m_ASWLocal.m_hAutoAimTarget.Set(bestent);
			//Msg("marine aa def setting aa ent to %d\n", bestent->entindex());

			m_fOnTarget = true;

			return bestang;
		}
		/*
		else if (nearmissent)
		{
			if (asw_DebugAutoAim.GetBool())
			{
				Msg("Nearmiss aiming at a %s dot=%f\n", nearmissent->GetClassname(), nearmissdot);
				NDebugOverlay::EntityBounds(nearmissent, 0,255,0, 255, 1.0f);
			}
			QAngle bestang;
			Vector verticalonlydir = v_forward;
			verticalonlydir.z = nearmissdir.z;
			VectorAngles( verticalonlydir, bestang );

			bestang -= EyeAngles();	 //  - m_Local.m_vecPunchAngle

			//m_hAutoAimTarget = bestent;
			//m_fOnTarget = true;
			return bestang;
		}
		*/
	}

	return QAngle( 0, 0, 0 );
}

void CASW_Player::FlashlightTurnOn()
{
	/*
	if (GetMarine() && !(GetMarine()->GetFlags() & FL_FROZEN))	// don't allow this if the marine is frozen
		GetMarine()->FlashlightToggle();
	else
		BaseClass::FlashlightTurnOn();
	*/
}

void CASW_Player::FlashlightTurnOff()
{
	/*if (GetMarine())
		GetMarine()->FlashlightTurnOff();
	else
		BaseClass::FlashlightTurnOff();
		*/
}

void OrderNearestHoldingMarineToFollow()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());;
	CASW_Marine *pMyMarine = pPlayer->GetMarine();
	if (pPlayer && pMyMarine)
	{
		if (pMyMarine->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
			return;

		CASW_Game_Resource *pGameResource = ASWGameResource();
		if (!pGameResource)
			return;
		
		// check if we preselected a specific marine to order
		CASW_Marine *pTarget = pPlayer->m_hOrderingMarine.Get();

		// find the nearest holding marine
		if (!pTarget)
		{
			float nearest_dist = 9999;
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
				if (!pMR)
					continue;

				CASW_Marine* pMarine = pMR->GetMarineEntity();
				if (!pMarine || pMarine == pMyMarine || pMarine->GetHealth() <= 0		// skip if dead
						|| pMarine->GetASWOrders() == ASW_ORDER_FOLLOW	// skip if already following
						|| pMarine->GetCommander() != pPlayer)
					continue;
			
				float distance = pMyMarine->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin());
				if (pMarine->GetASWOrders() != ASW_ORDER_HOLD_POSITION)		// bias against marines that are already moving somewhere
					distance += 5000;
				if (distance < nearest_dist)
				{
					nearest_dist = distance;
					pTarget = pMarine;
				}
			}
		}

		// do an emote
		pMyMarine->DoEmote(4);	// stop


		// abort if we couldn't find another marine to order
		if (!pTarget)
			return;

		pTarget->OrdersFromPlayer(pPlayer, ASW_ORDER_FOLLOW, pPlayer->GetMarine(), true);			
	}
}

void OrderNearbyMarines(CASW_Player *pPlayer, ASW_Orders NewOrders, bool bAcknowledge)
{
	if ( !pPlayer )
		return;

	CASW_Marine *pMyMarine = pPlayer->GetMarine();
	if ( pPlayer && pMyMarine )
	{
		if ( pMyMarine->GetFlags() & FL_FROZEN )	// don't allow this if the marine is frozen
			return;

		// go through all marines and tell them to follow our marine
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if ( !pGameResource )
			return;

		// do an emote
		if ( NewOrders == ASW_ORDER_HOLD_POSITION && bAcknowledge )
		{
			pMyMarine->DoEmote( 3 );	// go
		}

		else if ( NewOrders == ASW_ORDER_FOLLOW && bAcknowledge )
		{
			pMyMarine->DoEmote( 4 );	// stop
		}

		// first count how many marines are in range
		int iNearby = 0;
		for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;

			CASW_Marine* pMarine = pMR->GetMarineEntity();
			if ( !pMarine || pMarine == pMyMarine || pMarine->GetCommander() != pPlayer )
				continue;
		
			float distance = pMyMarine->GetAbsOrigin().DistTo( pMarine->GetAbsOrigin() );
			if ( distance < MARINE_ORDER_DISTANCE )
			{
				iNearby++;
			}
		}

		// pick one to chatter
		int iChatter = random->RandomInt( 0, iNearby - 1 ) + 1;

		// give the orders
		for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource* pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;

			CASW_Marine* pMarine = pMR->GetMarineEntity();
			if ( !pMarine || pMarine == pMyMarine || pMarine->GetHealth() <= 0 || pMarine->GetCommander() != pPlayer )
				continue;
		
			float distance = pMyMarine->GetAbsOrigin().DistTo( pMarine->GetAbsOrigin() );
			if ( distance < MARINE_ORDER_DISTANCE )
			{
				iChatter--;
				pMarine->OrdersFromPlayer( pPlayer, NewOrders, pPlayer->GetMarine(), bAcknowledge && iChatter == 0 );
			}
		}

		if ( iNearby >= 1 )
		{
			if ( NewOrders == ASW_ORDER_FOLLOW )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_command_follow" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
			else if ( NewOrders == ASW_ORDER_HOLD_POSITION )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_command_hold" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
}

void CASW_Player::ShowInfoMessage(CASW_Info_Message* pMessage)
{
	if (!pMessage)
		return;

	m_pCurrentInfoMessage = pMessage;
	m_fClearInfoMessageTime = gpGlobals->curtime + 1.0f;
}


void CASW_Player::MoveMarineToPredictedPosition()
{
	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;

	m_vecStoredPosition = pMarine->GetAbsOrigin();

	// sweep a bounding box ahead in the current velocity direction
	Vector vel = pMarine->GetAbsVelocity();
	INetChannelInfo *nci = engine->GetPlayerNetInfo( entindex() ); 
	if (!nci)
		return;
	float fVelScale = nci->GetLatency( FLOW_OUTGOING );
	Vector dest = m_vecStoredPosition + vel * fVelScale;

	Ray_t ray;
	trace_t pm;
	ray.Init( m_vecStoredPosition, dest, pMarine->CollisionProp()->OBBMins(), pMarine->CollisionProp()->OBBMaxs() );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, pMarine, ASW_COLLISION_GROUP_MARINE_POSITION_PREDICTION, &pm );
	
	dest = m_vecStoredPosition + vel * fVelScale * pm.fraction;
	pMarine->SetAbsOrigin(dest);
}

void CASW_Player::RestoreMarinePosition()
{
	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;

	pMarine->SetAbsOrigin(m_vecStoredPosition);
}

void CASW_Player::ActivateUseIcon( int iUseEntityIndex, int nHoldType )
{	
	// no using when you're dead
	if (!GetMarine() || GetMarine()->GetHealth()<=0)
		return;

	if (GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
			return;

	CBaseEntity *pEnt = CBaseEntity::Instance(iUseEntityIndex);
	if (!pEnt)
	{
		return;
	}

	// check this item is in our usable entities list
	bool bFound = false;
	for (int i=0;i<m_iUseEntities;i++)
	{
		if (pEnt == m_hUseEntities[i].Get())
		{
			bFound = true;
			break;
		}
	}
	if (!bFound)
		return;

	IASW_Server_Usable_Entity *pUsable = dynamic_cast<IASW_Server_Usable_Entity*>(pEnt);
	if (!pUsable)
		return;

	pUsable->ActivateUseIcon( GetMarine(), nHoldType );
}

void CASW_Player::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	if (asw_debug_pvs.GetBool())
	{
		Msg("Player:%d SetupVis\n", entindex());
	}
	if (m_bUsedFreeCam)
	{
		if (asw_debug_pvs.GetBool())
		{
			Msg("  freecam %s\n", VecToString(m_vecFreeCamOrigin));
		}
		engine->AddOriginToPVS(m_vecFreeCamOrigin);
	}
	CASW_Marine *pMarine = GetSpectatingMarine();
	bool bSpectating = true;
	if (!pMarine)
	{
		pMarine = GetMarine();
		bSpectating = false;
	}
	if (pMarine)
	{
		// asw - add the marine as our PVS position (since we're using radius based, this will do the job)
		if (pMarine->IsInVehicle())
		{
	#ifdef CLIENT_DLL
			if (pMarine->GetClientsideVehicle() && pMarine->GetClientsideVehicle()->GetEntity())
				engine->AddOriginToPVS(pMarine->GetClientsideVehicle()->GetEntity()->GetAbsOrigin());
	#else
			if (pMarine->GetASWVehicle() && pMarine->GetASWVehicle()->GetEntity())
				engine->AddOriginToPVS(pMarine->GetASWVehicle()->GetEntity()->GetAbsOrigin());
	#endif
		}
		else
		{
			//if (asw_debug_pvs.GetBool()) Msg(" Marine %f,%f,%f\n", pMarine->GetAbsOrigin().x, pMarine->GetAbsOrigin().y, pMarine->GetAbsOrigin().z);
			if (asw_debug_pvs.GetBool())
			{
				const Vector pos = pMarine->GetAbsOrigin();
				Msg("  Marine %f,%f,%f\n", pos.x, pos.y, pos.z);
			}
			engine->AddOriginToPVS(pMarine->GetAbsOrigin());
		}

		// Check for mapper cameras
		CPointCamera *pMapperCamera = GetPointCameraList();
		bool bMapperCam = false;
		for ( int cameraNum = 0; pMapperCamera != NULL; pMapperCamera = pMapperCamera->m_pNext )
		{
			if ( pMapperCamera->IsActive() && !ASW_IsSecurityCam( pMapperCamera ) )
			{
				engine->AddOriginToPVS( pMapperCamera->GetAbsOrigin() );
				bMapperCam = true;
				break;
			}

			++cameraNum;
		}

		if (pMarine->m_hUsingEntity.Get())
		{
			CASW_Computer_Area *pComputer = dynamic_cast<CASW_Computer_Area*>(pMarine->m_hUsingEntity.Get());
			if (pComputer)
			{
				if (pComputer->m_iActiveCam == 1 && pComputer->m_hSecurityCam1.Get())
				{
					// if we're here, a computer camera is active

					// check if any mapper set cameras are active, we shouldn't be on if they are
					CPointCamera *pCam = dynamic_cast<CPointCamera*>( pComputer->m_hSecurityCam1.Get() );
					Assert( pCam );

					if ( bMapperCam )
					{
						pCam->SetActive(false);
					}
					else
					{
						engine->AddOriginToPVS( pCam->GetAbsOrigin() );
						pCam->SetActive(true);
						CASW_PointCamera *pASWCam = dynamic_cast<CASW_PointCamera*>(pCam);
						if ( pASWCam )
						{
							pASWCam->m_bSecurityCam = true;
						}
					}
				}
			}
			// todo: check which is the cam being used and only activate that one
			//  turn off activation when we stop using?
		}
		if (pMarine->IsControllingTurret())
		{
			engine->AddOriginToPVS(pMarine->GetRemoteTurret()->GetAbsOrigin());
		}
	}
	else
	{
		//if (asw_debug_pvs.GetBool()) Msg(" Base\n");
		BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );
	}
}

#define ASW_PUSHAWAY_THINK_CONTEXT	"CSPushawayThink"

void CASW_Player::PushawayThink()
{
	if (GetMarine() && !GetMarine()->IsInVehicle())
	{
		// Push physics props out of our way.
		PerformObstaclePushaway( GetMarine() );
	}
	SetNextThink( gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, ASW_PUSHAWAY_THINK_CONTEXT );
}

void CASW_Player::RagdollBlendTest()
{
	if (!m_pBlendRagdoll)
		return;

	m_fBlendAmount += gpGlobals->frametime * 0.25f;
	while (m_fBlendAmount > 1.0f)
		m_fBlendAmount -= 1.0f;

	// sin wave it in and out
	//float fRealBlend = sin(m_fBlendAmount * 6.284);
	float fRealBlend = m_fBlendAmount * 2;
	if (fRealBlend > 1.0f)
		fRealBlend = 2.0f - fRealBlend;
	m_pBlendRagdoll->SetBlendWeight(abs(fRealBlend) * asw_blend_test_scale.GetFloat());
}

void CASW_Player::VoteMissionList(int nMissionOffset, int iNumSlots)
{
	if (!m_hVotingMissions.Get())
	{
		CASW_Voting_Missions *pVoting = (CASW_Voting_Missions*) CreateEntityByName("asw_voting_missions");
		if (!pVoting)
			return;		
		pVoting->Spawn();		
		m_hVotingMissions = pVoting;
	}
	m_hVotingMissions->SetListType(this, 1, nMissionOffset, iNumSlots);		// 1 = missions	
}

void CASW_Player::VoteCampaignMissionList(int nCampaignIndex, int nMissionOffset, int iNumSlots)
{
	if (!m_hVotingMissions.Get())
	{
		CASW_Voting_Missions *pVoting = (CASW_Voting_Missions*) CreateEntityByName("asw_voting_missions");
		if (!pVoting)
			return;		
		pVoting->Spawn();		
		m_hVotingMissions = pVoting;
	}
	m_hVotingMissions->SetListType( this, 1, nMissionOffset, iNumSlots, nCampaignIndex );		// 1 = missions	
}

void CASW_Player::VoteCampaignList(int nCampaignOffset, int iNumSlots)
{
	if (!m_hVotingMissions.Get())
	{
		CASW_Voting_Missions *pVoting = (CASW_Voting_Missions*) CreateEntityByName("asw_voting_missions");
		if (!pVoting)
			return;		
		pVoting->Spawn();		
		m_hVotingMissions = pVoting;
	}
	m_hVotingMissions->SetListType(this, 2, nCampaignOffset, iNumSlots);		// 2 = campaigns	
}

void CASW_Player::VoteSavedCampaignList(int nSaveOffset, int iNumSlots)
{
	if (!m_hVotingMissions.Get())
	{
		CASW_Voting_Missions *pVoting = (CASW_Voting_Missions*) CreateEntityByName("asw_voting_missions");
		if (!pVoting)
			return;		
		pVoting->Spawn();		
		m_hVotingMissions = pVoting;
	}
	m_hVotingMissions->SetListType(this, 3, nSaveOffset, iNumSlots);		// 3 = saved campaigns		
}

const char* CASW_Player::GetASWNetworkID()
{
	const char *pszNetworkID = GetNetworkIDString();	
	if (!Q_strcmp(pszNetworkID, "UNKNOWN") || !Q_strcmp(pszNetworkID, "HLTV") ||
		!Q_strcmp(pszNetworkID, "STEAM_ID_LAN") || !Q_strcmp(pszNetworkID, "STEAM_ID_PENDING") || 
		!Q_strcmp(pszNetworkID, "STEAM_1:0:0"))
	{
		// player has no valid steam ID, so let's use his IP address instead		
		INetChannelInfo *nci = engine->GetPlayerNetInfo( entindex() );
		if (nci)
		{
			pszNetworkID = nci->GetAddress();
		}
		else
		{
			pszNetworkID = GetPlayerName();
		}
	}
	// chop off the steam ID part...
	if (!Q_strncmp("STEAM_ID", pszNetworkID, 8))
	{
		pszNetworkID+=8;
	}
	return pszNetworkID;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity in front of the player, preferring
//			collidable entities, but allows selection of enities that are
//			on the other side of walls or objects
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* CASW_Player::FindPickerEntity()
{
	MDLCACHE_CRITICAL_SECTION();

	trace_t tr;
	UTIL_TraceLine( GetCrosshairTracePos(),
		GetCrosshairTracePos() + Vector( 0, 0, 10 ),
		MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 && tr.DidHitNonWorldEntity() )
	{
		return tr.m_pEnt;
	}
	
	// If trace fails, look for the nearest entity
	CBaseEntity *pNearestEntity = NULL;
	float fNearestDistance = -1;
	CBaseEntity *pCurrentEntity = gEntList.FirstEnt();
	while ( pCurrentEntity )
	{
		if ( !pCurrentEntity->IsWorld() )
		{
			float fDistance = pCurrentEntity->WorldSpaceCenter().DistTo( GetCrosshairTracePos() );
			if ( fNearestDistance == -1 || fDistance < fNearestDistance )
			{
				pNearestEntity = pCurrentEntity;
				fNearestDistance = fDistance;
			}
		}
		pCurrentEntity = gEntList.NextEnt( pCurrentEntity );
	}
	return pNearestEntity;
}

