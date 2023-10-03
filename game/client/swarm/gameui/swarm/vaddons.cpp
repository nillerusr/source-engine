//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#undef fopen

#include <tier0/platform.h>
#ifdef IS_WINDOWS_PC
#include "windows.h"
#endif

#include "VAddons.h"
#include "VGenericPanelList.h"
#include "KeyValues.h"
#include "VFooterPanel.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "fmtstr.h"
#include "vgui/ISurface.h"
#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Divider.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ProgressBar.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextImage.h"
#include "UtlBuffer.h"
#include "vpklib/packedstore.h"
#include "tier2/fileutils.h"
#include "nb_header_footer.h"
#include "nb_button.h"

// use the JPEGLIB_USE_STDIO define so that we can read in jpeg's from outside the game directory tree.  For Spray Import.
#define JPEGLIB_USE_STDIO
#include "jpeglib/jpeglib.h"
#undef JPEGLIB_USE_STDIO
#include <setjmp.h>
#include "bitmap/tgawriter.h"
#include "ivtex.h"
#include "vgetlegacydata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define ADDONLIST_FILENAME			"addonlist.txt"
#define ADDONINFO_FILENAME			"addoninfo.txt"
#define ADDONS_DIRNAME				"addons"
#define ADDONJPEG_FILENAME			"addonimage.jpg"
#define ADDONTGA_FILENAME			"addonimage.tga"
#define ADDONVTF_FILENAME			"addonimage.vtf"
#define ADDONVMT_FILENAME			"addonimage.vmt"

KeyValues *g_pPreloadedAddonListItemLayout = NULL;

//=============================================================================
AddonListItem::AddonListItem(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	m_LblName = new Label( this, "LblName", "" );
	m_LblName->DisableMouseInputForThisPanel( true );

	m_LblType = new Label( this, "LblType", "" );
	m_LblType->SetTextColorState( Label::CS_DULL );
	m_LblType->DisableMouseInputForThisPanel( true );

	m_BtnEnabled = new CheckButton( this, "AddonEnabledCheckbox", "" );

	m_bCurrentlySelected = false;
}

//=============================================================================
void AddonListItem::SetAddonName( const wchar_t* name )
{
	m_LblName->SetText( name );
}

//=============================================================================
void AddonListItem::SetAddonType( const wchar_t* type )
{
	m_LblType->SetText( type );
}

//=============================================================================
void AddonListItem::SetAddonEnabled( bool bEnabled )
{
	m_BtnEnabled->SetSelected( bEnabled );
}

//=============================================================================
bool AddonListItem::GetAddonEnabled( )
{
	return m_BtnEnabled->IsSelected();
}

//=============================================================================
void AddonListItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if ( !g_pPreloadedAddonListItemLayout )
	{
		const char *pszResource = "Resource/UI/BaseModUI/AddonListItem.res";
		g_pPreloadedAddonListItemLayout = new KeyValues( pszResource );
		g_pPreloadedAddonListItemLayout->LoadFromFile( g_pFullFileSystem, pszResource );
	}

	LoadControlSettings( "", NULL, g_pPreloadedAddonListItemLayout );

	SetBgColor( pScheme->GetColor( "Button.BgColor", Color( 64, 64, 64, 255 ) ) );

	m_hTextFont = pScheme->GetFont( "DefaultLarge", true );
}

void AddonListItem::OnMousePressed( vgui::MouseCode code )
{
	if ( MOUSE_LEFT == code )
	{
		GenericPanelList *pGenericList = dynamic_cast<GenericPanelList*>( GetParent() );

		unsigned short nindex;
		if ( pGenericList && pGenericList->GetPanelItemIndex( this, nindex ) )
		{
			pGenericList->SelectPanelItem( nindex, GenericPanelList::SD_DOWN );
		}
	}

	BaseClass::OnMousePressed( code );
}

void AddonListItem::OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
{
	BaseClass::OnMessage( params, ifromPanel );

	if ( !V_strcmp( params->GetName(), "PanelSelected" ) ) 
	{
		m_bCurrentlySelected = true;
	}
	if ( !V_strcmp( params->GetName(), "PanelUnSelected" ) ) 
	{
		m_bCurrentlySelected = false;
	}

}

void AddonListItem::Paint( )
{
	BaseClass::Paint();

	// Draw the graded outline for the selected item only
	if ( m_bCurrentlySelected )
	{
		int nPanelWide, nPanelTall;
		GetSize( nPanelWide, nPanelTall );

		//surface()->DrawSetColor( Color( 240, 0, 0, 255 ) );
		surface()->DrawSetColor( Color( 169, 213, 255, 128 ) );

		// Top lines
		surface()->DrawFilledRectFade( 0, 0, 0.5f * nPanelWide, 2, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, 0, nPanelWide, 2, 255, 0, true );

		// Bottom lines
		surface()->DrawFilledRectFade( 0, nPanelTall-2, 0.5f * nPanelWide, nPanelTall, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, nPanelTall-2, nPanelWide, nPanelTall, 255, 0, true );

		// Text Blotch
		int nTextWide, nTextTall, nNameX, nNameY, nNameWide, nNameTall;
		wchar_t wsAddonName[120];

		m_LblName->GetPos( nNameX, nNameY );
		m_LblName->GetSize( nNameWide, nNameTall );
		m_LblName->GetText( wsAddonName, sizeof( wsAddonName ) );
		surface()->GetTextSize( m_hTextFont, wsAddonName, nTextWide, nTextTall );
		int nBlotchWide = nTextWide + vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 75 );

		surface()->DrawFilledRectFade( 0, 2, 0.50f * nBlotchWide, nPanelTall-2, 0, 50, true );
		surface()->DrawFilledRectFade( 0.50f * nBlotchWide, 2, nBlotchWide, nPanelTall-2, 50, 0, true );
	}
}

void GetPrimaryModDirectory( char *pcModPath, int nSize )
{
	g_pFullFileSystem->GetSearchPath( "MOD", false, pcModPath, nSize );

	// It's possible that we have multiple MOD directories if there is DLC installed. If that's the case get the last one
	// in the semi-colon delimited list
	char *pSemi = V_strrchr( pcModPath, ';');
	if ( pSemi )
	{
		V_strncpy( pcModPath, ++pSemi, MAX_PATH );
	}
}

//=============================================================================
//
//=============================================================================
Addons::Addons( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, false, true )
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose(true);
	SetProportional( true );

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 75, 350 );

	m_GplAddons = new GenericPanelList( this, "GplAddons", GenericPanelList::ISM_ELEVATOR );
	m_GplAddons->ShowScrollProgress( true );
	m_GplAddons->SetScrollBarVisible( IsPC() );

	m_LblName = new Label( this, "LblName", "" );
	m_LblNoAddons = new Label( this, "LblNoAddons", "" );
	m_LblType = new Label( this, "LblType", "" );
	m_LblDescription = new Label( this, "LblDescription", "" );
	m_ImgAddonIcon = new ImagePanel( this, "ImgAddonIcon" );
	m_LblAuthor = new Label( this, "LblAuthor", "" );

	m_pSupportRequiredPanel = NULL;
	m_pInstallingSupportPanel = NULL;

	m_pDoNotAskForAssociation = new CvarToggleCheckButton<CGameUIConVarRef>( 
		this, 
		"CheckButtonAssociation", 
		"",
		"cl_ignore_vpk_association",
		true );

	SetLowerGarnishEnabled( true );
	m_pAddonList = NULL;
	m_ActiveControl = m_GplAddons;

	LoadControlSettings("Resource/UI/BaseModUI/Addons.res");

	UpdateFooter();
}

//=============================================================================
Addons::~Addons()
{
	delete m_GplAddons;
	m_pAddonList->deleteThis();
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void Addons::Activate()
{
	BaseClass::Activate();

	m_GplAddons->RemoveAllPanelItems();
	m_addonInfoList.RemoveAll();
	m_pAddonList ? m_pAddonList->deleteThis() : NULL;

	//
	// Get the list of addons
	//

	// Load particular info for each addon
	if ( LoadAddonListFile( m_pAddonList ) )
	{
		for ( KeyValues *pCur = m_pAddonList->GetFirstValue(); pCur; pCur = pCur->GetNextValue() )
		{
			char szAddonDirName[60];
			bool bIsVPK = true;

			// If the entry in the list is a .vpk then 
			if ( V_stristr( pCur->GetName(), ".vpk" ) )
			{
				V_StripExtension( pCur->GetName(), szAddonDirName, sizeof( szAddonDirName ) );
				ExtractAddonMetadata( szAddonDirName );
			}
			else
			{
				bIsVPK = false;
				V_strncpy( szAddonDirName, pCur->GetName(), sizeof( szAddonDirName ) );
			}

			Addons::AddonInfo addonInfo;

			// Copy info to the AddonInfo struct
			KeyValues *pAddonInfo;

			if ( LoadAddonInfoFile( pAddonInfo, szAddonDirName, bIsVPK ) )
			{
				// Convert JPEG image to VTF, if necessary
				GetAddonImage( pCur->GetName(), addonInfo.szImageName, sizeof( addonInfo.szImageName ), bIsVPK  );

				// Copy values from particular keys
				V_strncpy( addonInfo.szDirectory, pCur->GetName(), sizeof( addonInfo.szDirectory ) ); 

				V_wcsncpy( addonInfo.szName, pAddonInfo->GetWString( "addontitle", g_pVGuiLocalize->Find( "#L4D360UI_Addon_None_Specified" ) ), sizeof( addonInfo.szName ) );
				V_wcsncpy( addonInfo.szAuthor, pAddonInfo->GetWString( "addonauthor", g_pVGuiLocalize->Find( "#L4D360UI_Addon_None_Specified" ) ), sizeof( addonInfo.szAuthor ) );
				V_wcsncpy( addonInfo.szDescription, pAddonInfo->GetWString( "addonDescription", g_pVGuiLocalize->Find( "#L4D360UI_Addon_None_Specified" ) ), sizeof( addonInfo.szDescription ) );
				addonInfo.bEnabled = pCur->GetInt() != 0;

				// Generate the types string based on the value of keys
				bool bCampaign = false, bMaps = false, bSkin = false, bWeapon = false, bBoss = false, bCommon = false, bSurvivor = false, bSound = false, bMusic = false, bScript = false, bProp = false;

				bCampaign = pAddonInfo->GetInt( "addonContent_Campaign" ) != 0;
				bMaps = ( pAddonInfo->GetInt( "addonContent_Map" ) != 0 ) && !bCampaign;
				bSkin = pAddonInfo->GetInt( "addonContent_Skin" ) != 0;
				bWeapon = pAddonInfo->GetInt( "addonContent_weapon" ) != 0;
				bBoss = pAddonInfo->GetInt( "addonContent_BossInfected" ) != 0;
				bCommon = pAddonInfo->GetInt( "addonContent_CommonInfected" ) != 0;
				bSurvivor = pAddonInfo->GetInt( "addonContent_Survivor" ) != 0;
				bSound = pAddonInfo->GetInt( "addonContent_Sound" ) != 0;
				bScript = pAddonInfo->GetInt( "addonContent_Script" ) != 0;
				bMusic = pAddonInfo->GetInt( "addonContent_Music" ) != 0;
				bProp = pAddonInfo->GetInt( "addonContent_prop" ) != 0;

				// Make the addon types string based on the flags
				addonInfo.szTypes[0] = NULL;
				bCampaign ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Campaign" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bMaps ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Map" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bSkin ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Skin" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bWeapon ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Weapon" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bBoss ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Boss" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bCommon ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Common" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bSurvivor ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Survivor" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bSound ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Sound" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bScript ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Script" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bMusic ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Music" ), sizeof( addonInfo.szTypes ) ) : NULL;
				bProp ? wcsncat( addonInfo.szTypes, g_pVGuiLocalize->Find( "#L4D360UI_Addon_Type_Props" ), sizeof( addonInfo.szTypes ) ) : NULL;

				// Remove trailing ','
				if ( wcslen( addonInfo.szTypes ) )
				{
					wchar_t *pwcComma = wcsrchr( addonInfo.szTypes, ',' );
					if ( pwcComma )
					{
						*pwcComma = NULL;
					}
				}
			}

			m_addonInfoList.AddToTail( addonInfo );

			// Get rid of the temp files
			if ( bIsVPK )
			{
				char tempFilename[MAX_PATH];
				char modPath[MAX_PATH];
				
				GetPrimaryModDirectory( modPath, MAX_PATH );
				V_snprintf( tempFilename, sizeof( tempFilename ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONINFO_FILENAME );
				g_pFullFileSystem->RemoveFile( tempFilename );
				V_snprintf( tempFilename, sizeof( tempFilename ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONJPEG_FILENAME );
				g_pFullFileSystem->RemoveFile( tempFilename );
			}
		}
	}

	//
	// Add the addons to the list panel 
	//
	FOR_EACH_VEC( m_addonInfoList, i )
	{
		AddonListItem* panelItem = m_GplAddons->AddPanelItem<AddonListItem>( "AddonListItem" );

		panelItem->SetParent( m_GplAddons );
		panelItem->SetAddonName( m_addonInfoList[i].szName );
		panelItem->SetAddonType( m_addonInfoList[i].szTypes );
		panelItem->SetAddonEnabled( m_addonInfoList[i].bEnabled );
	}

	// Focus on the first item in the list
	if ( m_addonInfoList.Count() > 0 )
	{
		m_GplAddons->NavigateTo();
		m_GplAddons->SelectPanelItem( 0, GenericPanelList::SD_DOWN );
		m_ImgAddonIcon->SetShouldScaleImage( true );
	}
	else if ( GetLegacyData::IsInstalled() && !GetLegacyData::IsInstalling() )
	{
		// Show the "no add-ons" message
		m_LblNoAddons->SetText( "#L4D360UI_No_Addons_Installed" );
	}
}

void Addons::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
	}
}



//=============================================================================
void Addons::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetupAsDialogStyle();

	// show the disable if on
	CGameUIConVarRef cl_ignore_vpk_association( "cl_ignore_vpk_association" );
	if ( !cl_ignore_vpk_association.GetBool() )
	{
		m_pDoNotAskForAssociation->SetVisible( false );
		vgui::Label	*pLblCheckNoAssociation = dynamic_cast< vgui::Label * > ( FindChildByName( "LblCheckNoAssociation" ) );
		if ( pLblCheckNoAssociation )
		{
			pLblCheckNoAssociation->SetVisible( false );
		}
	}

	if ( GetLegacyData::CheckAndSeeIfShouldShow() )
	{
		m_pSupportRequiredPanel = dynamic_cast< vgui::EditablePanel * >( FindChildByName( "AddonSupportRequiredPanel" ) );
		m_pInstallingSupportPanel = dynamic_cast< vgui::EditablePanel * >( FindChildByName( "InstallingAddonSupportPanel" ) );

		if ( m_pSupportRequiredPanel )
		{
			m_pSupportRequiredPanel->SetVisible( true );
			SetControlVisible( "IconInstallSupport", true );
			SetControlVisible( "BtnInstallSupport", true );
		}

		if ( m_pInstallingSupportPanel )
		{
			m_pInstallingSupportPanel->SetVisible( false );
		}
	}	
}

//=============================================================================
void Addons::PaintBackground()
{
	//BaseClass::DrawDialogBackground( "#L4D360UI_My_Addons", NULL, "#L4D360UI_My_Addons_Desc", NULL );
}

//=============================================================================
void Addons::OnCommand(const char *command)
{
	if( V_strcmp( command, "Back" ) == 0 )
	{
		int i = 0;
		for ( KeyValues *pCur = m_pAddonList->GetFirstValue(); pCur; pCur = pCur->GetNextValue() )
		{
			AddonListItem *pPanelItem = static_cast<AddonListItem*>( m_GplAddons->GetPanelItem( i ) );
			int nEnabled = pPanelItem->GetAddonEnabled() ? 1 : 0;
			m_pAddonList->SetInt( pCur->GetName(), nEnabled );
			i++;
		}

		char szModPath[MAX_PATH];
		char szAddOnListPath[MAX_PATH];

		GetPrimaryModDirectory( szModPath, MAX_PATH );
		V_snprintf( szAddOnListPath, sizeof( szAddOnListPath ), "%s%s", szModPath, ADDONLIST_FILENAME );
		m_pAddonList->SaveToFile( g_pFullFileSystem, szAddOnListPath );
		engine->ClientCmd( "update_addon_paths; mission_reload" );

		if ( m_pDoNotAskForAssociation )
		{
			m_pDoNotAskForAssociation->ApplyChanges();
		}

		// Act as though 360 back button was pressed
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	else if ( V_strcmp( command, "InstallSupport" ) == 0 )
	{
		// This is where we send the command to install support!
#ifdef IS_WINDOWS_PC
		// App ID for the legacy addon data is 564
		ShellExecute ( 0, "open", "steam://install/564", NULL, 0, SW_SHOW );
#endif		
	}
	else
	{
		BaseClass::OnCommand( command );
	}	
}

//---------------------------------------------------------------------------------------------------------------------
// Loads the optional addonlist.txt file which lives in the same location as gameinfo.txt and defines additional search
// paths for content add-ons to mods.
//---------------------------------------------------------------------------------------------------------------------
bool Addons::LoadAddonListFile( KeyValues *&pAddons )
{
	char addonlistFilename[MAX_PATH];
	char modPath[MAX_PATH];

	GetPrimaryModDirectory( modPath, MAX_PATH );
	V_snprintf( addonlistFilename, sizeof( addonlistFilename), "%s%s", modPath, ADDONLIST_FILENAME );
	pAddons = new KeyValues( "AddonList" );

	return pAddons->LoadFromFile( g_pFullFileSystem, addonlistFilename );
}

//---------------------------------------------------------------------------------------------------------------------
// Loads the optional addonlist.txt file which lives in the same location as gameinfo.txt and defines additional search
// paths for content add-ons to mods.
//---------------------------------------------------------------------------------------------------------------------
bool Addons::LoadAddonInfoFile( KeyValues *&pAddonInfo, const char *pcAddonDir, bool bIsVPK )
{
	char addoninfoFilename[MAX_PATH];
	char modPath[MAX_PATH];

	GetPrimaryModDirectory( modPath, MAX_PATH );

	if ( bIsVPK )
	{
		V_snprintf( addoninfoFilename, sizeof( addoninfoFilename ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONINFO_FILENAME );	
	}
	else
	{
		V_snprintf( addoninfoFilename, sizeof( addoninfoFilename ), "%s%s%c%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, ADDONINFO_FILENAME );	
	}
	
	pAddonInfo = new KeyValues( "AddonInfo");

	return pAddonInfo->LoadFromFile( g_pFullFileSystem, addoninfoFilename );
}

void Addons::GetAddonImage( const char *pcAddonDir, char *pcImagePath, int nImagePathSize, bool bIsVPK )
{
	char jpegFilename[MAX_PATH];
	char tgaFilename[MAX_PATH];
	char vtfFilename[MAX_PATH];
	char vmtFilename[MAX_PATH];
	char dirPath[MAX_PATH];
	char modPath[MAX_PATH];

	GetPrimaryModDirectory( modPath, MAX_PATH );
	V_snprintf( tgaFilename, sizeof( tgaFilename ), "%s%s%s%c%s%c%s", modPath, "materials/vgui/", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, ADDONTGA_FILENAME );	

	if ( bIsVPK )
	{
		V_snprintf( jpegFilename, sizeof( jpegFilename ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONJPEG_FILENAME );	
	}
	else
	{
		V_snprintf( jpegFilename, sizeof( jpegFilename ), "%s%s%c%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, ADDONJPEG_FILENAME );	
	}

	V_snprintf( vtfFilename, sizeof( vtfFilename ), "%s%s%s%c%s%c%s", modPath, "materials/vgui/", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, ADDONVTF_FILENAME );
	V_snprintf( vmtFilename, sizeof( vmtFilename ), "%s%s%s%c%s%c%s", modPath, "materials/vgui/", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, ADDONVMT_FILENAME );

	// Create necessary subdirectories to hold converted files
	V_snprintf( dirPath, sizeof( dirPath ), "%s%s%s%c%s", modPath, "materials/vgui/", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir );
	g_pFullFileSystem->CreateDirHierarchy( dirPath );

	// Bypass the conversion process if we have a VMT
	FileFindHandle_t findHandleVMT;

	bool bHaveFile = ( NULL != g_pFullFileSystem->FindFirst( vmtFilename, &findHandleVMT ) );
	g_pFullFileSystem->FindClose( findHandleVMT );

	if ( bHaveFile )
	{
		char vmtBasePath[MAX_PATH];

		V_snprintf( vmtBasePath, sizeof( vmtBasePath ), "%s%c%s%c%s", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, "addonimage" );
		V_strncpy( pcImagePath, vmtBasePath, nImagePathSize );

	}
	else
	{
		// Copy the failsafe path
		V_strncpy( pcImagePath, "common/swarm_cycle", nImagePathSize );

		if ( CE_SUCCESS == SConvertJPEGToTGA( jpegFilename, tgaFilename) )
		{
			if ( CE_SUCCESS == ConvertTGAToVTF( tgaFilename ) )
			{
				if ( CE_SUCCESS == WriteVMT( vtfFilename, pcAddonDir ) )
				{
					char vmtBasePath[MAX_PATH];

					V_snprintf( vmtBasePath, sizeof( vmtBasePath ), "%s%c%s%c%s", ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir, CORRECT_PATH_SEPARATOR, "addonimage" );
					V_strncpy( pcImagePath, vmtBasePath, nImagePathSize );
				}
			}
		}
	}
}

void Addons::OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
{
	BaseClass::OnMessage( params, ifromPanel );
	
	if ( Q_strcmp( params->GetName(), "OnItemSelected" ) == 0 ) 
	{
		int index = ((KeyValues*)params)->GetInt( "index" );

		// Set the details for the item
		SetDetailsUIForAddon( index );
	}
}

void Addons::SetDetailsUIForAddon( int nIndex )
{
	wchar_t wsAuthorLabel[130];

	V_wcsncpy( wsAuthorLabel, g_pVGuiLocalize->Find( "#L4D360UI_Addon_By" ), sizeof( wsAuthorLabel ) );
	wcsncat( wsAuthorLabel, m_addonInfoList[nIndex].szAuthor, sizeof( wsAuthorLabel ) );

	m_LblName->SetText( m_addonInfoList[nIndex].szName );
	m_LblType->SetText( m_addonInfoList[nIndex].szTypes );
	m_LblAuthor->SetText( wsAuthorLabel );
	m_LblDescription->SetText( m_addonInfoList[nIndex].szDescription );
	m_ImgAddonIcon->SetImage( m_addonInfoList[nIndex].szImageName );
}


//-----------------------------------------------------------------------------
// Everything below was copied from the UI options page for converting sprays.
// TODO: Move these functions to a library so that they can be shared more 
//       sanely.
//-----------------------------------------------------------------------------
struct ValveJpegErrorHandler_t 
{
	// The default manager
	struct jpeg_error_mgr	m_Base;
	// For handling any errors
	jmp_buf					m_ErrorContext;
};

//-----------------------------------------------------------------------------
// Purpose: We'll override the default error handler so we can deal with errors without having to exit the engine
//-----------------------------------------------------------------------------
static void ValveJpegErrorHandler( j_common_ptr cinfo )
{
	ValveJpegErrorHandler_t *pError = reinterpret_cast< ValveJpegErrorHandler_t * >( cinfo->err );

	char buffer[ JMSG_LENGTH_MAX ];

	/* Create the message */
	( *cinfo->err->format_message )( cinfo, buffer );

	Warning( "%s\n", buffer );

	// Bail
	longjmp( pError->m_ErrorContext, 1 );
}

// convert the JPEG file given to a TGA file at the given output path.
Addons::ConversionErrorType Addons::SConvertJPEGToTGA(const char *jpegpath, const char *tgaPath)
{
#if !defined( _X360 )
	struct jpeg_decompress_struct jpegInfo;
	struct ValveJpegErrorHandler_t jerr;
	JSAMPROW row_pointer[1];
	int row_stride;
	int cur_row = 0;

	// image attributes
	int image_height;
	int image_width;

	// open the jpeg image file.
	FILE *infile = fopen(jpegpath, "rb");
	if (infile == NULL)
	{
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// setup error to print to stderr.
	jpegInfo.err = jpeg_std_error(&jerr.m_Base);

	jpegInfo.err->error_exit = &ValveJpegErrorHandler;

	// create the decompress struct.
	jpeg_create_decompress(&jpegInfo);

	if ( setjmp( jerr.m_ErrorContext ) )
	{
		// Get here if there is any error
		jpeg_destroy_decompress( &jpegInfo );

		fclose(infile);

		return CE_ERROR_PARSING_SOURCE;
	}

	jpeg_stdio_src(&jpegInfo, infile);

	// read in the jpeg header and make sure that's all good.
	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK)
	{
		fclose(infile);
		return CE_ERROR_PARSING_SOURCE;
	}

	// start the decompress with the jpeg engine.
	if ( !jpeg_start_decompress(&jpegInfo) )
	{
		jpeg_destroy_decompress(&jpegInfo);
		fclose(infile);
		return CE_ERROR_PARSING_SOURCE;
	}

	// now that we've started the decompress with the jpeg lib, we have the attributes of the
	// image ready to be read out of the decompress struct.
	row_stride = jpegInfo.output_width * jpegInfo.output_components;
	image_height = jpegInfo.image_height;
	image_width = jpegInfo.image_width;
	int mem_required = jpegInfo.image_height * jpegInfo.image_width * jpegInfo.output_components;

	// allocate the memory to read the image data into.
	unsigned char *buf = (unsigned char *)malloc(mem_required);
	if (buf == NULL)
	{
		jpeg_destroy_decompress(&jpegInfo);
		fclose(infile);
		return CE_MEMORY_ERROR;
	}

	// read in all the scan lines of the image into our image data buffer.
	bool working = true;
	while (working && (jpegInfo.output_scanline < jpegInfo.output_height))
	{
		row_pointer[0] = &(buf[cur_row * row_stride]);
		if ( !jpeg_read_scanlines(&jpegInfo, row_pointer, 1) )
		{
			working = false;
		}
		++cur_row;
	}

	if (!working)
	{
		free(buf);
		jpeg_destroy_decompress(&jpegInfo);
		fclose(infile);
		return CE_ERROR_PARSING_SOURCE;
	}

	jpeg_finish_decompress(&jpegInfo);

	fclose(infile);

	// ok, at this point we have read in the JPEG image to our buffer, now we need to write it out as a TGA file.
	CUtlBuffer outBuf;
	bool bRetVal = TGAWriter::WriteToBuffer( buf, outBuf, image_width, image_height, IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888 );
	if ( bRetVal )
	{
		if ( !g_pFullFileSystem->WriteFile( tgaPath, NULL, outBuf ) )
		{
			bRetVal = false;
		}
	}

	free(buf);
	return bRetVal ? CE_SUCCESS : CE_ERROR_WRITING_OUTPUT_FILE;

#else
	return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
#endif
}

struct TGAHeader {
	byte  identsize;          // size of ID field that follows 18 byte header (0 usually)
	byte  colourmaptype;      // type of colour map 0=none, 1=has palette
	byte  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	short colourmapstart;     // first colour map entry in palette
	short colourmaplength;    // number of colours in palette
	byte  colourmapbits;      // number of bits per palette entry 15,16,24,32

	short xstart;             // image x origin
	short ystart;             // image y origin
	short width;              // image width in pixels
	short height;             // image height in pixels
	byte  bits;               // image bits per pixel 8,16,24,32
	byte  descriptor;         // image descriptor bits (vh flip bits)
};


static void ReadTGAHeader(FILE *infile, TGAHeader &header)
{
	if (infile == NULL)
	{
		return;
	}

	fread(&header.identsize, sizeof(header.identsize), 1, infile);
	fread(&header.colourmaptype, sizeof(header.colourmaptype), 1, infile);
	fread(&header.imagetype, sizeof(header.imagetype), 1, infile);
	fread(&header.colourmapstart, sizeof(header.colourmapstart), 1, infile);
	fread(&header.colourmaplength, sizeof(header.colourmaplength), 1, infile);
	fread(&header.colourmapbits, sizeof(header.colourmapbits), 1, infile);
	fread(&header.xstart, sizeof(header.xstart), 1, infile);
	fread(&header.ystart, sizeof(header.ystart), 1, infile);
	fread(&header.width, sizeof(header.width), 1, infile);
	fread(&header.height, sizeof(header.height), 1, infile);
	fread(&header.bits, sizeof(header.bits), 1, infile);
	fread(&header.descriptor, sizeof(header.descriptor), 1, infile);
}

// convert TGA file at the given location to a VTF file of the same root name at the same location.
Addons::ConversionErrorType Addons::ConvertTGAToVTF(const char *tgaPath)
{
	FILE *infile = fopen(tgaPath, "rb");
	if (infile == NULL)
	{
		return CE_CANT_OPEN_SOURCE_FILE;
	}

	// read out the header of the image.
	TGAHeader header;
	ReadTGAHeader(infile, header);

	// check to make sure that the TGA has the proper dimensions and size.
	if (!IsPowerOfTwo(header.width) || !IsPowerOfTwo(header.height))
	{
		fclose(infile);
		return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	}

	// check to make sure that the TGA isn't too big.
	if ((header.width > 256) || (header.height > 256))
	{
		fclose(infile);
		return CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED;
	}

	int imageMemoryFootprint = header.width * header.height * header.bits / 8;

	CUtlBuffer inbuf(0, imageMemoryFootprint);

	// read in the image
	int nBytesRead = fread(inbuf.Base(), imageMemoryFootprint, 1, infile);

	fclose(infile);
	inbuf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	// load vtex_dll.dll and get the interface to it.
	CSysModule *vtexmod = Sys_LoadModule("vtex_dll");
	if (vtexmod == NULL)
	{
		return CE_ERROR_LOADING_DLL;
	}

	CreateInterfaceFn factory = Sys_GetFactory(vtexmod);
	if (factory == NULL)
	{
		Sys_UnloadModule(vtexmod);
		return CE_ERROR_LOADING_DLL;
	}

	IVTex *vtex = (IVTex *)factory(IVTEX_VERSION_STRING, NULL);
	if (vtex == NULL)
	{
		Sys_UnloadModule(vtexmod);
		return CE_ERROR_LOADING_DLL;
	}

	char *vtfParams[4];

	// the 0th entry is skipped cause normally thats the program name.
	vtfParams[0] = "";
	vtfParams[1] = "-quiet";
	vtfParams[2] = "-dontusegamedir";
	vtfParams[3] = (char *)tgaPath;

	// call vtex to do the conversion.
	vtex->VTex(4, vtfParams);

	Sys_UnloadModule(vtexmod);

	return CE_SUCCESS;
}

// write a VMT file for the spray VTF file at the given path.
Addons::ConversionErrorType Addons::WriteVMT( const char *vtfPath, const char *pcAddonDir )
{
	if (vtfPath == NULL)
	{
		return CE_ERROR_WRITING_OUTPUT_FILE;
	}

	// make the vmt filename
	char vmtPath[MAX_PATH*4];
	Q_strncpy(vmtPath, vtfPath, sizeof(vmtPath));
	char *c = vmtPath + strlen(vmtPath);
	while ((c > vmtPath) && (*(c-1) != '.'))
	{
		--c;
	}
	Q_strncpy(c, "vmt", sizeof(vmtPath) - (c - vmtPath));

	// get the root filename for the vtf file
	char filename[MAX_PATH];
	while ((c > vmtPath) && (*(c-1) != '/') && (*(c-1) != '\\'))
	{
		--c;
	}

	int i = 0;
	while ((*c != 0) && (*c != '.'))
	{
		filename[i++] = *(c++);
	}
	filename[i] = 0;

	// create the vmt file.
	FILE *vmtFile = fopen(vmtPath, "w");
	if (vmtFile == NULL)
	{
		return CE_ERROR_WRITING_OUTPUT_FILE;
	}

	// write the contents of the file.
	fprintf(vmtFile, "\"UnlitGeneric\"\n{\n\t\"$basetexture\"	\"%s%s%c%s%s\"\n\t\"$translucent\" 1\n\t\"$vertexcolor\" 1\n\t\"$vertexalpha\" 1\n\t\"$no_fullbright\" 1\n\t\"$ignorez\" 1\n\t\"$nolod\" 1\n}\n", "vgui/", ADDONS_DIRNAME, '/', pcAddonDir, "/addonimage" );

	fclose(vmtFile);

	return CE_SUCCESS;
}


void Addons::ExtractAddonMetadata( const char *pcAddonDir )
{
	char szModPath[MAX_PATH];
	char szAddonVPKFullPath[MAX_PATH];
	char szAddonInfoFullPath[MAX_PATH];

	GetPrimaryModDirectory( szModPath, MAX_PATH );

	// Construct path to the VPK and create the object
	V_snprintf( szAddonVPKFullPath, sizeof( szAddonVPKFullPath ), "%s%s%c%s.vpk", szModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pcAddonDir );
	CPackedStore mypack( szAddonVPKFullPath, g_pFullFileSystem );
	
	// Construct the output path for the addoninfo.txt and write it out
	V_snprintf( szAddonInfoFullPath, sizeof( szAddonInfoFullPath ), "%s%s%c%s", szModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONINFO_FILENAME );
	CPackedStoreFileHandle pInfoData = mypack.OpenFile( ADDONINFO_FILENAME );
	if ( pInfoData )
	{
		COutputFile outF( szAddonInfoFullPath );
		int nBytes = pInfoData.m_nFileSize;
		while( nBytes )
		{
			char cpBuf[65535];
			int nReadSize = MIN( sizeof( cpBuf ), nBytes );
			mypack.ReadData( pInfoData, cpBuf, nReadSize );
			outF.Write( cpBuf, nReadSize );
			nBytes -= nReadSize;
		}
		outF.Close();
	}

	// Construct the output path for the addonimage.jpg and write it out
	V_snprintf( szAddonInfoFullPath, sizeof( szAddonInfoFullPath ), "%s%s%c%s", szModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, ADDONJPEG_FILENAME );
	CPackedStoreFileHandle pJPEGData = mypack.OpenFile( ADDONJPEG_FILENAME );
	if ( pJPEGData )
	{
		COutputFile outF( szAddonInfoFullPath );
		int nBytes = pJPEGData.m_nFileSize;
		while( nBytes )
		{
			char cpBuf[65535];
			int nReadSize = MIN( sizeof( cpBuf ), nBytes );
			mypack.ReadData( pJPEGData, cpBuf, nReadSize );
			outF.Write( cpBuf, nReadSize );
			nBytes -= nReadSize;
		}
		outF.Close();
	}
}

void Addons::OnThink()
{
	BaseClass::OnThink();

	if ( GetLegacyData::IsInstalled() && m_pInstallingSupportPanel && !m_pInstallingSupportPanel->IsVisible() )
	{
		if ( m_pSupportRequiredPanel )
		{
			m_pSupportRequiredPanel->SetVisible( false );
			SetControlVisible( "IconInstallSupport", false );
			SetControlVisible( "BtnInstallSupport", false );
		}

		if ( m_pInstallingSupportPanel )
		{
			m_pInstallingSupportPanel->SetVisible( true );
		}
	}


	// If the 'installing' panel is visible, spin the spinner
	if ( m_pInstallingSupportPanel && m_pInstallingSupportPanel->IsVisible() )
	{
		static float flLastSpinnerTime = 0.0f;
		static int iSpinnerFrame = 0;

		vgui::ImagePanel *pSpinner = dynamic_cast< vgui::ImagePanel* >( m_pInstallingSupportPanel->FindChildByName( "SearchingIcon" ) );
		if ( pSpinner )
		{
			float flTime = Plat_FloatTime();
			if ( ( flLastSpinnerTime + 0.1f ) < flTime )
			{
				flLastSpinnerTime = flTime;
				pSpinner->SetFrame( iSpinnerFrame++ );
			}
		}

		// Lets check if the data has finished installing
		if ( GetLegacyData::IsInstalled() && !GetLegacyData::IsInstalling() )
		{
			m_pInstallingSupportPanel->SetVisible( false );

			// If we don't have any add-ons, show that message
			if ( !m_addonInfoList.Count() && m_LblNoAddons )
			{
				m_LblNoAddons->SetText( "#L4D360UI_No_Addons_Installed" );
				m_LblNoAddons->SetVisible( true );
			}
		}
	}
}