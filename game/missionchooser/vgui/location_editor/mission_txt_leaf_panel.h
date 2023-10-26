#ifndef _INCLUDED_MISSION_TXT_LEAF_PANEL_H
#define _INCLUDED_MISSION_TXT_LEAF_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "kv_leaf_panel.h"

namespace vgui
{
	class TextEntry;
	class Button;
};

// a vgui panel for showing a generic leaf
class CMission_Txt_Panel : public CKV_Leaf_Panel
{
	DECLARE_CLASS_SIMPLE( CMission_Txt_Panel, CKV_Leaf_Panel );

public:
	CMission_Txt_Panel( Panel *parent, const char *name );

	virtual void OnCommand( const char *command );
	void DoPick();
	void OnFileSelected(const char *fullpath);
	virtual void PerformLayout();

	DECLARE_PANELMAP();	
	vgui::Button* m_pPickButton;
};


#endif // _INCLUDED_MISSION_TXT_LEAF_PANEL_H