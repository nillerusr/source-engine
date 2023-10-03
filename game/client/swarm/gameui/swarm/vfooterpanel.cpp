//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VFooterPanel.h"
#include "vgui/IPanel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/ilocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;

// the xbox shrinks its footer when there is not help text
#define MINIMAL_FOOTER_SCALE	0.75f

struct buttonLayout_t
{
	bool	A_rAlight;
	int		A_x;
	int		A_y;

	bool	B_rAlight;
	int		B_x;
	int		B_y;

	bool	X_rAlight;
	int		X_x;
	int		X_y;

	bool	Y_rAlight;
	int		Y_x;
	int		Y_y;

	bool	DPad_rAlight;
	int		DPad_x;
	int		DPad_y;

	CBaseModFooterPanel::FooterButtons_t buttons;
};

buttonLayout_t g_ButtonLayouts[FF_MAX] =
{
	{ false,0,0,	false,0,0,		false,0,0,		false,0,0,		false,0,0,	0 },											// FF_NONE
	{ false,100,50,	false,0,0,		true,240,50,	false,0,0,		false,0,0,	FB_ABUTTON|FB_XBUTTON },						// FF_MAINMENU
	{ false,100,50,	false,225,50,	false,0,0,		false,0,0,		false,0,0,	FB_ABUTTON|FB_BBUTTON },						// FF_MAINMENU_FLYOUT,
	{ false,100,50,	false,225,50,	false,0,0,		false,0,0,		false,0,0,	FB_ABUTTON|FB_BBUTTON },						// FF_CONTROLLER,
	{ false,100,50,	false,225,50,	false,0,0,		false,0,0,		false,0,0,	FB_ABUTTON|FB_BBUTTON },						// FF_AB_ONLY
	{ false,100,50,	false,225,50,	true,240,50,	false,0,0,		false,0,0,	FB_ABUTTON|FB_BBUTTON|FB_XBUTTON },				// FF_ABX_ONLY
	{ false,100,50,	false,225,50,	false,0,0,		true,240,50,	false,0,0,	FB_ABUTTON|FB_BBUTTON|FB_YBUTTON },				// FF_ABY_ONLY
	{ false,225,50,	false,100,50,	false,0,0,		true,240,50,	false,0,0,	FB_ABUTTON|FB_BBUTTON|FB_YBUTTON },				// FF_A_GAMERCARD_BY
	{ false,225,50,	false,100,50,	true,240,50,	false,0,0,		false,0,0,	FB_ABUTTON|FB_BBUTTON|FB_XBUTTON },				// FF_A_GAMERCARD_BX
	{ false,225,50,	false,100,50,	true,240,38,	true,240,58,	false,0,0,	FB_ABUTTON|FB_BBUTTON|FB_XBUTTON|FB_YBUTTON },	// FF_A_GAMERCARD_BY__X_HIGH
	{ false,0,0,	false,100,50,	false,0,0,		false,0,0,		false,0,0,	FB_BBUTTON },									// FF_B_ONLY
	{ false,100,60,	false,0,0,		false,0,0,		true,240,60,	false,0,0,	FB_ABUTTON|FB_YBUTTON },						// FF_CONTROLLER_STICKS,
	{ false,0,0,	false,100,50,	false,0,0,		false,225,50,	false,0,0,	FB_BBUTTON|FB_YBUTTON },						// FF_ACHIEVEMENTS,
};

CBaseModFooterPanel::CBaseModFooterPanel( vgui::Panel *parent, const char *panelName ) :
BaseClass( parent, panelName, false, false, false, false )
{
	vgui::ipanel()->SetTopmostPopup( GetVPanel(), true );

	SetProportional( true );
	SetTitle( "", false );

	SetUpperGarnishEnabled( false );

	m_AButtonText[0] = '\0';
	m_BButtonText[0] = '\0';
	m_XButtonText[0] = '\0';
	m_YButtonText[0] = '\0';
	m_DPadButtonText[0] = '\0';
	m_HelpText[0] = '\0';
	m_szconverted[0] = L'\0';

	m_Format = FF_NONE;
	m_Buttons = 0;

	m_hHelpTextFont = vgui::INVALID_FONT;
	m_hButtonFont = vgui::INVALID_FONT;
	m_hButtonTextFont = vgui::INVALID_FONT;

	m_bInitialized = false;

	m_fFadeHelpTextStart = 0.0f;
}

CBaseModFooterPanel::~CBaseModFooterPanel()
{
}

void CBaseModFooterPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetBgColor( Color ( 0, 0, 0, 200 ) );

	m_hHelpTextFont = pScheme->GetFont( "DefaultMedium", true );	
	m_hButtonTextFont = pScheme->GetFont( "DefaultBold", true );
	m_hButtonFont = pScheme->GetFont( "GameUIButtonsTiny", true );

	m_bInitialized = true;
}

void CBaseModFooterPanel::UpdateHelp()
{
	if ( !m_bInitialized )
	{
		return;
	}

	if ( m_hHelpTextFont == vgui::INVALID_FONT )
	{
		// Fonts aren't initialized yet!
		DevWarning( "UI Footer: Help fonts not initialized!\n" );
		return;
	}

	vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
	if ( pLblHelpText )
	{
		// Use the text from last frame
		pLblHelpText->SetText( m_szconverted );

		// Get the centered text position for the next frame
		wchar_t *pUnicodeString = g_pVGuiLocalize->Find( m_HelpText );
		if ( pUnicodeString )
		{
			Q_wcsncpy( m_szconverted, pUnicodeString, sizeof( m_szconverted ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( m_HelpText, m_szconverted, sizeof( m_szconverted ) );
		}

		int textWide, textTall;
		vgui::surface()->GetTextSize( m_hHelpTextFont, m_szconverted, textWide, textTall );

		int wide, tall;
		GetSize( wide, tall );

		int iPanelWide = pLblHelpText->GetWide();

		// Center about either the wrapping width or the string width (whichever is smaller)
		int helpX, helpY;
		pLblHelpText->GetPos( helpX, helpY );
		
		if ( IsX360() )
		{
			helpY = vgui::scheme()->GetProportionalScaledValue( 25 );
		}

		pLblHelpText->SetPos( ( wide - MIN( iPanelWide, textWide ) )/2, helpY );

		int iAlpha = 255;

		if ( m_fFadeHelpTextStart != 0.0f )
		{
			iAlpha = MAX( static_cast<int>( 255.0f - ( Plat_FloatTime() - m_fFadeHelpTextStart ) * 255.0f ), 0 );
		}

		pLblHelpText->SetAlpha( iAlpha );
	}
}

void CBaseModFooterPanel::DrawButtonAndText( bool bRightAligned, int x, int y, const char *pButton, const char *pText )
{
	if ( !m_bInitialized )
	{
		return;
	}

	if ( m_hButtonFont == vgui::INVALID_FONT || m_hButtonTextFont == vgui::INVALID_FONT )
	{
		// Fonts aren't initialized yet!
		DevWarning( "UI Footer: Button fonts not initialized!\n" );
		return;
	}

	int buttonLen;
	wchar_t szButtonConverted[32];
	wchar_t *pButtonString = g_pVGuiLocalize->Find( pButton );
	if ( !pButtonString )
	{
		buttonLen = g_pVGuiLocalize->ConvertANSIToUnicode( pButton, szButtonConverted, sizeof( szButtonConverted ) );
		pButtonString = szButtonConverted;
	}
	else
	{
		buttonLen = V_wcslen( pButtonString );
	}

	int screenWide, screenTall;
	vgui::surface()->GetScreenSize( screenWide, screenTall );

	x = vgui::scheme()->GetProportionalScaledValue( x );
	y = vgui::scheme()->GetProportionalScaledValue( y );

	if ( bRightAligned )
	{
		x = screenWide - x;
	}

	int buttonWide, buttonTall;
	vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );

	int labelLen;
	wchar_t szLabelConverted[32];
	wchar_t *pLabelString = g_pVGuiLocalize->Find( pText );
	if ( !pLabelString )
	{
		labelLen = g_pVGuiLocalize->ConvertANSIToUnicode( pText, szLabelConverted, sizeof( szLabelConverted ) );
		pLabelString = szLabelConverted;
	}
	else
	{
		labelLen = V_wcslen( pLabelString );
	}

	int textWide, textTall;
	vgui::surface()->GetTextSize( m_hButtonTextFont, pLabelString, textWide, textTall );

	int iTitleSafeRightEdge = screenWide * 0.95;
	if ( bRightAligned && ( x + textWide ) > iTitleSafeRightEdge )
	{
		// This will go out of title safe, move to the left
		x = iTitleSafeRightEdge - textWide;
	}

	vgui::surface()->DrawSetTextFont( m_hButtonFont );
	vgui::surface()->DrawSetTextPos( x - buttonWide - 5, y - (buttonTall/2) );
	vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
	vgui::surface()->DrawPrintText( pButtonString, buttonLen );

	vgui::surface()->DrawSetTextFont( m_hButtonTextFont );
	vgui::surface()->DrawSetTextPos( x, y - (textTall/2) );
	vgui::surface()->DrawPrintText( pLabelString, labelLen );
}


bool CBaseModFooterPanel::HasContent( void )
{
	vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
	bool bHasVisibleHelp = pLblHelpText && pLblHelpText->IsVisible();

	if ( bHasVisibleHelp )
		return true;

	if ( IsX360() )
	{
		if ( m_Buttons )
			return true;
	}

	if ( IsPC() )
	{
		vgui::Panel *pCloudLabel = FindChildByName( "UsesCloudLabel" );
		if ( pCloudLabel && pCloudLabel->IsVisible() )
		{
			return true;
		}
	}

	return false;
}

void CBaseModFooterPanel::PaintBackground()
{	
	if ( !HasContent() )
		return;

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	y = 0;

	if ( IsX360() )
	{
		vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
		bool bHasVisibleHelp = pLblHelpText && pLblHelpText->IsVisible();

		if ( !bHasVisibleHelp )
		{
			// shrink and move the footer down
			y = tall * ( 1.0f - MINIMAL_FOOTER_SCALE );
			tall = tall * MINIMAL_FOOTER_SCALE;
		}
	}

	DrawSmearBackground( 0, y, wide, tall, true );

	if ( IsX360() )
	{
		buttonLayout_t *pLayout = &g_ButtonLayouts[m_Format];

		int yOffset = IsX360() ? 8 : 0;

		if ( m_Buttons & FB_ABUTTON )
		{
			DrawButtonAndText( pLayout->A_rAlight, pLayout->A_x, pLayout->A_y + yOffset, "#GameUI_Icons_A_3DButton", m_AButtonText );
		}
		if ( m_Buttons & FB_BBUTTON )
		{
			DrawButtonAndText( pLayout->B_rAlight, pLayout->B_x, pLayout->B_y + yOffset, "#GameUI_Icons_B_3DButton", m_BButtonText );
		}
		if ( m_Buttons & FB_XBUTTON )
		{
			DrawButtonAndText( pLayout->X_rAlight, pLayout->X_x, pLayout->X_y + yOffset, "#GameUI_Icons_X_3DButton", m_XButtonText );
		}
		if ( m_Buttons & FB_YBUTTON )
		{
			DrawButtonAndText( pLayout->Y_rAlight, pLayout->Y_x, pLayout->Y_y + yOffset, "#GameUI_Icons_Y_3DButton", m_YButtonText );
		}
		if ( m_Buttons & FB_DPAD )
		{
			DrawButtonAndText( pLayout->DPad_rAlight, pLayout->DPad_x, pLayout->DPad_y + yOffset, "#GameUI_Icons_CENTER_DPAD", m_DPadButtonText );
		}
	}

	UpdateHelp();
}

void CBaseModFooterPanel::GetPosition( int &x, int &y )
{
	int wide, tall;
	GetBounds( x, y, wide, tall );

	if ( IsPC() )
	{
		return;
	}

	vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
	bool bHasVisibleHelp = pLblHelpText && pLblHelpText->IsVisible();

	if ( !bHasVisibleHelp )
	{
		// shrink and move the footer down
		y += tall * ( 1.0f - MINIMAL_FOOTER_SCALE );
	}
}

void CBaseModFooterPanel::SetButtons( CBaseModFooterPanel::FooterButtons_t flags, FooterFormat_t format, bool bEnableHelp )
{
	// this state must stay locked to the button layout as we lack a stack
	// the stupid ui code already constantly slams the button state to maintain it's view of the global footer
	SetHelpTextEnabled( bEnableHelp );

	m_Format = format;

	// caller can only enable/disable what is allowed by the format
	m_Buttons = flags & g_ButtonLayouts[m_Format].buttons;	
}

void CBaseModFooterPanel::SetShowCloud( bool bShow )
{
	if ( IsPC() )
	{
		SetControlVisible( "ImageCloud", bShow );
		SetControlVisible( "UsesCloudLabel", bShow );
	}
}

CBaseModFooterPanel::FooterButtons_t CBaseModFooterPanel::GetButtons()
{
	return m_Buttons;
}

FooterFormat_t CBaseModFooterPanel::GetFormat()
{
	return m_Format;
}

bool CBaseModFooterPanel::GetHelpTextEnabled()
{
	vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
	return ( pLblHelpText && pLblHelpText->IsVisible() );
}

void CBaseModFooterPanel::SetButtonText( Button_t button, const char *pText )
{
	char *pButtonText;
	switch( button )
	{
	default:
	case FB_NONE:
		return;
	case FB_ABUTTON:
		pButtonText = m_AButtonText;
		break;
	case FB_BBUTTON:
		pButtonText = m_BButtonText;
		break;
	case FB_XBUTTON:
		pButtonText = m_XButtonText;
		break;
	case FB_YBUTTON:
		pButtonText = m_YButtonText;
		break;
	case FB_DPAD:
		pButtonText = m_DPadButtonText;
		break;
	}

	V_strncpy( pButtonText, pText, FOOTERPANEL_TEXTLEN );
}

void CBaseModFooterPanel::SetHelpTextEnabled( bool bEnabled )
{
	vgui::Label *pLblHelpText = dynamic_cast< vgui::Label* >( FindChildByName( "LblHelpText" ) );
	if ( pLblHelpText )
	{
		pLblHelpText->SetVisible( bEnabled );
	}
}

void CBaseModFooterPanel::SetHelpText( const char *pText )
{
	if ( pText )
	{
		Q_strncpy( m_HelpText, pText, sizeof( m_HelpText ) );
		m_fFadeHelpTextStart = 0.0f;
	}
}

void CBaseModFooterPanel::FadeHelpText( void )
{
	m_fFadeHelpTextStart = Plat_FloatTime();
}