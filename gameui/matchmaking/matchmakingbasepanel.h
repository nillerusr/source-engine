//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Serves as the base panel for the entire matchmaking UI
//
//=============================================================================//

#ifndef MATCHMAKINGBASEPANEL_H
#define MATCHMAKINGBASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"
#include "utlstack.h"
#include "const.h"

enum EGameType
{
	GAMETYPE_RANKED_MATCH,
	GAMETYPE_STANDARD_MATCH,
	GAMETYPE_SYSTEMLINK_MATCH,
};

//----------------------------
// CMatchmakingBasePanel
//----------------------------
class CMatchmakingBasePanel : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CMatchmakingBasePanel, CBaseDialog ); 

public:
	CMatchmakingBasePanel(vgui::Panel *parent);
	~CMatchmakingBasePanel();

	virtual void	OnCommand( const char *pCommand );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	Activate();

	void			SessionNotification( const int notification, const int param = 0 );
	void			SystemNotification( const int notification );
	void			UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost );
	void			SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping );

	void			OnLevelLoadingStarted();
	void			OnLevelLoadingFinished();
	void			CloseGameDialogs( bool bActivateNext = true );
	void			CloseAllDialogs( bool bActivateNext = true );
	void			CloseBaseDialogs( void );

	void			SetFooterButtons( CBaseDialog *pOwner, KeyValues *pData, int nButtonGap = -1 );
	void			ShowFooter( bool bShown );
	void			SetFooterButtonVisible( const char *pszText, bool bVisible );

	uint			GetGameType( void ) { return m_nGameType; }

	MESSAGE_FUNC_CHARPTR( LoadMap, "LoadMap", mapname );
private:
	void			OnOpenWelcomeDialog();
	void			OnOpenPauseDialog();
	void			OnOpenRankingsDialog();
	void			OnOpenSystemLinkDialog();
	void			OnOpenPlayerMatchDialog();
	void			OnOpenRankedMatchDialog();
    void			OnOpenAchievementsDialog();

    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Specific code for CS Achievements Display
    //=============================================================================

    // $TODO(HPE): Move this to a game-specific location
    void			OnOpenCSAchievementsDialog();

    //=============================================================================
    // HPE_END
    //=============================================================================

    void			OnOpenLeaderboardDialog( const char *pResourceName );
	void			OnOpenSessionOptionsDialog( const char *pResourceName );
	void			OnOpenSessionLobbyDialog( const char *pResourceName );
	void			OnOpenSessionBrowserDialog( const char *pResourceName );

	void			LoadSessionProperties();
	bool			ValidateSigninAndStorage( bool bOnlineRequired, const char *pIssuingCommand );
	void			CenterDialog( vgui::PHandle dlg );
	void			PushDialog( vgui::DHANDLE< CBaseDialog > &hDialog );
	void			PopDialog( bool bActivateNext = true );

	vgui::DHANDLE< CBaseDialog > m_hWelcomeDialog;
	vgui::DHANDLE< CBaseDialog > m_hPauseDialog;
	vgui::DHANDLE< CBaseDialog > m_hStatsDialog;
	vgui::DHANDLE< CBaseDialog > m_hRankingsDialog;
	vgui::DHANDLE< CBaseDialog > m_hLeaderboardDialog;
	vgui::DHANDLE< CBaseDialog > m_hSystemLinkDialog;
	vgui::DHANDLE< CBaseDialog > m_hPlayerMatchDialog;
	vgui::DHANDLE< CBaseDialog > m_hRankedMatchDialog;
	vgui::DHANDLE< CBaseDialog > m_hAchievementsDialog;
	vgui::DHANDLE< CBaseDialog > m_hSessionOptionsDialog;
	vgui::DHANDLE< CBaseDialog > m_hSessionLobbyDialog;
	vgui::DHANDLE< CBaseDialog > m_hSessionBrowserDialog;

	CUtlStack< vgui::DHANDLE< CBaseDialog > > m_DialogStack;

	uint		m_nSessionType;
	uint		m_nGameType;
	bool		m_bPlayingOnline;
	char		m_szMapLoadName[MAX_MAP_NAME];
	KeyValues	*m_pSessionKeys;

	CFooterPanel	*m_pFooter;
};


#endif // MATCHMAKINGBASEPANEL_H
