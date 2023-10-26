//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VKEYBOARDMOUSE_H__
#define __VKEYBOARDMOUSE_H__


#include "basemodui.h"
#include "VFlyoutMenu.h"


namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

class KeyboardMouse : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( KeyboardMouse, CBaseModFrame );

public:
	KeyboardMouse(vgui::Panel *parent, const char *panelName);
	~KeyboardMouse();

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	Panel* NavigateBack();

protected:
	virtual void Activate();
	virtual void OnThink();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );

private:
	void UpdateFooter( bool bEnableCloud );	

	BaseModHybridButton	*m_btnEditBindings;
	DropDownMenu		*m_drpMouseYInvert;
	DropDownMenu		*m_drpMouseFilter;
	SliderControl		*m_sldMouseSensitivity;
	DropDownMenu		*m_drpDeveloperConsole;
	DropDownMenu		*m_drpGamepadEnable;
	SliderControl		*m_sldGamepadHSensitivity;
	SliderControl		*m_sldGamepadVSensitivity;
	DropDownMenu		*m_drpGamepadYInvert;
	DropDownMenu		*m_drpGamepadSwapSticks;

	BaseModHybridButton	*m_btnCancel;
};

};

#endif // __VKEYBOARDMOUSE_H__