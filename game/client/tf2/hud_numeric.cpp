//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hud_numeric.h"
#include "iclientmode.h"
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/AnimationController.h>
#include "ctype.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pElementName - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHudNumeric::CHudNumeric( const char *pElementName, const char *panelName )
 : CHudElement( pElementName ), BaseClass( NULL, panelName )
{
	// Make sure we have the lookups built
	BuildPrintablesList();

	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetActive( true );
	SetAutoDelete( false );

	m_nTextLen = 0;
	Q_memset( m_szPreviousValue, 0, sizeof( m_szPreviousValue ) );
	Q_memset( m_szLatchedValue, 0, sizeof( m_szLatchedValue ) );

	m_bDrawLabel = true;
	m_bSendPulses = true;
	m_bPulseForced = true;

	m_flRotaryTime = 0.0f;
	m_flRotaryStartTime = 0.0f;
	m_flActualCharactersPerSecond = 7.0f;
}

bool CHudNumeric::s_bPrintablesBuilt;
CUtlRBTree< int, int > CHudNumeric::m_Printables( 0, 0, DefLessFunc( int ) );

//-----------------------------------------------------------------------------
// Purpose: Builds a list of printable characters
//-----------------------------------------------------------------------------
void CHudNumeric::BuildPrintablesList( void )
{
	if ( s_bPrintablesBuilt )
		return;

	s_bPrintablesBuilt = true;
	int i;
	for ( i = 0; i < 256; i++ )
	{
		if ( isalnum( i ) )  // isprint or isgraph?
		{
			m_Printables.Insert( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find the index of a printable character
// Input  : ch - 
// Output : int
//-----------------------------------------------------------------------------
int CHudNumeric::FindPrintableIndex( int ch )
{
	int idx = m_Printables.Find( ch );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *scheme - 
//-----------------------------------------------------------------------------
void CHudNumeric::ApplySchemeSettings(IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	m_flActualCharactersPerSecond = (float)m_flDesiredCharactersPerSecond;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudNumeric::IsRotating( void ) const
{
	if ( gpGlobals->curtime >= m_flRotaryStartTime &&
		 gpGlobals->curtime <= ( m_flRotaryStartTime + m_flRotaryTime ) )
	{
		 return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frac - 
//			startchar - 
//			endchar - 
//			prevchar - 
//			nextchar - 
//			subfrac - 
//-----------------------------------------------------------------------------
void CHudNumeric::GetRotatedChar( float frac, char startchar, char endchar, char& prevchar, char& nextchar, float& subfrac )
{
	// Recast into database of printable characters
	int startidx	= FindPrintableIndex( startchar );
	int endidx		= FindPrintableIndex( endchar );

	float diff		= ( float )endidx - ( float )startidx;

	// No delta
	if ( diff == 0.0f || 
		startidx == m_Printables.InvalidIndex() ||
		endidx == m_Printables.InvalidIndex() )
	{
		prevchar	= startchar;
		nextchar	= startchar;
		subfrac		= 0.0f;

		return;
	}

	bool reverse = false;
	// Going backwards is same as forward except frac is reversed
	if ( diff < 0.0f )
	{
		reverse = true;
		frac = 1.0f - frac;
		int temp = startidx;
		startidx = endidx;
		endidx = temp;
		diff = -diff;
	}

	float	foutindex = (float)startidx;
	int		outindex;
	float   fnextindex;
	int		nextindex;

	int		maxdelta = (int)diff;

	if ( m_nRotaryMaxDelta != 0 )
	{
		maxdelta = MIN( m_nRotaryMaxDelta, maxdelta );
	}

	// Quantize steps
	// Map frac into maxdelta discrete intervals
	float indicesperstep = diff / (float)maxdelta;
	float fnumstepstaken = clamp( frac * (float)maxdelta, 0.0f, (float)( maxdelta  -0.001f ) );
	int numstepstaken = (int)(fnumstepstaken);

	foutindex = (float)startidx + (float)numstepstaken * indicesperstep;
	foutindex = clamp( foutindex, (float)startidx, (float)endidx );

	outindex	= ( int )foutindex;

	fnextindex = (float)startidx + (float)( numstepstaken + 1 ) * indicesperstep;
	fnextindex = clamp( fnextindex, (float)startidx, (float)endidx );

	nextindex = (int)fnextindex;

	subfrac = fnumstepstaken - (float)numstepstaken;

	if ( !m_Printables.IsValidIndex( outindex ) ||
		 !m_Printables.IsValidIndex( nextindex ) )
	{
		prevchar	= startchar;
		nextchar	= startchar;
		subfrac		= 0.0f;
		return;
	}

	if ( reverse )
	{
		subfrac = 1.0f - subfrac;
		prevchar	= m_Printables[ nextindex ];
		nextchar	= m_Printables[ outindex ];
	}
	else
	{
		prevchar	= m_Printables[ outindex ];
		nextchar	= m_Printables[ nextindex ];
	}
}	

void CHudNumeric::PaintRotatedCharacterHoriz( int x, int y, vgui::HFont& font, int prevchar, int nextchar, float frac )
{
	frac = 3 * frac * frac - 2 * frac * frac * frac;

	int abcA, abcB, abcC;
	surface()->GetCharABCwide( font, prevchar, abcA, abcB, abcC );
	// int fontTall = surface()->GetFontTall( font );

	CharRenderInfo info;
	if ( frac < 0.5f )
	{
		surface()->DrawSetTextPos( x, y );

		// Paint the right half of the prevchar still
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			// Shift tex coord and y position half way down glyph
			info.verts[0].m_Position.x = Lerp( 0.5f, info.verts[0].m_Position.x, info.verts[1].m_Position.x );
			info.verts[0].m_TexCoord.x = Lerp( 0.5f, info.verts[0].m_TexCoord.x, info.verts[1].m_TexCoord.x );
			surface()->DrawRenderCharFromInfo( info );
		}

		surface()->DrawSetTextPos( x, y );

		// Paint up to frac from left of next char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[1].m_Position.x = Lerp( frac, info.verts[0].m_Position.x, info.verts[1].m_Position.x );
			info.verts[1].m_TexCoord.x = Lerp( frac, info.verts[0].m_TexCoord.x, info.verts[1].m_TexCoord.x );
			surface()->DrawRenderCharFromInfo( info );
		}

		// Paint divider

		surface()->DrawSetTextPos( x, y );

		// Paint from frac to 0.5 of prevchar on the left
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info )  )
		{
			// Shift tex coord and y position half way up glyph
			vgui::Vertex_t save[2];
			
			save[0] = info.verts[0];
			save[1] = info.verts[1];

			info.verts[0].m_Position.x = Lerp( frac, save[0].m_Position.x, save[1].m_Position.x );
			//info.verts[0].m_TexCoord.x = Lerp( frac, save[0].m_TexCoord.x, save[1].m_TexCoord.x );

			info.verts[1].m_Position.x = Lerp( 0.5f, save[0].m_Position.x, save[1].m_Position.x );
			info.verts[1].m_TexCoord.x = Lerp( 0.5f, save[0].m_TexCoord.x, save[1].m_TexCoord.x );
			
			surface()->DrawRenderCharFromInfo( info );
		}
	}
	else if ( frac > 0.5f )
	{
		surface()->DrawSetTextPos( x, y );

		// Paint entire left half of next char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position half way down glyph
			info.verts[1].m_Position.x = Lerp( 0.5f, info.verts[0].m_Position.x, info.verts[1].m_Position.x );
			info.verts[1].m_TexCoord.x = Lerp( 0.5f, info.verts[0].m_TexCoord.x, info.verts[1].m_TexCoord.x );
			surface()->DrawRenderCharFromInfo( info );
		}

		surface()->DrawSetTextPos( x, y );

		// paint a bit of the previous char at far right
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[0].m_Position.x = Lerp( frac, info.verts[0].m_Position.x, info.verts[1].m_Position.x );
			info.verts[0].m_TexCoord.x = Lerp( frac, info.verts[0].m_TexCoord.x, info.verts[1].m_TexCoord.x );
			surface()->DrawRenderCharFromInfo( info );
		}

		// Paint divider

		surface()->DrawSetTextPos( x, y );

		// Paint from 0.5 to frac of next char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			vgui::Vertex_t save[2];
			
			save[0] = info.verts[0];
			save[1] = info.verts[1];

			info.verts[0].m_Position.x = Lerp( 0.5f, save[0].m_Position.x, save[1].m_Position.x );
			info.verts[0].m_TexCoord.x = Lerp( 0.5f, save[0].m_TexCoord.x, save[1].m_TexCoord.x );

			info.verts[1].m_Position.x = Lerp( frac, save[0].m_Position.x, save[1].m_Position.x );
			//info.verts[1].m_TexCoord.x = Lerp( frac, save[0].m_TexCoord.x, save[1].m_TexCoord.x );
			
			surface()->DrawRenderCharFromInfo( info );
		}
	}

	x += ( abcA + abcB + abcC );
	surface()->DrawSetTextPos( x, y );
}

void CHudNumeric::PaintRotatedCharacterSpeedomter( int x, int y, vgui::HFont& font, int prevchar, int nextchar, float frac )
{
	frac = 3 * frac * frac - 2 * frac * frac * frac;

	int abcA, abcB, abcC;
	surface()->GetCharABCwide( font, prevchar, abcA, abcB, abcC );
	// int fontTall = surface()->GetFontTall( font );

	CharRenderInfo info;
	if ( frac <= 0.0f )
	{
		surface()->DrawSetTextPos( x, y );

		// Paint the whole previous char
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			surface()->DrawRenderCharFromInfo( info );
		}
	}
	else if ( frac >= 1.0f )
	{
		surface()->DrawSetTextPos( x, y );

		// Paint the whole previous char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			surface()->DrawRenderCharFromInfo( info );
		}
	}
	else
	{
		// Draw part of previous and part of next
		surface()->DrawSetTextPos( x, y );

		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[1].m_Position.y = Lerp( frac, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[1].m_TexCoord.y = Lerp( frac, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}

		// Paint divider

		surface()->DrawSetTextPos( x, y );

		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[0].m_Position.y = Lerp( frac, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[0].m_TexCoord.y = Lerp( frac, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}
	}

	x += ( abcA + abcB + abcC );
	surface()->DrawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			font - 
//			prevchar - 
//			nextchar - 
//			frac - 
//-----------------------------------------------------------------------------
void CHudNumeric::PaintRotatedCharacter( int x, int y, HFont& font, int prevchar, int nextchar, float frac )
{
	frac = SimpleSpline( frac );

	int abcA[2], abcB[2], abcC[2];
	surface()->GetCharABCwide( font, prevchar, abcA[0], abcB[0], abcC[0] );
	surface()->GetCharABCwide( font, nextchar, abcA[1], abcB[1], abcC[1] );

	int w0 = abcA[0] + abcB[0] + abcC[0];
	int w1 = abcA[1] + abcB[1] + abcC[1];

	int cellwidth = MAX( w0, w1 );

	int fontTall = surface()->GetFontTall( font );

	int dividery = y + clamp( Lerp( frac, 0, fontTall ), 2, fontTall - 2 );

	int xprev = x;
	int xnext = x;

	int diff = abs( abcB[0] - abcB[1] ) * 0.5f;

	if ( diff != 0 )
	{
		// Prev is wider than next, push next right a bit
		if ( w0 > w1 )
		{
			xnext += diff;
		}
		else
		{
			xprev += diff;
		}
	}

	CharRenderInfo info;
	if ( frac < 0.5f )
	{
		surface()->DrawSetTextPos( xprev, y );

		// Paint the bottom half of the prevchar still
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			// Shift tex coord and y position half way down glyph
			info.verts[0].m_Position.y = Lerp( 0.5f, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[0].m_TexCoord.y = Lerp( 0.5f, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}

		surface()->DrawSetTextPos( xnext, y );

		// Paint up to frac from top of prev char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[1].m_Position.y = Lerp( frac, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[1].m_TexCoord.y = Lerp( frac, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}

		// Paint divider
		if ( frac > 0.0f )
		{
			surface()->DrawSetColor( m_CharBgBorder );
			surface()->DrawLine( x - abcA[0] + 1, dividery, x + abcB[0] + abcC[0] - 1, dividery );
		}

		surface()->DrawSetTextPos( xprev, y );

		// Paint from frac to 0.5 of nextchar on the top
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info )  )
		{
			// Shift tex coord and y position half way up glyph
			vgui::Vertex_t save[2];
			
			save[0] = info.verts[0];
			save[1] = info.verts[1];

			info.verts[0].m_Position.y = Lerp( frac, save[0].m_Position.y, save[1].m_Position.y );
			//info.verts[0].m_TexCoord.y = Lerp( frac, save[0].m_TexCoord.y, save[1].m_TexCoord.y );

			info.verts[1].m_Position.y = Lerp( 0.5f, save[0].m_Position.y, save[1].m_Position.y );
			info.verts[1].m_TexCoord.y = Lerp( 0.5f, save[0].m_TexCoord.y, save[1].m_TexCoord.y );
			
			surface()->DrawRenderCharFromInfo( info );
		}
	}
	else if ( frac >= 0.5f )
	{
		surface()->DrawSetTextPos( xnext, y );

		// Paint entire top half of next char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position half way down glyph
			info.verts[1].m_Position.y = Lerp( 0.5f, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[1].m_TexCoord.y = Lerp( 0.5f, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}

		surface()->DrawSetTextPos( xprev, y );

		// paint a bit of the previous char at the bottom
		if ( surface()->DrawGetUnicodeCharRenderInfo( prevchar, info ) )
		{
			// Shift tex coord and y position part way up glyph
			info.verts[0].m_Position.y = Lerp( frac, info.verts[0].m_Position.y, info.verts[1].m_Position.y );
			info.verts[0].m_TexCoord.y = Lerp( frac, info.verts[0].m_TexCoord.y, info.verts[1].m_TexCoord.y );
			surface()->DrawRenderCharFromInfo( info );
		}

		// Paint divider
		if ( frac < 1.0f )
		{
			surface()->DrawSetColor( m_CharBgBorder );
			surface()->DrawLine( x - abcA[0] + 1, dividery, x + abcB[0] + abcC[0] - 1, dividery );
		}

		surface()->DrawSetTextPos( xnext, y );

		// Paint from 0.5 to frac of next char
		if ( surface()->DrawGetUnicodeCharRenderInfo( nextchar, info ) )
		{
			// Shift tex coord and y position half way up glyph
			vgui::Vertex_t save[2];
			
			save[0] = info.verts[0];
			save[1] = info.verts[1];

			info.verts[0].m_Position.y = Lerp( 0.5f, save[0].m_Position.y, save[1].m_Position.y );
			info.verts[0].m_TexCoord.y = Lerp( 0.5f, save[0].m_TexCoord.y, save[1].m_TexCoord.y );

			info.verts[1].m_Position.y = Lerp( frac, save[0].m_Position.y, save[1].m_Position.y );
			//info.verts[1].m_TexCoord.y = Lerp( frac, save[0].m_TexCoord.y, save[1].m_TexCoord.y );
			
			surface()->DrawRenderCharFromInfo( info );
		}
	}

	x += cellwidth;
	surface()->DrawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *prev - 
//			*next - 
// Output : float
//-----------------------------------------------------------------------------
float CHudNumeric::MaxCharacterDiff( const char *prev, const char *next )
{
	float maxdiff = 0.0f;

	int textlen = Q_strlen( next );
	int prevlen = Q_strlen( prev );
	int ch;
	for ( ch = 0; ch < textlen; ch++ )
	{
		int charfromend = textlen - ch;
		char c = next[ ch ];
		char prevc = prev[ MAX( 0, prevlen - charfromend ) ];
		if ( prevc == 0 )
		{
			int tempindex = FindPrintableIndex( c );
			if ( tempindex != m_Printables.InvalidIndex() )
			{
				tempindex = MAX( 0, tempindex - 3 );
				prevc = m_Printables[ tempindex ];
			}
			else
			{
				prevc = c;
			}
		}

		float diff = (float)fabs( FindPrintableIndex( c ) - FindPrintableIndex( prevc ) );

		if ( diff > maxdiff )
		{
			maxdiff = diff;
		}
	}

	float maxdelta = maxdiff;

	if ( m_nRotaryMaxDelta != 0 )
	{
		maxdelta = MIN( (float)m_nRotaryMaxDelta, maxdelta );
	}

	return maxdelta;
}

int CHudNumeric::ComputePixelsRequired( vgui::HFont& font, const char *text, int textlen )
{
	int pixels = 0;
	for ( int ch = 0; ch < textlen; ch++ )
	{
		char c = text[ ch ];
		pixels += surface()->GetCharacterWidth( font, c );
	}
	return pixels;
}

void CHudNumeric::DrawCharacterBackground( const char *text, int textlen, HFont& font, int x, int y )
{
	int abcA, abcB, abcC;
	int fontTall = surface()->GetFontTall( font );

	int ch;
	int curx = x - ComputePixelsRequired( font, text, textlen );
	for ( ch = 0; ch < textlen; ch++ )
	{
		char c = text[ ch ];

		surface()->GetCharABCwide( font, c, abcA, abcB, abcC );

		curx += abcA;

		surface()->DrawSetColor( m_CharBg );
		surface()->DrawFilledRect( curx - abcA, y + 2, curx + abcB + abcC, y + fontTall - 2 );

		if ( m_bDrawCharacterBackgroundBorder )
		{
			surface()->DrawSetColor( m_CharBgBorder );
			surface()->DrawOutlinedRect( curx - abcA, y + 2, curx + abcB + abcC, y + fontTall - 2  );
		}

		curx += ( abcB + abcC );
	}
}

void CHudNumeric::DrawCharacterForeground( const char *text, int textlen, HFont& font, int x, int y )
{
	if ( m_nRotaryEffect == ROTARY_EFFECT_NONE ||
		m_nRotaryEffect == ROTARY_EFFECT_SPEEDOMETER )	
		return;

	int abcA, abcB, abcC;
	int fontTall = surface()->GetFontTall( font );

	int ch;
	int curx = x - ComputePixelsRequired( font, text, textlen );
	for ( ch = 0; ch < textlen; ch++ )
	{
		char c = text[ ch ];

		surface()->GetCharABCwide( font, c, abcA, abcB, abcC );

		curx += abcA;

		switch ( m_nRotaryEffect )
		{
		default:
		case ROTARY_EFFECT_VERTICAL_ALARM:
			{
				surface()->DrawSetColor( m_CharFg );
				surface()->DrawLine( curx - abcA + 1, y + fontTall / 2, curx + abcB + abcC - 1, y + fontTall / 2 );
			}
			break;
		case ROTARY_EFFECT_HORIZONTAL_ALARM:
			{
				surface()->DrawSetColor( m_CharFg );
				int avex = curx + ( - abcA + abcB + abcC ) / 2;
				surface()->DrawLine( avex, y + 2, avex, y + fontTall - 2 );
			}
			break;
		}

		curx += ( abcB + abcC );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
//			textlen - 
//			font - 
//			x - 
//			y - 
//-----------------------------------------------------------------------------
void CHudNumeric::PaintStringRotary( float t, const char *text, int textlen, HFont& font, int x, int y )
{
	surface()->DrawSetTextFont( font );
	int ch;

	int prevlen = Q_strlen( m_szLatchedValue );

	// Compute pixels actually required
	int pixels = 0;

	for ( ch = 0; ch < textlen; ch++ )
	{
		int charfromend = textlen - ch;
		char c = text[ ch ];
		char prevc = m_szLatchedValue[ MAX( 0, prevlen - charfromend ) ];
		if ( prevc == 0 )
		{
			int tempindex = FindPrintableIndex( c );
			if ( tempindex != m_Printables.InvalidIndex() )
			{
				tempindex = MAX( 0, tempindex - 3 );
				prevc = m_Printables[ tempindex ];
			}
			else
			{
				prevc = c;
			}
		}

		float diff = (float)fabs( FindPrintableIndex( c ) - FindPrintableIndex( prevc ) );

		if ( m_nRotaryMaxDelta != 0 )
		{
			diff = MIN( (float)m_nRotaryMaxDelta, diff );
		}

		float dt = diff / m_flActualCharactersPerSecond;

		float frac = 1.0f;
		if ( dt > 0.0f )
		{
			frac = t / MIN( dt, m_flRotaryTime );
		}
		frac = clamp( frac, 0.0f, 1.0f );

		float s;
		char chstart, chend;
		GetRotatedChar( frac, prevc, c, chstart, chend, s );

		int w0 = surface()->GetCharacterWidth( font, chstart );
		int w1 = surface()->GetCharacterWidth( font, chend );

		pixels += MAX( w0, w1 );
	}

	surface()->DrawSetTextPos( x - pixels, y );

	// Now render
	for ( ch = 0; ch < textlen; ch++ )
	{
		int charfromend = textlen - ch;
		char c = text[ ch ];
		char prevc = m_szLatchedValue[ MAX( 0, prevlen - charfromend ) ];
		if ( prevc == 0 )
		{
			int tempindex = FindPrintableIndex( c );
			if ( tempindex != m_Printables.InvalidIndex() )
			{
				tempindex = MAX( 0, tempindex - 3 );
				prevc = m_Printables[ tempindex ];
			}
			else
			{
				prevc = c;
			}
		}

		float diff = (float)fabs( FindPrintableIndex( c ) - FindPrintableIndex( prevc ) );

		if ( m_nRotaryMaxDelta != 0 )
		{
			diff = MIN( (float)m_nRotaryMaxDelta, diff );
		}

		float dt = diff / m_flActualCharactersPerSecond;

		float frac = 1.0f;
		if ( dt > 0.0f )
		{
			frac = t / MIN( dt, m_flRotaryTime );
		}
		frac = clamp( frac, 0.0f, 1.0f );

		float s;
		char chstart, chend;
		GetRotatedChar( frac, prevc, c, chstart, chend, s );
		int outx, outy;
		surface()->DrawGetTextPos( outx, outy );
		switch ( m_nRotaryEffect )
		{
		default:
		case ROTARY_EFFECT_VERTICAL_ALARM:
			{
				PaintRotatedCharacter( outx, outy, font, chstart, chend, s );
			}
			break;
		case ROTARY_EFFECT_HORIZONTAL_ALARM:
			{
				PaintRotatedCharacterHoriz( outx, outy, font, chstart, chend, s );
			}
			break;
		case ROTARY_EFFECT_SPEEDOMETER:
			{
				PaintRotatedCharacterSpeedomter( outx, outy, font, chstart, chend, s );
			}
			break;
		}

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
//			textlen - 
//			font - 
//			x - 
//			y - 
//-----------------------------------------------------------------------------
void CHudNumeric::PaintString( const char *text, int textlen, HFont& font, int x, int y )
{
	if ( m_nRotaryEffect != ROTARY_EFFECT_NONE && 
		IsRotating() && 
		m_flRotaryTime > 0.0f )
	{
		float rotation_time = gpGlobals->curtime - m_flRotaryStartTime;

		PaintStringRotary( rotation_time, text, textlen, font, x, y );
		
		return;
	}
	PaintStringNormal( text, textlen, font, x, y );
}

void CHudNumeric::PaintStringNormal( const char *text, int textlen, vgui::HFont& font, int x, int y )
{
	surface()->DrawSetTextFont( font );
	int ch;
	surface()->DrawSetTextPos( x - ComputePixelsRequired( font, text, textlen ), y );

	for ( ch = 0; ch < textlen; ch++ )
	{
		char c = text[ ch ];
		surface()->DrawUnicodeChar( c );
	}
}

void CHudNumeric::PaintBackground( void )
{
	char value[ MAX_VALUE_LENGTH ];

	if ( !GetValue( value, sizeof( value ) ) )
		return;

	float alpha = m_flAlphaOverride / 255.0f;

	Color boxColor = GetBoxColor();
	boxColor[3] *= alpha;

	SetBgColor( boxColor );

	BaseClass::PaintBackground();

	if ( !m_bUseIcon )
		return;

	CHudTexture *tex = m_hIcon;
	if ( !tex )
		return;

	Color iconColor = m_IconColor;
	iconColor[3] *= alpha;

	tex->DrawSelf( m_flIconXPos, m_flIconYPos, m_flIconWidth, m_flIconHeight, iconColor );
}

void CHudNumeric::PaintValue( const char *value, int textlen, int wide, int tall, Color& clr )
{
	float alpha = m_flAlphaOverride / 255.0f;

	Color boxColor = GetBoxColor();
	boxColor[3] *= alpha;

	SetBgColor( boxColor );

	Color useColor = clr;
	useColor[3] *= alpha;

	surface()->DrawSetTextColor( useColor );

	int x = wide - label_xpos_right;
	int y = label_ypos;

	if ( m_bDrawLabel ) 
	{
		// Label
		PaintStringNormal( GetLabelText(), Q_strlen( GetLabelText() ), m_hLabelFont, x, y );
	}

	x = wide - value_xpos_right;
	y = value_ypos;

	if ( m_nRotaryEffect != ROTARY_EFFECT_NONE && 
		(bool)m_bDrawCharacterBackground )
	{
		DrawCharacterBackground( value, textlen, m_hTextFont, x, y );
	}

	PaintString( value, textlen, m_hTextFont, x, y );

	if ( m_nRotaryEffect != ROTARY_EFFECT_NONE && 
		(bool)m_bDrawCharacterForeground )
	{
		DrawCharacterForeground( value, textlen, m_hTextFont, x, y );
	}

	// draw the overbright blur
	for (float fl = m_flBlur; fl > 0.0f; fl -= 1.0f)
	{
		if (fl >= 1.0f)
		{
			PaintString( value, textlen, m_hTextFontPulsing, x, y );
		}
		else
		{
			// draw a percentage of the last one
			Color col = useColor;
			col[3] *= fl;
			surface()->DrawSetTextColor(col);
			PaintString( value, textlen, m_hTextFontPulsing, x, y );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Color
//-----------------------------------------------------------------------------
Color CHudNumeric::GetColor()
{
	return m_TextColor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Color
//-----------------------------------------------------------------------------
Color CHudNumeric::GetBoxColor()
{
	return m_BoxColor;
}

//-----------------------------------------------------------------------------
// Purpose: Redraw
//-----------------------------------------------------------------------------
void CHudNumeric::Paint( void )
{
	char value[ MAX_VALUE_LENGTH ];

	if ( !GetValue( value, sizeof( value ) ) )
		return;

	int w, h;
	GetSize( w, h );

	bool dopulse = ( Q_strcmp( value, m_szPreviousValue ) != 0 ) || ( m_bPulseForced );
	bool increment = false;
	if ( dopulse )
	{
		if ( atof( m_szPreviousValue ) <= atof( value ) )
		{
			increment = true;
		}

		Q_strncpy( m_szLatchedValue, m_szPreviousValue, sizeof( m_szLatchedValue ) );
		m_nTextLen = Q_strlen( value );
		
		m_flRotaryStartTime = gpGlobals->curtime;
		float maxdiff = MaxCharacterDiff( m_szLatchedValue, value );
		float timerequired = maxdiff / m_flDesiredCharactersPerSecond;
		m_flActualCharactersPerSecond = (float)m_flDesiredCharactersPerSecond;
		m_flRotaryTime = MIN( m_flRotaryTimeMax, timerequired );
		if ( m_flRotaryTime < timerequired )
		{
			// Speed up rotation since we're moving so far
			m_flActualCharactersPerSecond =  ( timerequired / m_flRotaryTime ) * m_flDesiredCharactersPerSecond;
		}
	}

	Q_strncpy( m_szPreviousValue, value, sizeof( m_szPreviousValue ) );

	Color clr = GetColor();
	PaintValue( value, m_nTextLen, w, h, clr );

	if ( dopulse && m_bSendPulses )
	{
		// Start with a short pulse
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( GetPulseEvent( increment ) );

		m_bPulseForced = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Derived class has forced a pulse
//-----------------------------------------------------------------------------
void CHudNumeric::ForcePulse( void )
{
	m_bPulseForced = true;
}
