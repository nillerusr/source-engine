#ifndef _INCLUDED_ASW_VGUI_COMPUTER_NEWS_H
#define _INCLUDED_ASW_VGUI_COMPUTER_NEWS_H

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

#define ASW_MAIL_ROWS 4

// computer page showing a news story

class CASW_VGUI_Computer_News : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_News, vgui::Panel );

	CASW_VGUI_Computer_News( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_News();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void ApplySettingAndFadeLabelIn(vgui::Label* pLabel);
	void SetLabelsFromFile();
	bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;
	vgui::Label* m_pTitleLabel;
	vgui::ImagePanel* m_pTitleIcon;
	vgui::ImagePanel* m_pTitleIconShadow;

	vgui::Label* m_pHeadline;
	vgui::Label* m_pHeadlineDate;
	vgui::Panel* m_pHeadlineBackdrop;
	vgui::Label* m_pBody[4];

	vgui::Label* m_pBodyLabel;
	
	bool m_bMouseOverBackButton;

	KeyValues * m_pKeyValues;	// key values describing the current text being read

	// overall scale of this window
	float m_fScale;
	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_NEWS_H */