//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "quest_notification_panel.h"
#include "vgui/ISurface.h"
#include "ienginevgui.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "basemodel_panel.h"
#include "tf_item_inventory.h"
#include "quest_log_panel.h"
#include "econ_controls.h"
#include "c_tf_player.h"
#include <vgui_controls/AnimationController.h>
#include "engine/IEngineSound.h"
#include "econ_item_system.h"
#include "tf_hud_item_progress_tracker.h"
#include "tf_spectatorgui.h"
#include "econ_quests.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar tf_quest_notification_line_delay( "tf_quest_notification_line_delay", "1.2", FCVAR_ARCHIVE );

extern ISoundEmitterSystemBase *soundemitterbase;
CQuestNotificationPanel *g_pQuestNotificationPanel = NULL;

DECLARE_HUDELEMENT( CQuestNotificationPanel );

CQuestNotification::CQuestNotification( CEconItem *pItem )
	: m_hItem( pItem )
{}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CQuestNotification::Present( CQuestNotificationPanel* pNotificationPanel )
{
	m_timerDialog.Start( tf_quest_notification_line_delay.GetFloat() );

	return 0.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuestNotification_Speaking::CQuestNotification_Speaking( CEconItem *pItem )
	: CQuestNotification( pItem )
{
	m_pszSoundToSpeak = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CQuestNotification_Speaking::Present( CQuestNotificationPanel* pNotificationPanel )
{
	CQuestNotification::Present( pNotificationPanel );

	if ( m_hItem )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( !pPlayer )
			return 0.f;

		CTFPlayer* pTFPlayer = ToTFPlayer( pPlayer );
		if ( !pTFPlayer )
			return 0.f;

		const GameItemDefinition_t *pItemDef = m_hItem->GetItemDefinition();
		// Get our quest theme
		const CQuestThemeDefinition *pTheme = pItemDef->GetQuestDef()->GetQuestTheme();
		if ( pTheme )
		{
			// Get the sound we need to speak
			m_pszSoundToSpeak = GetSoundEntry( pTheme, pTFPlayer->GetPlayerClass()->GetClassIndex() );
			float flPresentTime = 0.f;
			if ( m_pszSoundToSpeak )
			{
				flPresentTime = enginesound->GetSoundDuration( m_pszSoundToSpeak ) + m_timerDialog.GetCountdownDuration() + 1.f;
				m_timerShow.Start( enginesound->GetSoundDuration( m_pszSoundToSpeak ) + m_timerDialog.GetCountdownDuration() + 1.f );
			}

			return flPresentTime;
		}
	}

	return 0.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotification_Speaking::Update( CQuestNotificationPanel* pNotificationPanel )
{
	if ( m_timerDialog.IsElapsed() && m_timerDialog.HasStarted() && m_hItem )
	{
		m_timerDialog.Invalidate();

		// Play it!
		if ( m_pszSoundToSpeak )
		{
			vgui::surface()->PlaySound( m_pszSoundToSpeak );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestNotification_Speaking::IsDone() const
{
	return m_timerShow.IsElapsed() && m_timerShow.HasStarted();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQuestNotification_NewQuest::GetSoundEntry( const CQuestThemeDefinition* pTheme, int nClassIndex )
{
	return pTheme->GetGiveSoundForClass( nClassIndex );
}

bool CQuestNotification_NewQuest::ShouldPresent() const
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	CTFPlayer* pTFPlayer = ToTFPlayer( pPlayer );
	if ( !pTFPlayer )
		return false;

	IViewPortPanel* pSpecGuiPanel = gViewPortInterface->FindPanelByName( PANEL_SPECGUI );
	if ( !pTFPlayer->IsAlive() )
	{
		if ( !pSpecGuiPanel || !pSpecGuiPanel->IsVisible() )
			return false;
	}
	else
	{
		// Local player is in a spawn room
		if ( pTFPlayer->m_Shared.GetRespawnTouchCount() <= 0 )
			return false;
	}

	return true;
}

CQuestNotification_CompletedQuest::CQuestNotification_CompletedQuest( CEconItem *pItem )
	: CQuestNotification_Speaking( pItem )
{
	const char *pszSoundName = UTIL_GetRandomSoundFromEntry( "Quest.StatusTickComplete" );
	m_PresentTimer.Start( enginesound->GetSoundDuration( pszSoundName ) - 2.f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQuestNotification_CompletedQuest::GetSoundEntry( const CQuestThemeDefinition* pTheme, int nClassIndex )
{
	return pTheme->GetCompleteSoundForClass( nClassIndex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestNotification_CompletedQuest::ShouldPresent() const
{
	return m_PresentTimer.IsElapsed() && m_PresentTimer.HasStarted();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQuestNotification_FullyCompletedQuest::GetSoundEntry( const CQuestThemeDefinition* pTheme, int nClassIndex )
{
	return pTheme->GetFullyCompleteSoundForClass( nClassIndex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuestNotificationPanel::CQuestNotificationPanel( const char *pszElementName )
	: CHudElement( pszElementName )
	, EditablePanel( NULL, "QuestNotificationPanel" )
	, m_flTimeSinceLastShown( 0.f )
	, m_bIsPresenting( false )
	, m_mapNotifiedItemIDs( DefLessFunc( itemid_t ) )
	, m_bInitialized( false )
	, m_pMainContainer( NULL )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	g_pQuestNotificationPanel = this;

	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "inventory_updated" );
	ListenForGameEvent( "player_initial_spawn" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuestNotificationPanel::~CQuestNotificationPanel()
{}




void CQuestNotificationPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Default, load pauling
	LoadControlSettings( "Resource/UI/econ/QuestNotificationPanel_Pauling_standard.res" );

	m_pMainContainer = FindControl< EditablePanel >( "MainContainer", true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	CExLabel* pNewQuestLabel = FindControl< CExLabel >( "NewQuestText", true );
	if ( pNewQuestLabel )
	{
		const wchar_t *pszText = NULL;
		const char *pszTextKey = "#QuestNotification_Accept";
		if ( pszTextKey )
		{
			pszText = g_pVGuiLocalize->Find( pszTextKey );
		}
		if ( pszText )
		{					
			wchar_t wzFinal[512] = L"";
			UTIL_ReplaceKeyBindings( pszText, 0, wzFinal, sizeof( wzFinal ) );
			pNewQuestLabel->SetText( wzFinal );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::FireGameEvent( IGameEvent * event )
{
	const char *pszName = event->GetName();

	if ( FStrEq( pszName, "inventory_updated" ) || FStrEq( pszName, "player_death" ) )
	{
		CheckForNotificationOpportunities();
	}
	else if ( FStrEq( pszName, "player_initial_spawn" ) )
	{
		CTFPlayer *pNewPlayer = ToTFPlayer( UTIL_PlayerByIndex( event->GetInt( "index" ) ) );
		if ( pNewPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			// Reset every round
			m_mapNotifiedItemIDs.Purge();
			m_vecNotifications.PurgeAndDeleteElements();
			m_timerNotificationCooldown.Start( 0 );
			m_bInitialized = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::Reset()
{
	CheckForNotificationOpportunities();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::CheckForNotificationOpportunities()
{
	// Suppress making new notifications while in competitive play
	if ( TFGameRules() && TFGameRules()->IsCompetitiveMode() )
		return;

	FOR_EACH_VEC_BACK( m_vecNotifications, i )
	{
		// Clean up old entires for items that are now gone
		if ( m_vecNotifications[i]->GetItemHandle() == NULL )
		{
			delete m_vecNotifications[i];
			m_vecNotifications.Remove( i );
		}
	}

	CPlayerInventory *pInv = InventoryManager()->GetLocalInventory();
	Assert( pInv );
	if ( pInv )
	{
		for ( int i = 0 ; i < pInv->GetItemCount(); ++i )
		{
			CEconItemView *pItem = pInv->GetItem( i );

			// Check if this is a quest at all
			if ( pItem->GetItemDefinition()->GetQuestDef() == NULL ) 
				continue;

			CQuestNotification* pNotification = NULL;
			if ( IsUnacknowledged( pItem->GetInventoryPosition() ) )
			{
				pNotification = new CQuestNotification_NewQuest( pItem->GetSOCData() );
			}
			else if ( IsQuestItemFullyCompleted( pItem ) ) // Fully completed
			{
				pNotification = new CQuestNotification_FullyCompletedQuest( pItem->GetSOCData() );
			}
			else if ( IsQuestItemReadyToTurnIn( pItem ) ) // Ready to turn in
			{
				pNotification = new CQuestNotification_CompletedQuest( pItem->GetSOCData() );
			}
			else
			{
				// Clean up any pending notifications for normal quests
				FOR_EACH_VEC_BACK( m_vecNotifications, j )
				{
					if ( m_vecNotifications[j]->GetItemHandle() == pItem->GetSOCData() )
					{
						delete m_vecNotifications[j];
						m_vecNotifications.Remove( j );
					}
				}
			}

			if ( pNotification && !AddNotificationForItem( pItem, pNotification ) )
			{
				delete pNotification;
				pNotification = NULL;
			}
		}

		m_bInitialized = pInv->GetOwner().IsValid();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestNotificationPanel::AddNotificationForItem( const CEconItemView *pItem, CQuestNotification* pNotification )
{
	bool bTypeAlreadyInQueue = false;
	// Check if there's already a notification of this type
	FOR_EACH_VEC_BACK( m_vecNotifications, i )
	{
		// There's already a quest of this type in queue, no need to add another
		if ( m_vecNotifications[i]->GetType() == pNotification->GetType() )
		{
			bTypeAlreadyInQueue = true;
			break;
		}
	}

	// Find the notified bits
	auto idx = m_mapNotifiedItemIDs.Find( pItem->GetItemID() );
	if ( idx == m_mapNotifiedItemIDs.InvalidIndex() )
	{
		// Create if missing
		idx = m_mapNotifiedItemIDs.Insert( pItem->GetItemID() );
		m_mapNotifiedItemIDs[ idx ].SetSize( CQuestNotification::NUM_NOTIFICATION_TYPES );
		FOR_EACH_VEC( m_mapNotifiedItemIDs[ idx ], i )
		{
			m_mapNotifiedItemIDs[ idx ][ i ] = 0.f;
		}
	}

	// Check if we've already done a notification for this type recently
	if ( Plat_FloatTime() < m_mapNotifiedItemIDs[ idx ][ pNotification->GetType() ] || m_mapNotifiedItemIDs[ idx ][ pNotification->GetType() ] == NEVER_REPEAT )
	{
		return false;
	}

	bool bNotificationUsed = false;
	// Don't play completed notifications unless they happen mid-play
	if ( !bTypeAlreadyInQueue && ( m_bInitialized || pNotification->GetType() == CQuestNotification::NOTIFICATION_TYPE_NEW_QUEST ) )
	{
		// Add notification
		m_vecNotifications.AddToTail( pNotification );
		bNotificationUsed = true;
	}

	// Mark that we've created a notification of this type for this item
	m_mapNotifiedItemIDs[ idx ][ pNotification->GetType() ] = pNotification->GetReplayTime() == NEVER_REPEAT ? NEVER_REPEAT : Plat_FloatTime() + pNotification->GetReplayTime();

	return bNotificationUsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestNotificationPanel::ShouldDraw()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	CTFPlayer* pTFPlayer = ToTFPlayer( pPlayer );
	if ( !pTFPlayer )
		return false;

	// Not selected a class, so they haven't joined in
	if ( pTFPlayer->IsPlayerClass( 0 ) )
		return false;

	if ( !CHudElement::ShouldDraw() )
		return false;

	if ( TFGameRules() && TFGameRules()->IsCompetitiveMode() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::OnThink()
{
	if ( !ShouldDraw() )
		return;

	bool bHasStarted = m_animTimer.HasStarted();
	float flShowProgress = bHasStarted ? 1.f : 0.f;
	const float flTransitionTime = 0.5f;

	Update();
	
	if ( bHasStarted )
	{
		// Transitions
		if ( m_animTimer.GetElapsedTime() < flTransitionTime )
		{
			flShowProgress = Bias( m_animTimer.GetElapsedTime() / flTransitionTime, 0.75f );
		}
		else if ( ( m_animTimer.GetRemainingTime() + 1.f ) < flTransitionTime )
		{
			flShowProgress = Bias( Max( 0.0f, m_animTimer.GetRemainingTime() + 1.f ) / flTransitionTime, 0.25f );
		}
	}
	
	// Move the main container around
	if ( m_pMainContainer )
	{
		int nY = g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() ? g_pSpectatorGUI->GetTopBarHeight() : 0;

		float flXPos = RemapValClamped( flShowProgress, 0.f, 1.f, 0.f, m_pMainContainer->GetWide() + XRES( 4 ) );
		m_pMainContainer->SetPos( GetWide() - (int)flXPos, nY );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQuestNotificationPanel::ShouldPresent()
{
	if ( !m_timerNotificationCooldown.IsElapsed() )
		return false;

	// We need notifications!
	if ( m_vecNotifications.IsEmpty() )
		return false;

	// It's been a few seconds since we were last shown
	if ( ( Plat_FloatTime() - m_flTimeSinceLastShown ) < 1.5f )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuestNotificationPanel::Update()
{
	bool bAllowedToShow = ShouldPresent();

	if ( bAllowedToShow && !m_bIsPresenting )
	{
		if ( m_vecNotifications.Head()->ShouldPresent() )
		{
			float flPresentTime = m_vecNotifications.Head()->Present( this );
			m_animTimer.Start( flPresentTime );

			m_timerHoldUp.Start( 3.f );

			// Notification sound
			vgui::surface()->PlaySound( "ui/quest_alert.wav" );
			m_bIsPresenting = true;
		}
	}
	else if ( !bAllowedToShow && m_bIsPresenting && m_timerHoldUp.IsElapsed() )
	{
		m_flTimeSinceLastShown = Plat_FloatTime();
		// Play the slide-out animation
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "QuestNotification_Hide" );
		m_bIsPresenting = false;
	}
	else if ( m_bIsPresenting ) // We are presenting a notification
	{
		if ( m_vecNotifications.Count() )
		{
			m_vecNotifications.Head()->Update( this );
			// Check if the notification is done
			if ( m_vecNotifications.Head()->IsDone() )
			{
				// Start our cooldown
				m_timerNotificationCooldown.Start( 1.f );
				// We're done with this notification
				delete m_vecNotifications.Head();
				m_vecNotifications.Remove( 0 );
			}
		}
	}
}
