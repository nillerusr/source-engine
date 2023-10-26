#ifndef _INCLUDED_ASW_VGUI_COMPUTER_TUMBLER_HACK_H
#define _INCLUDED_ASW_VGUI_COMPUTER_TUMBLER_HACK_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "ImageButton.h"

class C_ASW_Hack_Computer;
class CASW_VGUI_Computer_Tumbler_Hack;

namespace vgui
{
	class ImagePanel;
};

// Tumbler panel class - will scroll a column of numbers round and round in either direction
class CASW_TumblerPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_TumblerPanel, vgui::Panel );
public:
	CASW_TumblerPanel( vgui::Panel *pParent, const char *pElementName, int iNumNumbers, int iTumblerIndex );
	virtual ~CASW_TumblerPanel();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	virtual void PerformLayout();
	void FadePanelIn(vgui::Panel* pPanel);

	CUtlVector<vgui::Label*> m_Numbers;
	vgui::Label *m_pEchoLabel;	// for echoing the number that's half on top and half on bottom	
	int m_iTumblerIndex;
	int m_iEchoEntry;
	int m_LastYCoord;
	CASW_VGUI_Computer_Tumbler_Hack* m_pTumblerHack;
};

class CASW_VGUI_Computer_Tumbler_Hack : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
public:
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Tumbler_Hack, vgui::Panel );

	CASW_VGUI_Computer_Tumbler_Hack( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Tumbler_Hack();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void FadePanelIn(vgui::Panel* pPanel);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void UpdateTumblerButtonTextures();
	float GetTracePanelY();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	void ClickedTumblerControl(int i);
	void OnCommand(char const* command);
	void UpdateCorrectStatus(bool bForce=false);

	// current computer hack
	CHandle<C_ASW_Hack_Computer> m_hHackComputer;

	int m_iNumColumns;
	int m_iColumnHeight;
	CUtlVector<CASW_TumblerPanel*> m_pTumblerPanels;
	CUtlVector<ImageButton*> m_pTumblerControls;
	CUtlVector<bool> m_bTumblerControlMouseOver;
	// todo: progress bar
	// todo: exit button
	// todo: title?
	// todo: other flavour?

	CUtlVector<vgui::Panel*> m_pStatusBlocks;
	CUtlVector<vgui::Label*> m_pStatusBlockLabels;

	vgui::Panel* m_pStatusBg;
	vgui::Label* m_pTitleLabel;
	vgui::Label* m_pAccessStatusLabel;
	vgui::Label* m_pAlignmentLabel;
	vgui::Label* m_pRejectedLabel;
	vgui::Label* m_pAccessLoggedLabel;
	int m_iFirstIncorrect;
	int m_iStatusBlocksCorrect;

	vgui::Panel* m_pTracePanel;
	vgui::ImagePanel* m_pFastMarker;

	// overall scale of this window
	float m_fScale;
	float m_fLastThinkTime;	
	int m_iMouseOverTumblerControl;
};

// correctness of overall puzzle can be determined by seeing how many tumblers have their correct number
//  in line with the previous tumbler

// doesn't have to be numbers? could be symbols (for a really hard hack, the symbol would need to be figured out somehow?)

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_TUMBLER_HACK_H */