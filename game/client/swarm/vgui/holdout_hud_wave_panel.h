#ifndef _INCLUDED_HOLDOUT_HUD_WAVE_PANEL
#define _INCLUDED_HOLDOUT_HUD_WAVE_PANEL

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

class Holdout_Hud_Wave_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( Holdout_Hud_Wave_Panel, vgui::EditablePanel );
public:
	Holdout_Hud_Wave_Panel( vgui::Panel *parent, const char *name );
	virtual ~Holdout_Hud_Wave_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

	void UpdateWaveProgressBar();
	
protected:
	vgui::Label* m_pWaveNumberLabel;
	vgui::Label* m_pScoreLabel;
	vgui::Panel* m_pWaveProgressBarBG;
	vgui::Panel* m_pWaveProgressBar;

	vgui::Label *m_pCountdownLabel;
	vgui::Label *m_pCountdownTimeLabel;

	// TODO: Floating + numbers when you score kills? or a deaths panel like in other valve games
};


#endif // _INCLUDED_HOLDOUT_HUD_WAVE_PANEL
