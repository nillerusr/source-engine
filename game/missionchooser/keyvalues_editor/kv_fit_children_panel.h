#ifndef _INCLUDED_KV_FIT_CHILDREN_PANEL_H
#define _INCLUDED_KV_FIT_CHILDREN_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "kv_editor_base_panel.h"

class FitChildrenLessFunc
{
public:
	bool Less( CKV_Editor_Base_Panel* const &pRoomTemplateLHS, CKV_Editor_Base_Panel* const &pRoomTemplateRHS, void *pCtx );
};

// a vgui panel that resizes to fit designated children, with some border
class CKV_Fit_Children_Panel : public CKV_Editor_Base_Panel
{
	DECLARE_CLASS_SIMPLE( CKV_Fit_Children_Panel, CKV_Editor_Base_Panel );

public:
	CKV_Fit_Children_Panel( Panel *parent, const char *name);

	virtual void PerformLayout();

	void AddAutoPositionPanel( CKV_Editor_Base_Panel *pPanel );
	void RemoveAutoPositionPanel( CKV_Editor_Base_Panel *pPanel );
	void ClearAutoPositionPanels();

	bool IsNodePanel( vgui::Panel *pPanel );	

	// list of panels we should position in a list inside us
	CUtlSortVector<CKV_Editor_Base_Panel*, FitChildrenLessFunc> m_AutoPositionPanels;

	int m_iAutoPositionStartY;
	CPanelAnimationVarAliasType( int, m_iBorder, "border", "5", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSpacing, "spacing", "1", "proportional_int" );
};


#endif // _INCLUDED_KV_FIT_CHILDREN_PANEL_H