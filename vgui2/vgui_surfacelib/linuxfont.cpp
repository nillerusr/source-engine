//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "vgui_surfacelib/linuxfont.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <utlbuffer.h>
#include <fontconfig/fontconfig.h>
#include <freetype/ftbitmap.h>
#include "materialsystem/imaterialsystem.h"

#include "vgui_surfacelib/FontManager.h"
#include "FontEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FT_LOAD_FLAGS	0 //$ (FT_LOAD_TARGET_LIGHT)

namespace {

// Freetype uses a lot of fixed float values that are 26.6 splits of a 32 bit word.
// to make it an int, shift down the 6 bits and round up if the high bit of the 6
// bits was set.
inline int32_t FIXED6_2INT(int32_t x)   { return ( (x>>6) + ( (x&0x20) ? (x<0 ? -1 : 1) : 0) ); }
inline float   FIXED6_2FLOAT(int32_t x) { return (float)x / 64.0f; }
inline int32_t INT_2FIXED6(int32_t x)   { return x << 6; }

}

bool CLinuxFont::ms_bSetFriendlyNameCacheLessFunc = false;
CUtlRBTree< CLinuxFont::font_name_entry > CLinuxFont::m_FriendlyNameCache;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLinuxFont::CLinuxFont() :
	m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc),
	m_ExtendedKernedABCWidthsCache( 256, 0, &ExtendedKernedABCWidthsCacheLessFunc )
{
	m_face = NULL;
	m_faceValid = false;
	m_iTall = 0;
	m_iHeight = 0;
	m_iHeightRequested = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	if ( !ms_bSetFriendlyNameCacheLessFunc )
	{
		ms_bSetFriendlyNameCacheLessFunc = true;
		SetDefLessFunc( m_FriendlyNameCache );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLinuxFont::~CLinuxFont()
{
	if( m_faceValid )
	{
		FT_Done_Face( m_face );
		m_face = NULL;
		m_faceValid = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: build a map of friendly (char *) name to crazy ATSU bytestream, so we can ask for "Tahoma" and actually load it
//-----------------------------------------------------------------------------
void CLinuxFont::CreateFontList()
{
	if ( m_FriendlyNameCache.Count() > 0 ) 
		return;

	if(!FcInit()) 
		return;
    FcConfig *config;
    FcPattern *pat;
    FcObjectSet *os;
    FcFontSet *fontset;
    int i;
    char *file;
	const char *name;

    config = FcConfigGetCurrent();
    pat = FcPatternCreate();
    os = FcObjectSetCreate();
    FcObjectSetAdd(os, FC_FILE);
    FcObjectSetAdd(os, FC_FULLNAME);
    FcObjectSetAdd(os, FC_FAMILY);
    FcObjectSetAdd(os, FC_SCALABLE);
    fontset = FcFontList(config, pat, os);
    if(!fontset) 
		return;
    for(i = 0; i < fontset->nfont; i++) 
	{
        FcBool scalable;

        if ( FcPatternGetBool(fontset->fonts[i], FC_SCALABLE, 0, &scalable) == FcResultMatch && !scalable )
            continue;

        if ( FcPatternGetString(fontset->fonts[i], FC_FAMILY, 0, (FcChar8**)&name) != FcResultMatch )
			continue;
		if ( FcPatternGetString(fontset->fonts[i], FC_FILE, 0, (FcChar8**)&file) != FcResultMatch )
			continue;
		
		font_name_entry entry;
        entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
        entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
        Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
        Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
        m_FriendlyNameCache.Insert( entry );

		// substitute Vera Sans for Tahoma on X
		if ( !V_stricmp( name, "Bitstream Vera Sans" ) )
		{
			name = "Tahoma";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );

			name = "Verdana";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );

			name = "Lucidia Console";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );
		}
    }

    FcFontSetDestroy(fontset);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
}

static FcPattern* FontMatch(const char* type, FcType vtype, const void* value,
                            ...)
{
    va_list ap;
    va_start(ap, value);

    FcPattern* pattern = FcPatternCreate();

    for (;;)
	{
        FcValue fcvalue;
        fcvalue.type = vtype;
        switch (vtype) {
            case FcTypeString:
                fcvalue.u.s = (FcChar8*) value;
                break;
            case FcTypeInteger:
                fcvalue.u.i = (int) value;
                break;
            default:
                Assert(!"FontMatch unhandled type");
        }
        FcPatternAdd(pattern, type, fcvalue, 0);

        type = va_arg(ap, const char *);
        if (!type)
            break;
        // FcType is promoted to int when passed through ...
        vtype = static_cast<FcType>(va_arg(ap, int));
        value = va_arg(ap, const void *);
    };
    va_end(ap);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);

    return match;
}

bool CLinuxFont::CreateFromMemory(const char *windowsFontName, void *data, int datasize, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = flags & vgui::ISurface::FONTFLAG_ANTIALIAS;
	m_bUnderlined = flags & vgui::ISurface::FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & vgui::ISurface::FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & vgui::ISurface::FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = flags & vgui::ISurface::FONTFLAG_ROTARY;
	m_bAdditive = flags & vgui::ISurface::FONTFLAG_ADDITIVE;

	if ( !HushAsserts() )
	{
		// These flags are NYI in Linux right now.
		Assert( !m_bAntiAliased );
		Assert( !m_bUnderlined );
		Assert( !m_bAdditive );
	}

	Assert( !m_faceValid );
	FT_Error error = FT_New_Memory_Face( FontManager().GetFontLibraryHandle(), (FT_Byte *)data, datasize, 0, &m_face );
	if ( error ) 
	{
		// FT_Err_Unknown_File_Format?
		Msg( "FT_New_Memory_Face failed. font:%s error:%d\n", windowsFontName, error );
		return false;
	} 

	if ( m_face->charmap == NULL )
	{
		FT_Error error = FT_Select_Charmap( m_face, FT_ENCODING_APPLE_ROMAN );
		if ( error )
		{
			FT_Done_Face( m_face );
			m_face = NULL;

			Msg( "Font %s has no valid charmap\n", windowsFontName );
			return false;
		}
	}

	m_iHeightRequested = m_iTall;

	// Loop through until we get a height that is less than or equal to the requested height.
	//  We tried using the BBOX ascender / descender, but it was overly large compared to Windows.
	//  We also tried using the size metrics, but the ascender wasn't high enough and accents were cut off.
	//  Descender from size metrics was too low for fonts with no lower case characters.
	// Compromise: Use ascent from O' and descent from bbox. Diffs on textures indicate this is best choice.
	//  Used these command lines vars and convars to help beyond compare linux and windows:
	//    -precachefontintlchars / -enable_font_bounding_boxes / vgui_spew_fonts / mat_texture_save_fonts

	bool bFirstTimeThrough = true;
	int IncAmount = -1;

	for ( ;; )
	{
		bool SetPixelSizesFailed = false;

		FT_Error error = FT_Set_Pixel_Sizes( m_face, 0, m_iHeightRequested );
		if ( error )
		{
			SetPixelSizesFailed = true;

			// If FT_Set_Pixel_Sizes fails, it should be because we've got a fixed-size font.
			Assert( m_face->face_flags & FT_FACE_FLAG_FIXED_SIZES );
			Assert( !( m_face->face_flags & FT_FACE_FLAG_SCALABLE ) );

			if ( m_face->num_fixed_sizes )
			{
				// Pick width/height of first size.
				int width = m_face->available_sizes[ 0 ].width;
				m_iHeightRequested = m_face->available_sizes[ 0 ].height;

				// Loop through all the other available sizes and find the closest match.
				for ( int i = 1; i < m_face->num_fixed_sizes; i++ )
				{
					if ( ( m_face->available_sizes[ i ].height <= m_iTall ) && 
						( m_face->available_sizes[ i ].height > m_iHeightRequested ) )
					{
						width = m_face->available_sizes[ i ].width;
						m_iHeightRequested = m_face->available_sizes[ i ].height;
					}
				}

				FT_Size_RequestRec req;

				Q_memset( &req, 0, sizeof( req ) );
				req.type           = FT_SIZE_REQUEST_TYPE_REAL_DIM;
				req.width          = INT_2FIXED6( width );
				req.height         = INT_2FIXED6( m_iHeightRequested );
				req.horiResolution = 0;
				req.vertResolution = 0;

				error = FT_Request_Size( m_face, &req );
				if ( error )
				{
					Msg( "FT_Request_Size failed on %s / %s\n",
						 m_face->family_name ? m_face->family_name : "??",
						 m_face->style_name ? m_face->style_name : "??" );
				}
			}
		}

		FT_Pos ascender, descender;

		if( SetPixelSizesFailed )
		{
			// If SetPixelSizesFailed failed, then we've hopefully got a fixed size
			//	font, and we can just use the metrics.
			ascender = m_face->size->metrics.ascender;
			descender = m_face->size->metrics.descender;
		}
		else
		{
			// Full bounding box ascent and descent:
			//   ( a * b ) / 0x10000. y_scale is 16.16.
			//$ ascender = FT_MulFix( m_face->bbox.yMax, m_face->size->metrics.y_scale );
			descender = FT_MulFix( m_face->bbox.yMin, m_face->size->metrics.y_scale );

			// Metrics ascent and descent
			ascender = m_face->size->metrics.ascender;
			//$ descender = m_face->size->metrics.descender;

			// While running with Spanish, the m_face->size->metrics.ascender is less
			// than the bitmap_top for the character.  This makes GetCharRGBA() chop off
			// the top of the O and the accent is skipped.  Complete hack here, but we
			// check for the tallest character we know about (O') and bump up the ascender
			// value if it is greater than what we've currently got.
			wchar_t ch = 0xd3;
			error = FT_Load_Char( m_face, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL); 
			if ( !error )
			{
				int glyph_index = FT_Get_Char_Index( m_face, ch );
				error = FT_Load_Glyph( m_face, glyph_index, FT_LOAD_RENDER );
				if ( !error )
				{
					FT_GlyphSlot slot = m_face->glyph;
					FT_Pos ascenderTop = INT_2FIXED6( slot->bitmap_top );

					if( ascenderTop > ascender )
					{
						ascender = ascenderTop;
					}
					else if ( !slot->bitmap.rows || !slot->bitmap.width )
					{
						// We didn't find an O' character in this font: use the full BBox.
						ascender = FT_MulFix( m_face->bbox.yMax, m_face->size->metrics.y_scale );
					}
				}
			}
		}

		m_iAscent = FIXED6_2INT( ascender );

		m_iMaxCharWidth = FIXED6_2INT( m_face->size->metrics.max_advance );

		const int fxpHeight = ascender + -descender + INT_2FIXED6( m_iDropShadowOffset + 2 * m_iOutlineSize );
		m_iHeight = FIXED6_2INT( fxpHeight );

		// If we're exact or we got too small, bail.
		if ( SetPixelSizesFailed || ( m_iHeight == m_iTall ) || ( m_iHeight < 7 ) || ( m_iHeightRequested <= 1 ) )
			break;

		if( bFirstTimeThrough )
		{
			bFirstTimeThrough = false;

			// If we're smaller than requested, start searching up.
			if ( m_iHeight < m_iTall )
				IncAmount = +1;
		}
		else if( IncAmount > 0 )
		{
			// If we're searching up and went too far, drop IncAmount and run down.
			if( m_iHeight > m_iTall )
				IncAmount = -1;
		}
		else if ( m_iHeight <= m_iTall )
		{
			// If the height is less than tall, we're done.
			break;
		}

		m_iHeightRequested += IncAmount;
	}

	m_faceValid = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Given a font name from windows, match it to the filename and return that.
//-----------------------------------------------------------------------------
char *CLinuxFont::GetFontFileName( const char *windowsFontName, int flags )
{
	bool bBold = false;
	const char *pchFontName = windowsFontName;

	if ( !Q_stricmp( pchFontName, "Tahoma" ) )
		pchFontName = "Bitstream Vera Sans";
	else if ( !Q_stricmp( pchFontName, "Arial Black" ) || Q_stristr( pchFontName, "bold" ) )
		bBold = true;

    const int italic = ( flags & vgui::ISurface::FONTFLAG_ITALIC ) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
	const int nFcWeight = bBold ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL;

    FcPattern *match = FontMatch( FC_FAMILY, FcTypeString, pchFontName,
								  FC_WEIGHT, FcTypeInteger, nFcWeight,
								  FC_SLANT, FcTypeInteger, italic,
								  NULL);
 	if ( !match )
    {
		AssertMsg1( false, "Unable to find font named %s\n", windowsFontName );
        return NULL;
    }
	else
	{
		char *filenameret = NULL;
		FcChar8* filename = NULL;

		if ( FcPatternGetString(match, FC_FILE, 0, &filename) != FcResultMatch )
		{
			AssertMsg1( false, "Unable to find font named %s\n", windowsFontName );
		}
		else
		{
			filenameret = strdup( ( char * )filename );
		}

		FcPatternDestroy( match );
		return filenameret;
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void CLinuxFont::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *prgba )
{
	bool bShouldAntialias = m_bAntiAliased;

	// filter out 
	if ( ( ch > 0x00FF ) && !( m_iFlags & vgui::ISurface::FONTFLAG_CUSTOM ) )
	{
		bShouldAntialias = false;
	}
	
	FT_Error error = FT_Load_Char( m_face, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL ); 
	if ( error )
	{
		Msg( "Error in FT_Load_Char: ch:%x error:%x\n", (int)ch, error );
		return;
	}

	int glyph_index = FT_Get_Char_Index( m_face, ch );
	error = FT_Load_Glyph( m_face, glyph_index, FT_LOAD_RENDER | FT_LOAD_FLAGS );
	if ( error )
	{
		Msg( "Error in FL_Load_Glyph: glyph_index:%d error:%x\n", glyph_index, error );
		return;
	}

	int yBitmapStart = 0;
	FT_GlyphSlot slot = m_face->glyph;
	int nSkipRows = ( m_iAscent - slot->bitmap_top );

	if( nSkipRows < 0 )
	{
		yBitmapStart = -nSkipRows;
		nSkipRows = 0;
	}
	if ( nSkipRows >= rgbaTall )
	{
		Msg( "nSkipRows(%d) > rgbaTall(%d) ch:%d\n", nSkipRows, rgbaTall, (int)ch );
		return;
	}

	if ( m_face->glyph->bitmap.width == 0 )
	{
		Msg( "m_face->glyph->bitmap.width is 0 for ch:%d %s\n", (int)ch, m_face->family_name ? m_face->family_name : "??" );
		return;
	}

	FT_Bitmap bitmap;
	FT_Library ftLibrary = FontManager().GetFontLibraryHandle();

	FT_Bitmap_New( &bitmap );

	error = FT_Bitmap_Convert( ftLibrary, &m_face->glyph->bitmap, &bitmap, 1 );
	if( error == 0 )
	{
		uint32 alpha_scale = 1;
		int Width = min( rgbaWide, bitmap.width );
		unsigned char *rgba = prgba + ( nSkipRows * rgbaWide * 4 );

		switch( m_face->glyph->bitmap.pixel_mode )
		{
		case FT_PIXEL_MODE_MONO: // 8-bit per pixel bitmap
			alpha_scale *= 256;
			break;
		case FT_PIXEL_MODE_GRAY2: // 2-bit per pixel bitmap
			alpha_scale *= 64;
			break;
		case FT_PIXEL_MODE_GRAY4: // 4-bit per pixel bitmap
			alpha_scale *= 16;
			break;
		}

		/* now draw to our target surface */
		for ( int y = yBitmapStart; y < MIN( bitmap.rows, rgbaTall - nSkipRows ); y++ )
		{
			for ( int x = 0; x < Width; x++ )
			{
				int rgbaOffset = 4 * ( x + m_iBlur ); // +(rgbaTall-y-1)*rgbaWide*4
				uint32 alpha = Min( 255U, alpha_scale * bitmap.buffer[ x + y * bitmap.pitch ] );

				rgba[ rgbaOffset + 0 ] =  255;
				rgba[ rgbaOffset + 1 ] =  255;
				rgba[ rgbaOffset + 2 ] =  255;
				rgba[ rgbaOffset + 3 ] =  alpha;
			}
			rgba += ( rgbaWide * 4 );
		}

		// apply requested effects in specified order
		ApplyDropShadowToTexture( rgbaWide, rgbaTall, prgba, m_iDropShadowOffset );
		ApplyOutlineToTexture( rgbaWide, rgbaTall, prgba, m_iOutlineSize );
		ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, prgba, m_iBlur );
		ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, prgba, m_iScanLines );
		ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, prgba, m_bRotary );
	}
	else
	{
		Msg( "FT_Bitmap_Convert failed: %d on %s\n", error, m_face->family_name ? m_face->family_name : "??" );
	}

	FT_Bitmap_Done( ftLibrary, &bitmap );
}

void CLinuxFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	abcA = abcC = wide = 0.0f;

	// look for it in the cache
	kerned_abc_cache_t finder = { ch, chBefore, chAfter };
	
	unsigned short iKerned = m_ExtendedKernedABCWidthsCache.Find(finder);
	if (m_ExtendedKernedABCWidthsCache.IsValidIndex(iKerned))
	{
		abcA = 0; //$ NYI. m_ExtendedKernedABCWidthsCache[iKerned].abc.abcA;
		abcC = 0; //$ NYI. m_ExtendedKernedABCWidthsCache[iKerned].abc.abcC;
		wide = m_ExtendedKernedABCWidthsCache[iKerned].abc.wide;
		return;
	}

    FT_UInt       glyph_index;
	FT_Bool       use_kerning;
	FT_UInt       previous;
	int32_t       iFxpPenX;
	 
	iFxpPenX = 0;
	wide = 0;

	use_kerning = FT_HAS_KERNING( m_face );
	previous    = chBefore;
	
	/* convert character code to glyph index */
	glyph_index = FT_Get_Char_Index( m_face, ch );
	
	/* retrieve kerning distance and move pen position */
	if ( use_kerning && previous && glyph_index )
	{
		FT_Vector  delta;
		 
		FT_Get_Kerning( m_face, previous, glyph_index,
						FT_KERNING_DEFAULT, &delta );
		 
		iFxpPenX += delta.x;
	}
	 
	/* load glyph image into the slot (erase previous one) */
	int error = FT_Load_Glyph( m_face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_FLAGS );
	if ( error )
	{
		Error( "Error in FL_Load_Glyph: glyph_index:%d ch:%x error:%x\n", glyph_index, (int)ch, error );
	}
	 
	FT_GlyphSlot slot = m_face->glyph;
	iFxpPenX += slot->advance.x;
	 
	if ( FIXED6_2INT(iFxpPenX) > wide )
		wide = FIXED6_2INT(iFxpPenX);
	
	//$ NYI: finder.abc.abcA = abcA;
	//$ NYI: finder.abc.abcC = abcC;
	finder.abc.wide = wide;
	m_ExtendedKernedABCWidthsCache.Insert(finder);
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CLinuxFont::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	Assert(IsValid());

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

	a = b = c = 0;

	FT_Error error = FT_Load_Char( m_face, ch, 0 ); 
	if ( error )
	{
		Msg( "Error in FT_Load_Char: ch:%x error:%x\n", ch, error );
		return;
	}

	// width: The glyph's width.
	// horiBearingX: Left side bearing for horizontal layout.
	// horiAdvance: Advance width for horizontal layout.
	FT_Glyph_Metrics metrics = m_face->glyph->metrics;

	finder.abc.a = metrics.horiBearingX / 64 - m_iBlur - m_iOutlineSize;
	finder.abc.b = metrics.width / 64 + ( ( m_iBlur + m_iOutlineSize ) * 2 ) + m_iDropShadowOffset;
	finder.abc.c = ( metrics.horiAdvance  - metrics.horiBearingX - metrics.width ) / 64 - m_iBlur - m_iDropShadowOffset - m_iOutlineSize;

	m_ExtendedABCWidthsCache.Insert( finder );

	a = finder.abc.a;
	b = finder.abc.b;
	c = finder.abc.c;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CLinuxFont::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if (!Q_stricmp(windowsFontName, m_szName.String() ) 
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
bool CLinuxFont::IsValid()
{
	if ( !m_szName.IsEmpty() )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int CLinuxFont::GetHeight()
{
	assert(IsValid());
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the requested height of the font
//-----------------------------------------------------------------------------
int CLinuxFont::GetHeightRequested()
{
	assert(IsValid());
	return m_iHeightRequested;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int CLinuxFont::GetAscent()
{
	assert(IsValid());
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int CLinuxFont::GetMaxCharWidth()
{
	assert(IsValid());
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int CLinuxFont::GetFlags()
{
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CLinuxFont::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CLinuxFont::ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs)
{
	return ( lhs.wch < rhs.wch ) ||
			( lhs.wch == rhs.wch && lhs.wchBefore < rhs.wchBefore ) ||
			( lhs.wch == rhs.wch && lhs.wchBefore == rhs.wchBefore && lhs.wchAfter < rhs.wchAfter );
}

void *CLinuxFont::SetAsActiveFont( void *cglContext )
{
	Assert( false );
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if this font has a glyph for the code point.
//-----------------------------------------------------------------------------
bool CLinuxFont::HasChar(wchar_t wch)
{
    return FT_Get_Char_Index( m_face, wch ) != 0;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CLinuxFont::Validate( CValidator &validator, const char *pchName )
{
	validator.Push( "CLinuxFont", this, pchName );

	m_ExtendedABCWidthsCache.Validate( validator, "m_ExtendedABCWidthsCache" );
	m_ExtendedKernedABCWidthsCache.Validate( validator, "m_ExtendedKernedABCWidthsCache" );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE
