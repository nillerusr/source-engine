//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include <windows.h>
#include <vgui/IVGui.h>
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui/IInput.h>
#include <KeyValues.h>

#include "vconfig_main.h"
#include "ManageGamesDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CManageGamesDialog *g_pManageGamesDialog = NULL;

class CModalDirectorySelectDialog : public vgui::DirectorySelectDialog
{
public:
	CModalDirectorySelectDialog( vgui::Panel *parent, const char *title )
		: vgui::DirectorySelectDialog( parent, title )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalDirectorySelectDialog( void )
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}


public:
	vgui::VPANEL	m_PrevAppFocusPanel;
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CManageGamesDialog::CManageGamesDialog( Panel *parent, const char *name, int configID ) : BaseClass( parent, name ), m_nConfigID( configID )
{
	Assert( !g_pManageGamesDialog );
	g_pManageGamesDialog = this;

	SetSize(384, 420);
	SetMinimumSize(200, 50);

	SetMinimizeButtonVisible( false );
	
	m_pGameNameEntry= new vgui::TextEntry( this, "GameName" );
	m_pGameDirEntry = new vgui::TextEntry( this, "GamePath" );
	
	LoadControlSettings( "ManageGamesDialog.res" );

	SetDeleteSelfOnClose( true );

	SetSizeable( false );
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CManageGamesDialog::~CManageGamesDialog( void )
{
	g_pManageGamesDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the game directory
//-----------------------------------------------------------------------------
void CManageGamesDialog::SetGameDir( const char *szDir )
{
	// Strip any trailing slashes
	char szGameDir[MAX_PATH];
	Q_strncpy( szGameDir, szDir, MAX_PATH );
	Q_StripTrailingSlash( szGameDir );

	m_pGameDirEntry->SetText( szGameDir );
}

//-----------------------------------------------------------------------------
// Purpose: Set the game name
//-----------------------------------------------------------------------------
void CManageGamesDialog::SetGameName( const char *szDir )
{
	m_pGameNameEntry->SetText( szDir );
}

//-----------------------------------------------------------------------------
// Purpose: Ensures the parameter name is unique
// Input  : *name - name to test
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CManageGamesDialog::IsGameNameUnique( const char *name )
{
	// Check all names
	for ( int i = 0; i < g_Configs.Count(); i++ )
	{
		// Skip ourself
		if ( i == m_nConfigID )
			continue;

		// Test it
		if ( Q_stricmp( name, g_Configs[i]->m_Name.Base() ) == 0 )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles dialog commands
//-----------------------------------------------------------------------------
void CManageGamesDialog::OnCommand( const char *command )
{
	// Handle "OK" button
	if ( Q_stricmp( command, "Select" ) == 0 )
	{
		char textBuffer[1024];

		// Save out the data
		m_pGameNameEntry->GetText( textBuffer, sizeof( textBuffer ) );
		
		// Make sure we're not setting this to a duplicate
		if ( IsGameNameUnique( textBuffer ) == false )
		{
			// Select the text
			m_pGameNameEntry->SelectAllText( true );

			// Pop a message box and refuse to close
			VGUIMessageBox( this, "Error", "Game name %s already exists!  Please enter a unique name.", textBuffer );

			BaseClass::OnCommand( command );
			return;
		}

		KeyValues *actionSignal = new KeyValues("ManageSelect");

		if ( actionSignal == NULL )
		{
			Assert( 0 );
			return;
		}

		// See if we need to add a new config
		if ( m_nConfigID == NEW_CONFIG_ID )
		{
			// Create a new data container and point to it
			m_nConfigID = g_Configs.AddToTail( new CGameConfig() );

			// Send an overidden action signal to notify that we've added, not edited a field
			actionSignal->SetName("AddSelect");
		}

		// Otherwise take the name
		UtlStrcpy( g_Configs[m_nConfigID]->m_Name, textBuffer );
		
		// Take the game directory
		m_pGameDirEntry->GetText( textBuffer, sizeof( textBuffer ) );

		// Strip off the trailing slash always
		Q_StripTrailingSlash( textBuffer );

		UtlStrcpy( g_Configs[m_nConfigID]->m_ModDir, textBuffer );
		
		// Tell the parent we altered its data so it can refresh
		PostActionSignal( actionSignal );

		Close();
	}
	// Modified to allow more than one browse button
	else if ( Q_stricmp( command, "BrowseDir" ) == 0 )
	{
		// Create a new dialog
		CModalDirectorySelectDialog *pDlg = vgui::SETUP_PANEL( new CModalDirectorySelectDialog( this, "Select Game Directory" ) );
		
		char textBuffer[1024];
		m_pGameDirEntry->GetText( textBuffer, sizeof( textBuffer ) );
		
		// Get the currently set dir and use that as the start
		pDlg->ExpandTreeToPath( textBuffer );
		pDlg->MoveToCenterOfScreen();
		pDlg->AddActionSignalTarget( this );
		pDlg->SetDeleteSelfOnClose( true );
		pDlg->DoModal();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Notify us that the directory dialog has returned a new entry
//-----------------------------------------------------------------------------
void CManageGamesDialog::OnChooseDirectory( const char *dir )
{
	SetGameDir( dir );
}
