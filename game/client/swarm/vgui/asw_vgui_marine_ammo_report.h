#ifndef _INCLUDED_ASW_VGUI_MARINE_AMMO_REPORT
#define _INCLUDED_ASW_VGUI_MARINE_AMMO_REPORT

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "asw_vgui_ammo_list.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};
class C_ASW_Marine;

#define ASW_AMMO_REPORT_SLOTS (ASW_AMMO_BAG_SLOTS+1)		// one for each ammo type in the ammo bag, plus mining laser clips

// shaded panel shown on screen to show which ammo is being carried by a particular marine

class CASW_VGUI_Marine_Ammo_Report : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Marine_Ammo_Report, vgui::Panel );

	CASW_VGUI_Marine_Ammo_Report( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Marine_Ammo_Report();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void Paint();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void SetMarine(C_ASW_Marine* pMarine);
	void SetAmmoTypesVisible();
	bool MarineHasWeapon(C_ASW_Marine *pMarine, int iAmmoType);
	
	vgui::Label* m_pTitle;
	vgui::Label* m_pAmmoName[ASW_AMMO_REPORT_SLOTS];
	vgui::Label* m_pAmmoCount[ASW_AMMO_REPORT_SLOTS];
	vgui::Label* m_pAmmoOverflow[ASW_AMMO_REPORT_SLOTS];
	vgui::ImagePanel* m_pAmmoIcon[ASW_AMMO_REPORT_SLOTS];
	vgui::ImagePanel* m_pClipsIcon[ASW_AMMO_REPORT_SLOTS];

	vgui::Label *m_pGlowLabel;

	C_ASW_Marine* GetMarine();
	EHANDLE m_hMarine;
	EHANDLE m_hQueuedMarine;
	bool m_bQueuedMarine;
	float m_fNextUpdateTime;
	bool m_bDoingSlowFade;

	// overall scale of this window
	float m_fScale;

	CPanelAnimationVar( vgui::HFont, m_hFont, "AmmoFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hGlowFont, "AmmoGlowFont", "DefaultBlur" );
};

#endif /* _INCLUDED_ASW_VGUI_MARINE_AMMO_REPORT */