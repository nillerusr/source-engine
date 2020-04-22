//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"
#include "tf_shareddefs.h"
#include "tf_matchmaking_dashboard.h"
#include "tf_gamerules.h"
#include "ienginevgui.h"
#include "clientmode_tf.h"
#include "tf_hud_disconnect_prompt.h"
#include "tf_gc_client.h"
#include "tf_party.h"
#include "../vgui2/src/VPanel.h"

using namespace vgui;
using namespace GCSDK;

ConVar tf_mm_dashboard_spew_enabled( "tf_mm_dashboard_spew_enabled", "0", FCVAR_ARCHIVE );
#define MMDashboardSpew(...)																		\
	do {																							\
		if ( tf_mm_dashboard_spew_enabled.GetBool() )												\
		{																							\
			ConColorMsg( Color( 187, 80, 255, 255 ), "MMDashboard:" __VA_ARGS__ );					\
		} 																							\
	} while(false)																					\

extern ConVar tf_mm_next_map_vote_time;

#ifdef STAGING_ONLY 
ConVar tf_mm_dashboard_force_show( "tf_mm_dashboard_force_show", "0", 0, "Force the mm dashboard to show" );
ConVar tf_mm_popup_state_override( "tf_mm_popup_state_override", "", 0, "Force state on mm dashboard popup" );
#endif



bool BInEndOfMatch()
{
	const bool bInEndOfMatch	= TFGameRules() &&
								  TFGameRules()->State_Get() == GR_STATE_GAME_OVER &&
								  GTFGCClientSystem()->BConnectedToMatchServer( false );

	return bInEndOfMatch;
}

//-----------------------------------------------------------------------------
// Purpose: Pnael that lives on the viewport that is a popup that we parent
//			the MM dashboard panels to
//-----------------------------------------------------------------------------
class CMatchMakingHUDPopupContainer : public Panel 
{
public:
	DECLARE_CLASS_SIMPLE( CMatchMakingHUDPopupContainer, Panel );
	CMatchMakingHUDPopupContainer()
		: Panel( g_pClientMode->GetViewport(), "MMDashboardPopupContainer" )
	{
		SetProportional( true );
		SetBounds( 0, 0, g_pClientMode->GetViewport()->GetWide(), g_pClientMode->GetViewport()->GetTall() );
		MakePopup();
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( false ); // This can never be true
		SetVisible( false );
		ivgui()->AddTickSignal( GetVPanel(), 100 );
	}

	virtual void OnTick()
	{
		BaseClass::OnThink();

		bool bChildrenVisible = false;
		int nCount = GetChildCount();
		for( int i=0; i < nCount && !bChildrenVisible; ++i )
		{
			CExpandablePanel* pChild = assert_cast< CExpandablePanel* >( GetChild( i ) );
			bChildrenVisible = bChildrenVisible || pChild->BIsExpanded() || ( !pChild->BIsExpanded() && pChild->GetPercentAnimated() != 1.f );
		}

		SetVisible( bChildrenVisible );
	}
};

CMMDashboardParentManager::CMMDashboardParentManager()
	: m_bAttachedToGameUI( false )
{
	ListenForGameEvent( "gameui_activated" );
	ListenForGameEvent( "gameui_hidden" );

	m_pHUDPopup = new CMatchMakingHUDPopupContainer();
}

//-----------------------------------------------------------------------------
// Purpose: Update who we need to parent the MM dashboard panels to
//-----------------------------------------------------------------------------
void CMMDashboardParentManager::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "gameui_activated" ) )
	{
		m_bAttachedToGameUI = false;
	}
	else if ( FStrEq( event->GetName(), "gameui_hidden" ) )
	{
		m_bAttachedToGameUI = true;
	}

	UpdateParenting();
}

void CMMDashboardParentManager::AddPanel( CExpandablePanel* pPanel )
{
	m_vecPanels.Insert( pPanel );
	UpdateParenting();
}

void CMMDashboardParentManager::RemovePanel( CExpandablePanel* pPanel )
{
	m_vecPanels.FindAndRemove( pPanel );
	m_vecPanels.RedoSort();
	UpdateParenting();
}

void CMMDashboardParentManager::PushModalFullscreenPopup( vgui::Panel* pPanel )
{
	m_vecFullscreenPopups.AddToTail( pPanel );
	UpdateParenting();
}

void CMMDashboardParentManager::PopModalFullscreenPopup( vgui::Panel* pPanel )
{
	m_vecFullscreenPopups.FindAndRemove( pPanel );
	UpdateParenting();
}

void CMMDashboardParentManager::UpdateParenting()
{
	m_bAttachedToGameUI ? AttachToGameUI() : AttachToTopMostPopup();
}

//-----------------------------------------------------------------------------
// Purpose: Parent the MM dashboard panels to the right panels
//-----------------------------------------------------------------------------
void CMMDashboardParentManager::AttachToGameUI()
{
	if ( !m_pHUDPopup )
	{
		return;
	}

	FOR_EACH_VEC( m_vecPanels, i )
	{
		CExpandablePanel *pPanel = m_vecPanels[ i ];
		bool bKBInput = pPanel->IsKeyBoardInputEnabled();
		bool bMouseInput = pPanel->IsMouseInputEnabled();

		pPanel->SetParent( (Panel*)m_pHUDPopup );

		// Restore mouse, KV input sensitivity because MakePopup forces both to true
		pPanel->SetKeyBoardInputEnabled( bKBInput );
		pPanel->SetMouseInputEnabled( bMouseInput );
		// Don't adopt the parent's proportionalness
		pPanel->SetProportional( true );
	}
}

void CMMDashboardParentManager::AttachToTopMostPopup()
{
	// Not being used.  Hide it.
	if ( m_pHUDPopup )
	{
		m_pHUDPopup->SetVisible( false );
	}

	FOR_EACH_VEC( m_vecPanels, i )
	{	
		Panel *pPanel = m_vecPanels[ i ];
		// No longer a popup
		surface()->ReleasePanel( pPanel->GetVPanel() );
		((VPanel*)pPanel->GetVPanel())->SetPopup( false );

		if ( m_vecFullscreenPopups.Count() )
		{
			pPanel->SetParent( m_vecFullscreenPopups.Tail() );
		}
		else
		{
			pPanel->SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
		}
		pPanel->MoveToFront();
		// Don't adopt the parent's proportionalness
		pPanel->SetProportional( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Snag the singleton CMMDashboardParentManager
//-----------------------------------------------------------------------------
CMMDashboardParentManager* GetMMDashboardParentManager()
{
	static CMMDashboardParentManager* s_pParentManager = NULL;
	if ( !s_pParentManager )
	{
		s_pParentManager = new CMMDashboardParentManager();
	}

	return s_pParentManager;
}


void CTFMatchmakingPopup::OnEnter()
{
	MMDashboardSpew( "Entering state %s\n", GetName() );

	Update();

	SetCollapsed( false );
}

void CTFMatchmakingPopup::OnUpdate() 
{
}

void CTFMatchmakingPopup::OnExit()
{
	MMDashboardSpew( "Exiting state %s\n", GetName() );
	SetCollapsed( true );
}


void CTFMatchmakingPopup::Update()
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFMatchmakingPopup::CTFMatchmakingPopup( const char* pszName, const char* pszResFile )
	: CExpandablePanel( NULL, pszName )
	, m_pszResFile( pszResFile )
	, m_bActive( false )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	ivgui()->AddTickSignal( GetVPanel(), 100 ); 

	GetMMDashboardParentManager()->AddPanel( this );
	SetKeyBoardInputEnabled( false );

	SetProportional( true );
	
	ListenForGameEvent( "rematch_failed_to_create" );
	ListenForGameEvent( "party_updated" );
}

CTFMatchmakingPopup::~CTFMatchmakingPopup()
{
	GetMMDashboardParentManager()->RemovePanel( this );
}

void CTFMatchmakingPopup::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetMouseInputEnabled( true );
	LoadControlSettings( m_pszResFile );

	// This cannot ever be true or else things get weird when in-game
	SetKeyBoardInputEnabled( false );

	if ( m_bActive )
	{
		OnEnter();
	}
	else
	{
		OnExit();
	}

	GetMMDashboardParentManager()->UpdateParenting();
}

void CTFMatchmakingPopup::OnThink()
{
	BaseClass::OnThink();

	// Move us to be touching the bottom of the dashboard panel
	// These panels have no relation whatsoever, so we're doing this manually
	Panel* pDashboard = GetMMDashboard();
	int nNewYPos = Max( pDashboard->GetYPos() + pDashboard->GetTall() - YRES(10), YRES(-5) );
	SetPos( GetXPos(), nNewYPos );

	if ( m_bActive )
	{
		OnUpdate();
	}
}

void CTFMatchmakingPopup::OnTick()
{
	BaseClass::OnTick();

	bool bShouldBeActive = ShouldBeActve();
	if ( bShouldBeActive != m_bActive )
	{
		if ( bShouldBeActive )
		{
			m_bActive = true;
			OnEnter();
		}
		else
		{
			m_bActive = false;
			OnExit();
		}
	}

	SetMouseInputEnabled( ShouldBeActve() );
	SetKeyBoardInputEnabled( false ); // Never
}


void CTFMatchmakingPopup::OnCommand( const char *command )
{
	if ( FStrEq( "join_match", command ) )
	{
		JoinMatch();
	}
	else if ( FStrEq( "abandon_match", command ) )
	{
		GTFGCClientSystem()->RejoinLobby( false );
	}
	else if ( FStrEq( "leave_queue", command ) )
	{
		// Only the leader can leave the queue
		if ( GTFGCClientSystem()->BIsPartyLeader() )
		{
			switch( GTFGCClientSystem()->GetSearchMode() )
			{
			case TF_Matchmaking_LADDER:
				GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_LADDER );
				break;

			case TF_Matchmaking_CASUAL:
				GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_CASUAL );
				break;

			default:
				// Unhandled
				Assert( false );
				break;
			};
		}
	}
	else if( FStrEq( "rematch", command ) )
	{
		if ( !GTFGCClientSystem()->BIsPartyLeader() )
			return;

		engine->ClientCmd( "rematch_vote 2" );
	}
	else if ( FStrEq( "new_match", command ) )
	{
		if ( !GTFGCClientSystem()->BIsPartyLeader() )
			return;

		GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_SEARCHING );
		engine->ClientCmd( "rematch_vote 1" );
	}
	else if ( FStrEq( "leave_party", command ) )
	{
		// Leave current party and create a new one!
		GTFGCClientSystem()->SendExitMatchmaking( false );
		return;
	}
}


void CTFMatchmakingPopup::FireGameEvent( IGameEvent *pEvent )
{
	if ( FStrEq( pEvent->GetName(), "rematch_failed_to_create" ) )
	{
		// If the GC failed to create our rematch, then go ahead and requeue
		if ( !GTFGCClientSystem()->BIsPartyLeader() )
			return;

		GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_SEARCHING );
	}
	else if ( FStrEq( pEvent->GetName(), "party_updated" ) )
	{
		if ( ShouldBeActve() )
		{
			Update();
		}
	}
}


#ifdef STAGING_ONLY
CON_COMMAND( reload_mm_popup, "Reload the mm popup panel. Pass any 2nd argument to recreate the panel." )
{
	bool bRecreate = false;
	if ( args.ArgC() == 2 )
	{
		bRecreate = true;
	}

	auto& vecPopups = CreateMMPopupPanels( bRecreate );

	if ( !bRecreate )
	{
		FOR_EACH_VEC( vecPopups, i )
		{
			vecPopups[ i ]->InvalidateLayout( true, true );
		}
	}
}

CON_COMMAND( reload_mm_dashboard, "Reload the mm join panel." )
{
	GetMMDashboard()->InvalidateLayout( true, true );
}
#endif


CTFMatchmakingDashboard::CTFMatchmakingDashboard()
	: CExpandablePanel( NULL, "MMDashboard" )
{
	SetKeyBoardInputEnabled( false );
	ivgui()->AddTickSignal( GetVPanel(), 100 );

	GetMMDashboardParentManager()->AddPanel( this );

	CreateMMPopupPanels();

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	SetProportional( true );
}

CTFMatchmakingDashboard::~CTFMatchmakingDashboard()
{
	GetMMDashboardParentManager()->RemovePanel( this );
}

void CTFMatchmakingDashboard::ApplySchemeSettings( vgui::IScheme *pScheme ) 
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetMouseInputEnabled( true );
	LoadControlSettings( "resource/UI/MatchMakingDashboard.res" );

	// This cannot ever be true or else things get weird when in-game
	SetKeyBoardInputEnabled( false );

	GetMMDashboardParentManager()->UpdateParenting();
}

void CTFMatchmakingDashboard::OnCommand( const char *command )
{
	if ( FStrEq( command, "disconnect" ) )
	{
		CTFParty* pParty = GTFGCClientSystem()->GetParty();
		bool bInPartyOfMany = pParty && pParty->GetNumMembers() > 1;

		const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( TFGameRules()->GetCurrentMatchGroup() );
		if ( pMatchDesc && pMatchDesc->BShouldAutomaticallyRequeueOnMatchEnd() && !bInPartyOfMany )
		{
			// If this is an auto-requeue match type, then assume hitting the "Disconnect" button
			// means you are done playing entirely with MM.  Close out to the main menu.
			bool bNoPenalty = ( GTFGCClientSystem()->GetAssignedMatchAbandonStatus() != k_EAbandonGameStatus_AbandonWithPenalty );
			if ( bNoPenalty )
			{
				GTFGCClientSystem()->EndMatchmaking( true );			
			}
			else
			{
				// Prompt if this would be considered an abandon with a penalty
				CTFDisconnectConfirmDialog *pDialog = BuildDisconnectConfirmDialog();
				if ( pDialog )
				{
					pDialog->Show();
				}
			}
		}
		else
		{
			// Go back to the MM screens.  The party leader will request the right wizard state
			switch( GTFGCClientSystem()->GetSearchMode() )
			{
			case TF_Matchmaking_LADDER:
				if ( GTFGCClientSystem()->BIsPartyLeader() )
				{
					GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_LADDER );
				}
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby ladder" );
				break;

			case TF_Matchmaking_CASUAL:
				if ( GTFGCClientSystem()->BIsPartyLeader() )
				{
					GTFGCClientSystem()->RequestSelectWizardStep( TF_Matchmaking_WizardStep_CASUAL );
				}
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby casual" );
				break;

			default:
				// Unhandled
				GTFGCClientSystem()->EndMatchmaking();
				Assert( false );
				break;
			};
		}

		// Disconnect from the server
		engine->DisconnectInternal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Figure out if we should be visible and require mouse input
//-----------------------------------------------------------------------------
void CTFMatchmakingDashboard::OnTick()
{
	bool bInEndOfMatch = TFGameRules() && TFGameRules()->State_Get() == GR_STATE_GAME_OVER;

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( TFGameRules() ? TFGameRules()->GetCurrentMatchGroup() : k_nMatchGroup_Invalid );
	bool bShouldBeVisible = GTFGCClientSystem()->BConnectedToMatchServer( false )
							&& bInEndOfMatch
							&& pMatchDesc
							&& pMatchDesc->BUsesDashboard();

#ifdef STAGING_ONLY
	bShouldBeVisible |= tf_mm_dashboard_force_show.GetBool();
#endif

	if ( BIsExpanded() && !bShouldBeVisible )
	{
		SetCollapsed( true );
	}
	else if ( !BIsExpanded() && bShouldBeVisible )
	{
		SetCollapsed( false );
	}

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( BIsExpanded() );
}

CUtlVector< IMMPopupFactory* > IMMPopupFactory::s_vecPopupFactories;

//-----------------------------------------------------------------------------
// Purpose: Snag the singleton CTFMatchmakingPopup
//-----------------------------------------------------------------------------
CUtlVector< CTFMatchmakingPopup* >& CreateMMPopupPanels( bool bRecreate /*= false*/ )
{
	static CUtlVector< CTFMatchmakingPopup* > s_vecPopups;

	if ( bRecreate && s_vecPopups.Count() )
	{
		FOR_EACH_VEC( s_vecPopups, i )
		{
			s_vecPopups[ i ]->MarkForDeletion();
		}
		s_vecPopups.Purge();
	}


	if ( s_vecPopups.IsEmpty() )
	{
		FOR_EACH_VEC( IMMPopupFactory::s_vecPopupFactories, i )
		{
			s_vecPopups.AddToTail( IMMPopupFactory::s_vecPopupFactories[i]->Create() );
		}
	}

	return s_vecPopups;
}


//-----------------------------------------------------------------------------
// Purpose: Snag the singleton CTFMatchmakingDashboard
//-----------------------------------------------------------------------------
CTFMatchmakingDashboard* GetMMDashboard()
{
	static CTFMatchmakingDashboard* s_pDashboardPanel = NULL;
	if ( !s_pDashboardPanel )
	{
		s_pDashboardPanel = new CTFMatchmakingDashboard();
	}

	return s_pDashboardPanel;
}
