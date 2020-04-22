//-----------------------------------------------------------------------------
// Name: Glyphs.cpp
//
// Desc: Functions and global variables for keeping track of font glyphs
//
// Hist: 09.06.02 - Revised Fontmaker sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Glyphs.h"
#include "FontMaker.h"

const COLORREF COLOR_WHITE   = RGB(255,255,255);
const COLORREF COLOR_BLACK   = RGB(  0,  0,  0);
const COLORREF COLOR_BLUE    = RGB(  0,  0,255);

//-----------------------------------------------------------------------------
// Name: CTextureFont()
// Desc: Constructor
//-----------------------------------------------------------------------------
CTextureFont::CTextureFont()
{
	ZeroMemory( this, sizeof( *this ) );

	m_bIncludeNullCharacter = TRUE;
	m_bAntialiasEffect = TRUE;

    // Texture info
    m_dwTextureWidth        = 256;
    m_dwTextureHeight       = 256;

    // Default glyph range
    WORD wStartGlyph = 32;
    WORD wEndGlyph   = 255;
    ExtractValidGlyphsFromRange( wStartGlyph, wEndGlyph );
}

//-----------------------------------------------------------------------------
// Name: ~CTextureFont()
// Desc: Destructor
//-----------------------------------------------------------------------------
CTextureFont::~CTextureFont()
{
    DestroyObjects();

    if ( m_hFont )
        DeleteObject( m_hFont );

    if ( m_pBits )
        delete[] m_pBits;

    m_pBits   = NULL;
    m_hFont   = NULL;
}

//-----------------------------------------------------------------------------
// ClearFont
//-----------------------------------------------------------------------------
void CTextureFont::ClearFont()
{
	DestroyObjects();

    ZeroMemory( &m_LogFont, sizeof(LOGFONT) );

	m_strFontName[0] = '\0';

	m_hFont = NULL;

	m_dwTextureWidth = 256;
	m_dwTextureHeight = 256;

	m_bAntialiasEffect = FALSE;
    m_bShadowEffect    = FALSE;
    m_bOutlineEffect   = FALSE;
	m_nBlur            = 0;
	m_nScanlines       = 0;
}

//-----------------------------------------------------------------------------
// Name: DestroyObjects()
// Desc: Cleans up all allocated resources for the class
//-----------------------------------------------------------------------------
VOID CTextureFont::DestroyObjects()
{
    if ( m_pGlyphs )
        delete[] m_pGlyphs;
    if ( m_ValidGlyphs )
        delete[] m_ValidGlyphs;
    if ( m_TranslatorTable )
        delete[] m_TranslatorTable;

	if ( m_pCustomFilename )
	{
		TL_Free( (void*)m_pCustomFilename );
		m_pCustomFilename = NULL;

		for (DWORD i=0; i<m_dwNumGlyphs; i++)
		{
			if ( m_pCustomGlyphFiles[i] )
			{
				TL_Free( m_pCustomGlyphFiles[i] );
				m_pCustomGlyphFiles[i] = NULL;
			}
		}
	}

    m_cMaxGlyph       = 0;
    m_dwNumGlyphs     = 0;
    m_pGlyphs         = NULL;
    m_ValidGlyphs     = NULL;
    m_TranslatorTable = NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HRESULT CTextureFont::InsertGlyph( WORD wGlyph )
{
	m_cMaxGlyph   = 0;
    m_dwNumGlyphs = 0;

	m_ValidGlyphs[wGlyph] = 1;

    for ( DWORD c=0; c<=65535; c++ )
    {
		if ( m_ValidGlyphs[c] )
		{
			m_dwNumGlyphs++;
			m_cMaxGlyph = (WCHAR)c;
		}
    }

	BuildTranslatorTable();
	theApp.CalculateAndRenderGlyphs();

	return S_OK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HRESULT CTextureFont::DeleteGlyph( WORD wGlyph )
{
    m_cMaxGlyph       = 0;
    m_dwNumGlyphs     = 0;

	m_ValidGlyphs[wGlyph] = 0;

    for ( DWORD c=0; c<=65535; c++ )
    {
		if ( m_ValidGlyphs[c] )
		{
			m_dwNumGlyphs++;
			m_cMaxGlyph = (WCHAR)c;
		}
    }

	BuildTranslatorTable();
	theApp.CalculateAndRenderGlyphs();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ExtractValidGlyphsFromRange()
// Desc: Set global variables to indicate we will be drawing all glyphs in the
//       range specified.
//-----------------------------------------------------------------------------
HRESULT CTextureFont::ExtractValidGlyphsFromRange( WORD wStartGlyph, WORD wEndGlyph )
{
    // Cleanup any previous entries
    if( m_ValidGlyphs )
        delete[] m_ValidGlyphs;

    m_cMaxGlyph       = 0;
    m_dwNumGlyphs     = 0;
    m_ValidGlyphs     = NULL;

    // Allocate memory for the array of vaild glyphs
    m_ValidGlyphs = new BYTE[65536];
    ZeroMemory( m_ValidGlyphs, 65536 );

    for( DWORD c=(DWORD)wStartGlyph; c<=(DWORD)wEndGlyph; c++ )
    {
        m_ValidGlyphs[c] = 1;
        m_dwNumGlyphs++;
    }

    m_cMaxGlyph = wEndGlyph;

	BuildTranslatorTable();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ExtractValidGlyphsFromRange()
// Desc: Set global variables to indicate we will be drawing all glyphs that
//       are present in the specified text file.
//-----------------------------------------------------------------------------
HRESULT CTextureFont::ExtractValidGlyphsFromFile( const CHAR* strFileName )
{
    // Open the file
    FILE* file = fopen( strFileName, "rb" );
    if( NULL == file )
        return E_FAIL;

    // Cleanup any previous entries
    if( m_ValidGlyphs )
        delete[] m_ValidGlyphs;

	m_cMaxGlyph       = 0;
    m_dwNumGlyphs     = 0;
    m_ValidGlyphs     = NULL;

    // Allocate memory for the array of vaild glyphs
    m_ValidGlyphs = new BYTE[65536];
    ZeroMemory( m_ValidGlyphs, 65536 );

    // Skip the unicode marker
    BOOL bIsUnicode = (fgetwc(file) == 0xfeff) ? TRUE : FALSE;

    if( bIsUnicode == FALSE )
        rewind( file );

    // Record which glyphs are valid
    WCHAR c;
    while( (WCHAR)EOF != ( c = bIsUnicode ? fgetwc(file) : fgetc(file) ) )
    {
        while( c == L'\\' )
        {
            c = bIsUnicode ? fgetwc(file) : fgetc(file);

            // Handle octal-coded characters
            if( isdigit(c) )
            {
                int code = (c - L'0');
                c = bIsUnicode ? fgetwc(file) : fgetc(file);
                
                if( isdigit(c) )
                {
                    code = code*8 + (c - L'0');
                    c = bIsUnicode ? fgetwc(file) : fgetc(file);

                    if( isdigit(c) )
                    {
                        code = code*8 + (c - L'0');
                        c = bIsUnicode ? fgetwc(file) : fgetc(file);
                    }
                }

                if( m_ValidGlyphs[code] == 0 )
                {
                    if( code > m_cMaxGlyph )
                        m_cMaxGlyph = (WCHAR)code;

                    m_dwNumGlyphs++;
                    m_ValidGlyphs[code] = 2;
                }
            }
            else
            { 
                // Add the backslash character
                if( L'\\' > m_cMaxGlyph )
                    m_cMaxGlyph = L'\\';

                if( m_ValidGlyphs[L'\\'] == 0 )
                {
                    m_dwNumGlyphs++;
                    m_ValidGlyphs[L'\\'] = 1;
                }
            }
        }

        if( m_ValidGlyphs[c] == 0 )
        {
            // If the character is a printable one, add it
            if( c != L'\n' && c != L'\r' && c != 0xffff )
            {
                m_dwNumGlyphs++;
                m_ValidGlyphs[c] = 1;

                if( c > m_cMaxGlyph )
                    m_cMaxGlyph = c;
            }
        }
    }

    // Done with the file
    fclose( file );

	BuildTranslatorTable();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: BuildTranslatorTable()
// Desc: Builds a table to translate from a WCHAR to a glyph index.
//-----------------------------------------------------------------------------
HRESULT CTextureFont::BuildTranslatorTable()
{
    if ( m_TranslatorTable )
        delete[] m_TranslatorTable;

	// Insure the \0 is there
    if ( m_bIncludeNullCharacter && 0 == m_ValidGlyphs[0] )
	{
        m_dwNumGlyphs++;
		m_ValidGlyphs[0] = 1;
	}

    // Fill the string of all valid glyphs and build the translator table
    m_TranslatorTable = new WORD[m_cMaxGlyph+1];
    ZeroMemory( m_TranslatorTable, sizeof(WORD)*(m_cMaxGlyph+1) );

	if ( !m_pCustomFilename )
	{
		// ttf has glyphs that are sequential
		DWORD dwGlyph = 0;
		for ( DWORD i=0; i<65536; i++ )
		{
			if ( m_ValidGlyphs[i] )
			{
				m_TranslatorTable[i] = (WORD)dwGlyph;
				dwGlyph++;
			}
		}
	}
	else
	{
		// custom font has glyphs that are scattered
		DWORD dwGlyph = 0;
		for ( DWORD i=0; i<m_dwNumGlyphs; i++ )
		{
			if ( !i && m_bIncludeNullCharacter )
			{
				m_TranslatorTable[0] = 0;
				dwGlyph++;
				continue;
			}

			m_TranslatorTable[m_customGlyphs[i-1]] = (WORD)dwGlyph;
			dwGlyph++;
		}
	}
    
    return S_OK;
}

void StripQuotedToken( char *pToken )
{
	int len = strlen( pToken );
	if ( !len )
		return;

	if ( pToken[0] == '\"' && pToken[len-1] == '\"' )
	{
		memcpy( pToken, pToken+1, len-1 );
		pToken[len-2] = '\0';
	}
}



//-----------------------------------------------------------------------------
// ReadCustomFontFile
//-----------------------------------------------------------------------------
HRESULT CTextureFont::ReadCustomFontFile( CHAR* strFileName )
{
	char			*pToken;
	char			fontName[128];
	bool			bSuccess;
	unsigned char	glyphs[256];
	char			*glyphFiles[512];
	char			basePath[MAX_PATH];
	int				numGlyphs;
	char			filename[MAX_PATH];

	bSuccess = false;
	numGlyphs = 0;

	ClearFont();

	TL_LoadScriptFile( strFileName );

	strcpy( basePath, strFileName );
	TL_StripFilename( basePath );
	TL_AddSeperatorToPath( basePath, basePath );

	fontName[0] = '\0';
	while ( 1 )
	{
		pToken = TL_GetToken( true );
		if ( !pToken || !pToken[0] )
			break;
		StripQuotedToken( pToken );

		// get font name
		if ( !stricmp( pToken, "fontName" ) )
		{
			pToken = TL_GetToken( true );
			if ( !pToken || !pToken[0] )
				goto cleanUp;
			StripQuotedToken( pToken );
			strcpy( fontName, pToken );
			continue;
		}
	
		// get glyph
		if ( strlen( pToken ) != 1 )
			goto cleanUp;
		glyphs[numGlyphs] = pToken[0];

		// get glyph file
		pToken = TL_GetToken( true );
		if ( !pToken || !pToken[0] )
			goto cleanUp;
		StripQuotedToken( pToken );
		sprintf( filename, "%s%s", basePath, pToken );
		glyphFiles[numGlyphs] = TL_CopyString( filename );

		numGlyphs++;
		if ( numGlyphs >= 256 )
			break;
	}

	if ( numGlyphs == 0 )
		goto cleanUp;

	m_pCustomFilename = TL_CopyString( strFileName );
	strcpy ( m_strFontName, fontName );
	
	m_dwTextureWidth = 256;
	m_dwTextureHeight = 256;

    m_ValidGlyphs = new BYTE[65536];
    ZeroMemory( m_ValidGlyphs, 65536 );

	m_dwNumGlyphs = numGlyphs;
	m_cMaxGlyph = 0;
	for (int i=0; i<numGlyphs; i++)
	{
        m_ValidGlyphs[glyphs[i]] = 1;

		if ( m_cMaxGlyph < glyphs[i] )
			m_cMaxGlyph = glyphs[i];

		m_customGlyphs[i] = glyphs[i];
		m_pCustomGlyphFiles[i] = glyphFiles[i];
    }

	BuildTranslatorTable();

	bSuccess = true;

cleanUp:
	TL_FreeScriptFile();
	return bSuccess ? S_OK : E_FAIL;
}

//-----------------------------------------------------------------------------
// Name: ReadFontInfoFile()
// Desc: Loads the font's glyph info from a file
//-----------------------------------------------------------------------------
HRESULT CTextureFont::ReadFontInfoFile( CHAR* strFileName )
{
	BitmapFont_t	bitmapFont;

	// open the info file
    FILE* file = fopen( strFileName, "rb" );
    if ( NULL == file )
        return E_FAIL;

	memset( &bitmapFont, 0, sizeof( BitmapFont_t ) );
	fread( &bitmapFont, 1, sizeof( BitmapFont_t ), file );

	if ( bitmapFont.m_id != BITMAPFONT_ID || bitmapFont.m_Version != BITMAPFONT_VERSION )
	{
		return E_FAIL;
	}

	theApp.SetTextureSize( bitmapFont.m_PageWidth, bitmapFont.m_PageHeight );

	ZeroMemory( m_ValidGlyphs, 65536 );

	m_dwNumGlyphs = 0;
	m_cMaxGlyph = 0;
	for (int i=0; i<256; i++)
	{
		if ( bitmapFont.m_TranslateTable[i] )
		{
			m_ValidGlyphs[i] = 1;
			m_cMaxGlyph = i;
			m_dwNumGlyphs++;
		}
	}
	BuildTranslatorTable();

	// success
    fclose( file );

	theApp.OnGlyphsCustom();
	theApp.CalculateAndRenderGlyphs();
	
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: WriteFontInfoFile()
// Desc: Writes the font's glyph info to a file
//-----------------------------------------------------------------------------
HRESULT CTextureFont::WriteFontInfoFile( CHAR* strFileName )
{
	BitmapFont_t	bitmapFont;
	BitmapGlyph_t	bitmapGlyph;

	// Create the info file
    FILE* file = fopen( strFileName, "wb" );
    if ( NULL == file )
        return E_FAIL;

	bitmapFont.m_id            = BITMAPFONT_ID;
	bitmapFont.m_Version       = BITMAPFONT_VERSION;
	bitmapFont.m_PageWidth     = (short)m_dwTextureWidth;
	bitmapFont.m_PageHeight    = (short)m_dwTextureHeight;
	bitmapFont.m_Ascent        = 0;
	bitmapFont.m_NumGlyphs     = (short)m_dwNumGlyphs;

	// generate flags
	bitmapFont.m_Flags = 0;
	if ( m_bAntialiasEffect )
	{
		bitmapFont.m_Flags |= BF_ANTIALIASED;
	}
	if ( m_bShadowEffect )
	{
		bitmapFont.m_Flags |= BF_DROPSHADOW;
	}
	if ( m_bOutlineEffect )
	{
		bitmapFont.m_Flags |= BF_OUTLINED;
	}
	if ( m_nBlur )
	{
		bitmapFont.m_Flags |= BF_BLURRED;
	}
	if ( m_nScanlines )
	{
		bitmapFont.m_Flags |= BF_SCANLINES;
	}
	if ( m_LogFont.lfItalic )
	{
		bitmapFont.m_Flags |= BF_ITALIC;
	}
	if ( m_LogFont.lfWeight > 400 )
	{
		bitmapFont.m_Flags |= BF_BOLD;
	}
	if ( m_pCustomFilename )
	{
		bitmapFont.m_Flags |= BF_CUSTOM;
	}

	// determine max char width from all glyphs
	bitmapFont.m_MaxCharWidth = 0;
    for (unsigned int i=0; i<m_dwNumGlyphs; i++ )
    {
		if ( bitmapFont.m_MaxCharWidth < m_pGlyphs[i].w )
		{
			bitmapFont.m_MaxCharWidth = m_pGlyphs[i].w;
		}
	}

	bitmapFont.m_MaxCharHeight = 0;
    for (unsigned int i=0; i<m_dwNumGlyphs; i++ )
    {
		if ( bitmapFont.m_MaxCharHeight < m_pGlyphs[i].h )
		{
			bitmapFont.m_MaxCharHeight = m_pGlyphs[i].h;
		}
	}

	// maps a char index to its actual glyph
	for (int i=0; i<256; i++)
	{
		if ( i <= m_cMaxGlyph )
		{
			bitmapFont.m_TranslateTable[i] = (unsigned char)m_TranslatorTable[i];
		}
		else
		{
			bitmapFont.m_TranslateTable[i] = 0;
		}
	}

	// write out the header
	fwrite( &bitmapFont, sizeof( BitmapFont_t ), 1, file ); 

    // Write out the vertical padding caused by effects
//	FLOAT fTopPadding    = ( m_bOutlineEffect ? 1.0f : 0.0f );
//	FLOAT fBottomPadding = ( m_bOutlineEffect ? ( m_bShadowEffect ? 2.0f : 1.0f ) : ( m_bShadowEffect ? 2.0f : 0.0f ) );
//	FLOAT fFontYAdvance  = fFontHeight - fTopPadding - fBottomPadding;
    
    // Write the glyph attributes to the file
    for (unsigned int i=0; i<m_dwNumGlyphs; i++ )
    {
		bitmapGlyph.x = m_pGlyphs[i].x;
		bitmapGlyph.y = m_pGlyphs[i].y;
		bitmapGlyph.w = m_pGlyphs[i].w;
		bitmapGlyph.h = m_pGlyphs[i].h;
		bitmapGlyph.a = m_pGlyphs[i].a;
		bitmapGlyph.b = m_pGlyphs[i].b;
		bitmapGlyph.c = m_pGlyphs[i].c;

        fwrite( &bitmapGlyph, sizeof( BitmapGlyph_t ), 1, file ); 
    }

	// success
    fclose( file );
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: WriteTargaFile()
// Desc: Writes 32-bit RGBA data to a .tga file
//-----------------------------------------------------------------------------
HRESULT WriteTargaFile( CHAR* strFileName, DWORD dwWidth, DWORD dwHeight, 
                        DWORD* pRGBAData )
{
    struct TargaHeader
    {
        BYTE IDLength;
        BYTE ColormapType;
        BYTE ImageType;
        BYTE ColormapSpecification[5];
        WORD XOrigin;
        WORD YOrigin;
        WORD ImageWidth;
        WORD ImageHeight;
        BYTE PixelDepth;
        BYTE ImageDescriptor;
    } tga;

    // Create the file
    FILE* file = fopen( strFileName, "wb" );
    if( NULL == file )
        return E_FAIL;

    // Write the TGA header
    ZeroMemory( &tga, sizeof(tga) );
    tga.IDLength        = 0;
    tga.ImageType       = 2;
    tga.ImageWidth      = (WORD)dwWidth;
    tga.ImageHeight     = (WORD)dwHeight;
    tga.PixelDepth      = 32;
    tga.ImageDescriptor = 0x28;
    fwrite( &tga, sizeof(TargaHeader), 1, file );

    // Write the pixels
    fwrite( pRGBAData, sizeof(DWORD), dwHeight*dwWidth, file );

    // Close the file and return okay
    fclose( file );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: WriteFontImageFile()
// Desc: Writes 32-bit RGBA data to a .tga file
//-----------------------------------------------------------------------------
HRESULT CTextureFont::WriteFontImageFile( CHAR* strFileName, bool bAdditiveMode, bool bCustomFont )
{
    // Convert the bits to have an alpha channel
    DWORD* pRGBAData = new DWORD[m_dwTextureWidth*m_dwTextureHeight];

	FLOAT	l;
    for ( DWORD i=0; i<m_dwTextureWidth*m_dwTextureHeight; i++ )
    {
		FLOAT a = ( ( 0xff000000 & m_pBits[i] ) >> 24L ) / 255.0f;
        FLOAT r = ( ( 0x00ff0000 & m_pBits[i] ) >> 16L ) / 255.0f;
        FLOAT g = ( ( 0x0000ff00 & m_pBits[i] ) >>  8L ) / 255.0f;
        FLOAT b = ( ( 0x000000ff & m_pBits[i] ) >>  0L ) / 255.0f;

		if ( bCustomFont )
		{
			if ( a == 0.0f && b == 1.0f )
			{
				// pure transluscent
				a = 0;
				r = g = b = 0.0f;
			}
			
			int red   = (int)(r * 255.0f);
			int green = (int)(g * 255.0f);
			int blue  = (int)(b * 255.0f);
			int alpha = (int)(a * 255.0f);

			pRGBAData[i] = (alpha<<24L) | (red<<16L) | (green<<8L) | (blue<<0L);
		}
		else
		{
			if ( bAdditiveMode )
			{
				// all channels should be same
				if (( r != g ) || ( r != b ))
				{
				}

				l = r;
				a = 1.0f;
			}
			else
			{
				a = r + (1-b);
				if ( a )
					l = r / a;
				else
					l = 1;
			}

			DWORD alpha = (DWORD)( a * 255.0f );
			DWORD lum   = (DWORD)( l * 255.0f );

			pRGBAData[i] = (alpha<<24L) | (lum<<16L) | (lum<<8L) | (lum<<0L);
		}
    }

    // Write the file
    HRESULT hr = WriteTargaFile( strFileName, m_dwTextureWidth,
                                 m_dwTextureHeight, pRGBAData );

    // Cleanup and return
    delete[] pRGBAData;
    return hr;
}

void GetBitmapBits2( HBITMAP hBitmap, int width, int height, void *pBits )
{
	memset( pBits, 0, width*height*4 );

	HDC hDC = CreateCompatibleDC( NULL );
	BITMAPINFO	bitmapInfo = {0};
	bitmapInfo.bmiHeader.biSize = sizeof( bitmapInfo.bmiHeader );
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = -height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	GetDIBits( hDC, hBitmap, 0, height, pBits, &bitmapInfo ,DIB_RGB_COLORS );
	DeleteDC( hDC );	
}

void SetBitmapBits2( HBITMAP hBitmap, int width, int height, void *pBits )
{
	HDC hDC = CreateCompatibleDC( NULL );
	BITMAPINFO	bitmapInfo = {0};
	bitmapInfo.bmiHeader.biSize = sizeof( bitmapInfo.bmiHeader );
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = -height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	SetDIBits( hDC, hBitmap, 0, height, pBits, &bitmapInfo ,DIB_RGB_COLORS );
	DeleteDC( hDC );	
}

//-----------------------------------------------------------------------------
//	SetTextureBits
//
//	Blit the rect back into the bitmap
//-----------------------------------------------------------------------------
void SetTextureBits( HBITMAP hBitmap, int bitmapWidth, int bitmapHeight, int x, int y, int w, int h, unsigned char *pRGBA )
{
	// get the enitre bitmap
	unsigned char *pBitmapBits = (unsigned char *)malloc( bitmapWidth * bitmapHeight * 4);
	GetBitmapBits2( hBitmap, bitmapWidth, bitmapHeight, pBitmapBits );

	// copy into bitmap bits
	unsigned char *pSrc = pRGBA;
	for (int yy=y; yy<y+h; yy++)
	{
		if ( yy >= bitmapHeight )
		{
			// past end of bitmap
			break;
		}

		unsigned char *pDst = pBitmapBits + (yy*bitmapWidth + x)*4;
		for (int xx=0; xx<w; xx++)
		{
			if ( xx+x < bitmapWidth )
			{
				pDst[0] = pSrc[0];
				pDst[1] = pSrc[1];
				pDst[2] = pSrc[2];
				pDst[3] = pSrc[3];
			}
			pSrc += 4;
			pDst += 4;
		}
	}

	SetBitmapBits2( hBitmap, bitmapWidth, bitmapHeight, pBitmapBits );

	free( pBitmapBits );
}

//-----------------------------------------------------------------------------
//	GetTextureBits
//
//	Blit the rect out of the bitmap
//-----------------------------------------------------------------------------
unsigned char *GetTextureBits( HBITMAP hBitmap, int bitmapWidth, int bitmapHeight, int x, int y, int w, int h )
{
	// get the enitre bitmap
	unsigned char *pBitmapBits = new unsigned char[bitmapWidth * bitmapHeight * 4];
    GetBitmapBits2( hBitmap, bitmapWidth, bitmapHeight, pBitmapBits );

	unsigned char *pRGBA = new unsigned char[w * h * 4];
	memset( pRGBA, 0, w*h*4 );

	// copy out bits
	unsigned char *pDst = pRGBA;
	for (int yy=y; yy<y+h; yy++)
	{
		if ( yy >= bitmapHeight )
		{
			// past last row of bitmap
			break;
		}

		unsigned char *pSrc = pBitmapBits + (yy*bitmapWidth + x)*4;
		for (int xx=0; xx<w; xx++)
		{
			if ( xx + x < bitmapWidth )
			{
				pDst[0] = pSrc[0];
				pDst[1] = pSrc[1];
				pDst[2] = pSrc[2];
				pDst[3] = pSrc[3];
			}
			pSrc += 4;
			pDst += 4;
		}
	}

	delete [] pBitmapBits;
	return pRGBA;
}


int		g_blur;
float	*g_pGaussianDistribution;

//-----------------------------------------------------------------------------
// Purpose: Gets the blur value for a single pixel
//-----------------------------------------------------------------------------
void GetBlurValueForPixel(unsigned char *src, int blur, float *gaussianDistribution, int srcX, int srcY, int srcWide, int srcTall, unsigned char *dest)
{
	int r = 0, g = 0, b = 0, a = 0;
	
	float accum = 0.0f;

	// scan the positive x direction
	int maxX = min(srcX + blur, srcWide);
	int minX = max(srcX - blur, 0);
	for (int x = minX; x <= maxX; x++)
	{
		int maxY = min(srcY + blur, srcTall - 1);
		int minY = max(srcY - blur, 0);
		for (int y = minY; y <= maxY; y++)
		{
			unsigned char *srcPos = src + ((x + (y * srcWide)) * 4);

			unsigned char red = srcPos[2];
			unsigned char green = srcPos[1];
			unsigned char blue = srcPos[0];

			// muliply by the value matrix
			float weight = gaussianDistribution[x - srcX + blur];
			float weight2 = gaussianDistribution[y - srcY + blur];
			accum += (red * (weight * weight2));
		}
	}

	// blurring decreased the range, for xbox kick some back
	accum *= 1.30f;

	// all the values are the same for fonts, just use the calculated alpha
	r = g = b = (int)accum;

	dest[0] = min(b, 255);
	dest[1] = min(g, 255);
	dest[2] = min(r, 255);
}

//-----------------------------------------------------------------------------
// ApplyGaussianBlurToTexture
//-----------------------------------------------------------------------------
void ApplyGaussianBlurToTexture( int blur, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba)
{
	// calculate our gaussian distribution for if we're blurred
	if ( blur > 1 && blur != g_blur )
	{
		g_blur = blur;
		g_pGaussianDistribution = new float[blur * 2 + 1];
		double sigma = 0.683f * blur;
		for (int x = 0; x <= (blur * 2); x++)
		{
			int val = x - blur;
			g_pGaussianDistribution[x] = (float)((1.0 / sqrt(2.0 * 3.14 * sigma * sigma)) * pow(2.7, -1.0 * (val * val) / (2.0 * sigma * sigma)));

			// brightening factor
			g_pGaussianDistribution[x] *= 1;
		}
	}

	// alloc a new buffer
	unsigned char *src = (unsigned char *)_alloca(rgbaWide * rgbaTall * 4);
	memcpy(src, rgba, rgbaWide * rgbaTall * 4);

	unsigned char *dest = rgba;
	for (int y = 0; y < rgbaTall; y++)
	{
		for (int x = 0; x < rgbaWide; x++)
		{
			// scan the source pixel
			GetBlurValueForPixel(src, blur, g_pGaussianDistribution, x, y, rgbaWide, rgbaTall, dest);

			// move to the next
			dest += 4;
		}
	}
}

//-----------------------------------------------------------------------------
// ApplyScanlineEffectToTexture
//-----------------------------------------------------------------------------
void ApplyScanlineEffectToTexture( int scanLines, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba)
{
	if (scanLines < 2)
		return;

	float scale;
	scale = 0;

	// darken all the areas except the scanlines
	for (int y = 0; y < rgbaTall; y++)
	{
		// skip the scan lines
		if (y % scanLines == 0)
			continue;

		DWORD *pBits = (DWORD*)&rgba[(rgbaX + ((y + rgbaY) * rgbaWide)) * 4];

		// darken the other lines
		for (int x = 0; x < rgbaWide; x++, pBits++)
		{
	        FLOAT r = ( ( 0x00ff0000 & pBits[0] ) >> 16L ) / 255.0f;
		    FLOAT g = ( ( 0x0000ff00 & pBits[0] ) >>  8L ) / 255.0f;
			FLOAT b = ( ( 0x000000ff & pBits[0] ) >>  0L ) / 255.0f;

			r *= scale;
			g *= scale;
			b *= scale;

			pBits[0] = (((int)(r * 255.0f))<<16) | (((int)(g * 255.0f))<<8) | ((int)(b * 255.0f));
			pBits[0] |= 0xFF000000;
		}
	}
}


//-----------------------------------------------------------------------------
// Name: RenderTTFGlyphs() 
// Desc: Draws the list of font glyphs in the scroll view
//-----------------------------------------------------------------------------
GLYPH_ATTR* CTextureFont::RenderTTFGlyphs( HFONT hFont, HBITMAP hBitmap,
                                      DWORD dwTextureWidth, DWORD dwTextureHeight, 
                                      BOOL bOutlineEffect, BOOL bShadowEffect,
									  int nScanlineEffect, int nBlurEffect,
									  BOOL bAntialias,
                                      BYTE* ValidGlyphs, DWORD dwNumGlyphs )
{
    // Create a DC
    HDC hDC = CreateCompatibleDC( NULL );

    // Associate the drawing surface
    SelectObject( hDC, hBitmap );

    // Create a clip region
    HRGN rgn = CreateRectRgn( 0, 0, dwTextureWidth, dwTextureHeight );
    SelectClipRgn( hDC, rgn );

    // Setup the DC for the font
    SetTextColor( hDC, COLOR_WHITE );
    SelectObject( hDC, hFont );
    SetTextAlign( hDC, TA_LEFT|TA_TOP|TA_UPDATECP );
    SetMapMode( hDC, MM_TEXT );
    SetBkMode( hDC, TRANSPARENT );

	if ( nScanlineEffect || nBlurEffect )
	{
		SetBkColor( hDC, COLOR_BLACK );

		if ( nBlurEffect < 2 )
			nBlurEffect = 2;
		if ( nScanlineEffect < 2 )
			nScanlineEffect = 2;
	}
	else
	{
		SetBkColor( hDC, COLOR_BLUE );
	}

    // Fill the background in blue
    RECT rect;
    SetRect( &rect, 0, 0, dwTextureWidth, dwTextureHeight );
    ExtTextOut( hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL );

    // Get the effective font height
    WCHAR str[2] = L"A";
    SIZE  size;
    GetTextExtentPoint32W( hDC, str, 1, &size );

    DWORD dwLeftOrigin = 1;
    DWORD dwTopOrigin  = 1;

    GLYPH_ATTR* pGlyphs = new GLYPH_ATTR[dwNumGlyphs];
	memset( pGlyphs, 0, dwNumGlyphs*sizeof( GLYPH_ATTR ) );
 
    // Loop through all printable character and output them to the bitmap..
    // Meanwhile, keep track of the corresponding tex coords for each character.
    DWORD x     = dwLeftOrigin;
    DWORD y     = dwTopOrigin;
    int   sx;
    int   sy;

	int numGlyphs = 0;

	for( DWORD i=0; i<65536; i++ )
    {
        if ( 0 == ValidGlyphs[i])
            continue;

        str[0] = (WCHAR)i;

        if ( i==0 && ValidGlyphs[i] == 1 )
		{
			// account for unprintable, but don't render it
			numGlyphs++;
			continue;
		}

        GetTextExtentPoint32W( hDC, str, 1, &size );

        // Get char width a different way
        int charwidth;
        GetCharWidth32( hDC, str[0], str[0], &charwidth );

        // Get the ABC widths for the letter
        ABC abc;
        if ( FALSE == GetCharABCWidthsW( hDC, str[0], str[0], &abc ) )
        {
            abc.abcA = 0;
            abc.abcB = size.cx;
            abc.abcC = 0;
        }

        int w = abc.abcB;
        int h = size.cy;

        // Determine padding for effects
		int left_padding = 0;
		int right_padding = 0;
		int top_padding = 0;
		int bottom_padding = 0;
		if ( bOutlineEffect || bShadowEffect )
		{
			if ( bOutlineEffect )
				left_padding = 1;

			if ( bOutlineEffect )
			{
				if ( bShadowEffect )
					right_padding = 2;
				else
					right_padding = 1;
			}
			else
			{
				if ( bShadowEffect )
					right_padding = 2;
				else
					right_padding = 0;
			}

			if ( bOutlineEffect )
				top_padding = 1;

			if ( bOutlineEffect )
			{
				if ( bShadowEffect )
					bottom_padding = 2;
				else
					bottom_padding = 1;
			}
			else
			{
				if ( bShadowEffect )
					bottom_padding = 2;
				else
					bottom_padding = 0;
			}
		}
		else if ( nBlurEffect )
		{
			left_padding = nBlurEffect;
			right_padding = nBlurEffect;
		}

        if ( ValidGlyphs[i] == 2 )
        {
            // Handle special characters
            // Advance to the next line, if necessary
            if ( x + h + left_padding + right_padding >= (int)dwTextureWidth )
            {
                x  = dwLeftOrigin;
                y += h + top_padding + bottom_padding + 1;
            }

            sx = x;
            sy = y;

            // Draw a square box for a placeholder for custom glyph graphics
            w = h + left_padding + right_padding;
            h = h + top_padding + bottom_padding;

            abc.abcA = 0;
            abc.abcB = w;
            abc.abcC = 0;

            RECT rect;
            SetRect( &rect, x, y, x+w, y+h );
            SetBkColor( hDC, COLOR_BLACK );
            ExtTextOut( hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL );
        }
        else
        {
            // Hack to adjust for Kanji
            if ( str[0] > 0x1000 )
            {
                w = h;
            }

			// Advance to the next line, if necessary
            if ( x + w + left_padding + right_padding + 1 >= (int)dwTextureWidth )
            {
                x  = dwLeftOrigin;
                y += h + top_padding + bottom_padding + 1;
            }

            sx = x;
            sy = y;

            // Adjust ccordinates to account for the leading edge
            if ( abc.abcA >= 0 )
                x += abc.abcA;
            else
                sx -= abc.abcA;

            // Hack to adjust for Kanji
            if ( str[0] > 0x1000 )
            {
                sx += abc.abcA;
            }

            // Add padding to the width and height
            w += left_padding + right_padding;
            h += top_padding + bottom_padding;
            abc.abcA -= left_padding;
            abc.abcB += left_padding + right_padding;
            abc.abcC -= right_padding;

			if ( bOutlineEffect || bShadowEffect )
			{
				if ( bOutlineEffect )
				{
					SetTextColor( hDC, COLOR_BLACK );
					MoveToEx( hDC, sx+0, sy+0, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+1, sy+0, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+2, sy+0, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+0, sy+1, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+2, sy+1, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+0, sy+2, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+1, sy+2, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					MoveToEx( hDC, sx+2, sy+2, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );

					if ( bShadowEffect )
					{
						MoveToEx( hDC, sx+3, sy+3, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					}
	                
					// Output the letter
					SetTextColor( hDC, COLOR_WHITE );
					MoveToEx( hDC, sx+1, sy+1, NULL ); ExtTextOutW( hDC, sx, sy, ETO_OPAQUE, NULL, str, 1, NULL );
				}
				else
				{
					if ( bShadowEffect )
					{
						SetTextColor( hDC, COLOR_BLACK );
						MoveToEx( hDC, sx+2, sy+2, NULL ); ExtTextOutW( hDC, 0, 0, ETO_OPAQUE, NULL, str, 1, NULL );
					}

					// Output the letter
					SetTextColor( hDC, COLOR_WHITE );
					MoveToEx( hDC, sx, sy, NULL ); ExtTextOutW( hDC, sx, sy, ETO_OPAQUE, NULL, str, 1, NULL );
				}
			}
			else if ( nBlurEffect )
			{
				// blur effect
				SetTextColor( hDC, COLOR_WHITE );
				MoveToEx( hDC, sx + nBlurEffect, sy, NULL ); 
				ExtTextOutW( hDC, sx, sy, ETO_OPAQUE, NULL, str, 1, NULL );

				// apply blur effect
				unsigned char *pBGRA = GetTextureBits( hBitmap, dwTextureWidth, dwTextureHeight, x, y, w, h );
				if ( pBGRA )
				{
					ApplyGaussianBlurToTexture( nBlurEffect, 0, 0, w, h, pBGRA );
					SetTextureBits( hBitmap, dwTextureWidth, dwTextureHeight, x, y, w, h, pBGRA );
					delete [] pBGRA;
				}
			}
			else
			{
				// normal, no effect
				// Output the letter
				SetTextColor( hDC, COLOR_WHITE );
				MoveToEx( hDC, sx, sy, NULL ); 
				ExtTextOutW( hDC, sx, sy, ETO_OPAQUE, NULL, str, 1, NULL );
			}

			// apply scanline effect
			if ( nScanlineEffect )
			{
				unsigned char *pBGRA = GetTextureBits( hBitmap, dwTextureWidth, dwTextureHeight, x, y, w, h );
				if ( pBGRA )
				{
					ApplyScanlineEffectToTexture( nScanlineEffect, 0, 0, w, h, pBGRA );
					SetTextureBits( hBitmap, dwTextureWidth, dwTextureHeight, x, y, w, h, pBGRA );
					delete [] pBGRA;
				}
			}

            // Hack for extended characters (like Kanji) that don't seem to report
            // correct ABC widths. In this case, use the width calculated from
            // drawing the glyph.
            if( str[0] > 0x1000 )
            {
                POINT pos;
                GetCurrentPositionEx( hDC, &pos );
                abc.abcB = pos.x - sx;

                if( abc.abcC < 0 )
                    abc.abcB -= abc.abcC;

                w = abc.abcB;
            }
        }

        // Store the glyph attributes
        pGlyphs[numGlyphs].x        = x;
        pGlyphs[numGlyphs].y        = y;
        pGlyphs[numGlyphs].w        = w;
        pGlyphs[numGlyphs].h        = h;
        pGlyphs[numGlyphs].a        = abc.abcA;
        pGlyphs[numGlyphs].b        = abc.abcB;
        pGlyphs[numGlyphs].c        = abc.abcC;
        pGlyphs[numGlyphs].fLeft    = ((FLOAT)(x+0)) / dwTextureWidth;
        pGlyphs[numGlyphs].fTop     = ((FLOAT)(y+0)) / dwTextureHeight;
        pGlyphs[numGlyphs].fRight   = ((FLOAT)(x+w)) / dwTextureWidth;
        pGlyphs[numGlyphs].fBottom  = ((FLOAT)(y+h)) / dwTextureHeight;
        numGlyphs++;

        // Advance the cursor to the next position
        x += w + 1;
    }

    SelectClipRgn( hDC, NULL );
    DeleteObject( rgn );
    DeleteDC( hDC );

    return pGlyphs;
}

//-----------------------------------------------------------------------------
// Name: RenderCustomGlyphs() 
// Desc: Draws the list of font glyphs in the scroll view
//-----------------------------------------------------------------------------
GLYPH_ATTR* CTextureFont::RenderCustomGlyphs( HBITMAP hBitmap )
{
	int		i;
	int		w;
	int		h;
	byte_t	*pTGAPixels;

	m_maxCustomCharHeight = 0;

	// Create a DC
    HDC hDC = CreateCompatibleDC( NULL );

    // Associate the drawing surface
    SelectObject( hDC, hBitmap );

    // Create a clip region
    HRGN rgn = CreateRectRgn( 0, 0, m_dwTextureWidth, m_dwTextureHeight );
    SelectClipRgn( hDC, rgn );

	// clear the background
	unsigned char *pBGRA = GetTextureBits( hBitmap, m_dwTextureWidth, m_dwTextureHeight, 0, 0, m_dwTextureWidth, m_dwTextureHeight );
	for (i=0; i<(int)(m_dwTextureHeight*m_dwTextureWidth); i++)
	{
		pBGRA[i*4+0] = 0xFF;
		pBGRA[i*4+1] = 0x00;
		pBGRA[i*4+2] = 0x00;
		pBGRA[i*4+3] = 0x00;
	}
	SetTextureBits( hBitmap, m_dwTextureWidth, m_dwTextureHeight, 0, 0, m_dwTextureWidth, m_dwTextureHeight, pBGRA );

	// build the glyph table
    GLYPH_ATTR* pGlyphs = new GLYPH_ATTR[m_dwNumGlyphs];
	memset( pGlyphs, 0, m_dwNumGlyphs*sizeof( GLYPH_ATTR ) );

	int x = 0;
	int y = 0;
	int maxHeight = 0;

	for( DWORD i=0; i<m_dwNumGlyphs; i++ )
    {
		if ( !i )
		{
			// account for null
			continue;
		}
		else if ( TL_Exists( m_pCustomGlyphFiles[i-1] ) )
		{
			TL_LoadTGA( m_pCustomGlyphFiles[i-1], &pTGAPixels, &w, &h );

			// convert to expected order
			for (int j=0; j<h*w; j++)
			{
				int r = pTGAPixels[j*4+0];
				int g = pTGAPixels[j*4+1];
				int b = pTGAPixels[j*4+2];

				pTGAPixels[j*4+0] = b;
				pTGAPixels[j*4+1] = g;
				pTGAPixels[j*4+2] = r;
			}
		}
		else 
		{
			// build a "bad" symbol
			pTGAPixels = (byte_t*)TL_Malloc( 32*32*4 );
			w = 32;
			h = 32;
			for (int j=0; j<32*32; j++)
			{
				pTGAPixels[j*4+0] = 0x00;
				pTGAPixels[j*4+1] = 0x00;
				pTGAPixels[j*4+2] = 0xFF;
				pTGAPixels[j*4+3] = 0xFF;
			}
		}

		if ( m_maxCustomCharHeight < h )
		{
			m_maxCustomCharHeight = h;
		}

		if ( maxHeight < h )
		{
			maxHeight = h;
		}

		if ( x + w > (int)m_dwTextureWidth )
		{
			// skip to new row
			y += maxHeight;
			x = 0;
			maxHeight = h;
		}

		SetTextureBits( hBitmap, m_dwTextureWidth, m_dwTextureHeight, x, y, w, h, pTGAPixels );
		TL_Free( pTGAPixels );

        // Store the glyph attributes
        pGlyphs[i].x        = x;
        pGlyphs[i].y        = y;
        pGlyphs[i].w        = w;
        pGlyphs[i].h        = h;
        pGlyphs[i].a        = 0;
        pGlyphs[i].b        = w;
        pGlyphs[i].c        = 0;
        pGlyphs[i].fLeft    = ((FLOAT)(x+0)) / m_dwTextureWidth;
        pGlyphs[i].fTop     = ((FLOAT)(y+0)) / m_dwTextureHeight;
        pGlyphs[i].fRight   = ((FLOAT)(x+w)) / m_dwTextureWidth;
        pGlyphs[i].fBottom  = ((FLOAT)(y+h)) / m_dwTextureHeight;

		x += w;
	}

    SelectClipRgn( hDC, NULL );
    DeleteObject( rgn );
    DeleteDC( hDC );
	delete [] pBGRA;

    return pGlyphs;
}

//-----------------------------------------------------------------------------
// Name: CalculateAndRenderGlyphs() 
// Desc: Draws the list of font glyphs
//-----------------------------------------------------------------------------
HRESULT CTextureFont::CalculateAndRenderGlyphs()
{
    // Create a bitmap
    HBITMAP hBitmap = CreateBitmap( m_dwTextureWidth, m_dwTextureHeight, 1, 32, NULL );

    if ( m_pGlyphs )
        delete[] m_pGlyphs;

	if ( m_pCustomFilename )
	{
		m_pGlyphs = RenderCustomGlyphs( hBitmap );
	}
	else
	{
		// Create a font
		if ( m_hFont )
		{
			DeleteObject( m_hFont );
		}

		if ( m_bAntialiasEffect )
		{
			m_LogFont.lfQuality = ANTIALIASED_QUALITY;
		}
		else
		{
			m_LogFont.lfQuality = NONANTIALIASED_QUALITY;
		}

		m_hFont = CreateFontIndirect( &m_LogFont );

		m_pGlyphs = RenderTTFGlyphs( 
									m_hFont, 
									hBitmap,
									m_dwTextureWidth,
									m_dwTextureHeight, 
                                    m_bOutlineEffect,
									m_bShadowEffect,
									m_nScanlines,
									m_nBlur,
									m_bAntialiasEffect,
                                    m_ValidGlyphs,
									m_dwNumGlyphs );
	}

    // Store the resulting bits
    if ( m_pBits )
        delete[] m_pBits;
    m_pBits = new DWORD[ m_dwTextureWidth * m_dwTextureHeight ];

	GetBitmapBits2( hBitmap, m_dwTextureWidth, m_dwTextureHeight, m_pBits );

    DeleteObject( hBitmap );

    return S_OK;
}
