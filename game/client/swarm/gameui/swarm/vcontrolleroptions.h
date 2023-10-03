//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCONTROLLEROPTIONS_H__
#define __VCONTROLLEROPTIONS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class SliderControl;
class DropDownMenu;

class ControllerOptions : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( ControllerOptions, CBaseModFrame );

public:
	ControllerOptions(vgui::Panel *parent, const char *panelName);
	~ControllerOptions();

	void Activate();
	void ResetControlValues( void );
	void OnThink();

	void ResetToDefaults( void );

	void ChangeToDuckMode( int iDuckMode );

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	Panel* NavigateBack();

protected:
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	void UpdateFooter();

	int m_iActiveUserSlot;

	SliderControl		*m_pVerticalSensitivity;
	SliderControl		*m_pHorizontalSensitivity;
	DropDownMenu		*m_pLookType;
	DropDownMenu		*m_pDuckMode;
	BaseModHybridButton	*m_pEditButtons;
	BaseModHybridButton	*m_pEditSticks;
	wchar_t				m_Title[128];

	bool m_bDirty;
	bool m_bNeedsActivate;

	int m_nResetControlValuesTicks; // used to delay polling the values until we've flushed the command buffer 
};

};

#endif
