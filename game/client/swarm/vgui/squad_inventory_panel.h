#ifndef _INCLUDED_SQUAD_INVENTORY_PANEL_H
#define _INCLUDED_SQUAD_INVENTORY_PANEL_H

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "asw_vgui_ingame_panel.h"

class CSquad_Inventory_Panel_Entry;
class C_ASW_Marine;
namespace vgui
{
	class ImagePanel;
	class Label;
};

// shows all the usable items your squad is carrying

class CSquad_Inventory_Panel : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
public:
	DECLARE_CLASS_SIMPLE( CSquad_Inventory_Panel, vgui::Panel );

	CSquad_Inventory_Panel( vgui::Panel *pParent, const char *pElementName );
	virtual ~CSquad_Inventory_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);

	void UpdateList();

protected:
	CUtlVector<CSquad_Inventory_Panel_Entry*> m_pEntries;
};

// individual entry in the list
class CSquad_Inventory_Panel_Entry : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CSquad_Inventory_Panel_Entry, vgui::Panel );

	CSquad_Inventory_Panel_Entry( vgui::Panel *pParent, const char *pElementName );
	virtual ~CSquad_Inventory_Panel_Entry();

	void SetDetails( C_ASW_Marine *pMarine, int iInventoryIndex );

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void UpdateImage();
	void ShowTooltip();
	void ActivateItem();

	// details for this entry
	CHandle<C_ASW_Marine> m_hMarine;
	int m_iInventoryIndex;

	vgui::ImagePanel *m_pWeaponImage;
	vgui::Label *m_pMarineNameLabel;
	bool m_bMouseOver;
};

#endif // _INCLUDED_SQUAD_INVENTORY_PANEL_H