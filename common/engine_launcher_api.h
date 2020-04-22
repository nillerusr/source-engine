//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// engine/launcher interface
#ifndef ENGINE_LAUNCHER_APIH
#define ENGINE_LAUNCHER_APIH
#ifdef _WIN32
#pragma once
#endif

#include "appframework/IAppSystem.h"

class CAppSystemGroup;


struct StartupInfo_t
{
	void *m_pInstance;
	const char *m_pBaseDirectory;	// Executable directory ("c:/program files/half-life 2", for example)
	const char *m_pInitialMod;		// Mod name ("cstrike", for example)
	const char *m_pInitialGame;		// Root game name ("hl2", for example, in the case of cstrike)
	CAppSystemGroup *m_pParentAppSystemGroup;
	bool m_bTextMode;
};


//-----------------------------------------------------------------------------
// Return values from the initialization stage of the application framework
//-----------------------------------------------------------------------------
enum
{
	INIT_RESTART = INIT_LAST_VAL,
	RUN_FIRST_VAL,
};


//-----------------------------------------------------------------------------
// Return values from IEngineAPI::Run.
//-----------------------------------------------------------------------------
enum
{
	RUN_OK = RUN_FIRST_VAL,
	RUN_RESTART,
};


//-----------------------------------------------------------------------------
// Main engine interface to launcher + tools
//-----------------------------------------------------------------------------
#define VENGINE_LAUNCHER_API_VERSION "VENGINE_LAUNCHER_API_VERSION004"

abstract_class IEngineAPI : public IAppSystem
{
// Functions
public:
	// This function must be called before init
	virtual void SetStartupInfo( StartupInfo_t &info ) = 0;

	// Run the engine
	virtual int Run( ) = 0;

	// Sets the engine to run in a particular editor window
	virtual void SetEngineWindow( void *hWnd ) = 0;

	// Sets the engine to run in a particular editor window
	virtual void PostConsoleCommand( const char *pConsoleCommand ) = 0;

	// Are we running the simulation?
	virtual bool IsRunningSimulation( ) const = 0;

	// Start/stop running the simulation
	virtual void ActivateSimulation( bool bActive ) = 0;

	// Reset the map we're on
	virtual void SetMap( const char *pMapName ) = 0;

};


#endif // ENGINE_LAUNCHER_APIH
