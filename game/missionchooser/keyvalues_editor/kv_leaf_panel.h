#ifndef _INCLUDED_KV_LEAF_PANEL_H
#define _INCLUDED_KV_LEAF_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "kv_editor_base_panel.h"

namespace vgui
{
	class TextEntry;
	class Label;
};

// a vgui panel for showing a generic leaf
class CKV_Leaf_Panel : public CKV_Editor_Base_Panel
{
	DECLARE_CLASS_SIMPLE( CKV_Leaf_Panel, CKV_Editor_Base_Panel );

public:
	CKV_Leaf_Panel( Panel *parent, const char *name);

	virtual void PerformLayout();
	virtual void UpdatePanel();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand( const char *command );

	MESSAGE_FUNC_PTR( TextEntryChanged, "TextChanged", panel );
	vgui::TextEntry *m_pTextEntry;
	vgui::Label *m_pLabel;
	vgui::Button* m_pDeleteButton;
};


#endif // _INCLUDED_KV_LEAF_PANEL_H