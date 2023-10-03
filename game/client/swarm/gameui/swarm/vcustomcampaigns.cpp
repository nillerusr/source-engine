//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VCustomCampaigns.h"
#include "VGenericPanelList.h"
#include "KeyValues.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
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
#include "vgetlegacydata.h"

// use the JPEGLIB_USE_STDIO define so that we can read in jpeg's from outside the game directory tree.  For Spray Import.
#define JPEGLIB_USE_STDIO
#include "jpeglib/jpeglib.h"
#undef JPEGLIB_USE_STDIO
#include <setjmp.h>
#include "bitmap/tgawriter.h"
#include "ivtex.h"

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

KeyValues *g_pPreloadedCustomCampaignListItemLayout = NULL;

//=============================================================================
CustomCampaignListItem::CustomCampaignListItem(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	m_LblName = new Label( this, "LblName", "" );
	m_LblName->DisableMouseInputForThisPanel( true );

	memset( m_campaignContext, 0, sizeof( m_campaignContext ) );

	m_bCurrentlySelected = false;
}

//=============================================================================
void CustomCampaignListItem::SetCustomCampaignName( const char* name )
{
	m_LblName->SetText( name );
}

//=============================================================================
void CustomCampaignListItem::SetCampaignContext( char const *szCampaignContext )
{
	Q_strncpy( m_campaignContext, szCampaignContext, ARRAYSIZE( m_campaignContext ) - 1 );
}

//=============================================================================
char const * CustomCampaignListItem::GetCampaignContext() const
{
	return m_campaignContext;
}

//=============================================================================
void CustomCampaignListItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if ( !g_pPreloadedCustomCampaignListItemLayout )
	{
		const char *pszResource = "Resource/UI/BaseModUI/CustomCampaignListItem.res";
		g_pPreloadedCustomCampaignListItemLayout = new KeyValues( pszResource );
		g_pPreloadedCustomCampaignListItemLayout->LoadFromFile( g_pFullFileSystem, pszResource );
	}

	LoadControlSettings( "", NULL, g_pPreloadedCustomCampaignListItemLayout );

	SetBgColor( pScheme->GetColor( "Button.BgColor", Color( 64, 64, 64, 255 ) ) );

	m_hTextFont = pScheme->GetFont( "DefaultLarge", true );
}

void CustomCampaignListItem::OnMousePressed( vgui::MouseCode code )
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

void CustomCampaignListItem::OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
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

void CustomCampaignListItem::Paint( )
{
	BaseClass::Paint();

	// Draw the graded outline for the selected item only
	if ( m_bCurrentlySelected )
	{
		int nPanelWide, nPanelTall;
		GetSize( nPanelWide, nPanelTall );

		surface()->DrawSetColor( Color( 240, 0, 0, 255 ) );

		// Top lines
		surface()->DrawFilledRectFade( 0, 0, 0.5f * nPanelWide, 2, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, 0, nPanelWide, 2, 255, 0, true );

		// Bottom lines
		surface()->DrawFilledRectFade( 0, nPanelTall-2, 0.5f * nPanelWide, nPanelTall, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, nPanelTall-2, nPanelWide, nPanelTall, 255, 0, true );

		// Text Blotch
		int nTextWide, nTextTall, nNameX, nNameY, nNameWide, nNameTall;
		wchar_t wsCustomCampaignName[120];

		m_LblName->GetPos( nNameX, nNameY );
		m_LblName->GetSize( nNameWide, nNameTall );
		m_LblName->GetText( wsCustomCampaignName, sizeof( wsCustomCampaignName ) );
		surface()->GetTextSize( m_hTextFont, wsCustomCampaignName, nTextWide, nTextTall );
		int nBlotchWide = nTextWide + vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 75 );

		surface()->DrawFilledRectFade( 0, 2, 0.50f * nBlotchWide, nPanelTall-2, 0, 50, true );
		surface()->DrawFilledRectFade( 0.50f * nBlotchWide, 2, nBlotchWide, nPanelTall-2, 50, 0, true );
	}
}

//=============================================================================
//
//=============================================================================
CustomCampaigns::CustomCampaigns( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false ),
	m_pDataSettings( NULL )
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );

	m_GplCustomCampaigns = new GenericPanelList( this, "GplCustomCampaigns", GenericPanelList::ISM_ELEVATOR );
	m_GplCustomCampaigns->ShowScrollProgress( true );
	m_GplCustomCampaigns->SetScrollBarVisible( IsPC() );

	m_hasAddonCampaign = false;
	m_SomeAddonNoSupport = false;

	m_imgLevelImage = NULL;
	m_lblAuthor = NULL;
	m_lblWebsite = NULL;
	m_lblDescription = NULL;
	m_btnSelect = NULL;
	m_lblNoCustomCampaigns = NULL;
}

//=============================================================================
CustomCampaigns::~CustomCampaigns()
{
	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	m_pDataSettings = NULL;
}

//=============================================================================
void CustomCampaigns::SetDataSettings( KeyValues *pSettings )
{
	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	
	m_pDataSettings = pSettings ? pSettings->MakeCopy() : NULL;
}

//=============================================================================
void CustomCampaigns::Activate()
{
	BaseClass::Activate();

	//
	// Show reminder to download legacy data (if necessary)
	//
	if ( GetLegacyData::CheckAndSeeIfShouldShow() )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_GETLEGACYDATA, this, false );
	}
	m_ActiveControl = m_GplCustomCampaigns;

	m_GplCustomCampaigns->RemoveAllPanelItems();

	// Build a list of campaigns
	KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
	if ( !pAllMissions )
		return;

	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "campaign" );

	for ( KeyValues *pMission = pAllMissions->GetFirstTrueSubKey(); pMission; pMission = pMission->GetNextTrueSubKey() )
	{
		// Skip builtin missions
		if ( pMission->GetInt( "builtin" ) )
			continue;

		m_hasAddonCampaign = true;

		// Check this campaign has chapters for this mode
		KeyValues *pFirstChapter = pMission->FindKey( CFmtStr( "modes/%s/1", szGameMode ) );
		if ( !pFirstChapter )
		{
			m_SomeAddonNoSupport = true;
			continue;
		}

		CustomCampaignListItem* panelItem = m_GplCustomCampaigns->AddPanelItem<CustomCampaignListItem>( "CustomCampaignListItem" );

		panelItem->SetParent( m_GplCustomCampaigns );
		panelItem->SetCustomCampaignName( pMission->GetString( "displaytitle" ) );
		panelItem->SetCampaignContext( pMission->GetString( "name" ) );
	}
}

//=============================================================================
void CustomCampaigns::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetupAsDialogStyle();

	m_btnCancel = dynamic_cast< BaseModUI::BaseModHybridButton* >( FindChildByName( "BtnCancel" ) );
	m_btnSelect = dynamic_cast< BaseModUI::BaseModHybridButton* >( FindChildByName( "BtnSelect" ) );
	m_lblNoCustomCampaigns = dynamic_cast< vgui::Label* >( FindChildByName( "LblNoCampaignsFound" ) );
	m_imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) );
	m_lblAuthor = dynamic_cast< vgui::Label* >( FindChildByName( "LblAuthor" ) );
	m_lblWebsite = dynamic_cast< vgui::Label* >( FindChildByName( "LblWebsite" ) );
	m_lblDescription = dynamic_cast< vgui::Label* >( FindChildByName( "LblDescription" ) );

	// Focus on the first item in the list
	if ( m_GplCustomCampaigns->GetPanelItemCount() )
	{
		if ( m_lblNoCustomCampaigns )
		{
			if ( m_SomeAddonNoSupport )
			{
				char const *szGameMode = m_pDataSettings->GetString( "game/mode", "campaign" );

				m_lblNoCustomCampaigns->SetVisible( true );
				m_lblNoCustomCampaigns->SetText( CFmtStr( "#L4D360UI_Some_CustomCampaigns_Installed_%s", szGameMode ) );
			}
			else
			{
				m_lblNoCustomCampaigns->SetVisible( false );
			}
		}
		m_GplCustomCampaigns->NavigateTo();
		m_GplCustomCampaigns->SelectPanelItem( 0, GenericPanelList::SD_DOWN );

		if ( m_btnSelect )
		{
			m_btnSelect->SetEnabled( true );
		}
	}
	else
	{
		if ( m_lblNoCustomCampaigns )
		{
			m_lblNoCustomCampaigns->SetVisible( true );
			const char *noCampaignText = "#L4D360UI_No_CustomCampaigns_Installed";

			if ( m_hasAddonCampaign )
			{
				char const *szGameMode = m_pDataSettings->GetString( "game/mode", "campaign" );

				m_lblNoCustomCampaigns->SetVisible( true );
				m_lblNoCustomCampaigns->SetText( CFmtStr( "#L4D360UI_No_CustomCampaigns_Installed_%s", szGameMode ) );
			}
			else
			{
				m_lblNoCustomCampaigns->SetText( noCampaignText );
			}
		}


		if ( m_imgLevelImage )
			m_imgLevelImage->SetVisible( false );
		if ( m_lblAuthor )
			m_lblAuthor->SetVisible( false );
		if ( m_lblWebsite )
			m_lblWebsite->SetVisible( false );
		if ( m_lblDescription )
			m_lblDescription->SetVisible( false );
		if ( m_btnSelect )
			m_btnSelect->SetEnabled( false );
		if ( m_btnCancel )
		{
			m_btnCancel->NavigateTo();
		}
	}
}

//=============================================================================
void CustomCampaigns::PaintBackground()
{
	BaseClass::DrawDialogBackground( "#L4D360UI_My_CustomCampaigns", NULL, "#L4D360UI_My_CustomCampaigns_Desc", NULL );
}

//=============================================================================
void CustomCampaigns::Select()
{
	CustomCampaignListItem *pSelectedItem = static_cast< CustomCampaignListItem * >( m_GplCustomCampaigns->GetSelectedPanelItem() );
	if ( !pSelectedItem )
		return;

	KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
	if ( !pAllMissions )
		return;

	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "campaign" );

	KeyValues *pFirstChapter = pAllMissions->FindKey( CFmtStr( "%s/modes/%s/1", pSelectedItem->GetCampaignContext(), szGameMode ) );
	if ( !pFirstChapter )
		return;

	CBaseModFrame *pFrame = GetNavBack();
	if ( !pFrame )
		return;

	KeyValues *pEvent = new KeyValues( "OnCustomCampaignSelected" );
	pEvent->SetString( "campaign", pSelectedItem->GetCampaignContext() );
	pEvent->SetInt( "chapter", 0 );
	PostMessage( pFrame, pEvent );
}

//=============================================================================
void CustomCampaigns::OnCommand(const char *command)
{
	if( V_strcmp( command, "Select" ) == 0 )
	{
		Select();

		// Act as though 360 back button was pressed
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	if( V_strcmp( command, "Back" ) == 0 )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	else
	{
		BaseClass::OnCommand( command );
	}	
}

//=============================================================================
void CustomCampaigns::OnItemSelected( const char* panelName )
{
	CustomCampaignListItem *pSelectedItem = static_cast< CustomCampaignListItem * >( m_GplCustomCampaigns->GetSelectedPanelItem() );
	if ( !pSelectedItem )
		return;

	KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
	if ( !pAllMissions )
		return;

	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "campaign" );

	KeyValues *pMission = pAllMissions->FindKey( pSelectedItem->GetCampaignContext() );
	KeyValues *pFirstChapter = pAllMissions->FindKey( CFmtStr( "%s/modes/%s/1", pSelectedItem->GetCampaignContext(), szGameMode ) );
	if ( !pFirstChapter || !pMission )
		return;

	const char *missionImage = pFirstChapter->GetString( "image" );
	const char *missionDesc = pMission->GetString( "description" );
	const char *campaignAuthor = pMission->GetString( "author" );
	const char *campaignWebsite = pMission->GetString( "website" );

	wchar_t finalString[MAX_PATH] = L"";
	wchar_t convertedString[MAX_PATH] = L"";

	if( m_lblAuthor )
	{
		if ( campaignAuthor )
		{
			const wchar_t * authorFormat = g_pVGuiLocalize->Find( "#L4D360UI_CustomCampaign_Author" );
			g_pVGuiLocalize->ConvertANSIToUnicode( campaignAuthor, convertedString, sizeof( convertedString ) );
			if ( authorFormat )
			{
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), authorFormat, 1, convertedString );
				m_lblAuthor->SetText( finalString );
			}
			m_lblAuthor->SetVisible( true );
		}
		else
		{
			m_lblAuthor->SetVisible( false );
		}
	}

	if( m_lblWebsite )
	{
		if ( campaignWebsite )
		{
			const wchar_t * websiteFormat = g_pVGuiLocalize->Find( "#L4D360UI_CustomCampaign_Website" );
			g_pVGuiLocalize->ConvertANSIToUnicode( campaignWebsite, convertedString, sizeof( convertedString ) );
			if ( websiteFormat )
			{
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), websiteFormat, 1, convertedString );
				m_lblWebsite->SetText( finalString );
			}
			m_lblWebsite->SetVisible( true );
		}
		else
		{
			m_lblWebsite->SetVisible( false );
		}
	}

	if( m_lblDescription )
	{
		if ( missionDesc )
		{
			m_lblDescription->SetText( missionDesc );
			m_lblDescription->SetVisible( true );
		}
		else
		{
			m_lblDescription->SetVisible( false );
		}
	}

	if ( m_imgLevelImage )
	{
		m_imgLevelImage->SetVisible( true );
		if( missionImage )
		{
			m_imgLevelImage->SetImage( missionImage );
		}
		else
		{
			m_imgLevelImage->SetImage( "swarm/MissionPics/addonMissionPic" );
		}
	}
}

