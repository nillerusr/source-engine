#ifndef _INCLUDED_TOGGLE_EXITS_PANEL_H
#define _INCLUDED_TOGGLE_EXITS_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "RoomTemplate.h"
#include "vgui/RoomTemplatePanel.h"

class CToggleExitButton;

// ===============================================================================================================
//   This panel positions itself around a RoomTemplatePanel and offers buttons for manipulating exits
// ===============================================================================================================
class CToggleExitsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CToggleExitsPanel, vgui::Panel );
public:

	CToggleExitsPanel(Panel *parent, const char *name);
	virtual ~CToggleExitsPanel();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	
	void SetRoomTemplatePanel( CRoomTemplatePanel *pPanel, bool bForceUpdate=false );
	const CRoomTemplate* GetRoomTemplate() const;

private:
	CUtlVector<CToggleExitButton*> m_ExitButtons;
	CRoomTemplatePanel *m_pRoomTemplatePanel;
};


// ===============================================================================================================
//   This button toggles an exit on/off for a particular room template
// ===============================================================================================================
class CToggleExitButton : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CToggleExitButton, vgui::Panel );
public:

	CToggleExitButton(Panel *parent, const char *name);
	virtual ~CToggleExitButton();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnMouseReleased( vgui::MouseCode code );

	void SetExitDetails( int iTileX, int iTileY, ExitDirection_t exitDirection );

	int m_iTileX;
	int m_iTileY;
	ExitDirection_t m_ExitDirection;

private:
	CToggleExitsPanel* GetToggleExitsPanel();
};

#endif // _INCLUDED_TOGGLE_EXITS_PANEL_H