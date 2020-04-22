//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "SaveGameDialog.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"

#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/QueryBox.h"

#include "KeyValues.h"
#include "filesystem.h"

#include <stdio.h>
#include <stdlib.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NEW_SAVE_GAME_TIMESTAMP	0xFFFFFFFF

//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CSaveGameDialog::CSaveGameDialog(vgui::Panel *parent) : BaseClass(parent, "SaveGameDialog")
{
	SetDeleteSelfOnClose(true);
	SetBounds(0, 0, 512, 384);
	SetSizeable( true );

	SetTitle("#GameUI_SaveGame", true);

	vgui::Button *cancel = new vgui::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	LoadControlSettings("Resource\\SaveGameDialog.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSaveGameDialog::~CSaveGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Saves game
//-----------------------------------------------------------------------------
void CSaveGameDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "loadsave" )  )
	{
		int saveIndex = GetSelectedItemSaveIndex();
		if ( m_SaveGames.IsValidIndex(saveIndex) )
		{
			// see if we're overwriting
			if ( m_SaveGames[saveIndex].iTimestamp == NEW_SAVE_GAME_TIMESTAMP )
			{
				// new save game, proceed
				OnCommand( "SaveOverwriteConfirmed" );
			}
			else
			{
				// open confirmation dialog
				QueryBox *box = new QueryBox( "#GameUI_ConfirmOverwriteSaveGame_Title", "#GameUI_ConfirmOverwriteSaveGame_Info" );
				box->AddActionSignalTarget(this);
				box->SetOKButtonText("#GameUI_ConfirmOverwriteSaveGame_OK");
				box->SetOKCommand(new KeyValues("Command", "command", "SaveOverwriteConfirmed"));
				box->DoModal();
			}
		}
	}
	else if ( !stricmp( command, "SaveOverwriteConfirmed" ) )
	{
		int saveIndex = GetSelectedItemSaveIndex();
		if ( m_SaveGames.IsValidIndex(saveIndex) )
		{
			// delete any existing save
			DeleteSaveGame( m_SaveGames[saveIndex].szFileName );

			// save to a new name
			char saveName[128];
			FindSaveSlot( saveName, sizeof(saveName) );
			if ( saveName[ 0 ] )
			{
				// Load the game, return to top and switch to engine
				char sz[ 256 ];
				Q_snprintf(sz, sizeof( sz ), "save %s\n", saveName );

				engine->ClientCmd_Unrestricted( sz );

				// Close this dialog
				Close();

				// hide the UI
				GameUI().HideGameUI();
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog and rebuilds the save list
//-----------------------------------------------------------------------------
void CSaveGameDialog::Activate()
{
	BaseClass::Activate();
	ScanSavedGames();
}

//-----------------------------------------------------------------------------
// Purpose: scans for save games
//-----------------------------------------------------------------------------
void CSaveGameDialog::OnScanningSaveGames()
{
	// create dummy item for current saved game
	SaveGameDescription_t save = { "NewSavedGame", "", "", "#GameUI_NewSaveGame", "", "", "Current", NEW_SAVE_GAME_TIMESTAMP };
	m_SaveGames.AddToTail(save);
}

//-----------------------------------------------------------------------------
// Purpose: generates a new save game name
//-----------------------------------------------------------------------------
void CSaveGameDialog::FindSaveSlot( char *buffer, int bufsize )
{
	buffer[0] = 0;
	char szFileName[512];
	for (int i = 0; i < 1000; i++)
	{
		Q_snprintf(szFileName, sizeof( szFileName ), "save/half-life-%03i.sav", i );

		FileHandle_t fp = g_pFullFileSystem->Open( szFileName, "rb" );
		if (!fp)
		{
			// clean up name
			Q_strncpy( buffer, szFileName + 5, bufsize );
			char *ext = strstr( buffer, ".sav" );
			if ( ext )
			{
			 *ext = 0;
			}
			return;
		}
		g_pFullFileSystem->Close(fp);
	}

	Assert(!("Could not generate new save game file"));
}