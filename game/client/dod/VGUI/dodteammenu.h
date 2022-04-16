//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODTEAMMENU_H
#define DODTEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include "teammenu.h"
#include "dodmenubackground.h"
#include "dodbutton.h"
#include "dodmouseoverpanelbutton.h"
#include "IconPanel.h"

//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CDODTeamMenu : public CTeamMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODTeamMenu, CTeamMenu );
		
public:
	CDODTeamMenu(IViewPort *pViewPort);
	~CDODTeamMenu();

	virtual void Update();
	virtual void OnTick();
	void ShowPanel( bool bShow );
	virtual void SetVisible( bool state );

	virtual void PaintBackground();
	virtual Panel *CreateControlByName( const char *controlName );

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual void OnKeyCodePressed(KeyCode code);

private:
	enum { NUM_TEAMS = 3 };

	MESSAGE_FUNC_CHARPTR( OnShowPage, "ShowPage", page );

	// VGUI2 override
	void OnCommand( const char *command);
	// helper functions
	void SetVisibleButton(const char *textEntryName, bool state);

	CDODMenuBackground   *m_pBackground;

	vgui::EditablePanel *m_pPanel;

	CDODMouseOverButton<vgui::EditablePanel> *m_pFirstButton;

	int m_iLastPlayerCount;

	int m_iActiveTeam;

	ButtonCode_t m_iTeamMenuKey;
};

#endif // DODTEAMMENU_H
