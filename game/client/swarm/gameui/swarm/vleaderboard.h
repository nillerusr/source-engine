//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VLEADERBOARD_H__
#define __VLEADERBOARD_H__

#include "basemodui.h"
#include "tier1/utlstack.h"

#include "VFlyoutMenu.h"

#if !defined( _X360 )
#include "steam/isteamuserstats.h"
#endif 

namespace BaseModUI {

	class GenericPanelList;
	class FoundGames;
	class BaseModHybridButton;

	typedef struct
	{
#if defined( _X360 )
		XUID m_xuid;
		char m_szGamerTag[XUSER_NAME_SIZE];
		int m_iControllerIndex;
#else
		CSteamID m_steamIDUser;
		char m_szName[MAX_PLAYER_NAME_LENGTH];
#endif
		int m_iRank;
		int m_iDisplayRank;
		bool m_bShowRank;
		int m_iAwards;
		int m_iTimeInHundredths;
		bool m_bLocalPlayer;

	} LeaderboardItem_t;	

	class LeaderboardListItem : public vgui::EditablePanel, public IBaseModFrameListener
	{
		DECLARE_CLASS_SIMPLE( LeaderboardListItem, vgui::EditablePanel );

	public:
		LeaderboardListItem( vgui::Panel *parent, const char *panelName );
		~LeaderboardListItem();

		virtual void RunFrame() {};

		void SetData( LeaderboardItem_t data );
		LeaderboardItem_t *GetDataForModify( void );

		virtual void PaintBackground();
		virtual void OnCommand( const char *command );

		virtual void NavigateTo( void );
		virtual void NavigateFrom( void );

		bool IsSelected( void ) { return m_bSelected; }
		void SetSelected( bool bSelected );

#if !defined( _X360 )
		bool HasMouseover( void ) { return m_bHasMouseover; }
		void SetHasMouseover( bool bHasMouseover ) { m_bHasMouseover = bHasMouseover; }

		virtual void OnCursorEntered( void );
#endif

		virtual void OnKeyCodePressed( vgui::KeyCode code );

		int GetRank( void );
		const char *GetPlayerName( void );
		int GetTimeInHundredths( void ) { return m_data.m_iTimeInHundredths; }

#if defined( _X360 )
		XUID GetXUID( void ) { return m_data.m_xuid; }
#else
		CSteamID GetSteamID( void ) { return m_data.m_steamIDUser; }
#endif

		void SetShowRank( bool bShowRank, int iRankToDisplay );
/*
#ifdef _WIN32 
		virtual void OnMousePressed( vgui::MouseCode code );
#endif
*/

	protected:
		void ApplySchemeSettings( vgui::IScheme *pScheme );
		void PerformLayout();

		void SetRankLabelText( void );

	private:
		LeaderboardItem_t m_data;

		bool m_bSelected : 1;

#if !defined( _X360 )
		bool m_bHasMouseover : 1;
#endif
	};

	//=============================================================================

	class Leaderboard : public CBaseModFrame, public IBaseModFrameListener, public FlyoutMenuListener
	{
		DECLARE_CLASS_SIMPLE( Leaderboard, CBaseModFrame );

	public:
		Leaderboard( vgui::Panel *parent );
		~Leaderboard();

		virtual void Activate();
		virtual void OnKeyCodePressed( vgui::KeyCode code );
		virtual void RunFrame() {};		// used in found games to bypass the blur?
		virtual void PaintBackground();
		virtual void OnThink();

		virtual void OnCommand( const char *command );

		enum LEADERBOARD_MODE { LEADERBOARD_FRIENDS, LEADERBOARD_GLOBAL };

		void NavigateTo();

		MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", panelName );


#if defined( _X360 )
		void AsyncLoadCompleted( void );
#else
#endif 

		virtual void SetDataSettings( KeyValues *pSettings );
		void OnMissionChapterChanged();
		
	public:
		MESSAGE_FUNC_INT_CHARPTR( MsgOnCustomCampaignSelected, "OnCustomCampaignSelected", chapter, campaign );

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

		void OnClose();
		void OnOpen();

		void UpdateFooter();
		bool StartSearching( bool bCenterOnLocalPlayer );
		void CancelCurrentSearch( void );
		void AddPlayersToList( void );
		void SortListItems( void );
		void SendNextRequestInQueue( void );

		int GetCurrentChapterContext( void );

		int GetCurrentLeaderboardView( void );

		// Accessors for pool of unused LeaderboardListItems
		LeaderboardListItem *GetListItemFromPool( void );
		void ReturnListItemToPool( LeaderboardListItem *pItem );
		void ReturnAllListItemsToPool( void );

	private:
		// Commands
		void CmdViewGamercard();
		void CmdToggleLeaderboardType();
		void CmdJumpToTop();
		void CmdJumpToMe();

		void CmdPageUp();
		void CmdPageDown();

		void CmdLeaderboardHelper( int nOffset );
		void CmdNextLeaderboard();
		void CmdPrevLeaderboard();

#if defined( _X360 )
		void AddLeaderboardEntries( XUSER_STATS_READ_RESULTS *pResults );
		void AddLeaderboardEntry( XUSER_STATS_ROW *pRow );
#else
		void AddLeaderboardEntry( LeaderboardEntry_t *pEntry );
#endif

		bool SendQuery_EnumFriends( void );
		bool SendQuery_StatsFriends( void );
		bool SendQuery_PlayerRank( void );
		bool SendQuery_RankOneStats( void );
		bool SendQuery_StatsGlobalPage( void );

#if !defined( _X360 )
		bool SendQuery_FindLeaderboard( void );
#endif

		bool HandleQuery_EnumFriends( void );
		bool HandleQuery_StatsFriends( void );
		bool HandleQuery_PlayerRank( void );
		bool HandleQuery_RankOneStats( void );
		bool HandleQuery_StatsGlobalPage( void );

		void ClearAsyncData( void );

		int GetPageFromRank( int iRank );

		bool IsAQueryPending( void ) { return m_pendingRequest.asyncQueryType != QUERY_NONE; }

	private:
		GenericPanelList* m_pPanelList;
		CUtlStack< LeaderboardListItem * > m_ListItemPool;

		LEADERBOARD_MODE m_Mode;		// Friends or Global

		KeyValues *m_pDataSettings;

#if !defined( _X360 )

		// Steam callbacks
		void OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure );
		CCallResult<Leaderboard, LeaderboardFindResult_t> m_SteamCallResultCreateLeaderboard;

		void OnLeaderboardDownloadedEntries( LeaderboardScoresDownloaded_t *pLeaderboardScoresDownloaded, bool bIOFailure );
		CCallResult<Leaderboard, LeaderboardScoresDownloaded_t> m_callResultDownloadEntries;

		SteamLeaderboard_t GetLeaderboardHandle( int iMapContext );
		void SetLeaderboardHandle( int iMapContext, SteamLeaderboard_t hLeaderboard );
		void GetLeaderboardName( int iMapContext, char *pszBuf, int iBufLen );

		MESSAGE_FUNC_PARAMS( OnPlayerDropDown, "PlayerDropDown", pKeyValues );

		void OpenPlayerFlyout( BaseModHybridButton *button, uint64 playerId, int x, int y );
		void ClosePlayerFlyout( void );
		uint64 m_flyoutPlayerId;

		CUtlMap< int, SteamLeaderboard_t > m_LeaderboardHandles;

		bool IsSearching( void ) { return m_bIsSearching; }

		bool m_bIsSearching;

		int m_iCurrentSpinnerValue;
		float m_flLastEngineSpinnerTime;
#endif

		// IFlyoutMenuListener
		virtual void OnNotifyChildFocus( vgui::Panel* child ) {}
		virtual void OnFlyoutMenuClose( vgui::Panel* flyTo ) {}
		virtual void OnFlyoutMenuCancelled( ) {}

		void InitializeDropDownControls();

#if defined( _X360 )
		CUtlVector< XUID > m_localPlayerXUIDs;
		CUtlVector< XUID > m_friendsXuids;
		XUSER_STATS_SPEC m_spec;
		char m_szLeaderboardViewNameSpec[128];

		int m_iCurrentMapBestTime;
		
		HANDLE m_hOverlapped;
		XOVERLAPPED m_xOverlapped;
		CUtlBuffer m_bufAsyncReadResults;
#endif

		typedef enum
		{
			QUERY_NONE = 0,

			QUERY_ENUM_FRIENDS,		// requested a list of XUIDS that are the player's xbox LIVE friends ( m_iAsyncQueryData = controller index )
			QUERY_STATS_FRIENDS,	// requested a chunk of data that is the player's friends stats ( m_iAsyncQueryData = controller index )
			QUERY_PLAYER_RANK,		// what global rank is the player?  ( m_iAsyncQueryData = controller index )
			QUERY_RANK_ONE_STATS,	// requested the world record time for the current map context  ( m_iAsyncQueryData = map context )
			QUERY_STATS_GLOBAL_PAGE,		// requested a page of global stats  ( m_iAsyncQueryData = page number )

			// Steam
			QUERY_FIND_LEADERBOARD,	// requesting a handle to a specific leaderboard ( m_iAsyncQueryData = unused )
		} AsyncQueryType;

		int m_iPrevSelectedRank;

		typedef struct 
		{
			AsyncQueryType asyncQueryType;
			int iData;							// same data as m_iAsyncQueryData

			void Init()
			{
				asyncQueryType = QUERY_NONE;
				iData = 0;
			}

		} PendingAsyncRequest_t;

		PendingAsyncRequest_t m_pendingRequest;	// The request that we last sent and haven't yet recieved

		CUtlQueue<PendingAsyncRequest_t> m_AsyncQueryQueue;

		int m_iAsyncQueryMapContext;	// The map context when the current query was sent

		int m_iMinPageLoaded;
		int m_iMaxPageLoaded;

		int m_iSelectRankWhenLoaded;
		bool m_bIgnoreSelectionChanges;

		CPanelAnimationVarAliasType( int, m_iTitleXOffset, "title_xpos", "0", "proportional_int" );
	};

};

#endif // __VLEADERBOARD_H__