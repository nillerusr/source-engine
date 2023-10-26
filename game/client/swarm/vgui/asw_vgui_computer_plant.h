#ifndef _INCLUDED_ASW_VGUI_COMPUTER_PLANT_H
#define _INCLUDED_ASW_VGUI_COMPUTER_PLANT_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

class C_ASW_Hack_Computer;
class ImageButton;

// computer page showing the status of the power generator in the Plant mission

class CASW_VGUI_Computer_Plant : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Plant, vgui::Panel );

	CASW_VGUI_Computer_Plant( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Plant();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void ApplySettingAndFadeLabelIn(vgui::Label* pLabel);
	void SetLabels();
	void ScrollRawTable();
	bool GetReactorOnline();
	bool m_bReactorOnline;
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	
	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;

	vgui::Label* m_pPlantTitleLabel;
	vgui::ImagePanel *m_pPlantIcon;
	vgui::ImagePanel *m_pPlantIconShadow;
	
	vgui::Label* m_pTemperatureNameLabel[5];
	vgui::Label* m_pTemperatureValueLabel[4];
	vgui::Label* m_pCoolantNameLabel[3];
	vgui::Label* m_pCoolantValueLabel[3];
	vgui::Label* m_pEnergyBusNameLabel;
	vgui::Label* m_pEnergyBusLabel[10];
	vgui::Label* m_pReactorStatusLabel;
	vgui::Label* m_pReactorOnlineLabel;	
	vgui::Label* m_pEnergyNameLabel;
	vgui::Label* m_pEnergyValueLabel;
	vgui::Label* m_pEfficiencyNameLabel;
	vgui::Label* m_pEfficiencyValueLabel;
	vgui::Label* m_pFlagLabel[12];
	
	bool m_bMouseOverBackButton;

	// overall scale of this window
	float m_fScale;

	float m_fNextScrollRawTableTime;
	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_PLANT_H */