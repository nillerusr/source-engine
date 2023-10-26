#ifndef _INCLUDED_OBJECTIVE_MAP_MARK_PANEL_H
#define _INCLUDED_OBJECTIVE_MAP_MARK_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

// responsible for drawing brackets/crosses on the map


class ObjectiveMapMarkPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveMapMarkPanel, vgui::Panel );
public:
	ObjectiveMapMarkPanel(Panel *parent, const char *name);
	virtual ~ObjectiveMapMarkPanel();
	void SetBracketScale(float f);

	virtual void Paint();
	virtual void Think();
	float m_fScale;
	int m_iBracketSize;
	bool m_bPulsing;
	bool m_bComplete;

	CPanelAnimationVarAliasType( int, m_nTopLeftBracketTexture, "TopLeftBracketTexture", "vgui/swarm/Briefing/TopLeftBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nTopRightBracketTexture, "TopRightBracketTexture", "vgui/swarm/Briefing/TopRightBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nBottomLeftBracketTexture, "BottomLeftBracketTexture", "vgui/swarm/Briefing/BottomLeftBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nBottomRightBracketTexture, "BottomRightBracketTexture", "vgui/swarm/Briefing/BottomRightBracket", "textureid" );
};

#endif // _INCLUDED_OBJECTIVE_MAP_MARK_PANEL_H
