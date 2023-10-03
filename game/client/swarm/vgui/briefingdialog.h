#ifndef _INCLUDED_BRIEFING_DIALOG_H
#define _INCLUDED_BRIEFING_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

class ImageButton;
class BriefingTooltip;

// generic pop up dialog used in briefing

class BriefingDialog : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( BriefingDialog, vgui::Panel );
public:
	BriefingDialog(vgui::Panel *parent, const char *name, const char *message,
		const char *oktext, const char *canceltext, const char *okcmd, const char *cancelcmd=NULL);
	virtual ~BriefingDialog();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void OnCommand(const char *command);
	
	vgui::Panel *m_pMessageBox;
	vgui::Label *m_pMessage;
	ImageButton* m_pOkayButton;
	ImageButton* m_pCancelButton;
	
	char m_szOkayCommand[128];
	char m_szCancelCommand[128];
};

#endif // _INCLUDED_BRIEFING_DIALOG_H
