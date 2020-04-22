//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox Compiled Bitmap Fonts
//
//=============================================================================//

// conversion from 'double' to 'float', possible loss of data
#pragma warning( disable : 4244 ) 
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "vgui_surfacelib/BitmapFont.h"
#include "vgui_surfacelib/FontManager.h"
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>
#include "filesystem.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct BitmapFontTable_t
{
	BitmapFontTable_t()
	{
		m_szName        = UTL_INVAL_SYMBOL;
		m_pBitmapFont   = NULL;
		m_pBitmapGlyphs = NULL;
		m_pTexture      = NULL;
	}

	CUtlSymbol		m_szName;
	BitmapFont_t	*m_pBitmapFont;
	BitmapGlyph_t	*m_pBitmapGlyphs;
	ITexture		*m_pTexture;
};

static CUtlVector< BitmapFontTable_t > g_BitmapFontTable( 1, 4 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBitmapFont::CBitmapFont()
{
	m_scalex = 1.0f;
	m_scaley = 1.0f;

	m_bitmapFontHandle = g_BitmapFontTable.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBitmapFont::~CBitmapFont()
{
}

//-----------------------------------------------------------------------------
// Purpose: creates the font.  returns false if the font cannot be mounted.
//-----------------------------------------------------------------------------
bool CBitmapFont::Create( const char *pFontFilename, float scalex, float scaley, int flags )
{
	MEM_ALLOC_CREDIT();

	if ( !pFontFilename || !pFontFilename[0] )
	{
		return false;
	}

	CUtlSymbol symbol;
	char fontName[MAX_PATH];
	Q_FileBase( pFontFilename, fontName, MAX_PATH );
	Q_strlower( fontName );
	symbol = fontName;

	// find a match that can use same entries
	BitmapFontTable_t *pFontTable = NULL;
	for ( int i=0; i<g_BitmapFontTable.Count(); i++ )
	{
		if ( symbol == g_BitmapFontTable[i].m_szName )
		{
			m_bitmapFontHandle = i;
			pFontTable = &g_BitmapFontTable[m_bitmapFontHandle];
			break;
		}
	}

	if ( !pFontTable )
	{
		void *pBuf = NULL;
		int nLength; 

		nLength = FontManager().FileSystem()->ReadFileEx( pFontFilename, "GAME", &pBuf ); 
		if ( nLength <= 0 || !pBuf )
		{
			// not found
			return false;
		}

		if ( ((BitmapFont_t*)pBuf)->m_id != LittleLong( BITMAPFONT_ID ) || ((BitmapFont_t*)pBuf)->m_Version != LittleLong( BITMAPFONT_VERSION ) )
		{
			// bad version
			return false;
		}

		if ( IsX360() )
		{
			CByteswap swap;
			swap.ActivateByteSwapping( true );
			swap.SwapFieldsToTargetEndian( (BitmapFont_t*)pBuf );
			swap.SwapFieldsToTargetEndian( (BitmapGlyph_t*)((char*)pBuf + sizeof( BitmapFont_t )), ((BitmapFont_t*)pBuf)->m_NumGlyphs );
		}

		// create it
		m_bitmapFontHandle = g_BitmapFontTable.AddToTail();
		pFontTable = &g_BitmapFontTable[m_bitmapFontHandle];

		pFontTable->m_szName = fontName;

		pFontTable->m_pBitmapFont = new BitmapFont_t;
		memcpy( pFontTable->m_pBitmapFont, pBuf, sizeof( BitmapFont_t ) );

		pFontTable->m_pBitmapGlyphs = new BitmapGlyph_t[pFontTable->m_pBitmapFont->m_NumGlyphs];
		memcpy( pFontTable->m_pBitmapGlyphs, (unsigned char*)pBuf + sizeof(BitmapFont_t), pFontTable->m_pBitmapFont->m_NumGlyphs*sizeof(BitmapGlyph_t) );

		FontManager().FileSystem()->FreeOptimalReadBuffer( pBuf );

		// load the art resources
		char textureName[MAX_PATH];
		Q_snprintf( textureName, MAX_PATH, "vgui/fonts/%s", fontName );
		pFontTable->m_pTexture = FontManager().MaterialSystem()->FindTexture( textureName, TEXTURE_GROUP_VGUI );

#if defined( _DEBUG ) && !defined( DX_TO_GL_ABSTRACTION )
		if ( pFontTable->m_pBitmapFont->m_PageWidth != pFontTable->m_pTexture->GetActualWidth() ||
			pFontTable->m_pBitmapFont->m_PageHeight != pFontTable->m_pTexture->GetActualHeight() )
		{
			// font is out of sync with its art
			Assert( 0 );
			return false;
		}
#endif
		// the font texture lives forever, ensure it doesn't get purged
		pFontTable->m_pTexture->IncrementReferenceCount();
	}

	// setup font properties
	m_scalex = scalex;
	m_scaley = scaley;

	// flags are derived from the baked font
	m_iFlags = vgui::ISurface::FONTFLAG_BITMAP;
	int bitmapFlags = pFontTable->m_pBitmapFont->m_Flags;

	if ( bitmapFlags & BF_ANTIALIASED )
	{
		m_iFlags |= vgui::ISurface::FONTFLAG_ANTIALIAS;
	}

	if ( bitmapFlags & BF_ITALIC )
	{
		m_iFlags |= vgui::ISurface::FONTFLAG_ITALIC;
	}

	if ( bitmapFlags & BF_BLURRED )
	{
		m_iFlags |= vgui::ISurface::FONTFLAG_GAUSSIANBLUR;
		m_iBlur = 1;
	}

	if ( bitmapFlags & BF_SCANLINES )
	{
		m_iScanLines = 1;
	}

	if ( bitmapFlags & BF_OUTLINED )
	{
		m_iFlags |= vgui::ISurface::FONTFLAG_OUTLINE;
		m_iOutlineSize = 1;
	}

	if ( bitmapFlags & BF_DROPSHADOW )
	{
		m_iFlags |= vgui::ISurface::FONTFLAG_DROPSHADOW;
		m_iDropShadowOffset = 1;
	}

	if ( flags & vgui::ISurface::FONTFLAG_ADDITIVE )
	{
		m_bAdditive = true;
		m_iFlags |= vgui::ISurface::FONTFLAG_ADDITIVE;
	}

	m_iMaxCharWidth = (float)pFontTable->m_pBitmapFont->m_MaxCharWidth * m_scalex;
	m_iHeight       = (float)pFontTable->m_pBitmapFont->m_MaxCharHeight * m_scaley;
	m_iAscent       = (float)pFontTable->m_pBitmapFont->m_Ascent * m_scaley;

	// mark as valid
	m_szName = fontName;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CBitmapFont::IsEqualTo( const char *windowsFontName, float scalex, float scaley, int flags )
{
	char fontname[MAX_PATH];
	Q_FileBase( windowsFontName, fontname, MAX_PATH );

	if ( !Q_stricmp( fontname, m_szName.String() ) &&
		m_scalex == scalex && 
		m_scaley == scaley  )
	{
		int commonFlags = m_iFlags & flags;
		if ( commonFlags & vgui::ISurface::FONTFLAG_ADDITIVE )
		{
			// an exact match
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: sets the scale for a font
//-----------------------------------------------------------------------------
void CBitmapFont::SetScale( float sx, float sy )
{
	m_scalex = sx;
	m_scaley = sy;
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CBitmapFont::GetCharABCWidths( int ch, int &a, int &b, int &c )
{
	Assert( IsValid() && ch >= 0 && ch <= 255 );

	BitmapFontTable_t *pFont = &g_BitmapFontTable[m_bitmapFontHandle];

	ch = pFont->m_pBitmapFont->m_TranslateTable[ch];
	a = (float)pFont->m_pBitmapGlyphs[ch].a * m_scalex;
	b = (float)pFont->m_pBitmapGlyphs[ch].b * m_scalex;
	c = (float)pFont->m_pBitmapGlyphs[ch].c * m_scalex;
}

void CBitmapFont::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *prgba )
{
	// CBitmapFont derives off CLinuxFont, etc. But you should never call GetCharRGBA on a bitmap font.
	// If we let this fall into the CLinuxFont code, we'd have a difficult to track down bug - so crash
	//	hard here...
	Error( "GetCharRGBA called on CBitmapFont." );
}

void CBitmapFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	Assert( IsValid() && ch >= 0 && ch <= 255 );

	float abcB;
	BitmapFontTable_t *pFont = &g_BitmapFontTable[m_bitmapFontHandle];

	ch = pFont->m_pBitmapFont->m_TranslateTable[ch];
	abcA = (float)pFont->m_pBitmapGlyphs[ch].a * m_scalex;
	abcB = (float)pFont->m_pBitmapGlyphs[ch].b * m_scalex;
	abcC = (float)pFont->m_pBitmapGlyphs[ch].c * m_scalex;

	wide = ( abcA + abcB + abcC );
}

//-----------------------------------------------------------------------------
// Purpose: gets the texcoords for a character
//-----------------------------------------------------------------------------
void CBitmapFont::GetCharCoords( int ch, float *left, float *top, float *right, float *bottom )
{
	Assert( IsValid() && ch >= 0 && ch <= 255 );

	BitmapFontTable_t *pFont = &g_BitmapFontTable[m_bitmapFontHandle];

	ch = pFont->m_pBitmapFont->m_TranslateTable[ch];
	*left   = (float)pFont->m_pBitmapGlyphs[ch].x/(float)pFont->m_pBitmapFont->m_PageWidth;
	*top    = (float)pFont->m_pBitmapGlyphs[ch].y/(float)pFont->m_pBitmapFont->m_PageHeight;
	*right  = (float)(pFont->m_pBitmapGlyphs[ch].x+pFont->m_pBitmapGlyphs[ch].w)/(float)pFont->m_pBitmapFont->m_PageWidth;
	*bottom = (float)(pFont->m_pBitmapGlyphs[ch].y+pFont->m_pBitmapGlyphs[ch].h)/(float)pFont->m_pBitmapFont->m_PageHeight;
}

//-----------------------------------------------------------------------------
// Purpose: gets the texture page
//-----------------------------------------------------------------------------
ITexture *CBitmapFont::GetTexturePage()
{
	Assert( IsValid() );

	return g_BitmapFontTable[m_bitmapFontHandle].m_pTexture;
}

BEGIN_BYTESWAP_DATADESC( BitmapGlyph_t )
	DEFINE_FIELD( x, FIELD_SHORT ),
	DEFINE_FIELD( y, FIELD_SHORT ),
	DEFINE_FIELD( w, FIELD_SHORT ),
	DEFINE_FIELD( h, FIELD_SHORT ),
	DEFINE_FIELD( a, FIELD_SHORT ),
	DEFINE_FIELD( b, FIELD_SHORT ),
	DEFINE_FIELD( c, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( BitmapFont_t )
	DEFINE_FIELD( m_id, FIELD_INTEGER ),
	DEFINE_FIELD( m_Version, FIELD_INTEGER ),
	DEFINE_FIELD( m_PageWidth, FIELD_SHORT ),
	DEFINE_FIELD( m_PageHeight, FIELD_SHORT ),
	DEFINE_FIELD( m_MaxCharWidth, FIELD_SHORT ),
	DEFINE_FIELD( m_MaxCharHeight, FIELD_SHORT ),
	DEFINE_FIELD( m_Flags, FIELD_SHORT ),
	DEFINE_FIELD( m_Ascent, FIELD_SHORT ),
	DEFINE_FIELD( m_NumGlyphs, FIELD_SHORT ),
	DEFINE_ARRAY( m_TranslateTable, FIELD_CHARACTER, 256 ),
END_BYTESWAP_DATADESC()
