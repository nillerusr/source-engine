#ifndef _INCLUDED_ASW_HUD_USE_ICON
#define _INCLUDED_ASW_HUD_USE_ICON
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "iasw_client_usable_entity.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};
class CBindPanel;

// shows the use icon in the middle of the screen when the player can interact with something in the game world

class CASW_HUD_Use_Icon : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_HUD_Use_Icon, vgui::Panel );
public:
	CASW_HUD_Use_Icon(vgui::Panel *pParent, const char *szPanelName);
	
	virtual void OnThink();
	virtual void Paint();	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();

	void ClearUseAction();
	void SetUseAction(ASWUseAction &action);	
	ASWUseAction* GetCurrentUseAction() { return &m_CurrentAction; }
	void PositionIcon();
	void SetCurrentToQueuedAction();
	void CustomPaintProgressBar( int ix, int iy, float flAlpha, float flProgress, const char *szCountText, vgui::HFont font, Color textCol, const char *szText );

	vgui::Label* m_pUseText;
	vgui::Label* m_pUseGlowText;

	vgui::Label* m_pHoldUseText;
	vgui::Label* m_pHoldUseGlowText;

	CBindPanel* m_pBindPanel;

	ASWUseAction m_CurrentAction;
	ASWUseAction m_QueuedAction;
	bool m_bHasQueued;

	int m_iImageW;
	int m_iImageT;
	int m_iImageX;
	int m_iImageY;

	CPanelAnimationVar( vgui::HFont, m_hFont, "UseIconFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hGlowFont, "UseIconGlowFont", "DefaultBlur" );
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
	//CPanelAnimationVarAliasType( int, m_nProgressBar, "HorizProgressBarTexture", "vgui/swarm/HUD/HorizProgressBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nProgressBar, "HorizProgressBarTexture", "vgui/loadingbar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nProgressBarBG, "HorizProgressBarTextureBG", "vgui/loadingbar_bg", "textureid" );

	// finding the current use key
	void FindUseKeyBind();
	char m_szUseKeyText[12];

	bool m_bHacking;

protected:
	void SetUseTextBounds( int x, int y, int w, int t );
	void SetHoldTextBounds( int x, int y, int w, int t );
	void FadeOut( float flDuration );
	void FadeIn( float flDuration );
};

#endif // _INCLUDED_ASW_HUD_USE_ICON