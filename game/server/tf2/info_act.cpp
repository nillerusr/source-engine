//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_team.h"
#include "tier1/strtools.h"
#include "baseentity.h"
#include "tf_shareddefs.h"
#include "info_act.h"

// Global pointer to the current act
CHandle<CInfoAct>	g_hCurrentAct;

BEGIN_DATADESC( CInfoAct )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Start", InputStart ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FinishWinNone", InputFinishWinNone ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FinishWin1", InputFinishWin1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FinishWin2", InputFinishWin2 ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "AddTime", InputAddTime ),

	// outputs
	DEFINE_OUTPUT( m_OnStarted, "OnStarted" ),
	DEFINE_OUTPUT( m_OnFinishedTeamNone, "OnFinishedWinNone" ),
	DEFINE_OUTPUT( m_OnFinishedTeam1, "OnFinishedWin1" ),
	DEFINE_OUTPUT( m_OnFinishedTeam2, "OnFinishedWin2" ),
	DEFINE_OUTPUT( m_OnTimerExpired, "OnTimerExpired" ),

	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_90_REMAINING], "OnRespawn1Team1_90sec" ),
	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_60_REMAINING], "OnRespawn1Team1_60sec" ),
	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_45_REMAINING], "OnRespawn1Team1_45sec" ),
	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_30_REMAINING], "OnRespawn1Team1_30sec" ),
	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_10_REMAINING], "OnRespawn1Team1_10sec" ),
	DEFINE_OUTPUT( m_Respawn1Team1Events[CInfoAct::RESPAWN_TIMER_0_REMAINING], "OnRespawn1Team1" ),
	DEFINE_OUTPUT( m_Respawn1Team1TimeRemaining, "Respawn1Team1TimeRemaining" ),

	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_90_REMAINING], "OnRespawn2Team1_90sec" ),
	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_60_REMAINING], "OnRespawn2Team1_60sec" ),
	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_45_REMAINING], "OnRespawn2Team1_45sec" ),
	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_30_REMAINING], "OnRespawn2Team1_30sec" ),
	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_10_REMAINING], "OnRespawn2Team1_10sec" ),
	DEFINE_OUTPUT( m_Respawn2Team1Events[CInfoAct::RESPAWN_TIMER_0_REMAINING], "OnRespawn2Team1" ),
	DEFINE_OUTPUT( m_Respawn2Team1TimeRemaining, "Respawn2Team1TimeRemaining" ),

	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_90_REMAINING], "OnRespawn1Team2_90sec" ),
	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_60_REMAINING], "OnRespawn1Team2_60sec" ),
	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_45_REMAINING], "OnRespawn1Team2_45sec" ),
	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_30_REMAINING], "OnRespawn1Team2_30sec" ),
	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_10_REMAINING], "OnRespawn1Team2_10sec" ),
	DEFINE_OUTPUT( m_Respawn1Team2Events[CInfoAct::RESPAWN_TIMER_0_REMAINING], "OnRespawn1Team2" ),
	DEFINE_OUTPUT( m_Respawn1Team2TimeRemaining, "Respawn1Team2TimeRemaining" ),

	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_90_REMAINING], "OnRespawn2Team2_90sec" ),
	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_60_REMAINING], "OnRespawn2Team2_60sec" ),
	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_45_REMAINING], "OnRespawn2Team2_45sec" ),
	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_30_REMAINING], "OnRespawn2Team2_30sec" ),
	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_10_REMAINING], "OnRespawn2Team2_10sec" ),
	DEFINE_OUTPUT( m_Respawn2Team2Events[CInfoAct::RESPAWN_TIMER_0_REMAINING], "OnRespawn2Team2" ),
	DEFINE_OUTPUT( m_Respawn2Team2TimeRemaining, "Respawn2Team2TimeRemaining" ),

	DEFINE_OUTPUT( m_Team1RespawnDelayDone, "OnTeam1RespawnDelayDone" ),
	DEFINE_OUTPUT( m_Team2RespawnDelayDone, "OnTeam2RespawnDelayDone" ),

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iActNumber, FIELD_INTEGER, "ActNumber" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flActTimeLimit, FIELD_FLOAT, "ActTimeLimit" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_iszIntermissionCamera, FIELD_STRING, "IntermissionCamera" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawn1Team1Time, FIELD_INTEGER, "Respawn1Team1Time" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawn2Team1Time, FIELD_INTEGER, "Respawn2Team1Time" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawn1Team2Time, FIELD_INTEGER, "Respawn1Team2Time" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawn2Team2Time, FIELD_INTEGER, "Respawn2Team2Time" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawnTeam1Delay, FIELD_INTEGER, "RespawnTeam1InitialDelay" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nRespawnTeam2Delay, FIELD_INTEGER, "RespawnTeam2InitialDelay" ),

	// functions
	DEFINE_FUNCTION( ActThink ),
	DEFINE_FUNCTION( ActThinkEndActOverlayTime ),
	DEFINE_FUNCTION( RespawnTimerThink ),
	DEFINE_FUNCTION( Team1RespawnDelayThink ),
	DEFINE_FUNCTION( Team2RespawnDelayThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CInfoAct, DT_InfoAct)
	SendPropInt(SENDINFO(m_iActNumber), 5 ),
	SendPropInt(SENDINFO(m_spawnflags), SF_ACT_BITS, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(m_flActTimeLimit), 12 ),
	SendPropInt(SENDINFO(m_nRespawn1Team1Time), 8 ),
	SendPropInt(SENDINFO(m_nRespawn2Team1Time), 8 ),
	SendPropInt(SENDINFO(m_nRespawn1Team2Time), 8 ),
	SendPropInt(SENDINFO(m_nRespawn2Team2Time), 8 ),
	SendPropInt(SENDINFO(m_nRespawnTeam1Delay), 8 ),
	SendPropInt(SENDINFO(m_nRespawnTeam2Delay), 8 ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( info_act, CInfoAct );


#define RESPAWN_TIMER_CONTEXT			"RespawnTimerThink"
#define RESPAWN_TEAM_1_DELAY_CONTEXT	"RespawnTeam1DelayThink"
#define RESPAWN_TEAM_2_DELAY_CONTEXT	"RespawnTeam2DelayThink"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInfoAct::CInfoAct( void )
{
	// No act == -1
	m_iActNumber = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CInfoAct::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoAct::Spawn( void )
{
	m_flActStartedAt = 0;
	m_iWinners = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Set up respawn timers
//-----------------------------------------------------------------------------
void CInfoAct::SetUpRespawnTimers()
{
	// NOTE: Need to add the second there so the respawn timers don't immediately trigger
	SetContextThink( RespawnTimerThink, gpGlobals->curtime + 1.0f, RESPAWN_TIMER_CONTEXT );

	if (m_nRespawnTeam1Delay != 0)
	{
		SetContextThink( Team1RespawnDelayThink, gpGlobals->curtime + m_nRespawnTeam1Delay, RESPAWN_TEAM_1_DELAY_CONTEXT );
	}
	else
	{
		m_Team1RespawnDelayDone.FireOutput( this, this );
	}

	if (m_nRespawnTeam2Delay != 0)
	{
		SetContextThink( Team2RespawnDelayThink, gpGlobals->curtime + m_nRespawnTeam2Delay, RESPAWN_TEAM_2_DELAY_CONTEXT );
	}
	else
	{
		m_Team2RespawnDelayDone.FireOutput( this, this );
	}
}

void CInfoAct::ShutdownRespawnTimers()
{
	SetContextThink( NULL, 0, RESPAWN_TIMER_CONTEXT );
	SetContextThink( NULL, 0, RESPAWN_TEAM_1_DELAY_CONTEXT );
	SetContextThink( NULL, 0, RESPAWN_TEAM_2_DELAY_CONTEXT );
}


//-----------------------------------------------------------------------------
// Respawn delay
//-----------------------------------------------------------------------------
void CInfoAct::Team1RespawnDelayThink()
{
	m_Team1RespawnDelayDone.FireOutput( this, this );
	SetContextThink( NULL, 0, RESPAWN_TEAM_1_DELAY_CONTEXT );
}

void CInfoAct::Team2RespawnDelayThink()
{
	m_Team2RespawnDelayDone.FireOutput( this, this );
	SetContextThink( NULL, 0, RESPAWN_TEAM_2_DELAY_CONTEXT );
}


//-----------------------------------------------------------------------------
// Computes the time remaining
//-----------------------------------------------------------------------------
int CInfoAct::ComputeTimeRemaining( int nPeriod, int nDelay )
{
	if (nPeriod <= 0)
		return -1;

	int nTimeDelta = (int)(gpGlobals->curtime - m_flActStartedAt);
	Assert( nTimeDelta >= 0 );
	nTimeDelta -= nDelay;

	// This case takes care of the initial spawn delay time...
	if (nTimeDelta <= 0)
	{
		return nPeriod - nTimeDelta;
	}

	int nFactor = nTimeDelta / nPeriod;
	int nTimeRemainder = nTimeDelta - nFactor * nPeriod;
	if (nTimeRemainder == 0)
		return 0;

	return nPeriod - nTimeRemainder;
}


//-----------------------------------------------------------------------------
// Fires respawn events
//-----------------------------------------------------------------------------
void CInfoAct::FireRespawnEvents( int nTimeRemaining, COutputEvent *pRespawnEvents, COutputInt &respawnTime )
{
	if (nTimeRemaining < 0)
		return;

	switch (nTimeRemaining)
	{
	case 90:
		pRespawnEvents[RESPAWN_TIMER_90_REMAINING].FireOutput( this, this );
		break;
	case 60:
		pRespawnEvents[RESPAWN_TIMER_60_REMAINING].FireOutput( this, this );
		break;
	case 45:
		pRespawnEvents[RESPAWN_TIMER_45_REMAINING].FireOutput( this, this );
		break;
	case 30:
		pRespawnEvents[RESPAWN_TIMER_30_REMAINING].FireOutput( this, this );
		break;
	case 10:
		pRespawnEvents[RESPAWN_TIMER_10_REMAINING].FireOutput( this, this );
		break;
	case 0:
		pRespawnEvents[RESPAWN_TIMER_0_REMAINING].FireOutput( this, this );
		break;
	default:
		break;
	}

	respawnTime.Set( nTimeRemaining, this, this );
}


//-----------------------------------------------------------------------------
// Respawn timers
//-----------------------------------------------------------------------------
void CInfoAct::RespawnTimerThink()
{
	int nTimeRemaining = ComputeTimeRemaining( m_nRespawn1Team1Time, m_nRespawnTeam1Delay );
	FireRespawnEvents( nTimeRemaining, m_Respawn1Team1Events, m_Respawn1Team1TimeRemaining );

	nTimeRemaining = ComputeTimeRemaining( m_nRespawn2Team1Time, m_nRespawnTeam1Delay );
	FireRespawnEvents( nTimeRemaining, m_Respawn2Team1Events, m_Respawn2Team1TimeRemaining );

	nTimeRemaining = ComputeTimeRemaining( m_nRespawn1Team2Time, m_nRespawnTeam2Delay );
	FireRespawnEvents( nTimeRemaining, m_Respawn1Team2Events, m_Respawn1Team2TimeRemaining );

	nTimeRemaining = ComputeTimeRemaining( m_nRespawn2Team2Time, m_nRespawnTeam2Delay );
	FireRespawnEvents( nTimeRemaining, m_Respawn2Team2Events, m_Respawn2Team2TimeRemaining );

	SetNextThink( gpGlobals->curtime + 1.0f, RESPAWN_TIMER_CONTEXT );
}


//-----------------------------------------------------------------------------
// Purpose: The act has started
//-----------------------------------------------------------------------------
void CInfoAct::StartAct( void )
{
	// FIXME: Should this change?
	// Don't allow two simultaneous acts
	if (g_hCurrentAct)
	{
		g_hCurrentAct->FinishAct( );
	}

	// Set the global act to this
	g_hCurrentAct = this;

	m_flActStartedAt = gpGlobals->curtime;
	m_OnStarted.FireOutput( this, this );

	// Do we have a timelimit?
	if ( m_flActTimeLimit )
	{
		SetNextThink( gpGlobals->curtime + m_flActTimeLimit );
		SetThink( ActThink );
	}

	SetUpRespawnTimers();

	// Tell all the clients
	CReliableBroadcastRecipientFilter filter;

	UserMessageBegin( filter, "ActBegin" );
		WRITE_BYTE( (byte)m_iActNumber );
		WRITE_FLOAT( m_flActStartedAt );
	MessageEnd();

	// If we're not an intermission, clean up
	if ( !HasSpawnFlags( SF_ACT_INTERMISSION ) )
	{
		CleanupOnActStart();
	}

	// Cycle through all players and start the act
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer  )
		{
			// Am I an intermission?
			if ( HasSpawnFlags( SF_ACT_INTERMISSION ) )
			{
				StartIntermission( pPlayer );
			}
			else
			{
				StartActOverlayTime( pPlayer );
			}
		}
	}

	// Think again soon, to remove player locks
	if ( !HasSpawnFlags(SF_ACT_INTERMISSION) )
	{
		SetNextThink( gpGlobals->curtime + MIN_ACT_OVERLAY_TIME );
		SetThink( ActThinkEndActOverlayTime );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update a client who joined during the middle of an act
//-----------------------------------------------------------------------------
void CInfoAct::UpdateClient( CBaseTFPlayer *pPlayer )
{
	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "ActBegin" );
		WRITE_BYTE( (byte)m_iActNumber );
		WRITE_FLOAT( m_flActStartedAt );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: The act has finished
//-----------------------------------------------------------------------------
void CInfoAct::FinishAct( )
{
	if ( g_hCurrentAct.Get() != this )
	{
		DevWarning( 2, "Attempted to finish an act which wasn't started!\n" );
		return;
	}

	ShutdownRespawnTimers();

	switch( m_iWinners)
	{
	case 0:
		m_OnFinishedTeamNone.FireOutput( this, this );
		break;

	case 1:
		m_OnFinishedTeam1.FireOutput( this, this );
		break;

	case 2:
		m_OnFinishedTeam2.FireOutput( this, this );
		break;

	default:
		Assert(0);
		break;
	}

	g_hCurrentAct = NULL;

	// Tell all the clients
	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "ActEnd" );
		WRITE_BYTE( m_iWinners );
	MessageEnd();

	// Am I an intermission?
	if ( HasSpawnFlags( SF_ACT_INTERMISSION ) )
	{
		// Cycle through all players and end the intermission for them
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)UTIL_PlayerByIndex( i );
			if ( pPlayer  )
			{
				EndIntermission( pPlayer );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoAct::ActThink( void )
{
	m_OnTimerExpired.FireOutput( this,this );
}

//-----------------------------------------------------------------------------
// Purpose: Force the players not to move to give them time to read the act overlays
//-----------------------------------------------------------------------------
void CInfoAct::StartActOverlayTime( CBaseTFPlayer *pPlayer )
{
	// Lock the player in place
	pPlayer->CleanupOnActStart();
	pPlayer->LockPlayerInPlace();

	if ( pPlayer->GetActiveWeapon() )
	{
		pPlayer->GetActiveWeapon()->Holster();
	}

	pPlayer->m_Local.m_iHideHUD |= (HIDEHUD_WEAPONSELECTION | HIDEHUD_HEALTH);
	pPlayer->GetLocalData()->m_bForceMapOverview = true;
}

//-----------------------------------------------------------------------------
// Purpose: Release the players after overlay time has finished
//-----------------------------------------------------------------------------
void CInfoAct::EndActOverlayTime( CBaseTFPlayer *pPlayer )
{
	// Release the player
	pPlayer->UnlockPlayer();

	if ( pPlayer->GetActiveWeapon() )
	{
		pPlayer->GetActiveWeapon()->Deploy();
	}

	pPlayer->m_Local.m_iHideHUD &= ~(HIDEHUD_WEAPONSELECTION | HIDEHUD_HEALTH);
	pPlayer->GetLocalData()->m_bForceMapOverview = false;
}

//-----------------------------------------------------------------------------
// Purpose: Unlock the players after an act has started
//-----------------------------------------------------------------------------
void CInfoAct::ActThinkEndActOverlayTime( void )
{
	// Cycle through all players and end the intermission for them
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer  )
		{
			EndActOverlayTime( pPlayer );
		}
	}

	// Think again when the act ends, if we have a timelimit
	if ( m_flActTimeLimit )
	{
		SetNextThink( gpGlobals->curtime + m_flActTimeLimit - MIN_ACT_OVERLAY_TIME );
		SetThink( ActThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clean up entities before a new act starts
//-----------------------------------------------------------------------------
void CInfoAct::CleanupOnActStart( void )
{
	// Remove all resource chunks
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "resource_chunk" )) != NULL)
	{
		UTIL_Remove( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Intermission handling
//-----------------------------------------------------------------------------
void CInfoAct::StartIntermission( CBaseTFPlayer *pPlayer )
{
	// Do we have a camera point?
	if ( m_iszIntermissionCamera != NULL_STRING )
	{
		CBaseEntity *pCamera = gEntList.FindEntityByName( NULL, STRING(m_iszIntermissionCamera) );
		if ( pCamera )
		{
			// Move the player to the camera point
			pPlayer->SetViewEntity( pCamera );
			pPlayer->m_Local.m_iHideHUD |= (HIDEHUD_WEAPONSELECTION | HIDEHUD_HEALTH | HIDEHUD_MISCSTATUS);
		}
	}

	// Lock the player in place
	pPlayer->LockPlayerInPlace();
}

//-----------------------------------------------------------------------------
// Purpose: Intermission handling
//-----------------------------------------------------------------------------
void CInfoAct::EndIntermission( CBaseTFPlayer *pPlayer )
{
	// Force the player to respawn
	pPlayer->UnlockPlayer();
	pPlayer->SetViewEntity( pPlayer );
	pPlayer->ForceRespawn();
	pPlayer->m_Local.m_iHideHUD &= ~(HIDEHUD_WEAPONSELECTION | HIDEHUD_HEALTH | HIDEHUD_MISCSTATUS);
}

//-----------------------------------------------------------------------------
// Purpose: Force the act to start
//-----------------------------------------------------------------------------
void CInfoAct::InputStart( inputdata_t &inputdata )
{
	StartAct();
}

//-----------------------------------------------------------------------------
// Purpose: Force the act to finish, with team 1 as the winners
//-----------------------------------------------------------------------------
void CInfoAct::InputFinishWinNone( inputdata_t &inputdata )
{
	m_iWinners = 0;
	FinishAct();
}

//-----------------------------------------------------------------------------
// Purpose: Force the act to finish, with team 1 as the winners
//-----------------------------------------------------------------------------
void CInfoAct::InputFinishWin1( inputdata_t &inputdata )
{
	m_iWinners = 1;
	FinishAct();
}

//-----------------------------------------------------------------------------
// Purpose: Force the act to finish, with team 2 as the winners
//-----------------------------------------------------------------------------
void CInfoAct::InputFinishWin2( inputdata_t &inputdata )
{
	m_iWinners = 2;
	FinishAct();
}

//-----------------------------------------------------------------------------
// Purpose: Add time to the act's time
//-----------------------------------------------------------------------------
void CInfoAct::InputAddTime( inputdata_t &inputdata )
{
	float flNewTime = inputdata.value.Float();

	// Think again when the act ends, if we have a timelimit
	if ( flNewTime )
	{
		m_flActTimeLimit += flNewTime;
		SetNextThink( GetNextThink() + flNewTime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInfoAct::IsAWaitingAct( void )
{ 
	return HasSpawnFlags(SF_ACT_WAITINGFORGAMESTART);
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the current act (if any) is a waiting act.
//-----------------------------------------------------------------------------
bool CurrentActIsAWaitingAct( void )
{
	if ( g_hCurrentAct )
		return g_hCurrentAct->IsAWaitingAct();

	return false;
}