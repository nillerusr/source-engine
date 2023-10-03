//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VDOWNLOADS_H__
#define __VDOWNLOADS_H__

#include "basemodui.h"

namespace BaseModUI {

class Downloads : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( Downloads, CBaseModFrame );

public:
	Downloads(vgui::Panel *parent, const char *panelName);
	~Downloads();

	void OnCommand(const char *command);
};

};

#endif
