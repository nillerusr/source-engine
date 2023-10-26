#ifndef _INCLUDED_STAT_GRAPH_PLAYER_H
#define _INCLUDED_STAT_GRAPH_PLAYER_H

#ifdef _WIN32
#pragma once
#endif


#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

#include "stats_report.h"


namespace vgui
{
	class Label;
};

class ObjectiveMap;
class StatGraph;


#define ASW_STAT_GRAPH_VALUE_STAMPS 5
#define ASW_STAT_GRAPH_TIME_STAMPS 5


// shows stats breakdowns

class StatGraphPlayer : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( StatGraphPlayer, vgui::EditablePanel );
public:
	StatGraphPlayer( vgui::Panel *parent, const char *name );
	virtual ~StatGraphPlayer();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void PerformLayout();
	virtual void OnThink();

	StatGraph *m_pStatGraphs[ ASW_STATS_REPORT_MAX_PLAYERS ];

	vgui::Panel *m_pValueBox;
	vgui::Label *m_pValueStamps[ ASW_STAT_GRAPH_VALUE_STAMPS ];

	vgui::Panel *m_pTimeLine;
	vgui::Panel *m_pTimeScrub;
	vgui::Panel *m_pTimeScrubBar;
	vgui::Label *m_pTimeStamps[ ASW_STAT_GRAPH_TIME_STAMPS ];

	float m_fTimeInterp;
	bool m_bPaused;
};

#endif // _INCLUDED_STAT_GRAPH_PLAYER_H
