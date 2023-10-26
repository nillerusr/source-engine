//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VHybridButton.h"
#include "basemodpanel.h"
#include "VFooterPanel.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Tooltip.h"
#include "vgui/IVgui.h"
#include "tier1/KeyValues.h"
#include "vgui/ilocalize.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "gamemodes.h"

#ifndef _X360
#include <ctype.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( BaseModHybridButton, HybridButton );

void Demo_DisableButton( Button *pButton )
{
	BaseModHybridButton *pHybridButton = dynamic_cast<BaseModHybridButton *>(pButton);

	if (pHybridButton)
	{
		pHybridButton->SetEnabled( false );

		char szTooltip[512];
		wchar_t *wUnicode = g_pVGuiLocalize->Find( "#L4D360UI_MainMenu_DemoVersion" );
		if ( !wUnicode )
			wUnicode = L"";

		g_pVGuiLocalize->ConvertUnicodeToANSI( wUnicode, szTooltip, sizeof( szTooltip ) );

		pHybridButton->SetHelpText( szTooltip , false );
	}
}

void Dlc1_DisableButton( Button *pButton )
{
	BaseModHybridButton *pHybridButton = dynamic_cast<BaseModHybridButton *>(pButton);

	if(pHybridButton)
	{
		pHybridButton->SetEnabled( false );

		char szTooltip[512];
		wchar_t *wUnicode = g_pVGuiLocalize->Find( "#L4D360UI_DLC1_NotInstalled" );

		if ( !wUnicode )
			wUnicode = L"";

		g_pVGuiLocalize->ConvertUnicodeToANSI( wUnicode, szTooltip, sizeof( szTooltip ) );

		pHybridButton->SetHelpText( szTooltip , false );
	}
}

struct HybridEnableStates
{
	BaseModUI::BaseModHybridButton::EnableCondition mCondition;
	char mConditionName[64];
};

HybridEnableStates sHybridStates[] = 
{
	{ BaseModUI::BaseModHybridButton::EC_LIVE_REQUIRED,	"LiveRequired" },
	{ BaseModUI::BaseModHybridButton::EC_NOTFORDEMO,		"Never" }
};

//=============================================================================
// Constructor / Destructor
//=============================================================================

BaseModUI::BaseModHybridButton::BaseModHybridButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget, const char *pCmd )
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetContentAlignment( a_northwest );
	SetClosed();
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	SetConsoleStylePanel( true );

	m_isNavigateTo = false;
	m_bOnlyActiveUser = false;
	m_bIgnoreButtonA = false;

	mEnableCondition = EC_ALWAYS;

	m_nStyle = BUTTON_SIMPLE;
	m_hTextFont = 0;
	m_hTextBlurFont = 0;
	m_hHintTextFont = 0;
	m_hSelectionFont = 0;
	m_hSelectionBlurFont = 0;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;

	m_nWideAtOpen = 0;
}

BaseModUI::BaseModHybridButton::BaseModHybridButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget, const char *pCmd )
	: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetContentAlignment( a_northwest );
	SetClosed();
	SetButtonActivationType( ACTIVATE_ONRELEASED );

	m_isNavigateTo = false;
	m_iUsablePlayerIndex = -1;

	mEnableCondition = EC_ALWAYS;

	m_nStyle = BUTTON_SIMPLE;
	m_hTextFont = 0;
	m_hTextBlurFont = 0;
	m_hHintTextFont = 0;
	m_hSelectionFont = 0;
	m_hSelectionBlurFont = 0;

	m_originalTall = 0;
	m_textInsetX = 0;
	m_textInsetY = 0;

	m_iSelectedArrow = -1;
	m_iUnselectedArrow = -1;
}

BaseModUI::BaseModHybridButton::~BaseModHybridButton()
{
	// purposely not destroying our textures
	// otherwise i/o constantly on these
}

BaseModHybridButton::State BaseModHybridButton::GetCurrentState()
{
	State curState = Enabled;
	if ( IsPC() )
	{
		if ( HasFocus() )
		{
			curState = IsEnabled() ? Focus : FocusDisabled;
		}
	}
	if( m_isOpen )
	{
		curState = Open;
	}
	else if( IsArmed() || m_isNavigateTo ) //NavigateTo doesn't instantly give focus to the control
	{										//so this little boolean value is meant to let us know we should have focus for the "focus state"
		if( IsEnabled() )					//until vgui catches up and registers us as having focus.
		{
			curState = Focus;
		}
		else
		{
			curState = FocusDisabled;
		}

		if( IsArmed() )
		{
			m_isNavigateTo = false;
		}
	}
	else if( !IsEnabled() )
	{
		curState = Disabled;
	}

	return curState;
}

void BaseModHybridButton::SetOpen()
{
	if ( m_isOpen )
		return;
	m_isOpen = true;
	if ( IsPC() )
	{
		PostMessageToAllSiblingsOfType< BaseModHybridButton >( new KeyValues( "OnSiblingHybridButtonOpened" ) );
	}
}

void BaseModHybridButton::SetClosed()
{
	if ( !m_isOpen )
		return;

	m_isOpen = false;
}

void BaseModHybridButton::OnSiblingHybridButtonOpened()
{
	if ( !IsPC() )
		return;

	bool bClosed = false;

	FlyoutMenu *pActiveFlyout = FlyoutMenu::GetActiveMenu();

	if ( pActiveFlyout )
	{
		BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( pActiveFlyout->GetNavFrom() );
		if ( button && button == this )
		{
			// We need to close the flyout attached to this button
			FlyoutMenu::CloseActiveMenu();
			bClosed = true;
		}
	}

	if ( !bClosed )
	{
		SetClosed();
	}

	m_isNavigateTo = false;
}

char const *BaseModHybridButton::GetHelpText( bool bEnabled ) const
{
	return bEnabled ? m_enabledToolText.String() : m_disabledToolText.String();
}

void BaseModHybridButton::UpdateFooterHelpText()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		//Directly set the new tooltips
		footer->SetHelpText( IsEnabled() ? m_enabledToolText.String() : m_disabledToolText.String() );
	}
}

void BaseModHybridButton::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( IsPC() )
	{
		if( code == MOUSE_RIGHT )
		{
			FlyoutMenu::CloseActiveMenu( this );
		}
		else
		{
			if( (code == MOUSE_LEFT) && (IsEnabled() == false) && (dynamic_cast<FlyoutMenu *>( GetParent() ) == NULL) )
			{
				//when trying to use an inactive item that isn't part of a flyout. Close any open flyouts.
				FlyoutMenu::CloseActiveMenu( this );
			}
			RequestFocus( 0 );			
		}
	}
}

void BaseModHybridButton::NavigateTo( )
{
	BaseClass::NavigateTo( );

	UpdateFooterHelpText();

	FlyoutMenu* parentMenu = dynamic_cast< FlyoutMenu* >( GetParent() );
	if( parentMenu )
	{
		parentMenu->NotifyChildFocus( this );
	}

	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("OnHybridButtonNavigatedTo");
		msg->SetInt("button", ToHandle() );

		ivgui()->PostMessage(GetVParent(), msg, GetVPanel());
	}

	m_isNavigateTo = true;
	if ( IsPC() )
	{
		RequestFocus( 0 );
	}
}

void BaseModHybridButton::NavigateFrom( )
{
	BaseClass::NavigateFrom( );

	if ( IsPC() && CBaseModPanel::GetSingleton().GetFooterPanel() )
	{
		// Show no help text if they left the button
		CBaseModPanel::GetSingleton().GetFooterPanel()->FadeHelpText();
	}

	m_isNavigateTo = false;
}

void BaseModHybridButton::SetHelpText( const char* tooltip, bool enabled )
{
	if ( enabled )
	{
		m_enabledToolText = tooltip;
	}
	else
	{
		m_disabledToolText = tooltip;
	}

	// if we have the focus update the footer
	if ( HasFocus() )
	{
		UpdateFooterHelpText();
	}
}

// 0 = Deprecated CA buttons
// 1 = Normal Button
// 2 = MainMenu Button or InGameMenu Butto
// 3 = Flyout button
void BaseModHybridButton::PaintButtonEx()
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	Color blotchColor = pScheme->GetColor( "HybridButton.BlotchColor", Color( 0, 0, 0, 255 ) );
	Color borderColor = pScheme->GetColor( "HybridButton.BorderColor", Color( 0, 0, 0, 255 ) );

	int x, y;
	int wide, tall;
	GetSize( wide, tall );

	x = 0;

	// due to vertical resizing, center within the control
	y = ( tall - m_originalTall ) / 2;
	tall = m_originalTall;

	if ( m_nStyle == BUTTON_GAMEMODE )
	{
		y = 0;
	}

	if ( ( m_nStyle == BUTTON_DROPDOWN || m_nStyle == BUTTON_GAMEMODE ) && GetCurrentState() == Open && m_nWideAtOpen )
	{
		wide = m_nWideAtOpen;
	}

	bool bAnimateGlow = false;
	bool bDrawGlow = false;
	bool bDrawCursor = false;
	bool bDrawText = true;
	Color col;
	State curState = GetCurrentState();
	switch ( curState )
	{
		case Enabled:
			// selectable, just not highlighted
			if ( m_nStyle == BUTTON_RED || m_nStyle == BUTTON_REDMAIN )
			{
				//col.SetColor( 0, 128, 128, 255 );
				col.SetColor( 169, 213, 255, 255 );
			}
			else if ( m_nStyle == BUTTON_ALIENSWARMMENUBUTTON || m_nStyle == BUTTON_ALIENSWARMMENUBUTTONSMALL )
			{
				col.SetColor( 135, 170, 193, 255 );
			}
			else
			{
				//col.SetColor( 125, 125, 125, 255 );
				col.SetColor( 83, 148, 192, 255 );
			}
			break;
		case Disabled:
			//col.SetColor( 88, 97, 104, 255 );
			//col.SetColor( 65, 74, 96, 255 );
			col.SetColor( 32, 59, 82, 255 );
			bDrawText = true;
			bDrawGlow = false;
			break;
		case FocusDisabled:
			col.SetColor( 182, 189, 194, 255 );
			bDrawText = false;
			bDrawGlow = true;
			break;
		case Open:
			// flyout menu is attached
			//col.SetColor( 200, 200, 200, 255 );
			col.SetColor( 169, 213, 255, 255 );
			bDrawGlow = true;
			bDrawCursor = true;
			break;
		case Focus:
			// active item
			col.SetColor( 255, 255, 255, 255 );
			bDrawGlow = true;
			bAnimateGlow = true;
			if ( m_nStyle == BUTTON_SIMPLE ||
				 m_nStyle == BUTTON_DROPDOWN ||
				 m_nStyle == BUTTON_DIALOG ||
				 m_nStyle == BUTTON_RED )
			{
				bDrawCursor = true;
			}
			break;
	}

	wchar_t szUnicode[512];
	GetText( szUnicode, sizeof( szUnicode ) );
	int len = V_wcslen( szUnicode );

	int textWide, textTall;
	surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

	textWide = clamp( textWide, 0, wide - m_textInsetX * 2 );
	textTall = clamp( textTall, 0, tall - m_textInsetX * 2 );

	int textInsetX = m_textInsetX;
	if ( m_nStyle == BUTTON_DIALOG )
	{
		// dialog buttons are centered
		textInsetX = ( wide - textWide ) / 2;
	}

	if ( FlyoutMenu::GetActiveMenu() && FlyoutMenu::GetActiveMenu()->GetNavFrom() != this )
	{
		bDrawCursor = false;
	}

	if ( bDrawCursor )
	{
		// draw backing rectangle
		if ( curState == Open )
		{
			surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );
			surface()->DrawFilledRectFade( x, y, x+wide, y+tall, 0, 255, true );
		}

		// draw blotch
		surface()->DrawSetColor( blotchColor );
		if ( m_nStyle == BUTTON_DIALOG )
		{
			int blotchWide = textWide;
			int blotchX = x + textInsetX;
			surface()->DrawFilledRectFade( blotchX, y, blotchX + 0.50f * blotchWide, y+tall, 0, 150, true );
			surface()->DrawFilledRectFade( blotchX + 0.50f * blotchWide, y, blotchX + blotchWide, y+tall, 150, 0, true );
		}
		else
		{
			int blotchWide = textWide + vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 40 );
			int blotchX = x + textInsetX;
			surface()->DrawFilledRectFade( blotchX, y, blotchX + 0.25f * blotchWide, y+tall, 0, 150, true );
			surface()->DrawFilledRectFade( blotchX + 0.25f * blotchWide, y, blotchX + blotchWide, y+tall, 150, 0, true );
		}

		// draw border lines
		surface()->DrawSetColor( borderColor );
		if ( curState == Open )
		{
			FlyoutMenu *pActiveFlyout = FlyoutMenu::GetActiveMenu();
			BaseModHybridButton *button = dynamic_cast< BaseModHybridButton* >( pActiveFlyout ? pActiveFlyout->GetNavFrom() : NULL );
			if ( pActiveFlyout && pActiveFlyout->GetOriginalTall() == 0 && button && button == this )
			{
				surface()->DrawFilledRectFade( x, y, x + wide, y+2, 255, 0, true );
			}
			else
			{
				// the border lines end at the beginning of the flyout
				// the flyout will draw to complete the look
				surface()->DrawFilledRectFade( x, y, x + wide, y+2, 0, 255, true );
				surface()->DrawFilledRectFade( x, y+tall-2, x + wide, y+tall, 0, 255, true );
			}
		}
		else
		{
			// top and bottom border lines
			surface()->DrawFilledRectFade( x, y, x + 0.5f * wide, y+2, 0, 255, true );
			surface()->DrawFilledRectFade( x + 0.5f * wide, y, x + wide, y+2, 255, 0, true );
			surface()->DrawFilledRectFade( x, y+tall-2, x + 0.5f * wide, y+tall, 0, 255, true );
			surface()->DrawFilledRectFade( x + 0.5f * wide, y+tall-2, x + wide, y+tall, 255, 0, true );
		}
	}

	// assume drawn, unless otherwise shortened with ellipsis
	int iLabelCharsDrawn = len;

	// draw the text
	if ( bDrawText )
	{
		int availableWidth = GetWide() - x - textInsetX;

		if ( m_bShowDropDownIndicator )
		{
			int textTall = vgui::surface()->GetFontTall( m_hTextFont );
			int textLen = 0;

			int len = wcslen( szUnicode );
			for ( int i=0;i<len;i++ )
			{
				textLen += vgui::surface()->GetCharacterWidth( m_hTextFont, szUnicode[i] );
			}

			int imageX = x + textInsetX + textLen + m_iSelectedArrowSize / 2;
			int imageY = y + ( textTall - m_iSelectedArrowSize ) / 2;

			if ( ( imageX + m_iSelectedArrowSize ) > GetWide() )
			{
				imageX = GetWide() - m_iSelectedArrowSize;
			}

			if ( curState == Open || m_bOverrideDropDownIndicator )
			{
				vgui::surface()->DrawSetTexture( m_iSelectedArrow );
			}
			else
			{
				vgui::surface()->DrawSetTexture( m_iUnselectedArrow );
			}

			vgui::surface()->DrawSetColor( col );
			vgui::surface()->DrawTexturedRect( imageX, imageY, imageX + m_iSelectedArrowSize, imageY + m_iSelectedArrowSize );
			vgui::surface()->DrawSetTexture( 0 );

			availableWidth -= m_iSelectedArrowSize * 2;
		}

		vgui::surface()->DrawSetTextFont( m_hTextFont );
		vgui::surface()->DrawSetTextPos( x + textInsetX, y + m_textInsetY  );
		vgui::surface()->DrawSetTextColor( col );

		if ( textWide > availableWidth )
		{
			// length of 3 dots
			int ellipsesLen = 3 * vgui::surface()->GetCharacterWidth( m_hTextFont, L'.' );

			availableWidth -= ellipsesLen;

			iLabelCharsDrawn = 0;
			int charX = x + textInsetX;
			for ( int i=0; i < len; i++ )
			{
				vgui::surface()->DrawUnicodeChar( szUnicode[i] );
				iLabelCharsDrawn++;

				charX += vgui::surface()->GetCharacterWidth( m_hTextFont, szUnicode[i] );
				if ( charX >= ( x + textInsetX + availableWidth ) )
					break;
			}
			
			vgui::surface()->DrawPrintText( L"...", 3 );
		}
		else
		{
			vgui::surface()->DrawUnicodeString( szUnicode );
		}
	}
	else if ( GetCurrentState() == Disabled || GetCurrentState() == FocusDisabled )
	{
		Color textcol = col;
		textcol[ 3 ] = 64;

		vgui::surface()->DrawSetTextFont( m_hTextFont );
		vgui::surface()->DrawSetTextPos( x + textInsetX, y + m_textInsetY  );
		vgui::surface()->DrawSetTextColor( textcol );
		vgui::surface()->DrawPrintText( szUnicode, len );
	}

	// draw the help text
	if ( m_nStyle == BUTTON_GAMEMODE && curState != Open )
	{
		const char *pHelpText = GetHelpText( IsEnabled() );
		wchar_t szHelpUnicode[512];
		wchar_t *pUnicodeString;
		if ( pHelpText && pHelpText[0] == '#' )
		{
			pUnicodeString = g_pVGuiLocalize->Find( pHelpText );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pHelpText, szHelpUnicode, sizeof( szHelpUnicode ) );
			pUnicodeString = szHelpUnicode;
		}	
		vgui::surface()->DrawSetTextFont( m_hHintTextFont );
		vgui::surface()->DrawSetTextPos( x + textInsetX, y + m_textInsetY + m_nTextFontHeight );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( pUnicodeString, wcslen( pUnicodeString ) );
	}

	// draw the pulsing glow
	if ( bDrawGlow )
	{
		if ( !bDrawText )
		{
			vgui::surface()->DrawSetTextColor( col );
		}
		else
		{
			int alpha = bAnimateGlow ? 60.0f + 30.0f * sin( Plat_FloatTime() * 4.0f ) : 30;
			Color glowColor( 255, 255, 255, alpha );
			vgui::surface()->DrawSetTextColor( glowColor );
		}
		vgui::surface()->DrawSetTextFont( m_hTextBlurFont );
		vgui::surface()->DrawSetTextPos( x + textInsetX, y + m_textInsetY );

		for ( int i=0; i<len && i < iLabelCharsDrawn; i++ )
		{
			vgui::surface()->DrawUnicodeChar( szUnicode[i] );
		}
	}

	if ( m_nStyle == BUTTON_DROPDOWN && curState != Open )
	{
		// draw right justified item
		wchar_t *pUnicodeString = g_pVGuiLocalize->Find( m_DropDownSelection.String() );
		if ( !pUnicodeString )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DropDownSelection.String(), szUnicode, sizeof( szUnicode ) );
			pUnicodeString = szUnicode;
		}
		int len = V_wcslen( pUnicodeString );

		int textWide, textTall;
		surface()->GetTextSize( m_hSelectionFont, pUnicodeString, textWide, textTall );

		// horizontal right justify
		int xx = wide - textWide - textInsetX;
		// vertical center within
		int yy = ( tall - textTall )/2;

		// draw the drop down selection text
		vgui::surface()->DrawSetTextFont( m_hSelectionFont );
		vgui::surface()->DrawSetTextPos( xx, yy );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( pUnicodeString, len );

		if ( bDrawGlow )
		{
			int alpha = bAnimateGlow ? 60.0f + 30.0f * sin( Plat_FloatTime() * 4.0f ) : 30;
			vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, alpha ) );
			vgui::surface()->DrawSetTextFont( m_hSelectionBlurFont );
			vgui::surface()->DrawSetTextPos( xx, yy );
			vgui::surface()->DrawPrintText( pUnicodeString, len );
		}
	}
}

void BaseModHybridButton::Paint()
{
	// bypass ALL of CA's inept broken drawing code
	// not using VGUI controls, to much draw state is misconfigured by CA to salvage
	PaintButtonEx();

	// only do the forced selection for a single frame.
	m_isNavigateTo = false;
}

void BaseModHybridButton::OnThink()
{
	switch( mEnableCondition )
	{
	case EC_LIVE_REQUIRED:
		{
#ifdef _X360 
			SetEnabled( CUIGameData::Get()->SignedInToLive() );
#else
			SetEnabled( true );
#endif
		}	
		break;
	case EC_NOTFORDEMO:
		{
			if ( IsEnabled() )
			{
				Demo_DisableButton( this );
			}
		}
		break;

	}

	BaseClass::OnThink();
}

void BaseModHybridButton::ApplySettings( KeyValues * inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	vgui::IScheme *scheme = vgui::scheme()->GetIScheme( GetScheme() );
	if( !scheme )
		return;

	char keyString[128];

	// if a style is specified attempt to load values in from the SCHEME file
	const char *style = inResourceData->GetString( "Style", NULL );
	if ( !style )
	{
		style = "DefaultButton";
	}

	// this is a total bypass of the CA look
	m_nStyle = BUTTON_SIMPLE;
	V_snprintf( keyString, sizeof( keyString ), "%s.Style", style );
	const char *pFormatString = scheme->GetResourceString( keyString );
	if ( pFormatString && pFormatString[0] )
	{
		m_nStyle = (ButtonStyle_t)atoi( pFormatString );
	}
	if ( m_nStyle == BUTTON_MAINMENU )
	{
		m_hTextFont = scheme->GetFont( "MainBold", true );
		m_hTextBlurFont = scheme->GetFont( "MainBoldBlur", true );
	}
	else if ( m_nStyle == BUTTON_FLYOUTITEM )
	{
		m_hTextFont = scheme->GetFont( "DefaultMedium", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultMediumBlur", true );
	}
	else if ( m_nStyle == BUTTON_DROPDOWN )
	{
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
		m_hSelectionFont = scheme->GetFont( "DefaultMedium", true );
		m_hSelectionBlurFont = scheme->GetFont( "DefaultMediumBlur", true );
		m_nWideAtOpen = vgui::scheme()->GetProportionalScaledValue( inResourceData->GetInt( "wideatopen", 0 ) );
	}
	else if ( m_nStyle == BUTTON_DIALOG )
	{
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
	}
	else if ( m_nStyle == BUTTON_RED || m_nStyle == BUTTON_REDMAIN )
	{
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
	}
	else if ( m_nStyle == BUTTON_SMALL )
	{
		m_hTextFont = scheme->GetFont( "DefaultVerySmall", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultVerySmall", true );
	}
	else if ( m_nStyle == BUTTON_MEDIUM )
	{
		m_hTextFont = scheme->GetFont( "DefaultMedium", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultMediumBlur", true );
	}
	else if ( m_nStyle == BUTTON_GAMEMODE )
	{
		m_hTextFont = scheme->GetFont( "MainBold", true );
		m_hTextBlurFont = scheme->GetFont( "MainBoldBlur", true );
		m_hHintTextFont = scheme->GetFont( "Default", true );
		m_nWideAtOpen = vgui::scheme()->GetProportionalScaledValue( inResourceData->GetInt( "wideatopen", 0 ) );
	}
	else if ( m_nStyle == BUTTON_MAINMENUSMALL )
	{
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
	}
	else if ( m_nStyle == BUTTON_ALIENSWARMMENUBUTTON )
	{
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
	}
	else if ( m_nStyle == BUTTON_ALIENSWARMMENUBUTTONSMALL )
	{
		m_hTextFont = scheme->GetFont( "DefaultMedium", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultMediumBlur", true );
	}
	else if ( m_nStyle == BUTTON_ALIENSWARMDEFAULT )
	{
		m_hTextFont = scheme->GetFont( "Default", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBlur", true );
	}
	else
	{
		m_nStyle = BUTTON_SIMPLE;
		m_hTextFont = scheme->GetFont( "DefaultBold", true );
		m_hTextBlurFont = scheme->GetFont( "DefaultBoldBlur", true );
	}
	
	m_nTextFontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	m_nHintTextFontHeight = vgui::surface()->GetFontTall( m_hHintTextFont );

	// tooltips
	const char* enabledToolTipText = inResourceData->GetString( "tooltiptext" , NULL );
	if ( enabledToolTipText )
	{
		SetHelpText( enabledToolTipText, true );
	}

	const char* disabledToolTipText = inResourceData->GetString( "disabled_tooltiptext", NULL );
	if ( disabledToolTipText )
	{
		SetHelpText( disabledToolTipText, false );
	}

	//vgui's standard handling of the tabPosition tag doesn't properly navigate for the X360
	if ( inResourceData->GetInt( "tabPosition", 0 ) == 1 )
	{
		NavigateTo();
	}

	SetFont( m_hTextFont );

	//Get the x text inset
	V_snprintf( keyString, sizeof( keyString ), "%s.%s", style, "TextInsetX" );
	const char *result = scheme->GetResourceString( keyString );
	if ( *result != 0 )
	{
		m_textInsetX = atoi( result );
		m_textInsetX = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), m_textInsetX );
	}

	//Get the y text inset
	V_snprintf( keyString, sizeof( keyString ), "%s.%s", style, "TextInsetY" );
	result = scheme->GetResourceString( keyString );
	if( *result != 0 )
	{
		m_textInsetY = atoi( result );
		m_textInsetY = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), m_textInsetY );
	}

	//0 = press and release
	//1 = press
	//2 = release
	int activationType = inResourceData->GetInt( "ActivationType", IsPC() ? 1 : 2 );
	clamp( activationType, 0, 2 );
	SetButtonActivationType( static_cast< vgui::Button::ActivationType_t >( activationType ) );

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	m_originalTall = tall;

	if ( m_nStyle == BUTTON_GAMEMODE )
	{
		// needs to be exact height sized
		tall = m_nTextFontHeight + m_nHintTextFontHeight;
		SetSize( wide, tall );
		m_originalTall = m_nTextFontHeight;
	}

	m_iUsablePlayerIndex = USE_EVERYBODY;
	if ( const char *pszValue = inResourceData->GetString( "usablePlayerIndex", "" ) )
	{
		if ( !stricmp( "primary", pszValue ) )
		{
			m_iUsablePlayerIndex = USE_PRIMARY;
		}
		else if ( !stricmp( "nobody", pszValue ) )
		{
			m_iUsablePlayerIndex = USE_NOBODY;
		}
		else if ( isdigit( pszValue[0] ) )
		{
			m_iUsablePlayerIndex = atoi( pszValue );
		}
	}

	//handle different conditions to allow the control to be enabled and disabled automatically
	const char * condition = inResourceData->GetString( "EnableCondition" );
	for ( int index = 0; index < ( sizeof( sHybridStates ) / sizeof( HybridEnableStates ) ); ++index )
	{
		if ( Q_stricmp( condition, sHybridStates[ index ].mConditionName ) == 0 )
		{
			mEnableCondition = sHybridStates[ index ].mCondition;
			break;
		}
	}

	if ( mEnableCondition == EC_NOTFORDEMO )
	{
		if ( IsEnabled() )
		{
			Demo_DisableButton( this );
		}
	}

	m_bOnlyActiveUser = ( inResourceData->GetInt( "OnlyActiveUser", 0 ) != 0 );
	m_bIgnoreButtonA = ( inResourceData->GetInt( "IgnoreButtonA", 0 ) != 0 );

	m_bShowDropDownIndicator = ( inResourceData->GetInt( "ShowDropDownIndicator", 0 ) != 0 );
	m_bOverrideDropDownIndicator = false;

	m_iSelectedArrowSize = vgui::scheme()->GetProportionalScaledValue( inResourceData->GetInt( "DropDownIndicatorSize", 8 ) );
}


void BaseModHybridButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetReleasedSound( CBaseModPanel::GetSingleton().GetUISoundName( UISOUND_ACCEPT ) );

	const char *pImageName;

	// use find or create pattern, avoid pointless redundant i/o
	pImageName = "vgui/icon_arrow_down";
	m_iSelectedArrow = vgui::surface()->DrawGetTextureId( pImageName );
	if ( m_iSelectedArrow == -1 )
	{
		m_iSelectedArrow = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iSelectedArrow, pImageName, true, false );	
	}

	// use find or create pattern, avoid pointles redundant i/o
	pImageName = "vgui/icon_arrow";
	m_iUnselectedArrow = vgui::surface()->DrawGetTextureId( pImageName );
	if ( m_iUnselectedArrow == -1 )
	{
		m_iUnselectedArrow = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iUnselectedArrow, pImageName, true, false );	
	}
}

void BaseModHybridButton::OnKeyCodePressed( vgui::KeyCode code )
{
	int iJoystick = GetJoystickForCode( code );

	if ( m_bOnlyActiveUser )
	{
		// Only allow input from the active userid
		int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();

		if( iJoystick != userId || iJoystick < 0 )
		{	
			return;
		}
	}

	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( iJoystick );

	int iController = XBX_GetUserId( iJoystick );
	bool bIsPrimaryUser = ( iController >= 0 && XBX_GetPrimaryUserId() == DWORD( iController ) );

	KeyCode localCode = GetBaseButtonCode( code );

	if ( ( localCode == KEY_XBUTTON_A ) )
	{
		if ( m_bIgnoreButtonA )
		{
			// Don't swallow the a key... our parent wants it
			CallParentFunction(new KeyValues("KeyCodePressed", "code", code));
			return;
		}

		bool bEnabled = true;
		if ( !IsEnabled() )
		{
			bEnabled = false;
		}

		switch( m_iUsablePlayerIndex )
		{
		case USE_EVERYBODY:
			break;

		case USE_PRIMARY:
			if ( !bIsPrimaryUser )
				bEnabled = false;
			break;

		case USE_SLOT0:
		case USE_SLOT1:
		case USE_SLOT2:
		case USE_SLOT3:
			if ( iJoystick != m_iUsablePlayerIndex )
				bEnabled = false;
			break;

		default:
			bEnabled = false;
			break;
		}

		if ( !bEnabled )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}
	}

	if ( IsX360() && m_nStyle == BUTTON_GAMEMODE )
	{
		GameModes *pGameModes = dynamic_cast< GameModes * >( GetParent() );
		if ( pGameModes )
		{
			switch ( localCode )
			{
			case KEY_XBUTTON_A:
				if ( pGameModes->IsScrollBusy() )
				{
					// swallow it
					return;
				}
				break;

			case KEY_XBUTTON_LEFT:
			case KEY_XSTICK1_LEFT:
			case KEY_XSTICK2_LEFT:
				pGameModes->ScrollLeft();
				break;

			case KEY_XBUTTON_RIGHT:
			case KEY_XSTICK1_RIGHT:
			case KEY_XSTICK2_RIGHT:
				pGameModes->ScrollRight();
				break;
			}
		}
	}

	BaseClass::OnKeyCodePressed( code );
}

void BaseModHybridButton::OnKeyCodeReleased( KeyCode keycode )
{
	//at some point vgui_controls/button.cpp got a 360 only change to never set the armed state to false ever. Too late to fix the logic behind that, just roll with it on PC
	//fixes bug where menu items de-highlight after letting go of the arrow key used to navigate to it
	bool bOldArmedState = IsArmed();

	BaseClass::OnKeyCodeReleased( keycode );

	if( bOldArmedState && !IsArmed() )
		SetArmed( true );
}

void BaseModHybridButton::SetDropdownSelection( const char *pText )
{
	m_DropDownSelection = pText;
}

void BaseModHybridButton::EnableDropdownSelection( bool bEnable )
{
	m_bDropDownSelection = bEnable;
}

void BaseModHybridButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	if ( IsPC() )
	{
		if ( !m_isOpen )
		{
			if ( IsEnabled() && !HasFocus() )
			{
				CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			}

			if( GetParent() )
			{
				GetParent()->NavigateToChild( this );
			}
			else
			{
				NavigateTo();
			}
		}
	}
}

void BaseModHybridButton::OnCursorExited()
{
	// This is a hack for now, we shouldn't close if the cursor goes to the flyout of this item...
	// Maybe have VFloutMenu check the m_navFrom and it's one of these, keep the SetClosedState...
	BaseClass::OnCursorExited();
	if ( IsPC() )
	{
//		SetClosed();
	}
}

// Message targets that the button has been pressed
void BaseModHybridButton::FireActionSignal( void )
{
	BaseClass::FireActionSignal();

	if ( IsPC() )
	{
		PostMessageToAllSiblingsOfType< BaseModHybridButton >( new KeyValues( "OnSiblingHybridButtonOpened" ) );
	}
}


Panel* BaseModHybridButton::NavigateUp()
{
	Panel *target = BaseClass::NavigateUp();
	if ( IsPC() && !target && 
		(dynamic_cast< DropDownMenu * >( GetParent() ) || dynamic_cast< SliderControl * >( GetParent() )) )
	{
		target = GetParent()->NavigateUp();
	}

	return target;
}

Panel* BaseModHybridButton::NavigateDown()
{
	Panel *target = BaseClass::NavigateDown();
	if ( IsPC() && !target && 
		(dynamic_cast< DropDownMenu * >( GetParent() ) || dynamic_cast< SliderControl * >( GetParent() )) )
	{
		target = GetParent()->NavigateDown();
	}
	return target;
}

Panel* BaseModHybridButton::NavigateLeft()
{
	Panel *target = BaseClass::NavigateLeft();
	if ( IsPC() && !target && 
		dynamic_cast< DropDownMenu * >( GetParent() ) )
	{
		target = GetParent()->NavigateLeft();
	}
	return target;
}

Panel* BaseModHybridButton::NavigateRight()
{
	Panel *target = BaseClass::NavigateRight();
	if ( IsPC() && !target && 
		dynamic_cast< DropDownMenu * >( GetParent() ) )
	{
		target = GetParent()->NavigateRight();
	}
	return target;
}
