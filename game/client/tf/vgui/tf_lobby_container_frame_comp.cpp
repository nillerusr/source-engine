//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_gc_client.h"
#include "tf_party.h"

#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ScrollableEditablePanel.h"
#include "vgui_avatarimage.h"
#include "tf_leaderboardpanel.h"
#include "tf_lobbypanel_comp.h"
#include "tf_hud_mainmenuoverride.h"

#include "tf_lobby_container_frame_comp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------

// Purpose: Override of the generic messagebox dialog to provide a welcome-to-competitive message

//-----------------------------------------------------------------------------
ConVar tf_comp_welcome_hide_forever( "tf_comp_welcome_hide_forever", "0", FCVAR_ARCHIVE | FCVAR_HIDDEN );
ConVar tf_comp_welcome_hide( "tf_comp_welcome_hide", "0", FCVAR_HIDDEN );
class CTFCompetitiveWelcomeDialog : public CTFMessageBoxDialog
{
	DECLARE_CLASS_SIMPLE( CTFCompetitiveWelcomeDialog, CTFMessageBoxDialog );
public:
	CTFCompetitiveWelcomeDialog()
		: CTFMessageBoxDialog( NULL, (const char *)NULL, NULL, NULL, NULL )
		{}

	virtual ~CTFCompetitiveWelcomeDialog() {};

	virtual void OnCommand( const char *command )
	{
		if ( FStrEq( "hideforever", command ) )
		{
			tf_comp_welcome_hide_forever.SetValue( 1 );
			return;
		}
		else if ( FStrEq( "show_explanations", command ) )
		{
			CHudMainMenuOverride *pMMOverride = (CHudMainMenuOverride*)( gViewPortInterface->FindPanelByName( PANEL_MAINMENUOVERRIDE ) );
			pMMOverride->GetCompLobbyPanel()->OnCommand( command );
			OnCommand( "confirm" );
			return;
		}

		BaseClass::OnCommand( command );
	}

	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel )
	{
		CheckButton* pNeverAskAgainCheckBox = FindControl< CheckButton >( "NeverShowAgainCheckBox" );
		if ( panel == pNeverAskAgainCheckBox )
		{
			tf_comp_welcome_hide_forever.SetValue( pNeverAskAgainCheckBox->IsSelected() );
		}
	}

	virtual const char *GetResFile() OVERRIDE
	{
		// FIXME controller?
		return "Resource/UI/CompetitiveWelcomeDialog.res";
	}
};


//-----------------------------------------------------------------------------
CLobbyContainerFrame_Comp::CLobbyContainerFrame_Comp() 
	: CBaseLobbyContainerFrame( "LobbyContainerFrame" )
{
	// Our internal lobby panel
	m_pContents = new CLobbyPanel_Comp( this, this );
	m_pContents->AddActionSignalTarget( this );
	AddPage( m_pContents, "#TF_Matchmaking_HeaderCompetitive" );
	GetPropertySheet()->SetNavToRelay( m_pContents->GetName() );
	m_pContents->SetVisible( true );
}

//-----------------------------------------------------------------------------
CLobbyContainerFrame_Comp::~CLobbyContainerFrame_Comp( void )
{
}

void CLobbyContainerFrame_Comp::ShowPanel( bool bShow )
{
	if ( bShow )
	{
		if ( tf_comp_welcome_hide.GetBool() == false 
		  && tf_comp_welcome_hide_forever.GetBool() == false
		  && GTFGCClientSystem()->GetWizardStep() == TF_Matchmaking_WizardStep_LADDER )
		{
			CTFCompetitiveWelcomeDialog *pDialog = vgui::SETUP_PANEL( new CTFCompetitiveWelcomeDialog() );

			if ( pDialog )
			{
				tf_comp_welcome_hide.SetValue( 1 );
				pDialog->Show();
			}
			else
			{
				Warning( "Failed to create CompetitiveWelcomeDialog.  Outdated HUD?\n" );
			}
		}
	}



	BaseClass::ShowPanel( bShow );
}

void CLobbyContainerFrame_Comp::OnCommand( const char *command )
{
	if ( FStrEq( command, "next" ) )
	{
		switch ( GTFGCClientSystem()->GetWizardStep() )
		{
			case TF_Matchmaking_WizardStep_LADDER:
				StartSearch();
				break;

			default:
				AssertMsg1( false, "Unexpected wizard step %d", (int)GTFGCClientSystem()->GetWizardStep() );
				break;
		}
		return;
	}
	else if ( FStrEq( command, "show_explanations" ) )
	{
		CExplanationPopup *pPopup = FindControl<CExplanationPopup>( "StartExplanation" );
		if ( pPopup )
		{
			pPopup->Popup();
		}
		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
void CLobbyContainerFrame_Comp::WriteControls()
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
	
	if ( GCClientSystem()->BConnectedtoGC() )
	{
		if ( BIsPartyLeader()  )
		{
			switch ( GTFGCClientSystem()->GetWizardStep() )
			{
				case TF_Matchmaking_WizardStep_LADDER:
					pszBackButtonText = "#TF_Matchmaking_Back";
					pszNextButtonText = "#TF_Matchmaking_StartSearch";
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

	m_pBackButton->SetText( pszBackButtonText );
	m_pNextButton->SetText( pszNextButtonText );
	m_pNextButton->SetVisible( pszNextButtonText != NULL );

	BaseClass::WriteControls();
}

//-----------------------------------------------------------------------------
bool CheckCompetitiveConvars()
{
	static ConVarRef mat_dxlevel( "mat_dxlevel");
	return mat_dxlevel.GetInt() >= 90;
}

//-----------------------------------------------------------------------------
bool CLobbyContainerFrame_Comp::VerifyPartyAuthorization() const
{
	// Solo
	CTFParty *pParty = GTFGCClientSystem()->GetParty();
	if ( pParty == NULL || pParty->GetNumMembers() <= 1 )
	{
		if ( !GTFGCClientSystem()->BHasCompetitiveAccess() )
		{
			ShowEconRequirementDialog( "#TF_Competitive_RequiresPass_Title", "#TF_Competitive_RequiresPass", CTFItemSchema::k_rchLadderPassItemDefName );
			return false;
		}
		else if ( !CheckCompetitiveConvars() )
		{
			ShowMessageBox( "#TF_Competitive_Convars_CantProceed_Title", "#TF_Competitive_Convars_CantProceed", "#GameUI_OK" );
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

		for ( int i = 0 ; i < pParty->GetNumMembers() ; ++i )
		{
			// Need a ticket and two-factor for the beta
			if ( !pParty->Obj().members( i ).competitive_access() )
			{
				bAnyMembersWithoutAuth = true;
				V_UTF8ToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( pParty->GetMember( i ) ), wszCharPlayerName, sizeof( wszCharPlayerName ) );
				g_pVGuiLocalize->ConstructString_safe( wszLocalized, g_pVGuiLocalize->Find( "#TF_Matchmaking_MissingPass" ), 1, wszCharPlayerName );
				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof( szLocalized ) );

				GTFGCClientSystem()->SendSteamLobbyChat( CTFGCClientSystem::k_eLobbyMsg_SystemMsgFromLeader, szLocalized );
			}
		}

		if ( bAnyMembersWithoutAuth )
		{
			ShowMessageBox( "#TF_Competitive_RequiresPass_Title", "#TF_Competitive_RequiresPass", "#GameUI_OK" );
			return false;
		}
	}

	CLobbyPanel_Comp* pCompContents = static_cast< CLobbyPanel_Comp* >( m_pContents );
	if ( pCompContents )
	{
		uint32 unModeType = pCompContents->GetMatchGroup();

		if ( unModeType >= k_nMatchGroup_Ladder_First && unModeType <= k_nMatchGroup_Ladder_Last )
		{
			GTFGCClientSystem()->SetLadderType( unModeType );
		}
	}

	return true;
}

void CLobbyContainerFrame_Comp::HandleBackPressed()
{
	switch ( GTFGCClientSystem()->GetWizardStep() )
	{
		case TF_Matchmaking_WizardStep_LADDER:
			// !FIXME! Really need to confirm this!
			GTFGCClientSystem()->EndMatchmaking();
			// And hide us
			ShowPanel( false );
			return;

		case TF_Matchmaking_WizardStep_SEARCHING:
			switch ( GTFGCClientSystem()->GetSearchMode() )
			{
				case TF_Matchmaking_LADDER:
					GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_LADDER );
					return;
			}
			break;

		default:
			Msg( "Unexpected wizard step %d", (int)GTFGCClientSystem()->GetWizardStep() );
			break;
	}

	// Unhandled case
	BaseClass::HandleBackPressed();
}
