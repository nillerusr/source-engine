//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VINGAMEDIFFICULTYSELECT_H__
#define __VINGAMEDIFFICULTYSELECT_H__

#include "basemodui.h"

namespace BaseModUI {

class SpinnerControl;

class InGameDifficultySelect : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( InGameDifficultySelect, CBaseModFrame );

public:
	InGameDifficultySelect(vgui::Panel *parent, const char *panelName);
	~InGameDifficultySelect();

	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void LoadLayout( void );
	void OnCommand(const char *command);
};

};

#endif // __VINGAMEDIFFICULTYSELECT_H__