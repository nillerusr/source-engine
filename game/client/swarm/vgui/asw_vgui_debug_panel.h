#ifndef _INCLUDED_ASW_VGUI_DEBUG_PANEL
#define _INCLUDED_ASW_VGUI_DEBUG_PANEL


#include "asw_vgui_ingame_panel.h"

class ImageButton;
namespace vgui
{
	class Label;
	class Button;
	class ImagePanel;
};

// panel for showing/selecting an individual debug command
class CASW_Debug_Panel_Entry : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CASW_Debug_Panel_Entry, vgui::Panel );

	CASW_Debug_Panel_Entry( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_Debug_Panel_Entry();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	void LoadFromKeyValues( KeyValues *pEntryKey );
	virtual void OnThink();

	vgui::Label *m_pLabel;
	KeyValues *m_pEntryKeys;
};

// panel with commonly used debug commands

class CASW_VGUI_Debug_Panel : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{	
public:
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Debug_Panel, vgui::Panel );

	CASW_VGUI_Debug_Panel( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Debug_Panel();
	
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	
	KeyValues *m_pDebugKV;

	vgui::Panel *m_pDebugBar;
	CUtlVector<ImageButton*> m_MenuButtons;
	vgui::Panel *m_pCurrentMenu;
	CUtlVector<CASW_Debug_Panel_Entry*> m_MenuEntries;
	KeyValues *m_pCurrentMenuKeys;
};

#endif /* _INCLUDED_ASW_VGUI_DEBUG_PANEL */