#ifndef _INCLUDED_ASW_VGUI_COMPUTER_DOWNLOAD_DOCS_H
#define _INCLUDED_ASW_VGUI_COMPUTER_DOWNLOAD_DOCS_H

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

// computer page allowing player to download critical files

class CASW_VGUI_Computer_Download_Docs : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Download_Docs, vgui::Panel );

	CASW_VGUI_Computer_Download_Docs( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Download_Docs();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void ApplySettingAndFadeLabelIn(vgui::Label* pLabel);
	bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;
	vgui::Label* m_pTitleLabel;
	bool m_bSetTextComplete;	// have we set the title to say download complete yet?
	vgui::ImagePanel* m_pFilesIcon;
	vgui::ImagePanel* m_pFilesIconShadow;
	vgui::ImagePanel* m_pArrow;
	vgui::ImagePanel* m_pDeviceIcon;
	vgui::ImagePanel* m_pDeviceIconShadow;

	vgui::Panel* m_pProgressBarBackdrop;
	vgui::Panel* m_pProgressBar;
	
	bool m_bMouseOverBackButton;

	KeyValues * m_pKeyValues;	// key values describing the current text being read

	// overall scale of this window
	float m_fScale;
	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_DOWNLOAD_DOCS_H */