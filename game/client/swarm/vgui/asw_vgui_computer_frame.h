#ifndef _INCLUDED_ASW_VGUI_COMPUTER_FRAME_H
#define _INCLUDED_ASW_VGUI_COMPUTER_FRAME_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"
#include "asw_vgui_frame.h"

class C_ASW_Hack_Computer;
class CASW_VGUI_Computer_Menu;
class CASW_VGUI_Computer_Splash;
class ImageButton;

// frame holding the computer vgui panels

class CASW_VGUI_Computer_Frame : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Frame, vgui::Panel );

	CASW_VGUI_Computer_Frame( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackDoor );
	virtual ~CASW_VGUI_Computer_Frame();
	
	virtual void ASWInit();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void OnCommand( const char *pCommand );
	virtual void PaintBackground();
	virtual void Paint();
	virtual void PerformLayout();
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void SplashFinished();

	virtual void SetTitleHidden(bool bHidden);
	virtual void SetHackOption(int iOption);

	virtual void SetBackdrop(int iBackdropType);
	int m_iBackdropType;

	bool IsPDA();

	// current door hack
	C_ASW_Hack_Computer* m_pHackComputer;
	CASW_VGUI_Computer_Splash *m_pSplash;
	CASW_VGUI_Computer_Menu *m_pMenuPanel;
	ImageButton* m_pLogoffLabel;
	
	vgui::Panel* m_pCurrentPanel;
	vgui::ImagePanel* m_pScan[3];
	vgui::ImagePanel *m_pBackdropImage;
	int m_iScanHeight;	

	bool m_bPlayingSplash;

	// overall scale of this window
	float m_fScale;

	float m_fLastThinkTime;
	bool m_bSetAlpha;
	bool m_bHideLogoffButton;
};

class CASW_VGUI_Computer_Container : public CASW_VGUI_Frame
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Computer_Container, CASW_VGUI_Frame );

	CASW_VGUI_Computer_Container(Panel *parent, const char *panelName, const char *pszTitle);
	virtual ~CASW_VGUI_Computer_Container();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();

	virtual void OnThink();
};

#endif /* _INCLUDED_ASW_VGUI_COMPUTER_FRAME_H */