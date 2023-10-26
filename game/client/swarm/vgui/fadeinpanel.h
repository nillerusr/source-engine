#ifndef _INCLUDED_ASW_FADEINPANEL_H
#define _INCLUDED_ASW_FADEINPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui {
	class ImagePanel;
}

//-----------------------------------------------------------------------------
// Purpose: Panel that draws a black image over the screen that steadily fades out
//		Panel deletes itself when the fade is finished
//-----------------------------------------------------------------------------
class FadeInPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( FadeInPanel, vgui::Panel );
public:
	FadeInPanel(vgui::Panel *parent, const char *name);
	virtual ~FadeInPanel();
	void StartSlowRemove();	// starts the panel fading out
	void AllowFastRemove();	// allows the panel to start fading out immediately when we reach the ASW_GS_INGAME state
	virtual void PerformLayout();
	virtual void OnThink();	// fades the panel out if we're in the ASW_GS_INGAME state for more than 2 seconds
	vgui::ImagePanel* m_pBlackImage;

	bool m_bSlowRemove;
	bool m_bFastRemove;
	float m_fIngameTime;
};

#endif // _INCLUDED_ASW_FADEINPANEL_H
