//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BasePanel.h"
#include "LoadGameDialog.h"

#include "winlite.h"
#include "vgui/ISurface.h"

#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "ixboxsystem.h"
#include "filesystem.h"

#include "SaveGameBrowserDialog.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CLoadGameDialogXbox::CLoadGameDialogXbox( vgui::Panel *parent ) : BaseClass( parent )
{
	m_bFilterAutosaves = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	vgui::Label *pTitle = (Label *) FindChildByName( "TitleLabel" );
	if ( pTitle )
	{
		pTitle->SetText( "#GameUI_LoadGame" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::PerformSelectedAction( void )
{
	BaseClass::PerformSelectedAction();
	
	if ( !GetNumPanels() )
		return;

	SetControlDisabled( true );

	// Warn the player if they're already in a map
	if ( !GameUI().HasSavedThisMenuSession() && GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
	{
		BasePanel()->ShowMessageDialog( MD_SAVE_BEFORE_LOAD, this );
	}
	else
	{
		// Otherwise just do it
		OnCommand( "LoadGame" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::PerformDeletion( void )
{
	// Cannot delete autosaves!
	CGameSavePanel *pPanel = GetActivePanel();
	if ( pPanel == NULL || ( pPanel && pPanel->IsAutoSaveType() ) )
		return;

	BaseClass::PerformDeletion();

	SetControlDisabled( true );

	vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
	BasePanel()->ShowMessageDialog( MD_DELETE_SAVE_CONFIRM, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bNewSaveSelected - 
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::UpdateFooterOptions( void )
{
	CFooterPanel *pFooter = GetFooterPanel();

	// Show available buttons
	pFooter->ClearButtons();

	// Make sure we have panels to show
	if ( HasActivePanels() )
	{
		pFooter->AddNewButtonLabel( "#GameUI_Load", "#GameUI_Icons_A_BUTTON" );
		
		// Don't allow deletions of autosaves!
		CGameSavePanel *pPanel = GetActivePanel();
		if ( pPanel && pPanel->IsAutoSaveType() == false )
		{
			pFooter->AddNewButtonLabel( "#GameUI_Delete", "#GameUI_Icons_X_BUTTON" );
		}
	}

	// Always allow storage devices changes and cancelling
	pFooter->AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
	pFooter->AddNewButtonLabel( "#GameUI_Console_StorageChange", "#GameUI_Icons_Y_BUTTON" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::OnCommand( const char *command )
{
	m_KeyRepeat.Reset();

	if ( !Q_stricmp( command, "LoadGame" ) )
	{
		// Must have an active panel to perform this action
		if ( GetNumPanels() == 0 )
		{
			SetControlDisabled( false );
			return;
		}

		const SaveGameDescription_t *pSave = GetActivePanelSaveDescription();

		// Load the saved game
		char szCmd[ 256 ];
		Q_snprintf( szCmd, sizeof( szCmd ), "xload %s", pSave->szShortName );
		engine->ClientCmd_Unrestricted( szCmd );

		// Ignore all other input while we're open
		OnClose();	
	}
	else if ( !Q_stricmp( command, "DeleteGame" ) )
	{
		// Must have an active panel to perform this action
		if ( GetNumPanels() == 0 )
		{
			SetControlDisabled( false );
			return;
		}

		// Delete the game they've selected
		const SaveGameDescription_t *pSave = GetActivePanelSaveDescription();
		DeleteSaveGame( pSave );
		RemoveActivePanel();
	}
	else if ( !Q_stricmp( command, "RefreshSaveGames" ) )
	{
		// FIXME: At this point the rug has been pulled out from undereath us!
		RefreshSaveGames();
	}
	else if ( !Q_stricmp( command, "LoadGameCancelled" ) )
	{
		SetControlDisabled( false );
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel( NULL );
	}
	else if ( !Q_stricmp( command, "DeleteGameCancelled" ) )
	{
		SetControlDisabled( false );
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes an existing save game
//-----------------------------------------------------------------------------
void CLoadGameDialogXbox::DeleteSaveGame( const SaveGameDescription_t *pSaveDesc )
{
	if ( pSaveDesc == NULL )
	{
		SetControlDisabled( false );
		return;
	}

	// If we're deleting our more recent save game, we need to make sure we setup the engine to properly load the last most recent
	if ( Q_stristr( engine->GetMostRecentSaveGame(), pSaveDesc->szShortName ) )
	{
		// We must have at least two active save games that we know about
		if ( GetNumPanels() > 1 )
		{
			// The panels are sorted by how recent they are, so the first element is the most recent
			const SaveGameDescription_t *pDesc = GetPanelSaveDecription( 0 );
			if ( pDesc == pSaveDesc )
			{
				// If we're deleting our most recent, we need to pick the next most recent
				pDesc = GetPanelSaveDecription( 1 );
			}

			// Remember this filename for the next time we need to reload
			if ( pDesc )
			{
				engine->SetMostRecentSaveGame( pDesc->szShortName );
			}
		}
	}

	// Delete the save game file
	g_pFullFileSystem->RemoveFile( pSaveDesc->szFileName, "MOD" );

	vgui::surface()->PlaySound( "UI/buttonclick.wav" );

	// Return control
	SetControlDisabled( false );
}
