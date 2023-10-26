//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONSBUTTONS_H__
#define __VCONTROLLEROPTIONSBUTTONS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class SliderControl;
class DropDownMenu;

class ControllerOptionsButtons : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( ControllerOptionsButtons, CBaseModFrame );

public:
	ControllerOptionsButtons(vgui::Panel *parent, const char *panelName);
	~ControllerOptionsButtons();

	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void Activate();
	void OnThink();
	void RecalculateBindingLabels();

	void OnCommand(const char *command);

	virtual void OnKeyCodePressed(vgui::KeyCode code);

	Panel* NavigateBack();

	MESSAGE_FUNC_HANDLE( OnHybridButtonNavigatedTo, "OnHybridButtonNavigatedTo", button );

private:
	void UpdateFooter();

	int m_iActiveUserSlot;
	bool m_bActivateOnFirstThink;
	EditablePanel *m_pLabelContainer;
	int m_nRecalculateLabelsTicks; // used to delay polling the values until we've flushed the command buffer 
};

};

#endif
