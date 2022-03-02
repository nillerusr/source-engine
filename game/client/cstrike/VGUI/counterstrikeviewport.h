//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COUNTERSTRIKEVIEWPORT_H
#define COUNTERSTRIKEVIEWPORT_H

#include "cs_shareddefs.h"
#include "baseviewport.h"


using namespace vgui;

namespace vgui 
{
	class Panel;
	class Label;
	class CBitmapImagePanel;
}

class CCSTeamMenu;
class CCSClassMenu;
class CCSSpectatorGUI;
class CCSClientScoreBoard;
class CBuyMenu;
class CCSClientScoreBoardDialog;



//==============================================================================
class CounterStrikeViewport : public CBaseViewport
{

private:
	DECLARE_CLASS_SIMPLE( CounterStrikeViewport, CBaseViewport );

public:

	IViewPortPanel* CreatePanelByName(const char *szPanelName);
	void CreateDefaultPanels( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager );
		
	int GetDeathMessageStartHeight( void );

	virtual void ShowBackGround(bool bShow) 
	{
		m_pBackGround->SetVisible( false );	// CS:S menus paint their own backgrounds...
	}

private:
	void CenterWindow( vgui::Frame *win );

};


#endif // COUNTERSTRIKEVIEWPORT_H
