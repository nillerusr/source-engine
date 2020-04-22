//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFCTEAMMENU_H
#define TFCTEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <teammenu.h>

//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CTFCTeamMenu : public CTeamMenu
{
private:
	DECLARE_CLASS_SIMPLE( CTFCTeamMenu, CTeamMenu );
		
public:
	CTFCTeamMenu(IViewPort *pViewPort);
	~CTFCTeamMenu();

	virtual void ApplySettings( KeyValues *inResourceData );

	void Update();
	void ShowPanel( bool bShow );

private:
	enum { NUM_TEAMS = 3 };

	// VGUI2 override
	void OnCommand( const char *command);
	// helper functions
	void SetVisibleButton(const char *textEntryName, bool state);
};

#endif // TFCTEAMMENU_H
