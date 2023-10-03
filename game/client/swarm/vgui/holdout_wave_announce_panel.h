#ifndef _INCLUDED_HOLDOUT_WAVE_ANNOUNCE_PANEL_H
#define _INCLUDED_HOLDOUT_WAVE_ANNOUNCE_PANEL_H

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

// this panel announces the next wave

class Holdout_Wave_Announce_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( Holdout_Wave_Announce_Panel, vgui::EditablePanel );
public:
	Holdout_Wave_Announce_Panel( vgui::Panel *parent, const char *name );
	virtual ~Holdout_Wave_Announce_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();
	void Init( int nWave, float flDuration );

protected:
	vgui::Label* m_pWaveAnnounceLabel;
	vgui::Label* m_pGetReadyLabel;

	int m_nWave;
	float m_flClosePanelTime;
	float m_flWaveNumberSoundTime;
	float m_flGetReadySlideSoundTime;
	float m_flGetReadySoundTime;
};


#endif // _INCLUDED_HOLDOUT_WAVE_ANNOUNCE_PANEL_H
