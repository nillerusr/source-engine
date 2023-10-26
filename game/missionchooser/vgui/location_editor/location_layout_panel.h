#ifndef _INCLUDED_LOCATION_LAYOUT_PANEL_H
#define _INCLUDED_LOCATION_LAYOUT_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
};
class CLocation_Editor_Frame;
class CLocation_Layout_Panel;

// ====================================================================================================
// a panel showing an individual location
class CLocation_Panel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CLocation_Panel, vgui::Panel );
public:
	CLocation_Panel( Panel *parent, const char *name );
	virtual ~CLocation_Panel();

	virtual void OnThink();
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( vgui::MouseCode code );
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	void SetLocationID( int i );

	int m_iLocationID;

private:
	bool IsCursorOverHex();
	void UpdateHexColor();

	vgui::ImagePanel *m_pHexImage;
	vgui::Label *m_pIDLabel;
	KeyValues		  *m_actionMessage;
	CLocation_Layout_Panel *m_pLayoutPanel;
	int m_iLastXPos;
	int m_iLastYPos;

	// dragging hexes around
	bool m_bDragging;
	Vector2D m_MouseOffset;
};

//-----------------------------------------------------------------------------
// Purpose: Shows all location hexes
//-----------------------------------------------------------------------------
class CLocation_Layout_Panel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CLocation_Layout_Panel, vgui::Panel );

public:
	CLocation_Layout_Panel( Panel *parent, vgui::Panel *pActionTarget, const char *name );
	virtual ~CLocation_Layout_Panel();

	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	void CreateLocationPanels();
	CLocation_Editor_Frame* GetLocationEditorFrame() { return m_pEditorFrame; }

	CUtlVector<CLocation_Panel*> m_LocationPanels;
	vgui::Panel *m_pActionTarget;
	vgui::ImagePanel *m_pBgImage;
	CLocation_Editor_Frame* m_pEditorFrame;
};

#endif // _INCLUDED_LOCATION_LAYOUT_PANEL_H