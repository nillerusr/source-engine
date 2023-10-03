#ifndef _INCLUDED_ASW_VGUI_FAST_RELOAD_H
#define _INCLUDED_ASW_VGUI_FAST_RELOAD_H

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class ImagePanel;
};
class C_ASW_Weapon;

// panel to show fast reload timing (this was just a test, fast reloading is currently disabled)

class CASW_VGUI_Fast_Reload : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Fast_Reload, vgui::Panel );

	CASW_VGUI_Fast_Reload( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Fast_Reload();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void PositionBars(C_ASW_Weapon *pWeapon);
	
	CPanelAnimationVarAliasType( int, m_nFastReloadBarBG, "FastReloadBarBG", "vgui/swarm/HUD/HorizHealthBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nWhiteTexture, "WhiteTexture", "vgui/white", "textureid" );

	vgui::Panel *m_pBar;
	vgui::Panel *m_pFastSection;
	vgui::Panel *m_pProgress;

	float m_flLastReloadProgress;
	float m_flLastNextAttack;
	float m_flLastFastReloadStart;
	float m_flLastFastReloadEnd;
};

#endif /* _INCLUDED_ASW_VGUI_FAST_RELOAD_H */