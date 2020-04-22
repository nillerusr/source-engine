//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox 360 support for TrueType Fonts. The only cuurent solution is to use XUI
// to mount the TTF, and rasterize glyph into a render target. XUI does not support
// rasterization directly to a system memory region.
//
//=====================================================================================//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "FontEffects.h"
#include "vgui_surfacelib/Win32Font.h"
#include "vgui_surfacelib/FontManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool s_bSupportsUnicode = true;

//-----------------------------------------------------------------------------
// Determine possible style from parameters.
//-----------------------------------------------------------------------------
int GetStyleFromParameters( int iFlags, int iWeight )
{
	// Available xbox TTF styles are very restricted.
	int style = XUI_FONT_STYLE_NORMAL;
	if ( iFlags & vgui::ISurface::FONTFLAG_ITALIC )
		style |= XUI_FONT_STYLE_ITALIC;
	if ( iFlags & vgui::ISurface::FONTFLAG_UNDERLINE )
		style |= XUI_FONT_STYLE_UNDERLINE;
	if ( iWeight > 400 )
		style |= XUI_FONT_STYLE_BOLD;
	return style;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWin32Font::CWin32Font()
{
	m_szName = UTL_INVAL_SYMBOL;
	m_iTall = 0;
	m_iWeight = 0;
	m_iHeight = 0;
	m_iAscent = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_hFont = NULL;
	m_hDC = NULL;
	m_bAntiAliased = false;
	m_iBlur = 0;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	m_rgiBitmapSize[0] = 0;
	m_rgiBitmapSize[1] = 0;

	Q_memset( m_ABCWidthsCache, 0, sizeof( m_ABCWidthsCache ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWin32Font::~CWin32Font()
{
	CloseResource();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the font.
//-----------------------------------------------------------------------------
bool CWin32Font::Create( const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	// setup font properties
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = (flags & vgui::ISurface::FONTFLAG_ANTIALIAS) ? 1 : 0;
	m_iDropShadowOffset = (flags & vgui::ISurface::FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & vgui::ISurface::FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = (flags & vgui::ISurface::FONTFLAG_ROTARY) ? 1 : 0;
	m_bAdditive = (flags & vgui::ISurface::FONTFLAG_ADDITIVE) ? 1 : 0;

	int style = GetStyleFromParameters( flags, weight );

	// must support > 128, there are characters in this range in the custom fonts
	COMPILE_TIME_ASSERT( ABCWIDTHS_CACHE_SIZE == 256 );

	XUIFontMetrics fontMetrics;
	XUICharMetrics charMetrics[256];

	// many redundant requests are made that are actually the same font metrics
	// find it in the metric cache first based on the true specific keys
	if ( !FontManager().GetCachedXUIMetrics( windowsFontName, tall, style, &fontMetrics, charMetrics ) )
	{
		m_hFont = FontManager().MaterialSystem()->OpenTrueTypeFont( windowsFontName, tall, style );
		if ( !m_hFont )
		{
			return false;
		}

		// getting the metrics is an expensive i/o operation, cache results
		FontManager().MaterialSystem()->GetTrueTypeFontMetrics( m_hFont, &fontMetrics, charMetrics );
		FontManager().SetCachedXUIMetrics( windowsFontName, tall, style, &fontMetrics, charMetrics );
	}

	m_szName = windowsFontName;

	m_iHeight = fontMetrics.fMaxHeight + m_iDropShadowOffset + 2 * m_iOutlineSize;
	m_iMaxCharWidth = fontMetrics.fMaxWidth;
	m_iAscent = fontMetrics.fMaxAscent;

	// determine cell bounds
	m_rgiBitmapSize[0] = m_iMaxCharWidth + m_iOutlineSize * 2;
	m_rgiBitmapSize[1] = m_iHeight;

	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	Assert( ABCWIDTHS_CACHE_SIZE <= 256 );
	Q_memset( m_ABCWidthsCache, 0, sizeof( m_ABCWidthsCache ) );

	for ( int i = 1; i < ABCWIDTHS_CACHE_SIZE; i++ )
	{
		int a,b,c;

		// Determine real a,b,c mapping from XUI Character Metrics
		a = charMetrics[i].fMinX - 1; // Add one column of padding to make up for font rendering blurring into left column (and adjust in b)
		b = charMetrics[i].fMaxX - charMetrics[i].fMinX + 1;
		c = charMetrics[i].fAdvance - charMetrics[i].fMaxX; // NOTE: We probably should add a column here, but it's rarely needed in our current fonts so we're opting to save memory instead

		// Widen for blur, outline, and shadow. Need to widen b and reduce a and c.
		m_ABCWidthsCache[i].a = a - m_iBlur - m_iOutlineSize;
		m_ABCWidthsCache[i].b = b + ( ( m_iBlur + m_iOutlineSize ) * 2 ) + m_iDropShadowOffset;
		m_ABCWidthsCache[i].c = c - m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: generates texture data (written into appropriate font page subrects) for multiple chars
//-----------------------------------------------------------------------------
void CWin32Font::GetCharsRGBA( newChar_t *newChars, int numNewChars, unsigned char *pRGBA )
{
	if ( !m_hFont )
	{
		// demand request for font glyph, re-create font
		int style = GetStyleFromParameters( m_iFlags, m_iWeight );
		m_hFont = FontManager().MaterialSystem()->OpenTrueTypeFont( GetName(), m_iTall, style );
	}

	wchar_t	*pWch		= (wchar_t *)_alloca( numNewChars*sizeof(wchar_t) );
	int	*pOffsetX		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pOffsetY		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pWidth			= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pHeight		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pRGBAOffset	= (int *)_alloca( numNewChars*sizeof(int) );
	for ( int i = 0; i < numNewChars; i++ )
	{
		int a, c, wide;
		GetCharABCWidths( newChars[i].wch, a, wide, c );
		pWch[i]			= newChars[i].wch;
		pOffsetX[i]		= -a;
		pOffsetY[i]		= 0;
		pWidth[i]		= newChars[i].fontWide;
		pHeight[i]		= newChars[i].fontTall;
		pRGBAOffset[i]	= newChars[i].offset;
	}
	if ( !FontManager().MaterialSystem()->GetTrueTypeGlyphs( m_hFont, numNewChars, pWch, pOffsetX, pOffsetY, pWidth, pHeight, pRGBA, pRGBAOffset ) )
	{
		// failure
		return;
	}

	for ( int i = 0; i < numNewChars; i++ )
	{
		// apply requested effects in specified order
		unsigned char *pCharRGBA = pRGBA + newChars[i].offset;
		ApplyDropShadowToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iDropShadowOffset );
		ApplyOutlineToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iOutlineSize );
		ApplyGaussianBlurToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iBlur );
		ApplyScanlineEffectToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iScanLines );
		ApplyRotaryEffectToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_bRotary );
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture at specified rect
//-----------------------------------------------------------------------------
void CWin32Font::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *pRGBA )
{
	newChar_t newChar;
	newChar.wch = ch;
	newChar.fontWide = rgbaWide;
	newChar.fontTall = rgbaTall;
	newChar.offset = 0;
	GetCharsRGBA( &newChar, 1, pRGBA );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CWin32Font::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// do an true comparison that accounts for non-supported behaviors that gets remapped
	// avoids creating fonts that are graphically equivalent, though specified differently
	if ( !stricmp( windowsFontName, m_szName.String() ) &&
		m_iTall == tall && 
		m_iBlur == blur &&
		m_iScanLines == scanlines )
	{
		// only these flags affect the font glyphs
		int validFlags = vgui::ISurface::FONTFLAG_DROPSHADOW | 
						vgui::ISurface::FONTFLAG_OUTLINE | 
						vgui::ISurface::FONTFLAG_ROTARY |
						vgui::ISurface::FONTFLAG_ITALIC |
						vgui::ISurface::FONTFLAG_UNDERLINE;
		if ( ( m_iFlags & validFlags ) == ( flags & validFlags ) )
		{
			if ( GetStyleFromParameters( m_iFlags, m_iWeight ) == GetStyleFromParameters( flags, weight ) )
			{
				// the font is equivalent
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool CWin32Font::IsValid()
{
	if ( m_szName != UTL_INVAL_SYMBOL )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: set the font to be the one to currently draw with in the gdi
//-----------------------------------------------------------------------------
void CWin32Font::SetAsActiveFont( HDC hdc )
{
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CWin32Font::GetCharABCWidths( int ch, int &a, int &b, int &c )
{
	Assert( IsValid() );

	if ( ch < ABCWIDTHS_CACHE_SIZE )
	{
		// use the cache entry
		a = m_ABCWidthsCache[ch].a;
		b = m_ABCWidthsCache[ch].b;
		c = m_ABCWidthsCache[ch].c;
	}
	else
	{
		// cannot support getting character metrics outside of the font initialization
		DevMsg( "CWin32Font: Cannot resolve character %d in font %s\n", ch, m_szName.String() );
		Assert( 0 );

		a = 0;
		b = 0;
		c = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int CWin32Font::GetHeight()
{
	Assert( IsValid() );
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int CWin32Font::GetAscent()
{
	Assert( IsValid() );
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int CWin32Font::GetMaxCharWidth()
{
	Assert( IsValid() );
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int CWin32Font::GetFlags()
{
	Assert( IsValid() );
	return m_iFlags;
}

void CWin32Font::CloseResource()
{
	if ( !m_hFont )
	{
		return;
	}

	// many fonts are blindly precached by vgui and never used
	// save memory and don't hold font open, re-open if glyph actually requested used during draw
	FontManager().MaterialSystem()->CloseTrueTypeFont( m_hFont );
	m_hFont = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the kerned size of a char, for win32 just pass thru for now
//-----------------------------------------------------------------------------
void CWin32Font::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA )
{
	int a,b,c;
	GetCharABCWidths(ch, a, b, c );
	wide = ( a + b + c);
	abcA = a;
}
