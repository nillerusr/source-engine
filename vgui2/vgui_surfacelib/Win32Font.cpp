//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#pragma warning( disable : 4244 ) // conversion from 'double' to 'float', possible loss of data

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "vgui_surfacelib/Win32Font.h"
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>
#include "FontEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static OSVERSIONINFO s_OsVersionInfo;
static bool s_bOsVersionInitialized = false;
bool s_bSupportsUnicode = false;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWin32Font::CWin32Font() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc)
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
	m_hDIB = NULL;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	m_rgiBitmapSize[ 0 ] = m_rgiBitmapSize[ 1 ] = 0;

#if defined( _X360 )
	Q_memset( m_ABCWidthsCache, 0, sizeof( m_ABCWidthsCache ) );
#endif

	m_ExtendedABCWidthsCache.EnsureCapacity( 128 );

	if ( !s_bOsVersionInitialized )
	{
		// get the operating system version
		s_bOsVersionInitialized = true;
		memset(&s_OsVersionInfo, 0, sizeof(s_OsVersionInfo));
		s_OsVersionInfo.dwOSVersionInfoSize = sizeof(s_OsVersionInfo);
		GetVersionEx(&s_OsVersionInfo);

		if (s_OsVersionInfo.dwMajorVersion >= 5)
		{
			s_bSupportsUnicode = true;
		}
		else
		{
			s_bSupportsUnicode = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWin32Font::~CWin32Font()
{
	if ( m_hFont )
		::DeleteObject( m_hFont );
	if ( m_hDC )
		::DeleteDC( m_hDC );
	if ( m_hDIB )
		::DeleteObject( m_hDIB );
}

//-----------------------------------------------------------------------------
// Purpose: Font iteration callback function
//			used to determine whether or not a font exists on the system
//-----------------------------------------------------------------------------
extern bool g_bFontFound = false;
int CALLBACK FontEnumProc( 
	const LOGFONT *lpelfe,		// logical-font data
	const TEXTMETRIC *lpntme,	// physical-font data
	DWORD FontType,				// type of font
	LPARAM lParam )				// application-defined data
{
	g_bFontFound = true;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: creates the font from windows.  returns false if font does not exist in the OS.
//-----------------------------------------------------------------------------
bool CWin32Font::Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = (flags & vgui::ISurface::FONTFLAG_ANTIALIAS) ? 1 : 0;
	m_bUnderlined = flags & vgui::ISurface::FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & vgui::ISurface::FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & vgui::ISurface::FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = (flags & vgui::ISurface::FONTFLAG_ROTARY) ? 1 : 0;
	m_bAdditive = (flags & vgui::ISurface::FONTFLAG_ADDITIVE) ? 1 : 0;

	int charset = (flags & vgui::ISurface::FONTFLAG_SYMBOL) ? SYMBOL_CHARSET : ANSI_CHARSET;

	// hack for japanese win98 support
	if ( !stricmp( windowsFontName, "win98japanese" ) )
	{
		// use any font that contains the japanese charset
		charset = SHIFTJIS_CHARSET;
		m_szName = "Tahoma";
	}

	// create our windows device context
	m_hDC = ::CreateCompatibleDC(NULL);
	Assert( m_hDC );

	// see if the font exists on the system
	LOGFONT logfont;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfPitchAndFamily = 0;
	strcpy(logfont.lfFaceName, m_szName.String());
	g_bFontFound = false;
	::EnumFontFamiliesEx(m_hDC, &logfont, &FontEnumProc, 0, 0);
	if (!g_bFontFound)
	{
		// needs to go to a fallback
		m_szName = UTL_INVAL_SYMBOL;
		return false;
	}

	m_hFont = ::CreateFontA(tall, 0, 0, 0, 
								m_iWeight, 
								flags & vgui::ISurface::FONTFLAG_ITALIC, 
								flags & vgui::ISurface::FONTFLAG_UNDERLINE, 
								flags & vgui::ISurface::FONTFLAG_STRIKEOUT, 
								charset, 
								OUT_DEFAULT_PRECIS, 
								CLIP_DEFAULT_PRECIS, 
								m_bAntiAliased ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, 
								DEFAULT_PITCH | FF_DONTCARE, 
								windowsFontName);
	if (!m_hFont)
	{
		Error("Couldn't create windows font '%s'\n", windowsFontName);
		m_szName = UTL_INVAL_SYMBOL;
		return false;
	}

	// set as the active font
	::SetMapMode(m_hDC, MM_TEXT);
	::SelectObject(m_hDC, m_hFont);
	::SetTextAlign(m_hDC, TA_LEFT | TA_TOP | TA_UPDATECP);

	// get info about the font
	::TEXTMETRIC tm;
	memset( &tm, 0, sizeof( tm ) );
	if ( !GetTextMetrics(m_hDC, &tm) )
	{
		m_szName = UTL_INVAL_SYMBOL;
		return false;
	}

	m_iHeight = tm.tmHeight + m_iDropShadowOffset + 2 * m_iOutlineSize;
	m_iMaxCharWidth = tm.tmMaxCharWidth;
	m_iAscent = tm.tmAscent;

	// code for rendering to a bitmap
	m_rgiBitmapSize[0] = tm.tmMaxCharWidth + m_iOutlineSize * 2;
	m_rgiBitmapSize[1] = tm.tmHeight + m_iDropShadowOffset + m_iOutlineSize * 2;

	::BITMAPINFOHEADER header;
	memset(&header, 0, sizeof(header));
	header.biSize = sizeof(header);
	header.biWidth = m_rgiBitmapSize[0];
	header.biHeight = -m_rgiBitmapSize[1];
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;

	m_hDIB = ::CreateDIBSection(m_hDC, (BITMAPINFO*)&header, DIB_RGB_COLORS, (void**)(&m_pBuf), NULL, 0);
	::SelectObject(m_hDC, m_hDIB);

#if defined( _X360 )
	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	memset(m_ABCWidthsCache, 0, sizeof(m_ABCWidthsCache));
	ABC abc[ABCWIDTHS_CACHE_SIZE];
	Assert(ABCWIDTHS_CACHE_SIZE <= 256);
	if (::GetCharABCWidthsW(m_hDC, 0, ABCWIDTHS_CACHE_SIZE - 1, &abc[0]) || ::GetCharABCWidthsA(m_hDC, 0, ABCWIDTHS_CACHE_SIZE - 1, &abc[0]))
	{	
		// copy out into our formated structure
		for (int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
		{
			m_ABCWidthsCache[i].a = abc[i].abcA - m_iBlur - m_iOutlineSize;
			m_ABCWidthsCache[i].b = abc[i].abcB + ((m_iBlur + m_iOutlineSize) * 2) + m_iDropShadowOffset;
			m_ABCWidthsCache[i].c = abc[i].abcC - m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
		}
	}
	else
	{
		Warning("GetCharABCWidths() failed for windows font '%s'\n", windowsFontName);

		// since that failed, it must be fixed width, zero everything so a and c will be zeros, then
		// fill b with the value from TEXTMETRIC
		for (int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
		{
			// fallback to old method, no underhangs/overhangs (a/c)
			SIZE size;
			char mbcs[6] = { 0 };
			wchar_t wch = (wchar_t)i;
			::WideCharToMultiByte(CP_ACP, 0, &wch, 1, mbcs, sizeof(mbcs), NULL, NULL);
			if (::GetTextExtentPoint32(m_hDC, mbcs, strlen(mbcs), &size))
			{
				m_ABCWidthsCache[i].b = size.cx;
			}
			else
			{
				// failed to get width, just use the average width
				m_ABCWidthsCache[i].b = (char)tm.tmAveCharWidth;
			}
		}
	}
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void CWin32Font::GetCharRGBA(wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *rgba)
{
	int a, b, c;
	GetCharABCWidths(ch, a, b, c);

	// set us up to render into our dib
	::SelectObject(m_hDC, m_hFont);

	int wide = b;
	if ( m_bUnderlined )
	{
		wide += ( a + c );
	}

	int tall = m_iHeight;
	GLYPHMETRICS glyphMetrics;
	MAT2 mat2 = { { 0, 1}, { 0, 0}, { 0, 0}, { 0, 1}};
	int bytesNeeded = 0;

	bool bShouldAntialias = m_bAntiAliased;
	// filter out 
	if ( ch > 0x00FF && !(m_iFlags & vgui::ISurface::FONTFLAG_CUSTOM) )
	{
		bShouldAntialias = false;
	}
	if ( !s_bSupportsUnicode )
	{
		// win98 hack, don't antialias some characters that ::GetGlyphOutline() produces bad results for
		if (ch == 'I' || ch == '1')
		{
			bShouldAntialias = false;
		}

		// don't antialias big fonts at all (since win98 often produces bad results)
		if (m_iHeight >= 13)
		{
			bShouldAntialias = false;
		}
	}


	// only antialias latin characters, since it essentially always fails for asian characters
	if (bShouldAntialias)
	{
		// try and get the glyph directly
		::SelectObject(m_hDC, m_hFont);
		bytesNeeded = ::GetGlyphOutline(m_hDC, ch, GGO_GRAY8_BITMAP, &glyphMetrics, 0, NULL, &mat2);
	}

	if (bytesNeeded > 0)
	{
		// take it
		unsigned char *lpbuf = (unsigned char *)_alloca(bytesNeeded);
		::GetGlyphOutline(m_hDC, ch, GGO_GRAY8_BITMAP, &glyphMetrics, bytesNeeded, lpbuf, &mat2);

		// rows are on DWORD boundaries
		wide = glyphMetrics.gmBlackBoxX;
		while (wide % 4 != 0)
		{
			wide++;
		}

		// see where we should start rendering
		int pushDown = m_iAscent - glyphMetrics.gmptGlyphOrigin.y;

		// set where we start copying from
		int xstart = 0;

		// don't copy the first set of pixels if the antialiased bmp is bigger than the char width
		if ((int)glyphMetrics.gmBlackBoxX >= b + 2)
		{
			xstart = (glyphMetrics.gmBlackBoxX - b) / 2;
		}

		// iterate through copying the generated dib into the texture
		for (unsigned int j = 0; j < glyphMetrics.gmBlackBoxY; j++)
		{
			for (unsigned int i = xstart; i < glyphMetrics.gmBlackBoxX; i++)
			{
				int x = i - xstart + m_iBlur + m_iOutlineSize;
				int y = j + pushDown;
				if ((x < rgbaWide) && (y < rgbaTall))
				{
					unsigned char grayscale = lpbuf[(j*wide+i)];

					float r, g, b, a;
					if (grayscale)
					{
						r = g = b = 1.0f;
						a = (grayscale + 0) / 64.0f;
						if (a > 1.0f) a = 1.0f;
					}
					else
					{
						r = g = b = a = 0.0f;
					}

					// Don't want anything drawn for tab characters.
					if (ch == '\t')
					{
						r = g = b = 0;
					}

					unsigned char *dst = &rgba[(y*rgbaWide+x)*4];
					dst[0] = (unsigned char)(r * 255.0f);
					dst[1] = (unsigned char)(g * 255.0f);
					dst[2] = (unsigned char)(b * 255.0f);
					dst[3] = (unsigned char)(a * 255.0f);
				}
			}
		}
	}
	else
	{
		// use render-to-bitmap to get our font texture
		::SetBkColor(m_hDC, RGB(0, 0, 0));
		::SetTextColor(m_hDC, RGB(255, 255, 255));
		::SetBkMode(m_hDC, OPAQUE);
		if ( m_bUnderlined )
		{
			::MoveToEx(m_hDC, 0, 0, NULL);
		}
		else
		{
			::MoveToEx(m_hDC, -a, 0, NULL);
		}

		// render the character
		wchar_t wch = (wchar_t)ch;
		
		if (s_bSupportsUnicode)
		{
			// clear the background first
			RECT rect = { 0, 0, wide, tall};
			::ExtTextOutW( m_hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL );

			// just use the unicode renderer
			::ExtTextOutW( m_hDC, 0, 0, 0, NULL, &wch, 1, NULL );
		}
		else
		{
			// clear the background first (it may not get done automatically in win98/ME
			RECT rect = { 0, 0, wide, tall};
			::ExtTextOut(m_hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

			// convert the character using the current codepage
			char mbcs[6] = { 0 };
			::WideCharToMultiByte(CP_ACP, 0, &wch, 1, mbcs, sizeof(mbcs), NULL, NULL);
			::ExtTextOutA(m_hDC, 0, 0, 0, NULL, mbcs, strlen(mbcs), NULL);
		}

		::SetBkMode(m_hDC, TRANSPARENT);

		if (wide > m_rgiBitmapSize[0])
		{
			wide = m_rgiBitmapSize[0];
		}
		if (tall > m_rgiBitmapSize[1])
		{
			tall = m_rgiBitmapSize[1];
		}

		// iterate through copying the generated dib into the texture
		for (int j = (int)m_iOutlineSize; j < tall - (int)m_iOutlineSize; j++ )
		{
			// only copy from within the dib, ignore the outline border we are artificially adding
			for (int i = (int)m_iOutlineSize; i < wide - (int)m_iDropShadowOffset - (int)m_iOutlineSize; i++)
			{
				if ((i < rgbaWide) && (j < rgbaTall))
				{
					unsigned char *src = &m_pBuf[(i + j*m_rgiBitmapSize[0])*4];
					unsigned char *dst = &rgba[(i + j*rgbaWide)*4];

					// Don't want anything drawn for tab characters.
					unsigned char r, g, b;
					if ( ch == '\t' )
					{
						r = g = b = 0;
					}
					else
					{
						r = src[0];
						g = src[1];
						b = src[2];
					}

					// generate alpha based on luminance conversion
					dst[0] = r;
					dst[1] = g;
					dst[2] = b;
					dst[3] = (unsigned char)((float)r * 0.34f + (float)g * 0.55f + (float)b * 0.11f);
				}
			}
		}

		// if we have a dropshadow, we need to clean off the bottom row of pixels
		// this is because of a bug in winME that writes noise to them, only on the first time the game is run after a reboot
		// the bottom row should guaranteed to be empty to fit the dropshadow
		if ( m_iDropShadowOffset )
		{
			unsigned char *dst = &rgba[((m_iHeight - 1) * rgbaWide) * 4];
			for (int i = 0; i < wide; i++)
			{
				dst[0] = 0;
				dst[1] = 0;
				dst[2] = 0;
				dst[3] = 0;
				dst += 4;
			}
		}
	}

	// apply requested effects in specified order
	ApplyDropShadowToTexture( rgbaWide, rgbaTall, rgba, m_iDropShadowOffset );
	ApplyOutlineToTexture( rgbaWide, rgbaTall, rgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, rgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, rgba, m_bRotary );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CWin32Font::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if ( !stricmp(windowsFontName, m_szName.String() ) 
		&& m_iTall == tall
		&& m_iWeight == weight
		&& m_iBlur == blur
		&& m_iFlags == flags)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool CWin32Font::IsValid()
{
	if ( m_szName.IsValid() && m_szName.String()[0] )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: set the font to be the one to currently draw with in the gdi
//-----------------------------------------------------------------------------
void CWin32Font::SetAsActiveFont(HDC hdc)
{
	Assert( IsValid() );
	::SelectObject( hdc, m_hFont );
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CWin32Font::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	Assert( IsValid() );
#if defined( _X360 )
	if (ch < ABCWIDTHS_CACHE_SIZE)
	{
		// use the cache entry
		a = m_ABCWidthsCache[ch].a;
		b = m_ABCWidthsCache[ch].b;
		c = m_ABCWidthsCache[ch].c;
	}
	else
#endif
	{

		// look for it in the cache
		abc_cache_t finder = { (wchar_t)ch };

		unsigned short i = m_ExtendedABCWidthsCache.Find(finder);
		if (m_ExtendedABCWidthsCache.IsValidIndex(i))
		{
			a = m_ExtendedABCWidthsCache[i].abc.a;
			b = m_ExtendedABCWidthsCache[i].abc.b;
			c = m_ExtendedABCWidthsCache[i].abc.c;
			return;
		}

		// not in the cache, get from windows (this call is a little slow)
		ABC abc;
		if (::GetCharABCWidthsW(m_hDC, ch, ch, &abc) || ::GetCharABCWidthsA(m_hDC, ch, ch, &abc))
		{
			a = abc.abcA;
			b = abc.abcB;
			c = abc.abcC;
		}
		else
		{
			// wide character version failed, try the old api function
			SIZE size;
			char mbcs[6] = { 0 };
			wchar_t wch = ch;
			::WideCharToMultiByte(CP_ACP, 0, &wch, 1, mbcs, sizeof(mbcs), NULL, NULL);
			if (::GetTextExtentPoint32(m_hDC, mbcs, strlen(mbcs), &size))
			{
				a = c = 0;
				b = size.cx;
			}
			else
			{
				// failed to get width, just use the max width
				a = c = 0;
				b = m_iMaxCharWidth;
			}
		}

		// add to the cache
		finder.abc.a = a - m_iBlur - m_iOutlineSize;
		finder.abc.b = b + ((m_iBlur + m_iOutlineSize) * 2) + m_iDropShadowOffset;
		finder.abc.c = c - m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
		m_ExtendedABCWidthsCache.Insert(finder);
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
// Purpose: returns the requested height of the font
//-----------------------------------------------------------------------------
int CWin32Font::GetHeightRequested()
{
	assert(IsValid());
	return m_iTall;
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
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CWin32Font::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
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


