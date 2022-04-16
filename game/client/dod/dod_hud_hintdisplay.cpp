//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Label.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "text_message.h"
#include "dod_hud_freezepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CDODHudHintDisplay : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CDODHudHintDisplay, vgui::Panel );

public:
	CDODHudHintDisplay( const char *pElementName );

	void Init();
	void Reset();
	void MsgFunc_HintText( bf_read &msg );
	void FireGameEvent( IGameEvent * event);

	bool SetHintText( wchar_t *text );

	virtual void PerformLayout();

	virtual bool IsVisible( void );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

private:
	vgui::HFont m_hFont;
	Color		m_bgColor;
	vgui::Label *m_pLabel;
	CUtlVector<vgui::Label *> m_Labels;
	CPanelAnimationVarAliasType( int, m_iTextX, "text_xpos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextY, "text_ypos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iCenterX, "center_x", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iCenterY, "center_y", "0", "proportional_int" );
};

DECLARE_HUDELEMENT( CDODHudHintDisplay );
DECLARE_HUD_MESSAGE( CDODHudHintDisplay, HintText );

#define MAX_HINT_STRINGS 5


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODHudHintDisplay::CDODHudHintDisplay( const char *pElementName ) : BaseClass(NULL, "HudHintDisplay"), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 0 );
	m_pLabel = new vgui::Label( this, "HudHintDisplayLabel", "" );

	RegisterForRenderGroup( "winpanel" );
	RegisterForRenderGroup( "freezepanel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::Init()
{
	HOOK_HUD_MESSAGE( CDODHudHintDisplay, HintText );

	// listen for client side events
	ListenForGameEvent( "player_hintmessage" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::Reset()
{
	SetHintText( NULL );
	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( GetSchemeColor("HintMessageFg", pScheme) );
	m_hFont = pScheme->GetFont( "HudHintText", true );
	m_pLabel->SetBgColor( GetSchemeColor("HintMessageBg", pScheme) );
	m_pLabel->SetPaintBackgroundType( 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the hint text, replacing variables as necessary
//-----------------------------------------------------------------------------
bool CDODHudHintDisplay::SetHintText( wchar_t *text )
{
	// clear the existing text
	for (int i = 0; i < m_Labels.Count(); i++)
	{
		m_Labels[i]->MarkForDeletion();
	}
	m_Labels.RemoveAll();

	wchar_t *p = text;

	while ( p )
	{
		wchar_t *line = p;
		wchar_t *end = wcschr( p, L'\n' );
		if ( end )
		{
			//*end = 0;	//eek
			p = end+1;
		}
		else
		{
			p = NULL;
		}

		// copy to a new buf if there are vars
		wchar_t buf[512];
		buf[0] = '\0';
		int pos = 0;

		wchar_t *ws = line;
		while( ws != end && *ws != 0 )
		{
			// check for variables
			if ( *ws == '%' )
			{
				++ws;

				wchar_t *end = wcschr( ws, '%' );
				if ( end )
				{
					wchar_t token[64];
					wcsncpy( token, ws, end - ws );
					token[end - ws] = 0;

					ws += end - ws;

					// lookup key names
					char binding[64];
					g_pVGuiLocalize->ConvertUnicodeToANSI( token, binding, sizeof(binding) );

					const char *key = engine->Key_LookupBinding( *binding == '+' ? binding + 1 : binding );
					if ( !key )
					{
						key = "< not bound >";
					}

					//!! change some key names into better names
					char friendlyName[64];
					Q_snprintf( friendlyName, sizeof(friendlyName), "%s", key );
					Q_strupr( friendlyName );

					g_pVGuiLocalize->ConvertANSIToUnicode( friendlyName, token, sizeof(token) );

					buf[pos] = '\0';
					wcscat( buf, token );
					pos += wcslen(token);
				}
				else
				{
					buf[pos] = *ws;
					++pos;
				}
			}
			else
			{
				buf[pos] = *ws;
				++pos;
			}

			++ws;
		}

		buf[pos] = '\0';

		// put it in a label
		//vgui::Label *label = vgui::SETUP_PANEL(new vgui::Label(this, NULL, line));
		vgui::Label *label = vgui::SETUP_PANEL(new vgui::Label(this, NULL, buf));
		label->SetFont( m_hFont );
		label->SetPaintBackgroundEnabled( false );
		label->SetPaintBorderEnabled( false );
		label->SizeToContents();
		label->SetContentAlignment( vgui::Label::a_west );
		label->SetFgColor( GetFgColor() );
		m_Labels.AddToTail( vgui::SETUP_PANEL(label) );
	}
	InvalidateLayout( true );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Resizes the label
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::PerformLayout()
{
	BaseClass::PerformLayout();
	int i;

	int wide, tall;
	GetSize( wide, tall );

	// find the widest line
	int labelWide = 0;
	for ( i=0; i<m_Labels.Count(); ++i )
	{
		labelWide = MAX( labelWide, m_Labels[i]->GetWide() );
	}

	// find the total height
	int fontTall = vgui::surface()->GetFontTall( m_hFont );
	int labelTall = fontTall * m_Labels.Count();

	labelWide += m_iTextX*2;
	labelTall += m_iTextY*2;
	int x, y;
	if ( m_iCenterX < 0 )
	{
		x = 0;
	}
	else if ( m_iCenterX > 0 )
	{
		x = wide - labelWide;
	}
	else
	{
		x = (wide - labelWide) / 2;
	}

	if ( m_iCenterY > 0 )
	{
		y = 0;
	}
	else if ( m_iCenterY < 0 )
	{
		y = tall - labelTall;
	}
	else
	{
		y = (tall - labelTall) / 2;
	}
	m_pLabel->SetBounds( x, y, labelWide, labelTall );

	// now lay out the sub-labels
	for ( i=0; i<m_Labels.Count(); ++i )
	{
		int xOffset = (labelWide - m_Labels[i]->GetWide())/2;
		m_Labels[i]->SetPos( x + xOffset, y + m_iTextY + i*fontTall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::OnThink()
{
	m_pLabel->SetFgColor(GetFgColor());
	for (int i = 0; i < m_Labels.Count(); i++)
	{
		m_Labels[i]->SetFgColor(GetFgColor());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activates the hint display
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::MsgFunc_HintText( bf_read &msg )
{
	// read the string(s)
	char szString[255];
	static wchar_t szBuf[128];
	static wchar_t *pszBuf;

	// init buffers & pointers
	szBuf[0] = 0;
	pszBuf = szBuf;

	// read string and localize it
	msg.ReadString( szString, sizeof(szString) );

	char *tmpStr = hudtextmessage->LookupString( szString, NULL );

	// try to localize
	if ( tmpStr )
	{
		pszBuf = g_pVGuiLocalize->Find( tmpStr );
	}
	else
	{
		pszBuf = g_pVGuiLocalize->Find( szString );
	}

	if ( !pszBuf )
	{
		// use plain ASCII string 
		g_pVGuiLocalize->ConvertANSIToUnicode( szString, szBuf, sizeof(szBuf) );
		pszBuf = szBuf;
	}

	// make it visible
	if ( SetHintText( pszBuf ) )
	{
		SetVisible( true );
		//SetAlpha( 255 );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HintMessageShow" ); 
	}
	else
	{
		// it's being cleared, hide the panel
		//SetAlpha( 0 );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HintMessageHide" ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activates the hint display upon recieving a hint
//-----------------------------------------------------------------------------
void CDODHudHintDisplay::FireGameEvent( IGameEvent * event)
{
	// we sometimes hide the element when it's covered, don't start
	// a hint during that time
	if ( !ShouldDraw() )
		return;

	static wchar_t *pszBuf;
	static wchar_t szBuf[128];

	const char *hintmessage = event->GetString( "hintmessage" );

	char *tmpStr = hudtextmessage->LookupString( hintmessage, NULL );

	// try to localize
	if ( tmpStr )
	{
		pszBuf = g_pVGuiLocalize->Find( tmpStr );
	}
	else
	{
		pszBuf = g_pVGuiLocalize->Find( hintmessage );
	}

	if ( !pszBuf )
	{
		// its not in titles.txt or dod_english.txt, just print the text of it
		// use plain ASCII string 
		g_pVGuiLocalize->ConvertANSIToUnicode( hintmessage, szBuf, sizeof(szBuf) );
		pszBuf = szBuf;
	}

	// make it visible
	if ( SetHintText( pszBuf ) )
	{
		SetVisible( true );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HintMessageShow" ); 
	}
	else
	{
		// it's being cleared, hide the panel
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HintMessageHide" ); 
	}
}

bool CDODHudHintDisplay::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	if ( !ShouldDraw() )
		return false;

	return BaseClass::IsVisible();
}

