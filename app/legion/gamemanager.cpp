//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: List of game managers to update  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "gamemanager.h"
#include "tier0/icommandline.h"

// FIXME: REMOVE (for Sleep)
#include <windows.h>


//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
int IGameManager::m_nFrameNumber = 0;
bool IGameManager::m_bStopRequested = false;
bool IGameManager::m_bIsRunning = false;
bool IGameManager::m_bIsInitialized = false;
bool IGameManager::m_bLevelStartRequested = false;
bool IGameManager::m_bLevelShutdownRequested = false;
float IGameManager::m_flCurrentTime = 0.0f;
float IGameManager::m_flLastTime = 0.0f;
LevelState_t IGameManager::m_LevelState = NOT_IN_LEVEL;

CUtlVector< IGameManager* > IGameManager::m_GameManagers;


//-----------------------------------------------------------------------------
// Adds a system to the list of systems to run
//-----------------------------------------------------------------------------
void IGameManager::Add( IGameManager* pSys )
{
	Assert( !m_bIsRunning );
	m_GameManagers.AddToTail( pSys );
}


//-----------------------------------------------------------------------------
// Removes a system from the list of systems to update
//-----------------------------------------------------------------------------
void IGameManager::Remove( IGameManager* pSys )
{
	Assert( !m_bIsRunning );
	m_GameManagers.FindAndRemove( pSys );
}


//-----------------------------------------------------------------------------
// Removes *all* systems from the list of systems to update
//-----------------------------------------------------------------------------
void IGameManager::RemoveAll( )
{
	m_GameManagers.RemoveAll();
}


//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void IGameManager::InvokeMethod( GameManagerFunc_t f )
{
	int i;
	int nCount = m_GameManagers.Count();
	for ( i = 0; i < nCount; ++i )
	{
		(m_GameManagers[i]->*f)();
	}
}

void IGameManager::InvokeMethodReverseOrder( GameManagerFunc_t f )
{
	int i;
	int nCount = m_GameManagers.Count();
	for ( i = nCount; --i >= 0; )
	{
		(m_GameManagers[i]->*f)();
	}
}

bool IGameManager::InvokeMethod( GameManagerInitFunc_t f )
{
	int i;
	int nCount = m_GameManagers.Count();
	for ( i = 0; i < nCount; ++i )
	{
		if ( !(m_GameManagers[i]->*f)() )
			return false;
	}
	return true;
}

LevelRetVal_t IGameManager::InvokeLevelMethod( GameManagerLevelFunc_t f, bool bFirstCall )
{
	LevelRetVal_t nRetVal = FINISHED;
	int i;
	int nCount = m_GameManagers.Count();
	for ( i = 0; i < nCount; ++i )
	{
		LevelRetVal_t val = (m_GameManagers[i]->*f)( bFirstCall );
		if ( val == FAILED )
			return FAILED;
		if ( val == MORE_WORK )
		{
			nRetVal = MORE_WORK;
		}
	}
	return nRetVal;
}

LevelRetVal_t IGameManager::InvokeLevelMethodReverseOrder( GameManagerLevelFunc_t f, bool bFirstCall )
{
	LevelRetVal_t nRetVal = FINISHED;
	int i;
	int nCount = m_GameManagers.Count();
	for ( i = 0; i < nCount; ++i )
	{
		LevelRetVal_t val = ( m_GameManagers[i]->*f )( bFirstCall );
		if ( val == FAILED )
		{
			nRetVal = FAILED;
		}
		if ( ( val == MORE_WORK ) && ( nRetVal != FAILED ) )
		{
			nRetVal = MORE_WORK;
		}
	}
	return nRetVal;
}


//-----------------------------------------------------------------------------
// Init, shutdown game system
//-----------------------------------------------------------------------------
bool IGameManager::InitAllManagers()
{
	m_nFrameNumber = 0;
	if ( !InvokeMethod( &IGameManager::Init ) )
		return false;

	m_bIsInitialized = true;
	return true;
}

void IGameManager::ShutdownAllManagers()
{
	if ( m_bIsInitialized )
	{
		InvokeMethodReverseOrder( &IGameManager::Shutdown );
		m_bIsInitialized = false;
	}
}


//-----------------------------------------------------------------------------
// Updates the state machine related to loading levels
//-----------------------------------------------------------------------------
void IGameManager::UpdateLevelStateMachine()
{
	// Do we want to switch into the level shutdown state?
	bool bFirstLevelShutdownFrame = false;
	if ( m_bLevelShutdownRequested )
	{
		if ( m_LevelState != LOADING_LEVEL )
		{
			m_bLevelShutdownRequested = false;
		}
		if ( m_LevelState == IN_LEVEL )
		{
			m_LevelState = SHUTTING_DOWN_LEVEL;
			bFirstLevelShutdownFrame = true;
		}
	}

	// Perform level shutdown
	if ( m_LevelState == SHUTTING_DOWN_LEVEL )
	{
		LevelRetVal_t val = InvokeLevelMethodReverseOrder( &IGameManager::LevelShutdown, bFirstLevelShutdownFrame );
		if ( val != MORE_WORK )
		{
			m_LevelState = NOT_IN_LEVEL;
		}
	}

	// Do we want to switch into the level startup state?
	bool bFirstLevelStartFrame = false;
	if ( m_bLevelStartRequested )
	{
		if ( m_LevelState != SHUTTING_DOWN_LEVEL )
		{
			m_bLevelStartRequested = false;
		}
		if ( m_LevelState == NOT_IN_LEVEL )
		{
			m_LevelState = LOADING_LEVEL;
			bFirstLevelStartFrame = true;
		}
	}

	// Perform level load
	if ( m_LevelState == LOADING_LEVEL )
	{
		LevelRetVal_t val = InvokeLevelMethod( &IGameManager::LevelInit, bFirstLevelStartFrame );
		if ( val == FAILED )
		{
			m_LevelState = NOT_IN_LEVEL;
		}
		else if ( val == FINISHED )
		{
			m_LevelState = IN_LEVEL;
		}
	}
}


//-----------------------------------------------------------------------------
// Runs the main loop.
//-----------------------------------------------------------------------------
void IGameManager::Start()
{
	Assert( !m_bIsRunning && m_bIsInitialized );

	m_bIsRunning = true;
	m_bStopRequested = false;

	// This option is useful when running the app twice on the same machine
	// It makes the 2nd instance of the app run a lot faster
	bool bPlayNice = ( CommandLine()->CheckParm( "-yieldcycles" ) != 0 );

	float flStartTime = m_flCurrentTime = m_flLastTime = Plat_FloatTime();
	int nFramesSimulated = 0;
	int nCount = m_GameManagers.Count();
	while ( !m_bStopRequested )
	{
		UpdateLevelStateMachine();

		m_flLastTime = m_flCurrentTime;
		m_flCurrentTime = Plat_FloatTime();
		int nSimulationFramesNeeded = 1 + (int)( ( m_flCurrentTime - flStartTime ) / TICK_INTERVAL );
		while( nSimulationFramesNeeded > nFramesSimulated )
		{
			for ( int i = 0; i < nCount; ++i )
			{
				if ( m_GameManagers[i]->PerformsSimulation() )
				{
					m_GameManagers[i]->Update();
				}
			}
			++m_nFrameNumber;
			++nFramesSimulated;
		}

		// Always do I/O related managers regardless of framerate
		for ( int i = 0; i < nCount; ++i )
		{
			if ( !m_GameManagers[i]->PerformsSimulation() )
			{
				m_GameManagers[i]->Update();
			}
		}

		if ( bPlayNice )
		{
			Sleep( 1 );
		}
	}

	m_bIsRunning = false;
}


//-----------------------------------------------------------------------------
// Stops the main loop at the next appropriate time
//-----------------------------------------------------------------------------
void IGameManager::Stop()
{
	if ( m_bIsRunning )
	{
		m_bStopRequested = true;
	}
}


//-----------------------------------------------------------------------------
// Returns the current frame number
//-----------------------------------------------------------------------------
int IGameManager::FrameNumber()
{
	return m_nFrameNumber;
}

float IGameManager::CurrentSimulationTime()
{
	return m_nFrameNumber * TICK_INTERVAL;
}

float IGameManager::SimulationDeltaTime()
{
	return TICK_INTERVAL;
}


//-----------------------------------------------------------------------------
// Used in rendering
//-----------------------------------------------------------------------------
float IGameManager::CurrentTime()
{
	return m_flCurrentTime;
}

float IGameManager::DeltaTime()
{
	return m_flCurrentTime - m_flLastTime;
}


//-----------------------------------------------------------------------------
// Returns the current level state
//-----------------------------------------------------------------------------
LevelState_t IGameManager::GetLevelState()
{
	return m_LevelState;
}


//-----------------------------------------------------------------------------
// Start loading a level
//-----------------------------------------------------------------------------
void IGameManager::StartNewLevel()
{
	m_bLevelShutdownRequested = true;
	m_bLevelStartRequested = true;
}

void IGameManager::ShutdownLevel()
{
	m_bLevelShutdownRequested = true;
}

