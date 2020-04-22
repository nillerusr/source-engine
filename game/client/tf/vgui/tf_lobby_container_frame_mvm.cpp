//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_party.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/SectionedListPanel.h"
#include "tf_lobbypanel_mvm.h"

#include "tf_lobby_container_frame_mvm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
CLobbyContainerFrame_MvM::CLobbyContainerFrame_MvM()
	: CBaseLobbyContainerFrame( "LobbyContainerFrame" )
{
	// Our internal lobby panel
	m_pContents = new CLobbyPanel_MvM( this, this );
	m_pContents->MoveToFront();
	m_pContents->AddActionSignalTarget( this );
	AddPage( m_pContents, "#TF_Matchmaking_HeaderMvM" );
	GetPropertySheet()->SetNavToRelay( m_pContents->GetName() );
	m_pContents->SetVisible( true );
}

//-----------------------------------------------------------------------------
CLobbyContainerFrame_MvM::~CLobbyContainerFrame_MvM( void )
{
}

//-----------------------------------------------------------------------------
void CLobbyContainerFrame_MvM::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pStartPartyButton = dynamic_cast<vgui::Button *>(FindChildByName( "StartPartyButton", true )); Assert( m_pStartPartyButton );
	m_pPlayNowButton = dynamic_cast<vgui::Button *>(FindChildByName( "PlayNowButton", true )); Assert( m_pPlayNowButton );
	m_pPracticeButton = dynamic_cast<vgui::Button *>(FindChildByName( "PracticeButton", true )); Assert( m_pPracticeButton );
}

bool CLobbyContainerFrame_MvM::VerifyPartyAuthorization() const 
{
	// They want to Mann Up.  Confirm that everybody in the party has a ticket.
	// if they are in a party of one, we provide slightly more specific UI.
	bool bBraggingRights = GTFGCClientSystem()->GetSearchPlayForBraggingRights();

	// Early out. Anyone can play for free
	if ( !bBraggingRights )
		return true;
	
	// Solo
	CTFParty *pParty = GTFGCClientSystem()->GetParty();
	if ( pParty == NULL || pParty->GetNumMembers() <= 1 )
	{
		if ( bBraggingRights && !GTFGCClientSystem()->BLocalPlayerInventoryHasMvmTicket() )
		{
			ShowEconRequirementDialog( "#TF_MvM_RequiresTicket_Title", "#TF_MvM_RequiresTicket", CTFItemSchema::k_rchMvMTicketItemDefName );
			return false;
		}
	}
	// Group
	else
	{
		wchar_t wszLocalized[512];
		char szLocalized[512];
		wchar_t wszCharPlayerName[128];

		bool bAnyMembersWithoutAuth = false;

		if ( bBraggingRights )
		{
			for ( int i = 0 ; i < pParty->GetNumMembers() ; ++i )
			{
				if ( !pParty->Obj().members( i ).owns_ticket() )
				{
					bAnyMembersWithoutAuth = true;

					V_UTF8ToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( pParty->GetMember( i ) ), wszCharPlayerName, sizeof( wszCharPlayerName ) );
					g_pVGuiLocalize->ConstructString_safe( wszLocalized, g_pVGuiLocalize->Find( "#TF_Matchmaking_MissingTicket" ), 1, wszCharPlayerName );
					g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof( szLocalized ) );

					GTFGCClientSystem()->SendSteamLobbyChat( CTFGCClientSystem::k_eLobbyMsg_SystemMsgFromLeader, szLocalized );
				}
			}
		}

		if ( bAnyMembersWithoutAuth )
		{
			if ( bBraggingRights )
			{
				ShowMessageBox( "#TF_MvM_RequiresTicket_Title", "#TF_MvM_RequiresTicketParty", "#GameUI_OK" );
				return false;
			}
		}
	}

	return true;
}

void CLobbyContainerFrame_MvM::HandleBackPressed()
{
	switch ( GTFGCClientSystem()->GetWizardStep() )
	{
		case TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS:
			// !FIXME! Rreally need to confirm this!
			GTFGCClientSystem()->EndMatchmaking();
			// And hide us
			ShowPanel( false );
			return;

#ifdef USE_MVM_TOUR
		case TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY:
			GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
			return;
#endif // USE_MVM_TOUR

		case TF_Matchmaking_WizardStep_MVM_CHALLENGE:
#ifdef USE_MVM_TOUR
			if ( GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
			{
				TF_Matchmaking_WizardStep step = TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY;
				GTFGCClientSystem()->RequestSelectWizardStep( step );
			}
			else
			{
				GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
			}
#else // new mm
			GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );
#endif // USE_MVM_TOUR
			return;

		case TF_Matchmaking_WizardStep_SEARCHING:
			GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_CHALLENGE );
			return;

		default:
			Msg( "Unexpected wizard step %d", (int)GTFGCClientSystem()->GetWizardStep() );
			break;
	}
	
	// Unhandled case
	BaseClass::HandleBackPressed();
}

//-----------------------------------------------------------------------------
void CLobbyContainerFrame_MvM::OnCommand( const char *command )
{
	if ( FStrEq( command, "learn_more" ) )
	{
		if ( steamapicontext && steamapicontext->SteamFriends() )
		{
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( "http://www.teamfortress.com/mvm/" );
		}
		return;
	}
	else if ( FStrEq( command, "mannup" ) )
	{
		GTFGCClientSystem()->SetSearchPlayForBraggingRights( true );
#ifdef USE_MVM_TOUR
		GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY );
#else // new mm
		GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_CHALLENGE );
#endif // USE_MVM_TOUR	
		return;
	}
	else if ( FStrEq( command, "practice" ) )
	{
		GTFGCClientSystem()->SetSearchPlayForBraggingRights( false );
		GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_CHALLENGE );
		return;
	}
	else if ( FStrEq( command, "next" ) )
	{
		switch ( GTFGCClientSystem()->GetWizardStep() )
		{
			case TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS:
#ifdef USE_MVM_TOUR
				if ( GTFGCClientSystem()->GetSearchPlayForBraggingRights() )
				{
					GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY );
				}
				else
				{
					GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_CHALLENGE );
				}
				break;

			case TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY:
#endif // USE_MVM_TOUR
				GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_MVM_CHALLENGE );
				break;

			case TF_Matchmaking_WizardStep_MVM_CHALLENGE:
			case TF_Matchmaking_WizardStep_LADDER:
				StartSearch();
				break;

			default:
				AssertMsg1( false, "Unexpected wizard step %d", (int)GTFGCClientSystem()->GetWizardStep() );
				break;
		}
		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyContainerFrame_MvM::OnKeyCodePressed(vgui::KeyCode code)
{
	ButtonCode_t nButtonCode = GetBaseButtonCode( code );

	if ( nButtonCode == KEY_XBUTTON_Y )
	{
		static_cast< CLobbyPanel_MvM* >( m_pContents )->ToggleSquadSurplusCheckButton();
	}

	BaseClass::OnKeyCodePressed( code );
}


//-----------------------------------------------------------------------------
void CLobbyContainerFrame_MvM::WriteControls()
{
	// Make sure we want to be in matchmaking.  (If we don't, the frame should hide us pretty quickly.)
	// We might get an event or something right at the transition point occasionally when the UI should
	// not be visible
	if ( GTFGCClientSystem()->GetMatchmakingUIState() == eMatchmakingUIState_Inactive )
	{
		return;
	}
	const char *pszBackButtonText = "#TF_Matchmaking_Back";
	const char *pszNextButtonText = NULL;

	CMvMMissionSet challenges;
	GTFGCClientSystem()->GetSearchChallenges( challenges );

	bool bShowPlayNowButtons = false;

	if ( GCClientSystem()->BConnectedtoGC() )
	{
		if ( BIsPartyLeader()  )
		{
			switch ( GTFGCClientSystem()->GetWizardStep() )
			{
				case TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS:
				{
					if ( !m_pStartPartyButton->IsVisible() )
					{
						pszBackButtonText = "#TF_Matchmaking_LeaveParty";
					}
					else
					{
						pszBackButtonText = "#TF_Matchmaking_Back";
					}

					bShowPlayNowButtons = BIsPartyLeader();
					break;
				}

				case TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY:
#ifdef USE_MVM_TOUR
					pszBackButtonText = "#TF_Matchmaking_Back";
					pszNextButtonText = "#TF_MvM_SelectChallenge";

					SetNextButtonEnabled( GTFGCClientSystem()->GetSearchMannUpTourIndex() >= 0 );
#else // new mm
					AssertMsg( 0, "This is legacy code. We don't have concept of tour anymore." );
#endif // USE_MVM_TOUR
					break;

				case TF_Matchmaking_WizardStep_MVM_CHALLENGE:
					pszBackButtonText = "#TF_Matchmaking_Back";

					pszNextButtonText = "#TF_Matchmaking_StartSearch";
					SetNextButtonEnabled( !challenges.IsEmpty() );

					break;

				case TF_Matchmaking_WizardStep_SEARCHING:
					pszBackButtonText = "#TF_Matchmaking_CancelSearch";
					break;

				case TF_Matchmaking_WizardStep_INVALID:
					// Still being setup
					break;

				default:
					AssertMsg1( false, "Unknown wizard step %d", (int)GTFGCClientSystem()->GetWizardStep() );
					break;
			}
		}
		else
		{
			pszBackButtonText = "#TF_Matchmaking_LeaveParty";
			m_pNextButton->SetEnabled( false );
		}
	}

	m_pPlayNowButton->SetVisible( bShowPlayNowButtons );
	m_pPracticeButton->SetVisible( bShowPlayNowButtons );
	SetControlVisible( "LearnMoreButton", GTFGCClientSystem()->GetWizardStep() == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS );

	// Set appropriate page title
	switch ( GTFGCClientSystem()->GetSearchMode() )
	{
		case TF_Matchmaking_MVM:
			if ( GTFGCClientSystem()->GetSearchPlayForBraggingRights() || 
				 GTFGCClientSystem()->GetWizardStep() == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS )
			{
				GetPropertySheet()->SetTabTitle( 0, "#TF_MvM_HeaderCoop" );
			}
			else
			{
				GetPropertySheet()->SetTabTitle( 0, "#TF_MvM_HeaderPractice" );
			}
			break;

		default:
			AssertMsg1( false, "Invalid search mode %d", GTFGCClientSystem()->GetSearchMode() );
			break;
	}

	// Check if we already have a party, then make sure and show it
	if ( m_pStartPartyButton->IsVisible() && m_pContents->NumPlayersInParty() > 1 )
	{
		m_pContents->SetControlVisible( "PartyActiveGroupBox", true );
	}

	SetControlVisible( "PlayWithFriendsExplanation", ShouldShowPartyButton() );


	static_cast< CLobbyPanel_MvM* >( m_pContents )->SetMannUpTicketCount( GTFGCClientSystem()->GetLocalPlayerInventoryMvmTicketCount() );
	static_cast< CLobbyPanel_MvM* >( m_pContents )->SetSquadSurplusCount( GTFGCClientSystem()->GetLocalPlayerInventorySquadSurplusVoucherCount() );

	m_pBackButton->SetText( pszBackButtonText );
	if ( pszNextButtonText )
	{
		m_pNextButton->SetText( pszNextButtonText );
		m_pNextButton->SetVisible( true );
	}
	else
	{
		m_pNextButton->SetVisible( false );
	}

	BaseClass::WriteControls();
}

