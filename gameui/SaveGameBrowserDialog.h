//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SAVEGAMEBROWSERDIALOG_H
#define SAVEGAMEBROWSERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/KeyRepeat.h"

#include "BasePanel.h"

class CSaveGameBrowserDialog;

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: selectable item with screenshot for an individual chapter in the dialog
//-----------------------------------------------------------------------------
class CGameSavePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CGameSavePanel, vgui::EditablePanel );

public:
	
					CGameSavePanel( CSaveGameBrowserDialog *parent, SaveGameDescription_t *pSaveDesc, bool bCommandPanel = false );
					~CGameSavePanel( void );

	virtual	void	ApplySchemeSettings( IScheme *pScheme );
	
	bool IsAutoSaveType( void ) { return ( Q_stristr( m_SaveInfo.szType, "autosave" ) != 0 ); }

	const SaveGameDescription_t *GetSaveInfo( void ) { return ( const SaveGameDescription_t * ) &m_SaveInfo; }
	void SetDescription( SaveGameDescription_t *pDesc );

protected:

	SaveGameDescription_t	m_SaveInfo;	// Stored internally for easy access

	ImagePanel	*m_pLevelPicBorder;
	ImagePanel	*m_pLevelPic;
	ImagePanel	*m_pCommentaryIcon;
	Label		*m_pChapterTitle;
	Label		*m_pTime;
	Label		*m_pElapsedTime;
	Label		*m_pType;

	Color	m_TextColor;
	Color	m_DisabledColor;
	Color	m_SelectedColor;
	Color	m_FillColor;

	bool	m_bNewSavePanel;
};

// Slot indices in new game menu
#define INVALID_INDEX	-1
#define SLOT_OFFLEFT	0
#define SLOT_LEFT		1
#define SLOT_CENTER		2
#define SLOT_RIGHT		3
#define SLOT_OFFRIGHT	4
#define	NUM_SLOTS		5

//-----------------------------------------------------------------------------
// Purpose: Handles starting a new game, skill and chapter selection
//-----------------------------------------------------------------------------
class CSaveGameBrowserDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CSaveGameBrowserDialog, vgui::Frame );

public:
	MESSAGE_FUNC( FinishScroll,	"FinishScroll" );
	MESSAGE_FUNC( FinishDelete,	"FinishDelete" );
	MESSAGE_FUNC( FinishInsert,	"FinishInsert" );
	MESSAGE_FUNC( FinishOverwriteFadeDown, "FinishOverwriteFadeDown" );
	MESSAGE_FUNC( CloseAfterSave, "CloseAfterSave" );

	CSaveGameBrowserDialog(vgui::Panel *parent );
	~CSaveGameBrowserDialog();

	virtual void	OnKeyCodePressed( vgui::KeyCode code );

	virtual void	Activate( void );
	virtual void	ApplySettings( KeyValues *inResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnClose( void );
	virtual void	PaintBackground();
	virtual void	PerformSelectedAction( void );
	virtual void	PerformDeletion( void );
	virtual void	UpdateFooterOptions( void );
	virtual void	SortSaveGames( SaveGameDescription_t *pSaves, unsigned int nNumSaves );
	virtual void	OnThink( void );
	virtual void	OnKeyCodeReleased( vgui::KeyCode code );
	virtual void	OnDoneScanningSaveGames( void ) {}
	virtual void	RefreshSaveGames( void );
	
	unsigned int	GetNumPanels( void ) { return m_SavePanels.Count(); }
	bool			HasActivePanels( void ) { return ( m_SavePanels.Count() != 0 ); }
	CGameSavePanel	*GetActivePanel( void );
	int				GetActivePanelIndex( void ) { return m_iSelectedSave; }
	CFooterPanel	*GetFooterPanel( void ) { return m_pFooter; }
	const SaveGameDescription_t *GetPanelSaveDecription( int idx ) { return ( IsValidPanel(idx) ? m_SavePanels[idx]->GetSaveInfo() : NULL ); }
	const SaveGameDescription_t *GetActivePanelSaveDescription( void ) { return GetPanelSaveDecription( m_iSelectedSave ); }

	uint			GetStorageSpaceUsed( void ) { return m_nUsedStorageSpace; }

	void			SetSelectedSaveIndex( int index );
	void			SetSelectedSave( const char *chapter );
	void			AddPanel( CGameSavePanel *pPanel ) { m_SavePanels.AddToHead( pPanel ); }
	
	void			RemoveActivePanel( void );
	void			AnimateInsertNewPanel( const SaveGameDescription_t *pDesc );
	void			AnimateOverwriteActivePanel( const SaveGameDescription_t *pNewDesc );

	void			SetControlDisabled( bool bState ) { m_bControlDisabled = bState; }

	// Xbox: Defined values are also used to shift the slot indices
	enum EScrollDirection
	{
		SCROLL_RIGHT	= -1,
		SCROLL_NONE		=  0,
		SCROLL_LEFT		=  1
	};
	EScrollDirection	m_ScrollDirection;

protected:

	bool				m_bFilterAutosaves;
	CKeyRepeatHandler	m_KeyRepeat;

	bool				ParseSaveData( char const *pszFileName, char const *pszShortName, SaveGameDescription_t *save );

private:
	
	CUtlVector<CGameSavePanel *>	m_SavePanels;
	int								m_iSelectedSave;
	float							m_ScrollSpeedSlow;
	float							m_ScrollSpeedFast;
	int								m_nDeletedPanel;	// Panel being subtracted
	int								m_nAddedPanel;		// Panel being added
	SaveGameDescription_t			m_NewSaveGameDesc;	// Held for panel animations
	uint							m_nUsedStorageSpace;	// Amount of disk space used by save games 
	
	vgui::Panel			*m_pCenterBg;
	CFooterPanel		*m_pFooter;

	// Xbox
	void	ScrollSelectionPanels( EScrollDirection dir );
	void	PreScroll( EScrollDirection dir );
	void	PostScroll( EScrollDirection dir );
	void	SetFastScroll( bool fast );
	void	ContinueScrolling( void );
	void	AnimateSelectionPanels( void );
	void	ShiftPanelIndices( int offset );
	bool	IsValidPanel( const int idx );
	void	InitPanelIndexForDisplay( const int idx );
	void	UpdateMenuComponents( EScrollDirection dir );
	void	ScanSavedGames( bool bIgnoreAutosave );
	void	LayoutPanels( void );
	
	// "No Save Games" label
	void	ShowNoSaveGameUI( void );
	void	HideNoSaveGameUI( void );
	void	PerformSlideAction( int nPanelIndex, int nNextPanelIndex );
	void	AnimateDialogStart( void );

	int		m_nCenterBgTallDefault;
	int		m_PanelXPos[ NUM_SLOTS ];
	int		m_PanelYPos[ NUM_SLOTS ];
	float	m_PanelAlpha[ NUM_SLOTS ];
	int		m_PanelIndex[ NUM_SLOTS ];
	float	m_ScrollSpeed;
	int		m_ButtonPressed;
	int		m_ScrollCt;
	bool	m_bScrolling : 1;
	bool	m_bSaveGameIsCorrupt : 1;
	bool	m_bControlDisabled : 1;
};


#endif // SAVEGAMEBROWSERDIALOG_H