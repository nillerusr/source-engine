#ifndef _INCLUDED_HOLDOUT_END_SCORE_PANEL_H
#define _INCLUDED_HOLDOUT_END_SCORE_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class Label;
	class ImagePanel;
}

// this panel shows your current score and wave on the HUD

class Holdout_End_Score_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( Holdout_End_Score_Panel, vgui::EditablePanel );
public:
	Holdout_End_Score_Panel( vgui::Panel *parent, const char *name );
	virtual ~Holdout_End_Score_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	// TODO: Overall scores
};


#endif // _INCLUDED_HOLDOUT_END_SCORE_PANEL_H
