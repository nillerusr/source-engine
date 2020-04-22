//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef RAWLOGPANEL_H
#define RAWLOGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

#include "RemoteServer.h"

namespace vgui
{
	class CConsolePanel;
}
//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CRawLogPanel : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CRawLogPanel, vgui::PropertyPage );
public:
	CRawLogPanel(vgui::Panel *parent, const char *name);
	~CRawLogPanel();

	// property page handlers
	virtual void OnPageShow();
	virtual void OnPageHide();
	void DoInsertString(const char *str);

private:
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

	vgui::CConsolePanel *m_pConsole;
};

#endif // RAWLOGPANEL_H