#ifndef _INCLUDED_ASW_VGUI_COMPUTER_WEATHER_H
#define _INCLUDED_ASW_VGUI_COMPUTER_WEATHER_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "vstdlib/random.h"

class C_ASW_Hack_Computer;
class ImageButton;

#define ASW_WEATHER_ENTRIES 7

class CASW_VGUI_Computer_Weather : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Weather, vgui::Panel );

	CASW_VGUI_Computer_Weather( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Weather();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void ApplySettingAndFadeLabelIn(vgui::Label* pLabel);	
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	void SetWeatherLabels();
	void SetFloatLabel(vgui::Label *pLabel, const char *string, float fValue);
	void SetIntLabel(vgui::Label *pLabel, const char *string, int iValue);

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;
	vgui::Label* m_pTitleLabel;
	vgui::ImagePanel* m_pTitleIcon;
	vgui::ImagePanel* m_pTitleIconShadow;
	
	vgui::Label* m_pWeatherLabel[ASW_WEATHER_ENTRIES];
	vgui::Label* m_pWeatherValue[ASW_WEATHER_ENTRIES];

	vgui::Label* m_pWarningLabel;
	
	// overall scale of this window
	float m_fScale;
	float m_fLastThinkTime;
	bool m_bSetAlpha;
	CUniformRandomStream			m_NumberStream;			// Used to generate random numbers.	
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_WEATHER_H */