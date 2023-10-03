#ifndef _INCLUDED_ASW_VGUI_COMPUTER_MENU_H
#define _INCLUDED_ASW_VGUI_COMPUTER_MENU_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

class C_ASW_Hack_Computer;

namespace vgui {
	class Button;
};
class ImageButton;

#define ASW_COMPUTER_MAX_MENU_ITEMS 7

// main menu for the computers

class CASW_VGUI_Computer_Menu : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Menu, vgui::Panel );

	CASW_VGUI_Computer_Menu( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Menu();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	void SetIcons();
	void SetIcon(int icon, const char *szLabel, const char *szIcon);
	void LayoutMenuOptions();
	void FindMouseOverOption();
	virtual void OnCommand(char const* command);
	void ClickedMenuOption(int i);
	bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	void GoStraightToSingleOption();	// go straight to the one option that the current computer provides
	int m_iMouseOverOption;
	int m_iOldMouseOverOption;
	int m_iOptions;

	// server telling us to change which menu option we're showing;
	virtual void SetHackOption(int iOption);
	virtual void LaunchHackOption(int iOption);
	int m_iPrepareHackOption;
	int m_iPreviousHackOption;
	int m_iAutodownload;	// if set to non -1 value we'll go straight into download files instead of showing the main menu

	void ShowMenu();
	void HideMenu(bool bHideHeader);

	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;

	ImageButton *m_pMenuLabel[ASW_COMPUTER_MAX_MENU_ITEMS];
	vgui::ImagePanel *m_pMenuIcon[ASW_COMPUTER_MAX_MENU_ITEMS];
	vgui::ImagePanel *m_pMenuIconShadow[ASW_COMPUTER_MAX_MENU_ITEMS];
	vgui::ImagePanel *m_pBlackBar[2];
	vgui::DHANDLE<vgui::Panel> m_hCurrentPage;
	
	vgui::Label *m_pAccessDeniedLabel;
	vgui::Label *m_pInsufficientRightsLabel;

	bool m_bFadingCurrentPage;
	void DoFadeCurrentPage();	// actually fades the current page out
	void FadeCurrentPage();		// requests server to return to menu, so we can fade our current page out and return there

	bool IsHacking();

	// overall scale of this window
	float m_fScale;

	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_MENU_H */