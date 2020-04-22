//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface that the GameUI dll exports
//
// $NoKeywords: $
//===========================================================================//

#ifndef VGUI_BASEUI_INTERFACE_H
#define VGUI_BASEUI_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "ienginevgui.h"
#include "inputsystem/ButtonCode.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

//-----------------------------------------------------------------------------
// Foward declarations
//-----------------------------------------------------------------------------
class IMatSystemSurface;
class Color;
struct InputEvent_t;


//-----------------------------------------------------------------------------
// Global singleton interfaces related to VGUI 
//-----------------------------------------------------------------------------
extern IMatSystemSurface *g_pMatSystemSurface;

// enumeration of level loading progress bar spots
enum LevelLoadingProgress_e
{
	PROGRESS_NONE,
	PROGRESS_CHANGELEVEL,
	PROGRESS_SPAWNSERVER,
	PROGRESS_LOADWORLDMODEL,
	PROGRESS_CRCMAP,
	PROGRESS_CRCCLIENTDLL,
	PROGRESS_CREATENETWORKSTRINGTABLES,
	PROGRESS_PRECACHEWORLD,
	PROGRESS_CLEARWORLD,
	PROGRESS_LEVELINIT,
	PROGRESS_PRECACHE,
	PROGRESS_ACTIVATESERVER,
	PROGRESS_BEGINCONNECT,
	PROGRESS_SIGNONCHALLENGE,
	PROGRESS_SIGNONCONNECT,
	PROGRESS_SIGNONCONNECTED,
	PROGRESS_PROCESSSERVERINFO,
	PROGRESS_PROCESSSTRINGTABLE,
	PROGRESS_SIGNONNEW,
	PROGRESS_SENDCLIENTINFO,
	PROGRESS_SENDSIGNONDATA,
	PROGRESS_SIGNONSPAWN,
	PROGRESS_FULLYCONNECTED,
	PROGRESS_READYTOPLAY,
	PROGRESS_HIGHESTITEM,	// must be last item in list
};



//-----------------------------------------------------------------------------
// Purpose: Centerpoint for handling all user interface in the engine
//-----------------------------------------------------------------------------
abstract_class IEngineVGuiInternal : public IEngineVGui
{
public:

	virtual void Init() = 0;
	virtual void Connect() = 0;
	virtual void Shutdown() = 0;
	virtual bool SetVGUIDirectories() = 0;
	virtual bool IsInitialized() const = 0;
	virtual CreateInterfaceFn GetGameUIFactory() = 0;
	virtual bool Key_Event( const InputEvent_t &event ) = 0;
	virtual void BackwardCompatibility_Paint() = 0;
	virtual void UpdateButtonState( const InputEvent_t &event ) = 0;
	virtual void PostInit() = 0;

    virtual void Paint( PaintMode_t mode ) = 0;

	// handlers for game UI (main menu)
	virtual void ActivateGameUI() = 0;
	virtual bool HideGameUI() = 0;
	virtual bool IsGameUIVisible() = 0;

	// console
	virtual void ShowConsole() = 0;
	virtual void HideConsole() = 0;
	virtual bool IsConsoleVisible() = 0;
	virtual void ClearConsole() = 0;

	virtual void HideDebugSystem() = 0;

	// level loading
	virtual void OnLevelLoadingStarted() = 0;
	virtual void OnLevelLoadingFinished() = 0;
	virtual void NotifyOfServerConnect(const char *game, int IP, int connectionPort, int queryPort) = 0;
	virtual void NotifyOfServerDisconnect() = 0;
	virtual void EnabledProgressBarForNextLoad() = 0;
	virtual void UpdateProgressBar(LevelLoadingProgress_e progress) = 0;
	virtual void UpdateCustomProgressBar( float progress, const wchar_t *desc ) = 0;
	virtual void StartCustomProgress() = 0;
	virtual void FinishCustomProgress() = 0;
	virtual void ShowErrorMessage() = 0;

	// Should pause?
	virtual bool ShouldPause() = 0;
	virtual void SetGameDLLPanelsVisible( bool show ) = 0;
	virtual void ShowNewGameDialog( int chapter ) = 0;

	virtual void Simulate() = 0;

	virtual void SetNotAllowedToHideGameUI( bool bNotAllowedToHide ) = 0;
	virtual void SetNotAllowedToShowGameUI( bool bNotAllowedToShow ) = 0;

	// Xbox 360
	virtual void SessionNotification( const int notification, const int param = 0 ) = 0;
	virtual void SystemNotification( const int notification ) = 0;
	virtual void ShowMessageDialog( const uint nType, vgui::Panel *pOwner = NULL ) = 0;
	virtual void UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost ) = 0;
	virtual void SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping ) = 0;
	virtual void OnCreditsFinished( void ) = 0;

	// Storage device validation:
	//		returns true right away if storage device has been previously selected.
	//		otherwise returns false and will set the variable pointed by pStorageDeviceValidated to 1
	//				  once the storage device is selected by user.
	virtual bool ValidateStorageDevice( int *pStorageDeviceValidated ) = 0;

	virtual void ConfirmQuit( void ) = 0;
};

// Purpose: singleton accessor
extern IEngineVGuiInternal *EngineVGui();

// Purpose: Play a sound
void VGui_PlaySound(const char *pFileName);

#endif // VGUI_BASEUI_INTERFACE_H
