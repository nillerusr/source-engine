//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VVOTEOPTIONS_H__
#define __VVOTEOPTIONS_H__

#include "basemodui.h"

namespace BaseModUI {

class VoteOptions : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( VoteOptions, CBaseModFrame );

public:
	VoteOptions(vgui::Panel *parent, const char *panelName);
	~VoteOptions();

	void OnCommand(const char *command);

private:
	vgui::Button* m_BtnBootPlayer;
	vgui::Button* m_BtnChangeScenario;
	vgui::Button* m_BtnChangeDifficulty;
	vgui::Button* m_BtnRestartScenario;
};

};

#endif // __VVOTEOPTIONS_H__