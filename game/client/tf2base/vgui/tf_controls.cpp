//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"

#include <vgui_controls/ScrollBarSlider.h>
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "tf_controls.h"


using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CTFButton, CTFButton );
DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CTFLabel, CTFLabel );
DECLARE_BUILD_FACTORY( CTFRichText );
DECLARE_BUILD_FACTORY( CTFFooter );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFButton::CTFButton( Panel *parent, const char *name, const char *text ) : Button( parent, name, text )
{
	m_szFont[0] = '\0';
	m_szColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFButton::CTFButton( Panel *parent, const char *name, const wchar_t *wszText ) : Button( parent, name, wszText )
{
	m_szFont[0] = '\0';
	m_szColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFButton::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szFont, inResourceData->GetString( "font", "Default" ), sizeof( m_szFont ) );
	Q_strncpy( m_szColor, inResourceData->GetString( "fgcolor", "Button.TextColor" ), sizeof( m_szColor ) );

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFont( pScheme->GetFont( m_szFont, true ) );
	SetFgColor( pScheme->GetColor( m_szColor, Color( 255, 255, 255, 255 ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFLabel::CTFLabel( Panel *parent, const char *name, const char *text ) : Label( parent, name, text )
{
	m_szColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFLabel::CTFLabel( Panel *parent, const char *name, const wchar_t *wszText ) : Label( parent, name, wszText )
{
	m_szColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFLabel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szColor, inResourceData->GetString( "fgcolor", "Label.TextColor" ), sizeof( m_szColor ) );

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFLabel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( pScheme->GetColor( m_szColor, Color( 255, 255, 255, 255 ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFRichText::CTFRichText( Panel *parent, const char *name ) : RichText( parent, name )
{
	m_szFont[0] = '\0';
	m_szColor[0] = '\0';

	SetCursor(dc_arrow);

	m_pUpArrow = new CTFImagePanel( this, "UpArrow" );
	if ( m_pUpArrow )
	{
		m_pUpArrow->SetShouldScaleImage( true );
		m_pUpArrow->SetImage( "chalkboard_scroll_up" );
		m_pUpArrow->SetFgColor( Color( 255, 255, 255, 255 ) );
		m_pUpArrow->SetAlpha( 255 );
		m_pUpArrow->SetVisible( false );
	}

	m_pLine = new ImagePanel( this, "Line" );
	if ( m_pLine )
	{
		m_pLine->SetShouldScaleImage( true );
		m_pLine->SetImage( "chalkboard_scroll_line" );
		m_pLine->SetVisible( false );
	}

	m_pDownArrow = new CTFImagePanel( this, "DownArrow" );
	if ( m_pDownArrow )
	{
		m_pDownArrow->SetShouldScaleImage( true );
		m_pDownArrow->SetImage( "chalkboard_scroll_down" );
		m_pDownArrow->SetFgColor( Color( 255, 255, 255, 255 ) );
		m_pDownArrow->SetAlpha( 255 );
		m_pDownArrow->SetVisible( false );
	}

	m_pBox = new ImagePanel( this, "Box" );
	if ( m_pBox )
	{
		m_pBox->SetShouldScaleImage( true );
		m_pBox->SetImage( "chalkboard_scroll_box" );
		m_pBox->SetVisible( false );
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szFont, inResourceData->GetString( "font", "Default" ), sizeof( m_szFont ) );
	Q_strncpy( m_szColor, inResourceData->GetString( "fgcolor", "RichText.TextColor" ), sizeof( m_szColor ) );

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFont( pScheme->GetFont( m_szFont, true  ) );
	SetFgColor( pScheme->GetColor( m_szColor, Color( 255, 255, 255, 255 ) ) );

	SetBorder( pScheme->GetBorder( "NoBorder" ) );
	SetBgColor( pScheme->GetColor( "Blank", Color( 0,0,0,0 ) ) );
	SetPanelInteractive( false );
	SetUnusedScrollbarInvisible( true );

	if ( m_pDownArrow  )
	{
		m_pDownArrow->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	if ( m_pUpArrow  )
	{
		m_pUpArrow->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	SetScrollBarImagesVisible( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( _vertScrollBar && _vertScrollBar->IsVisible() )
	{
		int nMin, nMax;
		_vertScrollBar->GetRange( nMin, nMax );
		_vertScrollBar->SetValue( nMin );

		int nScrollbarWide = _vertScrollBar->GetWide();

		int wide, tall;
		GetSize( wide, tall );

		if ( m_pUpArrow )
		{
			m_pUpArrow->SetBounds( wide - nScrollbarWide, 0, nScrollbarWide, nScrollbarWide );
		}

		if ( m_pLine )
		{
			m_pLine->SetBounds( wide - nScrollbarWide, nScrollbarWide, nScrollbarWide, tall - ( 2 * nScrollbarWide ) );
		}

		if ( m_pBox )
		{
			m_pBox->SetBounds( wide - nScrollbarWide, nScrollbarWide, nScrollbarWide, nScrollbarWide );
		}

		if ( m_pDownArrow )
		{
			m_pDownArrow->SetBounds( wide - nScrollbarWide, tall - nScrollbarWide, nScrollbarWide, nScrollbarWide );
		}

		SetScrollBarImagesVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::SetText( const wchar_t *text )
{
	wchar_t buffer[2048];
	Q_wcsncpy( buffer, text, sizeof( buffer ) );

	// transform '\r' to ' ' to eliminate double-spacing on line returns
	for ( wchar_t *ch = buffer; *ch != 0; ch++ )
	{
		if ( *ch == '\r' )
		{
			*ch = ' ';
		}
	}

	BaseClass::SetText( buffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::SetText( const char *text )
{
	char buffer[2048];
	Q_strncpy( buffer, text, sizeof( buffer ) );

	// transform '\r' to ' ' to eliminate double-spacing on line returns
	for ( char *ch = buffer; *ch != 0; ch++ )
	{
		if ( *ch == '\r' )
		{
			*ch = ' ';
		}
	}

	BaseClass::SetText( buffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::SetScrollBarImagesVisible( bool visible )
{
	if ( m_pDownArrow && m_pDownArrow->IsVisible() != visible )
	{
		m_pDownArrow->SetVisible( visible );
	}

	if ( m_pUpArrow && m_pUpArrow->IsVisible() != visible )
	{
		m_pUpArrow->SetVisible( visible );
	}

	if ( m_pLine && m_pLine->IsVisible() != visible )
	{
		m_pLine->SetVisible( visible );
	}

	if ( m_pBox && m_pBox->IsVisible() != visible )
	{
		m_pBox->SetVisible( visible );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRichText::OnTick()
{
	if ( !IsVisible() )
		return;

	if ( m_pDownArrow && m_pUpArrow && m_pLine && m_pBox )
	{
		if ( _vertScrollBar && _vertScrollBar->IsVisible() )
		{
			_vertScrollBar->SetZPos( 500 );

			// turn off painting the vertical scrollbar
			_vertScrollBar->SetPaintBackgroundEnabled( false );
			_vertScrollBar->SetPaintBorderEnabled( false );
			_vertScrollBar->SetPaintEnabled( false );
			_vertScrollBar->SetScrollbarButtonsVisible( false );

			// turn on our own images
			SetScrollBarImagesVisible ( true );

			// set the alpha on the up arrow
			int nMin, nMax;
			_vertScrollBar->GetRange( nMin, nMax );
			int nScrollPos = _vertScrollBar->GetValue();
			int nRangeWindow = _vertScrollBar->GetRangeWindow();
			int nBottom = nMax - nRangeWindow;
			if ( nBottom < 0 )
			{
				nBottom = 0;
			}

			// set the alpha on the up arrow
			int nAlpha = ( nScrollPos - nMin <= 0 ) ? 90 : 255;
			m_pUpArrow->SetAlpha( nAlpha );

			// set the alpha on the down arrow
			nAlpha = ( nScrollPos >= nBottom ) ? 90 : 255;
			m_pDownArrow->SetAlpha( nAlpha );

			ScrollBarSlider *pSlider = _vertScrollBar->GetSlider();
			if ( pSlider && pSlider->GetRangeWindow() > 0 )
			{
				int x, y, w, t, min, max;
				m_pLine->GetBounds( x, y, w, t );
				pSlider->GetNobPos( min, max );

				m_pBox->SetBounds( x, y + min, w, ( max - min ) );
			}
		}
		else
		{
			// turn off our images
			SetScrollBarImagesVisible ( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Xbox-specific panel that displays button icons text labels
//-----------------------------------------------------------------------------
CTFFooter::CTFFooter( Panel *parent, const char *panelName ) : BaseClass( parent, panelName ) 
{
	SetVisible( true );
	SetAlpha( 0 );

	m_nButtonGap = 32;
	m_ButtonPinRight = 100;
	m_FooterTall = 80;

	m_ButtonOffsetFromTop = 0;
	m_ButtonSeparator = 4;
	m_TextAdjust = 0;

	m_bPaintBackground = false;
	m_bCenterHorizontal = true;

	m_szButtonFont[0] = '\0';
	m_szTextFont[0] = '\0';
	m_szFGColor[0] = '\0';
	m_szBGColor[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFooter::~CTFFooter()
{
	ClearButtons();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hButtonFont = pScheme->GetFont( ( m_szButtonFont[0] != '\0' ) ? m_szButtonFont : "GameUIButtons" );
	m_hTextFont = pScheme->GetFont( ( m_szTextFont[0] != '\0' ) ? m_szTextFont : "MenuLarge" );

	SetFgColor( pScheme->GetColor( m_szFGColor, Color( 255, 255, 255, 255 ) ) );
	SetBgColor( pScheme->GetColor( m_szBGColor, Color( 0, 0, 0, 255 ) ) );

	int x, y, w, h;
	GetParent()->GetBounds( x, y, w, h );
	SetBounds( x, h - m_FooterTall, w, m_FooterTall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// gap between hints
	m_nButtonGap = inResourceData->GetInt( "buttongap", 32 );
	m_ButtonPinRight = inResourceData->GetInt( "button_pin_right", 100 );
	m_FooterTall = inResourceData->GetInt( "tall", 80 );
	m_ButtonOffsetFromTop = inResourceData->GetInt( "buttonoffsety", 0 );
	m_ButtonSeparator = inResourceData->GetInt( "button_separator", 4 );
	m_TextAdjust = inResourceData->GetInt( "textadjust", 0 );

	m_bCenterHorizontal = ( inResourceData->GetInt( "center", 1 ) == 1 );
	m_bPaintBackground = ( inResourceData->GetInt( "paintbackground", 0 ) == 1 );

	// fonts for text and button
	Q_strncpy( m_szTextFont, inResourceData->GetString( "fonttext", "MenuLarge" ), sizeof( m_szTextFont ) );
	Q_strncpy( m_szButtonFont, inResourceData->GetString( "fontbutton", "GameUIButtons" ), sizeof( m_szButtonFont ) );

	// fg and bg colors
	Q_strncpy( m_szFGColor, inResourceData->GetString( "fgcolor", "White" ), sizeof( m_szFGColor ) );
	Q_strncpy( m_szBGColor, inResourceData->GetString( "bgcolor", "Black" ), sizeof( m_szBGColor ) );

	// clear the buttons because we're going to re-add them here
	ClearButtons();

	for ( KeyValues *pButton = inResourceData->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
	{
		const char *pName = pButton->GetName();

		if ( !Q_stricmp( pName, "button" ) )
		{
			// Add a button to the footer
			const char *pName = pButton->GetString( "name", "NULL" );
			const char *pText = pButton->GetString( "text", "NULL" );
			const char *pIcon = pButton->GetString( "icon", "NULL" );
			AddNewButtonLabel( pName, pText, pIcon );
		}
	}

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::AddNewButtonLabel( const char *name, const char *text, const char *icon )
{
	FooterButton_t *button = new FooterButton_t;

	button->bVisible = true;
	Q_strncpy( button->name, name, sizeof( button->name ) );

	// Button icons are a single character
	wchar_t *pIcon = g_pVGuiLocalize->Find( icon );
	if ( pIcon )
	{
		button->icon[0] = pIcon[0];
		button->icon[1] = '\0';
	}
	else
	{
		button->icon[0] = '\0';
	}

	// Set the help text
	wchar_t *pText = g_pVGuiLocalize->Find( text );
	if ( pText )
	{
		wcsncpy( button->text, pText, wcslen( pText ) + 1 );
	}
	else
	{
		button->text[0] = '\0';
	}

	m_Buttons.AddToTail( button );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ShowButtonLabel( const char *name, bool show )
{
	for ( int i = 0; i < m_Buttons.Count(); ++i )
	{
		if ( !Q_stricmp( m_Buttons[ i ]->name, name ) )
		{
			m_Buttons[ i ]->bVisible = show;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::PaintBackground( void )
{
	if ( !m_bPaintBackground )
		return;

	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::Paint( void )
{
	// inset from right edge
	int wide = GetWide();

	// center the text within the button
	int buttonHeight = vgui::surface()->GetFontTall( m_hButtonFont );
	int fontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	int textY = ( buttonHeight - fontHeight )/2 + m_TextAdjust;

	if ( textY < 0 )
	{
		textY = 0;
	}

	int y = m_ButtonOffsetFromTop;

	if ( !m_bCenterHorizontal )
	{
		// draw the buttons, right to left
		int x = wide - m_ButtonPinRight;

		vgui::Label label( this, "temp", L"" );
		for ( int i = m_Buttons.Count() - 1 ; i >= 0 ; --i )
		{
			FooterButton_t *pButton = m_Buttons[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			int iTextWidth = label.GetWide();

			if ( iTextWidth == 0 )
				x += m_nButtonGap;	// There's no text, so remove the gap between buttons
			else
				x -= iTextWidth;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			// Draw the button
			// back up button width and a little extra to leave a gap between button and text
			x -= ( vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator );
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );

			// back up to next string
			x -= m_nButtonGap;
		}
	}
	else
	{
		// center the buttons (as a group)
		int x = wide / 2;
		int totalWidth = 0;
		int i = 0;
		int nButtonCount = 0;

		vgui::Label label( this, "temp", L"" );

		// need to loop through and figure out how wide our buttons and text are (with gaps between) so we can offset from the center
		for ( i = 0; i < m_Buttons.Count(); ++i )
		{
			FooterButton_t *pButton = m_Buttons[i];

			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			totalWidth += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] );
			totalWidth += m_ButtonSeparator;
			totalWidth += label.GetWide();

			nButtonCount++; // keep track of how many active buttons we'll be drawing
		}

		totalWidth += ( nButtonCount - 1 ) * m_nButtonGap; // add in the gaps between the buttons
		x -= ( totalWidth / 2 );

		for ( i = 0; i < m_Buttons.Count(); ++i )
		{
			FooterButton_t *pButton = m_Buttons[i];

			if ( !pButton->bVisible )
				continue;

			// Get the string length
			label.SetFont( m_hTextFont );
			label.SetText( pButton->text );
			label.SizeToContents();

			int iTextWidth = label.GetWide();

			// Draw the icon
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );
			x += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			x += iTextWidth + m_nButtonGap;
		}
	}
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFooter::ClearButtons( void )
{
	m_Buttons.PurgeAndDeleteElements();
}