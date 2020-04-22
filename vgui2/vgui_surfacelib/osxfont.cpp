//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <math.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>

#include "vgui_surfacelib/osxfont.h"
#include "FontEffects.h"

struct MetricsTweaks_t
{
	const char *m_windowsFontName;
	int m_sizeAdjust;
	float m_ascentMultiplier;
	float m_descentMultiplier;
	float m_leadingMultiplier;
};

//94: HFont:0x000000b9, TFTypeDeath, TFTypeDeathClientScheme-no, font:tfd, tall:27(28)
//95: HFont:0x000000ba, TFTypeDeath, TFTypeDeathClientScheme-p, font:tfd, tall:55(59)

static const MetricsTweaks_t g_defaultMetricTweaks = { NULL, 0, 1.0, 1.0, 1.0 };

static MetricsTweaks_t g_FontMetricTweaks[] =
{
	{ "Helvetica", 0, 1.0, 1.0, 1.05 },
	{ "Helvetica Bold", 0, 1.0, 1.0, 1.0 },
	{ "HL2cross", 0, 0.8, 1.0, 1.1},
	{ "Counter-Strike Logo", 0, 1.0, 1.0, 1.1 },
	{ "TF2", -2, 1.0, 1.0, 1.0 },
	{ "TF2 Professor", -2, 1.0, 2.0, 1.1 },
	{ "TF2 Build", -2, 1.0, 1.0, 1.0 },
	{ "Stubble bold", -6, 1.3, 1.0, 1.0 },
	{ "tfd", 0, 1.5, 1.0, 1.0 }, // "TFTypeDeath"
	//{ "TF2 Secondary", -2, 1.0, 1.0, 1.0 },
//	{ "Verdana", 0, 1.25, 1.0, 1.0 },
};

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef STAGING_ONLY
#define CHECK_ATSU_ERR( err ) if ( (err) != noErr ) Msg( "COSXFont::%s (%d) ATSU Error: %d\n", __FUNCTION__, __LINE__, err );
#else
#define CHECK_ATSU_ERR( err )
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COSXFont::COSXFont() : m_ExtendedABCWidthsCache( 256, 0, &ExtendedABCWidthsCacheLessFunc ),
						m_ExtendedKernedABCWidthsCache( 256, 0, &ExtendedKernedABCWidthsCacheLessFunc )
{
	m_iTall = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_bAntiAliased = false;
	m_bRotary = false;
	m_bAdditive = false;
	m_iDropShadowOffset;
	m_bUnderlined = false;
	m_iOutlineSize = 0;

	m_iHeight = 0;
	m_iMaxCharWidth = 0;
	m_iAscent = 0;

	m_iScanLines = 0;
	m_iBlur = 0;
	m_pGaussianDistribution = NULL;

	m_ATSUFont = kATSFontRefUnspecified;
	m_pContextMemory = NULL;
	m_ContextRef = 0;
	m_ATSUStyle = NULL;
	m_ATSUTextLayout = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COSXFont::~COSXFont()
{
	if ( m_ContextRef )
		CGContextRelease( m_ContextRef );

	if ( m_pContextMemory )
		delete [] m_pContextMemory;

	if ( m_ATSUStyle )
		ATSUDisposeStyle( m_ATSUStyle );

	if ( m_ATSUTextLayout )
		ATSUDisposeTextLayout( m_ATSUTextLayout );
}

bool COSXFont::CreateStyle( float flFontSize, bool bBold )
{
	OSStatus err = ATSUCreateStyle( &m_ATSUStyle );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return false;
	}

	Boolean isBold = bBold;
	Boolean isUnderlined = m_bUnderlined;
	float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Fixed fontsize = FloatToFixed( flFontSize );  // font size is based on 72dpi and not pixels, so convert to pixels
	Boolean isItalic = m_iFlags & vgui::ISurface::FONTFLAG_ITALIC;
	ATSStyleRenderingOptions renderOpt = kATSStyleNoOptions;

	renderOpt |= ( m_bAntiAliased ? kATSStyleApplyAntiAliasing : kATSStyleNoAntiAliasing );

	const ATSUAttributeTag styleTags[] =
	{
	    kATSUFontTag,
	    kATSUSizeTag,
	    kATSUQDItalicTag,
	    kATSUQDBoldfaceTag,
	    kATSUStyleRenderingOptionsTag,
	    kATSUQDUnderlineTag,
	    kATSURGBAlphaColorTag,
	};

	const ATSUAttributeValuePtr styleValues[] =
	{
	    &m_ATSUFont,
	    &fontsize,
	    &isItalic,
	    &isBold,
	    &renderOpt,
	    &isUnderlined,
	    &color,
	};

	const ByteCount styleSizes[] =
	{
	    sizeof( ATSUFontID ),
	    sizeof( Fixed ),
	    sizeof( Boolean ),
	    sizeof( Boolean ),
	    sizeof( renderOpt ),
	    sizeof( Boolean ),
	    sizeof( color ),
	};

	err = ATSUSetAttributes( m_ATSUStyle, Q_ARRAYSIZE( styleTags ), styleTags, styleSizes, styleValues );
	if (err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return false;
	}

	return true;
}

bool COSXFont::CreateTextLayout()
{
	OSStatus err = ATSUCreateTextLayout( &m_ATSUTextLayout );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return false;
	}

	Fract alignment = kATSUStartAlignment;
	ATSUTextMeasurement lineWidth = IntToFixed( 32000 );
	ATSLineLayoutOptions layoutOptions = kATSLineUseDeviceMetrics | kATSLineDisableAllLayoutOperations;

	ATSUAttributeTag theTags[] =
	{
	    kATSUCGContextTag,
	    kATSULineWidthTag,
	    kATSULineFlushFactorTag,
	    kATSULineLayoutOptionsTag,
	};
	ByteCount theSizes[] =
	{
	    sizeof( CGContextRef ),
	    sizeof( ATSUTextMeasurement ),
	    sizeof( Fract ),
	    sizeof( ATSLineLayoutOptions ),
	};
	ATSUAttributeValuePtr theValues[] =
	{
	    &m_ContextRef,
	    &lineWidth,
	    &alignment,
	    &layoutOptions,
	};

	err = ATSUSetLayoutControls( m_ATSUTextLayout, Q_ARRAYSIZE( theTags ), theTags, theSizes, theValues );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: creates the font from windows.  returns false if font does not exist in the OS.
//-----------------------------------------------------------------------------
bool COSXFont::Create( const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	// setup font properties
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bAntiAliased = flags & vgui::ISurface::FONTFLAG_ANTIALIAS;
	m_bUnderlined = flags & vgui::ISurface::FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = ( flags & vgui::ISurface::FONTFLAG_DROPSHADOW ) ? 1 : 0;
	m_iOutlineSize = ( flags & vgui::ISurface::FONTFLAG_OUTLINE ) ? 1 : 0;
	m_bRotary = flags & vgui::ISurface::FONTFLAG_ROTARY;
	m_bAdditive = flags & vgui::ISurface::FONTFLAG_ADDITIVE;

	m_ATSUFont = kATSFontRefUnspecified;
	CFStringRef fontName = CFStringCreateWithCString( kCFAllocatorDefault, windowsFontName, kCFStringEncodingUTF8 );
	if ( fontName )
	{
		m_ATSUFont = ATSFontFindFromPostScriptName( fontName, kATSOptionFlagsDefault );
		CFRelease( fontName );
	}

	if ( m_ATSUFont == kATSFontRefUnspecified )
	{
		CFStringRef fontName = CFStringCreateWithCString( kCFAllocatorDefault, windowsFontName, kCFStringEncodingUTF8 );
		if ( fontName )
		{
			m_ATSUFont = ATSFontFindFromName( fontName, kATSOptionFlagsDefault );
			CFRelease( fontName );
		}
	}

	if ( m_ATSUFont == kATSFontRefUnspecified )
	{
		CHECK_ATSU_ERR( -1 );
		return false;
	}

	ATSFontMetrics aMetrics;
	OSStatus err = ATSFontGetHorizontalMetrics( m_ATSUFont, kATSOptionFlagsDefault, &aMetrics );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return false;
	}

	MetricsTweaks_t metricTweaks = g_defaultMetricTweaks;
	for ( int i = 0; i < Q_ARRAYSIZE( g_FontMetricTweaks ); i++ )
	{
		if ( !Q_stricmp( windowsFontName, g_FontMetricTweaks[ i ].m_windowsFontName ) )
		{
			metricTweaks = g_FontMetricTweaks[ i ];
			break;
		}
	}

	bool bBold = ( !Q_stricmp( windowsFontName, "Arial Black" ) || Q_stristr( windowsFontName, "bold" ) );
	float flFontSize = ( (float)( m_iTall + metricTweaks.m_sizeAdjust ) / ( aMetrics.ascent - aMetrics.descent + aMetrics.leading ) );

	m_iAscent = ceil( ( aMetrics.ascent / ( aMetrics.ascent - aMetrics.descent + aMetrics.leading ) ) *
	                  ( m_iTall + metricTweaks.m_sizeAdjust ) * ( metricTweaks.m_ascentMultiplier ) );
	m_iHeight = ceil( ((float)( m_iTall + metricTweaks.m_sizeAdjust ) *
	                   ( aMetrics.ascent * metricTweaks.m_ascentMultiplier -
	                     aMetrics.descent * metricTweaks.m_descentMultiplier +
	                     aMetrics.leading * metricTweaks.m_leadingMultiplier ) +
	                   m_iDropShadowOffset + 2 * m_iOutlineSize ) );

	m_iMaxCharWidth = ( metricTweaks.m_leadingMultiplier * aMetrics.maxAdvanceWidth * m_iTall ) + 0.5f;

	unsigned int bytesPerRow = m_iMaxCharWidth * 4;

	m_pContextMemory = new char[ (int)bytesPerRow * m_iHeight ];
	Q_memset( m_pContextMemory, 0x0, (int)( bytesPerRow * m_iHeight ) );

	const size_t bitsPerComponent = 8;
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	m_ContextRef = CGBitmapContextCreate(
						  m_pContextMemory,
						  m_iMaxCharWidth,
						  m_iHeight,
						  bitsPerComponent,
						  bytesPerRow,
						  colorSpace,
						  kCGImageAlphaPremultipliedLast );

	CGColorSpaceRelease( colorSpace );

	CGContextSetAllowsAntialiasing( m_ContextRef, m_bAntiAliased );
	CGContextSetShouldAntialias( m_ContextRef, m_bAntiAliased );
	CGContextSetTextDrawingMode( m_ContextRef, kCGTextFill );
	CGContextSetRGBStrokeColor( m_ContextRef, 1.0f, 1.0f, 1.0f, 1.0f );
	CGContextSetLineWidth( m_ContextRef, 1 );
	
	// Calculate our gaussian distribution for if we're blurred.
	if ( m_iBlur > 1 )
	{
		m_pGaussianDistribution = new float[ m_iBlur * 2 + 1 ];
		double sigma = 0.683 * m_iBlur;

		for (int x = 0; x <= (m_iBlur * 2); x++)
		{
			int val = x - m_iBlur;
			m_pGaussianDistribution[x] = (float)(1.0f / sqrt(2 * 3.14 * sigma * sigma)) * pow(2.7, -1 * (val * val) / (2 * sigma * sigma));
		}
	}

	if ( !CreateStyle( flFontSize, bBold ) )
		return false;

	// Create our ATSUTextLayout object.
	if ( !CreateTextLayout() )
		return false;

	// Set our font name last as we use it to determine success (or not).
	m_szName = windowsFontName;
	return true;
}

void COSXFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA )
{
	// look for it in the cache
	kerned_abc_cache_t finder = { ch, chBefore, chAfter };

	wide = abcA = 0.0f;

	unsigned short iKerned = m_ExtendedKernedABCWidthsCache.Find( finder );
	if ( m_ExtendedKernedABCWidthsCache.IsValidIndex( iKerned ) )
	{
		abcA = m_ExtendedKernedABCWidthsCache[iKerned].abc.abcA;
		wide = m_ExtendedKernedABCWidthsCache[ iKerned ].abc.wide;
		return;
	}	

	if ( !m_ATSUStyle || ( ch == 0 ) )
		return;

	CFCharacterSetRef badCharSetRef = CFCharacterSetGetPredefined( kCFCharacterSetIllegal );

	uint32 i = 0;
	uint32 iTarget = 0;
	bool bBadChar = false;
	wchar_t wchString[ 4 ];

	if ( chBefore )
	{
		bBadChar = CFCharacterSetIsLongCharacterMember( badCharSetRef, chBefore );
		Assert( !bBadChar );
		if ( !bBadChar)
			wchString[ i++ ] = chBefore;
	}

	iTarget = i;
	bBadChar = CFCharacterSetIsLongCharacterMember( badCharSetRef, ch );
	Assert( !bBadChar );
	if ( bBadChar )
		return; // 0.0 width for bad characters.

	wchString[ i++ ] = ch;

	if ( chAfter )
	{
		bBadChar = CFCharacterSetIsLongCharacterMember( badCharSetRef, chAfter );
		Assert( !bBadChar );
		if ( !bBadChar)
			wchString[ i++ ] = chAfter;
	}

	wchString[ i ] = 0;

	CFStringRef convertedKey = CFStringCreateWithBytes( kCFAllocatorDefault, (const UInt8 *)wchString, i * sizeof(wchar_t), kCFStringEncodingUTF32LE, false );
	if ( !convertedKey )
		return;

	CFIndex usedBufLen = 0;
	char chUTF16Text[ Q_ARRAYSIZE( wchString ) * 2 ];

	CFStringGetBytes( convertedKey, CFRangeMake( 0, (int)i ), kCFStringEncodingUnicode, 0, false, (UInt8 *)chUTF16Text, Q_ARRAYSIZE( chUTF16Text ), &usedBufLen );
	CFRelease( convertedKey );

	OSStatus err = ATSUSetTextPointerLocation( m_ATSUTextLayout, (ConstUniCharArrayPtr)chUTF16Text, kATSUFromTextBeginning, kATSUToTextEnd, i );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetRunStyle( m_ATSUTextLayout, m_ATSUStyle, 0, i );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetTransientFontMatching( m_ATSUTextLayout, false );
	if ( err != noErr )
		CHECK_ATSU_ERR( err );

	ATSLayoutRecord *layoutRecords;
	ItemCount glyphCount;	

	layoutRecords = NULL;
	err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout( m_ATSUTextLayout, 0, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **) &layoutRecords, &glyphCount );
	CHECK_ATSU_ERR( err );
	if( err == noErr )
	{
		ATSGlyphIdealMetrics ScreenMetricts[ Q_ARRAYSIZE( wchString ) ];
		err = ATSUGlyphGetIdealMetrics( m_ATSUStyle, i, &layoutRecords[ 0 ].glyphID, sizeof( ATSLayoutRecord ), ScreenMetricts );
		CHECK_ATSU_ERR( err );
		if( err == noErr )
		{
			wide = ScreenMetricts[ iTarget ].advance.x;
			abcA = ScreenMetricts[ iTarget ].sideBearing.x - (float)m_iBlur - (float)m_iOutlineSize;
		}

		ATSUDirectReleaseLayoutDataArrayPtr( NULL, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void**) &layoutRecords );
	}

	if ( ( wide < 0.001f && abcA < 0.001f ) || ( ch == chAfter ) )
	{
		// For some fonts the kerning engine has an issue and considers the second character in a pair
		// (i.e the ee in bleed) to be zero width and abcA and the first char to be double width,
		// so in that case fall back to simple 1 char metrics.
		int a, b, c;
		GetCharABCWidths( ch, a, b, c );

		wide = a + b + c;
		abcA = a;
	}

	// printf( "GetKernedCharWidth: %c (%f, %f) (%c, %c)\n", (char)ch, wide, abcA, chBefore, chAfter );

	finder.abc.abcA = abcA;
	finder.abc.wide = wide;
	m_ExtendedKernedABCWidthsCache.Insert( finder );
}

static UniCharCount WcharToUnichar( wchar_t ch, UniChar (&dest)[ 3 ] )
{
	UniCharCount runLength;

	if ( ch <= 0xFFFF )
	{
		dest[ 0 ] = (UniChar)ch;
		dest[ 1 ] = 0;
		return 1;
	}

	ch -= 0x010000;
	dest[ 0 ] = (UniChar)( ch >> 10 ) | 0xD800;
	dest[ 1 ] = (UniChar)( ch & 0x3FF ) | 0xDC00;
	dest[ 2 ] = 0;
	return 2;
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void COSXFont::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *rgba )
{
	if ( !m_ContextRef )
	{
		Assert( !"Context ref not setup to allow GetCharRGBA" );
		return;
	}

	UniChar buffer[ 3 ];
	UniCharCount runLength = WcharToUnichar( ch, buffer );

	OSStatus err = ATSUSetTextPointerLocation( m_ATSUTextLayout, buffer, kATSUFromTextBeginning, kATSUToTextEnd, runLength );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetRunStyle( m_ATSUTextLayout, m_ATSUStyle, 0, runLength );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetTransientFontMatching( m_ATSUTextLayout, true );
	if ( err != noErr )
		CHECK_ATSU_ERR( err );

	CGRect rect = { { 0, 0 }, { m_iMaxCharWidth, m_iHeight } };
	CGContextClearRect( m_ContextRef, rect );

	CGContextFlush( m_ContextRef );

	// You are not seeing a bug.  We need the background to have a full
	// alpha channel since it's going to bitblt in the overlay.  But osx when it renders
	// to a full alpha channel likes to 'antialias' the alpha as well as the color blending.
	// Turning off antialiasing doesn't just make the characters jagged, but when a font
	// is 1 pixel wide it ends up alpha blending into the background.  By rendering it
	// more than once we dominate this alpha.  Looking at worst case, 3 was the magic number.
	// Messing with the anti-aliasing settings on the bitmap, or the alpha config (including
	// alpha-skip-last) caused no change, or complete disabling of antialiasing.  Glad these
	// are cached.
	if ( m_iHeight < 20 )
	{
		err = ATSUDrawText( m_ATSUTextLayout, kATSUFromTextBeginning, kATSUToTextEnd, Long2Fix( 0 ), Long2Fix( m_iHeight - m_iAscent ) );
		CHECK_ATSU_ERR( err );
	}

	if ( ( m_iHeight < 16 ) || ( m_iFlags & vgui::ISurface::FONTFLAG_CUSTOM ) )
	{
		err = ATSUDrawText( m_ATSUTextLayout, kATSUFromTextBeginning, kATSUToTextEnd, Long2Fix( 0 ), Long2Fix( m_iHeight - m_iAscent ) );
		CHECK_ATSU_ERR( err );
	}

	err = ATSUDrawText( m_ATSUTextLayout, kATSUFromTextBeginning, kATSUToTextEnd, Long2Fix( 0 ), Long2Fix( m_iHeight - m_iAscent ) );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}

	char *pContextData = (char *)CGBitmapContextGetData( m_ContextRef );

	unsigned char *pchPixelData = rgba;
	for ( int y = 0; y < rgbaTall; y++ )
	{
		char *row = pContextData + y * m_iMaxCharWidth * 4;
		Q_memcpy( pchPixelData, row, rgbaWide * 4 );
		pchPixelData+= ( rgbaWide * 4 );
	}

	// apply requested effects in specified order
	ApplyDropShadowToTexture( rgbaWide, rgbaTall, rgba, m_iDropShadowOffset );
	ApplyOutlineToTexture( rgbaWide, rgbaTall, rgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, rgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, rgba, m_bRotary );
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void COSXFont::GetCharABCWidths( int ch, int &a, int &b, int &c )
{
	Assert( IsValid() );

	// Look for it in the cache.
	abc_cache_t finder = { (wchar_t)ch };

	unsigned short i = m_ExtendedABCWidthsCache.Find( finder );
	if ( m_ExtendedABCWidthsCache.IsValidIndex( i ) )
	{
		a = m_ExtendedABCWidthsCache[i].abc.a;
		b = m_ExtendedABCWidthsCache[i].abc.b;
		c = m_ExtendedABCWidthsCache[i].abc.c;
		return;
	}

	UniChar buffer[ 3 ];
	UniCharCount runLength = WcharToUnichar( ch, buffer );

	OSStatus err = ATSUSetTextPointerLocation( m_ATSUTextLayout, buffer, kATSUFromTextBeginning, kATSUToTextEnd, runLength );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetRunStyle( m_ATSUTextLayout, m_ATSUStyle, 0, runLength );
	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	err = ATSUSetTransientFontMatching( m_ATSUTextLayout, true );
	if ( err != noErr )
		CHECK_ATSU_ERR( err );

	ItemCount glyphCount = 0;
	ATSLayoutRecord *layoutRecords;
	err = ATSUDirectGetLayoutDataArrayPtrFromTextLayout( m_ATSUTextLayout, 0, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, ( void ** )&layoutRecords, &glyphCount );
	if (err != noErr)
	{
		CHECK_ATSU_ERR( err );
		return;
	}
	ATSGlyphRef glyph = layoutRecords->glyphID;

	ATSGlyphScreenMetrics gm;
	err = ATSUGlyphGetScreenMetrics( m_ATSUStyle, 1, &glyph, 0, false, false, &gm );
	ATSUDirectReleaseLayoutDataArrayPtr( NULL, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, ( void ** )&layoutRecords );

	if ( err != noErr )
	{
		// ATSUGlyphGetScreenMetrics fails when font matching happens. When this occurs,
		//  grab the full bounding box for the character and try to fake up some abc values.
		ATSUTextMeasurement fTextBefore, fTextAfter, fAscent, fDescent;
		err = ATSUGetUnjustifiedBounds(	m_ATSUTextLayout,
		                                kATSUFromTextBeginning,
		                                kATSUToTextEnd,
		                                &fTextBefore,
		                                &fTextAfter,
		                                &fAscent,
		                                &fDescent);
		gm.sideBearing.x = 0.2f;
		gm.deviceAdvance.x = FixedToFloat( fTextAfter );
		gm.otherSideBearing.x = 0.2f;
	}

	if ( err != noErr )
	{
		CHECK_ATSU_ERR( err );
		return;
	}

	finder.abc.a = gm.sideBearing.x - (float)m_iBlur - m_iOutlineSize;
	finder.abc.b = gm.deviceAdvance.x + ( ( (float)m_iBlur + m_iOutlineSize ) * 2 ) + m_iDropShadowOffset;
	finder.abc.c = gm.otherSideBearing.x - (float)m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
	// finder.abc.a = ceil( finder.abc.a );
	// finder.abc.b = ceil( finder.abc.b );
	// finder.abc.c = ceil( finder.abc.c );

	if ( m_iFlags & vgui::ISurface::FONTFLAG_ITALIC )
	{
		finder.abc.b += 4.0f;
	}

	if ( finder.abc.a + finder.abc.b + finder.abc.c == 0 )
	{
		if ( finder.abc.b + finder.abc.c == 0 )
			finder.abc.c = 0;
		else if ( finder.abc.a + finder.abc.b == 0 )
			finder.abc.a = 0;
		else
			finder.abc.a = finder.abc.c = 0;
	}

	a = finder.abc.a;
	b = finder.abc.b;
	c = finder.abc.c;

	m_ExtendedABCWidthsCache.Insert( finder );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool COSXFont::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if ( !Q_stricmp( windowsFontName, m_szName.String() ) &&
		( m_iTall == tall ) &&
		( m_iWeight == weight ) &&
		( m_iBlur == blur ) &&
		( m_iFlags == flags ) )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool COSXFont::IsValid()
{
	if ( !m_szName.IsEmpty() && m_szName.String()[ 0 ] )
		return true;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int COSXFont::GetHeight()
{
	Assert( IsValid() );
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the requested height of the font
//-----------------------------------------------------------------------------
int COSXFont::GetHeightRequested()
{
	Assert( IsValid() );
	return m_iTall;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int COSXFont::GetAscent()
{
	Assert( IsValid() );
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int COSXFont::GetMaxCharWidth()
{
	Assert( IsValid() );
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int COSXFont::GetFlags()
{
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool COSXFont::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool COSXFont::ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs)
{
	return ( lhs.wch < rhs.wch ) ||
			( lhs.wch == rhs.wch && lhs.wchBefore < rhs.wchBefore ) ||
			( lhs.wch == rhs.wch && lhs.wchBefore == rhs.wchBefore && lhs.wchAfter < rhs.wchAfter );
}


ATSUStyle *COSXFont::SetAsActiveFont( CGContextRef cgContext )
{
	CGContextSelectFont ( cgContext, m_szName.String(), m_iHeight, kCGEncodingMacRoman );
	return &m_ATSUStyle;
}


#ifdef DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void COSXFont::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "COSXFont", this, pchName );

	m_ExtendedABCWidthsCache.Validate( validator, "m_ExtendedABCWidthsCache" );
	m_ExtendedKernedABCWidthsCache.Validate( validator, "m_ExtendedKernedABCWidthsCache" );
	validator.ClaimMemory( m_pGaussianDistribution );

	validator.Pop();
}

#endif // DBGFLAG_VALIDATE
