#include "cbase.h"
#include "nb_button.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CNB_Button, CNB_Button );

CNB_Button::CNB_Button(Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget, const char *pCmd)
: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	// == MANAGED_MEMBER_CREATION_END ==
}
CNB_Button::CNB_Button(Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget, const char *pCmd)
: BaseClass( parent, panelName, text, pActionSignalTarget, pCmd )
{
}

CNB_Button::~CNB_Button()
{

}

void CNB_Button::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetButtonBorderEnabled( false );

	SetReleasedSound( "UI/menu_accept.wav" );
}

void CNB_Button::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Button::OnThink()
{
	BaseClass::OnThink();
}

void CNB_Button::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}


void CNB_Button::Paint()
{
	if ( !ShouldPaint() )
		return; 

	BaseClass::BaseClass::Paint();  // skip drawing regular vgui::Button's focus border
}

void CNB_Button::DrawRoundedBox( int x, int y, int wide, int tall, Color color, float normalizedAlpha, bool bHighlightGradient, Color highlightCenterColor )
{
	if ( m_nNBBgTextureId1 == -1 ||
		m_nNBBgTextureId2 == -1 ||
		m_nNBBgTextureId3 == -1 ||
		m_nNBBgTextureId4 == -1 )
	{
		return;
	}

	color[3] *= normalizedAlpha;

	// work out our bounds
	int cornerWide, cornerTall;
	GetCornerTextureSize( cornerWide, cornerTall );

	// draw the background in the areas not occupied by the corners
	// draw it in three horizontal strips
	surface()->DrawSetColor(color);
	surface()->DrawFilledRect(x + cornerWide, y, x + wide - cornerWide,	y + cornerTall);
	surface()->DrawFilledRect(x, y + cornerTall, x + wide, y + tall - cornerTall);
	surface()->DrawFilledRect(x + cornerWide, y + tall - cornerTall, x + wide - cornerWide, y + tall);

	// draw the corners
	surface()->DrawSetTexture(m_nNBBgTextureId1);
	surface()->DrawTexturedRect(x, y, x + cornerWide, y + cornerTall);
	surface()->DrawSetTexture(m_nNBBgTextureId2);
	surface()->DrawTexturedRect(x + wide - cornerWide, y, x + wide, y + cornerTall);
	surface()->DrawSetTexture(m_nNBBgTextureId3);
	surface()->DrawTexturedRect(x + wide - cornerWide, y + tall - cornerTall, x + wide, y + tall);
	surface()->DrawSetTexture(m_nNBBgTextureId4);
	surface()->DrawTexturedRect(x + 0, y + tall - cornerTall, x + cornerWide, y + tall);

	if ( bHighlightGradient )
	{
		surface()->DrawSetColor(highlightCenterColor);
		surface()->DrawFilledRectFade( x + cornerWide, y, x + wide * 0.5f, y + tall, 0, 255, true );
		surface()->DrawFilledRectFade( x + wide * 0.5f, y, x + wide - cornerWide, y + tall, 255, 0, true );
	}
}

void CNB_Button::PaintBackground()
{
	// draw gray outline background
	DrawRoundedBox( 0, 0, GetWide(), GetTall(), Color( 78, 94, 110, 255 ), 1.0f, false, Color( 0, 0, 0, 0 ) );

	int nBorder = MAX( YRES( 1 ), 1 );
	if ( IsArmed() || IsDepressed() )
	{
		DrawRoundedBox( nBorder, nBorder, GetWide() - nBorder * 2, GetTall() - nBorder * 2, Color( 20, 59, 96, 255 ), 1.0f, true, Color( 28, 80, 130, 255 ) );
	}
	else if ( IsEnabled() )
	{
		DrawRoundedBox( nBorder, nBorder, GetWide() - nBorder * 2, GetTall() - nBorder * 2, Color( 24, 43, 66, 255 ), 1.0f, false, Color( 0, 0, 0, 0 ) );
	}
	else
	{
		DrawRoundedBox( nBorder, nBorder, GetWide() - nBorder * 2, GetTall() - nBorder * 2, Color( 65, 78, 91, 255 ), 1.0f, false, Color( 0, 0, 0, 0 ) );
	}
}

void CNB_Button::OnCursorEntered()
{
	if ( IsPC() )
	{
		if ( IsEnabled() && !HasFocus() )
		{
			vgui::surface()->PlaySound( "UI/menu_focus.wav" );
		}
	}
	BaseClass::OnCursorEntered();
}