#ifndef _INCLUDED_KV_EDITOR_PANEL_H
#define _INCLUDED_KV_EDITOR_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>

class CKV_Editor;

// a vgui panel that resizes to fit its children, with some border
class CKV_Editor_Base_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CKV_Editor_Base_Panel, vgui::EditablePanel );

public:
	CKV_Editor_Base_Panel( Panel *parent, const char *name);

	virtual void PerformLayout();

	virtual void UpdatePanel() { }
	void SetKey( KeyValues *pKey );
	KeyValues* GetKey() { return m_pKey; }
	void SetKeyParent( KeyValues *pKey );

	void SetFileSpecNode( KeyValues *pKey );
	void SetEditor( CKV_Editor *pEditor ) { m_pEditor = pEditor; }
	void SetAllowDeletion( bool bCanDelete ) { m_bAllowDeletion = bCanDelete; }

	float GetSortOrder() const { return m_flSortOrder; }
	void SetSortOrder( float flSortOrder ) { m_flSortOrder = flSortOrder; }

protected:
	KeyValues *m_pKey;
	KeyValues *m_pKeyParent;
	KeyValues *m_pFileSpecNode;
	CKV_Editor *m_pEditor;
	float m_flSortOrder;
	bool m_bAllowDeletion;
};


#endif // _INCLUDED_KV_EDITOR_PANEL_H