#include "cbase.h"

#include "nb_main_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/ImagePanel.h"
#include "nb_lobby_tooltip.h"
#include "nb_lobby_row.h"
#include "nb_lobby_row_small.h"
#include "nb_select_marine_panel.h"
#include "nb_select_weapon_panel.h"
#include "nb_vote_panel.h"
#include "asw_briefing.h"
#include <vgui/ILocalize.h>
#include "asw_marine_profile.h"
#include "ForceReadyPanel.h"
#include "asw_gamerules.h"
#include "KeyValues.h"
#include "nb_mission_summary.h"
#include "nb_mission_panel.h"
#include "nb_mission_options.h"
#include "nb_spend_skill_points.h"
#include "nb_header_footer.h"
#include "nb_select_mission_panel.h"
#include "nb_button.h"
#include "gameui/swarm/uigamedata.h"
#include "c_asw_game_resource.h"
#include "vgui_bitmapbutton.h"
#include "clientmode_asw.h"
#include "c_asw_player.h"
#include "nb_promotion_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define CHAT_BUTTON_ICON "vgui/briefing/chat_icon"
#define VOTE_BUTTON_ICON "vgui/briefing/vote_icon"


CUtlVector<int> CNB_Main_Panel::s_QueuedSpendSkillPoints;

void CNB_Main_Panel::QueueSpendSkillPoints( int nProfileIndex )
{
	if ( s_QueuedSpendSkillPoints.Find( nProfileIndex ) != s_QueuedSpendSkillPoints.InvalidIndex() )
		return;

	s_QueuedSpendSkillPoints.AddToTail( nProfileIndex );
}

void CNB_Main_Panel::RemoveFromSpendQueue( int nProfileIndex )
{
	for ( int i = s_QueuedSpendSkillPoints.Count() - 1; i >= 0; i-- )
	{
		if ( s_QueuedSpendSkillPoints[i] == nProfileIndex )
		{
			s_QueuedSpendSkillPoints.Remove( i );
		}
	}
}

CNB_Main_Panel::CNB_Main_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pLeaderLabel = new vgui::Label( this, "LeaderLabel", "" );
	m_pReadyCheckImage = new vgui::ImagePanel( this, "ReadyCheckImage" );
	m_pLobbyRow0 = new CNB_Lobby_Row( this, "LobbyRow0" );
	m_pLobbyRow1 = new CNB_Lobby_Row_Small( this, "LobbyRow1" );
	m_pLobbyRow2 = new CNB_Lobby_Row_Small( this, "LobbyRow2" );
	m_pLobbyRow3 = new CNB_Lobby_Row_Small( this, "LobbyRow3" );
	m_pLobbyTooltip = new CNB_Lobby_Tooltip( this, "LobbyTooltip" );
	m_pMissionSummary = new CNB_Mission_Summary( this, "MissionSummary" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pVotePanel = new CNB_Vote_Panel( this, "VotePanel" );
	m_pReadyButton = new CNB_Button( this, "ReadyButton", "", this, "ReadyButton" );
	m_pMissionDetailsButton = new CNB_Button( this, "MissionDetailsButton", "", this, "MissionDetailsButton" );
	m_pFriendsButton = new CNB_Button( this, "FriendsButton", "", this, "FriendsButton" );
	m_pChatButton = new CBitmapButton( this, "ChatButton", "" );
	m_pChatButton->AddActionSignalTarget( this );
	m_pChatButton->SetCommand( "ChatButton" );
	m_pVoteButton = new CBitmapButton( this, "VoteButton", "" );
	m_pVoteButton->AddActionSignalTarget( this );
	m_pVoteButton->SetCommand( "VoteButton" );
	m_pPromotionButton = new CNB_Button( this, "PromotionButton", "", this, "PromotionButton" );

	m_pHeaderFooter->SetTitle( "#nb_mission_prep" );

	m_pLobbyRow1->m_nLobbySlot = 1;
	m_pLobbyRow2->m_nLobbySlot = 2;
	m_pLobbyRow3->m_nLobbySlot = 3;
	m_bLocalLeader = false;
}

CNB_Main_Panel::~CNB_Main_Panel()
{

}

void CNB_Main_Panel::ProcessSkillSpendQueue()
{
	if ( s_QueuedSpendSkillPoints.Count() > 0 )
	{
		SpendSkillPointsOnMarine( s_QueuedSpendSkillPoints[0] );
	}
}

void CNB_Main_Panel::OnFinishedSpendingSkillPoints()
{
	m_hSubScreen = NULL;
	ProcessSkillSpendQueue();
}

void CNB_Main_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_main_panel.res" );

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	color32 grey;
	grey.r = 190;
	grey.g = 190;
	grey.b = 190;
	grey.a = 255;

	char imagename[255];
	Q_snprintf( imagename, sizeof(imagename), "vgui/briefing/chat_icon" );
	m_pChatButton->SetImage( CBitmapButton::BUTTON_ENABLED, CHAT_BUTTON_ICON, grey );
	m_pChatButton->SetImage( CBitmapButton::BUTTON_DISABLED, CHAT_BUTTON_ICON, grey );
	m_pChatButton->SetImage( CBitmapButton::BUTTON_PRESSED, CHAT_BUTTON_ICON, white );		
	m_pChatButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, CHAT_BUTTON_ICON, white );
	m_pVoteButton->SetImage( CBitmapButton::BUTTON_ENABLED, VOTE_BUTTON_ICON, grey );
	m_pVoteButton->SetImage( CBitmapButton::BUTTON_DISABLED, VOTE_BUTTON_ICON, grey );
	m_pVoteButton->SetImage( CBitmapButton::BUTTON_PRESSED, VOTE_BUTTON_ICON, white );		
	m_pVoteButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, VOTE_BUTTON_ICON, white );
}

void CNB_Main_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Main_Panel::OnThink()
{
	BaseClass::OnThink();

	if ( !Briefing() )
		return;

	m_pFriendsButton->SetVisible( ! ( ASWGameResource() && ASWGameResource()->IsOfflineGame() ) );	
	m_pChatButton->SetVisible( gpGlobals->maxClients > 1 );
	m_pVoteButton->SetVisible( gpGlobals->maxClients > 1 );
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	m_pPromotionButton->SetVisible( pPlayer && pPlayer->GetExperience() >= ( ASW_XP_CAP * g_flPromotionXPScale[ pPlayer->GetPromotion() ] ) && pPlayer->GetPromotion() < ASW_PROMOTION_CAP );

	
	
	const char *pszLeaderName = Briefing()->GetLeaderName();
	if ( pszLeaderName )
	{
		m_pLeaderLabel->SetVisible( ! ( ASWGameResource() && ASWGameResource()->IsOfflineGame() ) );

		wchar_t wszPlayerName[32];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszLeaderName, wszPlayerName, sizeof(wszPlayerName));

		wchar_t wszBuffer[128];
		g_pVGuiLocalize->ConstructString( wszBuffer, sizeof(wszBuffer), g_pVGuiLocalize->Find( "#nb_leader" ), 1, wszPlayerName );

		m_pLeaderLabel->SetText( wszBuffer );
	}
	else
	{
		m_pLeaderLabel->SetVisible( false );
	}

	if ( !m_hSubScreen.Get() )
	{
		m_pLobbyRow0->CheckTooltip( m_pLobbyTooltip );
		m_pLobbyRow1->CheckTooltip( m_pLobbyTooltip );
		m_pLobbyRow2->CheckTooltip( m_pLobbyTooltip );
		m_pLobbyRow3->CheckTooltip( m_pLobbyTooltip );

		ProcessSkillSpendQueue();
	}

	bool bLocalLeader = Briefing()->IsLocalPlayerLeader();
	if ( bLocalLeader != m_bLocalLeader )
	{
		if ( bLocalLeader )
		{
			m_pReadyButton->SetText( "#nb_start_mission" );
			m_pReadyCheckImage->SetVisible( false );
		}
		else
		{
			m_pReadyButton->SetText( "#nb_ready" );
			m_pReadyCheckImage->SetVisible( true );
		}
		m_bLocalLeader = bLocalLeader;
	}

	if ( !m_bLocalLeader )
	{
		if ( Briefing()->GetCommanderReady( 0 ) )
		{
			m_pReadyCheckImage->SetImage( "swarm/HUD/TickBoxTicked" );
		}
		else
		{
			m_pReadyCheckImage->SetImage( "swarm/HUD/TickBoxEmpty" );
		}
	}
}

void CNB_Main_Panel::ChangeMarine( int nLobbySlot )
{
	if ( !Briefing()->IsLobbySlotLocal( nLobbySlot ) )
		return;

	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CNB_Select_Marine_Panel *pMarinePanel = new CNB_Select_Marine_Panel( this, "Select_Marine_Panel" );
	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfile( nLobbySlot );
	pMarinePanel->m_nInitialProfileIndex = pProfile ? pProfile->m_ProfileIndex : -1;

	if ( Briefing()->IsOfflineGame() )
	{
		pMarinePanel->m_nPreferredLobbySlot = nLobbySlot;
	}
	pMarinePanel->InitMarineList();
	pMarinePanel->MoveToFront();
	Briefing()->SetChangingWeaponSlot( 1 );

	m_hSubScreen = pMarinePanel;

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1, "ASWComputer.MenuButton" );
}

void CNB_Main_Panel::ChangeWeapon( int nLobbySlot, int nInventorySlot )
{
	if ( !Briefing()->IsLobbySlotLocal( nLobbySlot ) )
		return;

	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfile( nLobbySlot );
	if ( !pProfile )
		return;

	int nProfileIndex = pProfile->m_ProfileIndex;
	if ( nProfileIndex == -1 )
		return;
	
	//CNB_Select_Mission_Panel *pWeaponPanel = new CNB_Select_Mission_Panel( this, "Select_Mission_Panel" );
	CNB_Select_Weapon_Panel *pWeaponPanel = new CNB_Select_Weapon_Panel( this, "Select_Weapon_Panel" );	
	pWeaponPanel->SelectWeapon( nProfileIndex, nInventorySlot );
	pWeaponPanel->InitWeaponList();
	pWeaponPanel->MoveToFront();

	Briefing()->SetChangingWeaponSlot( 2 + nInventorySlot );

	m_hSubScreen = pWeaponPanel;
}

void CNB_Main_Panel::SpendSkillPointsOnMarine( int nProfileIndex )
{
	if ( m_hSubScreen.Get() )
	{
		if ( dynamic_cast<CNB_Spend_Skill_Points*>( m_hSubScreen.Get() ) != NULL )	// already spending skill points on a marine
			return;

		m_hSubScreen->MarkForDeletion();
	}

	// remove from queue
	RemoveFromSpendQueue( nProfileIndex );

	CNB_Spend_Skill_Points *pPanel = new CNB_Spend_Skill_Points( this, "Spend_Skill_Points" );
	pPanel->m_nProfileIndex = nProfileIndex;
	pPanel->Init();
	pPanel->MoveToFront();

	Briefing()->SetChangingWeaponSlot( 1 );

	m_hSubScreen = pPanel;
}

void CNB_Main_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "ReadyButton" ) )
	{
		if ( m_bLocalLeader )
		{
			if ( Briefing()->CheckMissionRequirements() )
			{
				if ( Briefing()->AreOtherPlayersReady() )
				{
					Briefing()->StartMission();
				}
				else
				{
					// force other players to be ready?
					engine->ClientCmd("cl_wants_start"); // notify other players that we're waiting on them
					new ForceReadyPanel( GetParent(), "ForceReady", "#asw_force_startm", ASW_FR_BRIEFING );		// TODO: this breaks the IBriefing abstraction, fix it if we need that
				}
			}
		}
		else
		{
			Briefing()->ToggleLocalPlayerReady();
		}
	}
	else if ( !Q_stricmp( command, "OptionsButton" ) )
	{
		ShowMissionOptions();
	}
	else if ( !Q_stricmp( command, "FriendsButton" ) )
	{
#ifndef _X360 
		if ( BaseModUI::CUIGameData::Get() )
		{
			BaseModUI::CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
#endif
	}
	else if ( !Q_stricmp( command, "MissionDetailsButton" ) )
	{
		ShowMissionDetails();
	}
	else if ( !Q_stricmp( command, "ChatButton" ) )
	{
		if ( GetClientModeASW() )
		{
			GetClientModeASW()->ToggleMessageMode();
		}
	}
	else if ( !Q_stricmp( command, "VoteButton" ) )
	{
		engine->ClientCmd( "playerlist" );
	}
	else if ( !Q_stricmp( command, "PromotionButton" ) )
	{
		ShowPromotionPanel();
	}
	BaseClass::OnCommand( command );
}



void CNB_Main_Panel::ShowMissionDetails()
{
	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CNB_Mission_Panel *pPanel = new CNB_Mission_Panel( this, "MissionPanel" );
	pPanel->MoveToFront();

	m_hSubScreen = pPanel;
}

void CNB_Main_Panel::ShowMissionOptions()
{
	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CNB_Mission_Options *pPanel = new CNB_Mission_Options( this, "MissionOptions" );
	pPanel->MoveToFront();

	m_hSubScreen = pPanel;
}

void CNB_Main_Panel::ShowPromotionPanel()
{
	if ( m_hSubScreen.Get() )
	{
		m_hSubScreen->MarkForDeletion();
	}

	CNB_Promotion_Panel *pPanel = new CNB_Promotion_Panel( this, "PromotionPanel" );
	pPanel->MoveToFront();

	m_hSubScreen = pPanel;
}