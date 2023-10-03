//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VACHIEVEMENTS_H__
#define __VACHIEVEMENTS_H__

#include "basemodui.h"
#include "VGenericPanelList.h"
#include "vgui_controls/ProgressBar.h"

class IAchievement;

/*
// TODO: this is a temporary merge workaround until achievment interfaces get merged properly
// REMOVE THIS BLOCK AFTER ACHIEVEMENTS GET MERGED
class CBaseAchievement;
abstract_class IAchievement_TempMerge_CrashCrash
{
public:
	virtual int GetAchievementID() = 0;
	virtual const char *GetName() = 0;
	virtual int GetFlags() = 0;
	virtual int GetGoal() = 0;
	virtual int GetCount() = 0;
	virtual bool IsAchieved() = 0;
	virtual int GetPointValue() = 0;
	virtual bool ShouldSaveWithGame() = 0;
	virtual bool ShouldHideUntilAchieved() = 0;
	virtual const char *GetIconPath() = 0;
	virtual int GetDisplayOrder() = 0;
	virtual int GetNumComponents() = 0;
	virtual const char *GetComponentDisplayString( int iComponent ) = 0;
	virtual uint64 GetComponentBits() = 0;
};
#define IAchievement IAchievement_TempMerge_CrashCrash
abstract_class IAchievementMgr_TempMerge_CrashCrash
{
public:
	virtual IAchievement* GetAchievementByIndex( int index, int nPlayerSlot ) = 0;
	virtual IAchievement* GetAchievementByDisplayOrder( int orderIndex, int nPlayerSlot ) = 0;
	virtual IAchievement* GetAwardByDisplayOrder( int orderIndex, int nPlayerSlot ) = 0;
	virtual CBaseAchievement* GetAchievementByID ( int id, int nPlayerSlot ) = 0;
	virtual int GetAchievementCount( bool bAssets = false ) = 0;
	virtual void InitializeAchievements( ) = 0;
	virtual void AwardAchievement( int nAchievementID, int nPlayerSlot ) = 0;
	virtual void OnMapEvent( const char *pchEventName, int nPlayerSlot ) = 0;
	virtual void SaveGlobalStateIfDirty( ) = 0;
	virtual bool HasAchieved( const char *pchName, int nPlayerSlot ) = 0;
	virtual const CUtlVector<int>& GetAchievedDuringCurrentGame( int nPlayerSlot ) = 0;
};
// -- END TODO
*/

namespace BaseModUI {

class AchievementGenericPanelList;

class AchievementListItem : public vgui::EditablePanel, IGenericPanelListItem
{
	DECLARE_CLASS_SIMPLE( AchievementListItem, vgui::EditablePanel );

public:
	AchievementListItem( IAchievement *pAchievement );

	int GetGoal() const;
	int GetProgress() const;
	bool GetCompleted() const;
	int GetGamerScore() const;

	// Inherited from IGenericPanelListItem
	virtual bool IsLabel() { return false; }

protected:
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	void PerformLayout();
	void Paint(void);
	virtual void OnCommand( const char *command );

	virtual void OnSizeChanged( int newWide, int newTall );

	virtual void NavigateTo();

private:
	void SetAchievement( IAchievement *pAchievement );
	void SetAchievementName( const wchar_t* name );
	void SetAchievementHowTo( const wchar_t* howTo );
	void SetAchievementIcon(const char* iconName);
	void SetAchievementProgress(int progress);
	void SetAchievementGoal(int goal);
	void SetGamerScore(int score);

private:
	IAchievement *m_pAchievement;

	vgui::Label* m_LblName;
	vgui::Label* m_LblProgress;
	vgui::Divider* m_DivTitleDivider;
	vgui::ImagePanel* m_ImgAchievementIcon;
	vgui::Label* m_LblHowTo;
	vgui::ContinuousProgressBar* m_PrgProgress;
	vgui::Label* m_LblCurrProgress;
	vgui::Label* m_LblGamerscore;

	vgui::IBorder* m_DefaultBorder;
	vgui::IBorder* m_FocusBorder;

	int m_AchievementProgress;
	int m_AchievementGoal;
	int m_GamerScore;

	CPanelAnimationVarAliasType( float, m_flDetailsExtraHeight, "DetailsExtraHeight", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDetailsRowHeight, "DetailsRowHeight", "0", "proportional_float" );

	bool m_bShowingDetails;
	int m_iOriginalTall;
};


class AchievementListItemLabel : public vgui::EditablePanel, public IGenericPanelListItem
{
	DECLARE_CLASS_SIMPLE( AchievementListItemLabel, vgui::EditablePanel );

public:
	AchievementListItemLabel( vgui::Panel *parent, const char *panelName );
	~AchievementListItemLabel();

	void SetCategory( const wchar_t* category );

	// Inherited from IGenericPanelListItem
	virtual bool IsLabel() { return true; }

private:
	vgui::Label* m_LblCategory;
};


class Achievements : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( Achievements, CBaseModFrame );

public:
	Achievements(vgui::Panel *parent, const char *panelName);
	~Achievements();
	void Activate();
	void OnCommand(const char *command);
	void OnKeyCodePressed(vgui::KeyCode code);

#ifdef _X360
	virtual void NavigateTo();
	virtual void NavigateFrom();
#endif

	void PaintBackground( void );

	void ToggleDisplayType( bool bDisplayType );


protected:
	enum ACHIEVEMENT_FILTER { AF_ALL, AF_COMPLETED, AF_INCOMPLETE, AF_SURVIVORS, AF_INFECTED };
	enum ACHIEVEMENT_SORT { AS_COMPLETED, AS_INCOMPLETE, AS_GAMERSCORE };
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	int m_iStartingUserSlot;

private:
	void UpdateFooter();

	vgui::Label* m_LblComplete;
	vgui::Label* m_LblGamerscore;
	vgui::Label* m_LblScrollProgress;
	AchievementGenericPanelList* m_GplAchievements;
	vgui::ContinuousProgressBar* m_pProgressBar;
	ACHIEVEMENT_FILTER m_AchievementFilter;
	ACHIEVEMENT_SORT m_AchievementSort;

	// Awards
	AchievementGenericPanelList* m_GplAwards;

	float m_flTotalProgress;
	wchar_t m_wAchievementsTitle[128];
	wchar_t m_wAchievementsProgress[128];
	bool m_bShowingAssets;

	int m_iAwardCompleteCount;
	int m_iAchCompleteCount;
};

};

#endif // __VACHIEVEMENTS_H__