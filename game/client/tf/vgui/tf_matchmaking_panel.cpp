//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_matchmaking_panel.h"
#include "ienginevgui.h"
#include "c_tf_gamestats.h"
#include "clientmode_tf.h"
#include "tf_hud_mainmenuoverride.h"
#include "vgui_int.h"
#include "IGameUIFuncs.h" // for key bindings
#include <vgui_controls/AnimationController.h>
#include "vgui/IInput.h"
#include "tf_gc_client.h"
#include "tf_party.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>
															 
void AddSubKeyNamed( KeyValues *pKeys, const char *pszName );
extern void ShowEconRequirementDialog( const char *pTitle, const char *pText, const char *pItemDefName );

CMatchMakingPanel *GetMatchMakingPanel()
{
	CMatchMakingPanel *pMatchMakingPanel = (CMatchMakingPanel*)gViewPortInterface->FindPanelByName( PANEL_MATCHMAKING );
	return pMatchMakingPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatchMakingPanel::CMatchMakingPanel( IViewPort *pViewPort ) : EditablePanel( NULL, PANEL_MATCHMAKING ), m_iMMPanelKey( BUTTON_CODE_INVALID )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	SetProportional( true );

	ListenForGameEvent( "gameui_hidden" );
	ListenForGameEvent( "client_beginconnect" );

	EditablePanel *m_pMainContainer = new EditablePanel( this, "MainContainer" );
	Assert( m_pMainContainer );
	
	m_pCompetitiveModeGroupPanel = new vgui::EditablePanel( m_pMainContainer, "CompetitiveModeGroupBox" );
	m_pModeLabel = new vgui::Label( m_pCompetitiveModeGroupPanel, "LadderLabel", "" );
	m_pModeComboBox = new vgui::ComboBox( m_pCompetitiveModeGroupPanel, "ModeComboBox", 3, false );
	m_pModeComboBox->AddActionSignalTarget( this );
	m_pStopSearchButton = new vgui::Button( m_pCompetitiveModeGroupPanel, "StopSearchButton", "" );
	m_pStopSearchButton->AddActionSignalTarget( this );
	m_pSearchButton = new vgui::Button( m_pCompetitiveModeGroupPanel, "SearchButton", "" );
	m_pSearchButton->AddActionSignalTarget( this );
	m_pSearchActiveGroupBox = new vgui::EditablePanel( m_pMainContainer, "SearchActiveGroupBox" );
	m_pSearchActiveTitleLabel = new vgui::Label( m_pSearchActiveGroupBox, "SearchActiveTitle", "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMatchMakingPanel::~CMatchMakingPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::AttachToGameUI( void )
{
	C_CTFGameStats::ImmediateWriteInterfaceEvent( "interface_open",	"tf_matchmaking_panel" );

	if ( GetClientModeTFNormal()->GameUI() )
	{
		GetClientModeTFNormal()->GameUI()->SetMainMenuOverride( GetVPanel() );
	}

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
	SetCursor(dc_arrow);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CMatchMakingPanel::GetName( void )
{
	return PANEL_MATCHMAKING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/MatchMakingPanel.res" );

	// The outer dim / close button
	{
		Button *pButton = FindControl< Button >( "OutsideCloseButton" );
		if ( pButton )
		{
			pButton->AddActionSignalTarget( this );
			pButton->SetPaintBackgroundEnabled( false );
		}
	}

	if ( m_pModeComboBox && m_pModeComboBox->IsVisible() )
	{
		// Placeholder
		m_pModeComboBox->RemoveAll();
		vgui::HFont hFont = pScheme->GetFont( "HudFontSmallestBold", true );
		m_pModeComboBox->SetFont( hFont );
		KeyValues *pKeyValues = new KeyValues( "data" );
		pKeyValues->SetInt( "bracket", k_nMatchGroup_Ladder_6v6 );
		m_pModeComboBox->AddItem( "6v6", pKeyValues );
		pKeyValues->SetInt( "bracket", k_nMatchGroup_Ladder_9v9 );
		m_pModeComboBox->AddItem( "9v9", pKeyValues );
		pKeyValues->deleteThis();
		m_pModeComboBox->ActivateItemByRow( 0 );
	}

	if ( m_pSearchButton )
	{
		m_pSearchButton->AddActionSignalTarget( this );
	}
	if ( m_pStopSearchButton )
	{
		m_pStopSearchButton->AddActionSignalTarget( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::PerformLayout()
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::OnCommand( const char *pCommand )
{
	if ( FStrEq( pCommand, "close" ) )
	{
		if ( enginevgui->IsGameUIVisible() )
		{
			ShowPanel( false );
		}
		else
		{
			IViewPortPanel *pMMPanel = ( gViewPortInterface->FindPanelByName( PANEL_MATCHMAKING ) );
			if ( pMMPanel )
			{
				gViewPortInterface->ShowPanel( pMMPanel, false );
			}
		}
	}
	else if ( FStrEq( pCommand, "search" ) )
	{
		StartSearch();
	}
	else if ( FStrEq( pCommand, "stopsearch" ) )
	{
		StopSearch();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::OnTextChanged( KeyValues *data )
{
	if ( !IsVisible() )
		return;

	if ( !GCClientSystem()->BConnectedtoGC() )
		return;

	Panel *pPanel = reinterpret_cast< vgui::Panel* >( data->GetPtr( "panel" ) );

	vgui::ComboBox *pComboBox = dynamic_cast< vgui::ComboBox* >( pPanel );
	if ( pComboBox )
	{
		if ( pComboBox == m_pModeComboBox )
		{
			KeyValues *pUserData = m_pModeComboBox->GetActiveItemUserData();
			if ( !pUserData )
				return;

			uint32 unModeType = pUserData->GetInt( "bracket", (uint32)-1 );

			if ( unModeType >= k_nMatchGroup_Ladder_First && unModeType <= k_nMatchGroup_Ladder_Last )
			{
				GTFGCClientSystem()->SetLadderType( unModeType );
			}
// 			else if ( unModeType >= k_nMatchGroup_Quickplay_First && unModeType <= k_nMatchGroup_Quickplay_Last )
// 			{
// 				GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_QUICKPLAY );
// 			}

			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "gameui_hidden" ) )
	{
		ShowPanel( false );
		return;
	}
	else if ( FStrEq( event->GetName(), "client_beginconnect" ) )
	{
		ShowPanel( false );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::ShowPanel( bool bShow )
{
	// Snag this so we know what to listen for
	m_iMMPanelKey = gameuifuncs->GetButtonCodeForBind( "show_matchmaking" );

	if ( bShow 
	  && ( !steamapicontext
		|| !steamapicontext->SteamUtils()
		|| !steamapicontext->SteamMatchmakingServers()
		|| !steamapicontext->SteamUser()
		|| !steamapicontext->SteamUser()->BLoggedOn() ) )
	{
		Warning( "Steam not properly initialized or connected.\n" );
		ShowMessageBox( "#TF_MM_GenericFailure_Title", "#TF_MM_GenericFailure", "#GameUI_OK" );
		ShowPanel( false );
	}

	if ( bShow && !GCClientSystem()->BConnectedtoGC() )
	{
		Warning( "Not connected to GC.\n" );
		ShowMessageBox( "#TF_MM_NoGC_Title", "#TF_MM_NoGC", "#GameUI_OK" );
		ShowPanel( false );
	}

	bool bInMMGame = GTFGCClientSystem()->GetMatchmakingUIState() == eMatchmakingUIState_InGame &&
					 GTFGCClientSystem()->GetAssignedMatchAbandonStatus() == k_EAbandonGameStatus_AbandonWithPenalty;
	bool bSearching = GTFGCClientSystem()->GetMatchmakingUIState() == eMatchmakingUIState_InQueue;

	// DevMsg( "CMatchmakingPanel shown, bInMMGame %d bSearching %d\n", bInMMGame, bSearching );

	if ( m_pModeLabel )
	{
		m_pModeLabel->SetVisible( true );
	}
	if ( m_pModeComboBox )
	{
		m_pModeComboBox->SetEnabled( !bSearching && !bInMMGame );
		m_pModeComboBox->SetVisible( !bSearching );
	}
	if ( m_pSearchButton )
	{
		m_pSearchButton->SetEnabled( !bSearching && !bInMMGame );
		m_pSearchButton->SetVisible( !bSearching && !bInMMGame );
	}
	if ( m_pStopSearchButton )
	{
		m_pStopSearchButton->SetEnabled( bSearching && !bInMMGame );
		m_pStopSearchButton->SetVisible( bSearching && !bInMMGame );
	}
	if ( m_pSearchActiveGroupBox )
	{
		m_pSearchActiveGroupBox->SetVisible( bSearching && !bInMMGame );
	}
	
	if ( bShow )
	{
		KeyValues *pUserData = m_pModeComboBox->GetActiveItemUserData();
		if ( pUserData )
		{
			uint32 unModeType = pUserData->GetInt( "bracket", (uint32)-1 );

			if ( unModeType >= k_nMatchGroup_Ladder_First && unModeType <= k_nMatchGroup_Ladder_Last )
			{
				GTFGCClientSystem()->SetLadderType( unModeType );
			}
		}
	}

	SetVisible( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::OnKeyCodePressed( KeyCode code )
{
	if ( code == m_iMMPanelKey )
	{
		ShowPanel( false );
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		if ( IsVisible() )
		{
			SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::OnThink()
{
	if ( GTFGCClientSystem()->GetWizardStep() == TF_Matchmaking_WizardStep_SEARCHING )
	{
		const CMsgMatchmakingProgress &progress = GTFGCClientSystem()->m_msgMatchmakingProgress;
		wchar_t wszCount[32];
		CUtlVector< vgui::Label* > vecNearbyFields;
		vgui::Label *pNearbyColumnHead = dynamic_cast< vgui::Label* >( FindChildByName( "NearbyColumnHead", true ) );
		Assert( pNearbyColumnHead );
		vecNearbyFields.AddToTail( pNearbyColumnHead );

#define DO_FIELD( protobufname, labelname, bNearby ) \
			vgui::Label *p##labelname = dynamic_cast< vgui::Label* >( FindChildByName( #labelname, true ) ); \
			Assert( p##labelname ); \
			if ( p##labelname ) \
									{ \
				if ( progress.has_##protobufname() ) \
												{ \
					_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", progress.protobufname() ); \
					p##labelname->SetText( wszCount ); \
					if ( bNearby ) bHasAnyNearbyData = true; \
												} \
						else \
								{ \
					p##labelname->SetText( "#TF_Matchmaking_NoData" ); \
								} \
				if ( bNearby ) vecNearbyFields.AddToTail( p##labelname ); \
									}

		bool bHasAnyNearbyData = false;
		DO_FIELD( matching_worldwide_searching_players, PlayersSearchingMatchingWorldwideValue, false )
		DO_FIELD( matching_near_you_searching_players, PlayersSearchingMatchingNearbyValue, true )
		DO_FIELD( matching_worldwide_active_players, PlayersInGameMatchingWorldwideValue, false )
		DO_FIELD( matching_near_you_active_players, PlayersInGameMatchingNearbyValue, true )
		DO_FIELD( matching_worldwide_empty_gameservers, EmptyGameserversMatchingWorldwideValue, false )
		DO_FIELD( matching_near_you_empty_gameservers, EmptyGameserversMatchingNearbyValue, true )
		DO_FIELD( total_worldwide_searching_players, PlayersSearchingTotalWorldwideValue, false )
		DO_FIELD( total_near_you_searching_players, PlayersSearchingTotalNearbyValue, true )
		DO_FIELD( total_worldwide_active_players, PlayersInGameTotalWorldwideValue, false )
		DO_FIELD( total_near_you_active_players, PlayersInGameTotalNearbyValue, true )

		FOR_EACH_VEC( vecNearbyFields, i )
		{
			vecNearbyFields[i]->SetVisible( bHasAnyNearbyData );
		}

		// HOLY CHEESEBALL BUSY INDICATOR
		const wchar_t *pwszEllipses = &L"....."[4 - ( (unsigned)Plat_FloatTime() % 5U )];
		wchar_t wszLocalized[512];
		g_pVGuiLocalize->ConstructString_safe( wszLocalized, g_pVGuiLocalize->Find( "#TF_Matchmaking_Searching" ), 1, pwszEllipses );
		m_pSearchActiveTitleLabel->SetText( wszLocalized );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::SetVisible( bool bState ) 
{
	if ( bState == true )
	{
// 		IGameEvent *event = gameeventmanager->CreateEvent( "questlog_opened" );
// 		if ( event )
// 		{
// 			gameeventmanager->FireEventClientSide( event );
// 		}

		if ( enginevgui->IsGameUIVisible() )
		{
			AttachToGameUI();
		}
		else
		{
			ipanel()->SetParent( GetVPanel(), VGui_GetClientDLLRootPanel() );

			MakePopup( false, true );
			SetKeyBoardInputEnabled( true );
			SetMouseInputEnabled( true );
			MoveToFront();
		}

		engine->ClientCmd_Unrestricted( "gameui_preventescapetoshow\n" );
		vgui::surface()->PlaySound( "ui/panel_open.wav" );
	}
	else if ( IsVisible() )
	{
		// Detach from the GameUI when we hide
		IViewPortPanel *pMMOverride = gViewPortInterface->FindPanelByName( PANEL_MAINMENUOVERRIDE );
		if ( pMMOverride )
		{
			((CHudMainMenuOverride*)pMMOverride)->AttachToGameUI();	
		}

		engine->ClientCmd_Unrestricted( "gameui_allowescapetoshow\n" );
		vgui::surface()->PlaySound( "ui/panel_close.wav" );
	}

	BaseClass::SetVisible( bState );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::StartSearch( void )
{
#ifdef STAGING_ONLY
	GTFGCClientSystem()->EndMatchmaking();

	bool bCompetitive = GTFGCClientSystem()->GetWizardStep() == TF_Matchmaking_WizardStep_LADDER;

	// Solo
	CTFParty *pParty = GTFGCClientSystem()->GetParty();
	if ( !pParty || pParty->GetNumMembers() <= 1 )
	{
		if ( bCompetitive && !GTFGCClientSystem()->BHasCompetitiveAccess() )
		{
			ShowEconRequirementDialog( "#TF_Competitive_RequiresPass_Title", "#TF_Competitive_RequiresPass", CTFItemSchema::k_rchLadderPassItemDefName );
			return;
		}
		else if ( bCompetitive && !CheckCompetitiveConvars() )
		{
			ShowMessageBox( "#TF_Competitive_Convars_CantProceed_Title", "#TF_Competitive_Convars_CantProceed", "#GameUI_OK" );
			return;
		}
	}
	// Group
	else
	{
		wchar_t wszLocalized[512];
		char szLocalized[512];
		wchar_t wszCharPlayerName[128];

		bool bAnyMembersWithoutAuth = false;

		for ( int i = 0; i < pParty->GetNumMembers(); ++i )
		{
			if ( bCompetitive )
			{
				if ( !pParty->Obj().members( i ).competitive_access() )
				{
					bAnyMembersWithoutAuth = true;
					V_UTF8ToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( pParty->GetMember( i ) ), wszCharPlayerName, sizeof( wszCharPlayerName ) );
					g_pVGuiLocalize->ConstructString_safe( wszLocalized, g_pVGuiLocalize->Find( "#TF_Matchmaking_MissingPass" ), 1, wszCharPlayerName );
					g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof( szLocalized ) );

					GTFGCClientSystem()->SendSteamLobbyChat( CTFGCClientSystem::k_eLobbyMsg_SystemMsgFromLeader, szLocalized );
				}
			}
		}

		if ( bAnyMembersWithoutAuth )
		{
			ShowMessageBox( "#TF_Competitive_RequiresPass_Title", "#TF_Competitive_RequiresPass", "#GameUI_OK" );
			return;
		}
	}

	{
		KeyValues *pUserData = m_pModeComboBox->GetActiveItemUserData();
		if ( !pUserData )
			return;

		uint32 unModeType = pUserData->GetInt( "bracket", (uint32)-1 );
		if ( unModeType >= k_nMatchGroup_Ladder_First && unModeType <= k_nMatchGroup_Ladder_Last )
		{
			GTFGCClientSystem()->BeginMatchmaking( TF_Matchmaking_LADDER );
		}
		else if ( unModeType >= k_nMatchGroup_Quickplay_First && unModeType <= k_nMatchGroup_Quickplay_Last )
		{
			// No longer support quickplay
			Assert( false );
		}
	}


	GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_SEARCHING );

	if ( m_pModeLabel )
	{
		m_pModeLabel->SetVisible( false );
	}
	if ( m_pModeComboBox )
	{
		m_pModeComboBox->SetEnabled( false );
		m_pModeComboBox->SetVisible( false );
	}
	if ( m_pSearchButton )
	{
		m_pSearchButton->SetEnabled( false );
		m_pSearchButton->SetVisible( false );
	}
	if ( m_pStopSearchButton )
	{
		m_pStopSearchButton->SetEnabled( true );
		m_pStopSearchButton->SetVisible( true );
	}
	if ( m_pSearchActiveGroupBox )
	{
		m_pSearchActiveGroupBox->SetVisible( true );
	}
#endif // STAGING_ONLY
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchMakingPanel::StopSearch( void )
{
	if ( m_pModeLabel )
	{
		m_pModeLabel->SetVisible( true );
	}
	if ( m_pModeComboBox )
	{
		m_pModeComboBox->SetEnabled( true );
		m_pModeComboBox->SetVisible( true );
	}
	if ( m_pSearchButton )
	{
		m_pSearchButton->SetEnabled( true );
		m_pSearchButton->SetVisible( true );
	}
	if ( m_pStopSearchButton )
	{
		m_pStopSearchButton->SetEnabled( false );
		m_pStopSearchButton->SetVisible( false );
	}
	if ( m_pSearchActiveGroupBox )
	{
		m_pSearchActiveGroupBox->SetVisible( false );
	}

	GTFGCClientSystem()->EndMatchmaking();
}

#ifdef STAGING_ONLY
// Just VGUI things...
static void cc_tf_mm_panel_reload()
{
	CMatchMakingPanel *pPanel = GetMatchMakingPanel();
	if ( pPanel )
	{
		pPanel->InvalidateLayout( true, true );
		gViewPortInterface->ShowPanel( pPanel, true );
	}
}
ConCommand tf_mm_panel_reload( "tf_mm_panel_reload", cc_tf_mm_panel_reload );
#endif
