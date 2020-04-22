//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERCONFIGPANEL_H
#define SERVERCONFIGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "VarListPropertyPage.h"

//-----------------------------------------------------------------------------
// Purpose: Displays and allows modification to variable specific to the game
//-----------------------------------------------------------------------------
class CServerConfigPanel : public CVarListPropertyPage
{
public:
	CServerConfigPanel(vgui::Panel *parent, const char *name, const char *mod);
	~CServerConfigPanel();

protected:
	// variable updates
	virtual void OnResetData();
	virtual void OnThink();

private:
	float m_flUpdateTime;
	typedef CVarListPropertyPage BaseClass;
};

#endif // SERVERCONFIGPANEL_H