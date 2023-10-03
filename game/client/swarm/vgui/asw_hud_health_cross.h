#ifndef _INCLUDED_ASW_HUD_HEALTH_CROSS_H
#define _INCLUDED_ASW_HUD_HEALTH_CROSS_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_BasePanel.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/AnimationController.h>

class CASW_HUD_Marine_Portrait;
namespace vgui
{
	class Label;
};

// a particular marine portrait on the hud
class CASW_HUD_Health_Cross : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_HUD_Health_Cross, vgui::Panel );
public:
	CASW_HUD_Health_Cross(vgui::Panel *pParent, const char *szPanelName );

	virtual void OnThink();
	virtual void Paint();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "DefaultMedium" );
	CPanelAnimationVarAliasType( int, m_nFullHealthTexture, "FullHealth", "vgui/hud/health_full", "textureid" );
	CPanelAnimationVarAliasType( int, m_nNoHealthTexture,   "NoHealth",   "vgui/hud/health_none", "textureid" );

	CPanelAnimationVarAliasType( float, health_xpos, "digit_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, health_ypos, "digit_ypos", "0", "proportional_float" );

	vgui::Label *m_pHealthLabel;
	vgui::Label *m_pHealthLabelShadow;
	vgui::DHANDLE< CASW_HUD_Marine_Portrait > m_hPortrait;
	int m_iHealth;
};

#endif // _INCLUDED_ASW_HUD_HEALTH_CROSS_H