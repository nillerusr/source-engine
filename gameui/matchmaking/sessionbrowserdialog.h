//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Present a list of sessions from which the player can choose a game to join.
//
//=====================================================================================//

#ifndef SESSIONBROWSERDIALOG_H
#define SESSIONBROWSERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

class CSessionBrowserDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CSessionBrowserDialog, CBaseDialog ); 

public:
	CSessionBrowserDialog( vgui::Panel *parent, KeyValues *pDialogKeys );
	~CSessionBrowserDialog();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *inResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();

	virtual void	SwapMenuItems( int iOne, int iTwo );

	void			UpdateScenarioDisplay( void );
	void			SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping );

	KeyValues	*m_pDialogKeys;

	CUtlVector< CScenarioInfoPanel* >	m_pScenarioInfos;
	CUtlVector< int >					m_ScenarioIndices;
	CUtlVector< int >					m_SearchIndices;
	CUtlVector< int >					m_GameStates;
	CUtlVector< int >					m_GameTimes;
	CUtlVector< XUID >					m_XUIDs;
};


#endif // SESSIONBROWSERDIALOG_H
