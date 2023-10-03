#ifndef _INCLUDED_ASW_VGUI_STYLIN_CAM
#define _INCLUDED_ASW_VGUI_STYLIN_CAM
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui/IScheme.h"
#include "asw_hudelement.h"

// This panel shows a camera view of the marines or some other event
//   (usually slow motion stylin' action!)

namespace vgui
{
	class Label;
	class ImagePanel;
};
class C_PointCamera;

class CASW_VGUI_Stylin_Cam : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Stylin_Cam, vgui::Panel );

	CASW_VGUI_Stylin_Cam( const char *pElementName );
	virtual ~CASW_VGUI_Stylin_Cam();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void PerformLayout();
	int GetStylinCamSize();

	bool ShouldShowStylinCam();

	vgui::ImagePanel *m_pCameraImage;
	bool m_bFadingOutCameraImage, m_bFadingInCameraImage;

	bool ShouldShowCommanderFace();

	vgui::ImagePanel *m_pCommanderImage;
	vgui::ImagePanel *m_pCommanderFlash;
	bool m_bFadingOutCommanderFace, m_bFadingInCommanderFace;

	bool m_bCommanderFace;
};

#endif /* _INCLUDED_ASW_VGUI_STYLIN_CAM */