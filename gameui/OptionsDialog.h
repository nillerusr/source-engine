//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyDialog.h"
#include "vgui_controls/KeyRepeat.h"

//-----------------------------------------------------------------------------
// Purpose: Holds all the game option pages
//-----------------------------------------------------------------------------
class COptionsDialog : public vgui::PropertyDialog
{
	DECLARE_CLASS_SIMPLE( COptionsDialog, vgui::PropertyDialog );

public:
	COptionsDialog(vgui::Panel *parent);
	~COptionsDialog();

	void Run();
	virtual void Activate();

	void OnKeyCodePressed( vgui::KeyCode code );

	vgui::PropertyPage* GetOptionsSubMultiplayer( void ) { return m_pOptionsSubMultiplayer; }

	MESSAGE_FUNC( OnGameUIHidden, "GameUIHidden" );	// called when the GameUI is hidden

private:
	class COptionsSubAudio *m_pOptionsSubAudio;
	class COptionsSubVideo *m_pOptionsSubVideo;
	vgui::PropertyPage *m_pOptionsSubMultiplayer;
};


#define OPTIONS_MAX_NUM_ITEMS 15

struct OptionData_t;

//-----------------------------------------------------------------------------
// Purpose: Holds all the game option pages
//-----------------------------------------------------------------------------
class COptionsDialogXbox : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( COptionsDialogXbox, vgui::Frame );

public:
	COptionsDialogXbox( vgui::Panel *parent, bool bControllerOptions = false );
	~COptionsDialogXbox();

	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		ApplySettings( KeyValues *inResourceData );
	virtual void		OnClose();
	virtual void		OnKeyCodePressed( vgui::KeyCode code );
	virtual void		OnCommand(const char *command);

	virtual void		OnKeyCodeReleased( vgui::KeyCode code);
	virtual void		OnThink();

private:
	void	HandleInactiveKeyCodePressed( vgui::KeyCode code );
	void	HandleActiveKeyCodePressed( vgui::KeyCode code );
	void	HandleBindKeyCodePressed( vgui::KeyCode code );

	int		GetSelectionLabel( void ) { return m_iSelection - m_iScroll; }

	void	ActivateSelection( void );
	void	DeactivateSelection( void );

	void	ChangeSelection( int iChange );
	void	UpdateFooter( void );
	void	UpdateSelection( void );
	void	UpdateScroll( void );

	void	UncacheChoices( void );
	void	GetChoiceFromConvar( OptionData_t *pOption );
	void	ChangeValue( float fChange );
	void	UnbindOption( OptionData_t *pOption, int iLabel );

	void	UpdateValue( OptionData_t *pOption, int iLabel );
	void	UpdateBind( OptionData_t *pOption, int iLabel, ButtonCode_t codeIgnore = BUTTON_CODE_INVALID, ButtonCode_t codeAdd = BUTTON_CODE_INVALID );
	void	UpdateAllBinds( ButtonCode_t code );

	void	FillInDefaultBindings( void );

	bool	ShouldSkipOption( KeyValues *pKey );
	void	ReadOptionsFromFile( const char *pchFileName );
	void	SortOptions( void );

	void	InitializeSliderDefaults( void );

private:
	bool	m_bControllerOptions;
	bool	m_bOptionsChanged;
	bool	m_bOldForceEnglishAudio;

	CFooterPanel		*m_pFooter;

	CUtlVector<OptionData_t*>	*m_pOptions;

	bool			m_bSelectionActive;
	OptionData_t	*m_pSelectedOption;

	int		m_iSelection;
	int		m_iScroll;
	int		m_iSelectorYStart;
	int		m_iOptionSpacing;
	int		m_iNumItems;

	int		m_iXAxisState;
	int		m_iYAxisState;
	float	m_fNextChangeTime;

	vgui::Panel			*m_pOptionsSelectionLeft;
	vgui::Panel			*m_pOptionsSelectionLeft2;
	vgui::Label			*m_pOptionsUpArrow;
	vgui::Label			*m_pOptionsDownArrow;

	vgui::Label			*(m_pOptionLabels[ OPTIONS_MAX_NUM_ITEMS ]);
	vgui::Label			*(m_pValueLabels[ OPTIONS_MAX_NUM_ITEMS ]);
	vgui::AnalogBar		*(m_pValueBars[ OPTIONS_MAX_NUM_ITEMS ]);

	vgui::HFont			m_hLabelFont;
	vgui::HFont			m_hButtonFont;

	Color	m_SelectedColor;

	vgui::CKeyRepeatHandler	m_KeyRepeat;

	int m_nButtonGap;
};

#endif // OPTIONSDIALOG_H
