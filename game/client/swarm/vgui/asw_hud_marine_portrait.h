#ifndef _INCLUDED_ASW_MARINE_PORTRAIT_H
#define _INCLUDED_ASW_MARINE_PORTRAIT_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_BasePanel.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/AnimationController.h>

namespace vgui
{
	class Label;
	class ImagePanel;
};

class C_ASW_Marine_Resource;
class CASWHudPortraits;
class CASW_HUD_Health_Cross;
class HUDVideoPanel;

extern ConVar asw_draw_hud;
// a particular marine portrait on the hud
class CASW_HUD_Marine_Portrait : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_HUD_Marine_Portrait, vgui::Panel );
public:
	CASW_HUD_Marine_Portrait(vgui::Panel *pParent, const char *szPanelName, CASWHudPortraits *pPortraitContainer );
	
	virtual void OnThink();
	virtual void Paint();
	void PaintClassIcon();
	void PaintPortrait();
	void PaintAmmo();
	void PaintMeds();
	void PaintUsing(float fFraction);
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	void PositionMarinePanel();
	void SetMarineResource(C_ASW_Marine_Resource *pMarineResource, int index, bool bLocalMarine, bool bForcePosition=false);	
	float GetUsingFraction();
	void SetScale(float fScale);

	HUDVideoPanel *m_pVideoFace;
	
	vgui::Label *m_pMarineName;
	vgui::Label *m_pMarineNameGlow;
	vgui::Label *m_pMarineNumber;
	
	vgui::ImagePanel *m_pBulletsIcon;
	vgui::ImagePanel *m_pClipsIcon;

	vgui::ImagePanel *m_pLeadershipAuraIcon;

	vgui::ImagePanel *m_pFiringOverlay;
	float m_fLastFiringFade;
	vgui::ImagePanel *m_pInfestedOverlay;
	vgui::Label *m_pReloadingOverlay;
	vgui::Label *m_pLowAmmoOverlay;
	vgui::Label *m_pLowAmmoOverlay2;
	vgui::Label *m_pNoAmmoOverlay;
	vgui::Label *m_pNoAmmoOverlay2;
	CASW_HUD_Health_Cross *m_pHealthCross;
	float m_fInfestedFade;

	void SetFonts();
	
	CHandle<C_ASW_Marine_Resource> m_hMarineResource;
	bool m_bLocalMarine;
	int m_iIndex;
	bool m_bMedic;
	float m_fLastHealth;
	float m_fScale;
	bool m_bLastWide, m_bLastFace, m_bLastInfested, m_bLastHealthCross;	// were we a wide panel last time?

	CPanelAnimationVar( vgui::HFont, m_hMarineNameFont, "MarineNameFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hMarineNameSmallFont, "MarineNameFontSmall", "DefaultMedium" );
	CPanelAnimationVar( vgui::HFont, m_hMarineNameGlowFont, "MarineNameGlowFont", "DefaultBlur" );
	CPanelAnimationVar( vgui::HFont, m_hMarineNameSmallGlowFont, "MarineNameGlowFont", "DefaultMediumBlur" );	
	CPanelAnimationVar( vgui::HFont, m_hMarineNumberFont, "MarineNumberFont", "Default" );	
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nHorizHealthBar, "HealthBarTexture", "vgui/swarm/HUD/HorizHealthBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nVertAmmoBar, "VertAmmoBarTexture", "vgui/swarm/HUD/VertAmmoBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nUsingTexture, "UsingTexture", "vgui/swarm/HUD/UsingOverlaySmall", "textureid" );
	CPanelAnimationVarAliasType( int, m_nTinyClipIcon, "TinyClipTexture", "vgui/swarm/HUD/TinyClipIcon", "textureid" );
	CPanelAnimationVarAliasType( int, m_nTinyClipIconHoriz, "TinyClipHorizTexture", "vgui/swarm/HUD/TinyClipIconHoriz", "textureid" );
	CPanelAnimationVarAliasType( int, m_nWhiteTexture, "WhiteTexture", "vgui/white", "textureid" );
	CPanelAnimationVarAliasType( int, m_nMedChargeTexture, "MedCharge", "vgui/swarm/HUD/MedCharge", "textureid" );
	CPanelAnimationVarAliasType( int, m_nMedChargeHorizTexture, "MedChargeHoriz", "vgui/swarm/HUD/MedChargeHoriz", "textureid" );

	vgui::DHANDLE< CASWHudPortraits > m_hPortraitContainer;
};

#endif // _INCLUDED_ASW_MARINE_PORTRAIT_H