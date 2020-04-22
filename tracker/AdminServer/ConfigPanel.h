//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CConfigPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CConfigPanel, vgui::Frame ); 
public:
	CConfigPanel(vgui::Panel *parent, bool autorefresh,bool savercon,int refreshtime,bool graphs, int graphsrefreshtime,bool getlogs);
	~CConfigPanel();

	void Run();
	
protected:
	// message handlers
	MESSAGE_FUNC_PTR( OnButtonToggled, "ButtonToggled", panel );
	MESSAGE_FUNC_PTR( OnRadioButtonChecked, "RadioButtonChecked", panel )
	{
		OnButtonToggled( panel );
	}

	// vgui overrides
	virtual void OnClose();
	virtual void OnCommand(const char *command);

private:

	void SetControlText(const char *textEntryName, const char *text);

	vgui::Button *m_pOkayButton;
	vgui::Button *m_pCloseButton;
	vgui::CheckButton *m_pRconCheckButton;
	vgui::CheckButton *m_pRefreshCheckButton;
	vgui::TextEntry *m_pRefreshTextEntry;
	vgui::CheckButton *m_pGraphsButton;
	vgui::TextEntry *m_pGraphsRefreshTimeTextEntry;
	vgui::CheckButton *m_pLogsButton;
};

#endif // CONFIGPANEL_H
