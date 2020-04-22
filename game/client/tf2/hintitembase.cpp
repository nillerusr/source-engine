//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hintitembase.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include "PanelEffect.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *parent - 
//			*panelName - 
//			*text - 
//			itemwidth - 
//-----------------------------------------------------------------------------
CHintItemBase::CHintItemBase( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, panelName ), m_pObject( NULL )
{
	m_pLabel = new vgui::Label( this, "TFTextHint", "" );
	m_pLabel->SetContentAlignment( vgui::Label::a_west );
	
	//	m_pIndex = new vgui::Label( this, "TFTextHintIndex", "" );
	//	m_pIndex->setContentAlignment( vgui::Label::a_west );
	//	m_pIndex->SetBounds( 20, 0, 20, 20 );
	//	m_nIndex = 0;	
	
	m_bCompleted = false;
	m_bActive = false;
	m_flActivateTime = 0.0f;
	
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	SetFormatString( "" );

	m_bAutoComplete			= false;
	m_flAutoCompleteTime	= 0.0f;

	m_hSmallFont = m_hMarlettFont = 0; 
}


//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void CHintItemBase::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hSmallFont = pScheme->GetFont( "DefaultVerySmall" );
	m_hMarlettFont = pScheme->GetFont( "Marlett" );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKeyValues - 
//-----------------------------------------------------------------------------
void CHintItemBase::ParseItem( KeyValues *pKeyValues )
{
	if ( !pKeyValues )
		return;
	
	const char *title = pKeyValues->GetString( "title", "" );
	if ( title )
	{
		SetText( title );
	}

	const char *fmt = pKeyValues->GetString( "formatstring", "" );
	if ( fmt )
	{
		SetFormatString( fmt );
	}

	const char *autocomplete = pKeyValues->GetString( "autocomplete" );
	if ( autocomplete && autocomplete[ 0 ] )
	{
		SetAutoComplete( ( float ) atof( autocomplete ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : elapsed_time - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetAutoComplete( float elapsed_time )
{
	m_bAutoComplete = true;
	m_flAutoCompleteTime = elapsed_time;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : newWide - 
//			newTall - 
//-----------------------------------------------------------------------------
void CHintItemBase::OnSizeChanged( int newWide, int newTall )
{
	m_pLabel->SetBounds( 20, 0, GetWide() - 20, newTall );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetText( const char *text )
{
	m_pLabel->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetFormatString( const char *fmt )
{
	Q_strncpy( m_szFormatString, fmt, MAX_TEXT_LENGTH );
	m_bUseFormatString = fmt[ 0 ] ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintItemBase::GetFormatString( void )
{
	Assert( m_bUseFormatString );
	return m_szFormatString;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *instring - 
//			keylength - 
//			**ppOutstring - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintItemBase::CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring )
{
	Assert( m_bUseFormatString );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintItemBase::ComputeTitle( void )
{
	if ( !m_bUseFormatString )
		return;

	char fixed[ MAX_TEXT_LENGTH ];

	char *in, *out;
	in = m_szFormatString;
	out = fixed;

	while ( *in )
	{
		if ( *in == '!' && *(in+1) == '!' )
		{
			in += 2;

			int length;
			const char *text;

			if ( CheckKeyAndValue( in, &length, &text ) )
			{
				const char *t = text;
				while ( *t )
				{
					*out++ = *t++;
				}

				in += length;
			}
		}
		else
		{
			*out++ = *in++;
		}
	}

	*out = 0;

	SetText( fixed );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *binding - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CHintItemBase::GetKeyNameForBinding( const char *binding )
{
	const char *keyname = engine->Key_LookupBinding( binding );
	if ( keyname )
	{
		return keyname;
	}

	// Return original string if not bound to a key
	return binding;
}

//-----------------------------------------------------------------------------
// Purpose: Delete's the hint object
//-----------------------------------------------------------------------------
void CHintItemBase::DeleteThis( void )
{
	// Remove any associated effects
	DestroyPanelEffects( this );
	
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : incolor - 
//			frac - 
// Output : static int
//-----------------------------------------------------------------------------
static int TextHintColorMod( int incolor, float frac )
{
	int maxcolor = incolor + (int)( ( 255.0f - (float)incolor ) / 2.0f );
	int midcolor = ( maxcolor + incolor ) / 2;
	int range = maxcolor - midcolor;
	
	int clr = midcolor + range * frac;
	
	clr = clamp( clr, 0, 255 );
	return clr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintItemBase::PaintBackground()
{
	// force label color
	if ( m_pLabel )
	{
		m_pLabel->SetFgColor( Color( 0, 0, 0, 255 ) );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
	}
	
	int w, h;
	
	GetSize( w, h );
	
	float f2 = 0.0f;
	
	float dt =  gpGlobals->curtime - m_flActivateTime; 
	
	if ( dt < 5.0f )
	{
		f2 = fmod( gpGlobals->curtime, 1.0f );
	}
	
	int r = 80;
	int g = 105;
	int b = 90;
	
	vgui::surface()->DrawSetTextFont( m_hMarlettFont );
	vgui::surface()->DrawSetTextColor( r, g, b, 255 );
	vgui::surface()->DrawSetTextPos( 2 + 3 * f2, 3 );

	wchar_t ch[2];
	g_pVGuiLocalize->ConvertANSIToUnicode( "4", ch, sizeof( ch ) );
	vgui::surface()->DrawPrintText( ch, 1 );

	if ( m_bAutoComplete )
	{
		vgui::surface()->DrawSetTextColor( 0, 0, 0, 63 );
		vgui::surface()->DrawSetTextFont( m_hSmallFont );

		char sz[ 32 ];
		
		int len = Q_snprintf( sz, sizeof( sz ), "%i", (int)( m_flAutoCompleteTime + 0.5f ) );

		int x = w - len * 10 - 2;
		int y = 6;

		wchar_t szconverted[ 32 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( sz, szconverted, sizeof(szconverted)  );

		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawPrintText( szconverted, wcslen( szconverted ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintItemBase::GetCompleted( void )
{
	return m_bCompleted;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : active - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetActive( bool active )
{
	bool changed = m_bActive != active;
	
	m_bActive = active;
	
	if ( !changed )
		return;
	
	if ( m_bActive )
	{
		m_flActivateTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintItemBase::GetActive( void )
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintItemBase::ShouldRenderPanelEffects( void )
{
	// By default, render active item's effects.
	// A hint could have it's effects always render, though
	return GetActive();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintItemBase::Think( void )
{
	// Check for completion
	if ( m_bAutoComplete && GetActive() )
	{
		m_flAutoCompleteTime -= gpGlobals->frametime;
		if ( m_flAutoCompleteTime <= 0.0f )
		{
			m_bCompleted = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CHintItemBase::GetHeight( void )
{
	return GetTall();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetPosition( int x, int y )
{
	SetPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetItemNumber( int index )
{
/*
m_nIndex = index;

  char sz[ 32 ];
  Q_snprintf( sz, sizeof( sz ), "%i", m_nIndex );
  
	m_pIndex->setText( sz );
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : visible - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetVisible( bool visible )
{
	BaseClass::SetVisible( visible );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetHintTarget( vgui::Panel *panel )
{
	m_pObject = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
//			*value - 
//-----------------------------------------------------------------------------
void CHintItemBase::SetKeyValue( const char *key, const char *value )
{
}
