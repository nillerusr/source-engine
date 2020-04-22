//========= Copyright Valve Corporation, All rights reserved. ============//
#include "mdmpRipper.h"
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/HTML.h"

#ifndef MDDETAILPANEL_H
#define MDDETAILPANEL_H

#ifdef _WIN32
#pragma once
#endif

using namespace vgui;

class CMDDetailPanel : public vgui::EditablePanel
{
	vgui::HTML *m_pDetailWindow;	
	DECLARE_CLASS_SIMPLE( CMDDetailPanel, vgui::EditablePanel );
public:
	CMDDetailPanel( vgui::Panel *pParent, const char *pName );
	virtual void OpenURL( const char *url );	
	virtual void OnCommand( const char *pCommand );
	virtual void Close();
};

#endif // MDDETAILPANEL_H
