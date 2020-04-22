//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DIALOGMENU_H
#define DIALOGMENU_H
#ifdef _WIN32
#pragma once
#endif

#if defined(_WIN32) && !defined(_X360)
#include "winlite.h"		// FILETIME
#endif

#include "vgui_controls/Panel.h"
#include "vgui_controls/Frame.h"

class IAchievement;

#define MAX_COMMAND_LEN		256
#define MAX_COLUMNS			32

class CDialogMenu;
class CBaseDialog;

struct sessionProperty_t
{
	static const int MAX_KEY_LEN = 64;
	byte		nType;
	char		szID[MAX_KEY_LEN];
	char		szValue[MAX_KEY_LEN];
	char		szValueType[MAX_KEY_LEN];
};

//-----------------------------------------------------------------------
// Base class representing a generic menu item. Supports two text labels,
// where the first label is the "action" text and the second is an optional
// description of the action.
//-----------------------------------------------------------------------
class CMenuItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMenuItem, vgui::EditablePanel ); 

public:
	CMenuItem( CDialogMenu *pParent, const char *pTitle, const char *pDescription );
	virtual ~CMenuItem();

	virtual void PerformLayout();
	virtual void ApplySettings( KeyValues *pSettings );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void SetFocus( const bool bActive );
	virtual void SetEnabled( bool bEnabled );
	virtual void SetActiveColumn( int col );
	virtual bool IsEnabled();
	virtual void OnClick();

protected:
	CDialogMenu *m_pParent;

	vgui::Label *m_pTitle;
	vgui::Label *m_pDescription;

	Color		m_BgColor;
	Color		m_BgColorActive;
	
	int			m_nDisabledAlpha;
	int			m_nBottomMargin;
	int			m_nRightMargin;

	bool		m_bEnabled;
};

//-----------------------------------------------------------------------
// CCommandItem
//
// Menu item that issues a command when clicked.
//-----------------------------------------------------------------------
class CCommandItem : public CMenuItem
{
	DECLARE_CLASS_SIMPLE( CCommandItem, CMenuItem ); 

public:
	CCommandItem( CDialogMenu *pParent, const char *pTitle, const char *pDescription, const char *pCommand );
	virtual ~CCommandItem();

	virtual void OnClick();
	virtual void SetFocus( const bool bActive );

	bool m_bHasFocus;

	char m_szCommand[MAX_PATH];
};

//-----------------------------------------------------------------------
// CPlayerItem
//
// Menu item to display a player in the lobby.
//-----------------------------------------------------------------------
class CPlayerItem : public CCommandItem
{
	DECLARE_CLASS_SIMPLE( CMenuItem, CCommandItem ); 

public:
	CPlayerItem( CDialogMenu *pParent, const char *pTitle, int64 nId, byte bVoice, bool bReady );
	virtual ~CPlayerItem();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void OnClick();

	vgui::Label	*m_pVoiceIcon;
	vgui::Label *m_pReadyIcon;

	byte		m_bVoice;
	bool		m_bReady;
	uint64		m_nId;
};

//-----------------------------------------------------------------------
// CBrowserItem
//
// Menu item used to display session search results, etc.
//-----------------------------------------------------------------------
class CBrowserItem : public CCommandItem
{
	DECLARE_CLASS_SIMPLE( CBrowserItem, CCommandItem ); 

public:
	CBrowserItem( CDialogMenu *pParent, const char *pHost, const char *pPlayers, const char *pScenario, const char *pPing );
	virtual ~CBrowserItem();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *pSettings );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	
private:
	vgui::Label	*m_pPlayers;
	vgui::Label *m_pScenario;
	vgui::Label *m_pPing;
};

//-----------------------------------------------------------------------
// COptionsItem
//
// Menu item used to present a list of options for the player to select
// from, such as "choose a map" or "number of rounds".
//-----------------------------------------------------------------------
class COptionsItem : public CMenuItem
{
	DECLARE_CLASS_SIMPLE( COptionsItem, CMenuItem );

public:
	COptionsItem( CDialogMenu *pParent, const char *pLabel );
	virtual ~COptionsItem();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *pSettings );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	SetFocus( const bool bActive );

	void			SetOptionFocus( unsigned int idx );
	void			SetOptionFocusNext();
	void			SetOptionFocusPrev();

	void					AddOption( const char *pLabelText, const sessionProperty_t &option );
	int						GetActiveOptionIndex();
	const sessionProperty_t &GetActiveOption();

	void			DeleteAllOptions()
	{
		m_Options.RemoveAll();
		m_OptionLabels.PurgeAndDeleteElements();
		m_nActiveOption = m_Options.InvalidIndex();
	}
private:
	int				m_nActiveOption;
	int				m_nOptionsXPos;
	int				m_nOptionsMinWide;
	int				m_nOptionsLeftMargin;
	int				m_nMaxOptionWidth;
	int				m_nArrowGap;

	CUtlVector< vgui::Label* >		m_OptionLabels;
	CUtlVector< sessionProperty_t >	m_Options;

	char			m_szOptionsFont[64];
	vgui::HFont		m_hOptionsFont;

	vgui::Label		*m_pLeftArrow;
	vgui::Label		*m_pRightArrow;
};

//-----------------------------------------------------------------------
// CAchievementItem
//
// Menu item used to present an achievement - including image, title,
// description, points and unlock date. Clicking the item opens another
// dialog with additional information about the achievement.
//-----------------------------------------------------------------------
class CAchievementItem : public CMenuItem
{
	DECLARE_CLASS_SIMPLE( CAchievementItem, CMenuItem );

public:
	CAchievementItem( CDialogMenu *pParent, const wchar_t *pName, const wchar_t *pDesc, uint points, bool bUnlocked, IAchievement* pSourceAchievement );
	virtual ~CAchievementItem();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	vgui::Label			*m_pPoints;
	vgui::ImagePanel	*m_pLockedIcon;
	vgui::ImagePanel	*m_pUnlockedIcon;
	vgui::ImagePanel	*m_pImage;

	vgui::ImagePanel	*m_pPercentageBarBackground;
	vgui::ImagePanel	*m_pPercentageBar;
	vgui::Label			*m_pPercentageText;

	IAchievement		*m_pSourceAchievement;

	Color				m_AchievedBGColor;
	Color				m_UnachievedBGColor;

	CPanelAnimationVar( Color, m_clrProgressBar, "ProgressBarColor", "140 140 140 255" );
};

//-----------------------------------------------------------------------
// CSectionedItem
//
// Menu item used to display some number of data entries, which are arranged
// into columns.  Supports scrolling through columns horizontally with the 
// ability to "lock" columns so they don't scroll
//-----------------------------------------------------------------------
class CSectionedItem : public CCommandItem
{
	DECLARE_CLASS_SIMPLE( CSectionedItem, CCommandItem );

public:
	CSectionedItem( CDialogMenu *pParent, const char **ppEntries, int ct );
	virtual ~CSectionedItem();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *pSettings );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void	SetActiveColumn( int col );

	void			ClearSections();
	void			AddSection( const char *pText, int wide );

	struct section_s
	{
		int	wide;
		vgui::Label *pLabel;
	};
	CUtlVector< section_s >m_Sections;

	bool m_bHeader;
};

//--------------------------------------------------------------------------------------
// Generic menu for Xbox 360 matchmaking dialogs. Contains a list of CMenuItems arranged
// vertically. The user can navigate the list using the controller and click on any
// item. A clicked item may send a command to the dialog and the dialog responds accordingly.
//--------------------------------------------------------------------------------------
class CDialogMenu : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDialogMenu, vgui::Panel ); 

public:
	CDialogMenu();
	~CDialogMenu();

	virtual void		OnCommand( const char *pCommand );
	virtual void		ApplySettings( KeyValues *inResourceData );
	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		PerformLayout();
	void				SetFilter( const char *pFilter );
	virtual bool		HandleKeyCode( vgui::KeyCode code );
	void				SetMaxVisibleItems( uint nMaxVisibleItems );
	void				SetParent( CBaseDialog *pParent );

	// Menu items
	CCommandItem		*AddCommandItem( const char *pTitleLabel, const char *pDescLabel, const char *pCommand );
	CPlayerItem			*AddPlayerItem( const char *pTitleLabel, int64 nId, byte bVoice, bool bReady );
	CBrowserItem		*AddBrowserItem( const char *pHost, const char *pPlayers, const char *pScenario, const char *pPing );
	COptionsItem		*AddOptionsItem( const char *pLabel );
	CSectionedItem		*AddSectionedItem( const char **ppEntries, int ct );
	CAchievementItem	*AddAchievementItem( const wchar_t *pName, const wchar_t *pDesc, uint cred, bool bUnlocked, IAchievement* pSourceAchievement );
	CMenuItem			*AddItemInternal( CMenuItem *pItem );

	void				RemovePlayerItem( int idx );
	void				SortMenuItems();
	void				ClearItems();

	// Navigation
	void				SetFocus( int idx );
	void				SetFocusNext();
	void				SetFocusPrev();
	void				SetOptionFocusNext();
	void				SetOptionFocusPrev();
	void				SetColumnFocusNext();
	void				SetColumnFocusPrev();
	void				UpdateBaseColumnIndex();

	// Accessors
	CMenuItem			*GetItem( int idx);
	int					GetItemCount();
	int					GetActiveItemIndex();
	int					GetActiveColumnIndex();
	int					GetActiveOptionIndex( int idx );
	int					GetVisibleItemCount();
	int					GetVisibleColumnCount();
	int					GetFirstUnlockedColumnIndex();
	int					GetBaseRowIndex();
	void				SetBaseRowIndex( int idx );
	int					GetColumnXPos( int idx );
	int					GetColumnYPos( int idx );
	int					GetColumnWide( int idx );
	int					GetColumnAlignment( int idx );
	vgui::HFont			GetColumnFont( int idx );
	Color				GetColumnColor( int idx );
	bool				GetColumnSortType( int idx );

private:
	struct columninfo_s
	{
		int			xpos;
		int			ypos;
		int			wide;
		int			align;
		bool		bLocked;
		Color		color;
		vgui::HFont	hFont;
		bool		bSortDown;
	};
	CUtlVector< columninfo_s >m_Columns;
	CUtlVector< CMenuItem* > m_MenuItems;

	CBaseDialog		*m_pParent;
	CSectionedItem	*m_pHeader;
	vgui::IScheme	*m_pScheme;

	char	m_szFilter[MAX_COMMAND_LEN];	// string to use as a keyvalues filter when reading in menu items

	int		m_nItemSpacing;			// gap between menu items
	int		m_nMinWide;				// minimum width - final menu width will always be >= m_nMinWide

	bool	m_bInitialized;
	bool	m_bUseFilter;
	bool	m_bHasHeader;
	int		m_nMaxVisibleItems;		// max number of items to display in the menu
	int		m_nMaxVisibleColumns;	// max number of columns to display in the menu
	int		m_nActiveColumn;		// index of the current active column
	int		m_nBaseColumnIdx;		// array index of the first non-static column
	int		m_nBaseRowIdx;			// array index of the first visible row
	int		m_nActive;				// index of the current active item
	int		m_iUnlocked;			// first unlocked column in the menu
};

#endif	// DIALOGMENU_H
