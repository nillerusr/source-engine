//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <windows.h>
#include "SDKLauncherDialog.h"
#include "configs.h"

#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ProgressBox.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Divider.h>
#include <vgui_controls/menu.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "vgui_controls/button.h"
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/iinput.h>
#include <KeyValues.h>
#include <FileSystem.h>
#include <io.h>
#include "CreateModWizard.h"
#include "filesystem_tools.h"
#include "min_footprint_files.h"
#include "ConfigManager.h"
#include "filesystem_tools.h"
#include <iregistry.h>

#include <sys/stat.h>
#include "sdklauncher_main.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CSDKLauncherDialog *g_pSDKLauncherDialog = NULL;
CGameConfigManager	g_ConfigManager;
char g_engineDir[50];

//-----------------------------------------------------------------------------
// Purpose: Retrieves a URL out of the external localization file and opens it in a
//			web browser.
// Input  : *lpszLocalName - Localized name of the URL to open via shell execution
//-----------------------------------------------------------------------------
void OpenLocalizedURL( const char *lpszLocalName )
{
	// Find and convert the localized unicode string
	char pURLStr[MAX_PATH];
	wchar_t *pURLUni = g_pVGuiLocalize->Find( lpszLocalName );
	g_pVGuiLocalize->ConvertUnicodeToANSI( pURLUni, pURLStr, sizeof( pURLStr ) );

	// Open the URL through the shell
	vgui::system()->ShellExecute( "open", pURLStr );
}

class CResetConfirmationMessageBox : public vgui::Frame
{
public:
	typedef vgui::Frame BaseClass;

	CResetConfirmationMessageBox( Panel *pParent, const char *pPanelName )
		: BaseClass( pParent, pPanelName )
	{
		SetSize( 200, 200 );
		SetMinimumSize( 250, 100 );
		SetSizeable( false );
		LoadControlSettings( "resetconfirmbox.res" );
	}
	void OnCommand( const char *command )
	{
		if ( Q_stricmp( command, "ResetConfigs" ) == 0 )
		{
			Close();
			
			if ( GetVParent() )
			{
				PostMessage( GetVParent(), new KeyValues( "Command", "command", "ResetConfigs"));
			}
		}
		else if ( Q_stricmp( command, "MoreInfo" ) == 0 )
		{
			OpenLocalizedURL( "URL_Reset_Config" );
		}

		BaseClass::OnCommand( command );
	}
};

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
			OpenLocalizedURL( "URL_Convert_INI" );
		}
	}
};

class CGameInfoMessageBox : public vgui::Frame
{
public:
	typedef vgui::Frame BaseClass;

	CGameInfoMessageBox( Panel *pParent, const char *pPanelName )
		: BaseClass( pParent, pPanelName )
	{
		SetSize( 200, 200 );
		SetMinimumSize( 250, 100 );

		LoadControlSettings( "hl2infobox.res" );
	}

	virtual void OnCommand( const char *command )
	{
		BaseClass::OnCommand( command );

		// For some weird reason, this dialog can 
		if ( Q_stricmp( command, "RunAnyway" ) == 0 )
		{
			m_pDialog->Launch( m_iActiveItem, true );
			MarkForDeletion();
		}
		else if ( Q_stricmp( command, "Troubleshooting" ) == 0 )
		{
			OpenLocalizedURL( "URL_SDK_FAQ" );
			MarkForDeletion();
		}
	}

	CSDKLauncherDialog *m_pDialog;
	int m_iActiveItem;
};

class CMinFootprintRefreshConfirmationDialog : public vgui::Frame
{
public:
	typedef vgui::Frame BaseClass;

	CMinFootprintRefreshConfirmationDialog( Panel *pParent, const char *pPanelName )
		: BaseClass( pParent, pPanelName )
	{
		SetSizeable( false );
		LoadControlSettings( "min_footprint_confirm_box.res" );
		
		m_hOldModalSurface = input()->GetAppModalSurface();
		input()->SetAppModalSurface( GetVPanel() );
	}
	~CMinFootprintRefreshConfirmationDialog()
	{
		input()->SetAppModalSurface( m_hOldModalSurface );
	}
	
	void OnCommand( const char *command )
	{
		BaseClass::OnCommand( command );

		if ( Q_stricmp( command, "Continue" ) == 0 )
		{
			PostMessage( g_pSDKLauncherDialog, new KeyValues( "Command", "command", "RefreshMinFootprint" ) );
			MarkForDeletion();
		}
		else if ( Q_stricmp( command, "Close" ) == 0 )
		{
			MarkForDeletion();
		}
	}
	
	VPANEL m_hOldModalSurface;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSDKLauncherDialog::CSDKLauncherDialog(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	Assert( !g_pSDKLauncherDialog );
	g_pSDKLauncherDialog = this;

	SetSize(384, 480);
	SetMinimumSize(250, 200);

	SetMinimizeButtonVisible( true );
	m_pImageList = new ImageList( true );

	m_pMediaList = new SectionedListPanel(this, "MediaList");
	m_pMediaList->AddActionSignalTarget( this );
	m_pMediaList->SetImageList( m_pImageList, true );

	m_pContextMenu = new Menu( m_pMediaList, "AppsContextMenu" );
	
	m_pCurrentGameCombo = new ComboBox( this, "CurrentGameList", 8, false );
	m_pCurrentGameCombo->AddActionSignalTarget( this );

	m_pCurrentEngineCombo = new ComboBox( this, "CurrentEngineList", 4, false );
	m_pCurrentEngineCombo->AddActionSignalTarget( this );

	PopulateMediaList();

	LoadControlSettings( "SDKLauncherDialog.res" );

	GetEngineVersion( g_engineDir, sizeof( g_engineDir ) );

	PopulateCurrentEngineCombo( false );

	RefreshConfigs();
}

CSDKLauncherDialog::~CSDKLauncherDialog()
{
	delete m_pMediaList;
	g_pSDKLauncherDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: kills the whole app on close
//-----------------------------------------------------------------------------
void CSDKLauncherDialog::OnClose()
{
	BaseClass::OnClose();
	ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: loads media list from file
//-----------------------------------------------------------------------------
void CSDKLauncherDialog::PopulateMediaList()
{
	KeyValues *dataFile = new KeyValues("Media");
	dataFile->UsesEscapeSequences( true );

	if (dataFile->LoadFromFile( g_pFullFileSystem, "MediaList.txt", NULL ) )
	{
		// Load all the images.
		KeyValues *pImages = dataFile->FindKey( "Images" );
		if ( !pImages )
			Error( "MediaList.txt missing Images key." );

		for ( KeyValues *pCur=pImages->GetFirstTrueSubKey(); pCur; pCur=pCur->GetNextTrueSubKey() )
		{
			IImage *pImage = scheme()->GetImage( pCur->GetString( "Image", "" ), false );
			int iIndex = pCur->GetInt( "Index", -1 );
			if ( pImage && iIndex != -1 )
			{
				m_pImageList->SetImageAtIndex( iIndex, pImage );
			}
		}

		// Load all the sections.
		KeyValues *pSections = dataFile->FindKey( "Sections" );
		if ( !pSections )
			Error( "MediaList.txt missing Sections key." );

		for ( KeyValues *pSection=pSections->GetFirstTrueSubKey(); pSection; pSection=pSection->GetNextTrueSubKey() )
		{
			int id = pSection->GetInt( "id" );
			const char *pName = pSection->GetString( "Name" );
			m_pMediaList->AddSection( id, pName );
			m_pMediaList->AddColumnToSection( id, "Image", "", SectionedListPanel::COLUMN_IMAGE, 20 );
			m_pMediaList->AddColumnToSection( id, "Text", pName, 0, 200 );

			// Set all the rows.
			for ( KeyValues *kv = pSection->GetFirstTrueSubKey(); kv != NULL; kv=kv->GetNextTrueSubKey() )
			{
				m_pMediaList->AddItem( id, kv );
			}
		}
	}

	dataFile->deleteThis();
}

void CSDKLauncherDialog::Launch( int hActiveListItem, bool bForce )
{
	if (!m_pMediaList->IsItemIDValid(hActiveListItem))
		return;

	// display the file
	KeyValues *item = m_pMediaList->GetItemData( hActiveListItem );
	if ( !item )
		return;

	// Is this a ShellExecute or a program they want to run?
	const char *pStr = item->GetString( "InlineProgram", NULL );
	if ( pStr )
	{
		if ( Q_stricmp( pStr, "CreateMod" ) == 0 )
		{
			if ( !V_stricmp( g_engineDir, "ep1" ) || !V_stricmp( g_engineDir, "source2007" ) )
			{
				RunCreateModWizard( false );
			}
			else
			{
				VGUIMessageBox( this, "Unable to Run 'Create A Mod' Wizard", "Support for creating total conversions are not available using this engine versions." );
			}
		}
		else if ( Q_stricmp( pStr, "refresh_min_footprint" ) == 0 )
		{
			CMinFootprintRefreshConfirmationDialog *pDlg = new CMinFootprintRefreshConfirmationDialog( this, "RefreshConfirmation" );

			pDlg->RequestFocus();
			pDlg->SetVisible( true );
			pDlg->MoveToCenterOfScreen();
		}
		else if ( Q_stricmp( pStr, "reset_configs" ) == 0 )
		{
			CResetConfirmationMessageBox *pDlg = new CResetConfirmationMessageBox( this, "ResetConfirmation" );

			pDlg->RequestFocus();
			pDlg->SetVisible( true );
			pDlg->MoveToCenterOfScreen();
			input()->SetAppModalSurface( pDlg->GetVPanel() );
		}
		else
		{
			Error( "Unknown InlineProgram value: %s", pStr );
		}
		return;
	}
	
	pStr = item->GetString( "ShellExecute", NULL );
	if ( pStr )
	{
		// Replace tokens we know about like %basedir%.
		system()->ShellExecute( "open", pStr );
		return;
	}

	pStr = item->GetString( "Program", NULL );
	if ( pStr )
	{
		// Get our current config.
		KeyValues *kv = m_pCurrentGameCombo->GetActiveItemUserData();
		if ( !kv )
		{
			VGUIMessageBox( this, "Error", "No game configurations to run with." );
			return;
		}

		const char *pModDir = kv->GetString( "ModDir", NULL );
		if ( !pModDir )
		{
			VGUIMessageBox( this, "Error", "Missing 'ModDir' key/value." );
			return;
		}

		bool bRun = true;
		if ( !bForce && Q_stristr( pStr, "%gamedir%" ) )
		{
			// Make sure the currently-selected gamedir is valid and has a gameinfo.txt in it.
			char testStr[MAX_PATH];
			Q_strncpy( testStr, pModDir, sizeof( testStr ) );
			Q_AppendSlash( testStr, sizeof( testStr ) );
			Q_strncat( testStr, "gameinfo.txt", sizeof( testStr ), COPY_ALL_CHARACTERS );
			if ( _access( testStr, 0 ) != 0 )
			{
				CGameInfoMessageBox *dlg = new CGameInfoMessageBox( this, "GameInfoMessageBox" );
				
				dlg->m_pDialog = this;
				dlg->m_iActiveItem = hActiveListItem;

				dlg->RequestFocus();
				dlg->SetVisible( true );
				dlg->MoveToCenterOfScreen();
				input()->SetAppModalSurface( dlg->GetVPanel() );

				bRun = false;
			}
		}

		if ( bRun )
		{
			// Get the program name and its arguments.
			char programNameTemp[1024], programName[1024], launchDirectory[1024];

			SubstituteBaseDir( pStr, programNameTemp, sizeof( programNameTemp ) );
			V_StrSubst( programNameTemp, "%gamedir%", pModDir, programName, sizeof( programName ) );
			V_strncpy( programNameTemp, programName, sizeof( programNameTemp ) );
			V_StrSubst( programNameTemp, "%enginedir%", g_engineDir , programName, sizeof( programName ) );
			
			V_strncpy( launchDirectory, GetSDKLauncherBaseDirectory(), sizeof( launchDirectory ) );
			V_strncat( launchDirectory, "\\bin\\", sizeof( launchDirectory ) );
			V_strncat( launchDirectory, g_engineDir, sizeof( launchDirectory ) );

			// Check to see if we're running in tools mode
			if ( NULL != V_strstr( programName, "-tools" ) )
			{	
				// We can't run tools mode in engine versions earlier than OB 
				if ( !V_strcmp( g_engineDir, "ep1" ) )
				{
					VGUIMessageBox( this, "Error", "Source Engine Tools is not compatible with the selected engine version." );
					return;
				}

				// If we are running the engine tools then change our launch directory to the game directory
				V_strncpy( launchDirectory, pModDir, sizeof( launchDirectory ) );
				V_StripLastDir( launchDirectory, sizeof( launchDirectory ) );
				V_strncat( launchDirectory, "bin", sizeof( launchDirectory ) );
			}

			STARTUPINFO si;
			memset( &si, 0, sizeof( si ) );
			si.cb = sizeof( si );

			PROCESS_INFORMATION pi;
			memset( &pi, 0, sizeof( pi ) );


			DWORD dwFlags = 0;
			if ( !CreateProcess( 
				0,
				programName, 
				NULL,							// security
				NULL,
				TRUE,
				dwFlags,						// flags
				NULL,							// environment
				launchDirectory,	// current directory
				&si,
				&pi ) )
			{
				::MessageBoxA( NULL, GetLastWindowsErrorString(), "Error", MB_OK | MB_ICONINFORMATION | MB_APPLMODAL );
			}
		}
	}
}


void CSDKLauncherDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "LaunchButton" ) == 0 )
	{
		Launch( m_pMediaList->GetSelectedItem(), false );
	}
	else if ( Q_stricmp( command, "ResetConfigs" ) == 0 )
	{
		ResetConfigs();
	}
	else if ( Q_stricmp( command, "RefreshMinFootprint" ) == 0 )
	{
		DumpMinFootprintFiles( true );
	}

	BaseClass::OnCommand( command );
}


void CSDKLauncherDialog::OnItemDoubleLeftClick( int iItem )
{
	Launch( iItem, false );
}


void CSDKLauncherDialog::OnItemContextMenu( int hActiveListItem )
{
	if (!m_pMediaList->IsItemIDValid(hActiveListItem))
		return;

	// display the file
	KeyValues *item = m_pMediaList->GetItemData( hActiveListItem );
	if ( !item )
		return;

	// Build the context menu.
	m_pContextMenu->DeleteAllItems();

	// Is this a ShellExecute or a program they want to run?
	const char *pStr = item->GetString( "ShellExecute", NULL );
	if ( pStr )
		m_pContextMenu->AddMenuItem( "RunGame", "Open In Web Browser", new KeyValues("ItemDoubleLeftClick", "itemID", hActiveListItem), this);
	else
		m_pContextMenu->AddMenuItem( "RunGame", "Launch", new KeyValues("ItemDoubleLeftClick", "itemID", hActiveListItem), this);

	int menuWide, menuTall;
	m_pContextMenu->SetVisible(true);
	m_pContextMenu->InvalidateLayout(true);
	m_pContextMenu->GetSize(menuWide, menuTall);

	// work out where the cursor is and therefore the best place to put the menu
	int wide, tall;
	surface()->GetScreenSize(wide, tall);

	int cx, cy;
	input()->GetCursorPos(cx, cy);

	if (wide - menuWide > cx)
	{
		// menu hanging right
		if (tall - menuTall > cy)
		{
			// menu hanging down
			m_pContextMenu->SetPos(cx, cy);
		}
		else
		{
			// menu hanging up
			m_pContextMenu->SetPos(cx, cy - menuTall);
		}
	}
	else
	{
		// menu hanging left
		if (tall - menuTall > cy)
		{
			// menu hanging down
			m_pContextMenu->SetPos(cx - menuWide, cy);
		}
		else
		{
			// menu hanging up
			m_pContextMenu->SetPos(cx - menuWide, cy - menuTall);
		}
	}

	m_pContextMenu->RequestFocus();
	m_pContextMenu->MoveToFront();
}

bool CSDKLauncherDialog::ParseConfigs( CUtlVector<CGameConfig*> &configs )
{
	if ( !g_ConfigManager.IsLoaded() )
		return false;

	// Find the games block of the keyvalues
	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();
	
	if ( gameBlock == NULL )
	{
		return false;
	}

	// Iterate through all subkeys
	for ( KeyValues *pGame=gameBlock->GetFirstTrueSubKey(); pGame; pGame=pGame->GetNextTrueSubKey() )
	{
		const char *pName = pGame->GetName();
		const char *pDir = pGame->GetString( TOKEN_GAME_DIRECTORY );

		CGameConfig	*newConfig = new CGameConfig();
		UtlStrcpy( newConfig->m_Name, pName );
		UtlStrcpy( newConfig->m_ModDir, pDir );

		configs.AddToTail( newConfig );
	}

	return true;
}

void CSDKLauncherDialog::PopulateCurrentEngineCombo( bool bSelectLast )
{
	int nActiveEngine = 0;

	m_pCurrentEngineCombo->DeleteAllItems();

	// Add ep1 engine
	KeyValues *kv = new KeyValues( "values" );
	kv = new KeyValues( "values" );
	kv->SetString( "EngineVer", "ep1" );
	m_pCurrentEngineCombo->AddItem( "Source Engine 2006", kv );
	kv->deleteThis();
	if ( !V_strcmp( g_engineDir, "ep1" ) )
	{
		nActiveEngine = 0;
	}

	// Add ep2 engine
	kv = new KeyValues( "values" );
	kv->SetString( "EngineVer", "source2007" );
	m_pCurrentEngineCombo->AddItem( "Source Engine 2007", kv );
	kv->deleteThis();
	if ( !V_strcmp( g_engineDir, "source2007" ) )
	{
		nActiveEngine = 1;
	}

	// Add SP engine
	kv = new KeyValues( "values" );
	kv->SetString( "EngineVer", "source2009" );
	m_pCurrentEngineCombo->AddItem( "Source Engine 2009", kv );
	kv->deleteThis();
	if ( !V_strcmp( g_engineDir, "source2009" ) )
	{
		nActiveEngine = 2;
	}

	// Add MP engine
	kv = new KeyValues( "values" );
	kv->SetString( "EngineVer", "orangebox" );
	m_pCurrentEngineCombo->AddItem( "Source Engine MP", kv );
	kv->deleteThis();
	if ( !V_strcmp( g_engineDir, "orangebox" ) )
	{
		nActiveEngine = 3;
	}

	// Determine active configuration
	m_pCurrentEngineCombo->ActivateItem( nActiveEngine );
}

void CSDKLauncherDialog::SetCurrentGame( const char* pcCurrentGame )
{
	int activeConfig = -1;

	for ( int i=0; i < m_pCurrentGameCombo->GetItemCount(); i++ )
	{
		KeyValues *kv = m_pCurrentGameCombo->GetItemUserData( i );

		// Check to see if this is our currently active game
		if ( Q_stricmp( kv->GetString( "ModDir" ), pcCurrentGame ) == 0 )
		{
			activeConfig = i;
			continue;
		}
	}

	if ( activeConfig > -1 )
	{
		// Activate our currently selected game
		m_pCurrentGameCombo->ActivateItem( activeConfig );
	}
	else
	{
		// If the VCONFIG value for the game is not found then repopulate
		PopulateCurrentGameCombo( false );
	}
}


void CSDKLauncherDialog::PopulateCurrentGameCombo( bool bSelectLast )
{
	m_pCurrentGameCombo->DeleteAllItems();

	CUtlVector<CGameConfig*> configs;

	ParseConfigs( configs );

	char szGame[MAX_PATH];
	GetVConfigRegistrySetting( GAMEDIR_TOKEN, szGame, sizeof( szGame ) );

	int activeConfig = -1;

	CUtlVector<int> itemIDs;
	itemIDs.SetSize( configs.Count() );
	for ( int i=0; i < configs.Count(); i++ )
	{
		KeyValues *kv = new KeyValues( "values" );
		kv->SetString( "ModDir", configs[i]->m_ModDir.Base() );
		kv->SetPtr( "panel", m_pCurrentGameCombo );

		
		// Check to see if this is our currently active game
		if ( Q_stricmp( configs[i]->m_ModDir.Base(), szGame ) == 0 )
		{
			activeConfig = i;
		}

		itemIDs[i] = m_pCurrentGameCombo->AddItem( configs[i]->m_Name.Base(), kv );
		
		kv->deleteThis();
	}

	// Activate the correct entry if valid
	if ( itemIDs.Count() > 0 )
	{
		m_pCurrentGameCombo->SetEnabled( true );

		if ( bSelectLast )
		{
			m_pCurrentGameCombo->ActivateItem( itemIDs[itemIDs.Count()-1] );
		}
		else
		{
			if ( activeConfig > -1 )
			{
				// Activate our currently selected game
				m_pCurrentGameCombo->ActivateItem( activeConfig );
			}
			else
			{
				// Always default to the first otherwise
				m_pCurrentGameCombo->ActivateItem( 0 );
			}
		}
	}
	else
	{
		m_pCurrentGameCombo->SetEnabled( false );
	}

	configs.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: Selection has changed in the active config drop-down, set the environment
//-----------------------------------------------------------------------------
void CSDKLauncherDialog::OnTextChanged( KeyValues *pkv )
{
	const vgui::ComboBox* combo = (vgui::ComboBox*)pkv->GetPtr("panel");

	if ( combo ==  m_pCurrentGameCombo)
	{
		KeyValues *kv = m_pCurrentGameCombo->GetActiveItemUserData();

		if ( kv )
		{
			const char *modDir = kv->GetString( "ModDir" );
			SetVConfigRegistrySetting( GAMEDIR_TOKEN, modDir, true );
		}
	}
	else if ( combo == m_pCurrentEngineCombo )
	{
		KeyValues *kv = m_pCurrentEngineCombo->GetActiveItemUserData();

		if ( kv )
		{
			const char *engineDir = kv->GetString( "EngineVer" );
			SetEngineVersion( engineDir );
			RefreshConfigs();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Refresh our configs after a file change outside our process
//-----------------------------------------------------------------------------
void CSDKLauncherDialog::RefreshConfigs( void )
{
	char szGameConfigDir[MAX_PATH];

	V_strcpy_safe( szGameConfigDir, GetSDKLauncherBinDirectory() );
	V_AppendSlash( szGameConfigDir, MAX_PATH );
	V_strncat( szGameConfigDir, g_engineDir, MAX_PATH );
	V_AppendSlash( szGameConfigDir, MAX_PATH );
	V_strncat( szGameConfigDir, "bin", MAX_PATH );

	// Set directory in which GameConfig.txt is found
	g_ConfigManager.SetBaseDirectory( szGameConfigDir );

	// Tell the config manager which games to put in the config by default
	if ( !stricmp( g_engineDir, "ep1" ) )
	{
		g_ConfigManager.SetSDKEpoch( EP1 );
	}
	else if ( !stricmp( g_engineDir, "source2007" ) )
	{
		g_ConfigManager.SetSDKEpoch( EP2 );
	}
	else if ( !stricmp( g_engineDir, "source2009" ) )
	{
		g_ConfigManager.SetSDKEpoch( SP2009 );
	}
	else 
	{
		g_ConfigManager.SetSDKEpoch( MP2009 );
	}


	// Load configurations
	if ( g_ConfigManager.LoadConfigs( szGameConfigDir ) == false )
	{
		m_pCurrentGameCombo->DeleteAllItems();
		m_pCurrentGameCombo->SetEnabled( false );
	}
	else
	{
		m_pCurrentGameCombo->SetEnabled( true );

		if ( g_ConfigManager.WasConvertedOnLoad() )
		{
			// Notify of a conversion
			CConversionInfoMessageBox *pDlg = new CConversionInfoMessageBox( this, "ConversionInfo" );

			pDlg->RequestFocus();
			pDlg->SetVisible( true );
			pDlg->MoveToCenterOfScreen();
			input()->SetAppModalSurface( pDlg->GetVPanel() );
		}
	}

	// Dump the current settings and reparse the change
	PopulateCurrentGameCombo( false );
}

//-----------------------------------------------------------------------------
// Purpose: Reset our config files
//-----------------------------------------------------------------------------
void CSDKLauncherDialog::ResetConfigs( void )
{
	// Reset the configs
	g_ConfigManager.ResetConfigs();
	
	// Refresh the listing
	PopulateCurrentGameCombo( false );

	// Notify the user
	VGUIMessageBox( this, "Complete", "Your game configurations have successfully been reset to their default values." );
}

void CSDKLauncherDialog::GetEngineVersion(char* pcEngineVer, int nSize)
{
	IRegistry *reg = InstanceRegistry( "Source SDK" );
	Assert( reg );
	V_strncpy( pcEngineVer, reg->ReadString( "EngineVer", "orangebox" ), nSize );
	ReleaseInstancedRegistry( reg );
}

void CSDKLauncherDialog::SetEngineVersion(const char *pcEngineVer)
{
	IRegistry *reg = InstanceRegistry( "Source SDK" );
	Assert( reg );
	reg->WriteString( "EngineVer", pcEngineVer );
	ReleaseInstancedRegistry( reg );

	// Set the global to the same value as the registry
	V_strncpy( g_engineDir, pcEngineVer, sizeof( g_engineDir ) );
}