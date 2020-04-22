//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_gc_client.h"
#include "tf_party.h"

#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/SectionedListPanel.h"
#include "vgui_bitmapimage.h"
#include "vgui_avatarimage.h"
#include "store/store_panel.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include <vgui_controls/ImageList.h>

#include "tf_lobbypanel_mvm.h"
#include "tf_lobby_container_frame_mvm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern ConVar tf_matchmaking_join_in_progress;
ConVar tf_matchmaking_ticket_help( "tf_matchmaking_ticket_help", "0", FCVAR_CLIENTDLL | FCVAR_DONTRECORD | FCVAR_ARCHIVE | FCVAR_HIDDEN, "Saved if the player has see the ticket help screen." );

const int k_iPopIndex_Any = -1000;
const int k_iPopIndex_OnlyNotYetCompleted = -1001;
const int k_iPopIndex_AnyNormal = -1002;
const int k_iPopIndex_AnyIntermediate = -1003;
const int k_iPopIndex_AnyAdvanced = -1004;
const int k_iPopIndex_AnyExpert = -1005;
const int k_iPopIndex_AnyHaunted = -1006;

static void GetMvmChallengeSet( int idxChallenge, CMvMMissionSet &result )
{
	result.Clear();

	if ( idxChallenge >= 0 )
	{
		result.SetMissionBySchemaIndex( idxChallenge, true );
		return;
	}

	bool bMannUP = GTFGCClientSystem()->GetSearchPlayForBraggingRights();
#ifdef USE_MVM_TOUR
	int idxTour = GTFGCClientSystem()->GetSearchMannUpTourIndex();
	Assert( bMannUP || idxTour < 0 );
#endif // USE_MVM_TOUR

#ifdef USE_MVM_TOUR
	uint32 nNotCompletedChallenges = ~0U;
	CTFParty *pParty = GTFGCClientSystem()->GetParty();
	if ( pParty )
	{
		for ( int i = 0 ; i < pParty->GetNumMembers() ; ++i )
		{
			nNotCompletedChallenges &= ~pParty->Obj().members( i ).completed_missions();
		}
	}
	else
	{
		if ( idxTour >= 0 )
		{
			uint32 nTours = 0, nCompletedChallenge = 0;
			GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxTour, &nTours, &nCompletedChallenge );

			nNotCompletedChallenges = ~nCompletedChallenge;
		}
	}
#endif // USE_MVM_TOUR

	for ( int i = 0 ; i < GetItemSchema()->GetMvmMissions().Count() ; ++i )
	{
		const MvMMission_t &chal = GetItemSchema()->GetMvmMissions()[ i ];

		// Cannot select non-MannUp missions in mann up mode
#ifdef USE_MVM_TOUR
		int iBadgeSlot = (idxTour < 0) ? -1 : GetItemSchema()->GetMvmMissionBadgeSlotForTour( idxTour, i );
		if ( bMannUP && iBadgeSlot < 0 )
			continue;
#else // new mm
		bool bIsChallengeInMannUp = chal.m_unMannUpPoints > 0;
		if ( bMannUP && !bIsChallengeInMannUp )
			continue;
#endif // USE_MVM_TOUR

		// Does this challenge fit the search criteria?
		bool bSelect = false;
		switch ( idxChallenge )
		{
			case k_iPopIndex_Any:
					bSelect = true;
				break;
			case k_iPopIndex_OnlyNotYetCompleted:
#ifdef USE_MVM_TOUR
				if ( iBadgeSlot >= 0 )
				{
					int iChallengeBit = ( 1 << iBadgeSlot );
					if ( nNotCompletedChallenges & iChallengeBit )
					{
						bSelect = true;
					}
				}
#endif // USE_MVM_TOUR
				break;

			case k_iPopIndex_AnyNormal:
				bSelect = ( chal.m_eDifficulty == k_EMvMChallengeDifficulty_Normal );
				break;

			case k_iPopIndex_AnyIntermediate:
				bSelect = ( chal.m_eDifficulty == k_EMvMChallengeDifficulty_Intermediate );
				break;

			case k_iPopIndex_AnyAdvanced:
				bSelect = ( chal.m_eDifficulty == k_EMvMChallengeDifficulty_Advanced );
				break;

			case k_iPopIndex_AnyExpert:
				bSelect = ( chal.m_eDifficulty == k_EMvMChallengeDifficulty_Expert );
				break;

			case k_iPopIndex_AnyHaunted:
				bSelect = ( chal.m_eDifficulty == k_EMvMChallengeDifficulty_Haunted );
				break;

			default:
				Assert( false );
		}
		result.SetMissionBySchemaIndex( i, bSelect );
	}
}

extern Color s_colorBannedPlayerListItem;
extern Color s_colorPlayerListItem;
extern Color s_colorChatRemovedFromQueue;
extern Color s_colorChatAddedToQueue;
extern Color s_colorChatPlayerJoinedParty;
extern Color s_colorChatPlayerJoinedPartyName;
extern Color s_colorChatPlayerLeftParty;
extern Color s_colorChatPlayerLeftPartyName;
extern Color s_colorChatPlayerChatName;
extern Color s_colorChatPlayerChatText;
extern Color s_colorChatDefault;
extern Color s_colorChallengeForegroundEnabled;
extern Color s_colorChallengeForegroundHaunted;
extern Color s_colorChallengeForegroundDisabled;
extern Color s_colorChallengeHeader;


static void GetPlayerNameForSteamID( wchar_t *wCharPlayerName, int nBufSizeBytes, const CSteamID &steamID )
{
	const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
	V_UTF8ToUnicode( pszName, wCharPlayerName, nBufSizeBytes );
}

CLobbyPanel_MvM::CLobbyPanel_MvM( vgui::Panel *pParent, CBaseLobbyContainerFrame* pLobbyContainer ) 
	: CBaseLobbyPanel( pParent, pLobbyContainer )
{
	m_pMvMMannVsMachineGroupPanel = NULL;
	m_pMvMMannUpGroupPanel = NULL;
	m_pMvMPracticeGroupPanel = NULL;
	m_pMvMTourOfDutyGroupPanel = NULL;
	m_pMvMTourOfDutyListGroupBox = NULL;
	m_pTourList = NULL;

	m_MvMEconItemsGroupBox = NULL;
	m_pSquadSurplusCheckButton = NULL;
	m_pOpenStoreButton = NULL;
	m_pOpenStoreButton2 = NULL;
	m_pOpenHelpButton = NULL;
	m_pMannUpNowButton = NULL;
	m_pMannUpTourLootDescriptionBox = NULL;
	m_pMannUpTourLootImage = NULL;
	//m_pMannUpTourLootDetailLabel = NULL;
	m_pTourDifficultyWarning = NULL;
	
	m_MvMPracticeGroupPanel = NULL;

	m_pMvMSelectChallengeGroupPanel = NULL;
	m_pMVMChallengeListGroupBox = NULL;

	// MvM
	m_pMvMMannVsMachineGroupPanel = new vgui::EditablePanel( this, "MannVsMachineGroupBox" );
	m_pMvMMannUpGroupPanel = new vgui::EditablePanel( this, "MannUpGroupBox" );
	m_pMvMPracticeGroupPanel = new vgui::EditablePanel( this, "PracticeGroupBox" );
	m_pMvMTourOfDutyGroupPanel = new vgui::EditablePanel( this, "MvMTourOfDutyGroupBox" );

	m_MvMEconItemsGroupBox = new vgui::EditablePanel( this, "MvMEconItemsGroupBox" );
	m_pMannUpTicketImage = new vgui::ImagePanel( m_MvMEconItemsGroupBox, "MannUpTicketImage" );
	m_pSquadSurplusImage = new vgui::ImagePanel( m_MvMEconItemsGroupBox, "SquadSurplusImage" );

	m_pMannUpTourLootDescriptionBox = new vgui::EditablePanel( this, "MannUpTourLootDescriptionBox" );
	m_pMannUpTourLootImage = new vgui::ImagePanel( m_pMannUpTourLootDescriptionBox, "TourLootImage" );
	//m_pMannUpTourLootDetailLabel = new vgui::Label( m_pMannUpTourLootDescriptionBox, "TourLootDetailLabel", " );

	m_MvMPracticeGroupPanel = new vgui::EditablePanel( this, "MvMPracticeGroupBox" );

	m_pMvMSelectChallengeGroupPanel = new vgui::EditablePanel( this, "MvMSelectChallengeGroupBox" );
	m_pMVMChallengeListGroupBox = new vgui::EditablePanel( m_pMvMSelectChallengeGroupPanel, "ChallengeListGroupBox" );
	m_pChallengeList = new ChallengeList( this, m_pMVMChallengeListGroupBox, "ChallengeList" );

	m_pMvMTourOfDutyListGroupBox = new vgui::EditablePanel( m_pMvMTourOfDutyGroupPanel, "TourlistGroupBox" );
	m_pTourList = new vgui::SectionedListPanel( m_pMvMTourOfDutyListGroupBox, "TourList" );

	m_pTourDifficultyWarning = new vgui::Label( m_pMvMTourOfDutyGroupPanel, "TourDifficultyWarning", "" );

	m_fontChallengeListHeader = 0;
	m_fontChallengeListItem = 0;
}

CLobbyPanel_MvM::~CLobbyPanel_MvM()
{}

//-----------------------------------------------------------------------------
void CLobbyPanel_MvM::FireGameEvent( IGameEvent *event )
{
	BaseClass::FireGameEvent( event );

	const char *pszEventName = event->GetName();

	if ( !Q_stricmp( pszEventName, "mm_lobby_member_join" ) )
	{
#ifdef USE_MVM_TOUR
		WriteTourList();
#endif // USE_MVM_TOUR
		WriteChallengeList();

		return;
	}
	else if ( !Q_stricmp( pszEventName, "mm_lobby_member_leave" ) )
	{
#ifdef USE_MVM_TOUR
		WriteTourList();
#endif // USE_MVM_TOUR
		WriteChallengeList();

		return;
	}
}

void CLobbyPanel_MvM::OnCommand( const char *command )
{
	if ( FStrEq( command, "open_store_ticket" ) )
	{
		// Open the store, and show the upgrade advice
		EconUI()->CloseEconUI();

		CSchemaItemDefHandle hItemDef( CTFItemSchema::k_rchMvMTicketItemDefName );
		EconUI()->GetStorePanel()->AddToCartAndCheckoutImmediately( hItemDef->GetDefinitionIndex() );
		return;
	}
	else if ( FStrEq( command, "open_store_voucher" ) )
	{
		// Open the store, and show the upgrade advice
		EconUI()->CloseEconUI();

		CSchemaItemDefHandle hItemDef( CTFItemSchema::k_rchMvMSquadSurplusVoucherItemDefName );
		EconUI()->GetStorePanel()->AddToCartAndCheckoutImmediately( hItemDef->GetDefinitionIndex() );
		return;
	}
	else if ( FStrEq( command, "open_help" ) )
	{
		CExplanationPopup *pPopup = dynamic_cast< CExplanationPopup* >( GetParent()->FindChildByName("StartExplanation") );
		if ( pPopup )
		{
			pPopup->Popup();
		}
		return;
	}
	else if ( FStrEq( command, "mann_up_now" ) )
	{
		GTFGCClientSystem()->SetSearchChallenges( CMvMMissionSet() );
		m_pContainer->OnCommand( "back" );
		m_pContainer->OnCommand( "mannup" );
		return;
	}

	BaseClass::OnCommand( command );
}

void CLobbyPanel_MvM::SetMannUpTicketCount( int nCount )
{
	m_pMannUpTicketImage->SetImage( nCount > 0 ? "pve/mvm_ticket_active" : "pve/mvm_ticket_inactive" );

	char szCount[ 5 ];
	V_snprintf( szCount, sizeof( szCount ), "%i", nCount );

	m_MvMEconItemsGroupBox->SetDialogVariable( "ticket_count", szCount );
}

void CLobbyPanel_MvM::SetSquadSurplusCount( int nCount )
{
	char szCount[ 5 ];
	V_snprintf( szCount, sizeof( szCount ), "%i", nCount );

	m_MvMEconItemsGroupBox->SetDialogVariable( "voucher_count", szCount );
}

EMatchGroup CLobbyPanel_MvM::GetMatchGroup( void ) const
{
	return GTFGCClientSystem()->GetSearchPlayForBraggingRights() ? k_nMatchGroup_MvM_MannUp : k_nMatchGroup_MvM_Practice;
}

void CLobbyPanel_MvM::OnCheckButtonChecked( vgui::Panel *panel )
{
	if ( m_iWritingPanel > 0 )
		return;

	if ( panel == m_pSquadSurplusCheckButton )
	{
		if ( BIsPartyInUIState() && GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
		{
			if ( m_pSquadSurplusCheckButton->IsSelected() )
			{
				if ( GTFGCClientSystem()->BLocalPlayerInventoryHasSquadSurplusVoucher() )
				{
					GTFGCClientSystem()->SetLocalPlayerSquadSurplus( true );
					m_pSquadSurplusImage->SetImage( "pve/mvm_voucher_active" );
				}
				else
				{
					m_pSquadSurplusCheckButton->SetSilentMode( true );
					m_pSquadSurplusCheckButton->SetSelected( false );
					m_pSquadSurplusCheckButton->SetSilentMode( false );
					ShowEconRequirementDialog( "#TF_MvM_RequiresSquadSurplusVoucher_Title", "#TF_MvM_RequiresSquadSurplusVoucher", CTFItemSchema::k_rchMvMSquadSurplusVoucherItemDefName );
				}
			}
			else
			{
				GTFGCClientSystem()->SetLocalPlayerSquadSurplus( false );

				m_pSquadSurplusImage->SetImage( "pve/mvm_voucher_inactive" );
			}
		}
		else
		{
			WriteGameSettingsControls();
		}

		return;
	}

	BaseClass::OnCheckButtonChecked( panel );
}


void CLobbyPanel_MvM::OnItemLeftClick( vgui::Panel* panel )
{
	if ( m_iWritingPanel > 0 )
		return;

#ifdef USE_MVM_TOUR
	if ( panel == m_pTourList )
	{
		OnClickedOnTour();
	}
#endif // USE_MVM_TOUR
	else if ( panel == m_pChallengeList )
	{
		OnClickedOnChallenge();
	}
	
	BaseClass::OnItemLeftClick( panel );
}

void CLobbyPanel_MvM::ApplyChatUserSettings( const LobbyPlayerInfo& player, KeyValues* pSettings ) const
{
	if ( GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
	{
		pSettings->SetInt( "has_ticket", player.m_bHasTicket ? m_iImageHasTicket : m_iImageNoTicket );
		pSettings->SetInt( "squad_surplus", player.m_bSquadSurplus ? m_iImageSquadSurplus : m_iImageNoSquadSurplus );
	}
}

#ifdef USE_MVM_TOUR
void CLobbyPanel_MvM::OnClickedOnTour()
{
	int iSelected = m_pTourList->GetSelectedItem();
	m_pTourList->SetSelectedItem( -1 );
	if ( iSelected < 0 )
		return;
	if ( BIsPartyLeader() && BIsPartyInUIState() )
	{
		int iTourIndex = m_pTourList->GetItemData( iSelected )->GetInt( "tour_index", -1 );
		Assert( iTourIndex >= 0 );
		GTFGCClientSystem()->SetSearchMannUpTourIndex( iTourIndex );
	}
	else
	{
		WriteChallengeList();
	}
}
#endif // USE_MVM_TOUR

void CLobbyPanel_MvM::WriteGameSettingsControls()
{
	BaseClass::WriteGameSettingsControls();

	// Make sure we want to be in matchmaking.  (If we don't, the frame should hide us pretty quickly.)
	// We might get an event or something right at the transition point occasionally when the UI should
	// not be visible
	if ( GTFGCClientSystem()->GetMatchmakingUIState() == eMatchmakingUIState_Inactive )
	{
		return;
	}

	++m_iWritingPanel;

	bool bLeader = BIsPartyLeader();
	bool bInUIState = BIsPartyInUIState();

	TF_Matchmaking_WizardStep eWizardStep = GTFGCClientSystem()->GetWizardStep();

	// MVM
	m_pMvMMannVsMachineGroupPanel->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
	m_pMvMMannUpGroupPanel->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
	m_pMvMPracticeGroupPanel->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
	m_pMannUpTourLootDescriptionBox->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY );
	m_pMvMTourOfDutyGroupPanel->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY );
	m_pMvMSelectChallengeGroupPanel->SetVisible( eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE );

	bool bShowBottomPanel = ( eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE );

	if ( !tf_matchmaking_ticket_help.GetBool() && bShowBottomPanel && GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
	{
		OnCommand( "open_help" );
		tf_matchmaking_ticket_help.SetValue( 1 );
	}

	m_MvMEconItemsGroupBox->SetVisible( bShowBottomPanel && GTFGCClientSystem()->GetSearchPlayForBraggingRights() );
	m_MvMPracticeGroupPanel->SetVisible( bShowBottomPanel && !GTFGCClientSystem()->GetSearchPlayForBraggingRights() );

	if ( m_pMvMTourOfDutyGroupPanel->IsVisible() )
	{
		SetNavToRelay( m_pMvMTourOfDutyGroupPanel->GetName() );
	}
	else if ( m_pMvMSelectChallengeGroupPanel->IsVisible() )
	if ( m_pMvMSelectChallengeGroupPanel->IsVisible() )
	{
		SetNavToRelay( m_pMvMSelectChallengeGroupPanel->GetName() );
	}

	if ( m_MvMPracticeGroupPanel->IsVisible() )
	{
		SetNavToRelay( m_MvMPracticeGroupPanel->GetName() );
	}
	else if ( m_MvMEconItemsGroupBox->IsVisible() )
	{
		SetNavToRelay( m_MvMEconItemsGroupBox->GetName() );
	}

	m_pContainer->SetNextButtonEnabled( true );


#ifdef USE_MVM_TOUR
	WriteTourList();
#endif // USE_MVM_TOUR
	WriteChallengeList();

	FOR_EACH_VEC( m_vecSearchCriteriaLabels, i )
	{
		m_vecSearchCriteriaLabels[i]->SetEnabled( bInUIState );
	}

	m_pMVMChallengeListGroupBox->SetControlVisible( "GreyOutPanel", !( bLeader && bInUIState ) );
#ifdef USE_MVM_TOUR
	m_pMvMTourOfDutyListGroupBox->SetControlVisible( "GreyOutPanel", !( bLeader && bInUIState ) );
#endif // USE_MVM_TOUR
	bool bPlayForBraggingRights = GTFGCClientSystem()->GetSearchPlayForBraggingRights();
	m_pSquadSurplusCheckButton->SetEnabled( bInUIState && bPlayForBraggingRights );
	m_pSquadSurplusCheckButton->SetSilentMode( true );
	m_pSquadSurplusCheckButton->SetSelected( GTFGCClientSystem()->GetLocalPlayerSquadSurplus() );
	m_pSquadSurplusCheckButton->SetSilentMode( false );

	m_pSquadSurplusImage->SetImage( GTFGCClientSystem()->GetLocalPlayerSquadSurplus() ? "pve/mvm_voucher_active" : "pve/mvm_voucher_inactive" );

	--m_iWritingPanel;
}

bool CLobbyPanel_MvM::ShouldShowLateJoin() const
{
	TF_Matchmaking_WizardStep eWizardStep = GTFGCClientSystem()->GetWizardStep();
	return 
#ifdef USE_MVM_TOUR
		( eWizardStep == TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY ) ||
#endif // USE_MVM_TOUR
		( eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE ) ||
		( ( eWizardStep == TF_Matchmaking_WizardStep_SEARCHING ) && ( GTFGCClientSystem()->GetSearchMode() == TF_Matchmaking_MVM ) );
}

#ifdef USE_MVM_TOUR
void CLobbyPanel_MvM::WriteTourList()
{
	if ( !GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
		return;

	++m_iWritingPanel;

	bool bLeader = BIsPartyLeader();
	bool bInUIState = BIsPartyInUIState();
	int idxSelectedTour = GTFGCClientSystem()->GetSearchMannUpTourIndex();

	m_pTourList->RemoveAll();
	m_pTourList->RemoveAllSections();

	m_pTourList->SetClickable( bLeader && bInUIState );
	m_pTourList->AddSection( 0, "Tour name" );
	m_pTourList->SetSectionAlwaysVisible( 0, false );
	m_pTourList->SetSectionFgColor( 0, s_colorChallengeHeader );
	m_pTourList->SetSectionDividerColor( 0, Color(0,0,0,0) );
	m_pTourList->AddColumnToSection( 0, "new","", vgui::SectionedListPanel::COLUMN_IMAGE, m_iNewWidth );
	m_pTourList->AddColumnToSection( 0, "check_box", "", vgui::SectionedListPanel::COLUMN_IMAGE, m_iChallengeCheckBoxWidth );
	m_pTourList->AddColumnToSection( 0, "spacer", "", 0, m_iChallengeSpacer );
 	m_pTourList->AddColumnToSection( 0, "display_name", "Tour", 0, m_iTourNameWidth );
 	m_pTourList->AddColumnToSection( 0, "skill", "Difficulty", 0, m_iTourSkillWidth );
 	m_pTourList->AddColumnToSection( 0, "progress", "Progress", 0, m_iTourProgressWidth );
 	m_pTourList->AddColumnToSection( 0, "badge_level", "Tours Completed", 0, m_iTourNumberWidth );
	m_pTourList->SetFontSection( 0, m_fontChallengeListHeader );

	bool bCompletedOneAdvancedTour = false;
	uint32 unBadgeLevel = 0, unCompletedChallengeMask = 0;
	const char *pszWarningString = "#TF_MVM_Tour_ExpertDifficulty_Warning";
	
	// Local player has completed at least one Advanced tour?
	FOR_EACH_VEC( GetItemSchema()->GetMvmTours(), idxTour )
	{
		GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxTour, &unBadgeLevel, &unCompletedChallengeMask );
		
		const MvMTour_t &tourInfo = GetItemSchema()->GetMvmTours()[idxTour];
		if ( tourInfo.m_eDifficulty >= k_EMvMChallengeDifficulty_Advanced && unBadgeLevel > 0 )
		{
			bCompletedOneAdvancedTour = true;
			break;
		}
	}

	// Add a row for each tour
	FOR_EACH_VEC( GetItemSchema()->GetMvmTours(), idxTour )
	{
		const MvMTour_t &tour = GetItemSchema()->GetMvmTours()[ idxTour ];

		KeyValues *kvItem = new KeyValues("item");

		GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxTour, &unBadgeLevel, &unCompletedChallengeMask );

		int nCompletedChallengeCount = 0;
		for ( int i = 0 ; i < tour.m_vecMissions.Count() ; ++i )
		{
			if ( unCompletedChallengeMask & ( 1 << tour.m_vecMissions[i].m_iBadgeSlot ) )
			{
				++nCompletedChallengeCount;
			}
		}

		char cchTemp[256];
		V_sprintf_safe( cchTemp, "%d / %d", nCompletedChallengeCount, tour.m_vecMissions.Count() );
		kvItem->SetString( "progress", cchTemp );

		uint32 iTourNumber = Max( 1U, unBadgeLevel );
		V_sprintf_safe( cchTemp, "%d", iTourNumber );
		kvItem->SetString( "badge_level", cchTemp );

		if ( tour.m_bIsNew )
		{
			kvItem->SetInt( "new", m_iImageNew );
		}

		kvItem->SetInt( "check_box", idxSelectedTour == idxTour ? m_iImageRadioButtonYes : m_iImageRadioButtonNo );
		kvItem->SetString( "display_name", tour.m_sTourNameLocalizationToken.Get() );
		kvItem->SetString( "skill", GetMvMChallengeDifficultyLocName( tour.m_eDifficulty ) );
		kvItem->SetInt( "tour_index", idxTour );
		int itemID = m_pTourList->AddItem( 0, kvItem );
		m_pTourList->SetItemFont( itemID, m_fontChallengeListItem );
		
		if ( tour.m_eDifficulty >= k_EMvMChallengeDifficulty_Expert && !bCompletedOneAdvancedTour )
		{
			m_pTourList->SetItemFgColor( itemID, s_colorBannedPlayerListItem );
			pszWarningString = "#TF_MVM_Tour_ExpertDifficulty_Denied";
		}
		else
		{
			m_pTourList->SetItemFgColor( itemID, s_colorChallengeForegroundEnabled );
		}
	}
	m_pTourList->SetSelectedItem( idxSelectedTour );

	const char *pszSelectedTourLocToken = "TF_MvM_Tour_NoSelection";
	const char *pszLootImage = "pve/mvm_loot_image";
	bool bShowDifficultyWarning = false;
	if ( idxSelectedTour >= 0 )
	{
		const MvMTour_t &tour = GetItemSchema()->GetMvmTours()[ idxSelectedTour ];
		pszLootImage = tour.m_sLootImageName.Get();
		pszSelectedTourLocToken = tour.m_sTourNameLocalizationToken.Get();

		// Check if we should show the difficulty warning
		if ( tour.m_eDifficulty >= k_EMvMChallengeDifficulty_Expert )
		{
			// Deny expert mode if they haven't completed at least one Advanced tour
			if (  !bCompletedOneAdvancedTour )
			{
				m_pContainer->SetNextButtonEnabled( false );
			}

			// Local player hasn't completed one mission?
			if ( !GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxSelectedTour, &unBadgeLevel, &unCompletedChallengeMask )
				|| unBadgeLevel == 0 )
			{
				bShowDifficultyWarning = true;
			}

			// Anybody in the party hasn't completed a mission?
			CTFParty *pParty = GTFGCClientSystem()->GetParty();
			if ( pParty != NULL )
			{
				for ( int i = 0 ; !bShowDifficultyWarning && i < pParty->GetNumMembers() ; ++i )
				{
					if ( pParty->Obj().members( i ).badge_level() == 0 )
					{
						bShowDifficultyWarning = true;
					}
				}
			}
		}
	}

	char archTemp[ 256 ];
	V_sprintf_safe( archTemp, "%s_LootDescription", pszSelectedTourLocToken );
	m_pMannUpTourLootDescriptionBox->SetDialogVariable( "tour_loot_detail", g_pVGuiLocalize->Find( archTemp ) );

	m_pMannUpTourLootImage->SetImage( pszLootImage );

	wchar_t wszLocalized[512];
	g_pVGuiLocalize->ConstructString_safe( wszLocalized, g_pVGuiLocalize->Find( pszWarningString ), 0 );
	m_pTourDifficultyWarning->SetText( wszLocalized );
	m_pTourDifficultyWarning->SetVisible( bShowDifficultyWarning );

	--m_iWritingPanel;
}
#endif // USE_MVM_TOUR

void CLobbyPanel_MvM::WriteChallengeList()
{
	++m_iWritingPanel;

	bool bLeader = BIsPartyLeader();
	bool bInUIState = BIsPartyInUIState();
	bool bForBraggingRights = GTFGCClientSystem()->GetSearchPlayForBraggingRights();
	
#ifdef USE_MVM_TOUR
	int idxTour = GTFGCClientSystem()->GetSearchMannUpTourIndex();

	char szTours[ 8 ] = "";
	if ( idxTour >= 0 )
	{
		uint32 nTours, nCompletedChallenge;
		GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxTour, &nTours, &nCompletedChallenge );
		if ( nTours < 1 ) // if we don't have a badge, show "1"
			nTours = 1;
		V_snprintf( szTours, sizeof( szTours ), "%u", nTours );
	}

	m_pChallengeList->SetClickable( bLeader && bInUIState );
	if ( idxTour < 0 )
	{
		m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "tour_name", g_pVGuiLocalize->Find( "#TF_MvM_Missions" ) );
	}
	else
	{
		m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "tour_name", g_pVGuiLocalize->Find( GetItemSchema()->GetMvmTours()[ idxTour ].m_sTourNameLocalizationToken.Get() ) );
	}
	m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "complete_heading", g_pVGuiLocalize->Find( bForBraggingRights ? "#TF_MvM_Complete" : "#TF_MvM_Difficulty" ) );
	m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "tour_level", szTours );
	m_pMvMSelectChallengeGroupPanel->SetControlVisible( "TourLevelImage", idxTour >= 0 );
#else // new mm
	char szTours[ 8 ] = "";

	m_pChallengeList->SetClickable( bLeader && bInUIState );
	m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "tour_name", g_pVGuiLocalize->Find( "#TF_MvM_Missions" ) );
	m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "complete_heading", g_pVGuiLocalize->Find( "#TF_MvM_Difficulty" ) );
	m_pMvMSelectChallengeGroupPanel->SetDialogVariable( "tour_level", szTours );
	m_pMvMSelectChallengeGroupPanel->SetControlVisible( "TourLevelImage", false );
#endif // USE_MVM_TOUR

	CMvMMissionSet searchChallenges;
	GTFGCClientSystem()->GetSearchChallenges( searchChallenges );

	m_pChallengeList->RemoveAll();
	m_pChallengeList->RemoveAllSections();
//	int iSelectChallengeItem = -1;

	int nCurrentSection = -1;
	KeyValues *kvItem = NULL;
	int itemID = 0;

	//
	// Top section is for special multi-select checkboxes
	//

	++nCurrentSection;

	m_pChallengeList->AddSection( nCurrentSection, "dummy_any_section" );
	m_pChallengeList->SetSectionAlwaysVisible( nCurrentSection, true );
	m_pChallengeList->SetSectionDividerColor( nCurrentSection, Color(0,0,0,0) );
	//m_pChallengeList->SetSectionFgColor( nCurrentSection, Color( 255, 255, 255, 255 ) );
	m_pChallengeList->AddColumnToSection( nCurrentSection, "check_box", "", vgui::SectionedListPanel::COLUMN_IMAGE, m_iChallengeCheckBoxWidth );
	m_pChallengeList->AddColumnToSection( nCurrentSection, "spacer", "", 0, m_iChallengeSpacer );
	m_pChallengeList->AddColumnToSection( nCurrentSection, "display_name", "", 0, m_iChallengeNameWidth );
	//m_pChallengeList->AddColumnToSection( nCurrentSection, "completed", "", 0, m_iChallengeCompletedWidth );
	//m_pChallengeList->SetFontSection( nCurrentSection, m_fontChallengeListHeader );
	//m_pChallengeList->SetSectionMinimumContentHeight( nCurrentSection, m_iMapImageHeight );

	m_pChallengeList->m_vecMapImages.Purge();

	// List of special multi-select options
	struct MissionMultiSelect_t
	{
		int m_idxChallenge;
		const char *m_pszDisplayName;
		bool m_bMannUp;
		bool m_bBootCamp;
	};
	static const MissionMultiSelect_t arMultiSelect[] =
	{
#ifdef USE_MVM_TOUR
		{ k_iPopIndex_Any, "#TF_MvM_AnyChallenge", true, false },
//		{ k_iPopIndex_AnyHaunted, "#TF_MvM_AnyHauntedChallenge", false, true },
		{ k_iPopIndex_AnyNormal, "#TF_MvM_AnyNormalChallenge", false, true },
		{ k_iPopIndex_AnyIntermediate, "#TF_MvM_AnyIntermediateChallenge", false, true },
		{ k_iPopIndex_AnyAdvanced, "#TF_MvM_AnyAdvancedChallenge", false, true },
		{ k_iPopIndex_AnyExpert, "#TF_MvM_AnyExpertChallenge", false, true },
		{ k_iPopIndex_OnlyNotYetCompleted, "#TF_MvM_OnlyChallengeNotYetCompleted", true, false }
#else // new mm
		{ k_iPopIndex_Any, "#TF_MvM_AnyChallenge", true, true },
//		{ k_iPopIndex_AnyHaunted, "#TF_MvM_AnyHauntedChallenge", false, true },
		{ k_iPopIndex_AnyNormal, "#TF_MvM_AnyNormalChallenge", true, true },
		{ k_iPopIndex_AnyIntermediate, "#TF_MvM_AnyIntermediateChallenge", true, true },
		{ k_iPopIndex_AnyAdvanced, "#TF_MvM_AnyAdvancedChallenge", true, true },
		{ k_iPopIndex_AnyExpert, "#TF_MvM_AnyExpertChallenge", true, true },
		{ k_iPopIndex_OnlyNotYetCompleted, "#TF_MvM_OnlyChallengeNotYetCompleted", false, false }
#endif // USE_MVM_TOUR
	};

	// Scan each potential multi-select option
	for ( int i = 0 ; i < Q_ARRAYSIZE( arMultiSelect ) ; ++i )
	{
		const MissionMultiSelect_t &ms = arMultiSelect[i];

		// Check if entry is applicable for this mode
		if ( bForBraggingRights ? !ms.m_bMannUp : !ms.m_bBootCamp )
			continue;

		// Gather list of all missions that fit this mode
		CMvMMissionSet msChallenges;
		GetMvmChallengeSet( ms.m_idxChallenge, msChallenges );

		// Any missions actually met the criteria for this multi-select?
		// (e.g. we might not have any intermediate missions active right now).
		int iCheckImage;
		if ( msChallenges.IsEmpty() )
		{
			if ( ms.m_idxChallenge != k_iPopIndex_OnlyNotYetCompleted )
				continue;
			iCheckImage = m_iImageCheckBoxDisabled;
		}
		else
		{

			// Determine checkbox status.  "Only not yet completed" is special
			if ( ms.m_idxChallenge == k_iPopIndex_OnlyNotYetCompleted )
			{
				if ( searchChallenges == msChallenges )
					iCheckImage = m_iImageCheckBoxYes; // all items currently selected
				else 
					iCheckImage = m_iImageCheckBoxNo; // does not exactly match, show as a "no"
			}
			else
			{
				// Get set of checked challenges that fall under this category
				CMvMMissionSet checked( searchChallenges );
				checked.Intersect( msChallenges );

				if ( checked == msChallenges )
					iCheckImage = m_iImageCheckBoxYes; // all items currently selected
				//else if ( !checked.IsEmpty() ) // Nope, don't ever show "mixed" state
				//	iCheckImage = m_iImageCheckBoxMixed; // some items currently selected
				else
					iCheckImage = m_iImageCheckBoxNo; // no items currently selected
			}

		}

		kvItem = new KeyValues("item");
		kvItem->SetInt( "check_box", iCheckImage );
		kvItem->SetString( "display_name", ms.m_pszDisplayName );
		kvItem->SetInt( "pop_index", ms.m_idxChallenge );
		itemID = m_pChallengeList->AddItem( nCurrentSection, kvItem );
		m_pChallengeList->SetItemFont( itemID, m_fontChallengeListItem );

		Color color = s_colorChallengeForegroundEnabled;
		if ( ms.m_idxChallenge == k_iPopIndex_AnyHaunted )
			color = s_colorChallengeForegroundHaunted;
		if ( iCheckImage == m_iImageCheckBoxDisabled )
			color = s_colorChallengeForegroundDisabled;
		m_pChallengeList->SetItemFgColor( itemID, color );
	}


	//
	// Now add a section for each map
	//

	int nCurrentMap = -1;
	FOR_EACH_VEC( GetItemSchema()->GetMvmMissions(), iMissionIndex )
	{
		const MvMMission_t &mission = GetItemSchema()->GetMvmMissions()[ iMissionIndex ];

#ifdef USE_MVM_TOUR
		if ( bForBraggingRights && GetItemSchema()->FindMvmMissionInTour( idxTour, iMissionIndex) < 0 ) // !KLUDGE! This is sort of crappy, we probably should iterate the tour's mission list rather than iterating the larger list with filtering
#else // new mm
		if ( bForBraggingRights && !searchChallenges.GetMissionBySchemaIndex( iMissionIndex ) )
#endif // USE_MVM_TOUR
			continue;

		kvItem = new KeyValues("item");

		const MvMMap_t &map = GetItemSchema()->GetMvmMaps()[ mission.m_iDisplayMapIndex ];

		if ( mission.m_iDisplayMapIndex != nCurrentMap )
		{
			++nCurrentSection;
			
			m_pChallengeList->AddSection( nCurrentSection, map.m_sDisplayName.Get() );
			m_pChallengeList->SetSectionAlwaysVisible( nCurrentSection, true );
			m_pChallengeList->SetSectionFgColor( nCurrentSection, s_colorChallengeHeader );
			m_pChallengeList->SetSectionDividerColor( nCurrentSection, Color(0,0,0,0) );
#ifdef USE_MVM_TOUR
			m_pChallengeList->AddColumnToSection( nCurrentSection, "check_box", "", vgui::SectionedListPanel::COLUMN_IMAGE, m_iChallengeCheckBoxWidth );
#else // new mm
			// for mannup, don't show check box
			if ( bForBraggingRights )
			{
				m_pChallengeList->AddColumnToSection( nCurrentSection, "spacer", "", 0, m_iChallengeCheckBoxWidth );
			}
			else
			{
				m_pChallengeList->AddColumnToSection( nCurrentSection, "check_box", "", vgui::SectionedListPanel::COLUMN_IMAGE, m_iChallengeCheckBoxWidth );
			}
#endif // USE_MVM_TOUR
			m_pChallengeList->AddColumnToSection( nCurrentSection, "spacer", "", 0, m_iChallengeSpacer );
			m_pChallengeList->AddColumnToSection( nCurrentSection, "display_name", map.m_sDisplayName.Get(), 0, m_iChallengeNameWidth );
			m_pChallengeList->AddColumnToSection( nCurrentSection, "skill", "", 0, m_iChallengeSkillWidth );
			m_pChallengeList->SetFontSection( nCurrentSection, m_fontChallengeListHeader );
			m_pChallengeList->SetSectionMinimumHeight( nCurrentSection, m_iMapImageHeight );

			BitmapImage &img = m_pChallengeList->m_vecMapImages[ m_pChallengeList->m_vecMapImages.AddToTail() ];
			CFmtStr sImageName("vgui/maps/menu_thumb_%s", map.m_sMap.Get() );
			img.SetImageFile( sImageName );

			nCurrentMap = mission.m_iDisplayMapIndex;
		}

		//wchar_t wszChallengeName[ 256 ];
		//g_pVGuiLocalize->ConstructString_safe( wszChallengeName, L"%s1 (%s2)", 2, 
		//								  g_pVGuiLocalize->Find( mission.m_sDisplayName.Get() ), g_pVGuiLocalize->Find( mission.m_sMode.Get() ) );
		//kvItem->SetWString( "display_name", wszChallengeName );
		kvItem->SetString( "display_name", mission.m_sDisplayName.Get() );

		bool bSelected = searchChallenges.GetMissionBySchemaIndex( iMissionIndex );

		kvItem->SetInt( "check_box", bSelected ? m_iImageCheckBoxYes : m_iImageCheckBoxNo );
		const char *pszDifficulty = "";
#ifdef USE_MVM_TOUR
		if ( !bForBraggingRights )
#endif // USE_MVM_TOUR
		{
			pszDifficulty = GetMvMChallengeDifficultyLocName( mission.m_eDifficulty );
		}
		kvItem->SetString( "skill", pszDifficulty );
		kvItem->SetInt( "pop_index", iMissionIndex );
		itemID = m_pChallengeList->AddItem( nCurrentSection, kvItem );
		m_pChallengeList->SetItemFont( itemID, m_fontChallengeListItem );

		Color color = s_colorChallengeForegroundEnabled;
		if ( mission.m_eDifficulty == k_EMvMChallengeDifficulty_Haunted )
			color = s_colorChallengeForegroundHaunted;
		m_pChallengeList->SetItemFgColor( itemID, color );

		kvItem->deleteThis();
	}

	--m_iWritingPanel;
}


//-----------------------------------------------------------------------------
void CLobbyPanel_MvM::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pOpenStoreButton = dynamic_cast<vgui::Button *>(FindChildByName( "OpenStoreButton", true )); Assert( m_pOpenStoreButton );
	m_pOpenStoreButton2 = dynamic_cast<vgui::Button *>(FindChildByName( "OpenStoreButton2", true )); Assert( m_pOpenStoreButton2 );
	m_pOpenHelpButton = dynamic_cast<vgui::Button *>(FindChildByName( "OpenHelpButton", true )); Assert( m_pOpenHelpButton );
	m_pSquadSurplusCheckButton = dynamic_cast<vgui::CheckButton *>(FindChildByName( "SquadSurplusCheckButton", true )); Assert( m_pSquadSurplusCheckButton );
	m_pMannUpNowButton = dynamic_cast<vgui::Button *>(FindChildByName( "MannUpNowButton", true )); Assert( m_pMannUpNowButton );

	m_iImageHasTicket = m_pImageList->AddImage( vgui::scheme()->GetImage( "pve/mvm_ticket_small", true ) );
		m_pImageList->GetImage( m_iImageHasTicket )->SetSize( m_iHasTicketWidth, m_iHasTicketWidth );
	m_iImageNoTicket = m_pImageList->AddImage( vgui::scheme()->GetImage( "pve/mvm_no_ticket_small", true ) );
		m_pImageList->GetImage( m_iImageNoTicket )->SetSize( m_iHasTicketWidth, m_iHasTicketWidth );
	m_iImageSquadSurplus = m_pImageList->AddImage( vgui::scheme()->GetImage( "pve/mvm_squad_surplus_small", true ) );
		m_pImageList->GetImage( m_iImageSquadSurplus )->SetSize( m_iSquadSurplusWidth, m_iSquadSurplusWidth );
	m_iImageNoSquadSurplus = m_pImageList->AddImage( vgui::scheme()->GetImage( "pve/mvm_no_squad_surplus_small", true ) );
		m_pImageList->GetImage( m_iImageNoSquadSurplus )->SetSize( m_iSquadSurplusWidth, m_iSquadSurplusWidth );

	m_pChatPlayerList->SetImageList( m_pImageList, false );
	m_pChatPlayerList->SetVisible( true );
	m_pChallengeList->SetImageList( m_pImageList, false );
#ifdef USE_MVM_TOUR
	m_pTourList->SetImageList( m_pImageList, false );
#endif // USE_MVM_TOUR

	//
	// Populate the challenge list
	//

	m_pChallengeList->AddActionSignalTarget( this );
#ifdef USE_MVM_TOUR
	m_pTourList->AddActionSignalTarget( this );
#endif // USE_MVM_TOUR
	m_pSquadSurplusCheckButton->AddActionSignalTarget( this );
	m_pChatPlayerList->AddActionSignalTarget( this );
	m_pOpenStoreButton->AddActionSignalTarget( this );
	m_pOpenStoreButton2->AddActionSignalTarget( this );
	m_pOpenHelpButton->AddActionSignalTarget( this );
	m_pMannUpNowButton->AddActionSignalTarget( this );

	m_pChallengeList->SetVerticalScrollbar( true );
	m_pChallengeList->RemoveAll();
	m_pChallengeList->RemoveAllSections();
	m_pChallengeList->SetDrawHeaders( true );
	m_pChallengeList->SetClickable( true );
	m_pChallengeList->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pChallengeList->SetBorder( NULL );

#ifdef USE_MVM_TOUR
	m_pTourList->SetDrawHeaders( false );
#endif // USE_MVM_TOUR

	m_fontChallengeListHeader = pScheme->GetFont( "HudFontSmallestBold", true );
	m_fontChallengeListItem = pScheme->GetFont( "HudFontSmallest", true );

	//
	// Populate the player list
	//
	int nAvatarWidth = ( ( m_iAvatarWidth * 5 / 4 ) + 1 );
	int nExtraWidth = m_pChatPlayerList->GetWide() - nAvatarWidth - m_iPlayerNameWidth - m_iBannedWidth - m_iHasTicketWidth - m_iSquadSurplusWidth - m_iBadgeLevelWidth;

	m_pChatPlayerList->AddColumnToSection( 0, "avatar", "#TF_Players", vgui::SectionedListPanel::COLUMN_IMAGE, nAvatarWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "name", "", 0, m_iPlayerNameWidth + nExtraWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "is_banned", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, m_iBannedWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "has_ticket", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, m_iHasTicketWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "squad_surplus", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, m_iSquadSurplusWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "badge_level", "#TF_MvM_Tours", vgui::SectionedListPanel::COLUMN_CENTER, m_iBadgeLevelWidth );
}

void CLobbyPanel_MvM::PerformLayout()
{
#ifdef USE_MVM_TOUR
	WriteTourList();
#endif // USE_MVM_TOUR
	WriteChallengeList();

	BaseClass::PerformLayout();
}

void CLobbyPanel_MvM::OnClickedOnChallenge()
{
	int iSelected = m_pChallengeList->GetSelectedItem();
	m_pChallengeList->SetSelectedItem( -1 );
	if ( iSelected < 0 )
		return;
	if ( BIsPartyLeader() && BIsPartyInUIState() )
	{
		int iChallengeIndex = m_pChallengeList->GetItemData( iSelected )->GetInt( "pop_index", -1 );

#ifndef USE_MVM_TOUR
		// disallow player to select individual challenge in mannup
		if ( iChallengeIndex >= 0 && GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
			return;
#endif // !USE_MVM_TOUR

		CMvMMissionSet searchChallenges;

		// Fetch current selection.  Except when clicking the "only uncompleted" checkbox, which is special
		if ( iChallengeIndex != k_iPopIndex_OnlyNotYetCompleted )
			GTFGCClientSystem()->GetSearchChallenges( searchChallenges );

		CMvMMissionSet setChallenges;
		GetMvmChallengeSet( iChallengeIndex, setChallenges );
		bool bSelect = ( m_pChallengeList->GetItemData( iSelected )->GetInt( "check_box" ) != m_iImageCheckBoxYes );
		for ( int i = 0 ; i < GetItemSchema()->GetMvmMissions().Count() ; ++i )
		{
			if ( setChallenges.GetMissionBySchemaIndex( i ) )
			{
				searchChallenges.SetMissionBySchemaIndex( i, bSelect );
			}
		}

		GTFGCClientSystem()->SetSearchChallenges( searchChallenges );
	}
	else
	{
		WriteChallengeList();
	}
}


void CLobbyPanel_MvM::ChallengeList::Paint()
{
	vgui::SectionedListPanel::Paint();

	FOR_EACH_VEC( m_vecMapImages, i )
	{
		int x, y, w, h;
		if ( !GetSectionHeaderBounds( i + 1, x, y, w, h ) )
		{
			Assert( "MvM map mismatch" );
			continue;
		}

		// Dear god.  Why is VGUI such a piece of crap?
		// And why is it such excruciating pain to do
		// anything at all?

		// Select subrectangle within image to draw
		w = m_pLobbyPanel->m_iMapImageWidth;
		h = m_pLobbyPanel->m_iMapImageHeight;
		m_vecMapImages[i].SetViewport( true, 0.0, 0.0, 1.0, (float)h / (float)w );

		// Compute horiziontal position of icon
		int gutter = vgui::scheme()->GetProportionalScaledValue( 5 );
		x -= gutter + m_pLobbyPanel->m_iMapImageWidth;

		// Save clipping rectangle.  We want to be able to draw off to the left,
		// outside of our bounding box, but we need to keep the vertical clipping
		int left, top, right, bottom;
		bool bDisabled;
		g_pMatSystemSurface->GetClippingRect( left, top, right, bottom, bDisabled );

		// Adjust clipping rectangle
		int sx = x;
		int sy = y;
		LocalToScreen(sx, sy);
		g_pMatSystemSurface->SetClippingRect( sx, top, right, bottom );

		m_vecMapImages[i].DoPaint( x, y, w, h );

		// Restore clipping rectangle
		g_pMatSystemSurface->SetClippingRect( left, top, right, bottom );
		g_pMatSystemSurface->DisableClipping( bDisabled );
	}

#ifdef USE_MVM_TOUR
	// We can only do checkmarks if we know what tour they are working towards
	int idxTour = GTFGCClientSystem()->GetSearchMannUpTourIndex();
	if ( idxTour >= 0 )
	{
		int nCheckSize = m_pLobbyPanel->m_iChallengeCompletedSize;
		int nCompletedX0 = m_pLobbyPanel->m_iChallengeCheckBoxWidth + m_pLobbyPanel->m_iChallengeNameWidth + m_pLobbyPanel->m_iChallengeSkillWidth / 4;

		uint32 nTours, nCompletedChallenge;
		bool bFoundPlayerCompletedChallenges = GTFGCClientSystem()->BGetLocalPlayerBadgeInfoForTour( idxTour, &nTours, &nCompletedChallenge );

		for ( int i = 0 ; i < GetItemCount() ; ++i )
		{
			// Get The Pop File Name for this item
			KeyValues *pkv = GetItemData( GetItemIDFromRow( i ) );
			int iMissionIndexInSchema = pkv->GetInt( "pop_index", -1 );
			if ( iMissionIndexInSchema < 0 ) // special multi-select entry
				continue;

			int iBadgeSlot = GetItemSchema()->GetMvmMissionBadgeSlotForTour( idxTour, iMissionIndexInSchema );
			if ( iBadgeSlot < 0 )
				continue;

			int x, y, w, h;
			GetItemBounds( i, x, y, w, h );

			if ( bFoundPlayerCompletedChallenges )
			{
				if ( nCompletedChallenge & (1 << iBadgeSlot) )
				{
					m_imageChallengeCompleted.SetColor( Color(255,255,255,255) );
				}
				else if ( GetSelectedItem() == i )
				{
					m_imageChallengeCompleted.SetColor( Color(0,0,0,255) );
				}
				else
				{
					m_imageChallengeCompleted.SetColor( Color(0,0,0,70) );
				}
				int checkX0 = nCompletedX0;
				int checkY0 = y + ( h - nCheckSize ) / 2;
				m_imageChallengeCompleted.DoPaint( checkX0, checkY0, nCheckSize, nCheckSize );
			}
		}
	}
#endif // USE_MVM_TOUR
}
