#ifndef _INCLUDED_EXPERIENCE_STAT_LINE_H
#define _INCLUDED_EXPERIENCE_STAT_LINE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "asw_player_shared.h"

namespace vgui
{
	class IScheme;
	class ImagePanel;
	class Label;
};
class StatsBar;
class C_ASW_Player;
class BriefingTooltip;

// this is one of the sub-bars on the experience screen
// it shows experience awarded for a particular action (healing, hacking, completing objectives, kills, etc.)

class ExperienceStatLine : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ExperienceStatLine, vgui::EditablePanel );
public:
	ExperienceStatLine(vgui::Panel *parent, const char *name, CASW_Earned_XP_t XPType );
	virtual ~ExperienceStatLine();

	virtual void OnThink();
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );
	void InitFor( C_ASW_Player *pPlayer );
	void UpdateVisibility( C_ASW_Player *pPlayer = NULL );
	virtual void SetVisible(bool state);

	vgui::Label	*m_pStatNum;
	vgui::Label	*m_pTitle;
	vgui::Label *m_pCounter;
	vgui::ImagePanel *m_pIcon;
	StatsBar *m_pStatsBar;

	CASW_Earned_XP_t m_XPType;

	EHANDLE m_hPlayer;
};

// stat line for bonus XP earned from medals + achievements
class MedalStatLine : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( MedalStatLine, vgui::EditablePanel );
public:
	MedalStatLine(vgui::Panel *parent, const char *name );
	virtual ~MedalStatLine();

	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	void SetMedalIndex( int nMedalIndex );
	void SetAchievementIndex( int nAchievementIndex );
	BriefingTooltip* GetTooltip();

	vgui::Label	*m_pTitle;
	vgui::Label	*m_pDescription;
	vgui::Label *m_pCounter;
	vgui::ImagePanel *m_pIcon;

	int m_nAchievementIndex;
	int m_nMedalIndex;
	int m_nXP;

	EHANDLE m_hPlayer;

	char m_szMedalName[64];
	char m_szMedalDescription[64];
	vgui::DHANDLE<BriefingTooltip> m_hTooltip;
	bool m_bShowTooltip;
};

#endif // _INCLUDED_EXPERIENCE_STAT_LINE_H
