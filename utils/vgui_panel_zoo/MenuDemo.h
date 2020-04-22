//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Controls.h>

#include <vgui_controls/Menu.h> 
#include <vgui_controls/MenuButton.h>
#include <Keyvalues.h>

using namespace vgui;


class MenuDemo: public DemoPage
{
	public:
		MenuDemo(Panel *parent, const char *name);
		~MenuDemo();
		void InitMenus();
		
		void OnMaggie();
		
	protected:
		// Menu that opens when button is pressed
		Menu *m_pMenu;

		// Button to trigger the menu
		MenuButton *m_pMenuButton;
		
	private:
		// explain this
		DECLARE_PANELMAP();
				
};