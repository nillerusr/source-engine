//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SESSIONLOBBYDIALOG_H
#define SESSIONLOBBYDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Simple menu to choose a matchmaking session type
//-----------------------------------------------------------------------------
class CSessionLobbyDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CSessionLobbyDialog, CBaseDialog ); 

public:
	CSessionLobbyDialog( vgui::Panel *parent );
	~CSessionLobbyDialog();

	virtual void	OnCommand( const char *pCommand );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *inResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	void			SetDialogKeys( KeyValues *pKeys );
	void			UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost );
	void			UpdateCountdown( int seconds );

private:
	void			ActivateNextMenu();
	void			ActivatePreviousMenu();
	void			UpdatePlayerCountDisplay( int iTeam );
	void			SetLobbyReadyState( int nPlayersNeeded );
	void			PositionTeamInfos();

	void			SetTextFromKeyvalues( CPropertyLabel *pLabel );

	void			SetStartGame( bool bStartGame );

	enum
	{
		BLUE_TEAM_LOBBY,
		RED_TEAM_LOBBY,
		TOTAL_LOBBY_TEAMS,
	};

	CDialogMenu			m_Menus[TOTAL_LOBBY_TEAMS];

	vgui::Panel			*m_pLobbyStateBg;
	CPropertyLabel		*m_pLobbyStateLabel;
	CPropertyLabel		*m_pLobbyStateIcon;
	CPropertyLabel		*m_pHostLabel;
	vgui::EditablePanel	*m_pHostOptionsPanel;

	KeyValues			*m_pDialogKeys;

	CScenarioInfoPanel	*m_pScenarioInfo;
	CScenarioInfoPanel	*m_pTeamInfos[TOTAL_LOBBY_TEAMS];

	int					m_nMinInfoHeight[TOTAL_LOBBY_TEAMS];

	uint64				m_nHostId;
	bool				m_bReady;
	bool				m_bHostLobby;
	bool				m_bCenterOnScreen;
	int					m_iLocalTeam;
	int					m_iActiveMenu;
	int					m_nImageBorderWidth;
	int					m_nTeamspacing;
	char				m_szCommand[MAX_COMMAND_LEN];

	bool				m_bStartingGame;
	int					m_nLastPlayersNeeded;
};


#endif // SESSIONLOBBYDIALOG_H
