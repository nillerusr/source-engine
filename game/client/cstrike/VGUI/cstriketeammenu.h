//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTEAMMENU_H
#define CSTEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <teammenu.h>

//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CCSTeamMenu : public CTeamMenu
{
private:
	DECLARE_CLASS_SIMPLE( CCSTeamMenu, CTeamMenu );
		
public:
	CCSTeamMenu(IViewPort *pViewPort);
	~CCSTeamMenu();

	void Update();
	void ShowPanel( bool bShow );
	virtual void SetVisible(bool state);

private:
	enum { NUM_TEAMS = 3 };

	// VGUI2 override
	virtual void OnCommand( const char *command);
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	// helper functions
	void SetVisibleButton(const char *textEntryName, bool state);

	bool m_bVIPMap;

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};

#endif // CSTEAMMENU_H
