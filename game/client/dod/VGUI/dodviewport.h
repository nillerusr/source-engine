//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODVIEWPORT_H
#define DODVIEWPORT_H

#include "baseviewport.h"

using namespace vgui;

namespace vgui 
{
	class Panel;
}

class CDODTeamMenu;
class CDODClassMenu_Allies;
class CDODClassMenu_Axis;
class CDODSpectatorGUI;
class CDODClientScoreBoardDialog;
class CDODMenuBackground;


//==============================================================================
class DODViewport : public CBaseViewport
{

private:
	DECLARE_CLASS_SIMPLE( DODViewport, CBaseViewport );

public:

	IViewPortPanel* CreatePanelByName(const char *szPanelName);
	void CreateDefaultPanels( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		
	int GetDeathMessageStartHeight( void );

	// Never show the background
	virtual void ShowBackGround(bool bShow) { NULL; }
};


#endif // DODVIEWPORT_H
