#ifndef _INCLUDED_STATS_REPORT_H
#define _INCLUDED_STATS_REPORT_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "steam/steam_api.h"

namespace vgui
{
	class Label;
	class Button;
};

class ObjectiveMap;
class StatGraphPlayer;
class DebriefTextPage;
class CAvatarImagePanel;


#define ASW_STATS_REPORT_MAX_PLAYERS 4
#define ASW_STATS_REPORT_CATEGORIES 4


// shows stats breakdowns

class StatsReport : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( StatsReport, vgui::EditablePanel );
public:
	StatsReport(vgui::Panel *parent, const char *name);
	virtual ~StatsReport();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );

	void SetStatCategory( int nCategory );
	void SetPlayerNames( void );

	ObjectiveMap *m_pObjectiveMap;
	vgui::Button *m_pCategoryButtons[ ASW_STATS_REPORT_CATEGORIES ];
	vgui::Label *m_pPlayerNames[ ASW_STATS_REPORT_MAX_PLAYERS ];
	vgui::ImagePanel *m_pReadyCheckImages[ ASW_STATS_REPORT_MAX_PLAYERS ];
	CAvatarImagePanel *m_pAvatarImages[ ASW_STATS_REPORT_MAX_PLAYERS ];
	vgui::Panel *m_pPlayerNamesPosition;
	StatGraphPlayer *m_pStatGraphPlayer;

	DebriefTextPage *m_pDebrief;

	Color m_rgbaStatsReportPlayerColors[ ASW_STATS_REPORT_MAX_PLAYERS ];
	float m_fPlayerNamePosY[ ASW_STATS_REPORT_MAX_PLAYERS ];
};

#endif // _INCLUDED_STATS_REPORT_H
