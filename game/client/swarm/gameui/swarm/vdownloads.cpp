//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VDownloads.h"
#include "VFooterPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
Downloads::Downloads(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );
	SetTitle("#L4D360UI_Downloads", false);

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled(true);

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( 0, FF_NONE, true );
	}

	m_ActiveControl = NULL;
}

//=============================================================================
Downloads::~Downloads()
{

}

//=============================================================================
void Downloads::OnCommand(const char *command)
{
}