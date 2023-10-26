#ifndef _INCLUDED_ASW_VGUI_SKIP_INTRO_H
#define _INCLUDED_ASW_VGUI_SKIP_INTRO_H

#include <vgui/IScheme.h>
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "asw_vgui_ingame_panel.h"

// invisible full screen panel used to detect a mouse click during the intro to skip straight to the campaign screen

class CASW_VGUI_Skip_Intro : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Skip_Intro, vgui::Panel );

	CASW_VGUI_Skip_Intro( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Skip_Intro();
		
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
};

#endif /* _INCLUDED_ASW_VGUI_SKIP_INTRO_H */