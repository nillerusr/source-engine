//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CONTROLS_H
#define TF_CONTROLS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>
#include <vgui/IVGui.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>
#include "utlvector.h"
#include "vgui_controls/PHandle.h"
#include <vgui_controls/Tooltip.h>
#include "econ_controls.h"
#include "sc_hinticon.h"
#if defined( TF_CLIENT_DLL )
#include "tf_shareddefs.h"
#include "tf_imagepanel.h"
#endif
#include <vgui_controls/Frame.h>
#include <../common/GameUI/scriptobject.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/CheckButton.h>

wchar_t* LocalizeNumberWithToken( const char* pszLocToken, int nValue );

//-----------------------------------------------------------------------------
// Purpose: Xbox-specific panel that displays button icons text labels
//-----------------------------------------------------------------------------
class CTFFooter : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFFooter, vgui::EditablePanel );

public:
	CTFFooter( Panel *parent, const char *panelName );
	virtual ~CTFFooter();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	Paint( void );
	virtual void	PaintBackground( void );

	void			ShowButtonLabel( const char *name, bool show = true );
	void			AddNewButtonLabel( const char *name, const char *text, const char *icon );
	void			ClearButtons();

private:
	struct FooterButton_t
	{
		bool	bVisible;
		char	name[MAX_PATH];
		wchar_t	text[MAX_PATH];
		wchar_t	icon[3];			// icon can be one or two characters
	};

	CUtlVector< FooterButton_t* > m_Buttons;

	bool			m_bPaintBackground;		// fill the background?
	int				m_nButtonGap;			// space between buttons
	int				m_FooterTall;			// height of the footer
	int				m_ButtonOffsetFromTop;	// how far below the top the buttons should be drawn
	int				m_ButtonSeparator;		// space between the button icon and text
	int				m_TextAdjust;			// extra adjustment for the text (vertically)...text is centered on the button icon and then this value is applied
	bool			m_bCenterHorizontal;	// center buttons horizontally?
	int				m_ButtonPinRight;		// if not centered, this is the distance from the right margin that we use to start drawing buttons (right to left)

	char			m_szTextFont[64];		// font for the button text
	char			m_szButtonFont[64];		// font for the button icon
	char			m_szFGColor[64];		// foreground color (text)
	char			m_szBGColor[64];		// background color (fill color)

	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hTextFont;
};

//-----------------------------------------------------------------------------
// Purpose: Tooltip for the main menu. Isn't a panel, it just wraps the 
// show/hide/position handling for the embedded panel.
//-----------------------------------------------------------------------------
class CMainMenuToolTip : public vgui::BaseTooltip 
{
	DECLARE_CLASS_SIMPLE( CMainMenuToolTip, vgui::BaseTooltip );
public:
	CMainMenuToolTip(vgui::Panel *parent, const char *text = NULL) : vgui::BaseTooltip( parent, text )
	{
		m_pEmbeddedPanel = NULL;
	}
	virtual ~CMainMenuToolTip() {}

	virtual void SetText(const char *text);
	const char *GetText() { return NULL; }

	virtual void HideTooltip();
	virtual void PerformLayout();

	void SetEmbeddedPanel( vgui::EditablePanel *pPanel )
	{
		m_pEmbeddedPanel = pPanel;
	}

protected:
	vgui::EditablePanel	*m_pEmbeddedPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Simple TF-styled text tooltip
//-----------------------------------------------------------------------------
class CTFTextToolTip : public CMainMenuToolTip 
{
	DECLARE_CLASS_SIMPLE( CTFTextToolTip, CMainMenuToolTip );
public:
	CTFTextToolTip(vgui::Panel *parent, const char *text = NULL) : CMainMenuToolTip( parent, text )
	{
	}
	virtual void PerformLayout();
	virtual void PositionWindow( vgui::Panel *pTipPanel );
	virtual void SetText(const char *text)
	{
		_isDirty = true;
		BaseClass::SetText( text );
	}
};


//-----------------------------------------------------------------------------
// Purpose: Displays a TF specific list of options
//			This is essentially a TF-styled version of the GameUI Advanced Multiplayer Options Dialog
//-----------------------------------------------------------------------------
class CTFAdvancedOptionsDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFAdvancedOptionsDialog, vgui::EditablePanel ); 

public:
	CTFAdvancedOptionsDialog(vgui::Panel *parent);
	~CTFAdvancedOptionsDialog();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );

	void	Deploy( void );

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	CInfoDescription	*m_pDescription;
	mpcontrol_t			*m_pList;
	vgui::PanelListPanel *m_pListPanel;
	CTFTextToolTip		*m_pToolTip;
	vgui::EditablePanel	*m_pToolTipEmbeddedPanel;

	CPanelAnimationVarAliasType( int, m_iControlW, "control_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iControlH, "control_h", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSliderW, "slider_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSliderH, "slider_h", "0", "proportional_int" );
};

//-----------------------------------------------------------------------------
// Purpose: Scrollable panel where you can define children within the .res file
//-----------------------------------------------------------------------------
class CExScrollingEditablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CExScrollingEditablePanel, vgui::EditablePanel );
public:
	CExScrollingEditablePanel( Panel *pParent, const char *pszName );
	virtual ~CExScrollingEditablePanel();

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnSizeChanged( int newWide, int newTall ) OVERRIDE;

	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );
	virtual void OnMouseWheeled( int delta ) OVERRIDE;	// respond to mouse wheel events
	void ResetScrollAmount() { m_nLastScrollValue = 0; m_pScrollBar->SetValue(0); }
protected:

	void ShiftChildren( int nDistance );

	vgui::ScrollBar *m_pScrollBar;
	int m_nLastScrollValue;
	bool m_bUseMouseWheelToScroll;
	CPanelAnimationVarAliasType( int, m_iScrollStep, "scroll_step", "10", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_iBottomBuffer, "bottom_buffer", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_bRestrictWidth, "restrict_width", "1", "proportional_int" );
};

//-----------------------------------------------------------------------------
// An extension of CExScrollingEditablePanel where panels can be added to form
// a list.
//-----------------------------------------------------------------------------
class CScrollableList : public CExScrollingEditablePanel
{
	DECLARE_CLASS_SIMPLE( CScrollableList, CExScrollingEditablePanel );
public:
	CScrollableList( Panel* pParent, const char* pszName )
		: CExScrollingEditablePanel( pParent, pszName )
	{}

	virtual ~CScrollableList();

	virtual void PerformLayout() OVERRIDE;

	void AddPanel( Panel* pPanel, int nGap );
	void ClearAutoLayoutPanels();

private:

	struct LayoutInfo_t
	{
		Panel* m_pPanel;
		int m_nGap;
	};
	
	CUtlVector< LayoutInfo_t > m_vecAutoLayoutPanels;
};

//-----------------------------------------------------------------------------
// A checkbox where keyvalue data can be stored and retrieved (typically in OnCheckButtonChecked)
// so that you don't need a pointer to the checkbox in order to determine WHICH
// checkbox got checked.
//-----------------------------------------------------------------------------
class CExCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE( CExCheckButton, vgui::CheckButton );
public:
	CExCheckButton( Panel* pParent, const char* pszName )
		: BaseClass( pParent, pszName, NULL )
		, m_pKVData( NULL )
	{}

	virtual ~CExCheckButton()
	{
		if ( m_pKVData )
			m_pKVData->deleteThis();
	}

	void SetData( KeyValues* pKVData )
	{
		if ( m_pKVData )
		{
			m_pKVData->deleteThis();
			m_pKVData = NULL;
		}

		m_pKVData = pKVData;
	}

	KeyValues* GetData() const
	{
		return m_pKVData;
	}

private:
	KeyValues *m_pKVData;
};

class CExpandablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CExpandablePanel, vgui::EditablePanel );
public:
	CExpandablePanel( Panel* pParent, const char* pszName );

	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	virtual void OnToggleCollapse( bool bIsExpanded ) {}

	void SetCollapsed( bool bCollapsed );
	void ToggleCollapse();
	bool BIsExpanded() const { return m_bExpanded; }
	void SetExpandedHeight( int nNewHeight );
	float GetPercentAnimated() const;

protected:

	CPanelAnimationVarAliasType( float, m_flResizeTime, "resize_time", "0.4", "float" );
	CPanelAnimationVarAliasType( int, m_nCollapsedHeight, "collapsed_height", "17", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExpandedHeight, "expanded_height", "50", "proportional_int" );

private:

	bool m_bExpanded;
	float m_flAnimEndTime;
};

#endif // TF_CONTROLS_H
