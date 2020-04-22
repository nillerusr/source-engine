//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FONTMANAGER_H
#define FONTMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "vgui_surfacelib/FontAmalgam.h"
#include "materialsystem/imaterialsystem.h"
#include "filesystem.h"
#include "vguifont.h"

#ifdef LINUX
#include <ft2build.h>
#include FT_FREETYPE_H
typedef void *(*FontDataHelper)( const char *pchFontName, int &size, const char *fontFileName );
#endif

#ifdef CreateFont
#undef CreateFont
#endif


using vgui::HFont;

//-----------------------------------------------------------------------------
// Purpose: Creates and maintains list of actively used fonts
//-----------------------------------------------------------------------------
class CFontManager
{
public:
	CFontManager();
	~CFontManager();

	void SetLanguage(const char *language);

	// clears the current font list, frees any resources
	void ClearAllFonts();

	HFont CreateFont();
	bool SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);
	bool SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax);
	bool SetBitmapFontGlyphSet(HFont font, const char *windowsFontName, float scalex, float scaley, int flags);
	void SetFontScale(HFont font, float sx, float sy);
	const char *GetFontName( HFont font );
	const char *GetFontFamilyName( HFont font );
	void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c);
	int GetFontTall(HFont font);
	int GetFontTallRequested(HFont font);
	int GetFontAscent(HFont font, wchar_t wch);
	int GetCharacterWidth(HFont font, int ch);
	bool GetFontUnderlined( HFont font );
	void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall);

	font_t *GetFontForChar(HFont, wchar_t wch);
	bool IsFontAdditive(HFont font);
	bool IsBitmapFont(HFont font );

	void SetInterfaces( IFileSystem *pFileSystem, IMaterialSystem *pMaterialSystem ) 
	{ 
		m_pFileSystem = pFileSystem; 
		m_pMaterialSystem = pMaterialSystem;
	}
	IFileSystem *FileSystem() { return m_pFileSystem; }
	IMaterialSystem *MaterialSystem() { return m_pMaterialSystem; }

#ifdef LINUX
	FT_Library GetFontLibraryHandle() { return library; }
	void SetFontDataHelper( FontDataHelper helper ) { m_pFontDataHelper = helper; }
#endif

#if defined( _X360 )
	// secondary cache to speed TTF setup
	bool GetCachedXUIMetrics( const char *pWindowsFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] );
	void SetCachedXUIMetrics( const char *pWindowsFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] );
#endif

	// used as a hint that intensive TTF operations are finished
	void ClearTemporaryFontCache();
	void GetKernedCharWidth( vgui::HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC );

private:
	bool IsFontForeignLanguageCapable(const char *windowsFontName);
	font_t *CreateOrFindWin32Font(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);
	CBitmapFont *CreateOrFindBitmapFont(const char *windowsFontName, float scalex, float scaley, int flags);
	const char *GetFallbackFontName(const char *windowsFontName);
	const char *GetForeignFallbackFontName();

	CUtlVector<CFontAmalgam> m_FontAmalgams;
	CUtlVector<font_t *> m_Win32Fonts;

#ifdef LINUX
	FT_Library library; 
	FontDataHelper m_pFontDataHelper;
#endif
	char m_szLanguage[64];
	IFileSystem		*m_pFileSystem;
	IMaterialSystem *m_pMaterialSystem;

#if defined( _X360 )
	// These are really bounded by the number of fonts that the game would ever realistically create, so ~100 is expected.
	// Many of these fonts are redundant and the same underlying metrics can be used. This avoid the very expensive TTF font metric lookup.
	struct XUIMetricCache_t
	{
		// the font signature that can change
		CUtlSymbol		fontSymbol;
		int				tall;
		int				style;

		// the metrics
		XUIFontMetrics	fontMetrics;
		XUICharMetrics	charMetrics[256];
	};
	CUtlVector< XUIMetricCache_t > m_XUIMetricCache;
#endif
};

// singleton accessor
extern CFontManager &FontManager();


#endif // FONTMANAGER_H
