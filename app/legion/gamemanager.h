//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// State we are in
//-----------------------------------------------------------------------------
enum LevelState_t
{
	NOT_IN_LEVEL = 0,
	LOADING_LEVEL,
	IN_LEVEL,
	SHUTTING_DOWN_LEVEL,
};


//-----------------------------------------------------------------------------
// State we are in
//-----------------------------------------------------------------------------
enum LevelRetVal_t
{
	FAILED = 0,
	MORE_WORK,
	FINISHED,
};


//-----------------------------------------------------------------------------
// Tick interval
//-----------------------------------------------------------------------------
#define TICK_INTERVAL	0.015f


//-----------------------------------------------------------------------------
// Game managers are singleton objects in the game code responsible for various tasks
// The order in which the server systems appear in this list are the
// order in which they are initialized and updated. They are shut down in
// reverse order from which they are initialized.
//-----------------------------------------------------------------------------
abstract_class IGameManager
{
public:
	// GameManagers are expected to implement these methods.
	virtual bool Init() = 0;
	virtual LevelRetVal_t LevelInit( bool bFirstCall ) = 0;
	virtual void Update( ) = 0;
	virtual LevelRetVal_t LevelShutdown( bool bFirstCall ) = 0;
	virtual void Shutdown() = 0;
	
	// Called during game save
	virtual void OnSave() = 0;

	// Called during game restore
	virtual void OnRestore() = 0;

	// This this game manager involved in I/O or simulation?
	virtual bool PerformsSimulation() = 0;

	// Add, remove game managers
	static void Add( IGameManager* pSys );
	static void Remove( IGameManager* pSys );
	static void RemoveAll( );

	// Init, shutdown game managers
	static bool InitAllManagers();
	static void ShutdownAllManagers();

	// Start, stop running game managers
	static void Start ();
	static void Stop ();
	static int FrameNumber();

	// Used in simulation
	static float CurrentSimulationTime();
	static float SimulationDeltaTime();

	// Used in rendering
	static float CurrentTime();
	static float DeltaTime();

	// Start loading a level
	static void StartNewLevel();
	static void ShutdownLevel();
	static LevelState_t GetLevelState();

protected:
	// Updates the state machine related to loading levels
	static void UpdateLevelStateMachine();	 

	virtual ~IGameManager() {}

	typedef LevelRetVal_t (IGameManager::*GameManagerLevelFunc_t)( bool bFirstCall );
	typedef bool (IGameManager::*GameManagerInitFunc_t)();
	typedef void (IGameManager::*GameManagerFunc_t)();

	// Used to invoke a method of all added game managers in order
	static void InvokeMethod( GameManagerFunc_t f );
	static void InvokeMethodReverseOrder( GameManagerFunc_t f );
	static bool InvokeMethod( GameManagerInitFunc_t f );
	static LevelRetVal_t InvokeLevelMethod( GameManagerLevelFunc_t f, bool bFirstCall );
	static LevelRetVal_t InvokeLevelMethodReverseOrder( GameManagerLevelFunc_t f, bool bFirstCall );

	static bool m_bLevelShutdownRequested;
	static bool m_bLevelStartRequested;
	static bool m_bStopRequested;
	static CUtlVector< IGameManager* > m_GameManagers;
	static bool m_bIsRunning;
	static bool m_bIsInitialized;
	static int  m_nFrameNumber;
	static float m_flCurrentTime;
	static float m_flLastTime;
	static LevelState_t m_LevelState;
};


//-----------------------------------------------------------------------------
// Default decorator base-class for IGameManager
//-----------------------------------------------------------------------------
template< class BaseClass = IGameManager >
class CGameManager : public BaseClass
{
public:
	virtual ~CGameManager();

	// GameManagers are expected to implement these methods.
	// NOTE: If Init or LevelInit fail, they are expected to call 
	virtual bool Init() { return true; }
	virtual LevelRetVal_t LevelInit( bool bFirstCall ) { return FINISHED; }
	virtual void Update( ) {}
	virtual LevelRetVal_t LevelShutdown( bool bFirstCall ) { return FINISHED; }
	virtual void Shutdown() {}
	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual bool PerformsSimulation() { return false; }
};


//-----------------------------------------------------------------------------
// Automatically remove the game system if it gets deleted.
//-----------------------------------------------------------------------------
template< class BaseClass >
inline CGameManager< BaseClass >::~CGameManager()
{
	Remove( this );
}


#endif // GAMEMANAGER_H
