//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dod gamerules round timer 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_round_timer.h"

#ifdef CLIENT_DLL

	#include "iclientmode.h"
	#include "vgui_controls/AnimationController.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL

	// Use this proxy to flash the round timer whenever the timer is restarted
	// because trapping the round start event doesn't work ( the event also flushes
	// all hud events and obliterates our TimerFlash event )
	static void RecvProxy_TimerPaused( const CRecvProxyData *pData, void *pStruct, void *pOut )
	{
		CDODRoundTimer *pTimer = (CDODRoundTimer *) pStruct;

		bool bTimerPaused = ( pData->m_Value.m_Int > 0 );

		if ( bTimerPaused == false )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TimerFlash" ); 
		}

		pTimer->InternalSetPaused( bTimerPaused );
	}

#endif

LINK_ENTITY_TO_CLASS( dod_round_timer, CDODRoundTimer );

IMPLEMENT_NETWORKCLASS_ALIASED( DODRoundTimer, DT_DODRoundTimer )

BEGIN_NETWORK_TABLE_NOBASE( CDODRoundTimer, DT_DODRoundTimer )
	#ifdef CLIENT_DLL

		RecvPropInt( RECVINFO( m_bTimerPaused ), 0, RecvProxy_TimerPaused ),
		RecvPropTime( RECVINFO( m_flTimeRemaining ) ),
		RecvPropTime( RECVINFO( m_flTimerEndTime ) ),

	#else

		SendPropBool( SENDINFO( m_bTimerPaused ) ),
		SendPropTime( SENDINFO( m_flTimeRemaining ) ),
		SendPropTime( SENDINFO( m_flTimerEndTime ) ),

	#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
	CDODRoundTimer *g_DODRoundTimer = NULL;
#endif

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CDODRoundTimer::CDODRoundTimer( void )
{
#ifndef CLIENT_DLL
	m_bTimerPaused = true;
	m_flTimeRemaining = 0;
	m_iTimerMaxLength = 0;
#else
	g_DODRoundTimer = this;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CDODRoundTimer::~CDODRoundTimer( void )
{
#ifdef CLIENT_DLL
	g_DODRoundTimer = NULL;
#endif
}

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: The timer is always transmitted to clients
//-----------------------------------------------------------------------------
int CDODRoundTimer::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: To set the initial timer duration
//-----------------------------------------------------------------------------
void CDODRoundTimer::SetTimeRemaining( int iTimerSeconds )
{
	m_flTimeRemaining = (float)iTimerSeconds;
	m_flTimerEndTime = gpGlobals->curtime + m_flTimeRemaining;
	m_iTimerMaxLength = iTimerSeconds;
}

//-----------------------------------------------------------------------------
// Purpose: Timer is paused at round end, stops the countdown
//-----------------------------------------------------------------------------
void CDODRoundTimer::PauseTimer( void )
{
	if ( m_bTimerPaused == false )
	{
		m_bTimerPaused = true;

		m_flTimeRemaining = m_flTimerEndTime - gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: To start or re-start the timer after a pause
//-----------------------------------------------------------------------------
void CDODRoundTimer::ResumeTimer( void )
{
	if ( m_bTimerPaused == true )
	{
		m_bTimerPaused = false;

		m_flTimerEndTime = gpGlobals->curtime + m_flTimeRemaining;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the seconds left on the timer, paused or not.
//-----------------------------------------------------------------------------
float CDODRoundTimer::GetTimeRemaining( void )
{
	float flSecondsRemaining;

	if ( m_bTimerPaused )
	{
		flSecondsRemaining = m_flTimeRemaining;
	}
	else
	{
		flSecondsRemaining = m_flTimerEndTime - gpGlobals->curtime;
	}

	if ( flSecondsRemaining < 0 )
		flSecondsRemaining = 0;

	return flSecondsRemaining;
}

//-----------------------------------------------------------------------------
// Purpose: Add seconds to the timer while it is running or paused
//-----------------------------------------------------------------------------
void CDODRoundTimer::AddTimerSeconds( int iSecondsToAdd )
{
	// do a hud animation indicating that time has been added

	if ( m_bTimerPaused )
	{
		m_flTimeRemaining += (float)iSecondsToAdd;
	}
	else
	{
		m_flTimerEndTime += (float)iSecondsToAdd;
	}

	m_iTimerMaxLength += iSecondsToAdd;
}

int CDODRoundTimer::GetTimerMaxLength( void )
{
	return m_iTimerMaxLength;
}
