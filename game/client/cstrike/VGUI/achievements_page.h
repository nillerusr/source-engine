//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSACHIEVEMENTSPAGE_H
#define CSACHIEVEMENTSPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "c_cs_player.h"
#include "vgui_avatarimage.h"
#include "GameEventListener.h"

class CCSBaseAchievement;
class IScheme;
class CAchievementsPageGroupPanel;
class StatCard;

#define ACHIEVED_ICON_PATH "hud/icon_check.vtf"
#define LOCK_ICON_PATH "hud/icon_locked.vtf"

// Loads an achievement's icon into a specified image panel, or turns the panel off if no achievement icon was found.
bool CSLoadAchievementIconForPage( vgui::ImagePanel* pIconPanel, CCSBaseAchievement *pAchievement, const char *pszExt = NULL );

// Loads an achievement's icon into a specified image panel, or turns the panel off if no achievement icon was found.
bool CSLoadIconForPage( vgui::ImagePanel* pIconPanel, const char* pFilename, const char *pszExt = NULL );

// Updates a listed achievement item's progress bar. 
void CSUpdateProgressBarForPage( vgui::EditablePanel* pPanel, CCSBaseAchievement *pAchievement, Color clrProgressBar );

//////////////////////////////////////////////////////////////////////////// 
// PC version
//////////////////////////////////////////////////////////////////////////
class CAchievementsPage : public vgui::PropertyPage, public CGameEventListener
{
    DECLARE_CLASS_SIMPLE ( CAchievementsPage, vgui::PropertyPage );

public:
    CAchievementsPage( vgui::Panel *parent, const char *name );
    ~CAchievementsPage();

    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void UpdateTotalProgressDisplay();
	virtual void UpdateAchievementDialogInfo( void );

	virtual void OnPageShow();
	virtual void OnThink();

    virtual void ApplySettings( KeyValues *pResourceData );
    virtual void OnSizeChanged( int newWide, int newTall );

	virtual void FireGameEvent( IGameEvent *event );

    void CreateNewAchievementGroup( int iMinRange, int iMaxRange );
    void CreateOrUpdateComboItems( bool bCreate );
    void UpdateAchievementList(CAchievementsPageGroupPanel* groupPanel);
    void UpdateAchievementList(int minID, int maxID);

    vgui::PanelListPanel	*m_pAchievementsList;
    vgui::ImagePanel		*m_pListBG;

    vgui::PanelListPanel    *m_pGroupsList;
    vgui::ImagePanel        *m_pGroupListBG;

    vgui::ImagePanel		*m_pPercentageBarBackground;
    vgui::ImagePanel		*m_pPercentageBar;

	StatCard*				m_pStatCard;

    int m_iFixedWidth;

	bool m_bStatsDirty;
	bool m_bAchievementsDirty;

    typedef struct 
    {
        int m_iMinRange;
        int m_iMaxRange;
    } achievement_group_t;

    int m_iNumAchievementGroups;

    achievement_group_t m_AchievementGroups[15];
};

class CHiddenHUDToggleButton : public vgui::Button
{
    DECLARE_CLASS_SIMPLE( CHiddenHUDToggleButton, vgui::Button );

public:

    CHiddenHUDToggleButton(  vgui::Panel *pParent, const char *pName, const char *pText );

    virtual void DoClick( void );
};

//////////////////////////////////////////////////////////////////////////
// Individual item panel, displaying stats for one achievement
class CAchievementsPageItemPanel : public vgui::EditablePanel
{
    DECLARE_CLASS_SIMPLE( CAchievementsPageItemPanel, vgui::EditablePanel );

public:
    CAchievementsPageItemPanel( vgui::PanelListPanel *parent, const char* name);
    ~CAchievementsPageItemPanel();

    void SetAchievementInfo ( CCSBaseAchievement* pAchievement );
    CCSBaseAchievement* GetAchievementInfo( void ) { return m_pSourceAchievement; }
    void UpdateAchievementInfo( vgui::IScheme *pScheme );
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

    void ToggleShowOnHUDButton();

    MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

private:
	static void PreloadResourceFile();

    CCSBaseAchievement*		m_pSourceAchievement;
    int	                    m_iSourceAchievementIndex;

    vgui::PanelListPanel    *m_pParent;

    vgui::Label             *m_pAchievementNameLabel;
    vgui::Label             *m_pAchievementDescLabel;
    vgui::Label             *m_pPercentageText;   
    vgui::Label             *m_pAwardDate;   

    vgui::ImagePanel        *m_pLockedIcon;
    vgui::ImagePanel        *m_pAchievementIcon;

    vgui::ImagePanel        *m_pPercentageBarBackground;
    vgui::ImagePanel        *m_pPercentageBar;

    vgui::CheckButton       *m_pShowOnHUDButton;

    vgui::IScheme           *m_pSchemeSettings;

    CHiddenHUDToggleButton  *m_pHiddenHUDToggleButton;

    CPanelAnimationVar( Color, m_clrProgressBar, "ProgressBarColor", "140 140 140 255" );
};

class CGroupButton : public vgui::Button
{
    DECLARE_CLASS_SIMPLE( CGroupButton, vgui::Button );

public:

    CGroupButton(  vgui::Panel *pParent, const char *pName, const char *pText );

    virtual void DoClick( void );
};

//////////////////////////////////////////////////////////////////////////
// Individual achievement group panel, displaying info for one achievement group
class CAchievementsPageGroupPanel : public vgui::EditablePanel
{
    DECLARE_CLASS_SIMPLE( CAchievementsPageGroupPanel, vgui::EditablePanel );

public:
    CAchievementsPageGroupPanel( vgui::PanelListPanel *parent, CAchievementsPage *owner, const char* name, int iListItemID );
    ~CAchievementsPageGroupPanel();

    void SetGroupInfo ( const wchar_t* name, int firstAchievementID, int lastAchievementID );
    void UpdateAchievementInfo( vgui::IScheme *pScheme );
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

    int GetFirstAchievementID() { return m_iFirstAchievementID; }
    int GetLastAchievementID() { return m_iLastAchievementID; }

    vgui::PanelListPanel* GetParent() { return m_pParent; }
    CAchievementsPage* GetOwner() { return m_pOwner; }

    void SetGroupActive(bool active) { m_bActiveButton = active; }
    bool IsGroupActive() { return m_bActiveButton; }

private:
    void PreloadResourceFile( void );

    vgui::PanelListPanel    *m_pParent;
    CAchievementsPage       *m_pOwner;

    vgui::Label             *m_pAchievementGroupLabel;
    vgui::Label             *m_pPercentageText;

    CGroupButton            *m_pGroupButton;

    vgui::ImagePanel        *m_pGroupIcon;

    vgui::ImagePanel        *m_pPercentageBarBackground;
    vgui::ImagePanel        *m_pPercentageBar;

    vgui::IScheme           *m_pSchemeSettings;

    bool                    m_bActiveButton;

    CPanelAnimationVar( Color, m_clrProgressBar, "ProgressBarColor", "140 140 140 255" );

    int m_iFirstAchievementID;
    int m_iLastAchievementID;

    wchar_t                 *m_pGroupName;
};



#endif // CSACHIEVEMENTSPAGE_H
