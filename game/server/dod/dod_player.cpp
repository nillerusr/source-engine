//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "team.h"			//GetGlobalTeam
#include "in_buttons.h"
#include "dod_shareddefs.h"
#include "dod_cvars.h"
#include "weapon_dodbase.h"
#include "weapon_dodbasegrenade.h"
#include "weapon_dodbaserpg.h"
#include "weapon_dodbipodgun.h"
#include "dod_basegrenade.h"
#include "dod_ammo_box.h"
#include "effect_dispatch_data.h"
#include "movehelper_server.h"
#include "tier0/vprof.h"
#include "te_effect_dispatch.h"
#include "vphysics/player_controller.h"
#include <KeyValues.h>
#include "engine/IEngineSound.h"
#include "studio.h"
#include "dod_viewmodel.h"
#include "info_camera_link.h"
#include "dod_team.h"
#include "dod_control_point.h"
#include "dod_location.h"
#include "viewport_panel_names.h"
#include "obstacle_pushaway.h"
#include "datacache/imdlcache.h"
#include "bone_setup.h"
#include "dod_baserocket.h"
#include "dod_basegrenade.h"
#include "dod_bombtarget.h"
#include "collisionutils.h"
#include "gamestats.h"
#include "gameinterface.h"
#include "holiday_gift.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

ConVar sv_max_usercmd_future_ticks( "sv_max_usercmd_future_ticks", "8", 0, "Prevents clients from running usercmds too far in the future. Prevents speed hacks." );
ConVar sv_motd_unload_on_dismissal( "sv_motd_unload_on_dismissal", "0", 0, "If enabled, the MOTD contents will be unloaded when the player closes the MOTD." );

EHANDLE g_pLastAlliesSpawn;
EHANDLE g_pLastAxisSpawn;

int g_iLastAlliesSpawnIndex = 0;
int g_iLastAxisSpawnIndex = 0;

ConVar dod_playerstatetransitions( "dod_playerstatetransitions", "-2", FCVAR_CHEAT, "dod_playerstatetransitions <ent index or -1 for all>. Show player state transitions." );
ConVar dod_debugdamage( "dod_debugdamage", "0", FCVAR_CHEAT );

ConVar mp_bandage_heal_amount( "mp_bandage_heal_amount",
							  "40",
							  FCVAR_GAMEDLL,
							  "How much health to give after a successful bandage\n",
							  true, 0, false, 0 );

ConVar dod_freezecam( "dod_freezecam", "1", FCVAR_REPLICATED | FCVAR_ARCHIVE, "show players a freezecam shot of their killer on death" );

extern ConVar spec_freeze_time;
extern ConVar spec_freeze_traveltime;

#define DOD_PUSHAWAY_THINK_CONTEXT	"DODPushawayThink"
#define DOD_DEAFEN_CONTEXT	"DeafenContext"

int g_iCurrentLifeID = 0;

int GetNextLifeID( void )
{
	return ++g_iCurrentLifeID; 
}

// -------------------------------------------------------------------------------- //
// Classes
// -------------------------------------------------------------------------------- //

class CPhysicsPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:
	int ShouldMoveTo( IPhysicsObject *pObject, const Vector &position )
	{
		CDODPlayer *pPlayer = (CDODPlayer *)pObject->GetGameData();
		if ( pPlayer )
		{
			if ( pPlayer->TouchedPhysics() )
			{
				return 0;
			}
		}
		return 1;
	}
};

static CPhysicsPlayerCallback playerCallback;

// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CDODRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS( CDODRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle 
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( dod_ragdoll, CDODRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CDODRagdoll, DT_DODRagdoll )
	SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
	SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ) )
END_SEND_TABLE()


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
	SendPropInt( SENDINFO( m_nData ), 32 )
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

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( player, CDODPlayer );
PRECACHE_REGISTER(player);

// CDODPlayerShared Data Tables
//=============================

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CDODPlayerShared, DT_DODSharedLocalPlayerExclusive )
	SendPropInt( SENDINFO( m_iPlayerClass), 4 ),
	SendPropInt( SENDINFO( m_iDesiredPlayerClass ), 4 ),
	SendPropFloat( SENDINFO( m_flDeployedYawLimitLeft ) ),
	SendPropFloat( SENDINFO( m_flDeployedYawLimitRight ) ),
	SendPropInt( SENDINFO( m_iCPIndex ), 4 ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominated ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominated ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominatingMe ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominatingMe ) ) ),
END_SEND_TABLE()

// main table
BEGIN_SEND_TABLE_NOBASE( CDODPlayerShared, DT_DODPlayerShared )
	SendPropFloat( SENDINFO( m_flStamina ), 0, SPROP_NOSCALE | SPROP_CHANGES_OFTEN ),
	SendPropTime( SENDINFO( m_flSlowedUntilTime ) ),
	SendPropBool( SENDINFO( m_bProne ) ),
	SendPropBool( SENDINFO( m_bIsSprinting ) ),
	SendPropTime( SENDINFO( m_flGoProneTime ) ),
	SendPropTime( SENDINFO( m_flUnProneTime ) ),
	SendPropTime( SENDINFO( m_flDeployChangeTime ) ),
	SendPropTime( SENDINFO( m_flDeployedHeight ) ),
	SendPropBool( SENDINFO( m_bPlanting ) ),
	SendPropBool( SENDINFO( m_bDefusing ) ),
	SendPropDataTable( "dodsharedlocaldata", 0, &REFERENCE_SEND_TABLE(DT_DODSharedLocalPlayerExclusive), SendProxy_SendLocalDataTable ),
END_SEND_TABLE()


// CDODPlayer Data Tables
//=========================
extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CDODPlayer, DT_DODLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropFloat( SENDINFO(m_flStunDuration), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flStunMaxAlpha), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iProgressBarDuration ), 4, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flProgressBarStartTime ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CDODPlayer, DT_DODNonLocalPlayerExclusive )
	// send a lo-res origin to other players
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_COORD|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
END_SEND_TABLE()
		
// main table
IMPLEMENT_SERVERCLASS_ST( CDODPlayer, DT_DODPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// dod_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	// We need to send a hi-res origin to the local player to avoid prediction errors sliding along walls
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),

	// Data that only gets sent to the local player.
	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_DODPlayerShared ) ),

	// Data that only gets sent to the local player.
	SendPropDataTable( "dodlocaldata", 0, &REFERENCE_SEND_TABLE(DT_DODLocalPlayerExclusive), SendProxy_SendLocalDataTable ),
	SendPropDataTable( "dodnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_DODNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable ),

	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 13, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 13, SPROP_CHANGES_OFTEN ),
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropBool( SENDINFO( m_bSpawnInterpCounter ) ),
	SendPropInt( SENDINFO( m_iAchievementAwardsMask ), NUM_ACHIEVEMENT_AWARDS, SPROP_UNSIGNED ),
END_SEND_TABLE()

BEGIN_DATADESC( CDODPlayer )
	DEFINE_THINKFUNC( PushawayThink ),
	DEFINE_THINKFUNC( DeafenThink )
END_DATADESC()

// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT );

bool PlayerStatLessFunc( const int &lhs, const int &rhs )	
{ 
	return lhs < rhs; 
}

// -------------------------------------------------------------------------------- //
// CDODPlayer implementation.
// -------------------------------------------------------------------------------- //
CDODPlayer::CDODPlayer()
#if !defined(NO_STEAM)
:	m_CallbackGSStatsReceived( this, &CDODPlayer::OnGSStatsReceived )
#endif
{
	m_PlayerAnimState = CreatePlayerAnimState( this );
	m_Shared.Init( this );

	UseClientSideAnimation();
	m_angEyeAngles.Init();

	SetViewOffset( DOD_PLAYER_VIEW_OFFSET );
	
	m_pCurStateInfo = NULL;	// no state yet
	m_lifeState = LIFE_DEAD; // Start "dead".

	m_Shared.SetPlayerClass( PLAYERCLASS_UNDEFINED );

	m_bTeamChanged = false;

	// Clear the signals
	m_signals.Update();
	m_fHandleSignalsTime = 0.0f;

	// autokick
	m_flLastMovement = gpGlobals->curtime;

	m_flNextVoice = gpGlobals->curtime;
	m_flNextHandSignal = gpGlobals->curtime;

	m_flNextTimeCheck = gpGlobals->curtime;

	m_bAutoReload = true;

	// Init our hint system
	Hints()->Init( this, NUM_HINTS, g_pszHintMessages );
	Hints()->RegisterHintTimer( HINT_USE_MELEE, 60 );
	Hints()->RegisterHintTimer( HINT_USE_ZOOM, 30 );
	Hints()->RegisterHintTimer( HINT_USE_IRON_SIGHTS, 60 );
	Hints()->RegisterHintTimer( HINT_USE_SEMI_AUTO, 60 );
	Hints()->RegisterHintTimer( HINT_USE_SPRINT, 120 );
	Hints()->RegisterHintTimer( HINT_USE_DEPLOY, 30 );
	Hints()->RegisterHintTimer( HINT_USE_PRIME, 2 );
	
	memset( &m_WeaponStats, 0, sizeof(weaponstat_t) );

	m_KilledPlayers.SetLessFunc( PlayerStatLessFunc );
	m_KilledByPlayers.SetLessFunc( PlayerStatLessFunc );

	m_iNumAreaDefenses = 0;
	m_iNumAreaCaptures = 0;
	m_iNumBonusRoundKills = 0;

	memset( &m_flTimePlayedPerClass_Allies, 0, sizeof(m_flTimePlayedPerClass_Allies) );
	memset( &m_flTimePlayedPerClass_Axis, 0, sizeof(m_flTimePlayedPerClass_Axis) );
	m_flLastClassChangeTime = gpGlobals->curtime;

	Q_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );

	m_iAchievementAwardsMask = 0;

	m_hLastDroppedWeapon = NULL;
	m_hLastDroppedAmmoBox = NULL;

	m_flTimeAsClassAccumulator = 0;
}


CDODPlayer::~CDODPlayer()
{
	m_PlayerAnimState->Release();
}


CDODPlayer *CDODPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CDODPlayer::s_PlayerEdict = ed;
	return (CDODPlayer*)CreateEntityByName( className );
}

void CDODPlayer::PrecachePlayerModel( const char *szPlayerModel )
{
	PrecacheModel( szPlayerModel );

	// Strange numbers, but gotten from the actual max bounds of our
	// player models
	const static Vector mins( -13.9, -30.1, -12.1 );
	const static Vector maxs( 30.9, 30.1, 73.1 );

// Moved to pure_server_minimal.txt
//	engine->ForceModelBounds( szPlayerModel, mins, maxs );
}

void CDODPlayer::Precache()
{
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_RIFLEMAN );	
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_ASSAULT );
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_SUPPORT );
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_SNIPER );
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_MG );
	PrecachePlayerModel( DOD_PLAYERMODEL_AXIS_ROCKET );

	PrecachePlayerModel( DOD_PLAYERMODEL_US_RIFLEMAN );
	PrecachePlayerModel( DOD_PLAYERMODEL_US_ASSAULT );
	PrecachePlayerModel( DOD_PLAYERMODEL_US_SUPPORT );
	PrecachePlayerModel( DOD_PLAYERMODEL_US_SNIPER );
	PrecachePlayerModel( DOD_PLAYERMODEL_US_MG );
	PrecachePlayerModel( DOD_PLAYERMODEL_US_ROCKET );

/// Moved to pure_server_minimal.txt
//	engine->ForceSimpleMaterial( "materials/models/player/american/american_body.vmt" );
//	engine->ForceSimpleMaterial( "materials/models/player/american/american_gear.vmt" );
//	engine->ForceSimpleMaterial( "materials/models/player/german/german_body.vmt" );
//	engine->ForceSimpleMaterial( "materials/models/player/german/german_gear.vmt" );

	UTIL_PrecacheOther( "dod_ammo_box" );

	PrecacheScriptSound( "Player.FlashlightOn" );
	PrecacheScriptSound( "Player.FlashlightOff" );

	PrecacheScriptSound( "Player.FreezeCam" );
	PrecacheScriptSound( "Camera.SnapShot" );
	PrecacheScriptSound( "Achievement.Earned" );
	PrecacheScriptSound( "Game.Revenge" );
	PrecacheScriptSound( "Game.Domination" );
	PrecacheScriptSound( "Game.Nemesis" );

	PrecacheModel ( "sprites/glow01.vmt" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: Allow pre-frame adjustments on the player
//-----------------------------------------------------------------------------
ConVar sv_runcmds( "sv_runcmds", "1" );
void CDODPlayer::PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	VPROF( "CDODPlayer::PlayerRunCommand" );

	if ( !sv_runcmds.GetInt() )
		return;

	// don't run commands in the future
	if ( !IsEngineThreaded() && 
		( ucmd->tick_count > (gpGlobals->tickcount + sv_max_usercmd_future_ticks.GetInt()) ) )
	{
		DevMsg( "Client cmd out of sync (delta %i).\n", ucmd->tick_count - gpGlobals->tickcount );
		return;
	}

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}


//-----------------------------------------------------------------------------
// Purpose: Simulates a single frame of movement for a player
//-----------------------------------------------------------------------------
void CDODPlayer::RunPlayerMove( const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime )
{
	CUserCmd cmd;

	// Store off the globals.. they're gonna get whacked
	float flOldFrametime = gpGlobals->frametime;
	float flOldCurtime = gpGlobals->curtime;

	float flTimeBase = gpGlobals->curtime + gpGlobals->frametime - frametime;
	this->SetTimeBase( flTimeBase );

	CUserCmd lastUserCmd = *GetLastUserCommand();
	Q_memset( &cmd, 0, sizeof( cmd ) );
	MoveHelperServer()->SetHost( this );
	this->PlayerRunCommand( &cmd, MoveHelperServer() );
	this->SetLastUserCommand( cmd );

	// Clear out any fixangle that has been set
	this->pl.fixangle = FIXANGLE_NONE;

	// Restore the globals..
	gpGlobals->frametime = flOldFrametime;
	gpGlobals->curtime = flOldCurtime;
}


void CDODPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	State_Enter( STATE_WELCOME );
}

void CDODPlayer::Spawn()
{
	// Get rid of the progress bar...
	SetProgressBarTime( 0 );

	SetModel( DOD_PLAYERMODEL_US_RIFLEMAN );	//temporarily
	BaseClass::Spawn();
	
	AddFlag( FL_ONGROUND ); // set the player on the ground at the start of the round.

	m_Shared.SetStamina( 100 );
	m_bTeamChanged	= false;

	ClearCapAreaIndex();

	m_bHasGenericAmmo = true;

	m_bSlowedByHit = false;

	m_flIdleTime = gpGlobals->curtime + random->RandomFloat( 20, 30 );

	InitProne();
	InitSprinting();

	// update this counter, used to not interp players when they spawn
	m_bSpawnInterpCounter = !m_bSpawnInterpCounter;

	EmitSound( "Player.Spawn" );

	SetContextThink( &CDODPlayer::PushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, DOD_PUSHAWAY_THINK_CONTEXT );

	m_Shared.SetCPIndex( -1 );

	ResetBleeding();

	// Our own version of remove decals, as we can't pass entity messages to 
	// the target's base class.

	EntityMessageBegin( this );
		WRITE_BYTE( DOD_PLAYER_REMOVE_DECALS );
	MessageEnd();

	if ( m_Shared.PlayerClass() != PLAYERCLASS_UNDEFINED )
	{
		Hints()->StartHintTimer( HINT_USE_SPRINT );
	}

	// Get a unique "life id" used for tracking stats
	m_iLifeID = GetNextLifeID();

	SetDefusing( NULL );
	SetPlanting( NULL );

	m_iLastBlockAreaIndex = -1;
	m_iLastBlockCapAttempt = -1;

	if ( DODGameRules()->IsBombingTeam( GetTeamNumber() ) )
	{
		Hints()->HintMessage( HINT_BOMB_PLANT_MAP );
	}

	//CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );

	// Reset per-life achievement counts
	HandleHeadshotAchievement( 0 );
	HandleDeployedMGKillCount( 0 );	
	HandleEnemyWeaponsAchievement( 0 );
	ResetComboWeaponKill();

	RecalculateAchievementAwardsMask();

	// If we're spawning as a non-player team, flush the stats
	int iTeam = GetTeamNumber();
	if ( iTeam != TEAM_ALLIES && iTeam != TEAM_AXIS )
	{
		StatEvent_UploadStats();
	}

	m_StatProperty.SetClassAndTeamForThisLife( m_Shared.PlayerClass(), GetTeamNumber() );
}

void CDODPlayer::PlayerDeathThink()
{
	//overridden, do nothing
}

void CDODPlayer::CreateRagdollEntity()
{
	// If we already have a ragdoll, don't make another one.
	CDODRagdoll *pRagdoll = dynamic_cast< CDODRagdoll* >( m_hRagdoll.Get() );
	
	if( pRagdoll )
	{
		// MATTTODO: put ragdolls in a queue to disappear
		// for now remove the old one ..
		UTIL_Remove( pRagdoll );
		pRagdoll = NULL;
	}

	if ( !pRagdoll )
	{
		// and create a new one
		pRagdoll = dynamic_cast< CDODRagdoll* >( CreateEntityByName( "dod_ragdoll" ) );
	}

	if ( pRagdoll )
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

// Called when a player is disconnecting
void CDODPlayer::DestroyRagdoll( void )
{
	CDODRagdoll *pRagdoll = dynamic_cast< CDODRagdoll* >( m_hRagdoll.Get() );
	
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
	}
}

void CDODPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	// allow bots to react
//	TheBots->OnEvent( EVENT_PLAYER_DIED, this, info.GetAttacker() );

	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CDODPlayer *pScorer = ToDODPlayer( DODGameRules()->GetDeathScorer( pKiller, pInflictor ) );

	// Do this before we drop the victim's weapons to check the streak
	if ( GetDeployedKillStreak() >= ACHIEVEMENT_MG_STREAK_IS_DOMINATING )
	{
		if ( pScorer && pScorer->GetTeamNumber() != GetTeamNumber() && DODGameRules()->State_Get() == STATE_RND_RUNNING )
		{
			pScorer->AwardAchievement( ACHIEVEMENT_DOD_KILL_DOMINATING_MG );
		}
	}

	// see if this was a long range kill
	// distance from scorer to inflictor > 1200
	if ( pScorer && GetTeamNumber() != pScorer->GetTeamNumber() && pInflictor != pScorer )
	{
		const char *killer_weapon_name = STRING( pInflictor->m_iClassname );

		if ( strncmp( killer_weapon_name, "rocket_", 7 ) == 0 )
		{
			float flDist = ( pScorer->GetAbsOrigin() - pInflictor->GetAbsOrigin() ).Length();

			if ( flDist > ACHIEVEMENT_LONG_RANGE_ROCKET_DIST && DODGameRules()->State_Get() == STATE_RND_RUNNING )
			{
				bool bIsSniperZoomed = false;

				CWeaponDODBase *pWeapon = GetActiveDODWeapon();
				if( pWeapon && ( pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_SNIPER ) )
				{
					CWeaponDODBaseGun *pGun = static_cast<CWeaponDODBaseGun *>( pWeapon );
					bIsSniperZoomed = pGun->IsSniperZoomed();
				}

				// victim must be deployed sniper or deployed mg
				if ( bIsSniperZoomed || m_Shared.IsInMGDeploy() )
				{
					pScorer->AwardAchievement( ACHIEVEMENT_DOD_LONG_RANGE_ROCKET );
				}
			}
		}		
	}

	DropPrimaryWeapon();

	if ( DODGameRules()->GetPresentDropChance() >= RandomFloat() )
	{
		Vector vForward, vRight, vUp;
		AngleVectors( EyeAngles(), &vForward, &vRight, &vUp );	

		CHolidayGift::Create( WorldSpaceCenter(), GetAbsAngles(), EyeAngles(), GetAbsVelocity(), this );
	}

	FlashlightTurnOff();

	// Just in case the progress bar is on screen, kill it.
	SetProgressBarTime( 0 );

	// turn off our cap area index
	HandleSignals();

	//if we have a primed grenade, drop it
	CWeaponDODBase *pWpn = GetActiveDODWeapon();

	if( pWpn && pWpn->GetDODWpnData().m_WeaponType == WPN_TYPE_GRENADE )
	{
		CWeaponDODBaseGrenade *pGrenade = dynamic_cast<CWeaponDODBaseGrenade *>(pWpn);

		if( pGrenade && pGrenade->IsArmed() )
		{
			pGrenade->DropGrenade();
		}
	}

	if ( pScorer && pScorer != this && pScorer->GetTeamNumber() != GetTeamNumber() )
	{
		if ( m_bIsPlanting && m_pPlantTarget )
		{
			CDODBombTarget *pTarget = m_pPlantTarget;

			Assert( pTarget );

			if ( pTarget )
                pTarget->PlantBlocked( pScorer );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_kill_planter" );
			if ( event )
			{
				event->SetInt( "userid", pScorer->GetUserID() );
				event->SetInt( "victimid", GetUserID() );

				gameeventmanager->FireEvent( event );
			}
		}

		if ( m_bIsDefusing && m_pDefuseTarget && pScorer->GetTeamNumber() != GetTeamNumber() )
		{
			// Re-enable if we want to give a defend point to someone who kills a defuser

			//CDODBombTarget *pTarget = m_pDefuseTarget;
			//pTarget->DefuseBlocked( pScorer );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_kill_defuser" );
			if ( event )
			{
				event->SetInt( "userid", pScorer->GetUserID() );
				event->SetInt( "victimid", GetUserID() );

				gameeventmanager->FireEvent( event );
			}
		}
	}

	SetDefusing( NULL );
	SetPlanting( NULL );

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	if( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		// set new target
		m_hObserverTarget.Set( info.GetAttacker() ); 

		// reset fov to default
		SetFOV( this, 0 );
	}
	else
		m_hObserverTarget.Set( NULL );

	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );
	
	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity();

	State_Transition( STATE_DEATH_ANIM );	// Transition into the dying state.
	BaseClass::Event_Killed( subinfo );

	OutputDamageGiven();
	OutputDamageTaken();
	ResetDamageCounters();

	// stop any voice command or pain sounds that we may be making
	CPASFilter filter( WorldSpaceCenter() );
	EmitSound( filter, entindex(), "Voice.StopSounds" );

	ResetBleeding();

	m_flStunDuration = 0.0f;
	m_flStunMaxAlpha = 0;

	// cancel deafen think
	SetContextThink( NULL, 0.0, DOD_DEAFEN_CONTEXT );

	// cancel deafen dsp
	CSingleUserRecipientFilter user( this );
	enginesound->SetPlayerDSP( user, 0, false );

	CDODPlayer *pAttacker = ToDODPlayer( info.GetAttacker() );

	if( pAttacker && pAttacker->IsPlayer() )
	{
		// find the weapon id of the weapon

		DODWeaponID weaponID = WEAPON_NONE;

		CBaseEntity *pInflictor = info.GetInflictor();

		if ( pInflictor == pAttacker )
		{
			CWeaponDODBase *pWpn = pAttacker->GetActiveDODWeapon();

			if ( pWpn )
			{
				if ( info.GetDamageCustom() & MELEE_DMG_SECONDARYATTACK )
					weaponID = pWpn->GetAltWeaponID();
				else
			        weaponID = pWpn->GetStatsWeaponID();
			}
		}
		else
		{
			Assert( pInflictor );

			const char *weaponName = STRING( pInflictor->m_iClassname );

			if ( Q_strncmp( weaponName, "rocket_", 7 ) == 0 )
			{
				CDODBaseRocket *pRocket = dynamic_cast< CDODBaseRocket *>( pInflictor );
				if ( pRocket )
				{
					weaponID = pRocket->GetEmitterWeaponID();
				}
			}
			else if ( Q_strncmp( weaponName, "grenade_", 8 ) == 0 )
			{
				CDODBaseGrenade *pGren = dynamic_cast< CDODBaseGrenade *>( pInflictor );
				if ( pGren )
				{
					weaponID = pGren->GetEmitterWeaponID();
				}
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_player_killed" );
		if ( event )
		{
			event->SetInt( "attacker", pAttacker->GetUserID() );
			event->SetInt( "victim", GetUserID() );
			event->SetInt( "weapon", weaponID );

			gameeventmanager->FireEvent( event );
		}

		if ( DODGameRules()->IsInBonusRound() )
		{
			pAttacker->Stats_BonusRoundKill();
		}
	}	

	m_bIsDefusing = false;
	m_bIsPlanting = false;

	if ( pScorer != this )
	{
		StatEvent_WasKilled();
	}

	StatEvent_UploadStats();
}

bool CDODPlayer::ShouldInstantRespawn( void )
{
	CUtlVector<EHANDLE> *pSpawnPoints = DODGameRules()->GetSpawnPointListForTeam( GetTeamNumber() );

	if ( !pSpawnPoints )
		return false;

	const float flMaxDistSqr = ( 500*500 );
	Vector vecOrigin = GetAbsOrigin();
	int i;
	int count = pSpawnPoints->Count();
	for ( i=0;i<count;i++ )
	{
		EHANDLE handle = pSpawnPoints->Element(i);
		if ( handle )
		{
			CSpawnPoint *point = dynamic_cast<CSpawnPoint *>( handle.Get() );
			if ( point && !point->IsDisabled() )
			{
				// range
				Vector vecDist = point->GetAbsOrigin() - vecOrigin;
				if ( vecDist.LengthSqr() > flMaxDistSqr )
				{
					continue;
				}

				// los
				if ( FVisible( point ) )
				{
					return true;
				}			
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayer::InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity )
{
	if ( 0 ) //if ( !sv_turbophysics.GetBool() )
	{
		BaseClass::InitVCollision( vecAbsOrigin, vecAbsVelocity );

		// Setup the HL2 specific callback.
		GetPhysicsController()->SetEventHandler( &playerCallback );
	}
}

void CDODPlayer::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
	if ( 0 ) //if ( !sv_turbophysics.GetBool() )
	{
		if ( !CanMove() )
			return;

		BaseClass::VPhysicsShadowUpdate( pPhysics );
	}
}

void CDODPlayer::PushawayThink()
{
	// Push physics props out of our way.
	PerformObstaclePushaway( this );
	SetNextThink( gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, DOD_PUSHAWAY_THINK_CONTEXT );
}

void CDODPlayer::CheatImpulseCommands( int iImpulse )
{
	switch( iImpulse )
	{
		case 101:
		{
			if( sv_cheats->GetBool() )
			{
				extern int gEvilImpulse101;
				gEvilImpulse101 = true;

				GiveAmmo( 1000, DOD_AMMO_COLT );			
				GiveAmmo( 1000, DOD_AMMO_P38 );		
				GiveAmmo( 1000, DOD_AMMO_C96 );
				GiveAmmo( 1000, DOD_AMMO_GARAND );			
				GiveAmmo( 1000, DOD_AMMO_K98 );			
				GiveAmmo( 1000, DOD_AMMO_M1CARBINE );	
				GiveAmmo( 1000, DOD_AMMO_SPRING );		
				GiveAmmo( 1000, DOD_AMMO_SUBMG );			
				GiveAmmo( 1000, DOD_AMMO_BAR );			
				GiveAmmo( 1000, DOD_AMMO_30CAL );			
				GiveAmmo( 1000, DOD_AMMO_MG42 );			
				GiveAmmo( 1000, DOD_AMMO_ROCKET );		

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

void CDODPlayer::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();
	PointCameraSetupVisibility( this, area, pvs, pvssize );
}

void CDODPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}

CBaseEntity	*CDODPlayer::GiveNamedItem( const char *pszName, int iSubType )
{
	EHANDLE pent;

	pent = CreateEntityByName(pszName);
	if ( pent == NULL )
	{
		Msg( "NULL Ent in GiveNamedItem!\n" );
		return NULL;
	}

	pent->SetLocalOrigin( GetLocalOrigin() );
	pent->AddSpawnFlags( SF_NORESPAWN );

	if ( iSubType )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( (CBaseEntity*)pent );
		if ( pWeapon )
		{
			pWeapon->SetSubType( iSubType );
		}
	}

	DispatchSpawn( pent );

	if ( pent != NULL && !(pent->IsMarkedForDeletion()) ) 
	{
		pent->Touch( this );
	}

	return pent;
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDODPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDODPlayer::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
		EmitSound( "Player.FlashlightOn" );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDODPlayer::FlashlightTurnOff( void )
{
	if( IsEffectActive(EF_DIMLIGHT) )
	{
		RemoveEffects( EF_DIMLIGHT );

		if( m_iHealth > 0 )
		{
			EmitSound( "Player.FlashlightOff" );
		}
	}	
}


void CDODPlayer::PostThink()
{
	BaseClass::PostThink();

	if( m_Shared.IsProne() )
	{
		SetCollisionBounds( VEC_PRONE_HULL_MIN, VEC_PRONE_HULL_MAX );
	}

	if ( gpGlobals->curtime > m_fHandleSignalsTime )
	{
		m_fHandleSignalsTime = gpGlobals->curtime + 0.1;
		HandleSignals();
	}
	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}

void CDODPlayer::HandleCommand_Voice( const char *pcmd )
{
	// string should be formatted as "voice_gogogo"

	// go through our list of voice commands and if we find it,
	// emit a voice command

	int i = 0;
	while( g_VoiceCommands[i].pszCommandName != NULL )
	{
		if( Q_strcmp( pcmd, g_VoiceCommands[i].pszCommandName ) == 0 )
		{
			VoiceCommand( i );
			break;
		}
		i++;
	}
}

void CDODPlayer::VoiceCommand( int iVoiceCommand )
{
	//no voices in observer or when dead
	if ( !IsAlive() || IsObserver() )
		return;

	//no voices when game is over
	if (g_fGameOver)
		return;

	if (m_flNextVoice > gpGlobals->curtime )
		return;

	// Emit the voice sound
	CPASFilter filter( WorldSpaceCenter() );

	// Get the country text to fill into the sound name
	char *pszCountry = "US";
	
	if( GetTeamNumber() == TEAM_AXIS )
	{
		pszCountry = "German";
	}

	char szSound[128];
	Q_snprintf( szSound, sizeof(szSound), "Voice.%s_%s", pszCountry, g_VoiceCommands[iVoiceCommand].pszSoundName );

	EmitSound( filter, entindex(), szSound );

	// Don't show the subtitle to the other team
	int oppositeTeam = ( GetTeamNumber() == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;
	filter.RemoveRecipientsByTeam( GetGlobalDODTeam(oppositeTeam) );
	
	// further reduce the voice command subtitle radius
	float flDist;
	float flMaxDist = 1900;
	Vector vecEmitOrigin = GetAbsOrigin();

	int i;
	for ( i=1; i<= MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		flDist = ( pPlayer->GetAbsOrigin() - vecEmitOrigin ).Length2D();

		if ( flDist > flMaxDist )
			filter.RemoveRecipient( pPlayer );
	}

	// Send a subtitle to anyone in the PAS
	UserMessageBegin( filter, "VoiceSubtitle" );
		WRITE_BYTE( entindex() );
		WRITE_BYTE( GetTeamNumber() );
		WRITE_BYTE( iVoiceCommand );
	MessageEnd();

	PlayerAnimEvent_t iHandSignal = g_VoiceCommands[iVoiceCommand].iHandSignal;

	if( iHandSignal != PLAYERANIMEVENT_HS_NONE )
		DoAnimationEvent( iHandSignal );

	m_flNextVoice = gpGlobals->curtime + 1.0;
}

void CDODPlayer::HandleCommand_HandSignal( const char *pcmd )
{
	// string should be formatted as "signal_yes"

	int i = 0;
	while( g_HandSignals[i].pszCommandName != NULL )
	{
		if( Q_strcmp( pcmd, g_HandSignals[i].pszCommandName ) == 0 )
		{
			HandSignal( i );
			break;
		}
		i++;
	}
}

void CDODPlayer::HandSignal( int iSignal )
{
	//no hand signals in observer or when dead
	if ( !IsAlive() || IsObserver() )
		return;

	//or when game is over
	if (g_fGameOver)
		return;

	if (m_flNextHandSignal > gpGlobals->curtime )
		return;

	int oppositeTeam = ( GetTeamNumber() == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;

	// Emit the voice sound
	CRecipientFilter filter;
	filter.AddRecipientsByPVS( WorldSpaceCenter() );
	filter.RemoveRecipientsByTeam( GetGlobalTeam(oppositeTeam) );

	// Send a subtitle to anyone in the PAS
	UserMessageBegin( filter, "HandSignalSubtitle" );
		WRITE_BYTE( entindex() );
		WRITE_BYTE( iSignal );
	MessageEnd();

	DoAnimationEvent( g_HandSignals[iSignal].iHandSignal );

	m_flNextHandSignal = gpGlobals->curtime + 1.0;
}

void CDODPlayer::DropGenericAmmo( void )
{
	if ( !IsAlive() )
		return;

	if( m_bHasGenericAmmo == false )
		return;

	Vector vForward, vRight, vUp;
	AngleVectors( EyeAngles(), &vForward, &vRight, &vUp );

	CAmmoBox *pAmmo = CAmmoBox::Create( WorldSpaceCenter(), vec3_angle, this, GetTeamNumber() );

	pAmmo->ApplyAbsVelocityImpulse( vForward * 300 + vUp * 100 );

	m_hLastDroppedAmmoBox = pAmmo;

	m_bHasGenericAmmo = false;	
}

void CDODPlayer::ReturnGenericAmmo( void )
{
	//play pickup sound
	CPASFilter filter( WorldSpaceCenter() );
	EmitSound( filter, entindex(), "BaseCombatCharacter.AmmoPickup" );

	//allow them to drop generic ammo again
	m_bHasGenericAmmo = true;
}

bool CDODPlayer::GiveGenericAmmo( void )
{
	//give primary weapon ammo
	CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_PRIMARY );

	if( pWpn )
	{
		int nAmmoType = pWpn->GetPrimaryAmmoType();

		int iClipSize = pWpn->GetDODWpnData().iMaxClip1;

		int iAmmoPickupClips = pWpn->GetDODWpnData().m_iAmmoPickupClips;

		if( GiveAmmo( iAmmoPickupClips * iClipSize, nAmmoType, false ) > 0 )
		{
			//some ammo was picked up, consume the ammo box

			//play pickup sound
			CPASFilter filter( WorldSpaceCenter() );
			EmitSound( filter, entindex(), "BaseCombatCharacter.AmmoPickup" );

			return true;
		}
	}	

	return false;
}

ConVar dod_friendlyfiresafezone( "dod_friendlyfiresafezone",
								"100",
								FCVAR_ARCHIVE,
								"Units around a player where they will not damage teammates, even if FF is on",
								true, 0, false, 0 );

void CDODPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	bool bTakeDamage = true;

	CBasePlayer *pAttacker = (CBasePlayer*)ToBasePlayer( info.GetAttacker() );

	bool bFriendlyFire = DODGameRules()->IsFriendlyFireOn();

	if ( pAttacker && ( GetTeamNumber() == pAttacker->GetTeamNumber() ) )
	{
		bTakeDamage = bFriendlyFire;

		// Create a FF-free zone around players for melee and bullets
		if ( bFriendlyFire && ( info.GetDamageType() & (DMG_SLASH | DMG_BULLET | DMG_CLUB) ) )
		{
			float flDist = dod_friendlyfiresafezone.GetFloat();

			Vector vecDist = pAttacker->WorldSpaceCenter() - WorldSpaceCenter();

			if ( vecDist.LengthSqr() < ( flDist*flDist ) )
			{
				bTakeDamage = false;
			}			
		}
	}
		
	if ( m_takedamage != DAMAGE_YES )
		return;

	m_LastHitGroup = ptr->hitgroup;	
	m_LastDamageType = info.GetDamageType();

	Assert( ptr->hitgroup != HITGROUP_GENERIC );

	m_nForceBone = ptr->physicsbone;	//Save this bone for ragdoll

	float flDamage = info.GetDamage();
	float flOriginalDmg = flDamage;

	bool bHeadShot = false;

	//if its an enemy OR ff is on.
	if( bTakeDamage )
	{
		m_Shared.SetSlowedTime( 0.5f );
	}

	char *szHitbox = NULL;

	if( info.GetDamageType() & DMG_BLAST )
	{
		// don't do hitgroup specific grenade damage
		flDamage *= 1.0;
		szHitbox = "dmg_blast";
	}
	else
	{
		switch ( ptr->hitgroup )
		{
		case HITGROUP_HEAD:
			{
				flDamage *= 2.5; //regular head shot multiplier

				if( bTakeDamage )
				{
					Vector dir = vecDir;
					VectorNormalize(dir);

					if ( info.GetDamageType() & DMG_CLUB )
						dir *= 800;
					else
                        dir *= 400;

					dir.z += 100;		// add some lift so it pops better.

					PopHelmet( dir, ptr->endpos );
				}

				szHitbox = "head";
				
				bHeadShot = true;
			}
			break;

		case HITGROUP_CHEST:
			szHitbox = "chest";
			break;

		case HITGROUP_STOMACH:	
			szHitbox = "stomach";
			break;

		case HITGROUP_LEFTARM:
			flDamage *= 0.75;	
			szHitbox = "left arm";
			break;
			
		case HITGROUP_RIGHTARM:
			flDamage *= 0.75;			
			szHitbox = "right arm";
			break;

		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			{
				//we are slowed for 2 full seconds if we get a leg hit
				if( bTakeDamage )
				{
					//m_bSlowedByHit = true;
					//m_flUnslowTime = gpGlobals->curtime + 2;
					m_Shared.SetSlowedTime( 2.0f );
				}

				flDamage *= 0.75;
				
				szHitbox = "leg";
			}
			break;
			
		case HITGROUP_GENERIC:
			szHitbox = "(error - hit generic)";
			break;

		default:
			szHitbox = "(error - hit default)";
			break;
		}
	}	
	
	if ( bTakeDamage )
	{	
		if( dod_debugdamage.GetInt() )
		{
			char buf[256];

			Q_snprintf( buf, sizeof(buf), "%s hit %s in the %s hitgroup for %f damage ( %f base damage ) ( %s now has %d health )\n",
				pAttacker->GetPlayerName(),
				GetPlayerName(),
				szHitbox,
				flDamage, 
				flOriginalDmg,
				GetPlayerName(),
				GetHealth() - (int)flDamage );	

			// print to server
			UTIL_LogPrintf( "%s", buf );

			// print to injured
			ClientPrint( this, HUD_PRINTCONSOLE, buf );

			// print to shooter
			if ( pAttacker )
				ClientPrint( pAttacker, HUD_PRINTCONSOLE, buf );
		}

		// Since this code only runs on the server, make sure it shows the tempents it creates.
		CDisablePredictionFiltering disabler;

		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( flDamage, vecDir, ptr, info.GetDamageType() );

		CEffectData	data;
		data.m_vOrigin = ptr->endpos;
		data.m_vNormal = vecDir * -1;
		data.m_flScale = 4;
		data.m_fFlags = FX_BLOODSPRAY_ALL;
		data.m_nEntIndex = ptr->m_pEnt ?  ptr->m_pEnt->entindex() : 0;
		data.m_flMagnitude = flDamage;

		DispatchEffect( "dodblood", data );

		CTakeDamageInfo subInfo = info;

		subInfo.SetDamage( flDamage );

		AddMultiDamage( subInfo, this );
	}
}


bool CDODPlayer::DropActiveWeapon( void )
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();

	if( pWeapon )
	{
		if( pWeapon->CanDrop() )
		{
			DODWeaponDrop( pWeapon, true );
			return true;
		}
		else
		{
			int iWeaponType = pWeapon->GetDODWpnData().m_WeaponType;
			if( iWeaponType == WPN_TYPE_BAZOOKA || iWeaponType == WPN_TYPE_MG )
			{
				// they are deployed, cannot drop
				Hints()->HintMessage( "#game_cannot_drop_while" );
			}
			else
			{
				// must be a weapon type that cannot be dropped
				Hints()->HintMessage( "#game_cannot_drop" );
			}

			return false;
		}
	}

	return false;
}

CWeaponDODBase* CDODPlayer::GetActiveDODWeapon() const
{
	return dynamic_cast< CWeaponDODBase* >( GetActiveWeapon() );
}

void CDODPlayer::PreThink()
{
	BaseClass::PreThink();

	if ( m_afButtonLast != m_nButtons )
		m_flLastMovement = gpGlobals->curtime;

	if ( g_fGameOver )
		return;

	State_PreThink();

	//Reset bullet force accumulator, only lasts one frame, for ragdoll forces from multiple shots
	m_vecTotalBulletForce = vec3_origin;

	if ( mp_autokick.GetInt() && !IsBot() && !IsHLTV() )
	{
		if ( m_flLastMovement + mp_autokick.GetInt()*60 < gpGlobals->curtime )
		{
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_idle_kick", GetPlayerName() );
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", GetUserID() ) );
			m_flLastMovement = gpGlobals->curtime;
		}
	}
}


bool CDODPlayer::DropPrimaryWeapon( void )
{
	CWeaponDODBase *pWeapon = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_PRIMARY );

	if( pWeapon )
	{
		DODWeaponDrop( pWeapon, false );
		return true;
	}

	return false; 
}

bool CDODPlayer::DODWeaponDrop( CBaseCombatWeapon *pWeapon, bool bThrowForward )
{
	bool bSuccess = false;

	if ( pWeapon )
	{
		Vector vForward;

		AngleVectors( EyeAngles(), &vForward, NULL, NULL );
		Vector vTossPos = WorldSpaceCenter();

		if( bThrowForward )
			vTossPos = vTossPos + vForward * 64;

		Weapon_Drop( pWeapon, &vTossPos, NULL );
			
		pWeapon->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
		pWeapon->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

		CWeaponDODBase *pDODWeapon = dynamic_cast< CWeaponDODBase* >( pWeapon );

		if( pDODWeapon )
		{
			pDODWeapon->SetDieThink(true);

			pDODWeapon->SetWeaponModelIndex( pDODWeapon->GetDODWpnData().szWorldModel );

			//Find out the index of the ammo type
			int iAmmoIndex = pDODWeapon->GetPrimaryAmmoType();

			//If it has an ammo type, find out how much the player has
			if( iAmmoIndex != -1 )
			{
				int iAmmoToDrop = GetAmmoCount( iAmmoIndex );

				//Add this much to the dropped weapon
				pDODWeapon->SetExtraAmmoCount( iAmmoToDrop );

				//Remove all ammo of this type from the player
				SetAmmoCount( 0, iAmmoIndex );
			}
		}

		//=========================================
		// Teleport the weapon to the player's hand
		//=========================================
		int iBIndex = -1;
		int iWeaponBoneIndex = -1;

		CStudioHdr *hdr = pWeapon->GetModelPtr();
		// If I have a hand, set the weapon position to my hand bone position.
		if ( hdr && hdr->numbones() > 0 )
		{
			// Assume bone zero is the root
			for ( iWeaponBoneIndex = 0; iWeaponBoneIndex < hdr->numbones(); ++iWeaponBoneIndex )
			{
				iBIndex = LookupBone( hdr->pBone( iWeaponBoneIndex )->pszName() );
				// Found one!
				if ( iBIndex != -1 )
				{
					break;
				}
			}
			
			if ( iWeaponBoneIndex == hdr->numbones() )
				 return true;

			if ( iBIndex == -1 )
			{
				iBIndex = LookupBone( "ValveBiped.Bip01_R_Hand" );
			}
		}
		else
		{
			iBIndex = LookupBone( "ValveBiped.Bip01_R_Hand" );
		}

		if ( iBIndex != -1)  
		{
			Vector origin;
			QAngle angles;
			matrix3x4_t transform;

			// Get the transform for the weapon bonetoworldspace in the NPC
			GetBoneTransform( iBIndex, transform );

			// find offset of root bone from origin in local space
			// Make sure we're detached from hierarchy before doing this!!!
			pWeapon->StopFollowingEntity();
			pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
			pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
			pWeapon->InvalidateBoneCache();
			matrix3x4_t rootLocal;
			pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );

			// invert it
			matrix3x4_t rootInvLocal;
			MatrixInvert( rootLocal, rootInvLocal );

			matrix3x4_t weaponMatrix;
			ConcatTransforms( transform, rootInvLocal, weaponMatrix );
			MatrixAngles( weaponMatrix, angles, origin );

			// trace the bounding box of the weapon in the desired position
			trace_t tr;
			Vector mins, maxs;

			// not exactly correct bounds, we haven't rotated them to match the attachment
			pWeapon->CollisionProp()->WorldSpaceSurroundingBounds( &mins, &maxs );

			UTIL_TraceHull( WorldSpaceCenter(), origin, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			
			if ( tr.fraction < 1.0 )
				origin = WorldSpaceCenter();
			
			pWeapon->Teleport( &origin, &angles, NULL );
			
			//Have to teleport the physics object as well

			IPhysicsObject *pWeaponPhys = pWeapon->VPhysicsGetObject();

			if( pWeaponPhys )
			{
				Vector vPos;
				QAngle vAngles;
				pWeaponPhys->GetPosition( &vPos, &vAngles );
				pWeaponPhys->SetPosition( vPos, angles, true );

				AngularImpulse	angImp(0,0,0);
				Vector vecAdd = GetAbsVelocity();
				pWeaponPhys->AddVelocity( &vecAdd, &angImp );
			}

			m_hLastDroppedWeapon = pWeapon;
		}

		if ( !GetActiveWeapon() )
		{
			// we haven't auto-switched to anything usable
			// switch to the first weapon we find, even if its empty

			CBaseCombatWeapon *pCheck;

			for ( int i = 0 ; i < WeaponCount(); ++i )
			{
				pCheck = GetWeapon( i );
				if ( !pCheck || !pCheck->CanDeploy() )
				{
					continue;
				}

				Weapon_Switch( pCheck );
				break;
			}
		}
		
		bSuccess = true;
	}

	return bSuccess;
}

extern int g_iHelmetModels[NUM_HELMETS];

void CDODPlayer::PopHelmet( Vector vecDir, Vector vecForceOrigin )
{
	int iPlayerClass = m_Shared.PlayerClass();

	CDODTeam *pTeam = GetGlobalDODTeam( GetTeamNumber() );
	const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( iPlayerClass );

	// See if they already lost their helmet
	if( GetBodygroup( BODYGROUP_HELMET ) == pClassInfo.m_iHairGroup )
		return;
	
	// Nope.. take it off
	SetBodygroup( BODYGROUP_HELMET, pClassInfo.m_iHairGroup );

	// Add the velocity of the player
	vecDir += GetAbsVelocity();

	//CDisablePredictionFiltering disabler;

	EntityMessageBegin( this, true );
		WRITE_BYTE( DOD_PLAYER_POP_HELMET );
		WRITE_VEC3COORD( vecDir );
		WRITE_VEC3COORD( vecForceOrigin );
		WRITE_SHORT( g_iHelmetModels[pClassInfo.m_iDropHelmet] );
	MessageEnd();
}

bool CDODPlayer::BumpWeapon( CBaseCombatWeapon *pBaseWeapon )
{
	CWeaponDODBase *pWeapon = dynamic_cast< CWeaponDODBase* >( pBaseWeapon );
	if ( !pWeapon )
	{
		Assert( false );
		return false;
	}
	
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		extern int gEvilImpulse101;
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Don't let the player fetch weapons through walls
	if( !pWeapon->FVisible( this ) && !(GetFlags() & FL_NOTARGET) )
	{
		return false;
	}

	/*
	CBaseCombatWeapon *pOwnedWeapon = Weapon_OwnsThisType( pWeapon->GetClassname() );

	if( pOwnedWeapon != NULL && ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE ) )
	{
		// its an item that we can hold several of, and use up

		// Just give one ammo
		if( GiveAmmo( 1, pOwnedWeapon->GetPrimaryAmmoType(), true ) > 0 )
			return true;
		else
			return false;
	}
	*/

	// ----------------------------------------
	// If I already have it just take the ammo
	// ----------------------------------------
	if (Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType())) 
	{
		if( Weapon_EquipAmmoOnly( pWeapon ) )
		{
			// Only remove me if I have no ammo left
			if ( pWeapon->HasPrimaryAmmo() )
				return false;

			UTIL_Remove( pWeapon );
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();

	pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
	pWeapon->AddEffects( EF_NODRAW );

	Weapon_Equip( pWeapon );

	int iExtraAmmo = pWeapon->GetExtraAmmoCount();
	
	if( iExtraAmmo )
	{
		//Find out the index of the ammo
		int iAmmoIndex = pWeapon->GetPrimaryAmmoType();

		if( iAmmoIndex != -1 )
		{
			//Remove the extra ammo from the weapon
			pWeapon->SetExtraAmmoCount(0);

			//Give it to the player
			SetAmmoCount( iExtraAmmo, iAmmoIndex );
		}
	}

	return true;
}


bool CDODPlayer::HandleCommand_JoinClass( int iClass )
{
	Assert( GetTeamNumber() != TEAM_SPECTATOR );
	Assert( GetTeamNumber() != TEAM_UNASSIGNED );

	if( GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if( iClass == PLAYERCLASS_UNDEFINED )
		return false;	//they typed in something weird

	int iOldPlayerClass = m_Shared.DesiredPlayerClass();

	// See if we're joining the class we already are
	if( iClass == iOldPlayerClass )
		return true;

	if( !DODGameRules()->IsPlayerClassOnTeam( iClass, GetTeamNumber() ) )
		return false;

	const char *classname = DODGameRules()->GetPlayerClassName( iClass, GetTeamNumber() );

	if( DODGameRules()->CanPlayerJoinClass( this, iClass ) )
	{
		m_Shared.SetDesiredPlayerClass( iClass );	//real class value is set when the player spawns

		if( State_Get() == STATE_PICKINGCLASS )
			State_Transition( STATE_OBSERVER_MODE );

		if( iClass == PLAYERCLASS_RANDOM )
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
				ClientPrint(this, HUD_PRINTTALK, "#game_respawn_as", classname );
			}
			else
			{
				ClientPrint(this, HUD_PRINTTALK, "#game_spawn_as", classname );
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_changeclass" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetInt( "class", iClass );

			gameeventmanager->FireEvent( event );
		}

		TallyLatestTimePlayedPerClass( GetTeamNumber(), iOldPlayerClass );

		// if we're near a respawn point and can see it, just spawn us right away
		if ( ShouldInstantRespawn() )
		{
			DODRespawn();
			m_fNextSuicideTime = gpGlobals->curtime + 1.0;

			// if we dropped our primary weapon, and noone else picked it up, destroy it
			CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( m_hLastDroppedWeapon.Get() );
			if ( pWeapon )
			{
				if ( !pWeapon->GetOwner() )
				{	
					UTIL_Remove( m_hLastDroppedWeapon.Get() );
				}
			}

			// if we dropped our primary weapon, and noone else picked it up, destroy it
			CAmmoBox *pAmmo = dynamic_cast<CAmmoBox *>( m_hLastDroppedAmmoBox.Get() );
			if ( pAmmo )
			{
				UTIL_Remove( pAmmo );
			}
		}
	}
	else
	{
		ClientPrint(this, HUD_PRINTTALK, "#game_class_limit", classname );
		ShowClassSelectMenu();
	}

	// if they missed the last wave while choosing class
	// then restart it now
	DODGameRules()->CreateOrJoinRespawnWave( this );

	return true;
}

void CDODPlayer::ShowClassSelectMenu()
{
	if ( GetTeamNumber() == TEAM_ALLIES	)
	{
		ShowViewPortPanel( PANEL_CLASS_ALLIES );
	}
	else if ( GetTeamNumber() == TEAM_AXIS	)
	{
		ShowViewPortPanel( PANEL_CLASS_AXIS );
	}
}

void CDODPlayer::CheckRotateIntroCam( void )
{
	// Update whatever intro camera it's at.
	if( m_pIntroCamera && (gpGlobals->curtime >= m_fIntroCamTime) )
	{
		MoveToNextIntroCamera();
	}
}

int CDODPlayer::GetHealthAsString( char *pDest, int iDestSize )
{
	Q_snprintf( pDest, iDestSize, "%d", GetHealth() );
	return Q_strlen(pDest);
}

int CDODPlayer::GetLastPlayerIDAsString( char *pDest, int iDestSize )
{
	Q_snprintf( pDest, iDestSize, "last player id'd" );
	return Q_strlen(pDest);
}

int CDODPlayer::GetClosestPlayerHealthAsString( char *pDest, int iDestSize )
{
	Q_snprintf( pDest, iDestSize, "some guy's health" );
	return Q_strlen(pDest);
}

int CDODPlayer::GetPlayerClassAsString( char *pDest, int iDestSize )
{
	int team = GetTeamNumber();

	if( team == TEAM_ALLIES || team == TEAM_AXIS )
	{
		const char *pszClassName = DODGameRules()->GetPlayerClassName( m_Shared.PlayerClass(), GetTeamNumber() );

		Q_snprintf( pDest, iDestSize, "%s", pszClassName );
		return Q_strlen(pDest);
	}
	else
	{
		pDest[0] = '\0';
		return 0;
	}
}

int CDODPlayer::GetNearestLocationAsString( char *pDest, int iDestSize )
{
	float flMinDist = FLT_MAX;
	float flDist;

	const char *pLocationName = "";

	CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point" );

	while( pEnt )
	{
		Vector vecDelta = GetAbsOrigin() - pEnt->GetAbsOrigin();
		flDist = vecDelta.Length();

		if( flDist < flMinDist )
		{
			CControlPoint *pPoint = static_cast< CControlPoint* >( pEnt );
			pLocationName = pPoint->GetName();
			flMinDist = flDist;
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point" );
	}

	pEnt =	gEntList.FindEntityByClassname( NULL, "dod_location" );

	while( pEnt )
	{
		Vector vecDelta = GetAbsOrigin() - pEnt->GetAbsOrigin();
		flDist = vecDelta.Length();

		if( flDist < flMinDist )
		{
			CDODLocation *pPoint = static_cast< CDODLocation* >( pEnt );
			pLocationName = pPoint->GetName();
			flMinDist = flDist;
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "dod_location" );
	}

	Q_snprintf( pDest, iDestSize, "%s", pLocationName );
	return Q_strlen(pDest);
}

int CDODPlayer::GetTimeleftAsString( char *pDest, int iDestSize )
{
	if( !DODGameRules()->IsGameUnderTimeLimit() )
	{
		Q_snprintf( pDest, iDestSize, "-:--" );
		return 4;
	}
	else
	{

		int iTimeLeft = DODGameRules()->GetTimeLeft();

		if ( iTimeLeft < 0 )
		{
			Q_snprintf( pDest, iDestSize, "0:00" );
			return 4;
		}

		int iMinutes = iTimeLeft / 60;
		int iSeconds = iTimeLeft % 60;

		Q_snprintf( pDest, iDestSize, "%d:%.02d", iMinutes, iSeconds );
		return Q_strlen(pDest);
	}
}

// Copy the result into pDest
// return value is number of chars copied
int CDODPlayer::GetStringForEscapeSequence( char c, char *pDest, int iDestSize )
{
	int iCharsCopied = 0;

	switch( c )
	{
	case 't':
		iCharsCopied = GetTimeleftAsString( pDest, iDestSize );
		break;
	case 'h':
		iCharsCopied = GetHealthAsString( pDest, iDestSize );
		break;
	case 'l':
		iCharsCopied = GetNearestLocationAsString( pDest, iDestSize );
		break;
	case 'c':
		iCharsCopied = GetPlayerClassAsString( pDest, iDestSize );
		break;		
	/*case 'i':
		iCharsCopied = GetLastPlayerIDAsString( pDest, iDestSize );
		break;
	case 'r':
		iCharsCopied = GetClosestPlayerHealthAsString( pDest, iDestSize );
		break;
		*/
	default:
		iCharsCopied = 0;
		break;
	}

	return iCharsCopied;
}

void CDODPlayer::CheckChatText( char *p, int bufsize )
{
	//Look for escape sequences and replace

	char *buf = new char[bufsize];
	int pos = 0;

	// Parse say text for escape sequences
	for ( char *pSrc = p; pSrc != NULL && *pSrc != 0 && pos < bufsize-1; pSrc++ )
	{
		if ( *pSrc == '%' )
		{
			char szSubst[32];

			int iResult = GetStringForEscapeSequence( *(pSrc+1), szSubst, sizeof(szSubst) );

			if( iResult )
			{
				buf[pos] = '\0';
				Q_strncat( buf, szSubst, bufsize, COPY_ALL_CHARACTERS );

				pos = MIN( pos + Q_strlen(szSubst), bufsize-1 );

				pSrc++;
			}
			else
			{
				//just copy in the '%'
				buf[pos] = *pSrc;
				pos++;
			}
		}
		else
		{
			// copy each char across
			buf[pos] = *pSrc;
			pos++;
		}
	}

	buf[pos] = '\0';

	// copy buf back into p
	Q_strncpy( p, buf, bufsize );

	delete[] buf;	

	const char *pReadyCheck = p;

	DODGameRules()->CheckChatForReadySignal( this, pReadyCheck );
}

bool CDODPlayer::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "jointeam" ) ) 
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad jointeam syntax\n" );
		}

		int iTeam = atoi( args[1] );
		HandleCommand_JoinTeam( iTeam );
		return true;
	}
	else if( !Q_strncmp( pcmd, "cls_", 4 ) )
	{
		CDODTeam *pTeam = GetGlobalDODTeam( GetTeamNumber() );

		Assert( pTeam );

		int iClassIndex = PLAYERCLASS_UNDEFINED;

		if( pTeam->IsClassOnTeam( pcmd, iClassIndex ) )
		{
			HandleCommand_JoinClass( iClassIndex );
		}
		else
		{
			DevMsg( "player tried to join a class that isn't on this team ( %s )\n", pcmd );
			ShowClassSelectMenu();
		}
		return true;
	}
	else if ( FStrEq( pcmd, "spectate" ) )
	{
		// instantly join spectators
		HandleCommand_JoinTeam( TEAM_SPECTATOR );
		return true;
	}
	else if ( FStrEq( pcmd, "joingame" ) )
	{
		// player just closed MOTD dialog
		if ( m_iPlayerState == STATE_WELCOME )
		{
			State_Transition( STATE_PICKINGTEAM );
		}
		
		return true;
	}
	else if ( FStrEq( pcmd, "joinclass" ) ) 
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad joinclass syntax\n" );
		}

		int iClass = atoi( args[1] );
		HandleCommand_JoinClass( iClass );
		return true;
	}
	else if ( FStrEq( pcmd, "dropammo" ) )
	{
		DropGenericAmmo();
		return true;
	}
	else if ( FStrEq( pcmd, "drop" ) )
	{
		DropActiveWeapon();
		return true;
	}
	else if( !Q_strncmp( pcmd, "voice_", 6 ) )
	{
		HandleCommand_Voice( pcmd );
		return true;
	}
	else if( !Q_strncmp( pcmd, "signal_", 7 ) )
	{
		HandleCommand_HandSignal( pcmd );
		return true;
	}
	else if ( FStrEq( pcmd, "extendfreeze" ) )
	{
		m_flDeathTime += 2.0f;
		return true;
	}

#if defined ( DEBUG )
	else if ( FStrEq( pcmd, "debug" ) )
	{
		DODGameRules()->WriteStatsFile( "1.xml" );
		return true;
	}
	else if ( FStrEq( pcmd, "test_stun" ) )
	{
		Vector vecForward;
		AngleVectors( EyeAngles(), &vecForward );

		trace_t tr;
		UTIL_TraceLine( EyePosition(), EyePosition() + vecForward * 1000, MASK_SOLID, NULL, &tr );

		float flStunAmount = 100;
		float flStunRadius = 100;

		if ( args.ArgC() > 1 )
		{
			flStunAmount = atof( args[1] );
		}

		if ( args.ArgC() > 2 )
		{
			flStunRadius = atof( args[2] );
		}

		if ( tr.fraction < 1.0 )
		{
			CTakeDamageInfo info( this, this, vec3_origin, tr.endpos, flStunAmount, DMG_STUN );
			DODGameRules()->RadiusStun( info, tr.endpos, flStunRadius );
		}

		return true;
	}
	else if ( FStrEq( pcmd, "switch_prim" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_PRIMARY );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "switch_sec" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_SECONDARY );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "switch_melee" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_MELEE );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "switch_gren" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_GRENADES );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "switch_tnt" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_BOMB );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "nextwpn" ) )
	{
		CBaseCombatWeapon *pWpn = GetActiveWeapon();

		SwitchToNextBestWeapon( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "smoke" ) )
	{
		CWeaponDODBase *pWpn = (CWeaponDODBase *)Weapon_GetSlot( 3 );

		Weapon_Switch( pWpn );
		return true;
	}
	else if ( FStrEq( pcmd, "resetents" )  )	
	{
		DODGameRules()->CleanUpMap();
		return true;
	}
	else if( FStrEq( pcmd, "pop" ) )
	{
		Vector vecDir(0,0,200);

		if ( args.ArgC() > 1 )
		{
			vecDir.z = atof( args[1] );
		}

		PopHelmet( vecDir, vec3_origin );
		return true;
	}
	else if ( FStrEq( pcmd, "bandage" ) )
	{
		if ( sv_cheats->GetBool() )
		{
			Bandage();
		}
		return true;
	}
	else if ( FStrEq( pcmd, "printstats" ) )
	{
		PrintLifetimeStats();
		return true;
	}

#endif //_DEBUG

	return BaseClass::ClientCommand( args );
}

// returns true if the selection has been handled and the player's menu 
// can be closed...false if the menu should be displayed again
bool CDODPlayer::HandleCommand_JoinTeam( int team )
{
	CDODGameRules *mp = DODGameRules();
	int iOldTeam = GetTeamNumber();
	int iOldPlayerClass = m_Shared.DesiredPlayerClass();

	if ( !GetGlobalTeam( team ) )
	{
		Warning( "HandleCommand_JoinTeam( %d ) - invalid team index.\n", team );
		return false;
	}

	// If we already died and changed teams once, deny
	if( m_bTeamChanged && team != TEAM_SPECTATOR && iOldTeam != TEAM_SPECTATOR )
	{
		ClientPrint( this, HUD_PRINTCENTER, "game_switch_teams_once" );
		return true;
	}

	if ( team == TEAM_UNASSIGNED )
	{
		// Attempt to auto-select a team, may set team to T, CT or SPEC
		team = mp->SelectDefaultTeam();

		if ( team == TEAM_UNASSIGNED )
		{
			// still team unassigned, try to kick a bot if possible	
			 
			ClientPrint( this, HUD_PRINTTALK, "#All_Teams_Full" );

			team = TEAM_SPECTATOR;
		}
	}

	if ( team == iOldTeam )
		return true;	// we wouldn't change the team
	
	if ( mp->TeamFull( team ) )
	{
		if ( team == TEAM_ALLIES )
		{
			ClientPrint( this, HUD_PRINTTALK, "#Allies_Full" );
		}
		else if ( team == TEAM_AXIS )
		{
			ClientPrint( this, HUD_PRINTTALK, "#Axis_Full" );
		}

		ShowViewPortPanel( PANEL_TEAM );
		return false;
	}

	if ( team == TEAM_SPECTATOR )
	{
		// Prevent this if the cvar is set
		if ( !mp_allowspectators.GetInt() && !IsHLTV() )
		{
			ClientPrint( this, HUD_PRINTTALK, "#Cannot_Be_Spectator" );
			ShowViewPortPanel( PANEL_TEAM );
			return false;
		}

		ChangeTeam( TEAM_SPECTATOR );

		return true;
	}
	
	// If the code gets this far, the team is not TEAM_UNASSIGNED

	// Player is switching to a new team (It is possible to switch to the
	// same team just to choose a new appearance)

	if (mp->TeamStacked( team, GetTeamNumber() ))//players are allowed to change to their own team so they can just change their model
	{
		// The specified team is full
		ClientPrint( 
			this,
			HUD_PRINTCENTER,
			( team == TEAM_ALLIES ) ?	"#Allies_full" : "#Axis_full" );

		ShowViewPortPanel( PANEL_TEAM );
		return false;
	}

	// Show the appropriate Choose Appearance menu
	// This must come before ClientKill() for CheckWinConditions() to function properly

	// Switch their actual team...
	ChangeTeam( team );

	DODGameRules()->CreateOrJoinRespawnWave( this );

	// Force them to choose a new class
	m_Shared.SetDesiredPlayerClass( PLAYERCLASS_UNDEFINED );
	m_Shared.SetPlayerClass( PLAYERCLASS_UNDEFINED );

	TallyLatestTimePlayedPerClass( iOldTeam, iOldPlayerClass );

	return true;
}

void CDODPlayer::State_Transition( DODPlayerState newState )
{
	State_Leave();
	State_Enter( newState );
}


void CDODPlayer::State_Enter( DODPlayerState newState )
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo( newState );

	if ( dod_playerstatetransitions.GetInt() == -1 || dod_playerstatetransitions.GetInt() == entindex() )
	{
		if ( m_pCurStateInfo )
			Msg( "ShowStateTransitions: entering '%s'\n", m_pCurStateInfo->m_pStateName );
		else
			Msg( "ShowStateTransitions: entering #%d\n", newState );
	}
	
	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState)();
}

void CDODPlayer::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}


void CDODPlayer::State_PreThink()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnPreThink )
	{
		(this->*m_pCurStateInfo->pfnPreThink)();
	}
}


CDODPlayerStateInfo* CDODPlayer::State_LookupInfo( DODPlayerState state )
{
	// This table MUST match the 
	static CDODPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CDODPlayer::State_Enter_ACTIVE, NULL, &CDODPlayer::State_PreThink_ACTIVE },
		{ STATE_WELCOME,		"STATE_WELCOME",		&CDODPlayer::State_Enter_WELCOME, NULL, &CDODPlayer::State_PreThink_WELCOME },
		{ STATE_PICKINGTEAM,	"STATE_PICKINGTEAM",	&CDODPlayer::State_Enter_PICKINGTEAM, NULL,	&CDODPlayer::State_PreThink_PICKING },
		{ STATE_PICKINGCLASS,	"STATE_PICKINGCLASS",	&CDODPlayer::State_Enter_PICKINGCLASS, NULL,	&CDODPlayer::State_PreThink_PICKING },
		{ STATE_DEATH_ANIM,		"STATE_DEATH_ANIM",		&CDODPlayer::State_Enter_DEATH_ANIM,	NULL, &CDODPlayer::State_PreThink_DEATH_ANIM },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CDODPlayer::State_Enter_OBSERVER_MODE,	NULL, &CDODPlayer::State_PreThink_OBSERVER_MODE }
	};

	for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
	{
		if ( playerStateInfos[i].m_iPlayerState == state )
			return &playerStateInfos[i];
	}

	return NULL;
}

void CDODPlayer::PhysObjectSleep()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Sleep();
}

void CDODPlayer::PhysObjectWake()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Wake();
}

void CDODPlayer::State_Enter_WELCOME()
{
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	PhysObjectSleep();
	
	// Show info panel
	if ( IsBot() )
	{
		// If they want to auto join a team for debugging, pretend they clicked the button.
		CCommand args;
		args.Tokenize( "joingame" );
		ClientCommand( args );
	}
	else
	{
		const ConVar *hostname = cvar->FindVar( "hostname" );
		const char *title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";
		
		KeyValues *data = new KeyValues("data");
		data->SetString( "title", title );		// info panel title
		data->SetString( "type", "1" );			// show userdata from stringtable entry
		data->SetString( "msg",	"motd" );		// use this stringtable entry
		data->SetInt( "cmd", TEXTWINDOW_CMD_CHANGETEAM );  // exec this command if panel closed
		data->SetBool( "unload", sv_motd_unload_on_dismissal.GetBool() );

		ShowViewPortPanel( PANEL_INFO, true, data );

		data->deleteThis();
	}	
}


void CDODPlayer::State_PreThink_WELCOME()
{
	// Verify some state.
	Assert( GetMoveType() == MOVETYPE_NONE );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
	Assert( GetAbsVelocity().Length() == 0 );

	CheckRotateIntroCam();
}


void CDODPlayer::State_Enter_PICKINGTEAM()
{
	ShowViewPortPanel( PANEL_TEAM );
}

void CDODPlayer::State_PreThink_PICKING()
{
}

void CDODPlayer::State_Enter_DEATH_ANIM()
{
	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	// Used for a timer.
	m_flDeathTime = gpGlobals->curtime;

	StartObserverMode( OBS_MODE_DEATHCAM );	// go to observer mode

	RemoveEffects( EF_NODRAW );	// still draw player body

	DODGameRules()->CreateOrJoinRespawnWave( this );

	m_bAbortFreezeCam = false;

	m_bPlayedFreezeCamSound = false;
}

#define DOD_DEATH_ANIMATION_TIME 1.6f
#define DOD_FREEZECAM_LENGTH 3.4f

void CDODPlayer::State_PreThink_DEATH_ANIM()
{
	// If the anim is done playing, go to the next state (waiting for a keypress to 
	// either respawn the guy or put him into observer mode).
	if ( GetFlags() & FL_ONGROUND )
	{
		float flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vAbsVel = GetAbsVelocity();
			VectorNormalize( vAbsVel );
			vAbsVel *= flForward;
			SetAbsVelocity( vAbsVel );
		}
	}

	if ( dod_freezecam.GetBool() )
	{
		// important! freeze end time must match DEATH_CAM_TIME
		// or else players will start missing respawn waves.

		float flFreezeEnd = (m_flDeathTime + DOD_DEATH_ANIMATION_TIME + DOD_FREEZECAM_LENGTH );

		if ( !m_bPlayedFreezeCamSound && GetObserverTarget() && GetObserverTarget() != this )
		{
			// Start the sound so that it ends at the freezecam lock on time
			float flFreezeSoundLength = 0.3;
			float flFreezeSoundTime = m_flDeathTime + DOD_DEATH_ANIMATION_TIME - flFreezeSoundLength + 0.4f /* travel time */;
			if ( gpGlobals->curtime >= flFreezeSoundTime )
			{
				CSingleUserRecipientFilter filter( this );
				EmitSound_t params;
				params.m_flSoundTime = 0;
				params.m_pSoundName = "Player.FreezeCam";
				EmitSound( filter, entindex(), params );

				m_bPlayedFreezeCamSound = true;
			}
		}

		if ( gpGlobals->curtime >= (m_flDeathTime + DOD_DEATH_ANIMATION_TIME ) )	// allow 2 seconds death animation / death cam
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
				// If we're in freezecam, and we want out, abort.
				if ( m_bAbortFreezeCam )
				{
					m_lifeState = LIFE_DEAD;

					StopAnimation();

					IncrementInterpolationFrame();

					if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND) )
						SetMoveType( MOVETYPE_NONE );

					State_Transition( STATE_OBSERVER_MODE );
				}
			}

			// Don't allow anyone to respawn until freeze time is over, even if they're not
			// in freezecam. This prevents players skipping freezecam to spawn faster.

			if ( gpGlobals->curtime >= flFreezeEnd )
			{
				m_lifeState = LIFE_DEAD;

				StopAnimation();

				IncrementInterpolationFrame();

				if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND) )
					SetMoveType( MOVETYPE_NONE );

				State_Transition( STATE_OBSERVER_MODE );
			}
		}
	}
	else
	{
		if ( gpGlobals->curtime >= (m_flDeathTime + DEATH_CAM_TIME ) )	// allow x seconds death animation / death cam
		{
			m_lifeState = LIFE_DEAD;

			StopAnimation();

			IncrementInterpolationFrame();

			if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND) )
				SetMoveType( MOVETYPE_NONE );

			// Disabled death cam!
			//StartReplayMode( 8, 8, entindex() );

			State_Transition( STATE_OBSERVER_MODE );
		}
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayer::AttemptToExitFreezeCam( void )
{
	float flFreezeTravelTime = (m_flDeathTime + DOD_DEATH_ANIMATION_TIME ) + 0.4 /*spec_freeze_traveltime.GetFloat()*/ + 0.5;
	if ( gpGlobals->curtime < flFreezeTravelTime )
		return;

	m_bAbortFreezeCam = true;
}

void CDODPlayer::State_Enter_OBSERVER_MODE()
{
	// Always start a spectator session in chase mode
	m_iObserverLastMode = OBS_MODE_CHASE;

	if( m_hObserverTarget == NULL )
	{
		// find a new observer target
		CheckObserverSettings();
	}

	// Change our observer target to the player nearest us
	CTeam *pTeam = GetGlobalTeam( TEAM_ALLIES );

	CBasePlayer *pPlayer;
	Vector localOrigin = GetAbsOrigin();
	Vector targetOrigin;
	float flMinDist = FLT_MAX;
	float flDist;

	for ( int i=0;i<pTeam->GetNumPlayers();i++ )
	{
		pPlayer = pTeam->GetPlayer(i);

		if ( !pPlayer )
			continue;

		if ( !IsValidObserverTarget(pPlayer) )
			continue;

		targetOrigin = pPlayer->GetAbsOrigin();

		flDist = ( targetOrigin - localOrigin ).Length();

		if ( flDist < flMinDist )
		{
			m_hObserverTarget.Set( pPlayer );
			flMinDist = flDist;
		}
	}

	if ( GetTeamNumber() == TEAM_ALLIES || GetTeamNumber() == TEAM_AXIS )
	{
		// we are entering spec while playing ( not on TEAM_SPECTATOR )
		Hints()->HintMessage( HINT_CLASSMENU, true );
	}
		
	StartObserverMode( m_iObserverLastMode );
	
	PhysObjectSleep();
}

void CDODPlayer::State_PreThink_OBSERVER_MODE()
{
	// Make sure nobody has changed any of our state.
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
	
	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );
}

void CDODPlayer::State_Enter_PICKINGCLASS()
{
	// go to spec mode, if dying keep deathcam
	if ( GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		StartObserverMode( OBS_MODE_DEATHCAM );
	}
	else
	{
		StartObserverMode( OBS_MODE_ROAMING );
	}

	PhysObjectSleep();

	ShowClassSelectMenu();
}

void CDODPlayer::State_Enter_ACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
    m_Local.m_iHideHUD = 0;
	PhysObjectWake();
}

void CDODPlayer::State_PreThink_ACTIVE()
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize prone at spawn.
//-----------------------------------------------------------------------------
void CDODPlayer::InitProne( void )
{
	m_Shared.SetProne( false, true );
}

void CDODPlayer::InitSprinting( void )
{
	m_Shared.SetSprinting( false );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool CDODPlayer::CanSprint()
{
	return ( 
		//!IsWalking() &&									// Not if we're walking
		!( m_Local.m_bDucked && !m_Local.m_bDucking ) &&	// Nor if we're ducking
		(GetWaterLevel() != 3) );							// Certainly not underwater
}

void CDODPlayer::MoveToNextIntroCamera()
{
	m_pIntroCamera = gEntList.FindEntityByClassname( m_pIntroCamera, "point_viewcontrol" );

	// if m_pIntroCamera is NULL we just were at end of list, start searching from start again
	if(!m_pIntroCamera)
		m_pIntroCamera = gEntList.FindEntityByClassname(m_pIntroCamera, "point_viewcontrol");

	if( !m_pIntroCamera  ) //if there are no cameras find a spawn point and black out the screen
	{
		DODGameRules()->GetPlayerSpawnSpot( this );
		SetAbsAngles( QAngle( 0, 0, 0 ) );
		m_pIntroCamera = NULL;  // never update again
	}
	else
	{
		Vector vIntroCamera = m_pIntroCamera->GetAbsOrigin();
			
		QAngle CamAngles = m_pIntroCamera->GetAbsAngles();

		UTIL_SetSize( this, vec3_origin, vec3_origin );
	
		SetAbsOrigin( vIntroCamera );
		SetAbsAngles( CamAngles );
		SnapEyeAngles( CamAngles );
		SetViewOffset( vec3_origin );
		m_fIntroCamTime = gpGlobals->curtime + 6;
	}
}

CBaseEntity *CDODPlayer::SelectSpawnSpot( CUtlVector<EHANDLE> *pSpawnPoints, int &iLastSpawnIndex )
{
	Assert( pSpawnPoints );

	int iNumSpawns = pSpawnPoints->Count();

	CBaseEntity *pSpot = NULL;

	for ( int i=0;i<iNumSpawns;i++ )
	{
		int testIndex = ( iLastSpawnIndex + i ) % iNumSpawns;

		EHANDLE handle = pSpawnPoints->Element(testIndex);

		if ( !handle )
			continue;

		pSpot = handle.Get();

		if ( !pSpot )
			continue;

		if( g_pGameRules->IsSpawnPointValid( pSpot, this ) )
		{
			if ( pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
			{
				continue;
			}

			// if so, go to pSpot
			iLastSpawnIndex = testIndex;
			return pSpot;
		}
	}

	DevMsg("CDODPlayer::SelectSpawnSpot: couldn't find valid spawn point.\n");

	// If we get here, all spawn points are blocked.
	// Try the 4 locations around each spawn point

	Assert( pSpot );

	Vector vecForward, vecRight, vecUp;

	Vector mins = VEC_HULL_MIN;
	Vector maxs = VEC_HULL_MAX;
	float flHalfWidth = maxs.x;
	float flTestDist = 3 * flHalfWidth;

	for ( int i=0;i<iNumSpawns;i++ )
	{
		int testIndex = ( iLastSpawnIndex + i ) % iNumSpawns;

		EHANDLE handle = pSpawnPoints->Element(testIndex);

		if ( !handle )
			continue;

		pSpot = handle.Get();

		if ( !pSpot )
			continue;

		// we know this spot is blocked, but check to the N, E, S, W of it.

		// if we find a clear spot, create a new spawn point there, copied from pSpot

		AngleVectors( pSpot->GetAbsAngles(), &vecForward, &vecRight, &vecUp );

		for( int i=0;i<4;i++ )
		{
			Vector origin = pSpot->GetAbsOrigin();

			switch( i )
			{
			case 0:	origin += vecForward * flTestDist; break;
			case 1: origin += vecRight * flTestDist; break;
			case 2: origin -= vecForward * flTestDist; break;
			case 3: origin -= vecRight * flTestDist; break;
			}

			Vector vTestMins = origin + mins;
			Vector vTestMaxs = origin + maxs;

			if ( UTIL_IsSpaceEmpty( this, vTestMins, vTestMaxs ) )
			{
				QAngle spotAngles = pSpot->GetAbsAngles();

				// make a new spawnpoint so we don't have to do this a bunch of times
				pSpot = CreateEntityByName( pSpot->GetClassname() );
				pSpot->SetAbsOrigin( origin );
				pSpot->SetAbsAngles( spotAngles );

				// delete it in a while so we don't accumulate entities
				pSpot->SetThink( &CBaseEntity::SUB_Remove );
				pSpot->SetNextThink( gpGlobals->curtime + 0.5 );

				Assert( pSpot );
				
				iLastSpawnIndex = 0;

				return pSpot;
			}
		}
	}

	// if we get here, we're really screwed. Spawning the person is going to stick them
	// into someone or something, but we really tried hard, I swear.
	Assert( !"We should never be here" );

	return NULL;
}


CBaseEntity* CDODPlayer::EntSelectSpawnPoint()
{
	CBaseEntity *pSpot = NULL;

	switch( GetTeamNumber() )
	{
	case TEAM_ALLIES:
		{
			CUtlVector<EHANDLE> *pSpawnList = DODGameRules()->GetSpawnPointListForTeam( TEAM_ALLIES );
			pSpot = SelectSpawnSpot( pSpawnList, g_iLastAlliesSpawnIndex );	
		}
		break;
	case TEAM_AXIS:
		{
			CUtlVector<EHANDLE> *pSpawnList = DODGameRules()->GetSpawnPointListForTeam( TEAM_AXIS );
			pSpot = SelectSpawnSpot( pSpawnList, g_iLastAxisSpawnIndex );	
		}		
		break;
	case TEAM_SPECTATOR:
	case TEAM_UNASSIGNED:
	default:
		{
			pSpot = CBaseEntity::Instance( INDEXENT(0) );
		}
		break;		
	}

	if ( !pSpot )
	{
		Warning( "PutClientInServer: no valid spawns on level\n" );
		return CBaseEntity::Instance( INDEXENT(0) );
	}

	return pSpot;
} 

//-----------------------------------------------------------------------------
// Purpose: Put the player in the specified team
//-----------------------------------------------------------------------------
void CDODPlayer::ChangeTeam( int iTeamNum )
{
	if ( !GetGlobalTeam( iTeamNum ) )
	{
		Warning( "CDODPlayer::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;
	
	m_bTeamChanged = true;

	// do the team change:
	BaseClass::ChangeTeam( iTeamNum );

	// update client state 
	if ( iTeamNum == TEAM_UNASSIGNED )
	{
		State_Transition( STATE_OBSERVER_MODE );
	}
	else if ( iTeamNum == TEAM_SPECTATOR )
	{
		RemoveAllItems( true );
		
		State_Transition( STATE_OBSERVER_MODE );

		StatEvent_UploadStats();
		m_StatProperty.SetClassAndTeamForThisLife( -1, TEAM_SPECTATOR );
	}
	else // active player
	{
		if ( !IsDead() )
		{
			// Kill player if switching teams while alive
			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			// IncrementFragCount( 1 );
		}

		if( iOldTeam == TEAM_SPECTATOR )
			SetMoveType( MOVETYPE_NONE );

		// Put up the class selection menu.
		State_Transition( STATE_PICKINGCLASS );
	}

	RemoveNemesisRelationships();
}

void CDODPlayer::DODRespawn( void )
{
	// if it's a forced respawn, accumulate the stats now
	if ( IsAlive() )
	{
		StatEvent_UploadStats();
	}

	RemoveAllItems(true);

	StopObserverMode();

	State_Transition( STATE_ACTIVE );
	Spawn();
	m_nButtons = 0;
	SetNextThink( TICK_NEVER_THINK );

	OutputDamageGiven();
	OutputDamageTaken();
	ResetDamageCounters();
}

bool CDODPlayer::IsReadyToPlay( void )
{
	bool bResult = ( ( GetTeamNumber() == TEAM_ALLIES || GetTeamNumber() == TEAM_AXIS ) &&
						m_Shared.DesiredPlayerClass() != PLAYERCLASS_UNDEFINED );

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CDODPlayer::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
#if !defined( CLIENT_DLL )
	IServerVehicle *pVehicle = GetVehicle();
#else
	IClientVehicle *pVehicle = GetVehicle();
#endif	

	if (pVehicle && !UsingStandardWeaponsInVehicle())
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}

void CDODPlayer::SetScore( int score )
{
	m_iScore = score;
}

void CDODPlayer::AddScore( int num )
{
	int n = MAX( 0, num );
	m_iScore += n;
}

void CDODPlayer::HandleSignals( void )
{
	int changed = m_signals.Update();
	int state	= m_signals.GetState();

	if ( changed & SIGNAL_CAPTUREAREA )
	{
		if ( state & SIGNAL_CAPTUREAREA )
		{
			//do nothing
		}
		else
		{
			SetCapAreaIndex(-1);
			SetCPIndex(-1);
			m_iCapAreaIconIndex = -1;
		}
	}	
}

void CDODPlayer::SetCapAreaIndex( int index )
{
	m_iCapAreaIndex = index;
}

int CDODPlayer::GetCapAreaIndex( void )
{
	return m_iCapAreaIndex;
}

void CDODPlayer::SetCPIndex( int index )
{
	m_Shared.SetCPIndex( index );
}

int CDODPlayer::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// set damage type sustained
	m_bitsDamageType |= info.GetDamageType();

	int iInitialHealth = m_iHealth;

	if ( !CBaseCombatCharacter::OnTakeDamage_Alive( info ) )
		return 0;

	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );

	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("health", MAX(0, m_iHealth) );
		event->SetInt("damage", info.GetDamage() );
		event->SetInt("hitgroup", m_LastHitGroup );

		CBaseEntity *attacker = info.GetAttacker();
		const char *weaponName = "";
		DODWeaponID weaponID = WEAPON_NONE;

		if ( attacker->IsPlayer() )
		{
			CBasePlayer *player = ToBasePlayer( attacker );
			event->SetInt("attacker", player->GetUserID() ); // hurt by other player
			
			// ff damage
			// no hint for bomb explosions, it was their own fault!
			CDODPlayer *pDODPlayer = ToDODPlayer( player );
			if( pDODPlayer->GetTeamNumber() == GetTeamNumber() && pDODPlayer != this && !FBitSet( info.GetDamageType(), DMG_BOMB ) )
			{
				pDODPlayer->Hints()->HintMessage( HINT_FRIEND_INJURED );
			}

			CBaseEntity *pInflictor = info.GetInflictor();
			if ( pInflictor )
			{
				if ( pInflictor == pDODPlayer )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( pDODPlayer->GetActiveWeapon() )
					{
						CWeaponDODBase *pWeapon = pDODPlayer->GetActiveDODWeapon();
						weaponName = pWeapon->GetClassname();

						if ( info.GetDamageCustom() & MELEE_DMG_SECONDARYATTACK )
							weaponID = pWeapon->GetAltWeaponID();
						else
							weaponID = pWeapon->GetStatsWeaponID();
					}
				}
				else
				{
					weaponName = STRING( pInflictor->m_iClassname );  // it's just that easy
				}
			}
		}
		else
		{
			event->SetInt("attacker", 0 ); // hurt by "world"
		}

		if ( strncmp( weaponName, "weapon_", 7 ) == 0 )
		{
			weaponName += 7;
		}
		else if ( strncmp( weaponName, "rocket_", 7 ) == 0 )
		{
			weaponName += 7;

			CDODBaseRocket *pRocket = dynamic_cast< CDODBaseRocket *>( info.GetInflictor() );
			if ( pRocket )
			{
				weaponID = pRocket->GetEmitterWeaponID();
			}
		}
		else if ( strncmp( weaponName, "grenade_", 8 ) == 0 )
		{
			weaponName += 8;

			CDODBaseGrenade *pGren = dynamic_cast< CDODBaseGrenade *>( info.GetInflictor() );
			if ( pGren )
			{
				weaponID = pGren->GetEmitterWeaponID();
			}
		}
		else if ( Q_stricmp( weaponName, "dod_bomb_target" ) == 0 )
		{
			weaponID = WEAPON_NONE;
		}

		event->SetString( "weapon", weaponName );
		event->SetInt( "priority", 5 );

		gameeventmanager->FireEvent( event );

#ifndef CLIENT_DLL
		IGameEvent * stats_event = gameeventmanager->CreateEvent( "dod_stats_player_damage" );
		if ( stats_event && attacker->IsPlayer() && weaponID != WEAPON_NONE )
		{
			CBasePlayer *pAttacker = ToBasePlayer( attacker );

				int iDamage = ( info.GetDamage() + 0.5f );	// round to nearest integer

				stats_event->SetInt( "attacker", pAttacker->GetUserID() );
				stats_event->SetInt( "victim", GetUserID() );
				stats_event->SetInt( "weapon", weaponID );
				stats_event->SetInt( "damage", iDamage );

				// damage_given is the amount of damage applied, not to exceed how much life we have
				stats_event->SetInt( "damage_given", MIN( iDamage, iInitialHealth ) );

				CBaseEntity *pInflictor = info.GetInflictor();
				float flDist = ( pInflictor->GetAbsOrigin() - GetAbsOrigin() ).Length();
				stats_event->SetFloat( "distance", flDist );

				stats_event->SetInt("hitgroup", m_LastHitGroup );

				gameeventmanager->FireEvent( stats_event );
		}
#endif	//CLIENT_DLL
	}
	
	return 1;
}

ConVar dod_explosionforcescale( "dod_explosionforcescale", "1.0", FCVAR_CHEAT );
ConVar dod_bulletforcescale( "dod_bulletforcescale", "1.0", FCVAR_CHEAT );

int CDODPlayer::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	CBaseEntity *pInflictor = info.GetInflictor();

	if ( !pInflictor )
		return 0;

	if ( GetMoveType() == MOVETYPE_NOCLIP )
		return 0;

	// if the player's team does not match the inflictor's team
	// the player has changed teams between when they started the attack
	if( pInflictor->GetTeamNumber() != TEAM_UNASSIGNED && 
		info.GetAttacker() != NULL &&
		pInflictor->GetTeamNumber() != info.GetAttacker()->GetTeamNumber() )
	{
		info.SetDamage( 0 );
		info.SetDamageType( 0 );
	}

	bool bFriendlyFire = DODGameRules()->IsFriendlyFireOn();

	if ( bFriendlyFire ||
		info.GetAttacker()->GetTeamNumber() != GetTeamNumber() ||
		pInflictor == this ||
		info.GetAttacker() == this ||
		info.GetDamageType() & DMG_BOMB )
	{
		// Do special stun damage effect
		if ( info.GetDamageType() & DMG_STUN )
		{
			OnDamageByStun( info );
			return 0;
		}

		CDODPlayer *pPlayer = ToDODPlayer( info.GetAttacker() );

		// keep track of amount of damage last sustained
		m_lastDamageAmount = info.GetDamage();
		m_LastDamageType = info.GetDamageType();

		if( !FBitSet( info.GetDamageType(), DMG_FALL ) )
			Pain();

		float flForceScale = 1.0;

		// Do special explosion damage effect
		if ( info.GetDamageType() & DMG_BLAST )
		{
			OnDamagedByExplosion( info );
			flForceScale = dod_explosionforcescale.GetFloat();
		}

		if( info.GetDamageType() & DMG_BULLET )
		{
			flForceScale = dod_bulletforcescale.GetFloat();
		}

		// round up!
		int iDamage = (int)( info.GetDamage() + 0.5 );

		if ( pPlayer )
		{
			// Record for the shooter
			pPlayer->RecordDamageGiven( this, iDamage );

			// And for the victim
			RecordDamageTaken( pPlayer, iDamage );
		}
		else
		{
			RecordWorldDamageTaken( iDamage );
		}

		m_vecTotalBulletForce += info.GetDamageForce() * flForceScale;

		CSingleUserRecipientFilter user( this );
		user.MakeReliable();
		UserMessageBegin( user, "Damage" );
			WRITE_BYTE( (int)info.GetDamage() );
			WRITE_LONG( info.GetDamageType() );
			WRITE_VEC3COORD( info.GetInflictor()->WorldSpaceCenter() );
		MessageEnd();
		
		gamestats->Event_PlayerDamage( this, info );

		return CBaseCombatCharacter::OnTakeDamage( info );
	}
	else
	{
		return 0;
	}
}

void CDODPlayer::Pain( void )
{
	if ( m_LastDamageType & DMG_CLUB)
	{
		EmitSound( "Player.MajorPain" );
	}
	else if ( m_LastDamageType & DMG_BLAST )
	{
		EmitSound( "Player.MajorPain" );
	}
	else
	{
		EmitSound( "Player.MinorPain" );
	}
}

void CDODPlayer::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_LastDamageType & DMG_CLUB )
	{
		EmitSound( "Player.MegaPain" );
	}
	else if ( m_LastDamageType & DMG_BLAST )
	{
		EmitSound( "Player.MegaPain" );
	}
	else if ( m_LastHitGroup == HITGROUP_HEAD )	
	{
		EmitSound( "Player.DeathHeadShot" );
	}
	else
	{
		EmitSound( "Player.MinorPain" );
	}
}

void CDODPlayer::OnDamagedByExplosion( const CTakeDamageInfo &info )
{
	if ( info.GetDamage() >= 30.0f )
	{
		SetContextThink( &CDODPlayer::DeafenThink, gpGlobals->curtime + 0.3, DOD_DEAFEN_CONTEXT );

		// The blast will naturally blow the temp ent helmet away
		PopHelmet( info.GetDamagePosition(), info.GetDamageForce() );
	}	
}

ConVar dod_stun_min_pitch( "dod_stun_min_pitch", "30", FCVAR_CHEAT );
ConVar dod_stun_max_pitch( "dod_stun_max_pitch", "50", FCVAR_CHEAT );
ConVar dod_stun_min_yaw( "dod_stun_min_yaw", "120", FCVAR_CHEAT );
ConVar dod_stun_max_yaw( "dod_stun_max_yaw", "150", FCVAR_CHEAT );
ConVar dod_stun_min_roll( "dod_stun_min_roll", "15", FCVAR_CHEAT );
ConVar dod_stun_max_roll( "dod_stun_max_roll", "30", FCVAR_CHEAT );

void CDODPlayer::OnDamageByStun( const CTakeDamageInfo &info )
{
	DevMsg( 2, "took %.1f stun damage\n", info.GetDamage() );

	float flPercent = ( info.GetDamage() / 100.0f );

	m_flStunDuration = 0.0;	// mark it as dirty so it transmits, incase we get the same value twice
	m_flStunDuration = 4.0f * flPercent;
	m_flStunMaxAlpha = 255;

	float flPitch = flPercent * RandomFloat( dod_stun_min_pitch.GetFloat(), dod_stun_max_pitch.GetFloat() ) *
		( (RandomInt( 0, 1 )) ? 1 : -1 );

	float flYaw = flPercent * RandomFloat( dod_stun_min_yaw.GetFloat(), dod_stun_max_yaw.GetFloat() )
		* ( (RandomInt( 0, 1 )) ? 1 : -1 );

	float flRoll = flPercent * RandomFloat( dod_stun_min_roll.GetFloat(), dod_stun_max_roll.GetFloat() )
		* ( (RandomInt( 0, 1 )) ? 1 : -1 );

	DevMsg( 2, "punch: pitch %.1f  yaw %.1f  roll %.1f\n", flPitch, flYaw, flRoll );

	SetPunchAngle( QAngle( flPitch, flYaw, flRoll ) );
}

void CDODPlayer::DeafenThink( void )
{
	int effect = random->RandomInt( 32, 34 );

	CSingleUserRecipientFilter user( this );
	enginesound->SetPlayerDSP( user, effect, false );
}

//=======================================================
// Remember this amount of damage that we dealt for stats
//=======================================================
void CDODPlayer::RecordDamageGiven( CDODPlayer *pVictim, int iDamageGiven )
{
	if ( iDamageGiven <= 0 )
		return;

	FOR_EACH_LL( m_DamageGivenList, i )
	{
		if ( pVictim->GetLifeID() == m_DamageGivenList[i]->GetLifeID() )
		{
			m_DamageGivenList[i]->AddDamage( iDamageGiven );
			return;
		}
	}

	CDamageRecord *record = new CDamageRecord( pVictim->GetPlayerName(), pVictim->GetLifeID(), iDamageGiven );
	int tail = m_DamageGivenList.AddToTail();
	m_DamageGivenList[tail] = record;
}

//=======================================================
// Remember this amount of damage that we took for stats
//=======================================================
void CDODPlayer::RecordDamageTaken( CDODPlayer *pAttacker, int iDamageTaken )
{
	if ( iDamageTaken <= 0 )
		return;

	FOR_EACH_LL( m_DamageTakenList, i )
	{
		if ( pAttacker->GetLifeID() == m_DamageTakenList[i]->GetLifeID() )
		{
			m_DamageTakenList[i]->AddDamage( iDamageTaken );
			return;
		}
	}

	CDamageRecord *record = new CDamageRecord( pAttacker->GetPlayerName(), pAttacker->GetLifeID(), iDamageTaken );
	int tail = m_DamageTakenList.AddToTail();
	m_DamageTakenList[tail] = record;
}

void CDODPlayer::RecordWorldDamageTaken( int iDamageTaken )
{
	if ( iDamageTaken <= 0 )
		return;

	FOR_EACH_LL( m_DamageTakenList, i )
	{
		if ( m_DamageTakenList[i]->GetLifeID() == 0 )
		{
			m_DamageTakenList[i]->AddDamage( iDamageTaken );
			return;
		}
	}

	CDamageRecord *record = new CDamageRecord( "world", 0, iDamageTaken );
	int tail = m_DamageTakenList.AddToTail();
	m_DamageTakenList[tail] = record;
}

//=======================================================
// Reset our damage given and taken counters
//=======================================================
void CDODPlayer::ResetDamageCounters()
{
	m_DamageGivenList.PurgeAndDeleteElements();
	m_DamageTakenList.PurgeAndDeleteElements();
}

//=======================================================
// Output the damage that we dealt to other players
//=======================================================
void CDODPlayer::OutputDamageTaken( void )
{
	bool bPrintHeader = true;
	CDamageRecord *pRecord;
	char buf[64];
	int msg_dest = HUD_PRINTCONSOLE;

	FOR_EACH_LL( m_DamageTakenList, i )
	{
		if( bPrintHeader )
		{
			ClientPrint( this, msg_dest, "Player: %s1 - Damage Taken\n", GetPlayerName() );
			ClientPrint( this, msg_dest, "-------------------------\n" );
			bPrintHeader = false;
		}
		pRecord = m_DamageTakenList[i];

		if( pRecord )
		{
			Q_snprintf( buf, sizeof(buf), "( life ID %i ) - %d in %d %s",
				pRecord->GetLifeID(),
				pRecord->GetDamage(),
				pRecord->GetNumHits(),
				( pRecord->GetNumHits() == 1 ) ? "hit" : "hits" );

			ClientPrint( this, msg_dest, "Damage Taken from \"%s1\" - %s2\n", pRecord->GetPlayerName(), buf );
		}		
	}
}

//=======================================================
// Output the damage that we took from other players
//=======================================================
void CDODPlayer::OutputDamageGiven( void )
{
	bool bPrintHeader = true;
	CDamageRecord *pRecord;
	char buf[64];
	int msg_dest = HUD_PRINTCONSOLE;

	FOR_EACH_LL( m_DamageGivenList, i )
	{
		if( bPrintHeader )
		{
			ClientPrint( this, msg_dest, "Player: %s1 - Damage Given\n", GetPlayerName() );
			ClientPrint( this, msg_dest, "-------------------------\n" );
			bPrintHeader = false;
		}

		pRecord = m_DamageGivenList[i];

		if( pRecord )
		{	
			Q_snprintf( buf, sizeof(buf), "( life ID %i ) - %d in %d %s",
				pRecord->GetLifeID(),
				pRecord->GetDamage(),
				pRecord->GetNumHits(),
				( pRecord->GetNumHits() == 1 ) ? "hit" : "hits" );

			ClientPrint( this, msg_dest, "Damage Given to \"%s1\" - %s2\n", pRecord->GetPlayerName(), buf );
		}		
	}
}

// Sets the player's speed - returns true if successful
bool CDODPlayer::SetSpeed( int speed )
{
	DevMsg( 2, "Changing max speed to %d\n", speed );

	SetMaxSpeed( (float)speed );

	return true;
}
	
void CDODPlayer::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CDODViewModel *vm = ( CDODViewModel * )CreateEntityByName( "dod_viewmodel" );
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

void CDODPlayer::ResetScores( void )
{
	ResetFragCount();
	ResetDeathCount();
	SetScore(0);

	Q_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );
}

bool CDODPlayer::SetObserverMode(int mode)
{
	// DoD forces mp_forcecamera to be  team only
	mp_forcecamera.SetValue( OBS_ALLOW_TEAM );

	if ( mode < OBS_MODE_NONE || mode > OBS_MODE_ROAMING )
		return false;

	// Skip over OBS_MODE_ROAMING for dead players
	if( GetTeamNumber() > TEAM_SPECTATOR )
	{
		if( mode == OBS_MODE_ROAMING )
			mode = OBS_MODE_IN_EYE;
	}

	if ( m_iObserverMode > OBS_MODE_DEATHCAM )
	{
		// remember mode if we were really spectating before
		m_iObserverLastMode = m_iObserverMode;
	}

	m_iObserverMode = mode;

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

	}

	CheckObserverSettings();

	return true;	
}

extern ConVar sv_alltalk;
bool CDODPlayer::CanHearChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console
	if ( !pPlayer )
		return true;

	// teammates can always hear each other if alltalk is on
	if ( sv_alltalk.GetBool() == true )
		return true;

	// can hear dead people in bonus round and at round end
	if ( DODGameRules()->IsInBonusRound() || DODGameRules()->State_Get() == STATE_GAME_OVER )
		return true;

	// alive players cannot hear dead players
	if ( IsAlive() && !pPlayer->IsAlive() && !( pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) )
	{
		return false;
	}

	return true;
}

void CDODPlayer::ResetBleeding( void )
{
	SetBandager( NULL );
}

void CDODPlayer::Bandage( void )
{
	ResetBleeding();

	TakeHealth( mp_bandage_heal_amount.GetFloat(), 0 );

	// change helmet to bandages
}

void CDODPlayer::SetBandager( CDODPlayer *pPlayer )
{
	m_hBandager = pPlayer;	
}

bool CDODPlayer::IsBeingBandaged( void )
{
	return ( m_hBandager.Get() != NULL );
}

void CDODPlayer::CommitSuicide( bool bExplode /*= false*/, bool bForce /*= false*/ )
{
	CommitSuicide( vec3_origin, false, bForce );
}

void CDODPlayer::CommitSuicide( const Vector &vecForce, bool bExplode /*= false*/, bool bForce /*= false*/ )
{
	// If they're suiciding in the spawn, its most likely they're changing class
	// ( or possible suiciding to avoid being killed in the endround )
	// On linux the suicide option sometimes arrives before the joinclass so just 
	// reject it here
	if ( ShouldInstantRespawn() )
		return;

	if( !IsAlive() )
		return;

	// prevent suiciding too often
	if ( m_fNextSuicideTime > gpGlobals->curtime )
		return;

	// don't let them suicide for 5 seconds after suiciding
	m_fNextSuicideTime = gpGlobals->curtime + 5;  

	// have the player kill themselves
	CTakeDamageInfo info( this, this, 0, DMG_NEVERGIB );
	Event_Killed( info );
	Event_Dying( info );
}

bool CDODPlayer::StartReplayMode( float fDelay, float fDuration, int iEntity )
{
	if ( !BaseClass::StartReplayMode( fDelay, fDuration, iEntity ) )
		return false;

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();

	UserMessageBegin( filter, "KillCam" );
	WRITE_BYTE( OBS_MODE_IN_EYE );

	if ( m_hObserverTarget.Get() )
	{
		WRITE_BYTE( m_hObserverTarget.Get()->entindex() );	// first target
		WRITE_BYTE( entindex() );	//second target
	}
	else
	{
		WRITE_BYTE( entindex() );	// first target
		WRITE_BYTE( 0 );	//second target
	}
	MessageEnd();

	ClientPrint( this, HUD_PRINTCENTER, "Kill Cam Replay" );

	return true;
}

void CDODPlayer::StopReplayMode()
{
	BaseClass::StopReplayMode();

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();

	UserMessageBegin( filter, "KillCam" );
		WRITE_BYTE( OBS_MODE_NONE );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MessageEnd();
}

void CDODPlayer::PickUpWeapon( CWeaponDODBase *pWeapon )
{
	// if we have a primary weapon and we are allowed to drop it, drop it and 
	// pick up the one we +used

	CWeaponDODBase *pCurrentPrimaryWpn = (CWeaponDODBase *)Weapon_GetSlot( WPN_SLOT_PRIMARY );

	// drop primary if we can
	if( pCurrentPrimaryWpn )
	{
		if ( pCurrentPrimaryWpn->CanDrop() == false )
		{
			return;
		}

		DODWeaponDrop( pCurrentPrimaryWpn, true );
	}

	// pick up the new one
	if ( BumpWeapon( pWeapon ) )
	{
		pWeapon->OnPickedUp( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override setup bones so that is uses the render angles from
//			the DOD animation state to setup the hitboxes.
//-----------------------------------------------------------------------------
void CDODPlayer::SetupBones( matrix3x4_t *pBoneToWorld, int boneMask )
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


extern ConVar sv_debug_player_use;
extern float IntervalDistance( float x, float x0, float x1 );

//-----------------------------------------------------------------------------
// Purpose: Find which ents are higher priority for receiving our +use
//-----------------------------------------------------------------------------
int CDODPlayer::GetPriorityForPickUpEnt( CBaseEntity *pEnt )
{
	CDODBombTarget *pBombTarget = dynamic_cast< CDODBombTarget *>( pEnt );

	if ( pBombTarget )
	{
		// its a bomb target for planting or defusing, most important
		return 20;
	}

	CDODBaseGrenade *pGren = dynamic_cast< CDODBaseGrenade *>( pEnt );

	if ( pGren )
	{
		// its a grenade, high priority
		return 10;
	}

	CWeaponDODBase *pWeapon = dynamic_cast< CWeaponDODBase *>( pEnt );

	if ( pWeapon )
	{
		// weapons are medium priority
		return 5;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: like regular FindUseEntity, but picks up grenades before other things
//-----------------------------------------------------------------------------
CBaseEntity *CDODPlayer::FindUseEntity()
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;
	
	// UNDONE: Might be faster to just fold this range into the sphere query
	int count = 0;
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = forward - tangents[count]*up;
		VectorNormalize(down);
		UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS )
		{
			if ( sv_debug_player_use.GetBool() )
			{
				NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
				NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
			}

			pNearest = pObject;
			nearestDot = 0;
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	int iHighestPriority = GetPriorityForPickUpEnt( pObject );

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		//NEW FOR DOD 
		// if this entity is higher priority than previous ent, use this one

		int iPriority = GetPriorityForPickUpEnt( pObject );

		// if higher priority, always use
		// within the same priority, use the closer one
		if ( ( iPriority > iHighestPriority ) || ( iPriority == iHighestPriority && dot > nearestDot ) )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
				iHighestPriority = iPriority;
			}
		}
	}

	if ( sv_debug_player_use.GetBool() )
	{
		if ( !pNearest )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 255, 0, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 255, 0, 0, true, 30 );
		}
		else
		{
			NDebugOverlay::Box( pNearest->WorldSpaceCenter(), Vector(-8, -8, -8), Vector(8, 8, 8), 0, 255, 0, true, 30 );
		}
	}

	// Special check for bomb targets, whose radius for +use is larger than other entities
	if ( pNearest == NULL )
	{
		CBaseEntity *pEnt = NULL;
		float flBestDist = FLT_MAX;

		// If we didn't find anything, check to see if the bomb target is close enough to use.
		// This is done separately since there might be something blocking our LOS to it
		// but we might want to use it anyway if it's close enough.

		while( ( pEnt = gEntList.FindEntityByClassname( pEnt, "dod_bomb_target" ) ) != NULL )
		{
			CDODBombTarget *pTarget = static_cast<CDODBombTarget *>( pEnt );

			if ( !pTarget->CanPlantHere( this ) )
				continue;

			Vector pos = WorldSpaceCenter();

			float flDist = ( pos - pTarget->GetAbsOrigin() ).Length();

			Vector toBomb = pTarget->GetAbsOrigin() - EyePosition();
			toBomb.NormalizeInPlace();

			if ( DotProduct( forward, toBomb ) < 0.8 )
			{
				continue;
			}

			// if we are looking directly at a bomb target and it is within our radius, that automatically wins
			if ( flDist < flBestDist && flDist < DOD_BOMB_PLANT_RADIUS )
			{
				flBestDist = flDist;
				pNearest = pTarget;
			}
		}
	}

	return pNearest;
}

void CDODPlayer::PrintLifetimeStats( void )
{
	unsigned int i;

	Msg( "\nWeapon Stats\n================\n" );

	Msg( "                  (shots) (hits) (damage) (avg.dist) (kills) (hits taken) (dmg taken) (times killed) (accuracy)\n" );
	// ( "* thompson     4     3       110      89.8      1         1            0            0           75.0
	//Msg( "*%9s        %2i    %2i     %3i     %5.1f     %2i        %2i         %3i          %2i         %3.1f\n",

	for ( i=0;i<WEAPON_MAX;i++ )
	{			
		if ( m_WeaponStats[i].m_iNumShotsTaken > 0 )
		{
			Msg( "* %10s (%2i) %6i %6i %8i  %9.1f %7i    %9i   %9i      %9i   %9.1f\n",
				WeaponIDToAlias( i ), i,
				m_WeaponStats[i].m_iNumShotsTaken,
				m_WeaponStats[i].m_iNumShotsHit,
				m_WeaponStats[i].m_iTotalDamageGiven,
				m_WeaponStats[i].m_flAverageHitDistance,
				m_WeaponStats[i].m_iNumKills,
				m_WeaponStats[i].m_iNumHitsTaken,
				m_WeaponStats[i].m_iTotalDamageTaken,
				m_WeaponStats[i].m_iTimesKilled,
				100.0 * ( (float)m_WeaponStats[i].m_iNumShotsHit / (float)m_WeaponStats[i].m_iNumShotsTaken ) );
		}			
	}

	Msg( "\nPlayer Stats\n================\n" );

	for( i=0;i<m_KilledPlayers.Count();i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( m_KilledPlayers[i].m_iUserID );

		const char *pName = pPlayer ? pPlayer->GetPlayerName() : m_KilledByPlayers[i].m_szPlayerName;

		Msg( "* Killed Player '%s' %i times ( %i damage )\n", 
			pName,
			m_KilledPlayers[i].m_iKills,
			m_KilledPlayers[i].m_iTotalDamage );		
	}

	Msg( "\n" );

	for( i=0;i<m_KilledByPlayers.Count();i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( m_KilledByPlayers[i].m_iUserID );

		Assert( pPlayer && "If this fails, the player has disconnected. Fix this!" );

		const char *pName = pPlayer ? pPlayer->GetPlayerName() : m_KilledByPlayers[i].m_szPlayerName;

		Msg( "* Player '%s' killed you %i times ( %i damage )\n", 
			pName,
			m_KilledByPlayers[i].m_iKills,
			m_KilledByPlayers[i].m_iTotalDamage );
	}

	Msg( "\n" );

	Msg( "* Areas Captured: %i\n", m_iNumAreaCaptures );
	Msg( "* Areas Defended: %i\n", m_iNumAreaDefenses );
	Msg( "* Num Bonus Round Kills: %i\n", m_iNumBonusRoundKills );

	Msg( "\n" );

	// time spent as each class
	Msg( "Time spent as each class:\n" );

	// Make sure to tally the latest time
	int playerclass = m_Shared.DesiredPlayerClass();

	//evil, re-map -2 to 6 so it goes on the end of the array
	if ( playerclass == PLAYERCLASS_RANDOM )
		playerclass = 6;

	/*
	m_flTimePlayedPerClass[playerclass] += ( gpGlobals->curtime - m_flLastClassChangeTime );
	m_flLastClassChangeTime = gpGlobals->curtime;

	Msg( "* Rifleman: %.0f\n", m_flTimePlayedPerClass[0] );
	Msg( "* Assault: %.0f\n", m_flTimePlayedPerClass[1] );
	Msg( "* Support: %.0f\n", m_flTimePlayedPerClass[2] );
	Msg( "* Sniper: %.0f\n", m_flTimePlayedPerClass[3] );
	Msg( "* MG: %.0f\n", m_flTimePlayedPerClass[4] );
	Msg( "* Rocket: %.0f\n", m_flTimePlayedPerClass[5] );
	Msg( "* Random: %.0f\n", m_flTimePlayedPerClass[6] );

	Msg( "\n" );
	Msg( "Total time connected %.0f\n", ( gpGlobals->curtime - m_flConnectTime ) );
	*/
}

void CDODPlayer::Stats_WeaponFired( int weaponID )
{
	// increment shots taken for this weapon
	m_WeaponStats[weaponID].m_iNumShotsTaken++;

	DODGameRules()->Stats_WeaponFired( weaponID );

	StatEvent_WeaponFired( (DODWeaponID)weaponID );
}

void CDODPlayer::Stats_WeaponHit( CDODPlayer *pVictim, int weaponID, int iDamage, int iDamageGiven, int hitgroup, float flHitDistance )
{
	// distance
	float flTotalHitDistance = m_WeaponStats[weaponID].m_flAverageHitDistance * m_WeaponStats[weaponID].m_iNumShotsHit;
	flTotalHitDistance += flHitDistance;
	m_WeaponStats[weaponID].m_flAverageHitDistance = flTotalHitDistance / ( m_WeaponStats[weaponID].m_iNumShotsHit + 1 );

	// damage
	m_WeaponStats[weaponID].m_iTotalDamageGiven += iDamageGiven;

	// hitgroup
	m_WeaponStats[weaponID].m_iBodygroupsHit[hitgroup]++;

	// shots hit
	m_WeaponStats[weaponID].m_iNumShotsHit++;

	int userID = pVictim->GetUserID();

	// add total damage to the player record
	int lookup = m_KilledPlayers.Find( userID );
	if ( lookup == m_KilledPlayers.InvalidIndex() )
	{
		// make a new one
		playerstat_t p;
		Q_strncpy( p.m_szPlayerName, pVictim->GetPlayerName(), MAX_PLAYER_NAME_LENGTH );
		p.m_iUserID = userID;
		p.m_iKills = 0;
		p.m_iTotalDamage = iDamageGiven;

		m_KilledPlayers.Insert( userID, p );
	}
	else
	{
		m_KilledPlayers[lookup].m_iTotalDamage += iDamageGiven;
	}

	DODGameRules()->Stats_WeaponHit( weaponID, flHitDistance );

	StatEvent_WeaponHit( (DODWeaponID)weaponID, ( hitgroup == HITGROUP_HEAD ) );
}

void CDODPlayer::Stats_HitByWeapon( CDODPlayer *pAttacker, int weaponID, int iDamage, int iDamageGiven, int hitgroup )
{
	// damage
	m_WeaponStats[weaponID].m_iTotalDamageTaken += iDamageGiven;

	// hitgroup
	m_WeaponStats[weaponID].m_iHitInBodygroups[hitgroup]++;

	// shots hit
	m_WeaponStats[weaponID].m_iNumHitsTaken++;

	int userID = pAttacker->GetUserID();

	// add total damage to the player record
	int lookup = m_KilledByPlayers.Find( userID );
	if ( lookup == m_KilledByPlayers.InvalidIndex() )
	{
		// make a new one
		playerstat_t p;
		Q_strncpy( p.m_szPlayerName, pAttacker->GetPlayerName(), MAX_PLAYER_NAME_LENGTH );
		p.m_iUserID = userID;
		p.m_iKills = 0;
		p.m_iTotalDamage = iDamageGiven;

		m_KilledByPlayers.Insert( userID, p );
	}
	else
	{
		m_KilledByPlayers[lookup].m_iTotalDamage += iDamageGiven;
	}
}

void CDODPlayer::Stats_KilledPlayer( CDODPlayer *pVictim, int weaponID )
{
	m_WeaponStats[weaponID].m_iNumKills++;

	int userID = pVictim->GetUserID();

	// add a kill to the player record
	int lookup = m_KilledPlayers.Find( userID );
	if ( lookup == m_KilledPlayers.InvalidIndex() )
	{
		// make a new one
		playerstat_t p;
		Q_strncpy( p.m_szPlayerName, pVictim->GetPlayerName(), MAX_PLAYER_NAME_LENGTH );
		p.m_iUserID = userID;
		p.m_iKills = 1;
		p.m_iTotalDamage = 0;

		m_KilledPlayers.Insert( userID, p );
	}
	else
	{
		m_KilledPlayers[lookup].m_iKills++;
	}

	// Gamerules records kills per team
	DODGameRules()->Stats_PlayerKill( GetTeamNumber(), m_Shared.PlayerClass() );

	m_iPerRoundKills++;

	StatEvent_KilledPlayer( (DODWeaponID)weaponID );
}

void CDODPlayer::Stats_KilledByPlayer( CDODPlayer *pAttacker, int weaponID )
{
	m_WeaponStats[weaponID].m_iTimesKilled++;

	int userID = pAttacker->GetUserID();

	// add a kill to the player record
	int lookup = m_KilledByPlayers.Find( userID );
	if ( lookup == m_KilledByPlayers.InvalidIndex() )
	{
		// make a new one
		playerstat_t p;
		Q_strncpy( p.m_szPlayerName, pAttacker->GetPlayerName(), MAX_PLAYER_NAME_LENGTH );
		p.m_iUserID = userID;
		p.m_iKills = 1;
		p.m_iTotalDamage = 0;

		m_KilledByPlayers.Insert( userID, p );
	}
	else
	{
		m_KilledByPlayers[lookup].m_iKills++;
	}
}

void CDODPlayer::Stats_AreaDefended()
{
	// map count
	m_iNumAreaDefenses++;

	// round count
	m_iPerRoundDefenses++;
}

void CDODPlayer::Stats_AreaCaptured()
{
	// map count
	m_iNumAreaCaptures++;

	// round count
	m_iPerRoundCaptures++;
}

void CDODPlayer::Stats_BonusRoundKill()
{
	m_iNumBonusRoundKills++;
}

void CDODPlayer::Stats_BombDetonated()
{
	m_iPerRoundBombsDetonated++;
}

void CDODPlayer::NoteWeaponFired()
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

bool CDODPlayer::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if ( !( pCmd->buttons & IN_ATTACK ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5) )
		return false;

	return BaseClass::WantsLagCompensationOnEntity( pPlayer, pCmd, pEntityTransmitBits );
}

void CDODPlayer::TallyLatestTimePlayedPerClass( int iOldTeam, int iOldPlayerClass )
{
	// Tally the time we spent in the last class, for stats purposes
	if ( iOldPlayerClass != PLAYERCLASS_UNDEFINED && ( iOldTeam == TEAM_ALLIES || iOldTeam == TEAM_AXIS ) )
	{
		// store random in the last slot
		if ( iOldPlayerClass == PLAYERCLASS_RANDOM )
			iOldPlayerClass = 6;

		Assert( iOldPlayerClass >= 0 && iOldPlayerClass <= 6 );
		Assert( iOldTeam == TEAM_ALLIES || iOldTeam == TEAM_AXIS );

		if ( iOldTeam == TEAM_ALLIES )
		{
			m_flTimePlayedPerClass_Allies[iOldPlayerClass] += ( gpGlobals->curtime - m_flLastClassChangeTime );
		}
		else if ( iOldTeam == TEAM_AXIS )
		{
			m_flTimePlayedPerClass_Axis[iOldPlayerClass] += ( gpGlobals->curtime - m_flLastClassChangeTime );
		}		
	}

	m_flLastClassChangeTime = gpGlobals->curtime;
}

void CDODPlayer::ResetProgressBar( void )
{
	SetProgressBarTime( 0 );
}

void CDODPlayer::SetProgressBarTime( int barTime )
{
	m_iProgressBarDuration = barTime;
	m_flProgressBarStartTime = this->m_flSimulationTime;
}

void CDODPlayer::SetDefusing( CDODBombTarget *pTarget )
{
	bool bIsDefusing = ( pTarget != NULL ); 

	if ( bIsDefusing && !m_bIsDefusing )
	{
		// start defuse sound
		EmitSound( "Weapon_C4.Disarm" );
		m_Shared.SetDefusing( true );
	}
	else if ( !bIsDefusing && m_bIsDefusing )
	{
		// stop defuse sound
		StopSound( "Weapon_C4.Disarm" );
		m_Shared.SetDefusing( false );
	}

	m_bIsDefusing = bIsDefusing;

	m_pDefuseTarget = pTarget;
}

void CDODPlayer::SetPlanting( CDODBombTarget *pTarget )
{
	bool bIsPlanting = ( pTarget != NULL );

	if ( bIsPlanting && !m_bIsPlanting )
	{
		// start defuse sound
		EmitSound( "Weapon_C4.Plant" );
		m_Shared.SetPlanting( true );
	}
	else if ( !bIsPlanting && m_bIsPlanting )
	{
		// stop defuse sound
		StopSound( "Weapon_C4.Plant" );
		m_Shared.SetPlanting( false );
	}

	m_bIsPlanting = bIsPlanting;
	m_pPlantTarget = pTarget;	
}

void CDODPlayer::StoreCaptureBlock( int iAreaIndex, int iCapAttempt )
{
	m_iLastBlockAreaIndex = iAreaIndex;
	m_iLastBlockCapAttempt = iCapAttempt;
}

// The capture attempt number that was taking place the last time we blocked a capture
int CDODPlayer::GetLastBlockCapAttempt( void )
{
	return m_iLastBlockCapAttempt;
}

// The index of the area where we last blocked a capture
int CDODPlayer::GetLastBlockAreaIndex( void )
{
	return m_iLastBlockCapAttempt;
}

// NOTE! This is unused, unless we reenable our collision bounds to use USE_GAME_CODE
// - This extends our trigger bounds out a long ways and breaks many things, so only
// do this if you know what you're doing.
void CDODPlayer::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	m_Shared.ComputeWorldSpaceSurroundingBox( pVecWorldMins, pVecWorldMaxs );
}

// Called when we fire a bullet, with the number of headshots we hit
void CDODPlayer::HandleHeadshotAchievement( int iNumHeadshots )
{
	if ( iNumHeadshots <= 0 )
	{
		SetPerLifeCounterKV( "headshots", 0 );
	}
	else
	{
		if ( DODGameRules()->State_Get() != STATE_RND_RUNNING )
			return;

		int iHeadshots = GetPerLifeCounterKV( "headshots" ) + iNumHeadshots;

		if ( iHeadshots >= ACHIEVEMENT_NUM_CONSECUTIVE_HEADSHOTS )
		{
			AwardAchievement( ACHIEVEMENT_DOD_CONSECUTIVE_HEADSHOTS );
		}

		SetPerLifeCounterKV( "headshots", iHeadshots );
	}
}

void CDODPlayer::HandleDeployedMGKillCount( int iNumDeployedKills )
{
	if ( iNumDeployedKills <= 0 )
	{
		SetPerLifeCounterKV( "deployed_mg_kills", 0 );
	}
	else
	{
		if ( DODGameRules()->State_Get() != STATE_RND_RUNNING )
			return;

		int iKillsFromPos = GetPerLifeCounterKV( "deployed_mg_kills" ) + iNumDeployedKills;

		if ( iKillsFromPos >= ACHIEVEMENT_MG_STREAK_IS_DOMINATING )
		{
			AwardAchievement( ACHIEVEMENT_DOD_MG_POSITION_STREAK );
		}

		SetPerLifeCounterKV( "deployed_mg_kills", iKillsFromPos );
	}
}

int CDODPlayer::GetDeployedKillStreak( void )
{
	return GetPerLifeCounterKV( "deployed_mg_kills" );
}

void CDODPlayer::HandleEnemyWeaponsAchievement( int iNumEnemyWpnKills )
{
	if ( iNumEnemyWpnKills <= 0 )
	{
		SetPerLifeCounterKV( "enemy_wpn_kills", 0 );
	}
	else
	{
		if ( DODGameRules()->State_Get() != STATE_RND_RUNNING )
			return;

		int iKills = GetPerLifeCounterKV( "enemy_wpn_kills" ) + iNumEnemyWpnKills;

		if ( iKills >= ACHIEVEMENT_NUM_ENEMY_WPN_KILLS )
		{
			AwardAchievement( ACHIEVEMENT_DOD_USE_ENEMY_WEAPONS );
		}

		SetPerLifeCounterKV( "enemy_wpn_kills", iKills );
	}
}

void CDODPlayer::ResetComboWeaponKill( void )
{
	SetPerLifeCounterKV( "combo_wpn_mask", 0 );
}

void CDODPlayer::HandleComboWeaponKill( int iWeaponType )
{
	if ( DODGameRules()->State_Get() != STATE_RND_RUNNING )
		return;

	int iMask = GetPerLifeCounterKV( "combo_wpn_mask" );

	iMask |= iWeaponType;

	SetPerLifeCounterKV( "combo_wpn_mask", iMask );

	const int iRequiredMask = ( WPN_TYPE_MG | WPN_TYPE_SNIPER | WPN_TYPE_RIFLE | WPN_TYPE_SUBMG );
	const int iExplosiveMask = ( WPN_TYPE_GRENADE | WPN_TYPE_RIFLEGRENADE );

	if ( ( iMask & iRequiredMask ) == iRequiredMask ) // must have all these bits set
	{
		if ( ( iMask & iExplosiveMask ) > 0 )	// has one of these bits set
		{
			AwardAchievement( ACHIEVEMENT_DOD_WEAPON_MASTERY );
		}
	}
}

void CDODPlayer::PlayUseDenySound()
{
	EmitSound( "Player.UseDeny" );
}

//-----------------------------------------------------------------------------
// Purpose: Removes all nemesis relationships between this player and others
//-----------------------------------------------------------------------------
void CDODPlayer::RemoveNemesisRelationships()
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer != this )
		{
			// we are no longer dominating anyone
			m_Shared.SetPlayerDominated( pPlayer, false );
			Q_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );

			// noone is dominating us anymore
			pPlayer->m_Shared.SetPlayerDominated( this, false );
			pPlayer->iNumKilledByUnanswered[entindex()] = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when we get a new achievement, recalc our awards
//-----------------------------------------------------------------------------
void CDODPlayer::OnAchievementEarned( int iAchievement )
{
	BaseClass::OnAchievementEarned( iAchievement );

	RecalculateAchievementAwardsMask();
}

//-----------------------------------------------------------------------------
// Purpose: Determine what achievement awards this player has
//-----------------------------------------------------------------------------
void CDODPlayer::RecalculateAchievementAwardsMask( void )
{
#if !defined(NO_STEAM)

	if ( steamgameserverapicontext->SteamGameServerStats() )
	{
		CSteamID steamIDForPlayer;
		if ( GetSteamID( &steamIDForPlayer ) && steamIDForPlayer.BIndividualAccount() )
		{
			steamgameserverapicontext->SteamGameServerStats()->RequestUserStats( steamIDForPlayer );				
		}
	}
#endif
}

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: Called when Steam returns a user's stats
//-----------------------------------------------------------------------------
void CDODPlayer::OnGSStatsReceived( GSStatsReceived_t *pResponse )
{
	if ( pResponse->m_eResult != k_EResultOK )
		return;

	if ( pResponse->m_steamIDUser == GetSteamIDAsUInt64() )
	{
		for ( int i = 1; i < NUM_ACHIEVEMENT_AWARDS; i++ )
		{
			bool bUnlocked;
			if ( steamgameserverapicontext->SteamGameServerStats()->GetUserAchievement( pResponse->m_steamIDUser, g_pszAchievementAwards[i], &bUnlocked ) )
			{
				if ( bUnlocked )
				{
					m_iAchievementAwardsMask |= (1<<i);
					break;
				}
			}
		}
	}
}
#endif

void CDODPlayer::StatEvent_UploadStats( void )
{
	int iSecondsAlive = (int)( gpGlobals->curtime - m_flTimeAsClassAccumulator );

	m_StatProperty.IncrementPlayerClassStat( DODSTAT_PLAYTIME, iSecondsAlive );

	m_StatProperty.SendStatsToPlayer( this );

	m_flTimeAsClassAccumulator = gpGlobals->curtime;
}

void CDODPlayer::StatEvent_KilledPlayer( DODWeaponID iKillingWeapon )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_KILLS );

	m_StatProperty.IncrementWeaponStat( iKillingWeapon, DODSTAT_KILLS );
}

void CDODPlayer::StatEvent_WasKilled( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_DEATHS );
}

void CDODPlayer::StatEvent_RoundWin( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_ROUNDSWON );
}

void CDODPlayer::StatEvent_RoundLoss( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_ROUNDSLOST );
}

void CDODPlayer::StatEvent_PointCaptured( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_CAPTURES );
}

void CDODPlayer::StatEvent_CaptureBlocked( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_BLOCKS );
}

void CDODPlayer::StatEvent_BombPlanted( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_BOMBSPLANTED );
}

void CDODPlayer::StatEvent_BombDefused( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_BOMBSDEFUSED );
}

void CDODPlayer::StatEvent_ScoredDomination( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_DOMINATIONS );
}

void CDODPlayer::StatEvent_ScoredRevenge( void )
{
	m_StatProperty.IncrementPlayerClassStat( DODSTAT_REVENGES );
}

void CDODPlayer::StatEvent_WeaponFired( DODWeaponID iWeaponID )
{
	m_StatProperty.IncrementWeaponStat( iWeaponID, DODSTAT_SHOTS_FIRED );
}

void CDODPlayer::StatEvent_WeaponHit( DODWeaponID iWeaponID, bool bWasHeadshot )
{
	m_StatProperty.IncrementWeaponStat( iWeaponID, DODSTAT_SHOTS_HIT );

	if ( bWasHeadshot )
	{
		m_StatProperty.IncrementWeaponStat( iWeaponID, DODSTAT_HEADSHOTS );
	}
}


// CDODPlayerStatProperty

void CDODPlayerStatProperty::SetClassAndTeamForThisLife( int iPlayerClass, int iTeam )
{
	m_bRecordingStats = ( ( iTeam == TEAM_ALLIES || iTeam == TEAM_AXIS ) && ( iPlayerClass >= 0 && iPlayerClass <= NUM_DOD_PLAYERCLASSES ) );

	m_iCurrentLifePlayerClass = iPlayerClass;
	m_iCurrentLifePlayerTeam = iTeam;
}

void CDODPlayerStatProperty::IncrementPlayerClassStat( DODStatType_t statType, int iValue /* = 1 */ )
{
	if ( !m_bRecordingStats )
		return;

	Assert( m_iCurrentLifePlayerClass >= 0 && m_iCurrentLifePlayerClass <= 5 );

	m_PlayerStatsPerLife.m_iStat[statType] += iValue;
}

void CDODPlayerStatProperty::IncrementWeaponStat( DODWeaponID iWeaponID, DODStatType_t statType, int iValue /* = 1 */ )
{
	if ( !m_bRecordingStats )
		return;

	Assert( iWeaponID >= 0 && iWeaponID <= WEAPON_MAX );
	m_WeaponStatsPerLife[iWeaponID].m_iStat[statType] += iValue;
	m_bWeaponStatsDirty[iWeaponID] = true;
}

void CDODPlayerStatProperty::ResetPerLifeStats( void )
{
	Q_memset( &m_PlayerStatsPerLife, 0, sizeof(m_PlayerStatsPerLife) );
	Q_memset( &m_WeaponStatsPerLife, 0, sizeof(m_WeaponStatsPerLife) );
	Q_memset( &m_bWeaponStatsDirty, 0, sizeof(m_bWeaponStatsDirty) );
}

void CDODPlayerStatProperty::SendStatsToPlayer( CDODPlayer *pPlayer )
{
	if ( !m_bRecordingStats )
		return;

	// make a bit field of all the stats we want to send (all with non-zero values)
	int iStat;
	int iSendPlayerBits = 0;
	for ( iStat = DODSTAT_FIRST; iStat < DODSTAT_MAX; iStat++ )
	{
		if ( m_PlayerStatsPerLife.m_iStat[iStat] > 0 )
		{
			iSendPlayerBits |= ( 1 << ( iStat - DODSTAT_FIRST ) );
		}
	}

	CUtlVector<int> vecWeaponsToSend;

	// for every weapon that we have stats for, set a bit in the weapon mask
	for ( int iWeapon = WEAPON_NONE; iWeapon < WEAPON_MAX; iWeapon++ )
	{
		if ( m_bWeaponStatsDirty[iWeapon] )
		{
			vecWeaponsToSend.AddToTail( iWeapon );
		}
	}

	if ( iSendPlayerBits == 0 && vecWeaponsToSend.Count() == 0 )
	{
		ResetPerLifeStats();
		return;
	}

	iStat = DODSTAT_FIRST;

	CSingleUserRecipientFilter filter( (CBasePlayer *)pPlayer );
	filter.MakeReliable();

	UserMessageBegin( filter, "DODPlayerStatsUpdate" );

	WRITE_BYTE( m_iCurrentLifePlayerClass );						// write the class
	WRITE_BYTE( m_iCurrentLifePlayerTeam );

	WRITE_LONG( iSendPlayerBits );										// write the bit mask of which stats follow in the message

	// write all the non-zero stats according to the bit mask
	while ( iSendPlayerBits > 0 )
	{
		if ( iSendPlayerBits & 1 )
		{
			WRITE_LONG( m_PlayerStatsPerLife.m_iStat[iStat] );
		}
		iSendPlayerBits >>= 1;
		iStat++;
	}

	// now the weapon bits
	// how many weapons
	WRITE_BYTE( vecWeaponsToSend.Count() );

	int i;

	// send the weapons
	for ( i=0;i<vecWeaponsToSend.Count(); i++ )
	{
		WRITE_BYTE( vecWeaponsToSend.Element(i) );
	}

	// for each weapon that we're sending stats for
	for ( i=0;i<vecWeaponsToSend.Count(); i++ )
	{
		int iWeapon = vecWeaponsToSend.Element(i);

		// what stats does iWeapon want to send?
		int iWeaponStatBits = 0;

		for ( iStat = DODSTAT_FIRST; iStat < DODSTAT_MAX; iStat++ )
		{
			if ( m_WeaponStatsPerLife[iWeapon].m_iStat[iStat] > 0 )
			{
				iWeaponStatBits |= ( 1 << ( iStat - DODSTAT_FIRST ) );
			}
		}

		// send the mask of stats that we're sending for this weapon
		WRITE_LONG( iWeaponStatBits );

		// send the stats
		iStat = DODSTAT_FIRST;

		// send that mask
		while ( iWeaponStatBits > 0 )
		{
			if ( iWeaponStatBits & 1 )
			{
				WRITE_LONG( m_WeaponStatsPerLife[iWeapon].m_iStat[iStat] );
			}
			iWeaponStatBits >>= 1;
			iStat++;
		}
	}

	MessageEnd();

	ResetPerLifeStats();
}
