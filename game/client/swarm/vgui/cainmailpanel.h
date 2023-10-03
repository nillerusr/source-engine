#ifndef _INCLUDED_CAIN_MAIL_PANEL_H
#define _INCLUDED_CAIN_MAIL_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
	class Label;
	class WrappedLabel;
	class ImagePanel;
};

#define ASW_CAIN_MAIL_LINES 8

// shows mail from SynTek to Lt. Cain - used in outro
class CainMailPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CainMailPanel, vgui::Panel );
public:
	CainMailPanel(vgui::Panel *parent, const char *name);
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	void StartFadeIn(vgui::IScheme *pScheme);

	vgui::Label* m_MailLines[ASW_CAIN_MAIL_LINES];
	float m_fFadeOutTime;
};

#endif // _INCLUDED_CAIN_MAIL_PANEL_H