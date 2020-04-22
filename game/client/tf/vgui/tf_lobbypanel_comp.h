//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef TF_LOBBYPANEL_COMP_H
#define TF_LOBBYPANEL_COMP_H

#include "cbase.h"
#include "game/client/iviewport.h"
#include "tf_lobbypanel.h"
#include "tf_leaderboardpanel.h"
#include "local_steam_shared_object_listener.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace GCSDK;

class CBaseLobbyPanel;

namespace vgui
{
	class ScrollableEditablePanel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLadderLobbyLeaderboard : public CTFLeaderboardPanel
{
	DECLARE_CLASS_SIMPLE( CLadderLobbyLeaderboard, CTFLeaderboardPanel );
public:

	CLadderLobbyLeaderboard( Panel *pParent, const char *pszPanelName );

	//-----------------------------------------------------------------------------
	// Purpose: Create leaderboard panels
	//-----------------------------------------------------------------------------
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

	virtual bool GetLeaderboardData( CUtlVector< LeaderboardEntry_t* >& scores );
	virtual bool UpdateLeaderboards();

	void SetLeaderboard( const char *pszLeaderboardName, bool bGlobal );

	const char *GetLeaderboardName() const { return m_pszLeaderboardName; }
	bool IsDataValid( void ) { return m_bIsDataValid; }

private:
	const char *m_pszLeaderboardName;
	bool m_bGlobal;
	bool m_bIsDataValid;

	vgui::ScrollableEditablePanel *m_pScoreListScroller;
	EditablePanel *m_pScoreList;

	CTFTextToolTip		*m_pToolTip;
	vgui::EditablePanel		*m_pToolTipEmbeddedPanel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLobbyPanel_Comp : public CBaseLobbyPanel, public CLocalSteamSharedObjectListener
{
	DECLARE_CLASS_SIMPLE( CLobbyPanel_Comp, CBaseLobbyPanel );

public:
	CLobbyPanel_Comp( vgui::Panel *pParent, CBaseLobbyContainerFrame* pLobbyContainer );
	virtual ~CLobbyPanel_Comp();

	//
	// Panel overrides
	//
	virtual void PerformLayout() OVERRIDE;
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

	virtual EMatchGroup GetMatchGroup( void ) const OVERRIDE;

	virtual void SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;
	virtual void SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;

	virtual void OnThink() OVERRIDE;

	//
	// CGameEventListener overrides
	//
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

private:
	virtual bool ShouldShowLateJoin() const OVERRIDE;
	virtual void ApplyChatUserSettings( const LobbyPlayerInfo &player,KeyValues *pKV ) const OVERRIDE;
	virtual const char* GetResFile() const OVERRIDE { return "Resource/UI/LobbyPanel_Comp.res"; }

	CPanelAnimationVarAliasType( int, m_iStatMedalWidth, "stat_medal_width", "14", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iMedalCountWidth, "stat_medal_count_width", "20", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iHasPassWidth, "has_pass_width", "12", "proportional_int" );

	CUtlVector<vgui::Label *> m_vecSearchCriteriaLabels;

	// leaderboards
	CLadderLobbyLeaderboard *m_pCompetitiveModeLeaderboard;

	vgui::HFont m_fontMedalsCount;

	enum EMatchHistorySortMethods_t
	{
		SORT_BY_RESULT = 0,
		SORT_BY_DATE,
		SORT_BY_MAP,
		SORT_BY_KDR,

		NUM_SORT_METHODS
	};

	CScrollableList* m_pMatchHistoryScroller;
	EMatchHistorySortMethods_t m_eMatchSortMethod;
	bool m_bDescendingMatchHistorySort;

	float m_flCompetitiveRankProgress;
	float m_flCompetitiveRankPrevProgress;
	float m_flRefreshPlayerListTime;
	bool m_bCompetitiveRankChangePlayedSound;
	bool m_bMatchHistoryLoaded;

	void WriteGameSettingsControls() OVERRIDE;

	int GetMedalCountForStat( EMatchGroup unLadderType, RankStatType_t nStatType, int nMedalLevel );


	void UpdateMatchDataForLocalPlayer();
	bool m_bMatchDataForLocalPlayerDirty;
};

#endif //TF_LOBBYPANEL_COMP_H
