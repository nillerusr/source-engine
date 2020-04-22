//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Round timer for dod gamerules
//
//=============================================================================//

#ifndef DOD_ROUND_TIMER_H
#define DOD_ROUND_TIMER_H

#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#define CDODRoundTimer C_DODRoundTimer
#endif

class CDODRoundTimer : public CBaseEntity
{
public:
	DECLARE_CLASS( CDODRoundTimer, CBaseEntity );
	DECLARE_NETWORKCLASS();

	// Constructor
	CDODRoundTimer();

	// Destructor
	virtual ~CDODRoundTimer();

	// Set the initial length of the timer
	void SetTimeRemaining( int iTimerSeconds );

	// Add time to an already running ( or paused ) timer
	void AddTimerSeconds( int iSecondsToAdd );

	void PauseTimer( void );
	void ResumeTimer( void );

	// Returns seconds to display.
	// When paused shows amount of time left once the timer is resumed
	float GetTimeRemaining( void );

	int GetTimerMaxLength( void );

#ifndef CLIENT_DLL

	int UpdateTransmitState();

#else

	void InternalSetPaused( bool bPaused ) { m_bTimerPaused = bPaused; }

#endif	

private:
	CNetworkVar( bool, m_bTimerPaused );
	CNetworkVar( float, m_flTimeRemaining );
	CNetworkVar( float, m_flTimerEndTime );

	int m_iTimerMaxLength;	// Sum of starting duration plus any time we added
};

#ifdef CLIENT_DLL
	extern CDODRoundTimer *g_DODRoundTimer;
#endif

#endif	//DOD_ROUND_TIMER_H