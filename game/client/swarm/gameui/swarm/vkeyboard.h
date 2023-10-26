//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VKEYBOARD_H__
#define __VKEYBOARD_H__


#include "basemodui.h"
#include "VFlyoutMenu.h"

class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

class VKeyboard : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( VKeyboard, CBaseModFrame );

public:
	VKeyboard(vgui::Panel *parent, const char *panelName);
	~VKeyboard();

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
#ifndef _X360
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif

	virtual void OnCommand( const char *command );

private:
	void UpdateFooter();

	CNB_Header_Footer *m_pHeaderFooter;
	class COptionsSubKeyboard *m_pOptionsSubKeyboard;

	bool	m_bCloudEnabled;
};

};

#endif
