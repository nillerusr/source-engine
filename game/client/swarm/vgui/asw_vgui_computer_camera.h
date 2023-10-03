#ifndef _INCLUDED_ASW_VGUI_COMPUTER_CAMERA_H
#define _INCLUDED_ASW_VGUI_COMPUTER_CAMERA_H

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

// computer page showing a remote camera view

class CASW_VGUI_Computer_Camera : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Camera, vgui::Panel );

	CASW_VGUI_Computer_Camera( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Camera();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	void ASWInit();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	
	// current computer hack
	C_ASW_Hack_Computer* m_pHackComputer;
	ImageButton* m_pBackButton;
	vgui::Label* m_pCameraLabel;
	vgui::Label* m_pTitleLabel;
	vgui::ImagePanel* m_pCameraImage;
	bool m_bMouseOverBackButton;

	// overall scale of this window
	float m_fScale;

	float m_fLastThinkTime;
	bool m_bSetAlpha;
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_CAMERA_H */