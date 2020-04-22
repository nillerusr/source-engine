//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// An RTS!
//=============================================================================

#include "legion.h"
#include <windows.h>
#include "inputsystem/iinputsystem.h"
#include "networksystem/inetworksystem.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui_controls/controls.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/AnimationController.h"
#include "gamemanager.h"
#include "menumanager.h"
#include "physicsmanager.h"
#include "rendermanager.h"
#include "uimanager.h"
#include "inputmanager.h"
#include "networkmanager.h"
#include "worldmanager.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	OutputDebugString( pMsg );
	if ( type == SPEW_ASSERT )
	{
		DebuggerBreak();
	}
	return SPEW_CONTINUE;
}


static CLegionApp s_LegionApp;
extern CLegionApp *g_pApp = &s_LegionApp;
DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR( CLegionApp, s_LegionApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CLegionApp::Create()
{
	SpewOutputFunc( SpewFunc );

	// FIXME: Put this into tier1librariesconnect
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false );

	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "networksystem.dll",		NETWORKSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}

bool CLegionApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pInputSystem || !g_pFullFileSystem || !g_pNetworkSystem || !g_pMaterialSystem || !g_pVGui || !g_pVGuiSurface || !g_pMatSystemSurface )
	{
		Warning( "Legion is missing a required interface!\n" );
		return false;
	}
	return true;
}


void CLegionApp::PostShutdown()
{
	BaseClass::PostShutdown();
}


bool CLegionApp::RegisterConCommandBase( ConCommandBase *pCommand )
{
	// Mark for easy removal
	pCommand->AddFlags( FCVAR_CLIENTDLL );
	pCommand->SetNext( 0 );

	// Link to variable list
	g_pCVar->RegisterConCommandBase( pCommand );
	return true;
}

void CLegionApp::UnregisterConCommandBase( ConCommandBase *pCommand )
{
	// Unlink from variable list
	g_pCVar->UnlinkVariable( pCommand );
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CLegionApp::Main()
{
	if (!SetVideoMode())
		return 0;

	ConCommandBaseMgr::OneTimeInit( this );

	g_pMaterialSystem->ModInit();

	// World database
	IGameManager::Add( g_pWorldManager );

	// Output
	IGameManager::Add( g_pRenderManager );

	// Input
	IGameManager::Add( g_pNetworkManager );
	IGameManager::Add( g_pInputManager );
	IGameManager::Add( g_pMenuManager );
	IGameManager::Add( g_pUIManager );

	// Simulation
	IGameManager::Add( g_pPhysicsManager );

	// Init the game managers
	if ( !IGameManager::InitAllManagers() )
		return 0;

	// First menu to start on
	g_pMenuManager->PushMenu( "MainMenu" );

	// This is the main game loop
	IGameManager::Start();

	// Shut down game systems
	IGameManager::ShutdownAllManagers();

 	g_pMaterialSystem->ModShutdown();

	g_pCVar->UnlinkVariables( FCVAR_CLIENTDLL );

	return 1;
}



