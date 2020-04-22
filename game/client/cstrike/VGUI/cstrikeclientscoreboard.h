//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRIKECLIENTSCOREBOARDDIALOG_H
#define CSTRIKECLIENTSCOREBOARDDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <clientscoreboarddialog.h>
#include <vgui_controls/ImagePanel.h>
#include "cs_shareddefs.h"
#include <vgui_controls/Frame.h>
#include "vgui_avatarimage.h"


const int cMaxScoreLines = 32;  // This value must be > 2


//-----------------------------------------------------------------------------
// Purpose: Game ScoreBoard
//-----------------------------------------------------------------------------
class CCSClientScoreBoardDialog : public CClientScoreBoardDialog
{
private:
    DECLARE_CLASS_SIMPLE( CCSClientScoreBoardDialog, CClientScoreBoardDialog );

public:
    CCSClientScoreBoardDialog( IViewPort *pViewPort );
    ~CCSClientScoreBoardDialog();

    virtual void Update();

    // vgui overrides for rounded corner background
    void UpdateMvpElements();
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void ResetFromGameOverState();

    // [tj] Hook in here to hide other UI
    virtual void ShowPanel( bool state ); 

    // [tj] So we can do processing every frame
    virtual void OnThink();

protected:

	struct PlayerScoreInfo
	{
		const char*		szName;
		const char*		szClanTag;
		int				playerIndex;
		int				frags;
		int				deaths;
		int				ping;
		const char*		szStatus;
		bool			bStatusPlayerColor;
	};

	struct PlayerDisplay
	{
		vgui::Label*			pNameLabel;
		vgui::Label*			pClanLabel;
		vgui::Label*			pScoreLabel;
		vgui::Label*			pDeathsLabel;
		vgui::Label*			pPingLabel;
		vgui::Label*			pMVPCountLabel;
		CAvatarImagePanel*		pAvatar;
		vgui::ImagePanel*		pStatusImage;
		vgui::ImagePanel*		pMVPImage;
		vgui::ImagePanel*		pSelect;
	};

	struct TeamDisplayInfo
	{
		Color					playerDataColor;
		Color					playerClanColor;
		PlayerDisplay			playerDisplay[cMaxScoreLines];
		CUtlVector<PlayerScoreInfo*>  playerScores;		// For sorting team members outside of the listboxes
		int						scoreAreaInnerHeight;
		int						scoreAreaLineHeight;
		int						scoreAreaLinePreferredLeading;
		int						scoreAreaStartY;
		int						scoreAreaMinX;
		int						scoreAreaMaxX;
		int						maxPlayersVisible;
	};

	bool GetPlayerScoreInfo( int playerIndex, PlayerScoreInfo& playerScoreInfo );
	void UpdateTeamPlayerDisplay( TeamDisplayInfo& teamDisplay );
	void SetupTeamDisplay( TeamDisplayInfo& teamDisplay, const char* szTeamPrefix );

	void UpdateTeamInfo();
	void UpdatePlayerList();

	bool ForceLocalPlayerVisible( TeamDisplayInfo& teamDisplay );
	void UpdateSpectatorList();
	void UpdateHLTVList( void );
	void UpdateMatchEndText();

	bool ShouldShowAsSpectator( int iPlayerIndex );
	void FireGameEvent( IGameEvent *event );

	void UpdatePlayerColors( void );
	void AdjustFontToFit( const char *pString, vgui::Label *pLabel );

	static int PlayerSortFunction( PlayerScoreInfo* const* pPS1, PlayerScoreInfo* const* pPS2 );

private:
    vgui::HFont					m_listItemFont;
    vgui::HFont                 m_listItemFontSmaller;
    vgui::HFont                 m_listItemFontSmallest;
    vgui::HFont                 m_MVPFont;

    int			m_iImageDead;
    int			m_iImageMVP; // Not used in the section list explicitly.  Drawn over it
    int			m_iImageDominated;
    int			m_iImageNemesis;
    int			m_iImageBomb;
    int			m_iImageVIP;
    int			m_iImageFriend;
    int         m_iImageNemesisDead;
    int         m_iImageDominationDead;

	Color			m_DeadPlayerDataColor;
	Color			m_PlayerDataBgColor;
	Color			m_DeadPlayerClanColor;

	vgui::Label*	m_pWinConditionLabel;
	vgui::Label*	m_pClockLabel;
	vgui::Label*	m_pLabelMapName;
	vgui::Label*	m_pServerLabel;

    bool			m_gameOver;

    wchar_t			m_pMapName[256];
    wchar_t			m_pServerName[256];
    wchar_t			m_pStatsEnabled[256];
    wchar_t			m_pStatsDisabled[256];

    int m_LocalPlayerItemID;
	int m_MVPXOffset;

	TeamDisplayInfo	m_teamDisplayT;
	TeamDisplayInfo	m_teamDisplayCT;
};


#endif // CSTRIKECLIENTSCOREBOARDDIALOG_H
