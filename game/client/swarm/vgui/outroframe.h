#ifndef _INCLUDED_OUTRO_FRAME_H
#define _INCLUDED_OUTRO_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

// frame to hold outro VGUI stuff

class OutroFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( OutroFrame, vgui::Frame );

	OutroFrame(Panel *parent, const char *panelName, bool showTaskbarIcon = true);

	virtual void PerformLayout();
};
#endif // _INCLUDED_OUTRO_FRAME_H