//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSTEAMCLOUDCONFIRMATION_H__
#define __VSTEAMCLOUDCONFIRMATION_H__

#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

#include "basemodui.h"

namespace BaseModUI {

class SteamCloudConfirmation : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( SteamCloudConfirmation, CBaseModFrame );
public:
	SteamCloudConfirmation(vgui::Panel *parent, const char *panelName);
	~SteamCloudConfirmation();

protected:
	virtual void OnCommand(const char *command);
	virtual void OnThink();

private:
	vgui::CvarToggleCheckButton<CGameUIConVarRef> *m_pSteamCloudCheckBox;
};

};

#endif // __VSTEAMCLOUDCONFIRMATION_H__
