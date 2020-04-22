//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"
#include "tf_matchmaking_dashboard.h"
#include "tf_gc_client.h"
#include "tf_gamerules.h"

using namespace vgui;
using namespace GCSDK;

#ifdef STAGING_ONLY 
extern ConVar tf_mm_popup_state_override;
#endif

class CNewMatchFoundDashboardState : public CTFMatchmakingPopup
{
public:
	CNewMatchFoundDashboardState( const char* pszName, const char* pszResFile )
		: CTFMatchmakingPopup( pszName, pszResFile )
		, m_flAutoJoinTime( 0.f )
	{}

	virtual void OnEnter() OVERRIDE	
	{
		CTFMatchmakingPopup::OnEnter();

		// We want to coincide the auto-join time with a short pause after the match-summary sequence if we're in
		// a match.
		if ( GTFGCClientSystem()->BConnectedToMatchServer( false ) )
		{
			Assert( TFGameRules()->State_Get() == GR_STATE_GAME_OVER || TFGameRules()->State_Get() == GR_STATE_TEAM_WIN );

			const double flNow = gpGlobals->curtime;

			// There's a point that's the earliest we want to autojoin and also a point that's the latest we want to autojoin.
			const double flDesiredAutoJoinTime = flNow + 10.;
			const double flLatestJoinTime = TFGameRules()->State_Get() == GR_STATE_GAME_OVER ? TFGameRules()->GetStateTransitionTime() - 1. : TFGameRules()->GetStateTransitionTime() + TFGameRules()->GetPostMatchPeriod();
			const double flEarliestJoinTime = TFGameRules()->State_Get() == GR_STATE_GAME_OVER ? 0. : TFGameRules()->GetStateTransitionTime() + 10.;

			// We also want the minimum time of the autojoin to be 10 seconds long.  If the earliest join time is greater than
			// the now + 10, go with that.  If now + 10 is beyond the latest join time, go with the latest join time.
			m_flAutoJoinTime = Max( flEarliestJoinTime, flDesiredAutoJoinTime );
			m_flAutoJoinTime = Min( m_flAutoJoinTime, flLatestJoinTime );
		}
	}

	virtual void OnUpdate() OVERRIDE	
	{
		CTFMatchmakingPopup::OnUpdate();

		// Autojoin time
		wchar_t *pwszAutoJoinTime = NULL;

		// Update the countdown label
		double flTimeUntilAutoJoin = Max( 0., m_flAutoJoinTime - gpGlobals->curtime );
		pwszAutoJoinTime = LocalizeNumberWithToken( "TF_Matchmaking_RollingQueue_AutojoinWarning", ceil( flTimeUntilAutoJoin ) );
		
		SetDialogVariable( "auto_join", pwszAutoJoinTime );
		

		if ( m_flAutoJoinTime != 0.f && gpGlobals->curtime > m_flAutoJoinTime )
		{
			GTFGCClientSystem()->JoinMMMatch();
		}
	}

	virtual void OnExit() OVERRIDE	
	{
		CTFMatchmakingPopup::OnExit();

		// No more auto join timer
		m_flAutoJoinTime = 0.f;
	}

	virtual bool ShouldBeActve() const OVERRIDE
	{
#ifdef STAGING_ONLY
		if ( FStrEq( const_cast<CNewMatchFoundDashboardState*>(this)->GetName(), tf_mm_popup_state_override.GetString() ) ) 
			return true;
#endif

		if ( BInEndOfMatch() && !GTFGCClientSystem()->BConnectedToMatchServer( true ) && GTFGCClientSystem()->BHaveLiveMatch() )
		{
			return true;
		}

		return false;
	}

private:
	double m_flAutoJoinTime;
};

REG_MM_POPUP_FACTORY( CNewMatchFoundDashboardState, "NewMatchFound", "resource/UI/MatchMakingDashboardPopup_NewMatch.res" )