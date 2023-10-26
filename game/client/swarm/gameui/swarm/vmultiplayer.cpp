//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VMultiplayer.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "vgui_controls/fileopendialog.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/imagepanel.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "EngineInterface.h"
#include "filesystem.h"
#include "fmtstr.h"
#include "cdll_util.h"
#include "nb_header_footer.h"
#include "materialsystem/materialsystem_config.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


#define BAR_WIDTH 150
#define BAR_INCREMENT 8


#define MULTIPLAYER_SPRAYPAINT_COMMAND_PREFIX "_spraypaint"
#define MULTIPLAYER_CUSTOM_SPRAY_FOLDER "custom/"
#define MULTIPLAYER_SPRAY_FOLDER "materials/vgui/logos/"

int GetScreenAspectMode( int width, int height );
void SetFlyoutButtonText( const char *pchCommand, FlyoutMenu *pFlyout, const char *pchNewText );

// TODO: RemapLogo & SelectCustomLogoFile
void RemapLogo( char *pchLogoname, vgui::ImagePanel *pLogoImage, const char *finalpathSubfolder = NULL ) {}
bool SelectCustomLogoFile(const char *fullpath, const char *finalpathSubfolder = NULL) { return false; }


//=============================================================================
Multiplayer::Multiplayer(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose(true);

	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( true );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 150, 120 );

	m_drpAllowLanGames = NULL;
	m_drpAllowCustomContent = NULL;
	m_drpColorBlind = NULL;
	m_drpGameInstructor = NULL;
	m_drpAllowFreeLook = NULL;
	m_drpSpraypaint = NULL;
	m_btnBrowseSpraypaint = NULL;
	m_pSprayLogo = NULL;
	m_drpGore = NULL;

	m_btnCancel = NULL;

	m_hImportSprayDialog = NULL;
	m_hSelectSprayDialog = NULL;
}

//=============================================================================
Multiplayer::~Multiplayer()
{
	GameUI().AllowEngineHideGameUI();

	if ( m_hImportSprayDialog )
	{
		m_hImportSprayDialog->DeletePanel();
		m_hImportSprayDialog = NULL;
	}

	if ( m_hSelectSprayDialog )
	{
		m_hSelectSprayDialog->DeletePanel();
		m_hSelectSprayDialog = NULL;
	}

	UpdateFooter( false );
}

void Multiplayer::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

//=============================================================================
void Multiplayer::Activate()
{
	BaseClass::Activate();

	if ( m_drpAllowLanGames )
	{
		CGameUIConVarRef net_allow_multicast( "net_allow_multicast" );

		m_drpAllowLanGames->SetCurrentSelection( CFmtStr( "MpLanGames%sabled", net_allow_multicast.GetBool() ? "En" : "Dis" ) );

		FlyoutMenu *pFlyout = m_drpAllowLanGames->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpAllowCustomContent )
	{
		CGameUIConVarRef cl_downloadfilter( "cl_downloadfilter" );
		m_drpAllowCustomContent->SetCurrentSelection( CFmtStr( "#GameUI_DownloadFilter_%s", cl_downloadfilter.GetString() ) );

		FlyoutMenu *pFlyout = m_drpAllowCustomContent->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpColorBlind )
	{
		CGameUIConVarRef cl_colorblind( "cl_colorblind" );
		m_drpColorBlind->SetCurrentSelection( CFmtStr( "ColorBlind%d", cl_colorblind.GetInt() ) );

		FlyoutMenu *pFlyout = m_drpColorBlind->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpGameInstructor )
	{
		CGameUIConVarRef gameinstructor_enable( "gameinstructor_enable" );

		if ( gameinstructor_enable.GetBool() )
		{
			m_drpGameInstructor->SetCurrentSelection( "GameInstructorEnabled" );
		}
		else
		{
			m_drpGameInstructor->SetCurrentSelection( "GameInstructorDisabled" );
		}

		FlyoutMenu *pFlyout = m_drpGameInstructor->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpAllowFreeLook )
	{
		CGameUIConVarRef spec_allowroaming( "spec_allowroaming" );

		if ( spec_allowroaming.GetBool() )
		{
			m_drpAllowFreeLook->SetCurrentSelection( "AllowFreeLookEnabled" );
		}
		else
		{
			m_drpAllowFreeLook->SetCurrentSelection( "AllowFreeLookDisabled" );
		}

		FlyoutMenu *pFlyout = m_drpAllowFreeLook->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpSpraypaint )
	{
		InitLogoList();

		FlyoutMenu *pFlyout = m_drpSpraypaint->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	if ( m_drpAllowLanGames )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom( );
		m_drpAllowLanGames->NavigateTo();
		m_ActiveControl = m_drpAllowLanGames;
	}

	if ( m_drpGore )
	{
		CGameUIConVarRef z_wound_client_disabled ( "z_wound_client_disabled" );

		if ( z_wound_client_disabled.GetBool() )
		{
			m_drpGore->SetCurrentSelection( "#L4D360UI_Gore_Low" );
		}
		else
		{
			m_drpGore->SetCurrentSelection( "#L4D360UI_Gore_High" );
		}

		FlyoutMenu *pFlyout = m_drpGore->GetCurrentFlyout();
		if ( pFlyout )
		{
			pFlyout->SetListener( this );
		}
	}

	UpdateFooter( true );
}


void Multiplayer::UpdateFooter( bool bEnableCloud )
{
	if ( !BaseModUI::CBaseModPanel::GetSingletonPtr() )
		return;

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

		footer->SetShowCloud( bEnableCloud );
	}
}

void Multiplayer::OnThink()
{
	BaseClass::OnThink();

	bool needsActivate = false;

	if ( !m_drpAllowLanGames )
	{
		m_drpAllowLanGames = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpLanGamesDiscovery" ) );
		needsActivate = true;
	}

	if ( !m_drpAllowCustomContent )
	{
		m_drpAllowCustomContent = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpAllowCustomContent" ) );
		needsActivate = true;
	}

	if ( !m_drpColorBlind )
	{
		m_drpColorBlind = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpColorBlind" ) );
		needsActivate = true;
	}

	if ( !m_drpGameInstructor )
	{
		m_drpGameInstructor = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGameInstructor" ) );
		needsActivate = true;
	}

	if ( !m_drpAllowFreeLook )
	{
		m_drpAllowFreeLook = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpAllowFreeLook" ) );
		needsActivate = true;
	}

	if ( !m_drpSpraypaint )
	{
		m_drpSpraypaint = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpSpraypaint" ) );
		needsActivate = true;
	}

	if ( !m_btnBrowseSpraypaint )
	{
		m_btnBrowseSpraypaint = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnBrowseSpraypaint" ) );
		needsActivate = true;
	}

	if ( !m_pSprayLogo )
	{
		m_pSprayLogo = dynamic_cast< ImagePanel* >( FindChildByName( "LogoImage" ) );
		needsActivate = true;
	}

// 	if ( !m_btnCancel )
// 	{
// 		m_btnCancel = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnCancel" ) );
// 		needsActivate = true;
// 	}

	if ( !m_drpGore )
	{
		m_drpGore = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGore" ) );
		needsActivate = true;
	}

	if ( needsActivate )
	{
		Activate();
	}
}

void Multiplayer::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// nav back
		BaseClass::OnKeyCodePressed(code);
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

//=============================================================================
void Multiplayer::OnCommand(const char *command)
{
	if( Q_stricmp( "#GameUI_Headphones", command ) == 0 )
	{
		CGameUIConVarRef snd_surround_speakers("Snd_Surround_Speakers");
		snd_surround_speakers.SetValue( "0" );
	}
	
	else if ( char const *sz = StringAfterPrefix( command, "#GameUI_DownloadFilter_" ) )
	{
		CGameUIConVarRef  cl_downloadfilter( "cl_downloadfilter" );
		cl_downloadfilter.SetValue( sz );
	}
	else if ( char const *sz = StringAfterPrefix( command, "ColorBlind" ) )
	{
		CGameUIConVarRef cl_colorblind( "cl_colorblind" );
		cl_colorblind.SetValue( sz );
	}
	else if ( char const *sz = StringAfterPrefix( command, "GameInstructor" ) )
	{
		CGameUIConVarRef gameinstructor_enable( "gameinstructor_enable" );
		gameinstructor_enable.SetValue( !Q_stricmp( sz, "Enabled" ) );
	}
	else if ( char const *sz = StringAfterPrefix( command, "AllowFreeLook" ) )
	{
		CGameUIConVarRef spec_allowroaming( "spec_allowroaming" );
		spec_allowroaming.SetValue( !Q_stricmp( sz, "Enabled" ) );
	}
	else if ( char const *sz = StringAfterPrefix( command, "MpLanGames" ) )
	{
		CGameUIConVarRef net_allow_multicast( "net_allow_multicast" );
		net_allow_multicast.SetValue( !Q_stricmp( sz, "Enabled" ) );
	}

	else if ( StringHasPrefix( command, "_spraypaint" ) )
	{
		int iCommandNumberPosition = Q_strlen( MULTIPLAYER_SPRAYPAINT_COMMAND_PREFIX );
		int iLogo = clamp( command[ iCommandNumberPosition ] - '0', 0, m_nNumSpraypaintLogos - 1 );

		if ( m_nSpraypaint[ iLogo ].m_bCustom && m_nSpraypaint[ iLogo ].m_szFilename[ 0 ] == '\0' )
		{
			// Select a file from the custom directory!
			if ( m_hSelectSprayDialog )
			{
				// Always start fresh so the directory refreshes
				m_hSelectSprayDialog->DeletePanel();
				m_hSelectSprayDialog = NULL;
			}

			m_hSelectSprayDialog = new FileOpenDialog(this, "#L4D360UI_Multiplayer_Cutsom_Logo", true);
			m_hSelectSprayDialog->SetProportional( false );
			m_hSelectSprayDialog->SetDeleteSelfOnClose( false );
			m_hSelectSprayDialog->AddFilter("*.vtf", "*.vtf", true);
			m_hSelectSprayDialog->AddActionSignalTarget(this);

			char szCustomSprayDir[ MAX_PATH ];
			Q_snprintf( szCustomSprayDir, sizeof( szCustomSprayDir ), "%s/" MULTIPLAYER_SPRAY_FOLDER MULTIPLAYER_CUSTOM_SPRAY_FOLDER, engine->GetGameDirectory() );
			m_hSelectSprayDialog->SetStartDirectoryContext( "SelectCustomLogo", szCustomSprayDir );

			m_hSelectSprayDialog->DoModal(false);
			m_hSelectSprayDialog->Activate();

			// Hide directory buttons
			m_hSelectSprayDialog->SetControlVisible( "FolderUpButton", false );
			m_hSelectSprayDialog->SetControlVisible( "NewFolderButton", false );
			m_hSelectSprayDialog->SetControlVisible( "OpenInExplorerButton", false );
			m_hSelectSprayDialog->SetControlVisible( "LookInLabel", false );
			m_hSelectSprayDialog->SetControlVisible( "FullPathEdit", false );
			m_hSelectSprayDialog->SetControlVisible( "FileNameLabel", false );

			for ( int i = 0; i < m_hSelectSprayDialog->GetChildCount(); ++i )
			{
				// Need to hide text entry box so they can't manually type ".." and go back a dir
				TextEntry *pTextEntry = dynamic_cast<TextEntry*>( m_hSelectSprayDialog->GetChild( i ) );

				if ( pTextEntry && !dynamic_cast<ComboBox*>( pTextEntry ) )
				{
					pTextEntry->SetVisible( false );
				}
			}
		}
		else
		{
			char *pchCustomPath = ( ( m_nSpraypaint[ iLogo ].m_bCustom ) ? ( MULTIPLAYER_CUSTOM_SPRAY_FOLDER ) : ( "" ) );

			char rootFilename[MAX_PATH];
			Q_snprintf( rootFilename, sizeof(rootFilename), MULTIPLAYER_SPRAY_FOLDER "%s%s.vtf", pchCustomPath, m_nSpraypaint[ iLogo ].m_szFilename );

			CGameUIConVarRef cl_logofile( "cl_logofile", true );
			cl_logofile.SetValue( rootFilename );

			RemapLogo( m_nSpraypaint[ iLogo ].m_szFilename, m_pSprayLogo, pchCustomPath );

			// Clear out the custom image's filename so they will be forced to reselect
			for ( int i = 0; i < MAX_SPRAYPAINT_LOGOS; ++i )
			{
				if ( m_nSpraypaint[ iLogo ].m_bCustom )
				{
					m_nSpraypaint[ iLogo ].m_szFilename[ 0 ] = '\0';
				}
			}
		}
	}
	else if( Q_stricmp( "ImportSprayImage", command ) == 0 )
	{
		FlyoutMenu::CloseActiveMenu();

		if ( m_hImportSprayDialog == NULL)
		{
			m_hImportSprayDialog = new FileOpenDialog(this, "#GameUI_ImportSprayImage", true);
			m_hImportSprayDialog->SetProportional( false );
			m_hImportSprayDialog->SetDeleteSelfOnClose( false );
			m_hImportSprayDialog->AddFilter("*.tga,*.jpg,*.bmp,*.vtf", "#GameUI_All_Images", true);
			m_hImportSprayDialog->AddFilter("*.tga", "#GameUI_TGA_Images", false);
			m_hImportSprayDialog->AddFilter("*.jpg", "#GameUI_JPEG_Images", false);
			m_hImportSprayDialog->AddFilter("*.bmp", "#GameUI_BMP_Images", false);
			m_hImportSprayDialog->AddFilter("*.vtf", "#GameUI_VTF_Images", false);
			m_hImportSprayDialog->AddActionSignalTarget(this);

			char szGameDir[ MAX_PATH ];
			Q_snprintf( szGameDir, sizeof( szGameDir ), "%s/", engine->GetGameDirectory() );
			m_hImportSprayDialog->SetStartDirectoryContext( "ImportCustomLogo", szGameDir );
		}

		m_hImportSprayDialog->DoModal(false);
		m_hImportSprayDialog->Activate();
	}
	else if( Q_stricmp( "Back", command ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Gore_High" ) )
	{
		CGameUIConVarRef z_wound_client_disabled( "z_wound_client_disabled" );
		z_wound_client_disabled.SetValue( 0 );
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Gore_Low" ) )
	{
		CGameUIConVarRef z_wound_client_disabled( "z_wound_client_disabled" );
		z_wound_client_disabled.SetValue( 1 );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void Multiplayer::InitLogoList()
{
	if ( !m_drpSpraypaint )
	{
		return;
	}

	FlyoutMenu *pFlyout = m_drpSpraypaint->GetCurrentFlyout();

	char szCurrentButton[ 32 ];
	Q_strncpy( szCurrentButton, MULTIPLAYER_SPRAYPAINT_COMMAND_PREFIX, sizeof( szCurrentButton ) );

	int iCommandNumberPosition = Q_strlen( szCurrentButton );
	szCurrentButton[ iCommandNumberPosition + 1 ] = '\0';

	// Find our logos
	int initialItem = 0;

	CGameUIConVarRef cl_logofile( "cl_logofile", true );
	if ( !cl_logofile.IsValid() )
		return;

	const char *logofile = cl_logofile.GetString();

	m_nNumSpraypaintLogos = 0;

	KeyValues *pSpraysManifest = new KeyValues( "sprays_manifest" );
	KeyValues::AutoDelete autoDelete( pSpraysManifest );

	pSpraysManifest->LoadFromFile( g_pFullFileSystem, "scripts/sprays_manifest.txt", NULL );

	KeyValues *pSprayKey = pSpraysManifest->GetFirstSubKey();

	while ( m_nNumSpraypaintLogos < MAX_SPRAYPAINT_LOGOS - 1 && pSprayKey )
	{
		char filename[ MAX_PATH ];
		Q_snprintf( filename, sizeof(filename), MULTIPLAYER_SPRAY_FOLDER "%s", pSprayKey->GetString() );

		if ( g_pFullFileSystem->FileExists( filename, "GAME" ) )
		{
			Q_SetExtension( filename, ".vmt", sizeof( filename ) );

			if ( g_pFullFileSystem->FileExists( filename, "GAME" ) )
			{
				Q_strncpy( filename, pSprayKey->GetString(), sizeof(filename) );

				Q_StripExtension( filename, m_nSpraypaint[ m_nNumSpraypaintLogos ].m_szFilename, sizeof(m_nSpraypaint[ m_nNumSpraypaintLogos ].m_szFilename) );
				m_nSpraypaint[ m_nNumSpraypaintLogos ].m_bCustom = false;

				szCurrentButton[ iCommandNumberPosition ] = m_nNumSpraypaintLogos + '0';
				SetFlyoutButtonText( szCurrentButton, pFlyout, pSprayKey->GetName() );

				// check to see if this is the one we have set
				Q_snprintf( filename, sizeof(filename), MULTIPLAYER_SPRAY_FOLDER "%s", pSprayKey->GetString() );
				if (!Q_stricmp(filename, logofile))
				{
					initialItem = m_nNumSpraypaintLogos;
				}

				++m_nNumSpraypaintLogos;
			}
		}

		pSprayKey = pSprayKey->GetNextKey();
	}

	// Look for custom sprays
	char directory[ MAX_PATH ];
	Q_strncpy( directory, MULTIPLAYER_SPRAY_FOLDER MULTIPLAYER_CUSTOM_SPRAY_FOLDER "*.vtf", sizeof( directory ) );

	FileFindHandle_t fh;
	const char *fn = g_pFullFileSystem->FindFirst( directory, &fh );

	if ( fn )
	{
		char filename[ MAX_PATH ];
		Q_snprintf( filename, sizeof(filename), MULTIPLAYER_SPRAY_FOLDER MULTIPLAYER_CUSTOM_SPRAY_FOLDER "%s", fn );
		Q_SetExtension( filename, ".vmt", sizeof( filename ) );

		if ( g_pFullFileSystem->FileExists( filename, "GAME" ) )
		{
			// Found at least one custom logo
			szCurrentButton[ iCommandNumberPosition ] = m_nNumSpraypaintLogos + '0';
			SetFlyoutButtonText( szCurrentButton, pFlyout, "#L4D360UI_Multiplayer_Cutsom_Logo" );
			m_nSpraypaint[ m_nNumSpraypaintLogos ].m_szFilename[ 0 ] = '\0';
			m_nSpraypaint[ m_nNumSpraypaintLogos ].m_bCustom = true;

			++m_nNumSpraypaintLogos;

			Q_strncpy( filename, logofile, sizeof(filename) );
			Q_StripFilename( filename );
			Q_strncat( filename, "/", sizeof(filename) );

			// If their current spray is one of the custom ones
			if ( Q_stricmp( filename, MULTIPLAYER_SPRAY_FOLDER MULTIPLAYER_CUSTOM_SPRAY_FOLDER ) == 0 )
			{
				// Check if the file exists
				if ( g_pFullFileSystem->FileExists( logofile, "GAME" ) )
				{
					// Check if it has a matching VMT
					Q_strncpy( filename, logofile, sizeof( filename ) );
					Q_SetExtension( filename, ".vmt", sizeof( filename ) );

					if ( g_pFullFileSystem->FileExists( filename, "GAME" ) )
					{
						// Set this custom logo as our initial item
						const char *pchShortName = Q_UnqualifiedFileName( logofile );

						initialItem = m_nNumSpraypaintLogos - 1;
						Q_StripExtension( pchShortName, m_nSpraypaint[ initialItem ].m_szFilename, sizeof(m_nSpraypaint[ initialItem ].m_szFilename) );
					}
				}
			}
		}
	}

	g_pFullFileSystem->FindClose( fh );

	// Change the height to fit the active items
	pFlyout->SetBGTall( m_nNumSpraypaintLogos * 20 + 5 );

	// Disable the remaining possible choices
	for ( int i = m_nNumSpraypaintLogos; i < MAX_SPRAYPAINT_LOGOS; ++i )
	{
		szCurrentButton[ iCommandNumberPosition ] = i + '0';

		Button *pButton = pFlyout->FindChildButtonByCommand( szCurrentButton );
		if ( pButton )
		{
			pButton->SetVisible( false );
		}
	}

	szCurrentButton[ iCommandNumberPosition ] = initialItem + '0';
	m_drpSpraypaint->SetCurrentSelection( szCurrentButton );
}

// file selected.  This can only happen when someone selects an image to be imported as a spray logo.
void Multiplayer::OnFileSelected(const char *fullpath)
{
	bool bSuccess = SelectCustomLogoFile( fullpath, MULTIPLAYER_CUSTOM_SPRAY_FOLDER );

	if ( bSuccess )
	{
		CGameUIConVarRef cl_logofile( "cl_logofile", true );
		if ( cl_logofile.IsValid() )
		{
			// get the vtfFilename from the path.
			const char *vtfFilename = fullpath + Q_strlen(fullpath);
			while ((vtfFilename > fullpath) && (*(vtfFilename-1) != '\\') && (*(vtfFilename-1) != '/'))
			{
				--vtfFilename;
			}

			char rootFilename[MAX_PATH];
			Q_snprintf( rootFilename, sizeof(rootFilename), MULTIPLAYER_SPRAY_FOLDER MULTIPLAYER_CUSTOM_SPRAY_FOLDER "%s", vtfFilename );
			Q_SetExtension( rootFilename, ".vtf", sizeof( rootFilename ) );

			cl_logofile.SetValue( rootFilename );
		}

		// refresh the logo list so the new spray shows up.
		InitLogoList();
	}
}

void Multiplayer::OnFileSelectionCancelled(void)
{
	InitLogoList();
}

void Multiplayer::OnNotifyChildFocus( vgui::Panel* child )
{
}

void Multiplayer::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter( true );
}

void Multiplayer::OnFlyoutMenuCancelled()
{
}

//=============================================================================
Panel* Multiplayer::NavigateBack()
{
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );

	return BaseClass::NavigateBack();
}

void Multiplayer::PaintBackground()
{
	//BaseClass::DrawDialogBackground( "#GameUI_Multiplayer", NULL, "#L4D360UI_Multiplayer_Desc", NULL, NULL, true );
}

void Multiplayer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}
