#ifndef _INCLUDED_EXPERIENCE_REPORT_H
#define _INCLUDED_EXPERIENCE_REPORT_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class ExperienceBar;
class ExperienceStatLine;
class SkillAnimPanel;
class MedalStatLine;
class CNB_Island;

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::Label;
class WeaponUnlockPanel;
// == MANAGED_CLASS_DECLARATIONS_END ==

#define ASW_EXPERIENCE_REPORT_MAX_PLAYERS 4
#define ASW_EXPERIENCE_REPORT_STAT_LINES 7

// shows experience earned breakdown, weapons unlocked, etc.

class CExperienceReport : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CExperienceReport, vgui::EditablePanel );
public:
	CExperienceReport(vgui::Panel *parent, const char *name);
	virtual ~CExperienceReport();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void PerformLayout();
	virtual void OnThink();

	void Init();
	void UpdateMedals();

	CNB_Island *m_pXPBreakdownBackground;
	ExperienceBar *m_pExperienceBar[ ASW_EXPERIENCE_REPORT_MAX_PLAYERS ];
	ExperienceStatLine *m_pStatLine[ ASW_EXPERIENCE_REPORT_STAT_LINES ];
	SkillAnimPanel *m_pSkillAnim;

	CUtlVector<MedalStatLine*> m_pMedalLines;

	vgui::Label *m_pXPDifficultyScaleTitle;
	vgui::Label *m_pXPDifficultyScaleNumber;

	vgui::Label *m_pEarnedXPTitle;
	vgui::Label *m_pEarnedXPNumber;

	vgui::Label *m_pCheatsUsedLabel;
	vgui::Label *m_pUnofficialMapLabel;
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	WeaponUnlockPanel *m_pWeaponUnlockPanel;
	vgui::Label *m_pMedalsTitle;
	// == MANAGED_MEMBER_POINTERS_END ==

	int m_iPlayerLevel;
	const char* m_pszWeaponUnlockClass;
	float m_flOldBarMin;
	bool m_bOldCapped;

	bool m_bDoneAnimating;
	bool m_bPendingUnlockSequence;
	float m_flUnlockSequenceStart;

	int m_nPendingSuggestDifficulty;
	float m_flSuggestDifficultyStart;

	char m_szMedalString[255];
};

#endif // _INCLUDED_EXPERIENCE_REPORT_H
