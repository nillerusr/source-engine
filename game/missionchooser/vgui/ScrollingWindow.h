#ifndef TILEGEN_SCROLLINGWINDOW_H
#define TILEGEN_SCROLLINGWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"

namespace vgui
{
	class Scrollbar;
	class Menu;
};

// a window shows part of a larger child panel
//  scrollbars allow the user to scroll their view of that panel about
class CScrollingWindow : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CScrollingWindow, vgui::Panel );

public:

	CScrollingWindow(Panel *parent, const char *name);
	virtual ~CScrollingWindow();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void SetChildPanel(vgui::Panel *pPanel);	// set the panel visible inside our scrolling window
	void MoveToCenter();
	void MoveToLowerCenter();
	void MoveToTopLeft();
	virtual void OnMouseWheeled(int delta);
	virtual void OnThink();
	
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );

	vgui::ScrollBar *m_pHorizScrollbar;
	vgui::ScrollBar *m_pVertScrollbar;
	vgui::Panel *m_pView;	// panel we position inside the scrollbars and see our child panel through
	vgui::DHANDLE< vgui::Panel >	m_hChildPanel;
	int m_iChildTall;
	int m_iChildWide;
};

#endif TILEGEN_SCROLLINGWINDOW_H