//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VInGameChapterSelect.h"
#include "VFooterPanel.h"
#include "EngineInterface.h"
#include "VDropDownMenu.h"

#include "VHybridButton.h"
#include "VGameSettings.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "gameui_util.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
InGameChapterSelect::InGameChapterSelect(Panel *parent, const char *panelName):
	BaseClass(parent, panelName, false, true)
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, true );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}

	GameUI().PreventEngineHideGameUI();

	memset( m_chCampaign, 0, sizeof( m_chCampaign ) );
	m_nChapter = 0;
}

//=============================================================================
InGameChapterSelect::~InGameChapterSelect()
{
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void InGameChapterSelect::PaintBackground()
{
	Panel *pDrpChapter = FindChildByName( "DrpChapter" );
	bool bChapter = ( pDrpChapter && pDrpChapter->IsEnabled() );
	
	char const *szTitle = bChapter ? "#L4D360UI_ChapterSelect_Title" : "#L4D360UI_ChangeScenario";
	char const *szTip = bChapter ? "#L4D360UI_ChapterSelect_Description" : "#L4D360UI_ChangeScenario_Tip";
	
	BaseClass::DrawDialogBackground( szTitle, NULL, szTip, NULL, NULL, true );
}

//=============================================================================
void InGameChapterSelect::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );

	// Determine current game settings
	KeyValues *pGameSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
	KeyValues::AutoDelete autodelete_pGameSettings( pGameSettings );

	char const *szGameMode = pGameSettings->GetString( "game/mode", "campaign" );

	if ( !GameModeIsSingleChapter( szGameMode ) )
		pGameSettings->SetInt( "game/chapter", 1 );

	// Get mission and campaign info
	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoChapter = g_pMatchExtSwarm->GetMapInfo( pGameSettings, &pInfoMission );

	// Check if this is a custom mission?
	if ( pInfoMission && !pInfoMission->GetBool( "builtin" ) )
		pInfoChapter = pInfoMission = NULL;	// trigger to use another builtin mission

	if ( !pInfoMission || !pInfoChapter )
	{
		KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
		for ( pInfoMission = pAllMissions ? pAllMissions->GetFirstTrueSubKey() : NULL;
			  pInfoMission; pInfoMission = pInfoMission->GetNextTrueSubKey() )
		{
			if ( !pInfoMission->GetBool( "builtin" ) )
				continue;

			pInfoChapter = pInfoMission->FindKey( CFmtStr( "modes/%s/1", szGameMode ) );
			if ( pInfoChapter )
				break;
		}
	}

	Assert( pInfoMission && pInfoChapter );

	// set the dropdowns
	DropDownMenu *pMissionDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
	DropDownMenu *pChapterDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpChapter" ) );
	if( pMissionDropDown && pChapterDropDown ) //missions change what the available campaigns are, we should listen on that flyout as well
	{
		pMissionDropDown->SetFlyout( CFmtStr( "FlmMission%s", szGameMode ) );

		if ( pInfoMission && pInfoChapter )
		{
			pMissionDropDown->SetCurrentSelection( CFmtStr( "cmd_campaign_%s", pInfoMission->GetString( "name" ) ) );
			Q_strncpy( m_chCampaign, pInfoMission->GetString( "name" ), ARRAYSIZE( m_chCampaign ) );

			// Set this after setting the mission dropdown, as that will default the chapter to the first in the campaign
			pChapterDropDown->SetCurrentSelection( CFmtStr( "#L4D360UI_Chapter_%d", pInfoChapter->GetInt( "chapter" ) ) );
			m_nChapter = pInfoChapter->GetInt( "chapter" );
		}

		FlyoutMenu *flyout = pMissionDropDown->GetCurrentFlyout();
		if( flyout )
		{
			flyout->CloseMenu( NULL );
		}

		flyout = pChapterDropDown->GetCurrentFlyout();
		if( flyout )
		{
			flyout->CloseMenu( NULL );
		}

		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom( );
		}
		pMissionDropDown->NavigateTo();
		m_ActiveControl = pMissionDropDown;

		// Chapters are directly selectable only in some game modes
		pChapterDropDown->SetEnabled( GameModeIsSingleChapter( szGameMode ) );
	}

	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();
}

//=============================================================================
void InGameChapterSelect::Activate()
{
	BaseClass::Activate();
}

//=============================================================================
void InGameChapterSelect::OnCommand(const char *command)
{
	if ( const char* szMissionItem = StringAfterPrefix( command, "cmd_campaign_" ) )
	{
		if ( !Q_stricmp( szMissionItem, m_chCampaign ) )
			return;	// Setting to same mission

		Q_strncpy( m_chCampaign, szMissionItem, ARRAYSIZE( m_chCampaign ) );

		// Determine current game settings
		KeyValues *pGameSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		KeyValues::AutoDelete autodelete_pGameSettings( pGameSettings );
		if ( !pGameSettings )
			return;

		DropDownMenu *pMissionDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
		if( pMissionDropDown ) //we should become a listener for events pertaining to the mission flyout
		{
			FlyoutMenu* missionFlyout = pMissionDropDown->GetCurrentFlyout();
			if( missionFlyout )
			{
				missionFlyout->SetListener( this );
				if ( vgui::Button *pAddonBtn = missionFlyout->FindChildButtonByCommand( "cmd_addoncampaign" ) )
				{
					pAddonBtn->SetEnabled( false );
				}

				// Disable all other campaigns that cannot be used for a vote
				for ( int k = 0, kNum = missionFlyout->GetChildCount(); k < kNum; ++ k )
				{
					Panel *child = missionFlyout->GetChild( k );
					if ( BaseModHybridButton* button = dynamic_cast< BaseModHybridButton* >( child ) )
					{
						if ( const char* commandValue = button->GetCommand()->GetString( "command", NULL ) )
						{
							if ( char const *szMissionName = StringAfterPrefix( commandValue, "cmd_campaign_" ) )
							{
								pGameSettings->SetString( "game/campaign", szMissionName );
								pGameSettings->SetInt( "game/chapter", 1 );
								if ( !g_pMatchExtSwarm->GetMapInfo( pGameSettings ) )
								{
									button->SetEnabled( false );
								}
							}
						}
					}
				}
			}
		}

	}
	else if ( char const *szChapterSelected = StringAfterPrefix( command, "#L4D360UI_Chapter_" ) )
	{
		m_nChapter = atoi( szChapterSelected );
		UpdateChapterImage( m_chCampaign, m_nChapter );
	}
	else if ( !Q_stricmp( command, "Select" ) )
	{
		KeyValues *pGameSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		KeyValues::AutoDelete autodelete_pGameSettings( pGameSettings );

		if ( !GameModeIsSingleChapter( pGameSettings->GetString( "game/mode" ) ) )
			m_nChapter = 1;

		pGameSettings->SetString( "game/campaign", m_chCampaign );
		pGameSettings->SetInt( "game/chapter", m_nChapter );

		char const *szVoteCommand = "ChangeChapter";

		int iUser = GetGameUIActiveSplitScreenPlayerSlot();
		GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( iUser );

		Panel *pDrpChapter = FindChildByName( "DrpChapter" );
		if ( !pDrpChapter || !pDrpChapter->IsEnabled() )
		{
			szVoteCommand = "ChangeMission";
			m_nChapter = 1;

			engine->ClientCmd( CFmtStr( "callvote %s %s;", szVoteCommand, m_chCampaign ) );
		}
		else if ( KeyValues *pInfoMap = g_pMatchExtSwarm->GetMapInfo( pGameSettings ) )
		{
			engine->ClientCmd( CFmtStr( "callvote %s %s;", szVoteCommand, pInfoMap->GetString( "map" ) ) );
		}

		GameUI().AllowEngineHideGameUI();
		engine->ClientCmd( "gameui_hide" );
		Close();
	}	
	else if ( !Q_strcmp( command, "Cancel" ) )
	{
		GameUI().AllowEngineHideGameUI();
		engine->ClientCmd("gameui_hide");
		Close();
	}
}

void InGameChapterSelect::OnNotifyChildFocus( vgui::Panel* child )
{
	if ( !child )
	{
		return;
	}

	if ( BaseModHybridButton* button = dynamic_cast< BaseModHybridButton* >( child ) )
	{
		if ( const char* commandValue = button->GetCommand()->GetString( "command", NULL ) )
		{
			if ( char const *szMission = StringAfterPrefix( commandValue, "cmd_campaign_" ) )
			{
				UpdateChapterImage( szMission, 0 );
			}
			else if ( char const *szChapterIdx = StringAfterPrefix( commandValue, "#L4D360UI_Chapter_" ) )
			{
				UpdateChapterImage( m_chCampaign, atoi( szChapterIdx ) );
			}
			else if ( !Q_stricmp( "cmd_addoncampaign", commandValue ) )
			{
				UpdateChapterImage( NULL, 0, "maps/unknown" );
			}
		}
	}
}

void InGameChapterSelect::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateChapterImage( m_chCampaign, m_nChapter );
}

void InGameChapterSelect::OnFlyoutMenuCancelled()
{
	UpdateChapterImage( m_chCampaign, m_nChapter );
}

void InGameChapterSelect::UpdateChapterImage( char const *szMission, int nChapter, char const *szForceImage /* = NULL */ )
{
	
}
