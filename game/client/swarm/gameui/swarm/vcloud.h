//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCLOUD_H__
#define __VCLOUD_H__


#include "basemodui.h"
#include "VFlyoutMenu.h"

class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

class Cloud : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( Cloud, CBaseModFrame );

public:
	Cloud(vgui::Panel *parent, const char *panelName);
	~Cloud();

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
	void UpdateFooter();

	DropDownMenu		*m_drpCloud;

	BaseModHybridButton	*m_btnCancel;
	CNB_Header_Footer *m_pHeaderFooter;

	bool	m_bCloudEnabled;
};

};

#endif
