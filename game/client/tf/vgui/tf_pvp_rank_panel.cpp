//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"

#include "tf_pvp_rank_panel.h"
#include "tf_matchmaking_shared.h"
#include "basemodel_panel.h"
#include "vgui_controls/ProgressBar.h"
#include "tf_ladder_data.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include "tf_controls.h"
#include "vgui/ISurface.h"
#include "animation.h"
#include "clientmode_tf.h"
#include "vgui/IInput.h"
#include "vgui_controls/MenuItem.h"
#include "tf_gc_client.h"
#include "tf_xp_source.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef STAGING_ONLY
	extern ConVar tf_test_pvp_rank_xp_change;
#endif

ConVar tf_xp_breakdown_interval( "tf_xp_breakdown_interval", "1.85", FCVAR_DEVELOPMENTONLY );
ConVar tf_xp_breakdown_lifetime( "tf_xp_breakdown_lifetime", "3.5", FCVAR_DEVELOPMENTONLY );

extern const char *s_pszMatchGroups[];

CPvPRankPanel::XPState_t::XPState_t( EMatchGroup eMatchGroup )
	: m_nStartXP( 0u )
	, m_nTargetXP( 0u )
	, m_nActualXP( 0u )
	, m_bCurrentDeltaViewed( true )
	, m_eMatchGroup( eMatchGroup )
{
	Assert( m_eMatchGroup != k_nMatchGroup_Invalid );

	// Default to level 1's starting XP value
	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eMatchGroup );
	if ( pMatchDesc && pMatchDesc->m_pProgressionDesc )
	{
		auto level = pMatchDesc->m_pProgressionDesc->GetLevelByNumber( 1 );
		m_nStartXP = level.m_nStartXP;
		m_nActualXP = m_nStartXP;
		m_nTargetXP = m_nStartXP;
	}

	ListenForGameEvent( "experience_changed" );
	ListenForGameEvent( "server_spawn" );
}

void CPvPRankPanel::XPState_t::FireGameEvent( IGameEvent *pEvent )
{
	if ( FStrEq( pEvent->GetName(), "experience_changed" ) ) // For changing tf_progression_set_xp_to_level
	{
		UpdateXP( false );
	}
	else if ( FStrEq( pEvent->GetName(), "server_spawn" ) && GTFGCClientSystem()->BHaveLiveMatch()
		&& GTFGCClientSystem()->GetLiveMatchGroup() == m_eMatchGroup )
	{
		UpdateXP( false );
		// Acknowledge any outstanding XP sources when we start a match.  It looks really weird when
		// you see old match xp sources at the end of a match.
		GTFGCClientSystem()->AcknowledgePendingXPSources( m_eMatchGroup );
	}
}

void CPvPRankPanel::XPState_t::UpdateXP( bool bInitial )
{
	uint32 nStartXP = 0u;
	uint32 nNewXP = 0u;

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eMatchGroup );
	// Should only be creating this object for matches that have progressions
	Assert( pMatchDesc && pMatchDesc->m_pProgressionDesc );
	if ( pMatchDesc && pMatchDesc->m_pProgressionDesc )
	{
		// Default to level 1's lowest XP value
		auto level = pMatchDesc->m_pProgressionDesc->GetLevelByNumber( 1 );
		nNewXP = level.m_nStartXP;
		nStartXP = level.m_nStartXP;

		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			nStartXP = pMatchDesc->m_pProgressionDesc->GetLocalPlayerLastAckdExperience();
			// Get the actual user's XP
			nNewXP = pMatchDesc->m_pProgressionDesc->GetPlayerExperienceBySteamID( steamapicontext->SteamUser()->GetSteamID() );
		}
	}

	// If there's no change, don't do anything
	if ( nNewXP != m_nActualXP || bInitial )
	{
		m_nActualXP = nNewXP;
		m_nStartXP = nStartXP;
		m_bCurrentDeltaViewed = false;
	}

	if ( bInitial )
	{
		m_progressTimer.Start( 1.f );
		m_bCurrentDeltaViewed = true;
		m_nTargetXP = m_nStartXP;
	}
}

uint32 CPvPRankPanel::XPState_t::GetCurrentXP() const
{
	float flTimeProgress = 0.f;
	if ( m_progressTimer.HasStarted() )
	{
		flTimeProgress = Clamp( m_progressTimer.GetElapsedTime(), 0.f, m_progressTimer.GetCountdownDuration() );
		flTimeProgress = Gain( flTimeProgress / m_progressTimer.GetCountdownDuration(), 0.9f );
	}

	return RemapValClamped( flTimeProgress, 0.f, 1.f, m_nStartXP, m_nTargetXP );
}

bool CPvPRankPanel::XPState_t::BeginXPDeltaLerp()
{
	if ( !m_bCurrentDeltaViewed )
	{
		m_bCurrentDeltaViewed = true;
		// Change our target
		m_nTargetXP = m_nActualXP;
		m_progressTimer.Start( 5.f );

		return true;
	}

	return false;
}

void CPvPRankPanel::XPState_t::SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) 
{
	if ( pObject->GetTypeID() == CSOTFLadderData::k_nTypeID )
	{
		CSOTFLadderData* pLadderObject = (CSOTFLadderData*)pObject;
		if( (EMatchGroup)pLadderObject->Obj().match_group() != m_eMatchGroup )
			return;

		// We'll get a eSOCacheEvent_Incremental when we're actually creating the 
		// first CSOTFLadderData object.  We dont want that to come through as
		// "initializing" because it'll skip the leveling effects
		UpdateXP( eEvent == eSOCacheEvent_ListenerAdded );
	}

	if ( pObject->GetTypeID() == CXPSource::k_nTypeID )
	{
		CXPSource *pXPSource = (CXPSource*)pObject;
		if ( pXPSource->Obj().match_group() == m_eMatchGroup )
		{
			UpdateXP( false );
		}
	}
}

void CPvPRankPanel::XPState_t::SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent )
{
	if ( pObject->GetTypeID() == CSOTFLadderData::k_nTypeID )
	{
		CSOTFLadderData* pLadderObject = (CSOTFLadderData*)pObject;
		if( (EMatchGroup)pLadderObject->Obj().match_group() != m_eMatchGroup )
			return;

		// If the GC comes on after we've already subscribed to the cache,
		// eSOCacheEvent_Subscribed when the object is created.  This one
		// we want to skip the effects.
		UpdateXP( eEvent == eSOCacheEvent_Subscribed );
	}

	if ( pObject->GetTypeID() == CXPSource::k_nTypeID )
	{
		CXPSource *pXPSource = (CXPSource*)pObject;
		if ( pXPSource->Obj().match_group() == m_eMatchGroup )
		{
			UpdateXP( false );
		}
	}
}

class CMiniPvPRankPanel : public CPvPRankPanel
{
public:
	CMiniPvPRankPanel( Panel* pParent, const char* pszPanelName ) : CPvPRankPanel( pParent, pszPanelName ) 
	{} 

private:
	virtual KeyValues* GetConditions() const OVERRIDE
	{
		KeyValues* pConditions = new KeyValues( "conditions" );
		pConditions->AddSubKey( new KeyValues( "if_mini" ) );

		return pConditions;
	}
};

class CXPSourcePanel : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CXPSourcePanel , vgui::EditablePanel);
	CXPSourcePanel( Panel *pParent, const char* panelName, const CMsgTFXPSource& source )
		: BaseClass( pParent, panelName )
		, m_source( source )
	{}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE
	{
		BaseClass::ApplySchemeSettings( pScheme );

		LoadControlSettings( "resource/ui/XPSourcePanel.res" );
	}

	virtual void PerformLayout() OVERRIDE
	{
		BaseClass::PerformLayout();

		static wchar_t wszOutString[ 128 ];
		wchar_t wszCount[ 16 ];
		_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", m_source.amount() );
		const wchar_t *wpszFormat = g_pVGuiLocalize->Find( g_XPSourceDefs[ m_source.type() ].m_pszFormattingLocToken );
		g_pVGuiLocalize->ConstructString_safe( wszOutString, wpszFormat, 2, g_pVGuiLocalize->Find( g_XPSourceDefs[ m_source.type() ].m_pszTypeLocToken ), wszCount );

		SetDialogVariable( "source", wszOutString );
	}

protected:
	CMsgTFXPSource m_source;
};

class CScrollingXPSourcePanel : public CXPSourcePanel
{
public:
	DECLARE_CLASS_SIMPLE( CScrollingXPSourcePanel , CXPSourcePanel );
	CScrollingXPSourcePanel( Panel *pParent, const char* panelName, const CMsgTFXPSource& source, float flStartTime )
		: BaseClass( pParent, panelName, source )
		, m_flStartTime( flStartTime )
		, m_bStarted( false )
	{
		vgui::ivgui()->AddTickSignal( GetVPanel() );

		SetAutoDelete( false );
	}

	virtual ~CScrollingXPSourcePanel()
	{
	}

	virtual void OnTick() OVERRIDE
	{
		BaseClass::OnTick();

		if ( Plat_FloatTime() > m_flStartTime && !m_bStarted )
		{
			m_bStarted = true;
			SetVisible( true );

			// Do starting stuff
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, m_source.amount() >= 0 ? "XPSourceShow_Positive" : "XPSourceShow_Negative", false );

			if ( g_XPSourceDefs[ m_source.type() ].m_pszSoundName )
			{
				PlaySoundEntry( g_XPSourceDefs[ m_source.type() ].m_pszSoundName );
			}
		}

		SetVisible( m_bStarted );

		if ( Plat_FloatTime() > ( m_flStartTime + tf_xp_breakdown_lifetime.GetFloat() ) )
		{
			// We're done!  Delete ourselves
			MarkForDeletion();
		}
	}

private:

	float m_flStartTime;
	bool m_bStarted;
};

DECLARE_BUILD_FACTORY( CMiniPvPRankPanel );
DECLARE_BUILD_FACTORY( CPvPRankPanel );

CPvPRankPanel::CPvPRankPanel( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
	, m_eMatchGroup( k_nMatchGroup_Invalid )
	, m_pProgressionDesc( NULL )
	, m_pContinuousProgressBar( NULL )
	, m_pModelPanel( NULL )
	, m_pXPBar( NULL )
	, m_pBGPanel( NULL )
	, m_nLastLerpXP( 0 )
	, m_nLastSeenLevel( 0 )
	, m_bClicked( false )
{
	ListenForGameEvent( "begin_xp_lerp" );
}

void CPvPRankPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues* pConditions = GetConditions();

	LoadControlSettings( GetResFile(), NULL, NULL, pConditions );

	if ( pConditions )
	{
		pConditions->deleteThis();
		pConditions = NULL;
	}

	m_pBGPanel = FindControl< EditablePanel >( "BGPanel", true );
	m_pContinuousProgressBar = FindControl< ContinuousProgressBar >( "ContinuousProgressBar", true );
	m_pXPBar = FindControl< EditablePanel >( "XPBar", true );
	m_pModelPanel = FindControl< CBaseModelPanel >( "RankModel", true );
	m_pModelPanel->AddActionSignalTarget( this );

	m_pModelButton = FindChildByName( "MedalButton", true );

#ifdef STAGING_ONLY
	if ( m_pBGPanel )
	{
		Panel* pDebugButton = m_pBGPanel->FindChildByName( "TestLevelDownButton", true );
		if ( pDebugButton ) { pDebugButton->SetVisible( true ); }
		pDebugButton = m_pBGPanel->FindChildByName( "LevelDebugButton", true );
		if ( pDebugButton ) { pDebugButton->SetVisible( true ); }
		pDebugButton = m_pBGPanel->FindChildByName( "TestLevelUpButton", true );
		if ( pDebugButton ) { pDebugButton->SetVisible( true ); }
	}
#endif
}

void CPvPRankPanel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings( inResourceData );

	SetMatchGroup( (EMatchGroup)StringFieldToInt( inResourceData->GetString( "matchgroup" ), s_pszMatchGroups, (int)k_nMatchGroup_Count, false ) );
}

void CPvPRankPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( !m_pProgressionDesc )
		return;

	EditablePanel* pStatsContainer = FindControl< EditablePanel >( "Stats", true );
	if ( !pStatsContainer )
		return;

	if ( !m_pModelPanel )
		return;

	if ( !m_pXPBar )
		return;

	ProgressBar* pProgressBar = FindControl< ProgressBar > ( "ProgressBar", true );
	if ( !pProgressBar )
		return;

	if ( !m_pContinuousProgressBar )
		return;

	// Chop up the progress bar into 10th's and use it as a mask overtop
	pProgressBar->SetSegmentInfo( ( pProgressBar->GetWide() / 10 ) - ( ( 9 * 4 ) / 10 ) , 4 );

	const XPState_t& xpstate = GetXPState();
	uint32 nCurrentXP = xpstate.GetCurrentXP();
	const LevelInfo_t& levelCur = m_pProgressionDesc->GetLevelForExperience( nCurrentXP );
	m_pProgressionDesc->SetupBadgePanel( m_pModelPanel, levelCur );

	// Update labels
	UpdateControls( xpstate.GetStartXP(), nCurrentXP, levelCur );

	SetMatchStats();
}

void CPvPRankPanel::OnThink()
{
	 BaseClass::OnThink();

	 if ( m_pContinuousProgressBar && m_pXPBar && m_pModelPanel && m_pProgressionDesc && m_pBGPanel )
	 {

		// SUPER HACKS.  I dont have time to figure out popups
		if ( m_pModelButton && vgui::input()->IsMouseDown( MOUSE_LEFT ) )
		{
			int nMouseX, nMouseY; 
			input()->GetCursorPos( nMouseX, nMouseY );
			if ( !m_bClicked && m_pModelButton->IsWithin( nMouseX, nMouseY ) )
			{
				OnCommand( "medal_clicked" );
			}

			m_bClicked = true;
		}
		else
		{
			m_bClicked = false;
		}

		const XPState_t& xpstate = GetXPState();
		uint32 nCurrentXP = xpstate.GetCurrentXP();

		// Check if the last XP we lerp'd to isn't the current XP.  If it's not, then we want to animate over to it.
		if ( m_nLastLerpXP != nCurrentXP )
		{
			uint32 nPrevXP = xpstate.GetStartXP();
			const LevelInfo_t& levelCur = m_pProgressionDesc->GetLevelForExperience( nCurrentXP );

			UpdateControls( nPrevXP, nCurrentXP, levelCur );

			// We only want to do level up effects if we are thinking (visible) when the current XP passes the level boundary
			if ( m_nLastLerpXP != xpstate.GetStartXP() )
			{
				const LevelInfo_t& levelPrevThink = m_pProgressionDesc->GetLevelForExperience( m_nLastLerpXP );
				if ( levelCur.m_nLevelNum > levelPrevThink.m_nLevelNum ) // Level up :)
				{
					PlayLevelUpEffects( levelCur );
				}
				else if ( levelCur.m_nLevelNum < levelPrevThink.m_nLevelNum ) // Level down :(
				{
					PlayLevelDownEffects( levelCur );
				}
			}

			m_nLastLerpXP = nCurrentXP;
		}
		else 
		{
			// Our last lerp XP is caught up with current XP, but our last seen level is not up to date with what
			// our current level actually is.  This can happen if we haven't been thinking, but we did level up somewhere
			// else.  In this case, we need to update our badge.
			const LevelInfo_t& levelCur = m_pProgressionDesc->GetLevelForExperience( nCurrentXP );

			if ( m_nLastSeenLevel != levelCur.m_nLevelNum )
			{
				m_nLastSeenLevel = levelCur.m_nLevelNum;
				m_pProgressionDesc->SetupBadgePanel( m_pModelPanel, levelCur );
			}
		}
	 }
}


void CPvPRankPanel::UpdateControls( uint32 nPreviousXP, uint32 nCurrentXP, const LevelInfo_t& levelCurrent )
{
	if ( m_pContinuousProgressBar )
	{
		// Calculate progress bar percentages.  PrevBarProgress is the bar that shows where you started.
		// CurrentBarProgress is the bar that lerps from the start to your actual, current XP
		float flPrevBarProgress		= RemapValClamped( (float)nPreviousXP,	  (float)levelCurrent.m_nStartXP, (float)levelCurrent.m_nEndXP, 0.f, 1.f );
		float flCurrentBarProgress	= RemapValClamped( (float)nCurrentXP, (float)levelCurrent.m_nStartXP, (float)levelCurrent.m_nEndXP, 0.f, 1.f );

		m_pContinuousProgressBar->SetPrevProgress( flPrevBarProgress );
		m_pContinuousProgressBar->SetProgress( flCurrentBarProgress );
	}

	if ( m_pXPBar )
	{
		m_pXPBar->SetDialogVariable( "current_xp", LocalizeNumberWithToken( "TF_Competitive_XP_Current", nCurrentXP ) );

		if ( levelCurrent.m_nLevelNum < m_pProgressionDesc->GetNumLevels() )
		{
			m_pXPBar->SetDialogVariable( "next_level_xp", LocalizeNumberWithToken( "TF_Competitive_XP", levelCurrent.m_nEndXP ) );
		}
		else // Hide the next level XP value at max level
		{
			m_pXPBar->SetDialogVariable( "next_level_xp", "" );
		}

		static wchar_t wszOutString[ 128 ];
		wchar_t wszCount[ 16 ];
		_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", levelCurrent.m_nLevelNum );
		const wchar_t *wpszFormat = g_pVGuiLocalize->Find( m_pProgressionDesc->m_pszLevelToken );
		g_pVGuiLocalize->ConstructString_safe( wszOutString, wpszFormat, 2, wszCount, g_pVGuiLocalize->Find( levelCurrent.m_pszLevelTitle ) );

		m_pXPBar->SetDialogVariable( "level", wszOutString );
	}
}

void CPvPRankPanel::OnCommand( const char *command )
{
	if ( FStrEq( "medal_clicked", command ) )
	{
		// Default effects
		const char *pszSeqName = "click_A";
		const char *pszSoundName = "ui/mm_medal_click.wav";

		// Roll for a crit
		int nRandomRoll = RandomInt( 0, 9 );
		if ( nRandomRoll == 0 )
		{
			// CRIT!
			pszSeqName = "click_B";
			pszSoundName = "MatchMaking.MedalClickRare";
		}
	
		m_pModelPanel->PlaySequence( pszSeqName );
		PlaySoundEntry( pszSoundName );

		EditablePanel* pModelContainer = FindControl< EditablePanel >( "ModelContainer" );
		if ( pModelContainer )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pModelContainer, "PvPRankModelClicked", false);
		}

		return;
	}
	else if ( FStrEq( "begin_xp_lerp", command ) )
	{
		BeginXPLerp();
		return;
	}
	else if ( FStrEq( "update_base_state", command ) )
	{
		UpdateBaseState();
		return;
	}
	BaseClass::OnCommand( command );
}

void CPvPRankPanel::FireGameEvent( IGameEvent *pEvent )
{
	// This is really only for tf_test_pvp_rank_xp_change
	if ( FStrEq( pEvent->GetName(), "begin_xp_lerp" ) )
	{
		BeginXPLerp();
	}
}

void CPvPRankPanel::SetVisible( bool bVisible )
{
	 if ( IsVisible() != bVisible )
	 {
		 UpdateBaseState();
	 }

	 BaseClass::SetVisible( bVisible );
}

int SortXPSources( CXPSource* const* pLeft, CXPSource* const* pRight )
{
	return (*pLeft)->Obj().type() - (*pRight)->Obj().type();
}

void CPvPRankPanel::BeginXPLerp()
{
	XPState_t& xpstate = GetXPState();
	if ( xpstate.BeginXPDeltaLerp() )
	{
		// Play sounds if this is a change
		if ( xpstate.GetTargetXP() > xpstate.GetStartXP() )
		{
			PlaySoundEntry( "MatchMaking.RankProgressTickUp" );
		}
		else if ( xpstate.GetTargetXP() < xpstate.GetStartXP() )
		{
			PlaySoundEntry( "MatchMaking.RankProgressTickDown" );
		}
	}

	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		GCSDK::CGCClientSharedObjectCache *pSOCache = GCClientSystem()->GetSOCache( steamapicontext->SteamUser()->GetSteamID() );

		if ( pSOCache )
		{
			GCSDK::CGCClientSharedObjectTypeCache *pTypeCache = pSOCache->FindTypeCache( CXPSource::k_nTypeID );

			if ( pTypeCache )
			{
				CUtlVector< CXPSource* > vecSources;

				// Grab all the XP sources we want to show
				for ( uint32 i = 0; i < pTypeCache->GetCount(); ++i )
				{
					CXPSource *pXPSource = (CXPSource*)pTypeCache->GetObject( i );

					if ( pXPSource->Obj().match_group() == m_eMatchGroup )
					{
						vecSources.AddToTail( pXPSource );
					}
				}
					
				// Sort them so users get a consistent experience
				vecSources.Sort( &SortXPSources );
					
				// Show the sources
				FOR_EACH_VEC( vecSources, i )
				{
					CXPSource *pXPSource = vecSources[ i ];
					CScrollingXPSourcePanel* pSourcePanel = new CScrollingXPSourcePanel( this, "XPSourcePanel", pXPSource->Obj(), Plat_FloatTime() + ( i * tf_xp_breakdown_interval.GetFloat() ) );
					pSourcePanel->MakeReadyForUse();

					// Offset from the BGPanel.
					pSourcePanel->SetPos( m_pBGPanel->GetXPos() + m_iXPSourceNotificationCenterX - ( pSourcePanel->GetWide() * 0.5f ), m_pBGPanel->GetYPos() + m_pXPBar->GetYPos() + YRES( 22 ) - pSourcePanel->GetTall() );
				}
			}
		}
	}

	GTFGCClientSystem()->AcknowledgePendingXPSources( m_eMatchGroup );
}

void CPvPRankPanel::UpdateBaseState()
{
	if ( !m_pProgressionDesc || m_pModelPanel == NULL )
		return;

	// Set our "last seen" variables so things dont try to interpolate
	m_nLastLerpXP = GetXPState().GetCurrentXP();
	LevelInfo_t levelCur = m_pProgressionDesc->GetLevelForExperience( m_nLastLerpXP );
	m_nLastSeenLevel = levelCur.m_nLevelNum;

	m_pProgressionDesc->SetupBadgePanel( m_pModelPanel, levelCur );

	// Update progress bars and labels
	UpdateControls( GetXPState().GetStartXP(), m_nLastLerpXP, levelCur );
}

void CPvPRankPanel::OnAnimEvent( KeyValues *pParams )
{
	// This gets fired by the model panel that has the badge model in it when we want
	// to do our bodygroup changes.  This is so we can time it to when the model is doing
	// a flashy maneuver to mask the bodygroup change pop
	if ( FStrEq( pParams->GetString( "name" ), "AE_CL_BODYGROUP_SET_VALUE" ) && m_pProgressionDesc )
	{
		const LevelInfo_t& levelCur = m_pProgressionDesc->GetLevelForExperience( m_nLastLerpXP );
		m_pProgressionDesc->SetupBadgePanel( m_pModelPanel, levelCur );
	}
}

void CPvPRankPanel::PlayLevelUpEffects( const LevelInfo_t& level ) const
{
	m_pModelPanel->PlaySequence( "level_up" );
	PlaySoundEntry( level.m_pszLevelUpSound );
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pXPBar, "PvPRankLevelUpXPBar", false);
	
	EditablePanel* pModelContainer = const_cast< CPvPRankPanel* >( this )->FindControl< EditablePanel >( "ModelContainer" );
	if ( pModelContainer )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pModelContainer, "PvPRankLevelUpModel", false );
	}
}

void CPvPRankPanel::PlayLevelDownEffects( const LevelInfo_t& level ) const
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pXPBar, "PvPRankLevelDownXPBar", false);
	m_pModelPanel->PlaySequence( "level_down" );

	EditablePanel* pModelContainer = const_cast< CPvPRankPanel* >( this )->FindControl< EditablePanel >( "ModelContainer" );
	if ( pModelContainer )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pModelContainer, "PvPRankLevelDownModel", false );
	}
}

void CPvPRankPanel::SetMatchGroup( EMatchGroup eMatchGroup )
{
	if ( m_eMatchGroup != eMatchGroup )
	{
		m_eMatchGroup = eMatchGroup;
		m_pProgressionDesc = NULL;

		const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eMatchGroup );
		if ( pMatchDesc && pMatchDesc->m_pProgressionDesc )
		{
			// Snag the progression desc.  We use it often
			m_pProgressionDesc = pMatchDesc->m_pProgressionDesc;
			UpdateBaseState();
		}

		Assert( m_pProgressionDesc );

		// Many things needs to change
		InvalidateLayout( true, true );
		UpdateBaseState();
	}
}

void CPvPRankPanel::SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent )
{
	if ( pObject->GetTypeID() != CSOTFLadderData::k_nTypeID )
		return;

	SetMatchStats();
}

void CPvPRankPanel::SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent )
{
	if ( pObject->GetTypeID() != CSOTFLadderData::k_nTypeID )
		return;

	SetMatchStats();
}

void CPvPRankPanel::SetMatchStats( void )
{
	EditablePanel* pStatsContainer = FindControl< EditablePanel >( "Stats", true );
	if ( !pStatsContainer )
		return;

	CSOTFLadderData *pData = GetLocalPlayerLadderData( m_eMatchGroup );

	// Update all stats.  Default to 0 incase we dont have ladder data
	int nGames = 0, nKills = 0, nDeaths = 0, nDamage = 0, nHealing = 0, nSupport = 0, nScore = 0;

	if ( pData )
	{
		nGames = pData->Obj().games();
		nKills = pData->Obj().kills();
		nDeaths = pData->Obj().deaths();
		nDamage = pData->Obj().damage();
		nHealing = pData->Obj().healing();
		nSupport = pData->Obj().support();
		nScore = pData->Obj().score();
	}

	pStatsContainer->SetDialogVariable( "stat_games", LocalizeNumberWithToken( "TF_Competitive_Games", nGames ) );
	pStatsContainer->SetDialogVariable( "stat_kills", LocalizeNumberWithToken( "TF_Competitive_Kills", nKills ) );
	pStatsContainer->SetDialogVariable( "stat_deaths", LocalizeNumberWithToken( "TF_Competitive_Deaths", nDeaths ) );
	pStatsContainer->SetDialogVariable( "stat_damage", LocalizeNumberWithToken( "TF_Competitive_Damage", nDamage ) );
	pStatsContainer->SetDialogVariable( "stat_healing", LocalizeNumberWithToken( "TF_Competitive_Healing", nHealing ) );
	pStatsContainer->SetDialogVariable( "stat_support", LocalizeNumberWithToken( "TF_Competitive_Support", nSupport ) );
	pStatsContainer->SetDialogVariable( "stat_score", LocalizeNumberWithToken( "TF_Competitive_Score", nScore ) );
}

CPvPRankPanel::XPState_t& CPvPRankPanel::GetXPState() const
{
	// Singletons for each XPState_t
	static CUtlMap< EMatchGroup, CPvPRankPanel::XPState_t* > s_mapXPStates( DefLessFunc( EMatchGroup ) );

	auto idx = s_mapXPStates.Find( m_eMatchGroup );
	if ( idx == s_mapXPStates.InvalidIndex() )
	{
		idx = s_mapXPStates.Insert( m_eMatchGroup, new CPvPRankPanel::XPState_t( m_eMatchGroup ) );
	}

	return *s_mapXPStates[ idx ];
}

const char* CPvPRankPanel::GetResFile() const
{
	return m_pProgressionDesc ? m_pProgressionDesc->m_pszProgressionResFile : "resource/ui/PvPCompRankPanel.res";
}

KeyValues* CPvPRankPanel::GetConditions() const
{
	return NULL;
}
