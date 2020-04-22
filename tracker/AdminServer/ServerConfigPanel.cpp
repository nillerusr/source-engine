//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "ServerConfigPanel.h"

#include <vgui/ISystem.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerConfigPanel::CServerConfigPanel(vgui::Panel *parent, const char *name, const char *mod) : CVarListPropertyPage(parent, name)
{
	SetBounds(0, 0, 500, 170);
	LoadControlSettings("Admin\\ServerConfigPanel.res", "PLATFORM");

	// load our rules
	if (!LoadVarList("scripts/GameServerConfig.vdf"))
	{
		//!! no local mod info, need to load from server
		//!! always load from server if on a remote connection
	}

	m_flUpdateTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerConfigPanel::~CServerConfigPanel()
{

}

//-----------------------------------------------------------------------------
// Purpose: Reset data
//-----------------------------------------------------------------------------
void CServerConfigPanel::OnResetData()
{
	RefreshVarList();
	// update every minute
	m_flUpdateTime = (float)system()->GetFrameTime() + (60 * 1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if data needs to be refreshed
//-----------------------------------------------------------------------------
void CServerConfigPanel::OnThink()
{
	if (m_flUpdateTime < system()->GetFrameTime())
	{
		OnResetData();
	}
}
