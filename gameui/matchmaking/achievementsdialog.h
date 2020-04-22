//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ACHIEVEMENTSDIALOG_H
#define ACHIEVEMENTSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "tier1/KeyValues.h"
#include "TGAImagePanel.h"

#define MAX_ACHIEVEMENT_GROUPS 25

class IAchievement;

#define ACHIEVED_ICON_PATH "hud/icon_check.vtf"
#define LOCK_ICON_PATH "hud/icon_locked.vtf"

// Loads an achievement's icon into a specified image panel, or turns the panel off if no achievement icon was found.
bool LoadAchievementIcon( vgui::ImagePanel* pIconPanel, IAchievement *pAchievement, const char *pszExt = NULL );

// Updates a listed achievement item's progress bar. 
void UpdateProgressBar( vgui::EditablePanel* pPanel, IAchievement *pAchievement, Color clrProgressBar );

//-----------------------------------------------------------------------------
// Purpose: Simple menu to choose a matchmaking session type
//-----------------------------------------------------------------------------
class CAchievementsDialog_XBox : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CAchievementsDialog_XBox, CBaseDialog ); 

public:
	CAchievementsDialog_XBox(vgui::Panel *parent);
	~CAchievementsDialog_XBox();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	PerformLayout();

	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	HandleKeyRepeated( vgui::KeyCode code );

	virtual void	OnClose();


private:

	vgui::Panel	*m_pProgressBg;

	vgui::Panel *m_pProgressBar;
	vgui::Label *m_pProgressPercent;
	vgui::Label *m_pNumbering;
	vgui::Label	*m_pUpArrow;
	vgui::Label	*m_pDownArrow;

	KeyValues*	m_pResourceData;

	CFooterPanel *m_pFooter;

	bool		m_bCenterOnScreen;
	int			m_iNumItems;
	int			m_nTotalAchievements;	// Total achievements for this title
	int			m_nUnlocked;
	int			m_iSelection;
	int			m_iScroll;
};


//////////////////////////////////////////////////////////////////////////// 
// PC version
//////////////////////////////////////////////////////////////////////////
class CAchievementsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE ( CAchievementsDialog, vgui::Frame );

public:
	CAchievementsDialog( vgui::Panel *parent );
	~CAchievementsDialog();

	virtual void ApplySchemeSettings( IScheme *pScheme );
	void ScrollToItem( int nDirection );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void UpdateAchievementDialogInfo( void );
	virtual void OnCommand( const char* command );

	virtual void ApplySettings( KeyValues *pResourceData );
	virtual void OnSizeChanged( int newWide, int newTall );

	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	void CreateNewAchievementGroup( int iMinRange, int iMaxRange );
	void CreateOrUpdateComboItems( bool bCreate );
	void UpdateAchievementList();

	vgui::PanelListPanel	*m_pAchievementsList;
	vgui::ImagePanel		*m_pListBG;

	vgui::ImagePanel		*m_pPercentageBarBackground;
	vgui::ImagePanel		*m_pPercentageBar;

	vgui::ImagePanel		*m_pSelectionHighlight;

	vgui::ComboBox			*m_pAchievementPackCombo;
	vgui::CheckButton		*m_pHideAchievedCheck;

	int m_nUnlocked;
	int m_nTotalAchievements;

	int m_iFixedWidth;

	typedef struct 
	{
		int m_iMinRange;
		int m_iMaxRange;
		int m_iNumAchievements;
		int m_iNumUnlocked;
		int m_iDropDownGroupID;
	} achievement_group_t;

	int m_iNumAchievementGroups;

	achievement_group_t m_AchievementGroups[ MAX_ACHIEVEMENT_GROUPS ];

	int m_nScrollItem;
	int m_nOldScrollItem;
};

//////////////////////////////////////////////////////////////////////////
// Individual item panel, displaying stats for one achievement
class CAchievementDialogItemPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CAchievementDialogItemPanel, vgui::EditablePanel );

public:
	CAchievementDialogItemPanel( vgui::PanelListPanel *parent, const char* name, int iListItemID );
	~CAchievementDialogItemPanel();

	void SetAchievementInfo ( IAchievement* pAchievement );
	IAchievement* GetAchievementInfo( void ) { return m_pSourceAchievement; }
	void UpdateAchievementInfo( IScheme *pScheme );
	virtual void ApplySchemeSettings( IScheme *pScheme );
	void ToggleShowOnHUD( void );

	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

private:
	void PreloadResourceFile( void );

	IAchievement* m_pSourceAchievement;
	int	m_iSourceAchievementIndex;

	vgui::PanelListPanel *m_pParent;

	vgui::Label *m_pAchievementNameLabel;
	vgui::Label *m_pAchievementDescLabel;
	vgui::Label *m_pPercentageText;

	vgui::ImagePanel *m_pLockedIcon;
	vgui::ImagePanel *m_pAchievementIcon;

	vgui::ImagePanel		*m_pPercentageBarBackground;
	vgui::ImagePanel		*m_pPercentageBar;

	vgui::CheckButton		*m_pShowOnHUDCheck;

	vgui::IScheme			*m_pSchemeSettings;

	CPanelAnimationVar( Color, m_clrProgressBar, "ProgressBarColor", "140 140 140 255" );

	int m_iListItemID;
};

#endif // ACHIEVEMENTSDIALOG_H
