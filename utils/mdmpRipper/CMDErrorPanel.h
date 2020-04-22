//========= Copyright Valve Corporation, All rights reserved. ============//
#include "mdmpRipper.h"
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Frame.h"
#include "CMDDetailPanel.h"

#ifndef MDERRORPANEL_H
#define MDERRORPANEL_H

#ifdef _WIN32
#pragma once
#endif

using namespace vgui;

class CMDErrorPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMDErrorPanel, vgui::EditablePanel );

	vgui::ListPanel *m_pTokenList;	
	vgui::TextEntry *m_pMaxResults;
	vgui::TextEntry *m_pQueryCounts;

	CUtlVector<HANDLE> m_MiniDumpHandles;

public:
	CMDErrorPanel( vgui::Panel *pParent, const char *pName );
	virtual void OnCommand( const char *pCommand );
	virtual void Close();
	void DownloadMinidumps();
	void CheckModules();
	void NewQuery();
	void DetailScreen();
};

#endif // MDERRORPANEL_H
