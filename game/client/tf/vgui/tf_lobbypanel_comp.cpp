//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"

#include "tf_party.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/ScrollableEditablePanel.h"

#include "tf_lobbypanel_comp.h"
#include "tf_lobby_container_frame_comp.h"

#include "vgui/ISystem.h"

#include "tf_streams.h"
#include "tf_badge_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar tf_mm_ladder_ui_last_rating_change( "tf_mm_ladder_ui_last_rating_change", "0", FCVAR_HIDDEN | FCVAR_ARCHIVE, "Track last match skillrating change for UI." );
ConVar tf_mm_ladder_ui_last_rating_time( "tf_mm_ladder_ui_last_rating_time", "-1", FCVAR_HIDDEN | FCVAR_ARCHIVE, "Track last match skillrating change time for UI." );

class CLobbyPanel_Comp;

#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>

class CMatchHistoryEntryPanel : public CExpandablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CMatchHistoryEntryPanel, CExpandablePanel );
	CMatchHistoryEntryPanel( Panel* pParent, const char *pszPanelname )
		: BaseClass( pParent, pszPanelname )
	{}

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE
	{
		BaseClass::ApplySchemeSettings( pScheme );

		LoadControlSettings( "resource/ui/MatchHistoryEntryPanel.res" );
	}

	void SetMatchData( const CSOTFMatchResultPlayerStats& stats )
	{
		EditablePanel* pContainer = FindControl< EditablePanel >( "Container" );
		if ( !pContainer )
			return;

		// Match date
		CRTime matchdate( stats.endtime() );
		char rtime_buf[k_RTimeRenderBufferSize];
		matchdate.Render( rtime_buf );
		pContainer->SetDialogVariable( "match_date", rtime_buf );

		// Map name
		const MapDef_t* pMapDef = GetItemSchema()->GetMasterMapDefByIndex( stats.map_index() );
		const char* pszMapToken = "#TF_Map_Unknown";
		if ( pMapDef )
		{
			pszMapToken = pMapDef->pszMapNameLocKey;
		}
		pContainer->SetDialogVariable( "map_name", g_pVGuiLocalize->Find( pszMapToken ) );

		// KD ratio
		float flKDRatio = stats.kills();
		if ( stats.deaths() > 0 )
		{
			flKDRatio /= (float)stats.deaths();
		}
		pContainer->SetDialogVariable( "kd_ratio", CFmtStr( "%.1f", flKDRatio ) );

		pContainer->SetControlVisible( "WinLabel", stats.display_rating_change() > 0 );
		pContainer->SetControlVisible( "LossLabel", stats.display_rating_change() < 0 );

		EditablePanel* pStatsContainer = FindControl< EditablePanel >( "SlidingStatsContainer", true );
		if ( pStatsContainer )
		{
			pStatsContainer->SetDialogVariable( "stat_kills", LocalizeNumberWithToken( "TF_Competitive_Kills", stats.kills() ) );
			pStatsContainer->SetDialogVariable( "stat_deaths", LocalizeNumberWithToken( "TF_Competitive_Deaths", stats.deaths() ) );
			pStatsContainer->SetDialogVariable( "stat_damage", LocalizeNumberWithToken( "TF_Competitive_Damage", stats.damage() ) );
			pStatsContainer->SetDialogVariable( "stat_healing", LocalizeNumberWithToken( "TF_Competitive_Healing", stats.healing() ) );
			pStatsContainer->SetDialogVariable( "stat_support", LocalizeNumberWithToken( "TF_Competitive_Support", stats.support() ) );
			pStatsContainer->SetDialogVariable( "stat_score", LocalizeNumberWithToken( "TF_Competitive_Score", stats.score() ) );

			ScalableImagePanel* pMapImage = pStatsContainer->FindControl< ScalableImagePanel >( "BGImage", true );
			if ( pMapImage && pMapDef )
			{
				char imagename[ 512 ];
				Q_snprintf( imagename, sizeof( imagename ), "..\\vgui\\maps\\menu_thumb_%s", pMapDef->pszMapName );
				pMapImage->SetImage( imagename );
			}

			// Lambdas, wherefore art thou...
			SetupClassIcon( pStatsContainer, "scout", TF_CLASS_SCOUT, stats );
			SetupClassIcon( pStatsContainer, "soldier", TF_CLASS_SOLDIER, stats );
			SetupClassIcon( pStatsContainer, "pyro", TF_CLASS_PYRO, stats );
			SetupClassIcon( pStatsContainer, "demo", TF_CLASS_DEMOMAN, stats );
			SetupClassIcon( pStatsContainer, "heavy", TF_CLASS_HEAVYWEAPONS, stats );
			SetupClassIcon( pStatsContainer, "engineer", TF_CLASS_ENGINEER, stats );
			SetupClassIcon( pStatsContainer, "medic", TF_CLASS_MEDIC, stats );
			SetupClassIcon( pStatsContainer, "sniper", TF_CLASS_SNIPER, stats );
			SetupClassIcon( pStatsContainer, "spy", TF_CLASS_SPY, stats );

			SetupMedalForStat( pStatsContainer, stats.kills_medal(), "kills" );
			SetupMedalForStat( pStatsContainer, stats.damage_medal(), "damage" );
			SetupMedalForStat( pStatsContainer, stats.healing_medal(), "healing" );
			SetupMedalForStat( pStatsContainer, stats.support_medal(), "support" );
			SetupMedalForStat( pStatsContainer, stats.score_medal(), "score" );
		}
	}
private:

	void SetupMedalForStat( EditablePanel* pParent, int nStat, const char* pszStatName )
	{
		ScalableImagePanel* pMedalImage = pParent->FindControl< ScalableImagePanel >( CFmtStr( "%sMedal", pszStatName ) );
		if ( pMedalImage )
		{
			if ( nStat != StatMedal_None )
			{
				pMedalImage->SetImage( g_pszCompetitiveMedalImages[ nStat ] );
			}
			pMedalImage->SetVisible( nStat != StatMedal_None );
		}
	}

	void SetupClassIcon( EditablePanel* pParent, const char* pszClassName, int nBit, const CSOTFMatchResultPlayerStats& stats )
	{
		ScalableImagePanel* pClassIcon = pParent->FindControl< ScalableImagePanel >( CFmtStr( "%sIcon", pszClassName ), true );
		if( pClassIcon )
		{ 
			pClassIcon->SetImage( stats.classes_played() & (1<<nBit) ? CFmtStr( "class_icons/filter_%s_on", pszClassName ) : CFmtStr( "class_icons/filter_%s", pszClassName ) );
		} 
	}
};

DECLARE_BUILD_FACTORY( CMatchHistoryEntryPanel );

extern Color s_colorChallengeHeader;

DECLARE_BUILD_FACTORY( CLadderLobbyLeaderboard );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLadderLobbyLeaderboard::CLadderLobbyLeaderboard( Panel *pParent, const char *pszPanelName )
	: CTFLeaderboardPanel( pParent, pszPanelName )
{
	m_pScoreList = new vgui::EditablePanel( this, "ScoreList" );
	m_pScoreListScroller = new vgui::ScrollableEditablePanel( this, m_pScoreList, "ScoreListScroller" );
	m_pScoreListScroller->AddActionSignalTarget( this );

	m_pszLeaderboardName = "tf2_ladder_6v6";

	for ( int i = 0; i < 100; ++i )
	{
		vgui::EditablePanel *pEntryUI = new vgui::EditablePanel( m_pScoreList, "LeaderboardEntry" );
		m_vecLeaderboardEntries.AddToTail( pEntryUI );
	}

	m_pToolTip = new CTFTextToolTip( this );
 	m_pToolTipEmbeddedPanel = new vgui::EditablePanel( this, "TooltipPanel" );		
	m_pToolTipEmbeddedPanel->SetKeyBoardInputEnabled( false );
	m_pToolTipEmbeddedPanel->SetMouseInputEnabled( false );
 	m_pToolTip->SetEmbeddedPanel( m_pToolTipEmbeddedPanel );
	m_pToolTip->SetTooltipDelay( 0 );

	m_bIsDataValid = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLadderLobbyLeaderboard::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/econ/LobbyLeaderboard.res" );

	FOR_EACH_VEC( m_vecLeaderboardEntries, i )
	{
		m_vecLeaderboardEntries[i]->ApplySchemeSettings( pScheme );
		m_vecLeaderboardEntries[i]->LoadControlSettings( "Resource/UI/LeaderboardEntryRank.res" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLadderLobbyLeaderboard::PerformLayout()
{
	BaseClass::PerformLayout();

	UpdateLeaderboards();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLadderLobbyLeaderboard::OnCommand( const char *command )
{
	if ( Q_strnicmp( command, "stream", 6 ) == 0 )
	{
		vgui::system()->ShellExecute( "open", command + 7 );
		return;
	}
	else if ( FStrEq( "global", command ) )
	{
		if ( m_bGlobal != true )
		{
			m_bGlobal = true;
			UpdateLeaderboards();
		}

		return;
	}
	else if ( FStrEq( "local", command ) )
	{
		if ( m_bGlobal == true )
		{
			m_bGlobal = false;
			UpdateLeaderboards();
		}
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLadderLobbyLeaderboard::GetLeaderboardData( CUtlVector< LeaderboardEntry_t* >& scores )
{
	CUtlVector< LeaderboardEntry_t* > vecLadderScores;
	if ( Leaderboards_GetLadderLeaderboard( vecLadderScores, m_pszLeaderboardName, m_bGlobal ) )
	{
		scores.AddVectorToTail( vecLadderScores );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLadderLobbyLeaderboard::UpdateLeaderboards()
{
	CUtlVector< LeaderboardEntry_t* > scores;
	m_bIsDataValid = GetLeaderboardData( scores );
	if ( !m_bIsDataValid )
		return false;

	int nScoreListHeight = scores.Count() * m_yEntryStep;
	int nScrollerWidth, nScrollerHeight;
	m_pScoreListScroller->GetSize( nScrollerWidth, nScrollerHeight );

	m_pScoreList->SetSize( nScrollerWidth, Max( nScoreListHeight, nScrollerHeight ) );

	m_pScoreList->InvalidateLayout( true );
	m_pScoreListScroller->InvalidateLayout( true );
	m_pScoreListScroller->GetScrollbar()->InvalidateLayout( true );
	static int nScrollWidth = m_pScoreListScroller->GetScrollbar()->GetWide();
	m_pScoreListScroller->GetScrollbar()->SetWide( nScrollWidth>>1 );
	if ( m_pScoreListScroller->GetScrollbar()->GetButton( 0 ) &&
			m_pScoreListScroller->GetScrollbar()->GetButton( 1 ) )
	{
		m_pScoreListScroller->GetScrollbar()->GetButton( 0 )->SetVisible( false );
		m_pScoreListScroller->GetScrollbar()->GetButton( 1 )->SetVisible( false );
	}

	FOR_EACH_VEC( m_vecLeaderboardEntries, i )
	{
		EditablePanel *pContainer = dynamic_cast< EditablePanel* >( m_vecLeaderboardEntries[i] );
		if ( !pContainer )
			continue;

		Color colorToUse = i % 2 == 1 ? m_OddTextColor : m_EvenTextColor;

		bool bIsEntryVisible = i < scores.Count();
		pContainer->SetVisible( bIsEntryVisible );
		pContainer->SetPos( 0, i * m_yEntryStep );
		if ( bIsEntryVisible )
		{
			const LeaderboardEntry_t *pLeaderboardEntry = scores[i];
			const CSteamID &steamID = pLeaderboardEntry->m_steamIDUser;

#ifdef TWITCH_LEADERBOARD
			TwitchTvAccountInfo_t *pTwitchInfo = StreamManager()->GetTwitchTvAccountInfo( steamID.ConvertToUint64() );
			ETwitchTvState_t twitchState = pTwitchInfo ? pTwitchInfo->m_eTwitchTvState : k_ETwitchTvState_Error;
			// still waiting for twitch info to load
			if ( twitchState <= k_ETwitchTvState_Loading )
				return false;
#endif // TWITCH_LEADERBOARD

			bool bIsLocalPlayer = steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUser()->GetSteamID() == steamID;
			pContainer->SetDialogVariable( "position", m_bGlobal ? CFmtStr( "%d.", pLeaderboardEntry->m_nGlobalRank ) : "" );
			pContainer->SetDialogVariable( "username", InventoryManager()->PersonaName_Get( steamID.GetAccountID() ) );

			CExLabel *pNameText = dynamic_cast< CExLabel* >( pContainer->FindChildByName( "UserName" ) );
			if ( pNameText )
			{
				pNameText->SetColorStr( bIsLocalPlayer ? m_LocalPlayerTextColor : colorToUse );
			}

			CAvatarImagePanel *pAvatar = dynamic_cast< CAvatarImagePanel* >( pContainer->FindChildByName( "AvatarImage" ) );
			if ( pAvatar )
			{
				pAvatar->SetShouldDrawFriendIcon( false );
				pAvatar->SetPlayer( steamID, k_EAvatarSize32x32 );
			}

			CTFBadgePanel *pRankImage = dynamic_cast< CTFBadgePanel* >( pContainer->FindChildByName( "RankImage" ) );
			if ( pRankImage )
			{
				const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( k_nMatchGroup_Ladder_6v6 );
				const LevelInfo_t& levelInfo = pMatchDesc->m_pProgressionDesc->GetLevelForExperience( pLeaderboardEntry->m_nScore );
				pRankImage->SetupBadge( pMatchDesc->m_pProgressionDesc, levelInfo );

				wchar_t wszOutString[ 128 ];
				char szLocalized[512];
				wchar_t wszCount[ 16 ];
				_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", levelInfo.m_nLevelNum );
				const wchar_t *wpszFormat = g_pVGuiLocalize->Find( pMatchDesc->m_pProgressionDesc->m_pszLevelToken );
				g_pVGuiLocalize->ConstructString_safe( wszOutString, wpszFormat, 2, wszCount, g_pVGuiLocalize->Find( levelInfo.m_pszLevelTitle ) );
				g_pVGuiLocalize->ConvertUnicodeToANSI( wszOutString, szLocalized, sizeof( szLocalized ) );

				pRankImage->SetMouseInputEnabled( true );
				pRankImage->SetVisible( true );
				pRankImage->SetTooltip( m_pToolTip, szLocalized );
			}

			CExImageButton *pStreamImage = dynamic_cast< CExImageButton* >( pContainer->FindChildByName( "StreamImageButton" ) );
			if ( pStreamImage )
			{
#ifdef TWITCH_LEADERBOARD
				if ( twitchState == k_ETwitchTvState_Linked )
				{
					pStreamImage->SetVisible( true );
					pStreamImage->SetCommand( CFmtStr( "stream %s", pTwitchInfo->m_sTwitchTvChannel.String() ) );
				}
				else
#endif // TWITCH_LEADERBOARD
				{
					pStreamImage->SetVisible( false );
					pStreamImage->SetCommand( "" );
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLadderLobbyLeaderboard::SetLeaderboard( const char *pszLeaderboardName, bool bGlobal )
{
	m_pszLeaderboardName = pszLeaderboardName;
	m_bGlobal = bGlobal;

	UpdateLeaderboards();
}

static void GetPlayerNameForSteamID( wchar_t *wCharPlayerName, int nBufSizeBytes, const CSteamID &steamID )
{
	const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
	V_UTF8ToUnicode( pszName, wCharPlayerName, nBufSizeBytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLobbyPanel_Comp::CLobbyPanel_Comp( vgui::Panel *pParent, CBaseLobbyContainerFrame* pLobbyContainer ) 
	: CBaseLobbyPanel( pParent, pLobbyContainer )
	, m_pCompetitiveModeLeaderboard( NULL )
	, m_pMatchHistoryScroller( NULL )
	, m_eMatchSortMethod( SORT_BY_DATE )
	, m_bDescendingMatchHistorySort( true )
{
	// Comp
	m_fontMedalsCount = 0;

	m_flCompetitiveRankProgress = -1.f;
	m_flCompetitiveRankPrevProgress = -1.f;
	m_flRefreshPlayerListTime = -1.f;
	m_bCompetitiveRankChangePlayedSound = false;
	m_bMatchHistoryLoaded = false;
	m_bMatchDataForLocalPlayerDirty = true;

	ListenForGameEvent( "gc_new_session" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLobbyPanel_Comp::~CLobbyPanel_Comp()
{
	delete m_pImageList;
	m_pImageList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::OnCommand( const char *command )
{

	if ( FStrEq( command, "medals_help" ) )
	{
		CExplanationPopup *pPopup = dynamic_cast< CExplanationPopup* >( GetParent()->FindChildByName( "MedalsHelp" ) );
		if ( pPopup )
		{
			pPopup->Popup();
		}
		return;
	}
	else if ( FStrEq( command, "show_leaderboards" ) )
	{
		if ( m_pCompetitiveModeLeaderboard )
		{
			m_pCompetitiveModeLeaderboard->SetVisible( true );
		}
		 
		SetControlVisible( "MatchHistoryCategories", false, true );
		SetControlVisible( "MatchHistoryContainer", false, true );

		return;
	}
	else if ( FStrEq( command, "show_match_history" ) )
	{
		m_bMatchDataForLocalPlayerDirty = true;

		if ( m_pCompetitiveModeLeaderboard )
		{
			m_pCompetitiveModeLeaderboard->SetVisible( false );
		}

		SetControlVisible( "MatchHistoryCategories", true, true );
		SetControlVisible( "MatchHistoryContainer", true, true );
		
		return;
	}
	else if ( !Q_strncmp( "sort", command, 4 ) )
	{
		EMatchHistorySortMethods_t eNewMethod = (EMatchHistorySortMethods_t)atoi( command + 4 );

		if ( eNewMethod == m_eMatchSortMethod )
		{
			m_bDescendingMatchHistorySort = !m_bDescendingMatchHistorySort;
		}
		else
		{
			m_eMatchSortMethod = eNewMethod;
			m_bDescendingMatchHistorySort = true;
		}

		m_bMatchDataForLocalPlayerDirty = true;

		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLobbyPanel_Comp::ShouldShowLateJoin() const
{
	return false; // For now
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::ApplyChatUserSettings( const CBaseLobbyPanel::LobbyPlayerInfo &player, KeyValues *pKV ) const
{
	pKV->SetInt( "has_ticket", 0 );
	pKV->SetInt( "squad_surplus", 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::WriteGameSettingsControls()
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

	m_pContainer->SetNextButtonEnabled( true );

	SetControlVisible( "ScrollableContainer", GTFGCClientSystem()->GetWizardStep()== TF_Matchmaking_WizardStep_LADDER );
	
	--m_iWritingPanel;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLobbyPanel_Comp::GetMedalCountForStat( EMatchGroup unLadderType, RankStatType_t nStatType, int nMedalLevel )
{
	CSOTFLadderData *pData = GetLocalPlayerLadderData( unLadderType );
	if ( !pData )
		return 0;

	switch ( nStatType )
	{
	case RankStat_Score:
		if ( nMedalLevel == StatMedal_Bronze )
		{
			return pData->Obj().score_bronze();
		}
		else if ( nMedalLevel == StatMedal_Silver )
		{
			return pData->Obj().score_silver();
		}
		else if ( nMedalLevel == StatMedal_Gold )
		{
			return pData->Obj().score_gold();
		}
		break;
	case RankStat_Kills:
		if ( nMedalLevel == StatMedal_Bronze )
		{
			return pData->Obj().kills_bronze();
		}
		else if ( nMedalLevel == StatMedal_Silver )
		{
			return pData->Obj().kills_silver();
		}
		else if ( nMedalLevel == StatMedal_Gold )
		{
			return pData->Obj().kills_gold();
		}
		break;
	case RankStat_Damage:
		if ( nMedalLevel == StatMedal_Bronze )
		{
			return pData->Obj().damage_bronze();
		}
		else if ( nMedalLevel == StatMedal_Silver )
		{
			return pData->Obj().damage_silver();
		}
		else if ( nMedalLevel == StatMedal_Gold )
		{
			return pData->Obj().damage_gold();
		}
		break;
	case RankStat_Healing:
		if ( nMedalLevel == StatMedal_Bronze )
		{
			return pData->Obj().healing_bronze();
		}
		else if ( nMedalLevel == StatMedal_Silver )
		{
			return pData->Obj().healing_silver();
		}
		else if ( nMedalLevel == StatMedal_Gold )
		{
			return pData->Obj().healing_gold();
		}
		break;
	case RankStat_Support:
		if ( nMedalLevel == StatMedal_Bronze )
		{
			return pData->Obj().support_bronze();
		}
		else if ( nMedalLevel == StatMedal_Silver )
		{
			return pData->Obj().support_silver();
		}
		else if ( nMedalLevel == StatMedal_Gold )
		{
			return pData->Obj().support_gold();
		}
		break;
	case RankStat_Deaths:
		// Not supported
		break;
	default:
		Assert( 0 );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::OnThink()
{
	BaseClass::OnThink();

	if ( m_bMatchDataForLocalPlayerDirty )
	{
		UpdateMatchDataForLocalPlayer();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::FireGameEvent( IGameEvent *event )
{
	const char *pszEventname = event->GetName();
	if ( !Q_stricmp( pszEventname, "gc_new_session" ) )
	{
		// This is loaded on demand by the GC - if we have a new session, we need to re-request
		if ( m_bMatchHistoryLoaded )
		{
			m_bMatchHistoryLoaded = false;
			m_bMatchDataForLocalPlayerDirty = true;
		}
	}

	BaseClass::FireGameEvent( event );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
EMatchGroup CLobbyPanel_Comp::GetMatchGroup( void ) const
{
	return k_nMatchGroup_Ladder_6v6;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent )
{
	if ( pObject->GetTypeID() != CSOTFMatchResultPlayerInfo::k_nTypeID )
		return;

	m_bMatchDataForLocalPlayerDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent )
{
	if ( pObject->GetTypeID() != CSOTFMatchResultPlayerInfo::k_nTypeID )
		return;

	m_bMatchDataForLocalPlayerDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::PerformLayout()
{
	BaseClass::PerformLayout();

	m_bMatchDataForLocalPlayerDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLobbyPanel_Comp::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	EditablePanel* pPlaylistBGPanel = FindControl< EditablePanel >( "PlaylistBGPanel", true );
	m_pCompetitiveModeLeaderboard = pPlaylistBGPanel->FindControl< CLadderLobbyLeaderboard >( "Leaderboard", true );

	m_pMatchHistoryScroller = pPlaylistBGPanel->FindControl< CScrollableList >( "MatchHistoryContainer" );

	int nAvatarWidth = ( ( m_iAvatarWidth * 5 / 4 ) + 1 );
	int nExtraWidth = ( m_pChatPlayerList->GetWide() - ( 2 * nAvatarWidth ) - m_iPlayerNameWidth - m_iBannedWidth - m_iHasPassWidth );

	m_pChatPlayerList->AddColumnToSection( 0, "avatar", "#TF_Players", vgui::SectionedListPanel::COLUMN_IMAGE, nAvatarWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "name", "", 0, m_iPlayerNameWidth + nExtraWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "is_banned", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, m_iBannedWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "has_competitive_access", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, m_iHasPassWidth );
	m_pChatPlayerList->AddColumnToSection( 0, "rank", "", vgui::SectionedListPanel::COLUMN_IMAGE | vgui::SectionedListPanel::COLUMN_CENTER, nAvatarWidth );
	m_pChatPlayerList->SetDrawHeaders( false );
	m_fontMedalsCount = pScheme->GetFont( "HudFontSmallestBold", true );
}

static int SortResult( const CSOTFMatchResultPlayerStats * a, const CSOTFMatchResultPlayerStats * b )
{
	return a->display_rating_change() < b->display_rating_change();
}

static int SortDate( const CSOTFMatchResultPlayerStats * a, const CSOTFMatchResultPlayerStats * b )
{
	return a->endtime() < b->endtime();
}

static int SortMap( const CSOTFMatchResultPlayerStats * a, const CSOTFMatchResultPlayerStats * b )
{
	const MapDef_t* pMapDef = GetItemSchema()->GetMasterMapDefByIndex( a->map_index() );
	const wchar_t* pszAMapName =  g_pVGuiLocalize->Find( pMapDef ? pMapDef->pszMapNameLocKey : "#TF_Map_Unknown" );

	pMapDef = GetItemSchema()->GetMasterMapDefByIndex( b->map_index() );
	const wchar_t* pszBMapName = g_pVGuiLocalize->Find( pMapDef ? pMapDef->pszMapNameLocKey : "#TF_Map_Unknown" );

	return wcscoll( pszAMapName, pszBMapName );
}

static int SortKDR( const CSOTFMatchResultPlayerStats * a, const CSOTFMatchResultPlayerStats * b )
{
	float flAKDRatio = a->kills();
	if ( a->deaths() > 0 )
	{
		flAKDRatio /= (float)a->deaths();
	}

	float flBKDRatio = b->kills();
	if ( b->deaths() > 0 )
	{
		flBKDRatio /= (float)b->deaths();
	}

	return ( flBKDRatio - flAKDRatio ) * 1000.f;
}

void CLobbyPanel_Comp::UpdateMatchDataForLocalPlayer()
{
	m_bMatchDataForLocalPlayerDirty = false;

	CUtlVector < CSOTFMatchResultPlayerStats > vecMatches;
	GetLocalPlayerMatchHistory( GetMatchGroup(), vecMatches );

	if ( !m_bMatchHistoryLoaded )
	{
		GCSDK::CProtoBufMsg< CMsgGCMatchHistoryLoad > msg( k_EMsgGCMatchHistoryLoad );
		GCClientSystem()->BSendMessage( msg );

		m_bMatchHistoryLoaded = true;
	}

	if ( m_pMatchHistoryScroller )
	{
		m_pMatchHistoryScroller->ClearAutoLayoutPanels();

		typedef int (*MatchSortFunc)( const CSOTFMatchResultPlayerStats * a, const CSOTFMatchResultPlayerStats * b );

		struct SortMethodData_t
		{
			MatchSortFunc m_pfnSort;
			CExButton* m_pSortButton;
		};

		EditablePanel* pPlaylistBGPanel = FindControl< EditablePanel >( "PlaylistBGPanel", true );
		Assert( pPlaylistBGPanel );
		if ( !pPlaylistBGPanel )
			return;

		SortMethodData_t sortMethods[ NUM_SORT_METHODS ] = { { &SortResult, pPlaylistBGPanel->FindControl< CExButton >( "ResultButton", true ) }
														   , { &SortDate, pPlaylistBGPanel->FindControl< CExButton >( "DateButton", true ) }
														   , { &SortMap, pPlaylistBGPanel->FindControl< CExButton >( "MapButton", true ) }
														   , { &SortKDR, pPlaylistBGPanel->FindControl< CExButton >( "KDRButton", true ) } };

		// Sort
		vecMatches.Sort( sortMethods[ m_eMatchSortMethod ].m_pfnSort );

		Label* pSortArrow = pPlaylistBGPanel->FindControl< Label >( "SortArrow", true );
		Assert( pSortArrow );
		if ( !pSortArrow )
			return;

		// Update controls
		for( int i=0; i<ARRAYSIZE( sortMethods ); ++i )
		{
			if( sortMethods[ i ].m_pSortButton )
			{
				bool bSelected = i == m_eMatchSortMethod;
				sortMethods[ i ].m_pSortButton->SetSelected( bSelected );

				if ( bSelected )
				{
					int nDummy, nX;
					sortMethods[ i ].m_pSortButton->GetContentSize( nX, nDummy );
					// Move the sort arrow to the right edge of the selected panel
					pSortArrow->SetPos( sortMethods[ i ].m_pSortButton->GetXPos() + nX , pSortArrow->GetYPos() );
					// Fixup the label to be an up or down arrow
					pSortArrow->SetText( m_bDescendingMatchHistorySort ? L"6" : L"5" );
				}
			}
		}

		// Potentially go backwards
		if ( m_bDescendingMatchHistorySort )
		{
			FOR_EACH_VEC( vecMatches, i )
			{
				CMatchHistoryEntryPanel* pMatchEntryPanel = new CMatchHistoryEntryPanel( m_pMatchHistoryScroller, "MatchEntry" );
				pMatchEntryPanel->MakeReadyForUse();
				pMatchEntryPanel->SetMatchData( vecMatches[ i ] );
				m_pMatchHistoryScroller->AddPanel( pMatchEntryPanel, 5 );
			}
		}
		else
		{
			FOR_EACH_VEC_BACK( vecMatches, i )
			{
				CMatchHistoryEntryPanel* pMatchEntryPanel = new CMatchHistoryEntryPanel( m_pMatchHistoryScroller, "MatchEntry" );
				pMatchEntryPanel->MakeReadyForUse();
				pMatchEntryPanel->SetMatchData( vecMatches[ i ] );
				m_pMatchHistoryScroller->AddPanel( pMatchEntryPanel, 5 );
			}
		}

		m_pMatchHistoryScroller->MakeReadyForUse();
	}
}
