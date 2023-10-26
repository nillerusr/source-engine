//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGETLEGACYDATA_H__
#define __VGETLEGACYDATA_H__

#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

#include "basemodui.h"

namespace BaseModUI {

class GetLegacyData : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( GetLegacyData, CBaseModFrame );
public:
	GetLegacyData(vgui::Panel *parent, const char *panelName);
	~GetLegacyData();
	static bool CheckAndSeeIfShouldShow();
	static bool IsInstalled();
	static bool IsInstalling();

protected:
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);
	virtual void OnThink();

	vgui::Label *m_LblDesc;

};

};

#endif // __VADDONASSOCIATION_H__
