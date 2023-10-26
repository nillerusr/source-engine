//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONSSTICKS_H__
#define __VCONTROLLEROPTIONSSTICKS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class SliderControl;
class DropDownMenu;

class ControllerOptionsSticks : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( ControllerOptionsSticks, CBaseModFrame );

public:
	ControllerOptionsSticks(vgui::Panel *parent, const char *panelName);
	~ControllerOptionsSticks();

	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void Activate();
	void OnThink();

	void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);

	Panel* NavigateBack();

	void UpdateFooter();
	void UpdateHelpText();
	void UpdateButtonNames();

	MESSAGE_FUNC_HANDLE( OnHybridButtonNavigatedTo, "OnHybridButtonNavigatedTo", button );

private:
	int m_iActiveUserSlot;
	bool m_bActivateOnFirstThink;
};

};

#endif
