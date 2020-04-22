//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BasePanel.h"
#include "SaveGameDialog.h"

#include "winlite.h"		// FILETIME
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "ModInfo.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "vstdlib/random.h"

#include "BaseSaveGameDialog.h"
#include "SaveGameBrowserDialog.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include "vgui_controls/Frame.h"
#include "utlvector.h"

extern const char *COM_GetModDirectory();

using namespace vgui;

CSaveGameDialogXbox::CSaveGameDialogXbox( vgui::Panel *parent ) 
:	BaseClass( parent ),
	m_bGameSaving ( false ),
	m_bNewSaveAvailable( false )
{
	m_bFilterAutosaves = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::PerformSelectedAction( void )
{
	BaseClass::PerformSelectedAction();

	// If there are no panels, don't allow this
	if ( GetNumPanels() == 0 )
		return;

	SetControlDisabled( true );

	// Decide if this is an overwrite or a new save game
	bool bNewSave = ( GetActivePanelIndex() == 0 ) && m_bNewSaveAvailable;
	if ( bNewSave )
	{
		OnCommand( "SaveGame" );
	}
	else
	{
		BasePanel()->ShowMessageDialog( MD_SAVE_OVERWRITE, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bNewSaveSelected - 
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::UpdateFooterOptions( void )
{
	CFooterPanel *pFooter = GetFooterPanel();

	// Show available buttons
	pFooter->ClearButtons();

	bool bSavePanelsActive = ( GetNumPanels() != 0 );
	if ( bSavePanelsActive )
	{
		bool bNewSaveSelected = ( GetActivePanelIndex() == 0 ) && m_bNewSaveAvailable;
		if ( bNewSaveSelected )
		{
			pFooter->AddNewButtonLabel( "#GameUI_SaveGame_NewSave", "#GameUI_Icons_A_BUTTON" );
		}
		else
		{
			pFooter->AddNewButtonLabel( "#GameUI_SaveGame_Overwrite", "#GameUI_Icons_A_BUTTON" );
		}
	}

	// Always available
	pFooter->AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
	pFooter->AddNewButtonLabel( "#GameUI_Console_StorageChange", "#GameUI_Icons_Y_BUTTON" );
}

//-----------------------------------------------------------------------------
// Purpose: perfrom the save on a separate thread
//-----------------------------------------------------------------------------
class CAsyncCtxSaveGame : public CBasePanel::CAsyncJobContext
{
public:
	explicit CAsyncCtxSaveGame( CSaveGameDialogXbox *pDlg );
	~CAsyncCtxSaveGame() {}

public:
	virtual void ExecuteAsync();
	virtual void Completed();

public:
	char m_szFilename[MAX_PATH];
	CSaveGameDialogXbox *m_pSaveGameDlg;
};

CAsyncCtxSaveGame::CAsyncCtxSaveGame( CSaveGameDialogXbox *pDlg ) :
	CBasePanel::CAsyncJobContext( 3.0f ),	// Storage device info for at least 3 seconds
	m_pSaveGameDlg( pDlg )
{
	NULL;
}

void CAsyncCtxSaveGame::ExecuteAsync()
{
	// Sit and wait for the async save to finish
	for ( ; ; )
	{
		if ( !engine->IsSaveInProgress() )
			// Save operation is no longer in progress
			break;
		else
			ThreadSleep( 50 );
	}
}

void CAsyncCtxSaveGame::Completed()
{
	m_pSaveGameDlg->SaveCompleted( this );
}

//-----------------------------------------------------------------------------
// Purpose: kicks off the async save (called on the main thread)
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::InitiateSaving()
{
	// Determine whether this is a new save or overwrite
	bool bNewSave = ( GetActivePanelIndex() == 0 ) && m_bNewSaveAvailable;

	// Allocate the async context for saving
	CAsyncCtxSaveGame *pAsyncCtx = new CAsyncCtxSaveGame( this );

	// If this is an overwrite then there was an overwrite warning displayed
	if ( !bNewSave )
		BasePanel()->CloseMessageDialog( DIALOG_STACK_IDX_WARNING );
	// Now display the saving warning
	BasePanel()->ShowMessageDialog( MD_SAVING_WARNING, this );

	// Kick off saving
	char *szFilename = pAsyncCtx->m_szFilename;
	const int maxFilenameLen = sizeof( pAsyncCtx->m_szFilename );
	char szCmd[MAX_PATH];

	// See if this is the "new save game" slot
	if ( bNewSave )
	{
		// Create a new save game (name is created from the current time, which should be pretty unique)
#ifdef WIN32
		FILETIME currentTime;
		GetSystemTimeAsFileTime( &currentTime );
		Q_snprintf( szFilename, maxFilenameLen, "%s_%u", COM_GetModDirectory(), currentTime.dwLowDateTime );
#else
		time_t currentTime = time( NULL );
		Q_snprintf( szFilename, maxFilenameLen, "%s_%u", COM_GetModDirectory(), (unsigned)currentTime );
#endif
		Q_snprintf( szCmd, sizeof( szCmd ), "xsave %s", szFilename );
		engine->ExecuteClientCmd( szCmd );
		Q_strncat( szFilename, ".360.sav", maxFilenameLen );
	}
	else
	{
		const SaveGameDescription_t *pDesc = GetActivePanelSaveDescription();
		Q_strncpy( szFilename, pDesc->szShortName, maxFilenameLen );
		Q_snprintf( szCmd, sizeof( szCmd ), "xsave %s", szFilename );
		engine->ExecuteClientCmd( szCmd );
	}

	// Enqueue waiting
	BasePanel()->ExecuteAsync( pAsyncCtx );
}

//-----------------------------------------------------------------------------
// Purpose: handles the end of async save (called on the main thread)
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::SaveCompleted( CAsyncCtxSaveGame *pCtx )
{
	char const *szFilename = pCtx->m_szFilename;

	// We should now be saved so get the new desciption back from the file
	char szDirectory[MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "%s:/%s", COM_GetModDirectory(), szFilename );

	ParseSaveData( szDirectory, szFilename, &m_NewSaveDesc );

	// Close the progress dialog
	BasePanel()->CloseMessageDialog( DIALOG_STACK_IDX_WARNING );

	bool bNewSave = ( GetActivePanelIndex() == 0 ) && m_bNewSaveAvailable;
	if ( bNewSave )
	{
		AnimateInsertNewPanel( &m_NewSaveDesc );
	}
	else
	{
		AnimateOverwriteActivePanel( &m_NewSaveDesc );
	}

	m_bGameSaving = false;
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::OnCommand( const char *command )
{
	m_KeyRepeat.Reset();

	if ( !Q_stricmp( command, "SaveGame" ) )
	{
		if ( m_bGameSaving )
			return;
		m_bGameSaving = true;

		SetControlDisabled( true );

		// Initiate the saving operation
		InitiateSaving();
	}
	else if ( !Q_stricmp( command, "SaveSuccess" ) )
	{
		vgui::surface()->PlaySound( "UI/buttonclick.wav" );
		GameUI().SetSavedThisMenuSession( true );
	}
	else if ( !Q_stricmp( command, "CloseAndSelectResume" ) )
	{
		BasePanel()->ArmFirstMenuItem();
		OnCommand( "Close" );
	}
	else if ( !Q_stricmp( command, "OverwriteGameCancelled" ) )
	{
		SetControlDisabled( false );
	}
	else if ( !Q_stricmp( command, "RefreshSaveGames" ) )
	{
		RefreshSaveGames();
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel( NULL );
	}
	else if ( !m_bGameSaving )
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: On completion of scanning, prepend a utility slot on the stack
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::OnDoneScanningSaveGames( void )
{
	ConVarRef save_history_count("save_history_count" );

	m_bNewSaveAvailable = false;
#ifdef _X360
	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
		return;

	// We only allow 10 save games minus the number of autosaves, autosavedangerous, and autosave0?'s at once
	if ( GetNumPanels() >= 10 - ( 2 + (unsigned)save_history_count.GetInt() ) )
		return;

	if ( GetStorageSpaceUsed() + XBX_SAVEGAME_BYTES > XBX_PERSISTENT_BYTES_NEEDED )
		return;

	m_bNewSaveAvailable = true;
	SaveGameDescription_t bogusDesc = { "#GameUI_SaveGame_NewSavedGame", "#GameUI_SaveGame_NewSave", "#GameUI_SaveGame_NewSave", "#GameUI_SaveGame_NewSave", "#GameUI_SaveGame_NewSave", "#GameUI_SaveGame_NewSave", "#GameUI_SaveGame_NewSave", 0, 0 };
	CGameSavePanel *newSavePanel = SETUP_PANEL( new CGameSavePanel( this, &bogusDesc, true ) );
	AddPanel( newSavePanel );
#endif // _X360
}
