//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODCLIENTSCOREBOARDDIALOG_H
#define DODCLIENTSCOREBOARDDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <clientscoreboarddialog.h>
#include "dod_shareddefs.h"

//-----------------------------------------------------------------------------
// Purpose: Game ScoreBoard
//-----------------------------------------------------------------------------
class CDODClientScoreBoardDialog : public CClientScoreBoardDialog
{
private:
	DECLARE_CLASS_SIMPLE( CDODClientScoreBoardDialog, CClientScoreBoardDialog );
	
public:
	CDODClientScoreBoardDialog( IViewPort *pViewPort );
	~CDODClientScoreBoardDialog();

	virtual void Reset();
	virtual void Update();

	virtual void PaintBackground();
	virtual void PaintBorder();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void ShowPanel( bool bShow );

private:
	void InitPlayerList( vgui::SectionedListPanel *pPlayerList, int teamNumber );
	void UpdateTeamInfo();
	void UpdatePlayerList();
	void UpdateSpectatorList();
	bool GetPlayerScoreInfo( int playerIndex, KeyValues *outPlayerInfo );

	bool ShouldShowAsSpectator( int iPlayerIndex );
	void FireGameEvent( IGameEvent *event );

	static bool DODPlayerSortFunc( vgui::SectionedListPanel *list, int itemID1, int itemID2 );

	Color m_bgColor;
	Color m_borderColor;

	int m_iImageDead;
	int m_iImageDominated;
	int m_iImageNemesis;

	// player lists
	vgui::SectionedListPanel *m_pPlayerListAllies;
	vgui::SectionedListPanel *m_pPlayerListAxis;

	CPanelAnimationVarAliasType( int, m_iStatusWidth, "status_width", "35", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iFragsWidth, "frags_width", "30", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iProportionalAvatarWidth, "avatar_width_prop", "34", "proportional_int" );

	vgui::Label	*m_pAllies_PlayerCount;
	vgui::Label	*m_pAllies_Score;
	vgui::Label	*m_pAllies_Kills;
	vgui::Label	*m_pAllies_Deaths;
	vgui::Label	*m_pAllies_Ping;

	vgui::Label	*m_pAxis_PlayerCount;
	vgui::Label	*m_pAxis_Score;
	vgui::Label	*m_pAxis_Kills;
	vgui::Label	*m_pAxis_Deaths;
	vgui::Label	*m_pAxis_Ping;
};


#endif // DODCLIENTSCOREBOARDDIALOG_H
