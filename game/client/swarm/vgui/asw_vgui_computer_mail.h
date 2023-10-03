#ifndef _INCLUDED_ASW_VGUI_COMPUTER_MAIL_H
#define _INCLUDED_ASW_VGUI_COMPUTER_MAIL_H

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
namespace vgui {
	class PanelListPanel;
};

#define ASW_MAIL_ROWS 4

// computer/PDA page showing some mail

class CASW_VGUI_Computer_Mail : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Mail, vgui::Panel );

	CASW_VGUI_Computer_Mail( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Mail();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void ApplySettingAndFadeLabelIn(vgui::Label* pLabel);
	void SetLabelsFromMailFile();
	void ShowMail(int i);
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	virtual void ScrollMail();
	bool IsPDA();

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;
	vgui::Label* m_pTitleLabel;
	vgui::ImagePanel* m_pTitleIcon;
	vgui::ImagePanel* m_pTitleIconShadow;

	vgui::ImagePanel* m_pInboxIcon[2];
	vgui::Label* m_pInboxLabel;
	vgui::Label* m_pOutboxLabel;
	vgui::Label* m_pMailFrom[ASW_MAIL_ROWS];
	vgui::Label* m_pMailDate[ASW_MAIL_ROWS];
	ImageButton* m_pMailSubject[ASW_MAIL_ROWS];
	vgui::PanelListPanel *m_pBodyList;
	vgui::Label* m_pBody[4];

	vgui::Label* m_pBodyLabel;
	vgui::Label* m_pAccountLabel;
	vgui::Label* m_pName;
	ImageButton* m_pMoreButton;

	bool m_bMouseOverBackButton;

	KeyValues * m_pMailKeyValues;	// key values describing the current mail being read

	// overall scale of this window
	float m_fScale;
	float m_fLastThinkTime;
	int m_iSelectedMail;

	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_MAIL_H */