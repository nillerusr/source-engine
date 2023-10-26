#ifndef _INCLUDED_ASW_VGUI_AMMO_LIST
#define _INCLUDED_ASW_VGUI_AMMO_LIST

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "asw_weapon_ammo_bag_shared.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};

// this panel shows the contents of the ammo bag when its the active weapon

class CASW_VGUI_Ammo_List : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Ammo_List, vgui::Panel );

	CASW_VGUI_Ammo_List( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Ammo_List();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	
	vgui::Label* m_pTitle;
	vgui::Label* m_pTitleGlow;
	vgui::Label* m_pAmmoName[ASW_AMMO_BAG_SLOTS];
	vgui::Label* m_pAmmoCount[ASW_AMMO_BAG_SLOTS];
	vgui::ImagePanel* m_pAmmoIcon[ASW_AMMO_BAG_SLOTS];
	int m_iAmmoCount[ASW_AMMO_BAG_SLOTS];

	// overall scale of this window
	float m_fScale;

	CPanelAnimationVar( vgui::HFont, m_hFont, "AmmoFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hGlowFont, "AmmoGlowFont", "DefaultBlur" );
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );

	float m_fLastThinkTime;
};

#endif /* _INCLUDED_ASW_VGUI_AMMO_LIST */