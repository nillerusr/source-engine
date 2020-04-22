//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#if defined ( WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined( OSX )
#include <Carbon/Carbon.h>
#elif defined( LINUX )
//#error
#elif defined( _X360 )
#else
#error
#endif
#include "FontTextureCache.h"
#include "MatSystemSurface.h"
#include <vgui_surfacelib/BitmapFont.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include "bitmap/imageformat.h"
#include "vtf/vtf.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "pixelwriter.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CMatSystemSurface g_MatSystemSurface;
static int g_FontRenderBoundingBoxes = -1;

#define TEXTURE_PAGE_WIDTH	256
#define TEXTURE_PAGE_HEIGHT	256

// row size
int CFontTextureCache::s_pFontPageSize[FONT_PAGE_SIZE_COUNT] = 
{
	16,
	32,
	64,
	128,
	256,
};

static bool g_mat_texture_outline_fonts = false;
CON_COMMAND( mat_texture_outline_fonts, "Outline fonts textures." )
{
	g_mat_texture_outline_fonts = !g_mat_texture_outline_fonts;
	Msg( "mat_texture_outline_fonts: %d\n", g_mat_texture_outline_fonts );
	g_MatSystemSurface.ResetFontCaches();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontTextureCache::CFontTextureCache() 
	: m_CharCache(0, 256, CacheEntryLessFunc)
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontTextureCache::~CFontTextureCache()
{
}

//-----------------------------------------------------------------------------
// Purpose: Resets the cache
//-----------------------------------------------------------------------------
void CFontTextureCache::Clear()
{
	// remove all existing data
	m_CharCache.RemoveAll();
	m_PageList.RemoveAll();

	// reinitialize
	CacheEntry_t listHead = { 0, 0 };
	m_LRUListHeadIndex = m_CharCache.Insert(listHead);

	m_CharCache[m_LRUListHeadIndex].nextEntry = m_LRUListHeadIndex;
	m_CharCache[m_LRUListHeadIndex].prevEntry = m_LRUListHeadIndex;

	for (int i = 0; i < FONT_PAGE_SIZE_COUNT; ++i)
	{
		m_pCurrPage[i] = -1;
	}
	m_FontPages.SetLessFunc( DefLessFunc( vgui::HFont ) );
	m_FontPages.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: comparison function for cache entries
//-----------------------------------------------------------------------------
bool CFontTextureCache::CacheEntryLessFunc(CacheEntry_t const &lhs, CacheEntry_t const &rhs)
{
	if (lhs.font < rhs.font)
		return true;
	else if (lhs.font > rhs.font)
		return false;

	return (lhs.wch < rhs.wch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given char & font
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureForChar( vgui::HFont font, vgui::FontDrawType_t type, wchar_t wch, int *textureID, float **texCoords )
{
	// Ask for just one character
	return GetTextureForChars( font, type, &wch, textureID, texCoords, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given chars & font
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureForChars( vgui::HFont font, vgui::FontDrawType_t type, const wchar_t *wch, int *textureID, float **texCoords, int numChars )
{
	Assert( wch && textureID && texCoords );
	Assert( numChars >= 1 );

	if ( type == vgui::FONT_DRAW_DEFAULT )
	{
		type = g_MatSystemSurface.IsFontAdditive( font ) ? vgui::FONT_DRAW_ADDITIVE : vgui::FONT_DRAW_NONADDITIVE;
	}

	int typePage = (int)type - 1;
	typePage = clamp( typePage, 0, (int)vgui::FONT_DRAW_TYPE_COUNT - 1 );

	if ( FontManager().IsBitmapFont( font ) )
	{
		const int MAX_BITMAP_CHARS = 256;
		if ( numChars > MAX_BITMAP_CHARS )
		{
			// Increase MAX_BITMAP_CHARS
			Assert( 0 );
			return false;
		}
	
		for ( int i = 0; i < numChars; i++ )
		{
			static float	sTexCoords[ 4*MAX_BITMAP_CHARS ];
			CBitmapFont		*pWinFont;
			float			left, top, right, bottom;
			int				index;
			Page_t			*pPage;
			
			pWinFont = reinterpret_cast< CBitmapFont* >( FontManager().GetFontForChar( font, wch[i] ) );
			if ( !pWinFont )
			{
				// bad font handle
				return false;
			}

			// get the texture coords
			pWinFont->GetCharCoords( wch[i], &left, &top, &right, &bottom );
			sTexCoords[i*4 + 0] = left;
			sTexCoords[i*4 + 1] = top;
			sTexCoords[i*4 + 2] = right;
			sTexCoords[i*4 + 3] = bottom;

			// find font handle in our list of ready pages
			index = m_FontPages.Find( font );
			if ( index == m_FontPages.InvalidIndex() )
			{
				// not found, create the texture id and its materials
				index = m_FontPages.Insert( font );
				pPage = &m_FontPages.Element( index );

				for (int type = 0; type < FONT_DRAW_TYPE_COUNT; ++type )
				{
					pPage->textureID[type] = g_MatSystemSurface.CreateNewTextureID( false );
				}
				CreateFontMaterials( *pPage, pWinFont->GetTexturePage(), true );
			}

			texCoords[i] = &(sTexCoords[ i*4 ]);
			textureID[i] = m_FontPages.Element( index ).textureID[typePage];
		}
	}
	else
	{
		struct newPageEntry_t
		{
			int	page;	// The font page a new character will go in
			int	drawX;	// X location within the font page
			int	drawY;	// Y location within the font page
		};
		
		// Determine how many characters need to have their texture generated
		int numNewChars = 0;
		int maxNewCharTexels = 0;
		int totalNewCharTexels = 0;
		newChar_t		*newChars	= (newChar_t *)_alloca( numChars*sizeof( newChar_t ) );
		newPageEntry_t	*newEntries	= (newPageEntry_t *)_alloca( numChars*sizeof( newPageEntry_t ) );

		font_t *winFont = FontManager().GetFontForChar( font, wch[0] );
		if ( !winFont )
			return false;
		
		for ( int i = 0; i < numChars; i++ )
		{
			CacheEntry_t cacheItem;
			cacheItem.font = font;
			cacheItem.wch = wch[i];
			HCacheEntry cacheHandle = m_CharCache.Find( cacheItem );
			if ( ! m_CharCache.IsValidIndex( cacheHandle ) )
			{
				// All characters must come out of the same font
				if ( winFont != FontManager().GetFontForChar( font, wch[i] ) )
					return false;

				// get the char details
				int a, b, c;
				winFont->GetCharABCWidths( wch[i], a, b, c );
				int fontWide = max( b, 1 );
				int fontTall = max( winFont->GetHeight(), 1 );
				if ( winFont->GetUnderlined() )
				{
					fontWide += ( a + c );
				}

				// Get a texture to render into
				int page, drawX, drawY, twide, ttall;
				if ( !AllocatePageForChar( fontWide, fontTall, page, drawX, drawY, twide, ttall ) )
					return false;

				// accumulate data to pass to GetCharsRGBA below
				newEntries[	numNewChars ].page		= page;
				newEntries[	numNewChars ].drawX		= drawX;
				newEntries[	numNewChars ].drawY		= drawY;
				newChars[	numNewChars ].wch		= wch[i];
				newChars[	numNewChars ].fontWide	= fontWide;
				newChars[	numNewChars ].fontTall	= fontTall;
				newChars[	numNewChars ].offset	= 4*totalNewCharTexels;
				totalNewCharTexels += fontWide*fontTall;
				maxNewCharTexels = max( maxNewCharTexels, fontWide*fontTall );
				numNewChars++;

				// set the cache info
				cacheItem.page = page;

				// the 0.5 texel offset is done in CMatSystemTexture::SetMaterial() / CMatSystemSurface::StartDrawing()
				double adjust =  0.0f;

				cacheItem.texCoords[0] = (float)( (double)drawX / ((double)twide + adjust) );
				cacheItem.texCoords[1] = (float)( (double)drawY / ((double)ttall + adjust) );
				cacheItem.texCoords[2] = (float)( (double)(drawX + fontWide) / (double)twide );
				cacheItem.texCoords[3] = (float)( (double)(drawY + fontTall) / (double)ttall );

				m_CharCache.Insert(cacheItem);
				cacheHandle = m_CharCache.Find( cacheItem );
				Assert( m_CharCache.IsValidIndex( cacheHandle ) );
			}
			
			int page = m_CharCache[cacheHandle].page;
			textureID[i] = m_PageList[page].textureID[typePage];
			texCoords[i] = m_CharCache[cacheHandle].texCoords;
		}

		// Generate texture data for all newly-encountered characters
		if ( numNewChars > 0 )
		{

#ifdef _X360
			if ( numNewChars > 1 )
			{
				MEM_ALLOC_CREDIT();

				// Use the 360 fast path that generates multiple characters at once
				int newCharDataSize = totalNewCharTexels*4;
				CUtlBuffer newCharData( newCharDataSize, newCharDataSize, 0 );
				unsigned char *pRGBA = (unsigned char *)newCharData.Base();
				winFont->GetCharsRGBA( newChars, numNewChars, pRGBA );

				// Copy the data into our font pages
				for ( int i = 0; i < numNewChars; i++ )
				{
					newChar_t		& newChar	= newChars[i];
					newPageEntry_t	& newEntry	= newEntries[i];

					// upload the new sub texture 
					// NOTE: both textureIDs reference the same ITexture, so we're ok
					g_MatSystemSurface.DrawSetTexture( m_PageList[newEntry.page].textureID[typePage] );
					unsigned char *characterRGBA = pRGBA + newChar.offset;
					g_MatSystemSurface.DrawSetSubTextureRGBA( m_PageList[newEntry.page].textureID[typePage], newEntry.drawX, newEntry.drawY, characterRGBA, newChar.fontWide, newChar.fontTall );
				}
			}
			else
#endif
			{
				// create a buffer for new characters to be rendered into
				int nByteCount = maxNewCharTexels * 4;
				unsigned char *pRGBA = (unsigned char *)_alloca( nByteCount );

				// Generate characters individually
				for ( int i = 0; i < numNewChars; i++ )
				{
					newChar_t		& newChar	= newChars[i];
					newPageEntry_t	& newEntry	= newEntries[i];

					// render the character into the buffer
					Q_memset( pRGBA, 0, nByteCount );

					winFont->GetCharRGBA( newChar.wch, newChar.fontWide, newChar.fontTall, pRGBA );

					if ( g_mat_texture_outline_fonts )
					{
						int width = newChar.fontWide;
						int height = newChar.fontTall;

						CPixelWriter pixelWriter;
						pixelWriter.SetPixelMemory( IMAGE_FORMAT_RGBA8888, pRGBA, width * sizeof( BGRA8888_t ) );
						for( int x = 0; x < width; x++ )
						{
							pixelWriter.Seek( x, 0 );
							pixelWriter.WritePixel( 255, 0, 255, 255 );
							pixelWriter.Seek( x, height - 1 );
							pixelWriter.WritePixel( 255, 0, 255, 255 );
						}
						for( int y = 0; y < height; y++ )
						{
							if ( y < 4 || y > height - 4 )
							{
								pixelWriter.Seek( 0, y );
								pixelWriter.WritePixel( 255, 0, 255, 255 );
								pixelWriter.Seek( width - 1, y );
								pixelWriter.WritePixel( 255, 0, 255, 255 );
							}
						}
					}

					// upload the new sub texture 
					// NOTE: both textureIDs reference the same ITexture, so we're ok)
					g_MatSystemSurface.DrawSetTexture( m_PageList[newEntry.page].textureID[typePage] );
					g_MatSystemSurface.DrawSetSubTextureRGBA( m_PageList[newEntry.page].textureID[typePage], newEntry.drawX, newEntry.drawY, pRGBA, newChar.fontWide, newChar.fontTall );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates font materials
//-----------------------------------------------------------------------------
void CFontTextureCache::CreateFontMaterials( Page_t &page, ITexture *pFontTexture, bool bitmapFont )
{
	// The normal material
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetString( "$basetexture", pFontTexture->GetName() );
	IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( "__fontpage", pVMTKeyValues );
	pMaterial->Refresh();

	int typePageNonAdditive = (int)vgui::FONT_DRAW_NONADDITIVE-1;
	g_MatSystemSurface.DrawSetTextureMaterial( page.textureID[typePageNonAdditive], pMaterial );
	pMaterial->DecrementReferenceCount();

	// The additive material
	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetInt( "$additive", 1 );
	pVMTKeyValues->SetString( "$basetexture", pFontTexture->GetName() );
	pMaterial = g_pMaterialSystem->CreateMaterial( "__fontpage_additive", pVMTKeyValues );
 	pMaterial->Refresh();

	int typePageAdditive = (int)vgui::FONT_DRAW_ADDITIVE-1;
	if ( bitmapFont )
	{
		g_MatSystemSurface.DrawSetTextureMaterial( page.textureID[typePageAdditive], pMaterial );
	}
	else
	{
		g_MatSystemSurface.ReferenceProceduralMaterial( page.textureID[typePageAdditive], page.textureID[typePageNonAdditive], pMaterial );
	}
	pMaterial->DecrementReferenceCount();
}

//-----------------------------------------------------------------------------
// Computes the page size given a character height
//-----------------------------------------------------------------------------
int CFontTextureCache::ComputePageType( int charTall ) const
{
	for (int i = 0; i < FONT_PAGE_SIZE_COUNT; ++i)
	{
		if ( charTall < s_pFontPageSize[i] )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new page for a given character
//-----------------------------------------------------------------------------
bool CFontTextureCache::AllocatePageForChar(int charWide, int charTall, int &pageIndex, int &drawX, int &drawY, int &twide, int &ttall)
{
	// see if there is room in the last page for this character
	int nPageType = ComputePageType( charTall );
	if ( nPageType < 0 )
	{
		Assert( !"Font is too tall for texture cache of glyphs\n" );
		return false; 
	}
	
	pageIndex = m_pCurrPage[nPageType];

	int nNextX = 0;
	bool bNeedsNewPage = true;
	if ( pageIndex > -1 )
	{
		Page_t &page = m_PageList[ pageIndex ];

		nNextX = page.nextX + charWide;

		// make sure we have room on the current line of the texture page
		if ( nNextX > page.wide )
		{
			// move down a line
			page.nextX = 0;
			nNextX = charWide;
			page.nextY += page.tallestCharOnLine;
			page.tallestCharOnLine = charTall;
		}
		page.tallestCharOnLine = max( page.tallestCharOnLine, (short)charTall );

		bNeedsNewPage = ((page.nextY + page.tallestCharOnLine) > page.tall);
	}
	
	if ( bNeedsNewPage )
	{
		// allocate a new page
		pageIndex = m_PageList.AddToTail();
		Page_t &newPage = m_PageList[pageIndex];
		m_pCurrPage[nPageType] = pageIndex;

		for (int i = 0; i < FONT_DRAW_TYPE_COUNT; ++i )
		{
			newPage.textureID[i] = g_MatSystemSurface.CreateNewTextureID( true );
		}

		newPage.maxFontHeight = s_pFontPageSize[nPageType];
		newPage.wide = TEXTURE_PAGE_WIDTH;
		newPage.tall = TEXTURE_PAGE_HEIGHT;
		newPage.nextX = 0;
		newPage.nextY = 0;
		newPage.tallestCharOnLine = charTall;

		nNextX = charWide;

		static int nFontPageId = 0;
		char pTextureName[64];
		Q_snprintf( pTextureName, 64, "__font_page_%d", nFontPageId );
		++nFontPageId;

		MEM_ALLOC_CREDIT();
		ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( 
			pTextureName, 
			TEXTURE_GROUP_VGUI, 
			newPage.wide, 
			newPage.tall, 
			IMAGE_FORMAT_RGBA8888, 
			TEXTUREFLAGS_POINTSAMPLE | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY );

		CreateFontMaterials( newPage, pTexture );

		pTexture->DecrementReferenceCount();

		if ( IsPC() || !IsDebug() )
		{
			// clear the texture from the inital checkerboard to black
			// allocate for 32bpp format
			int nByteCount = TEXTURE_PAGE_WIDTH * TEXTURE_PAGE_HEIGHT * 4;
			unsigned char *pRGBA = (unsigned char *)_alloca( nByteCount );
			Q_memset( pRGBA, 0, nByteCount );

			int typePageNonAdditive = (int)(vgui::FONT_DRAW_NONADDITIVE)-1;
			g_MatSystemSurface.DrawSetTextureRGBA( newPage.textureID[typePageNonAdditive], pRGBA, newPage.wide, newPage.tall, false, false );
		}
	}

	// output the position
	Page_t &page = m_PageList[ pageIndex ];
	drawX = page.nextX;
	drawY = page.nextY;
	twide = page.wide;
	ttall = page.tall;

	// Update the next position to draw in
	page.nextX = nNextX + 1;
	return true;
}
