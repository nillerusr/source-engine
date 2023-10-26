#ifndef _INCLUDED_ASW_VGUI_COMPUTER_SPLASH_H
#define _INCLUDED_ASW_VGUI_COMPUTER_SPLASH_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

class C_ASW_Hack_Computer;

#define ASW_SPLASH_SCROLL_LINES 26

// computer page showing the syntek splash screen

class CASW_VGUI_Computer_Splash : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Splash, vgui::Panel );

	CASW_VGUI_Computer_Splash( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Splash();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void SetScale(float f);
	void ASWInit();
	void SetHidden(bool bHidden);
	bool IsPDA();

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;

	vgui::ImagePanel *m_pLogoImage;
	vgui::ImagePanel *m_pLogoGlitchImage;
	vgui::Label* m_pSynTekLabel;
	vgui::Label* m_pSloganLabel;
	float m_fNextGlitchTime;
	float m_fShowGlitchTime;
	float m_fSoundTime;
	bool m_bPlayedSound;

	// scrolling text
	vgui::Label* m_pScrollLine[ASW_SPLASH_SCROLL_LINES];
	int m_iNumLines;
	float m_fStartScrollingTime;
	float m_fFadeScrollerOutTime;

	// sliding out
	float m_fSlideOutTime;
	bool m_bSlidOut;

	// overall scale of this window
	float m_fScale;

	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_SPLASH_H */