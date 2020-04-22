//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <locale.h>
#include "vgui_surfacelib/BitmapFont.h"
#include "vgui_surfacelib/FontManager.h"
#include <vgui/ISurface.h>
#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CFontManager s_FontManager;

#ifdef WIN32
extern bool s_bSupportsUnicode;
#endif

#if !defined( _X360 )
#define MAX_INITIAL_FONTS	100
#else
#define MAX_INITIAL_FONTS	1
#endif

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CFontManager &FontManager()
{
	return s_FontManager;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontManager::CFontManager()
{
	// add a single empty font, to act as an invalid font handle 0
	m_FontAmalgams.EnsureCapacity( MAX_INITIAL_FONTS );
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity( MAX_INITIAL_FONTS );

#ifdef LINUX
	FT_Error error = FT_Init_FreeType( &library ); 
	if ( error )
		Error( "Unable to initalize freetype library, is it installed?" );
	m_pFontDataHelper = NULL;
#endif

	// setup our text locale
	setlocale( LC_CTYPE, "" );
	setlocale( LC_TIME, "" );
	setlocale( LC_COLLATE, "" );
	setlocale( LC_MONETARY, "" );

	m_pFileSystem = NULL;
	m_pMaterialSystem = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: language setting for font fallbacks
//-----------------------------------------------------------------------------
void CFontManager::SetLanguage(const char *language)
{
	Q_strncpy(m_szLanguage, language, sizeof(m_szLanguage));
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontManager::~CFontManager()
{
	ClearAllFonts();
	m_FontAmalgams.RemoveAll();
#ifdef LINUX
	FT_Done_FreeType( library );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: frees the fonts
//-----------------------------------------------------------------------------
void CFontManager::ClearAllFonts()
{
	// free the fonts
	for (int i = 0; i < m_Win32Fonts.Count(); i++)
	{
		delete m_Win32Fonts[i];
	}
	m_Win32Fonts.RemoveAll();

	for (int i = 0; i < m_FontAmalgams.Count(); i++)
	{
		m_FontAmalgams[i].RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::HFont CFontManager::CreateFont()
{
	int i = m_FontAmalgams.AddToTail();
	return i;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the valid glyph ranges for a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	return SetFontGlyphSet( font, windowsFontName, tall, weight, blur, scanlines, flags, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the valid glyph ranges for a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax)
{
	// ignore all but the first font added
	// need to rev vgui versions and change the name of this function
	if ( m_FontAmalgams[font].GetCount() > 0 )
	{
		// clear any existing fonts
		m_FontAmalgams[font].RemoveAll();
	}

	bool bForceSingleFontForXbox = false;
	if ( IsX360() )
	{
		// discovered xbox only allows glyphs from these languages from the foreign fallback font
		// prefer to have the entire range of chars from the font so UI doesn't suffer from glyph disparity
		if ( !V_stricmp( windowsFontName, "toolbox" ) )
		{
			// only the toolbox font is allowed to pass
		}
		else
		{
			if ( !V_stricmp( m_szLanguage, "polish" ) || 
				!V_stricmp( m_szLanguage, "russian" ) ||
				!V_stricmp( m_szLanguage, "japanese" ) ||
				!V_stricmp( m_szLanguage, "korean" ) ||
				!V_stricmp( m_szLanguage, "portuguese" ) ||
				!V_stricmp( m_szLanguage, "schinese" ) ||
				!V_stricmp( m_szLanguage, "tchinese" ) )
			{
				windowsFontName = GetForeignFallbackFontName();
				bForceSingleFontForXbox = true;
			}
		}
	}
	font_t *winFont = CreateOrFindWin32Font( windowsFontName, tall, weight, blur, scanlines, flags );


	// cycle until valid english/extended font support has been created
	do
	{
		// add to the amalgam
		if ( bForceSingleFontForXbox || IsFontForeignLanguageCapable( windowsFontName ) )
		{
			if ( winFont )
			{
				// font supports the full range of characters
				m_FontAmalgams[font].AddFont( winFont, 0x0000, 0xFFFF );
				return true;
			}
		}
		else
		{
			// font cannot provide glyphs and just supports the normal range
			// redirect to a font that can supply glyps
			const char *localizedFontName = GetForeignFallbackFontName();
			if ( winFont && !stricmp( localizedFontName, windowsFontName ) )
			{
				// it's the same font and can support the full range
				m_FontAmalgams[font].AddFont( winFont, 0x0000, 0xFFFF );
				return true;
			}

			// create the extended support font
			font_t *pExtendedFont = CreateOrFindWin32Font( localizedFontName, tall, weight, blur, scanlines, flags );
			if ( winFont && pExtendedFont )
			{
				// use the normal font for english characters, and the extended font for the rest
				int nMin = 0x0000, nMax = 0x00FF;

				// did we specify a range?
				if ( nRangeMin > 0 || nRangeMax > 0 )
				{
					nMin = nRangeMin;
					nMax = nRangeMax;

					// make sure they're in the correct order
					if ( nMin > nMax )
					{
						int nTemp = nMin;
						nMin = nMax;
						nMax = nTemp;
					}
				}

				if ( nMin > 0 )
				{
					m_FontAmalgams[font].AddFont( pExtendedFont, 0x0000, nMin - 1 );
				}

				m_FontAmalgams[font].AddFont( winFont, nMin, nMax );

				if ( nMax < 0xFFFF )
				{
					m_FontAmalgams[font].AddFont( pExtendedFont, nMax + 1, 0xFFFF );
				}

				return true;
			}
			else if ( pExtendedFont )
			{
				// the normal font failed to create
				// just use the extended font for the full range
				m_FontAmalgams[font].AddFont( pExtendedFont, 0x0000, 0xFFFF );
				return true;
			}
		}
		// no valid font has been created, so fallback to a different font and try again
	} 
	while ( NULL != ( windowsFontName = GetFallbackFontName( windowsFontName ) ) );

	// nothing successfully created
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetBitmapFontGlyphSet(HFont font, const char *windowsFontName, float scalex, float scaley, int flags)
{
	if ( m_FontAmalgams[font].GetCount() > 0 )
	{
		// clear any existing fonts
		m_FontAmalgams[font].RemoveAll();
	}

	CBitmapFont *winFont = CreateOrFindBitmapFont( windowsFontName, scalex, scaley, flags );
	if ( winFont )
	{
		// bitmap fonts are only 0-255
		m_FontAmalgams[font].AddFont( winFont, 0x0000, 0x00FF );
		return true;
	}

	// nothing successfully created
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
font_t *CFontManager::CreateOrFindWin32Font(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	font_t *winFont = NULL;
	int i;
	for (i = 0; i < m_Win32Fonts.Count(); i++)
	{
		if (m_Win32Fonts[i]->IsEqualTo(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			winFont = m_Win32Fonts[i];
			break;
		}
	}

	// create the new win32font if we didn't find it
	if (!winFont)
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();
		m_Win32Fonts[i] = NULL;

#ifdef LINUX
		int memSize = 0;
		void *pchFontData = m_pFontDataHelper( windowsFontName, memSize, NULL );

		if( !pchFontData )
		{
			// If we didn't find the font data in the font cache, then get the font filename...
			char *filename = CLinuxFont::GetFontFileName( windowsFontName, flags );
			if( filename )
			{
				// ... and try to add it to the font cache.
				pchFontData = m_pFontDataHelper( windowsFontName, memSize, filename );
				free( filename );
			}
		}

		if ( pchFontData )
		{
			m_Win32Fonts[i] = new font_t();
			if (m_Win32Fonts[i]->CreateFromMemory( windowsFontName, pchFontData, memSize, tall, weight, blur, scanlines, flags))
			{
				// add to the list
				winFont = m_Win32Fonts[i];
			}
		}

		if( !winFont )
		{
			// failed to create, remove
			if (  m_Win32Fonts[i] )
				delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}

#else
		m_Win32Fonts[i] = new font_t();
		if (m_Win32Fonts[i]->Create(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			// add to the list
			winFont = m_Win32Fonts[i];
		}
		else
		{
			// failed to create, remove
			delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}
#endif

	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
CBitmapFont *CFontManager::CreateOrFindBitmapFont(const char *windowsFontName, float scalex, float scaley, int flags)
{
	// see if we already have the font
	CBitmapFont *winFont = NULL;
	int i;
	for ( i = 0; i < m_Win32Fonts.Count(); i++ )
	{
		font_t *font = m_Win32Fonts[i];

		// Only looking for bitmap fonts
		int testflags = font->GetFlags();
		if ( !( testflags & vgui::ISurface::FONTFLAG_BITMAP ) )
		{
			continue;
		}

		CBitmapFont *bitmapFont = reinterpret_cast< CBitmapFont* >( font );
		if ( bitmapFont->IsEqualTo( windowsFontName, scalex, scaley, flags ) )
		{
			winFont = bitmapFont;
			break;
		}
	}

	// create the font if we didn't find it
	if ( !winFont )
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();

		CBitmapFont *bitmapFont = new CBitmapFont();
		if ( bitmapFont->Create( windowsFontName, scalex, scaley, flags ) )
		{
			// add to the list
			m_Win32Fonts[i] = bitmapFont;
			winFont = bitmapFont;
		}
		else
		{
			// failed to create, remove
			delete bitmapFont;
			m_Win32Fonts.Remove(i);
			return NULL;
		}
	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: sets the scale of a bitmap font
//-----------------------------------------------------------------------------
void CFontManager::SetFontScale(vgui::HFont font, float sx, float sy)
{
	m_FontAmalgams[font].SetFontScale( sx, sy );
}

const char *CFontManager::GetFontName( HFont font )
{
	// ignore the amalgam of disparate char ranges, assume the first font
	return m_FontAmalgams[font].GetFontName( 0 );
}

const char *CFontManager::GetFontFamilyName( HFont font )
{
	return m_FontAmalgams[font].GetFontFamilyName( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: gets the windows font for the particular font in the amalgam
//-----------------------------------------------------------------------------
font_t *CFontManager::GetFontForChar(vgui::HFont font, wchar_t wch)
{
	return m_FontAmalgams[font].GetFontForChar(wch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CFontManager::GetCharABCwide(HFont font, int ch, int &a, int &b, int &c)
{
	if ( !m_FontAmalgams.IsValidIndex( font ) )
	{
		a = b = c = 0;
		return;
	}

	font_t *winFont = m_FontAmalgams[font].GetFontForChar(ch);
	if (winFont)
	{
		winFont->GetCharABCWidths(ch, a, b, c);
	}
	else
	{
		// no font for this range, just use the default width
		a = c = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontTall(HFont font)
{
	return m_FontAmalgams[font].GetFontHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns requested height of the font
//-----------------------------------------------------------------------------
int CFontManager::GetFontTallRequested(HFont font)
{
	return m_FontAmalgams[font].GetFontHeightRequested();
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontAscent(HFont font, wchar_t wch)
{
	font_t *winFont = m_FontAmalgams[font].GetFontForChar(wch);
	if ( winFont )
	{
		return winFont->GetAscent();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFontManager::IsFontAdditive(HFont font)
{
	return ( m_FontAmalgams[font].GetFlags( 0 ) & vgui::ISurface::FONTFLAG_ADDITIVE ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFontManager::IsBitmapFont(HFont font)
{
	// A FontAmalgam is either some number of non-bitmap fonts, or a single bitmap font - so this check is valid
	return ( m_FontAmalgams[font].GetFlags( 0 ) & vgui::ISurface::FONTFLAG_BITMAP ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CFontManager::GetCharacterWidth(HFont font, int ch)
{
	if ( !iswcntrl( ch ) )
	{
		int a, b, c;
		GetCharABCwide(font, ch, a, b, c);
		return (a + b + c);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CFontManager::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	wide = 0;
	tall = 0;
	
	if (!text)
		return;

	tall = GetFontTall(font);
	
	float xx = 0;
	char chBefore = 0;
	char chAfter = 0;
	for (int i = 0; ; i++)
	{
		wchar_t ch = text[i];
		if (ch == 0)
		{
			break;
		}

		chAfter = text[i+1];

		if (ch == '\n')
		{
			tall += GetFontTall(font);
			xx=0;
		}
		else if (ch == '&')
		{
			// underscore character, so skip
		}
		else
		{
			float flWide, flabcA, flabcC;
			GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
			xx += flWide;
			if (xx > wide)
			{
				wide = ceil(xx);
			}
		}
		chBefore = ch;
	}
}

// font validation functions
struct FallbackFont_t
{
	const char *font;
	const char *fallbackFont;
};

#ifdef WIN32
const char *g_szValidAsianFonts[] = { "Marlett", NULL };
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
	{ "Tahoma", NULL },
	{ NULL, "Tahoma" },		// every other font falls back to this
};
#elif defined(OSX)
static const char *g_szValidAsianFonts[] = { "Apple Symbols", NULL };
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "Marlett", "Apple Symbols" },
	{ "Lucida Console", "Lucida Grande" },
	{ "Tahoma", "Helvetica" },
	{ "Helvetica", "Monaco" },
	{ "Monaco", NULL },
	{ NULL, "Monaco" }		// every other font falls back to this
};

#elif defined(LINUX)
static const char *g_szValidAsianFonts[] = { "Marlett", "WenQuanYi Zen Hei", "unifont", NULL };

// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "DejaVu Sans", NULL },
	{ NULL, "DejaVu Sans" },		// every other font falls back to this
};
#elif defined(_PS3)
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ NULL, "Tahoma" },		// every other font falls back to this
};
#else
#error
#endif

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is in the list of OK asian fonts
//-----------------------------------------------------------------------------
bool CFontManager::IsFontForeignLanguageCapable(const char *windowsFontName)
{
	if ( IsX360() )
	{
		return false;
	}

	for (int i = 0; g_szValidAsianFonts[i] != NULL; i++)
	{
		if (!stricmp(g_szValidAsianFonts[i], windowsFontName))
			return true;
	}

	// typeface isn't supported by asian languages
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: fallback fonts
//-----------------------------------------------------------------------------
const char *CFontManager::GetFallbackFontName(const char *windowsFontName)
{
	int i;
	for ( i = 0; g_FallbackFonts[i].font != NULL; i++ )
	{
		if (!stricmp(g_FallbackFonts[i].font, windowsFontName))
			return g_FallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_FallbackFonts[i].fallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: specialized fonts
//-----------------------------------------------------------------------------
const char *CFontManager::GetForeignFallbackFontName()
{
#ifdef WIN32
	// tahoma has all the necessary characters for asian/russian languages for winXP/2K+
	return "Tahoma";
#elif defined(OSX)
	return "Helvetica";
#elif defined(LINUX)
	return "WenQuanYi Zen Hei";
#elif defined(_PS3)
	return "Tahoma";
#else
#error
#endif
}

#if defined( _X360 )
bool CFontManager::GetCachedXUIMetrics( const char *pFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] )
{
	// linear lookup is good enough
	CUtlSymbol fontSymbol = pFontName;
	bool bFound = false;
	int i;
	for ( i = 0; i < m_XUIMetricCache.Count(); i++ )
	{
		if ( m_XUIMetricCache[i].fontSymbol == fontSymbol && m_XUIMetricCache[i].tall == tall && m_XUIMetricCache[i].style == style )
		{
			bFound = true;
			break;
		}
	}
	if ( !bFound )
	{
		return false;
	}

	// get from the cache
	*pFontMetrics = m_XUIMetricCache[i].fontMetrics;
	V_memcpy( charMetrics, m_XUIMetricCache[i].charMetrics, 256 * sizeof( XUICharMetrics ) );
	return true;
}
#endif

#if defined( _X360 )
void CFontManager::SetCachedXUIMetrics( const char *pFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] )
{
	MEM_ALLOC_CREDIT();

	int i = m_XUIMetricCache.AddToTail();

	m_XUIMetricCache[i].fontSymbol = pFontName;
	m_XUIMetricCache[i].tall = tall;
	m_XUIMetricCache[i].style = style;
	m_XUIMetricCache[i].fontMetrics = *pFontMetrics;
	V_memcpy( m_XUIMetricCache[i].charMetrics, charMetrics, 256 * sizeof( XUICharMetrics ) );
}
#endif

void CFontManager::ClearTemporaryFontCache()
{
#if defined( _X360 )
	COM_TimestampedLog( "ClearTemporaryFontCache(): Start" );

	m_XUIMetricCache.Purge();

	// many fonts are blindly precached by vgui and never used
	// font will re-open if glyph is actually requested
	for ( int i = 0; i < m_Win32Fonts.Count(); i++ )
	{
		m_Win32Fonts[i]->CloseResource();
	}

	COM_TimestampedLog( "ClearTemporaryFontCache(): Finish" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
bool CFontManager::GetFontUnderlined( HFont font )
{
	return m_FontAmalgams[font].GetUnderlined();
}

void CFontManager::GetKernedCharWidth( vgui::HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &flabcA, float &flabcC )
{
	wide = 0.0f;
	flabcA = 0.0f;
	
	Assert( font != vgui::INVALID_FONT );
	if ( font == vgui::INVALID_FONT )
		return;
		
	font_t *pFont = m_FontAmalgams[font].GetFontForChar(ch);
	if ( !pFont )
	{
		// no font for this range, just use the default width
		flabcA = 0.0f;
		wide = m_FontAmalgams[font].GetFontMaxWidth();
		return;
	}
	
	if ( m_FontAmalgams[font].GetFontForChar( chBefore ) != pFont )
		chBefore = 0;
	
	if ( m_FontAmalgams[font].GetFontForChar( chAfter ) != pFont )
		chAfter = 0;
	
#if defined(LINUX)
	pFont->GetKernedCharWidth( ch, chBefore, chAfter, wide, flabcA, flabcC );
#else
	pFont->GetKernedCharWidth( ch, chBefore, chAfter, wide, flabcA );
#endif
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CFontManager::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CFontManager", this, pchName );

	ValidateObj( m_FontAmalgams );
	for ( int iFont = 0; iFont < m_FontAmalgams.Count(); iFont++ )
	{
		ValidateObj( m_FontAmalgams[iFont] );
	}

	ValidateObj( m_Win32Fonts );
	for ( int iWin32Font = 0; iWin32Font < m_Win32Fonts.Count(); iWin32Font++ )
	{
		ValidatePtr( m_Win32Fonts[ iWin32Font ] );
	}

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE

