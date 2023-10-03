#ifndef _INCLUDED_HOLDOUT_WAVE_END_PANEL_H
#define _INCLUDED_HOLDOUT_WAVE_END_PANEL_H

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

class Holdout_Wave_End_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( Holdout_Wave_End_Panel, vgui::EditablePanel );
public:
	Holdout_Wave_End_Panel( vgui::Panel *parent, const char *name );
	virtual ~Holdout_Wave_End_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();
	void Init( int nWave, float flDuration );

protected:
	vgui::Label* m_WaveCompleteLabel;
	
	// TODO: Kills per player
	// TODO: Score gained that round
	// TODO: Total score

	int m_nWave;
	float m_flClosePanelTime;
};


#endif // _INCLUDED_HOLDOUT_WAVE_END_PANEL_H
