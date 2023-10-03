#ifndef _INCLUDED_ASW_VGUI_FRAME_H
#define _INCLUDED_ASW_VGUI_FRAME_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

namespace vgui {
	class ImagePanel;
};

class C_ASW_Hack;

// a panel that can frame other asw ingame panels.  Can be dragged around and closed (and minimised?)

class CASW_VGUI_Frame : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Frame, vgui::Panel );

	CASW_VGUI_Frame( vgui::Panel *pParent, const char *pElementName, const char *szTitle  );
	virtual ~CASW_VGUI_Frame();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual void PerformLayout();
	bool MouseClick(int x, int y, bool bRightClick, bool bDown);

	vgui::Label* m_pTitleLabel;
	vgui::ImagePanel *m_pCloseImage;
	vgui::Panel* m_pTitleBarLeft;
	vgui::Panel* m_pTitleBarRight;
	vgui::ImagePanel *m_pMiniImage;

	bool m_bFrameMinimized;

	bool m_bMouseOverTitleBar;
	bool m_bDragging;	// are we currently dragging the window around?
	int m_iDragOffsetX;
	int m_iDragOffsetY;

	bool m_bMouseOverCloseIcon;
	bool m_bMouseOverMiniIcon;

	// overall scale of this window
	int m_iTitleBarHeight;
	float m_fScale;
	
	C_ASW_Hack* m_pNotifyHackOnClose;
};

#endif /* _INCLUDED_ASW_VGUI_FRAME_H */