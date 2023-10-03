#ifndef BRIEFINGIMAGEPANEL_H
#define BRIEFINGIMAGEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/ImagePanel.h>

//class ChatEchoPanel;

//-----------------------------------------------------------------------------
// Purpose: Panel that holds a single image - shows the backdrop image in the briefing
//-----------------------------------------------------------------------------
class BriefingImagePanel : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( BriefingImagePanel, vgui::ImagePanel );
public:
	BriefingImagePanel(vgui::Panel *parent, const char *name);
	virtual ~BriefingImagePanel();
	virtual void OnThink();
	void CloseBriefingFrame();
	void StartMusic();

	virtual void PerformLayout();

	// no drawing the image here...
	virtual void PaintBackground() { }

	vgui::Panel* m_pOldParent;
	//ChatEchoPanel* m_pChatEcho;
	float m_fRestartMusicTime;
};


#endif // BRIEFINGIMAGEPANEL_H
