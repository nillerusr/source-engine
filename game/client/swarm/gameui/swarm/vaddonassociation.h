//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VADDONASSOCIATION_H__
#define __VADDONASSOCIATION_H__

#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

#include "basemodui.h"

namespace BaseModUI {

class AddonAssociation : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( AddonAssociation, CBaseModFrame );
public:
	enum EAssociation
	{
		kAssociation_None,
		kAssociation_Other,
		kAssociation_Ok
	};
	
	AddonAssociation(vgui::Panel *parent, const char *panelName);
	~AddonAssociation();

	static EAssociation VPKAssociation();
	static bool CheckAndSeeIfShouldShow();

protected:
	virtual void OnCommand(const char *command);
	virtual void OnThink();

private:
	vgui::CvarToggleCheckButton<CGameUIConVarRef> *m_pDoNotAskForAssociation;

};

};

#endif // __VADDONASSOCIATION_H__
