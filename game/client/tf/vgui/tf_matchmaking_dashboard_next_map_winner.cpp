//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"
#include "tf_matchmaking_dashboard.h"
#include "tf_gamerules.h"
#include "tf_gc_client.h"
#include <vgui/ISurface.h>

using namespace vgui;
using namespace GCSDK;

#ifdef STAGING_ONLY 
extern ConVar tf_mm_popup_state_override;
#endif

class CNextMapWinnerDashboardState : public CTFMatchmakingPopup
{
public:

	CNextMapWinnerDashboardState( const char* pszName, const char* pszResFile )
		: CTFMatchmakingPopup( pszName, pszResFile )
	{
	}

	virtual bool ShouldBeActve() const OVERRIDE
	{
#ifdef STAGING_ONLY
		if ( FStrEq( const_cast<CNextMapWinnerDashboardState*>(this)->GetName(), tf_mm_popup_state_override.GetString() ) ) 
			return true;
#endif

		int nVotes[ CTFGameRules::EUserNextMapVote::NUM_VOTE_STATES ];
		CTFGameRules::EUserNextMapVote eWinningVote = TFGameRules()->GetWinningVote( nVotes );

		if ( BInEndOfMatch() &&
			 TFGameRules() &&
			 TFGameRules()->GetCurrentNextMapVotingState() == CTFGameRules::NEXT_MAP_VOTE_STATE_MAP_CHOSEN_PAUSE &&
			 eWinningVote != CTFGameRules::USER_NEXT_MAP_VOTE_UNDECIDED && 
			 GTFGCClientSystem()->BConnectedToMatchServer( false ) )
		{
			return true;
		}

		return false;
	}

private:

	virtual void OnEnter() OVERRIDE
	{
		CTFMatchmakingPopup::OnEnter();

		MapDefIndex_t nWinningDefIndex;
		if ( TFGameRules() )
		{
			int nVotes[ CTFGameRules::EUserNextMapVote::NUM_VOTE_STATES ];
			CTFGameRules::EUserNextMapVote eWinningVote = TFGameRules()->GetWinningVote( nVotes );
			nWinningDefIndex = TFGameRules()->GetNextMapVoteOption( eWinningVote );
		}
		else
		{
			Assert( false );
			nWinningDefIndex = RandomInt( 1, GetItemSchema()->GetMapCount() - 1 );
		}

		// Sound effect for success
		surface()->PlaySound( UTIL_GetRandomSoundFromEntry( "Vote.Passed" ) );
		
		const MapDef_t *pMapDef = GetItemSchema()->GetMasterMapDefByIndex( nWinningDefIndex );
		if ( pMapDef )
		{
			Label* pMapNameLabel = FindControl< Label >( "NameLabel", true );
			if ( pMapNameLabel )
			{
				pMapNameLabel->SetText( g_pVGuiLocalize->Find( pMapDef->pszMapNameLocKey ) );
			}

			ScalableImagePanel* pMapImage = FindControl< ScalableImagePanel >( "MapImage", true );
			if ( pMapImage )
			{
				char imagename[ 512 ];
				Q_snprintf( imagename, sizeof( imagename ), "..\\vgui\\maps\\menu_thumb_%s", pMapDef->pszMapName );
				pMapImage->SetImage( imagename );
			}
		}
	}
};

REG_MM_POPUP_FACTORY( CNextMapWinnerDashboardState, "NextMapWinner", "resource/UI/MatchMakingDashboardPopup_NextMapWinner.res" )