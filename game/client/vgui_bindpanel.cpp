#include "cbase.h"
#include "vgui_bindpanel.h"
#include <vgui/ilocalize.h>
#include <vgui/isurface.h>
#include <vgui/ivgui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar in_joystick;

#define ICON_SIZE	0.04f	// Icons are ScreenWidth() * ICON_SIZE wide.
#define ICON_GAP	5		// Number of pixels between the icon and the text

CBindPanel::CBindPanel(Panel *parent, const char *panelName) : BaseClass( parent, panelName )
{
	SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/ClientScheme.res", "ClientScheme" ) );
	m_fWidthScale = 1.0f;
	m_szBind[0] = '\0';
	m_szKey[0] = '\0';
	m_szLastKey[0] = '\0';
	m_iSlot = -1;
	m_szBackgroundTextureName[0] = '\0';
	m_bController = false;
	m_bDrawKeyText = true;
	m_pBackground = NULL;
	m_fScale = 1.0f;
	m_fUpdateTime = 0;
}

CBindPanel::~CBindPanel()
{
}

void CBindPanel::OnThink( void )
{
	if ( gpGlobals->curtime > m_fUpdateTime )
	{
		UpdateKey();
	}
}

void CBindPanel::ApplySettings( KeyValues *inResourceData )
{
	const char *pszBind = inResourceData->GetString( "bind", "" );

	if ( pszBind && pszBind[0] )
	{
		int nSlot = vgui::ipanel()->GetMessageContextId( GetVPanel() );
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

		SetBind( pszBind, nSlot );
	}

	BaseClass::ApplySettings( inResourceData );
}

void CBindPanel::SetBind( const char *szBind, int iSlot )
{
	if ( !szBind )
	{
		m_szBind[0] = '\0';
		return;
	}
	Q_snprintf( m_szBind, sizeof( m_szBind ), "%s", szBind );

	m_iSlot = iSlot;

	UpdateKey();
}

void CBindPanel::UpdateKey()
{
	MEM_ALLOC_CREDIT();
	const char *key = engine->Key_LookupBindingEx( *m_szBind == '+' ? m_szBind + 1 : m_szBind, m_iSlot );
	if ( key )
	{
		Q_snprintf( m_szKey, sizeof( m_szKey ), key );
	}
	else
	{
		Q_snprintf( m_szKey, sizeof( m_szKey ), "?" );
	}
	if ( Q_stricmp( m_szKey, m_szLastKey ) )
	{
		Q_snprintf( m_szLastKey, sizeof( m_szLastKey ), "%s", m_szKey );
		UpdateBackgroundImage();
		InvalidateLayout();
		PostActionSignal( new KeyValues( "Command", "Command", "KeyChanged" ) );
	}

	m_fUpdateTime = gpGlobals->curtime + 0.75f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBindPanel::UpdateBackgroundImage()
{
	m_fWidthScale = 1.0f;
	m_bDrawKeyText = true;
	Q_strcpy( m_szBackgroundTextureName, "icon_blank" );

	if ( m_szKey[0] == '\0' )
	{
		if ( IsX360() )
		{
			Q_strcpy( m_szBackgroundTextureName, "icon_blank" );
		}
		else
		{
			Q_strcpy( m_szBackgroundTextureName, "icon_key_generic" );
		}
	}
	else if ( IsX360() )
	{
		// Use a blank background for the button icons
		Q_strcpy( m_szBackgroundTextureName, "icon_blank" );
		m_bController = true;

		if ( Q_strcmp( m_szKey, "L_SHOULDER" ) == 0 || 
			Q_strcmp( m_szKey, "R_SHOULDER" ) == 0 )
		{
			m_fWidthScale = 2.0f;
		}
	}
	else if ( in_joystick.GetInt() && 
		( Q_strcmp( m_szKey, "A_BUTTON" ) == 0 || 
		Q_strcmp( m_szKey, "B_BUTTON" ) == 0 || 
		Q_strcmp( m_szKey, "X_BUTTON" ) == 0 || 
		Q_strcmp( m_szKey, "Y_BUTTON" ) == 0 || 
		Q_strcmp( m_szKey, "L_SHOULDER" ) == 0 || 
		Q_strcmp( m_szKey, "R_SHOULDER" ) == 0 || 
		Q_strcmp( m_szKey, "BACK" ) == 0 || 
		Q_strcmp( m_szKey, "START" ) == 0 || 
		Q_strcmp( m_szKey, "STICK1" ) == 0 || 
		Q_strcmp( m_szKey, "STICK2" ) == 0 || 
		Q_strcmp( m_szKey, "UP" ) == 0 || 
		Q_strcmp( m_szKey, "DOWN" ) == 0 || 
		Q_strcmp( m_szKey, "LEFT" ) == 0 || 
		Q_strcmp( m_szKey, "RIGHT" ) == 0 || 
		Q_strcmp( m_szKey, "L_TRIGGER" ) == 0 || 
		Q_strcmp( m_szKey, "R_TRIGGER" ) == 0 ) )
	{
		// Use a blank background for the button icons
		Q_strcpy( m_szBackgroundTextureName, "icon_blank" );
		m_bController = true;

		if ( Q_strcmp( m_szKey, "L_SHOULDER" ) == 0 || 
			Q_strcmp( m_szKey, "R_SHOULDER" ) == 0 )
		{
			m_fWidthScale = 2.0f;
		}
	}
	else if ( Q_strcmp( m_szKey, "MOUSE1" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_mouseLeft" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "MOUSE2" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_mouseRight" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "MOUSE3" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_mouseThree" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "MWHEELUP" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_mouseWheel_up" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "MWHEELDOWN" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_mouseWheel_down" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "UPARROW" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_up" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "LEFTARROW" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_left" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "DOWNARROW" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_down" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "RIGHTARROW" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_right" );
		m_bDrawKeyText = false;
	}
	else if ( Q_strcmp( m_szKey, "SEMICOLON" ) == 0 || 
		Q_strcmp( m_szKey, "INS" ) == 0 || 
		Q_strcmp( m_szKey, "DEL" ) == 0 || 
		Q_strcmp( m_szKey, "HOME" ) == 0 || 
		Q_strcmp( m_szKey, "END" ) == 0 || 
		Q_strcmp( m_szKey, "PGUP" ) == 0 || 
		Q_strcmp( m_szKey, "PGDN" ) == 0 || 
		Q_strcmp( m_szKey, "PAUSE" ) == 0 || 
		Q_strcmp( m_szKey, "F10" ) == 0 || 
		Q_strcmp( m_szKey, "F11" ) == 0 || 
		Q_strcmp( m_szKey, "F12" ) == 0 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_generic" );
	}
	else if ( Q_strlen( m_szKey ) <= 2 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_generic" );
	}
	else if ( Q_strlen( m_szKey ) <= 6 )
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_wide" );
		m_fWidthScale = 2.f;
	}
	else
	{
		Q_strcpy( m_szBackgroundTextureName, "icon_key_wide" );
		m_fWidthScale = 3.f;
	}

	m_pBackground = HudIcons().GetIcon( m_szBackgroundTextureName );
}

void CBindPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_iIconTall = YRES( 25 );

	int iconWide = m_iIconTall * m_fWidthScale * m_fScale;

	SetSize( iconWide, m_iIconTall * m_fScale );
}

void CBindPanel::Paint()
{
	DrawBackgroundIcon();
	if ( m_bDrawKeyText )
	{
		DrawBindingName();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw an icon with the group of static icons. 
//-----------------------------------------------------------------------------
void CBindPanel::DrawBackgroundIcon()
{
	if ( m_bController || !m_pBackground )		// no background image with controller buttons
		return;

	int iconWide = m_iIconTall * m_fWidthScale * m_fScale;

	// Don't draw the icon if we're on 360 and have a binding to draw
	m_pBackground->DrawSelf( 0, 0,
							 iconWide, m_iIconTall * m_fScale,
							 GetFgColor() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBindPanel::DrawBindingName()
{
	int iconWide = m_iIconTall * m_fWidthScale * m_fScale;

	int x = (iconWide>>1);
	int y = ( m_iIconTall * m_fScale ) * 0.5f;

	if ( !m_bController && !IsConsole() )
	{
		// Draw the caption
		vgui::surface()->DrawSetTextFont( m_hKeysFont );
		int fontTall = vgui::surface()->GetFontTall( m_hKeysFont );

		char szBinding[ 256 ];
		Q_strcpy( szBinding, m_szKey );

		if ( Q_strcmp( szBinding, "SEMICOLON" ) == 0 )
		{
			Q_strcpy( szBinding, ";" );
		}
		else if ( Q_strlen( szBinding ) == 1 && szBinding[ 0 ] >= 'a' && szBinding[ 0 ] <= 'z' )
		{
			// Make single letters uppercase
			szBinding[ 0 ] += ( 'A' - 'a' );
		}

		wchar wszCaption[ 64 ];
		g_pVGuiLocalize->ConstructString( wszCaption, sizeof(wchar)*64, szBinding, NULL );

		int iWidth = GetScreenWidthForCaption( wszCaption, m_hKeysFont );

		// Draw black text
		vgui::surface()->DrawSetTextColor( 0,0,0, 255 );
		vgui::surface()->DrawSetTextPos( x - (iWidth>>1) - 1, y - (fontTall >>1) - 1 );
		vgui::surface()->DrawUnicodeString( wszCaption );
	}
	else
	{
		// Draw the caption
		wchar	wszCaption[ 64 ];

		vgui::surface()->DrawSetTextFont( m_hButtonFont );
		int fontTall = vgui::surface()->GetFontTall( m_hButtonFont );

		char szBinding[ 256 ];

		// Turn localized string into icon character
		Q_snprintf( szBinding, sizeof( szBinding ), "#GameUI_Icons_%s", m_szKey );
		g_pVGuiLocalize->ConstructString( wszCaption, sizeof( wszCaption ), g_pVGuiLocalize->Find( szBinding ), 0 );
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszCaption, szBinding, sizeof( szBinding ) );

		int iWidth = GetScreenWidthForCaption( wszCaption, m_hButtonFont );

		// Draw the button
		vgui::surface()->DrawSetTextColor( 255,255,255, 255 );
		vgui::surface()->DrawSetTextPos( x - (iWidth>>1), y - (fontTall >>1) );
		vgui::surface()->DrawUnicodeString( wszCaption );

	}
}



//-----------------------------------------------------------------------------
// Purpose: Figure out how wide (pixels) a string will be if rendered with this font
//-----------------------------------------------------------------------------
int CBindPanel::GetScreenWidthForCaption( const wchar *pString, vgui::HFont hFont )
{
	int iWidth = 0;

	for ( int iChar = 0; iChar < Q_wcslen( pString ); ++ iChar )
	{
		iWidth += vgui::surface()->GetCharacterWidth( hFont, pString[ iChar ] );
	}

	return iWidth;
}

void CBindPanel::SetScale( float fScale )
{
	m_fScale = fScale;
	InvalidateLayout();
}