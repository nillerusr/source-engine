#ifndef _INCLUDED_ASW_VGUI_COUNTDOWN_H
#define _INCLUDED_ASW_VGUI_COUNTDOWN_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/EditablePanel.h>
#include "vgui/IScheme.h"
#include "hudelement.h"
#include "c_asw_objective.h"

// This panel shows the countdown timer for a currently active asw_objective_countdown

namespace vgui
{
	class Label;
};

class C_ASW_Objective_Countdown;

class CASW_VGUI_Countdown_Panel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Countdown_Panel, vgui::EditablePanel );
public:
	CASW_VGUI_Countdown_Panel( vgui::Panel *pParent, const char *pElementName, C_ASW_Objective_Countdown *pCountdownObjective );
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	void SlideOut( float flDelay );
	void ShowCountdown( bool bShow );
	
	vgui::Label *m_pCountdownLabel;
	vgui::Label *m_pCountdownGlowLabel;
	vgui::Label *m_pWarningGlowLabel;
	vgui::Label *m_pWarningLabel;

	vgui::Label *m_pDetonationGlowLabel;
	vgui::Label *m_pDetonationLabel;
	vgui::Panel *m_pBackgroundPanel;

	bool m_bPlayed60, m_bPlayed30, m_bPlayed10;
	bool m_bCheckedInitialTime;
	int m_iTimeLeft;
	CHandle<C_ASW_Objective_Countdown> m_hCountdownObjective;
	bool m_bShown;
};

#endif /* _INCLUDED_ASW_VGUI_COUNTDOWN_H */