//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGAMEOPTIONS_H__
#define __VGAMEOPTIONS_H__

#include "basemodui.h"

namespace BaseModUI {

class SpinnerControl;

class GameOptions : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( GameOptions, CBaseModFrame );

public:
	GameOptions(vgui::Panel *parent, const char *panelName);
	~GameOptions();

	void OnCommand(const char *command);

	void Activate();

	MESSAGE_FUNC_CHARPTR( OnSetCurrentItem, "OnSetCurrentItem", panelName );

private:
	SpinnerControl* m_SpnInvertYAxis;
	SpinnerControl* m_SpnVibration;
	SpinnerControl* m_SpnAutoCrouch;
	SpinnerControl* m_SpnLookSensitivity;
};

}

#endif
