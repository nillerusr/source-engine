//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ClickableTabbedPanel.h"
#include <vgui/KeyValues.h>


using namespace vgui;

CClickableTabbedPanel::CClickableTabbedPanel(vgui2::Panel *parent, const char *panelName) : vgui2::PropertySheet(parent,panelName)
{

	//PropertySheet::onTabPressed
	
}

CClickableTabbedPanel::~CClickableTabbedPanel()
{


}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
/*void CClickableTabbedPanel::onMousePressed(MouseCode code)
{
		// check for context menu open
	if (code == MOUSE_RIGHT)
	{
			postActionSignal(new KeyValues("OpenContextMenu", "itemID", -1));
	}

}*/
