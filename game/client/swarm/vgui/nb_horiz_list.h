#ifndef _INCLUDED_NB_HORIZ_LIST_H
#define _INCLUDED_NB_HORIZ_LIST_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "vgui_controls/phandle.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::ImagePanel;
class CBitmapButton;
// == MANAGED_CLASS_DECLARATIONS_END ==
class vgui::ScrollBar;

class CNB_Horiz_List : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Horiz_List, vgui::EditablePanel );
public:
	CNB_Horiz_List( vgui::Panel *parent, const char *name );
	virtual ~CNB_Horiz_List();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void AddEntry( vgui::Panel *pPanel );
	vgui::Panel* GetHighlightedEntry();

	void SetHighlight( int nEntryIndex );
	bool ChangeScrollValue( int nChange );
	bool MouseOverScrollbar( void ) const;

	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::ImagePanel	*m_pBackgroundImage;
	vgui::ImagePanel	*m_pForegroundImage;
	CBitmapButton	*m_pLeftArrowButton;
	CBitmapButton	*m_pRightArrowButton;	
	// == MANAGED_MEMBER_POINTERS_END ==

	vgui::ScrollBar *m_pHorizScrollBar;
	vgui::Panel *m_pChildContainer;

	CUtlVector<vgui::PHandle> m_Entries;
	int m_nHighlightedEntry;
	float m_flContainerPos;
	float m_flContainerTargetPos;
	bool m_bInitialPlacement;	
	bool m_bShowScrollBar;
	bool m_bShowArrows;
	bool m_bHighlightOnMouseOver;
	bool m_bAutoScrollChange;
	int m_nEntryWide;

	float m_fScrollVelocity;
	float m_fScrollChange;
};

#endif // _INCLUDED_NB_HORIZ_LIST_H




