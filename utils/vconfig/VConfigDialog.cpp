//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog for selecting game configurations
//
//=====================================================================================//

#include <windows.h>

#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/MessageBox.h>
#include <KeyValues.h>

#include "vconfig_main.h"
#include "VConfigDialog.h"
#include "ManageGamesDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CVConfigDialog *g_pVConfigDialog = NULL;

class CConversionInfoMessageBox : public vgui::Frame
{
public:
	typedef vgui::Frame BaseClass;

	CConversionInfoMessageBox( Panel *pParent, const char *pPanelName )
		: BaseClass( pParent, pPanelName )
	{
		SetSize( 200, 200 );
		SetMinimumSize( 250, 100 );
		SetSizeable( false );
		LoadControlSettings( "convinfobox.res" );
	}

	virtual void OnCommand( const char *command )
	{
		BaseClass::OnCommand( command );

		// For some weird reason, this dialog can 
		if ( Q_stricmp( command, "ShowFAQ" ) == 0 )
		{
			system()->ShellExecute( "open", "http://www.valve-erc.com/srcsdk/faq.html#convertINI" );
		}
	}
};

class CModalPreserveMessageBox : public vgui::MessageBox
{
public:
	CModalPreserveMessageBox(const char *title, const char *text, vgui::Panel *parent)
		: vgui::MessageBox( title, text, parent )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalPreserveMessageBox()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}


public:
	vgui::VPANEL	m_PrevAppFocusPanel;
};
		
//-----------------------------------------------------------------------------
// Purpose: Utility function to pop up a VGUI message box
//-----------------------------------------------------------------------------
void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	vgui::MessageBox *dlg = new CModalPreserveMessageBox( pTitle, msg, pParent );
	dlg->DoModal();
	dlg->Activate();
	dlg->RequestFocus();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVConfigDialog::CVConfigDialog( Panel *parent, const char *name ) : BaseClass( parent, name ), m_bChanged( false )
{
	Assert( !g_pVConfigDialog );
	g_pVConfigDialog = this;

	SetSize(384, 420);
	SetMinimumSize(200, 100);

	SetMinimizeButtonVisible( true );
	
	m_pConfigCombo = new ComboBox( this, "ConfigCombo", 8, false );

	PopulateConfigList();

	LoadControlSettings( "VConfigDialog.res" );

	// See if we converted on load and notify
	if ( g_ConfigManager.WasConvertedOnLoad() )
	{
		//VGUIMessageBox( this, "Update Occured", "Your game configurations have been updated.\n\nA backup file GameCfg.INI.OLD has been created.\n\nPlease visit http://www.valve-erc.com/srcsdk/faq.html#GameConfigUpdate for more information." );
		CConversionInfoMessageBox *pDlg = new CConversionInfoMessageBox( this, "ConversionInfo" );

		pDlg->RequestFocus();
		pDlg->SetVisible( true );
		pDlg->MoveToCenterOfScreen();
		input()->SetAppModalSurface( pDlg->GetVPanel() );
	}	
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CVConfigDialog::~CVConfigDialog()
{
	delete m_pConfigCombo;
	g_pVConfigDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Populate the configuration list for selection
//-----------------------------------------------------------------------------
void CVConfigDialog::PopulateConfigList( bool bSelectActiveConfig /*=true*/ )
{
	int activeItem = -1;

	char szKeyValue[1024] = "\0";

	if ( bSelectActiveConfig )
	{
		// Get the currently set game configuration
		if ( GetVConfigRegistrySetting( GAMEDIR_TOKEN, szKeyValue, sizeof( szKeyValue ) ) == false )
		{
			//NOTE: We may want to pop an info dialog here if there was no initial VPROJECT setting
			activeItem = 0;		
		}
	}
	else
	{
		activeItem = m_pConfigCombo->GetActiveItem();
	}

	// Purge all our items
	m_pConfigCombo->DeleteAllItems();

	KeyValues *kv = new KeyValues( "Items" );

	// Add all configurations
	for ( int i = 0; i < g_Configs.Count(); i++ )
	{
		// Set the text
		kv->SetString( "ModDir", g_Configs[i]->m_ModDir.Base() );
		
		// Add the item into the list
		int index = m_pConfigCombo->AddItem( g_Configs[i]->m_Name.Base(), kv );

		if ( bSelectActiveConfig )
		{
			if ( !Q_stricmp( g_Configs[i]->m_ModDir.Base(), szKeyValue ) )
			{
				activeItem = index;
			}
		}
	}

	// Make sure we have an active item
	if ( activeItem < 0 )
	{
		// Give a warning if they have a mismatched directory!
		VGUIMessageBox( this, "Invalid Game Directory", "The currently selected game directory: %s is invalid.\nChoose a new directory, or select 'Cancel' to exit.\n", szKeyValue );

		// Default to the first config in the list
		activeItem = 0;
	}

	// Set us to the active config
	m_pConfigCombo->ActivateItem( activeItem );

	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CVConfigDialog::OnClose( void )
{
	BaseClass::OnClose();
	ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: Select the item from the list (updating the environment variable as well)
// Input  : index - item to select
//-----------------------------------------------------------------------------
void CVConfigDialog::SetGlobalConfig( const char *modDir )
{
	// Set our environment variable
	SetMayaScriptSettings( );
	SetXSIScriptSettings( );
	SetPathSettings( );
	SetVConfigRegistrySetting( GAMEDIR_TOKEN, modDir );
}

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CVConfigDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Select" ) == 0 )
	{
		int activeID = m_pConfigCombo->GetActiveItem();

		SetGlobalConfig( g_Configs[activeID]->m_ModDir.Base() );

		// Save off our data
		if ( m_bChanged )
		{
			SaveConfigs();
		}

		Close();
	}
	else if ( Q_stricmp( command, "Manage" ) == 0 )
	{
		int activeID = m_pConfigCombo->GetActiveItem();

		// Launch the edit window
		CManageGamesDialog *pDialog = new CManageGamesDialog( this, "ManageGamesDialog", activeID );

		pDialog->AddActionSignalTarget( this );
  		pDialog->SetGameDir( g_Configs[activeID]->m_ModDir.Base() );
  		pDialog->SetGameName( g_Configs[activeID]->m_Name.Base() );
		pDialog->DoModal();
	}
	else if ( Q_stricmp( command, "AddConfig" ) == 0 )
	{
		// Launch the edit window, specifying that we're adding a config
		CManageGamesDialog *pDialog = new CManageGamesDialog( this, "ManageGamesDialog", NEW_CONFIG_ID );

		pDialog->AddActionSignalTarget( this );
		pDialog->DoModal();
	}
	else if ( Q_stricmp( command, "RemoveConfig" ) == 0 )
	{	
		// Don't allow the list to completely vanish
		// NOTE: We should display the list as being empty, i.e. "<EMPTY>"
		if ( g_Configs.Count() <= 1 )
		{
			VGUIMessageBox( this, "Error", "Cannot remove last configuration from list!" );
			return;
		}

		// Remove this config from our list
		int activeID = m_pConfigCombo->GetActiveItem();

		RemoveConfig( activeID );

		ReloadConfigs( true );

		// Select the next entry
		m_pConfigCombo->ActivateItem( g_Configs.Count()-1 );

		// Mark that we changed our configs
		m_bChanged = true;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Manage dialog has reported a need to update
//-----------------------------------------------------------------------------
void CVConfigDialog::OnManageSelect( void )
{
	// Publish the config changes to the internal data in the configuration manager
	UpdateConfigs();

	// Update the configuration list
	PopulateConfigList( false );

	m_bChanged = true;
}

//-----------------------------------------------------------------------------
// Purpose: Manage dialog has reported that it has added a configuration
//-----------------------------------------------------------------------------
void CVConfigDialog::OnAddSelect( void )
{
	// Add the last config we entered
	AddConfig( g_Configs.Count()-1 );

	// Re-populate the configuration list
	ReloadConfigs();

	// Select the last entry (which will be the new one)
	m_pConfigCombo->ActivateItem( g_Configs.Count()-1 );

	// Mark us as changed
	m_bChanged = true;
}
