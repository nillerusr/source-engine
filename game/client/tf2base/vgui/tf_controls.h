//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
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
#include "tf_imagepanel.h"
#include <vgui_controls/ImagePanel.h>


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFButton : public vgui::Button
{
public:
	DECLARE_CLASS_SIMPLE( CTFButton, vgui::Button );

	CTFButton( vgui::Panel *parent, const char *name, const char *text );
	CTFButton( vgui::Panel *parent, const char *name, const wchar_t *wszText );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	char		m_szFont[64];
	char		m_szColor[64];
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFLabel : public vgui::Label
{
public:
	DECLARE_CLASS_SIMPLE( CTFLabel, vgui::Label );

	CTFLabel( vgui::Panel *parent, const char *panelName, const char *text );
	CTFLabel( vgui::Panel *parent, const char *panelName, const wchar_t *wszText );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	char		m_szColor[64];
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFRichText : public vgui::RichText
{
public:
	DECLARE_CLASS_SIMPLE( CTFRichText, vgui::RichText );

	CTFRichText( vgui::Panel *parent, const char *panelName );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void SetText( const char *text );
	virtual void SetText( const wchar_t *text );

	virtual void OnTick( void );
	void SetScrollBarImagesVisible( bool visible );

private:
	char		m_szFont[64];
	char		m_szColor[64];

	CTFImagePanel		*m_pUpArrow;
	vgui::ImagePanel	*m_pLine;
	CTFImagePanel		*m_pDownArrow;
	vgui::ImagePanel	*m_pBox;
};

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

#endif // TF_CONTROLS_H
