//-----------------------------------------------------------------------------
// Name: Glyphs.cpp
//
// Desc: Functions and global variables for keeping track of font glyphs
//
// Hist: 09.06.02 - Revised Fontmaker sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef GLYPHS_H
#define GLYPHS_H

//-----------------------------------------------------------------------------
// Name: struct GLYPH_ATTR
// Desc: A structure to hold attributes for one glpyh. The left, right, etc.
//       values are texture coordinate offsets into the resulting texture image
//       (which ends up in the .tga file). The offset, width, etc. values are
//       spacing information, used when rendering the font.
//-----------------------------------------------------------------------------
struct FILE_GLYPH_ATTR
{
    FLOAT fLeft, fTop, fRight, fBottom;
};

struct GLYPH_ATTR : public FILE_GLYPH_ATTR
{
    int   a, b, c;
    int   x, y, w, h;
};




//-----------------------------------------------------------------------------
// Name: class CTextureFont
// Desc: A class to hold all information about a texture-based font
//-----------------------------------------------------------------------------
class CTextureFont
{
public:
    // current ttf font
    LOGFONT     m_LogFont;
    HFONT       m_hFont;

	BOOL		m_bAntialiasEffect;
    BOOL        m_bShadowEffect;
    BOOL        m_bOutlineEffect;
	int			m_nBlur;
	int			m_nScanlines;

    // Glyph info
    BYTE*       m_ValidGlyphs;
    WCHAR       m_cMaxGlyph;
    WORD*       m_TranslatorTable;
	BOOL        m_bIncludeNullCharacter;
    DWORD       m_dwNumGlyphs;
    GLYPH_ATTR* m_pGlyphs;

    // Texture info
    DWORD       m_dwTextureWidth;
    DWORD       m_dwTextureHeight;
    DWORD*      m_pBits;

    CHAR        m_strFontName[MAX_PATH];

	// current custom font
	const char		*m_pCustomFilename;
	unsigned char	m_customGlyphs[256];
	char			*m_pCustomGlyphFiles[256];
	int				m_maxCustomCharHeight;

public:
    HRESULT DeleteGlyph( WORD wGlyph );
	HRESULT InsertGlyph( WORD wGlyph );
    HRESULT ExtractValidGlyphsFromRange( WORD wStartGlyph, WORD wEndGlyph );
    HRESULT ExtractValidGlyphsFromFile( const CHAR* strFileName );
    HRESULT BuildTranslatorTable();
    HRESULT CalculateAndRenderGlyphs();
    HRESULT ReadCustomFontFile( CHAR* strFileName );
    HRESULT ReadFontInfoFile( CHAR* strFileName );
    HRESULT WriteFontInfoFile( CHAR* strFileName );
	HRESULT WriteFontImageFile( CHAR* strFileName, bool bAdditiveMode, bool bCustomFont );

	VOID	ClearFont();
    VOID    DestroyObjects();

    CTextureFont();
    ~CTextureFont();

private:
	GLYPH_ATTR* RenderCustomGlyphs( HBITMAP hBitmap );

	GLYPH_ATTR* RenderTTFGlyphs( HFONT hFont, HBITMAP hBitmap,
                                      DWORD dwTextureWidth, DWORD dwTextureHeight, 
                                      BOOL bOutlineEffect, BOOL bShadowEffect,
									  int nScanlineEffect, int nBlurEffect,
									  BOOL bAntialias,
                                      BYTE* ValidGlyphs, DWORD dwNumGlyphs );
};




#endif // GLYPHS_H
