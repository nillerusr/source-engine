#ifndef _INCLUDED_KV_NODE_PANEL_H
#define _INCLUDED_KV_NODE_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "kv_fit_children_panel.h"

namespace vgui
{
	class Label;
	class Button;
};

// a vgui panel for showing a generic leaf
class CKV_Node_Panel : public CKV_Fit_Children_Panel
{
	DECLARE_CLASS_SIMPLE( CKV_Node_Panel, CKV_Fit_Children_Panel );

public:
	CKV_Node_Panel( Panel *parent, const char *name);

	virtual void PerformLayout();
	virtual void UpdatePanel();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand( const char *command );

	vgui::Label *m_pLabel;

	vgui::Button* m_pDeleteButton;
	CUtlVector<vgui::Button*> m_pAddButtons;

	MESSAGE_FUNC_CHARPTR( OnAddButtonPressed, "AddButtonPressed", szKeyName );

	CPanelAnimationVarAliasType( int, m_iLabelInsetX, "LabelInsetX", "2", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iLabelInsetY, "LabelInsetY", "2", "proportional_int" );
};


#endif // _INCLUDED_KV_NODE_PANEL_H