//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include "cmatlightmaps.h"

#include "colorspace.h"
#include "IHardwareConfigInternal.h"

#include "cmaterialsystem.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"
#include "bitmap/float_bm.h"

static ConVar mat_lightmap_pfms( "mat_lightmap_pfms", "0", FCVAR_MATERIAL_SYSTEM_THREAD, "Outputs .pfm files containing lightmap data for each lightmap page when a level exits." ); // Write PFM files for each lightmap page in the game directory when exiting a level 

#define USE_32BIT_LIGHTMAPS_ON_360 //uncomment to use 32bit lightmaps, be sure to keep this in sync with the same #define in stdshaders/lightmappedgeneric_ps2_3_x.h

#ifdef _X360
#define X360_USE_SIMD_LIGHTMAP
#endif

//-----------------------------------------------------------------------------

inline IMaterialInternal* CMatLightmaps::GetCurrentMaterialInternal() const
{
	return GetMaterialSystem()->GetRenderContextInternal()->GetCurrentMaterialInternal();
}

inline void CMatLightmaps::SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial)
{
	return GetMaterialSystem()->GetRenderContextInternal()->SetCurrentMaterialInternal( pCurrentMaterial );
}

inline IMaterialInternal *CMatLightmaps::GetMaterialInternal( MaterialHandle_t idx ) const
{
	return GetMaterialSystem()->GetMaterialInternal( idx );
}

inline const IMatRenderContextInternal *CMatLightmaps::GetRenderContextInternal() const
{
	return GetMaterialSystem()->GetRenderContextInternal();
}

inline IMatRenderContextInternal *CMatLightmaps::GetRenderContextInternal()
{
	return GetMaterialSystem()->GetRenderContextInternal();
}

inline const CMaterialDict *CMatLightmaps::GetMaterialDict() const
{
	return GetMaterialSystem()->GetMaterialDict();
}

inline CMaterialDict *CMatLightmaps::GetMaterialDict()
{
	return GetMaterialSystem()->GetMaterialDict();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CMatLightmaps::CMatLightmaps()
{
	m_currentWhiteLightmapMaterial = NULL;
	m_pLightmapPages = NULL;
	m_NumLightmapPages = 0;
	m_numSortIDs = 0;
	m_nUpdatingLightmapsStackDepth = 0;
	m_nLockedLightmap = -1;
	m_pLightmapDataPtrArray = NULL;
	m_eLightmapsState = STATE_DEFAULT;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::Shutdown( )
{
	// Clean up all lightmaps
	CleanupLightmaps();
}

//-----------------------------------------------------------------------------
// Assign enumeration IDs to all materials
//-----------------------------------------------------------------------------
void CMatLightmaps::EnumerateMaterials( void )
{
	// iterate in sorted order
	int id = 0;
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		GetMaterialInternal(i)->SetEnumerationID( id );
		++id;
	}
}


//-----------------------------------------------------------------------------
// Gets the maximum lightmap page size...
//-----------------------------------------------------------------------------
int CMatLightmaps::GetMaxLightmapPageWidth() const
{
	// FIXME: It's unclear which we want here.
	// It doesn't drastically increase primitives per DrawIndexedPrimitive
	// call at the moment to increase it, so let's not for now.
	
	// If we're using dynamic textures though, we want bigger that's for sure.
	// The tradeoff here is how much memory we waste if we don't fill the lightmap

	// We need to go to 512x256 textures because that's the only way bumped
	// lighting on displacements can work given the 128x128 allowance..
	int nWidth = 512;
	if ( nWidth > HardwareConfig()->MaxTextureWidth() )
		nWidth = HardwareConfig()->MaxTextureWidth();

	return nWidth;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetMaxLightmapPageHeight() const
{
	int nHeight = 256;

	if ( nHeight > HardwareConfig()->MaxTextureHeight() )
		nHeight = HardwareConfig()->MaxTextureHeight();

	return nHeight;
}


//-----------------------------------------------------------------------------
// Returns the lightmap page size
//-----------------------------------------------------------------------------
void CMatLightmaps::GetLightmapPageSize( int lightmapPageID, int *pWidth, int *pHeight ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		*pWidth = m_pLightmapPages[lightmapPageID].m_Width;
		*pHeight = m_pLightmapPages[lightmapPageID].m_Height;
		break;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		*pWidth = *pHeight = 1;
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		break;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		*pWidth = *pHeight = 1;
		break;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetLightmapWidth( int lightmapPageID ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		return m_pLightmapPages[lightmapPageID].m_Width;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		return 1;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		return 1;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CMatLightmaps::GetLightmapHeight( int lightmapPageID ) const
{
	switch( lightmapPageID )
	{
	default:
 		Assert( lightmapPageID >= 0 && lightmapPageID < GetNumLightmapPages() );
		return m_pLightmapPages[lightmapPageID].m_Height;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( !"Can't use CMatLightmaps to get properties of MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED" );
		return 1;
	
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		return 1;
	}
}


//-----------------------------------------------------------------------------
// Clean up lightmap pages.
//-----------------------------------------------------------------------------
void CMatLightmaps::CleanupLightmaps()
{
   if ( mat_lightmap_pfms.GetBool())
   {
      // Write PFM files containing lightmap data for this page
      for (int lightmap = 0; lightmap < GetNumLightmapPages(); lightmap++)
      {
         if ((NULL != m_pLightmapDataPtrArray) && (NULL != m_pLightmapDataPtrArray[lightmap]))
         {
            char szPFMFileName[MAX_PATH];

            sprintf(szPFMFileName, "Lightmap-Page-%d.pfm", lightmap);
            m_pLightmapDataPtrArray[lightmap]->WritePFM(szPFMFileName);
         }
      }
   }

   // Remove the lightmap data bitmap representations
   if (m_pLightmapDataPtrArray)
   {
      int i;
      for( i = 0; i < GetNumLightmapPages(); i++ )
      {
         delete m_pLightmapDataPtrArray[i];
      }

      delete [] m_pLightmapDataPtrArray;
      m_pLightmapDataPtrArray = NULL;
   }
   
   // delete old lightmap pages
	if( m_pLightmapPages )
	{
		int i;
		for( i = 0; i < GetNumLightmapPages(); i++ )
		{
			g_pShaderAPI->DeleteTexture( m_LightmapPageTextureHandles[i] );
		}
		delete [] m_pLightmapPages;
		m_pLightmapPages = 0;
	}

	m_NumLightmapPages = 0;
}

//-----------------------------------------------------------------------------
// Resets the lightmap page info for each material
//-----------------------------------------------------------------------------
void CMatLightmaps::ResetMaterialLightmapPageInfo( void )
{
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal *pMaterial = GetMaterialInternal(i);
		pMaterial->SetMinLightmapPageID( 9999 );
		pMaterial->SetMaxLightmapPageID( -9999 );
		pMaterial->SetNeedsWhiteLightmap( false );
	}
}

//-----------------------------------------------------------------------------
// This is called before any lightmap allocations take place
//-----------------------------------------------------------------------------
void CMatLightmaps::BeginLightmapAllocation()
{	
	// delete old lightmap pages
	CleanupLightmaps();

	m_ImagePackers.RemoveAll();
	int i = m_ImagePackers.AddToTail();
	m_ImagePackers[i].Reset( 0, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );

	SetCurrentMaterialInternal(0);
	m_currentWhiteLightmapMaterial = 0;
	m_numSortIDs = 0;

	// need to set the min and max sorting id number for each material to 
	// a default value that basically means that it hasn't been used yet.
	ResetMaterialLightmapPageInfo();

	EnumerateMaterials();
}


//-----------------------------------------------------------------------------
// Allocates space in the lightmaps; must be called after BeginLightmapAllocation
//-----------------------------------------------------------------------------
int CMatLightmaps::AllocateLightmap( int width, int height, 
		                               int offsetIntoLightmapPage[2],
									   IMaterial *iMaterial )
{
	IMaterialInternal *pMaterial = static_cast<IMaterialInternal *>( iMaterial );
	if ( !pMaterial )
	{
		Warning( "Programming error: CMatRenderContext::AllocateLightmap: NULL material\n" );
		return m_numSortIDs;
	}
	pMaterial = pMaterial->GetRealTimeVersion(); //always work with the real time versions of materials internally
	
	// material change
	int i;
	int nPackCount = m_ImagePackers.Count();
	if ( GetCurrentMaterialInternal() != pMaterial )
	{
		// If this happens, then we need to close out all image packers other than
		// the last one so as to produce as few sort IDs as possible
		for ( i = nPackCount - 1; --i >= 0; )
		{
			// NOTE: We *must* use the order preserving one here so the remaining one
			// is the last lightmap
			m_ImagePackers.Remove( i );
			--nPackCount;
		}

		// If it's not the first material, increment the sort id
		if (GetCurrentMaterialInternal())
		{
			m_ImagePackers[0].IncrementSortId( );
			++m_numSortIDs;
		}

		SetCurrentMaterialInternal(pMaterial);

		// This assertion guarantees we don't see the same material twice in this loop.
		Assert( pMaterial->GetMinLightmapPageID( ) > pMaterial->GetMaxLightmapPageID() );

		// NOTE: We may not use this lightmap page, but we might
		// we won't know for sure until the next material is passed in.
		// So, for now, we're going to forcibly add the current lightmap
		// page to this material so the sort IDs work out correctly.
		GetCurrentMaterialInternal()->SetMinLightmapPageID( GetNumLightmapPages() );
		GetCurrentMaterialInternal()->SetMaxLightmapPageID( GetNumLightmapPages() );
	}

	// Try to add it to any of the current images...
	bool bAdded = false;
	for ( i = 0; i < nPackCount; ++i )
	{
		bAdded = m_ImagePackers[i].AddBlock( width, height, &offsetIntoLightmapPage[0], &offsetIntoLightmapPage[1] );
		if ( bAdded )
			break;
	}

	if ( !bAdded )
	{
		++m_numSortIDs;
		i = m_ImagePackers.AddToTail();
		m_ImagePackers[i].Reset( m_numSortIDs, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );
		++m_NumLightmapPages;
		if ( !m_ImagePackers[i].AddBlock( width, height, &offsetIntoLightmapPage[0], &offsetIntoLightmapPage[1] ) )
		{
			Error( "MaterialSystem_Interface_t::AllocateLightmap: lightmap (%dx%d) too big to fit in page (%dx%d)\n", 
				width, height, GetMaxLightmapPageWidth(), GetMaxLightmapPageHeight() );
		}

		// Add this lightmap to the material...
		GetCurrentMaterialInternal()->SetMaxLightmapPageID( GetNumLightmapPages() );
	}

	return m_ImagePackers[i].GetSortId();
}

// UNDONE: This needs testing, but it appears as though creating these textures managed
// results in huge stalls whenever they are locked for modify.
// That makes sense given the d3d docs, but these have been flagged as managed for quite some time.
#define DYNAMIC_TEXTURES_NO_BACKING 1

void CMatLightmaps::EndLightmapAllocation()
{
	// count the last page that we were on.if it wasn't 
	// and count the last sortID that we were on
	m_NumLightmapPages++; 
	m_numSortIDs++;

	m_firstDynamicLightmap = m_NumLightmapPages;
	// UNDONE: Until we start using the separate dynamic lighting textures don't allocate them
	// NOTE: Enable this if we want to stop locking the base lightmaps and instead only lock update
	// these completely dynamic pages
//	m_NumLightmapPages += COUNT_DYNAMIC_LIGHTMAP_PAGES;
	m_dynamic.Init();

	// Compute the dimensions of the last lightmap 
	int lastLightmapPageWidth, lastLightmapPageHeight;
	int nLastIdx = m_ImagePackers.Count();
	m_ImagePackers[nLastIdx - 1].GetMinimumDimensions( &lastLightmapPageWidth, &lastLightmapPageHeight );
	m_ImagePackers.Purge();

	m_pLightmapPages = new LightmapPageInfo_t[GetNumLightmapPages()];
	Assert( m_pLightmapPages );

   if ( mat_lightmap_pfms.GetBool())
   {
      // This array will be used to write PFM files full of lightmap data
      m_pLightmapDataPtrArray = new FloatBitMap_t*[GetNumLightmapPages()];
   }

	int i;
	m_LightmapPageTextureHandles.EnsureCapacity( GetNumLightmapPages() );
	for ( i = 0; i < GetNumLightmapPages(); i++ )
	{
		// Compute lightmap dimensions
		bool lastStaticLightmap = ( i == (m_firstDynamicLightmap-1));
		m_pLightmapPages[i].m_Width = (unsigned short)(lastStaticLightmap ? lastLightmapPageWidth : GetMaxLightmapPageWidth());
		m_pLightmapPages[i].m_Height = (unsigned short)(lastStaticLightmap ? lastLightmapPageHeight : GetMaxLightmapPageHeight());
		m_pLightmapPages[i].m_Flags = 0;

		AllocateLightmapTexture( i );

        if ( mat_lightmap_pfms.GetBool())
        {
           // Initialize the pointers to lightmap data
           m_pLightmapDataPtrArray[i] = NULL;
        }
	}
}

//-----------------------------------------------------------------------------
// Allocate lightmap textures
//-----------------------------------------------------------------------------
void CMatLightmaps::AllocateLightmapTexture( int lightmap )
{
	bool bUseDynamicTextures = HardwareConfig()->PreferDynamicTextures();

	int flags = bUseDynamicTextures ? TEXTURE_CREATE_DYNAMIC : TEXTURE_CREATE_MANAGED;

	m_LightmapPageTextureHandles.EnsureCount( lightmap + 1 );

	char debugName[256];
	Q_snprintf( debugName, sizeof( debugName ), "[lightmap %d]", lightmap );
	
	ImageFormat imageFormat;
	switch ( HardwareConfig()->GetHDRType() )
	{
	default:
		Assert( 0 );
		// fall through.

	case HDR_TYPE_NONE:
#if !defined( _X360 )
		imageFormat = IMAGE_FORMAT_RGBA8888;
		flags |= TEXTURE_CREATE_SRGB;
#else
		imageFormat = IMAGE_FORMAT_LINEAR_RGBA8888;
#endif
		break;

	case HDR_TYPE_INTEGER:
#if !defined( _X360 )
		imageFormat = IMAGE_FORMAT_RGBA16161616;
#else
#		if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
			imageFormat = IMAGE_FORMAT_LINEAR_RGBA8888;
#		else
			imageFormat = IMAGE_FORMAT_LINEAR_RGBA16161616;
#		endif
#endif
		break;

	case HDR_TYPE_FLOAT:
		imageFormat = IMAGE_FORMAT_RGBA16161616F;
		break;
	}

	switch ( m_eLightmapsState )
	{
	case STATE_DEFAULT:
		// Allow allocations in default state
		{
			m_LightmapPageTextureHandles[lightmap] = g_pShaderAPI->CreateTexture( 
				GetLightmapWidth(lightmap), GetLightmapHeight(lightmap), 1,
				imageFormat, 
				1, 1, flags, debugName, TEXTURE_GROUP_LIGHTMAP );	// don't mipmap lightmaps

			// Load up the texture data
			g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );

			if ( !bUseDynamicTextures )
			{
				g_pShaderAPI->TexSetPriority( 1 );
			}

			// Blat out the lightmap bits
			InitLightmapBits( lightmap );
		}
		break;

	case STATE_RELEASED:
		// Not assigned m_LightmapPageTextureHandles[lightmap];
		DevMsg( "AllocateLightmapTexture(%d) in released lightmap state (STATE_RELEASED), delayed till \"Restore\".\n", lightmap );
		return;

	default:
		// Not assigned m_LightmapPageTextureHandles[lightmap];
		Warning( "AllocateLightmapTexture(%d) in unknown lightmap state (%d), skipped.\n", lightmap, m_eLightmapsState );
		Assert( !"AllocateLightmapTexture(?) in unknown lightmap state (?)" );
		return;
	}
}


int	CMatLightmaps::AllocateWhiteLightmap( IMaterial *iMaterial )
{
	IMaterialInternal *pMaterial = static_cast<IMaterialInternal *>( iMaterial );
	if( !pMaterial )
	{
		Warning( "Programming error: CMatRenderContext::AllocateWhiteLightmap: NULL material\n" );
		return m_numSortIDs;
	}
	pMaterial = pMaterial->GetRealTimeVersion(); //always work with the real time versions of materials internally

	if ( !m_currentWhiteLightmapMaterial || ( m_currentWhiteLightmapMaterial != pMaterial ) )
	{
		if ( !GetCurrentMaterialInternal() && !m_currentWhiteLightmapMaterial )
		{
			// don't increment if this is the very first material (ie. no lightmaps
			// allocated with AllocateLightmap
			// Assert( 0 );
		}
		else
		{
			// material change
			m_numSortIDs++;
#if 0
			char buf[128];
			Q_snprintf( buf, sizeof( buf ), "AllocateWhiteLightmap: m_numSortIDs = %d %s\n", m_numSortIDs, pMaterial->GetName() );
			OutputDebugString( buf );
#endif
		}
//		Warning( "%d material: \"%s\" lightmapPageID: -1\n", m_numSortIDs, pMaterial->GetName() );
		m_currentWhiteLightmapMaterial = pMaterial;
		pMaterial->SetNeedsWhiteLightmap( true );
	}

	return m_numSortIDs;
}

//-----------------------------------------------------------------------------
// Releases/restores lightmap pages
//-----------------------------------------------------------------------------
void CMatLightmaps::ReleaseLightmapPages()
{
	switch ( m_eLightmapsState )
	{
	case STATE_DEFAULT:
		// Allow release in default state only
		break;
	
	default:
		Warning( "ReleaseLightmapPages is expected in STATE_DEFAULT, current state = %d, discarded.\n", m_eLightmapsState );
		Assert( !"ReleaseLightmapPages is expected in STATE_DEFAULT" );
		return;
	}

	for( int i = 0; i < GetNumLightmapPages(); i++ )
	{
		g_pShaderAPI->DeleteTexture( m_LightmapPageTextureHandles[i] );
	}
	
	// We are now in released state
	m_eLightmapsState = STATE_RELEASED;
}

void CMatLightmaps::RestoreLightmapPages()
{
	switch ( m_eLightmapsState )
	{
	case STATE_RELEASED:
		// Allow restore in released state only
		break;

	default:
		Warning( "RestoreLightmapPages is expected in STATE_RELEASED, current state = %d, discarded.\n", m_eLightmapsState );
		Assert( !"RestoreLightmapPages is expected in STATE_RELEASED" );
		return;
	}

	// Switch to default state to allow allocations
	m_eLightmapsState = STATE_DEFAULT;

	for( int i = 0; i < GetNumLightmapPages(); i++ )
	{
		AllocateLightmapTexture( i );
	}
}


//-----------------------------------------------------------------------------
// This initializes the lightmap bits
//-----------------------------------------------------------------------------
void CMatLightmaps::InitLightmapBits( int lightmap )
{
	VPROF_( "CMatLightmaps::InitLightmapBits", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
	int width = GetLightmapWidth(lightmap);
	int height = GetLightmapHeight(lightmap);

	CPixelWriter writer;

	g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, width, height, writer ) )
		return;

	// Debug mode, make em green checkerboard
	if ( writer.IsUsingFloatFormat() )
	{
		for ( int j = 0; j < height; ++j )
		{
			writer.Seek( 0, j );
			for ( int k = 0; k < width; ++k )
			{
#ifndef _DEBUG
				writer.WritePixel( 1.0f, 1.0f, 1.0f );
#else // _DEBUG
				if( ( j + k ) & 1 )
				{
					writer.WritePixelF( 0.0f, 1.0f, 0.0f );
				}
				else
				{
					writer.WritePixelF( 0.0f, 0.0f, 0.0f );
				}
#endif // _DEBUG
			}
		}
	}
	else
	{
		for ( int j = 0; j < height; ++j )
		{
			writer.Seek( 0, j );
			for ( int k = 0; k < width; ++k )
			{
#ifndef _DEBUG
				// note: make this white to find multisample centroid sampling problems.
				//				writer.WritePixel( 255, 255, 255 );
				writer.WritePixel( 0, 0, 0 );
#else // _DEBUG
				if ( ( j + k ) & 1 )
				{
					writer.WritePixel( 0, 255, 0 );
				}
				else
				{
					writer.WritePixel( 0, 0, 0 );
				}
#endif // _DEBUG
			}
		}
	}

	g_pShaderAPI->TexUnlock();
}

bool CMatLightmaps::LockLightmap( int lightmap )
{
//	Warning( "locking lightmap page: %d\n", lightmap );
	VPROF_INCREMENT_COUNTER( "lightmap fullpage texlock", 1 );
	if( m_nLockedLightmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
	}
	g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmap] );
	int pageWidth  = m_pLightmapPages[lightmap].m_Width;
	int pageHeight = m_pLightmapPages[lightmap].m_Height;
	if (!g_pShaderAPI->TexLock( 0, 0, 0, 0,	pageWidth, pageHeight, m_LightmapPixelWriter ))
	{
		Assert( 0 );
		return false;
	}
	m_nLockedLightmap = lightmap;
	return true;
}

Vector4D ConvertLightmapColorToRGBScale( const float *lightmapColor )
{
	Vector4D result;

	float fScale = lightmapColor[0];
	for( int i = 1; i != 3; ++i )
	{
		if( lightmapColor[i] > fScale )
			fScale = lightmapColor[i];
	}

	fScale = ceil( fScale * (255.0f/16.0f) ) * (16.0f/255.0f);
	fScale = min( fScale, 16.0f );

	float fInvScale = 1.0f / fScale;

	for( int i = 0; i != 3; ++i )
	{
		result[i] = lightmapColor[i] * fInvScale;
		result[i] = ceil( result[i] * 255.0f ) * (1.0f/255.0f);
		result[i] = min( result[i], 1.0f );
	}

	fScale /= 16.0f;

	result.w = fScale;

	return result;
}

#ifdef _X360
// SIMD version of above
// input numbers from pSrc are on the domain [0..16]
// output is RGBA 
// ignores contents of w channel of input
// the shader does this: rOut = Rin * Ain * 16.0f 
// where Rin is [0..1], a float computed from a byte value [0..255]
// Ain is therefore the brightest channel (say R) divided by 16 and quantized
// Rin is computed from pSrc->r by dividing by Ain
// this outputs RGBa where RGB are [0..255] and a is the shader's scaling factor (also 0..255)
//
// WARNING - this code appears to be vulnerable to a compiler bug. Be very careful modifying and be
// sure to test
fltx4 ConvertLightmapColorToRGBScale( FLTX4 lightmapColor )
{
	
	static const fltx4 vTwoFiftyFive = {255.0f, 255.0f, 255.0f, 255.0f};
	static const fltx4 FourPoint1s = { 0.1, 0.1, 0.1, 0.1 };
	static const fltx4 vTwoFiftyFiveOverSixteen = {255.0f / 16.0f, 255.0f / 16.0f, 255.0f / 16.0f, 255.0f / 16.0f};
	// static const fltx4 vSixteenOverTwoFiftyFive = { 16.0f / 255.0f, 16.0f / 255.0f, 16.0f / 255.0f, 16.0f / 255.0f };


	// find the highest color value in lightmapColor and replicate it
	fltx4 scale = FindHighestSIMD3( lightmapColor );
	fltx4 minscale = FindLowestSIMD3( lightmapColor );
	fltx4 fl4OutofRange = OrSIMD( CmpGeSIMD( scale, Four_Ones ), CmpLeSIMD( scale, FourPoint1s ) );
	fl4OutofRange = OrSIMD( fl4OutofRange, CmpGtSIMD( minscale, MulSIMD( Four_PointFives, scale ) ) );

	// scale needs to be divided by 16 (because the shader multiplies it by 16)
	// then mapped to 0..255 and quantized. 
	scale = __vrfip(MulSIMD(scale, vTwoFiftyFiveOverSixteen)); // scale = ceil(scale * 255/16)
		
	fltx4 result = MulSIMD(vTwoFiftyFive, lightmapColor); // start the scale cooking on the final result
		
	fltx4 invScale = ReciprocalEstSIMD(scale); // invScale = (16/255)(1/scale). may be +inf
	invScale = MulSIMD(invScale, vTwoFiftyFiveOverSixteen); // take the quantizing factor back out
															// of the inverse scale (one less
															// dependent op if you do it this way)
		
	// scale the input channels
	// compute so the numbers are all 0..255 ints. (if one happens to 
	// be 256 due to numerical error in the reciprocation, the unsigned-saturate
	// store we'll use later on will bake it back down to 255)
	result = MulSIMD(result, invScale);
		
	// now, output --
	// if the input color was nonzero, slip the scale into return value's w
	// component and return. If the input was zero, return zero.

	result = MaskedAssign( 
		fl4OutofRange,
		SetWSIMD( result, scale ),
		SetWSIMD( MulSIMD( lightmapColor, vTwoFiftyFive ), vTwoFiftyFiveOverSixteen ) );
	return result;
}
#endif


// write bumped lightmap update to LDR 8-bit lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_LDR( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
	float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	for( int t = 0; t < pLightmapSize[1]; t++ )
	{
		int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

		for( int s = 0; s < nLightmapSize0; 
			s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
		{
			unsigned char color[4][3];

			ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
				&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
				&pFloatImageBump3[srcTexelOffset],
				color[0], color[1], color[2], color[3] );

			unsigned char alpha =  RoundFloatToByte( pFloatImage[srcTexelOffset+3] * 255.0f );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[0][0], color[0][1], color[0][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[1][0], color[1][1], color[1][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[2][0], color[2][1], color[2][2], alpha );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvance( color[3][0], color[3][1], color[3][2], alpha );
		}
	}
	if ( pfmOut )
	{
		for( int t = 0; t < pLightmapSize[1]; t++ )
		{
			int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
			for( int s = 0;  s < nLightmapSize0; s++,srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
			{
				unsigned char color[4][3];

				ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
					&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
					&pFloatImageBump3[srcTexelOffset],
					color[0], color[1], color[2], color[3] );

				unsigned char alpha =  RoundFloatToByte( pFloatImage[srcTexelOffset+3] * 255.0f );
				// Write data to the bitmapped represenations so that PFM files can be written
				PixRGBAF pixelData;
				pixelData.Red = color[0][0];                  
				pixelData.Green = color[0][1];                  
				pixelData.Blue = color[0][2];
				pixelData.Alpha = alpha;
				pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData);
			}
		}

	}
}

// write bumped lightmap update to HDR float lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_HDRF( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
												 float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	if ( IsX360() )
	{
		// 360 does not support HDR float mode 
		Assert( 0 );
		return;
	}

	Assert( !pfmOut );		// unsupported in this mode

	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	for( int t = 0; t < pLightmapSize[1]; t++ )
	{
		int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

		for( int s = 0; 
			s < nLightmapSize0; 
			s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
		{
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImage[srcTexelOffset], pFloatImage[srcTexelOffset+1],
				pFloatImage[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump1[srcTexelOffset], pFloatImageBump1[srcTexelOffset+1],
				pFloatImageBump1[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump2[srcTexelOffset], pFloatImageBump2[srcTexelOffset+1],
				pFloatImageBump2[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );

			m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
			m_LightmapPixelWriter.WritePixelNoAdvanceF( pFloatImageBump3[srcTexelOffset], pFloatImageBump3[srcTexelOffset+1],
				pFloatImageBump3[srcTexelOffset+2], pFloatImage[srcTexelOffset+3] );
		}
	}
}

#ifdef _X360
#pragma optimize("u", on)
#endif


#ifdef _X360

namespace {
	// pack a pixel into BGRA8888 and return it with the data packed into the w component
FORCEINLINE fltx4 PackPixel_BGRA8888( FLTX4 rgba ) 
{
	// this happens to be in an order such that we can use the handy builtin packing op
	// clamp to 0..255 (coz it might have leaked over)
	static const fltx4 vTwoFiftyFive = {255.0f, 255.0f, 255.0f, 255.0f};

	// the magic number such that when mul-accummulated against rbga,
	// gets us a representation 3.0 + (r)*2^-22 -- puts the bits at
	// the bottom of the float
	static const XMVECTOR   PackScale = { (1.0f / (FLOAT)(1 << 22)), (1.0f / (FLOAT)(1 << 22)), (1.0f / (FLOAT)(1 << 22)), (1.0f / (FLOAT)(1 << 22))}; // 255.0f / (FLOAT)(1 << 22)
	static const XMVECTOR   Three = {3.0f, 3.0f, 3.0f, 3.0f};

	fltx4 N = MinSIMD(vTwoFiftyFive, rgba); 

	N = __vmaddfp(N, PackScale, Three);
	N = __vpkd3d(N, N, VPACK_D3DCOLOR, VPACK_32, 0);  // pack into w word
	return N;
}

// A small store-gather buffer used in the 
// BumpedLightmapBitsToPixelWriter_HDRI_BGRA_X360().
// The store-gather buffers. Hopefully these will live in the L1
// cache, which will make writing to them, then to memory, faster
// than just using __stvewx to write directly into WC memory
// one noncontiguous float at a time. (If there weren't a huge
// compiler bug with __stvewx in the Apr07 XDK, that might not
// be the case.)
struct ALIGN128 CPixelWriterStoreGather
{
	enum {
		kRows = 4,
		kWordsPerRow = 32,
	};

	ALIGN128 uint32 m_data[kRows][kWordsPerRow]; // four rows of bgra data, aligned to 4 cache lines. dwords so memcpy works better.
	int m_wordsGathered;
	int m_bytesBetweenWriterRows; // the number of bytes spacing the maps inside the writer from each other
								// if we weren't gathering, we'd SkipBytes this many between the base map, bump1, etc.

	// write four rows, as SIMD registers, into the buffers
	inline void write( CPixelWriter * RESTRICT pLightmapPixelWriter, FLTX4 row0,  FLTX4 row1,  FLTX4 row2,  FLTX4 row3 ) RESTRICT
	{
		// if full, commit
		Assert(m_wordsGathered <= kWordsPerRow);
		AssertMsg((m_wordsGathered & 3) == 0, "Don't call CPixelWriterStoreGather::write after ::writeJustX"); // single-word writes have misaligned me
		if (m_wordsGathered >= kWordsPerRow)
		{
			commitWhenFull(pLightmapPixelWriter);
		}

		XMStoreVector4A( &m_data[0][m_wordsGathered], row0 );
		XMStoreVector4A( &m_data[1][m_wordsGathered], row1 );
		XMStoreVector4A( &m_data[2][m_wordsGathered], row2 );
		XMStoreVector4A( &m_data[3][m_wordsGathered], row3 );

		m_wordsGathered += 4 ; // four words per simd vec
	}

	// pluck the w component out of each of the rows, and store it into the gather buffer. Don't
	// call the other write function after calling this.
	inline void writeJustW( CPixelWriter * RESTRICT pLightmapPixelWriter, FLTX4 row0,  FLTX4 row1,  FLTX4 row2,  FLTX4 row3 ) RESTRICT
	{
		// if full, commit
		Assert(m_wordsGathered <= kWordsPerRow);
		if (m_wordsGathered >= kWordsPerRow)
		{
			commitWhenFull(pLightmapPixelWriter);
		}

		// for each fltx4, splat out x and then use the __stvewx to store
		// whichever word happens to align with the float pointer through
		// that pointer.

		__stvewx(__vspltw(row0, 3), &m_data[0][m_wordsGathered], 0 );
		__stvewx(__vspltw(row1, 3), &m_data[1][m_wordsGathered], 0 );
		__stvewx(__vspltw(row2, 3), &m_data[2][m_wordsGathered], 0 );
		__stvewx(__vspltw(row3, 3), &m_data[3][m_wordsGathered], 0 );

		m_wordsGathered += 1 ; // only stored one word
	}

	// Commit my buffers to the pixelwriter's memory, and advance its
	// pointer.
	void commit(CPixelWriter * RESTRICT pLightmapPixelWriter) RESTRICT
	{
		if (m_wordsGathered > 0)
		{
			unsigned char* RESTRICT pWriteInto = pLightmapPixelWriter->GetCurrentPixel();
			// we have to use memcpy because we're writing to non-cacheable memory,
			// but we can't even assume that the addresses we're writing to are
			// vector-aligned.
#ifdef memcpy // if someone's overriden the intrinsic, complain
#pragma error("You have overridden memcpy(), which is an XBOX360 intrinsic. This function will not behave optimally.")
#endif

			memcpy(pWriteInto, m_data[0], m_wordsGathered * sizeof(uint32));
			pWriteInto += m_bytesBetweenWriterRows;
			memcpy(pWriteInto, m_data[1], m_wordsGathered * sizeof(uint32));
			pWriteInto += m_bytesBetweenWriterRows;
			memcpy(pWriteInto, m_data[2], m_wordsGathered * sizeof(uint32));
			pWriteInto += m_bytesBetweenWriterRows;
			memcpy(pWriteInto, m_data[3], m_wordsGathered * sizeof(uint32));

			pLightmapPixelWriter->SkipBytes(m_wordsGathered * sizeof(uint32));
			m_wordsGathered = 0;
		}
	}

	// like commit, but the version we use when we know we're full.
	// Takes advantage of better compile-time generation for 
	// memcpy.
	void commitWhenFull(CPixelWriter * RESTRICT pLightmapPixelWriter) RESTRICT
	{
		unsigned char* RESTRICT pWriteInto = pLightmapPixelWriter->GetCurrentPixel();
		// we have to use memcpy because we're writing to non-cacheable memory,
		// but we can't even assume that the addresses we're writing to are
		// vector-aligned.
#ifdef memcpy // if someone's overriden the intrinsic, complain
#pragma error("You have overridden memcpy(), which is an XBOX360 intrinsic. This function will not behave optimally.")
#endif

		// if we're full, use compile-time known version of 
		// mempcy to take advantage of its ability to generate
		// inline code. In fact, use the dword-aligned
		// version so that we use the 64-bit writing funcs.
		Assert( m_wordsGathered == kWordsPerRow );
		COMPILE_TIME_ASSERT((kWordsPerRow & 3) == 0); // the number of words per row has to be a multiple of four
		
		memcpy(pWriteInto, reinterpret_cast<uint64* RESTRICT>(m_data[0]), kWordsPerRow * sizeof(uint32));
		pWriteInto += m_bytesBetweenWriterRows;
		memcpy(pWriteInto, reinterpret_cast<uint64* RESTRICT>(m_data[1]), kWordsPerRow * sizeof(uint32));
		pWriteInto += m_bytesBetweenWriterRows;
		memcpy(pWriteInto, reinterpret_cast<uint64* RESTRICT>(m_data[2]), kWordsPerRow * sizeof(uint32));
		pWriteInto += m_bytesBetweenWriterRows;
		memcpy(pWriteInto, reinterpret_cast<uint64* RESTRICT>(m_data[3]), kWordsPerRow * sizeof(uint32));
		
		pLightmapPixelWriter->SkipBytes(m_wordsGathered * sizeof(uint32));
		m_wordsGathered = 0;
	}

	// parameter: space between bump pages in the pixelwriter
	CPixelWriterStoreGather(int writerSizeBytes) : m_wordsGathered(0), m_bytesBetweenWriterRows(writerSizeBytes) {};

};
}


// this is a function for specifically writing bumped BGRA lightmaps -- in order for it
// to be properly scheduled, I needed to break out the inline functions. Also,
// to make the write-combined memory more efficient (and work around a bug in the
// April 2007 XDK), we need to store-gather our writes on the cache before blasting
// them out to write-combined memory. We can't simply write from the SIMD registers
// into the pixelwriter's data, because the difference between the output rows,
// eg nLightmap0WriterSizeBytes[0], might not be a multiple of 16. Unaligned stores
// to non-cacheable memory cause an alignment exception.
static void BumpedLightmapBitsToPixelWriter_HDRI_BGRA_X360( float* RESTRICT pFloatImage, float * RESTRICT pFloatImageBump1, float * RESTRICT pFloatImageBump2, 
													  float * RESTRICT pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut,
													  CPixelWriter * RESTRICT m_LightmapPixelWriter)
{
	AssertMsg(m_LightmapPixelWriter->GetPixelSize() == 4, "BGRA format is no longer four bytes long? This is unsupported on 360, and probably immoral as well.");
	const int nLightmap0WriterSizeBytes = pLightmapSize[0] * 4 /*m_LightmapPixelWriter->GetPixelSize()*/;
	// const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - 4 );

	// assert that 1 * 4 = 4 
	COMPILE_TIME_ASSERT(sizeof( Vector4D ) == sizeof(float) * 4); 

	AssertMsg(!pfmOut, "Runtime conversion of lightmaps to files is no longer supported on 360.\n");

	
	// The store-gather buffers. Hopefully these will live in the L1
	// cache, which will make writing to them, then to memory, faster
	// than just using __stvewx to write directly into WC memory
	// one noncontiguous float at a time. (If there weren't a huge
	// compiler bug with __stvewx in the Apr07 XDK, that might not
	// be the case.)
	CPixelWriterStoreGather storeGather(nLightmap0WriterSizeBytes);

	for( int t = 0; t < pLightmapSize[1]; t++ )
	{
#define	FOUR (sizeof( Vector4D ) / sizeof( float ))  //  make explicit when we're incrementing by length of a 4dvec
		int srcTexelOffset = ( FOUR ) * ( 0 + t * pLightmapSize[0] );
		m_LightmapPixelWriter->Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

		// Our code works best when we can process luxels in groups of four. So,
		// figure out how many four-luxel groups we can process,
		// then do them in groups, then process the remainder.
		unsigned int groupsOfFourLimit = (((unsigned int)pLightmapSize[0]) & ~3);
		
		// we want to hang on to this index when we're done with groups so we can do the remainder.
		unsigned int s; // counts the number of luxels processed
		for( s = 0; 
			s < groupsOfFourLimit; 
			s += 4, srcTexelOffset += 4 * ( FOUR ))
		{				
			static const fltx4 vSixteen = {16.0f, 16.0f, 16.0f, 16.0f};
			// the store-gather simds
			fltx4 outBaseMap = Four_Zeros, outBump1 = Four_Zeros, outBump2 = Four_Zeros, outBump3 = Four_Zeros;
			// we'll read four at a time
			fltx4 vFloatImage[4], vFloatImageBump1[4], vFloatImageBump2[4], vFloatImageBump3[4];


			// stripe these loads to cause less ERAT thrashing
			vFloatImage[0]	  = LoadUnalignedSIMD(pFloatImage	   + srcTexelOffset );
			vFloatImage[1]	  = LoadUnalignedSIMD(pFloatImage	   + srcTexelOffset + 4 );
			vFloatImage[2]	  = LoadUnalignedSIMD(pFloatImage	   + srcTexelOffset + 8 );
			vFloatImage[3]	  = LoadUnalignedSIMD(pFloatImage	   + srcTexelOffset + 12 );

			vFloatImageBump1[0] = LoadUnalignedSIMD(pFloatImageBump1 + srcTexelOffset );
			vFloatImageBump1[1] = LoadUnalignedSIMD(pFloatImageBump1 + srcTexelOffset + 4 );
			vFloatImageBump1[2] = LoadUnalignedSIMD(pFloatImageBump1 + srcTexelOffset + 8 );
			vFloatImageBump1[3] = LoadUnalignedSIMD(pFloatImageBump1 + srcTexelOffset + 12 );

			vFloatImageBump2[0] = LoadUnalignedSIMD(pFloatImageBump2 + srcTexelOffset );
			vFloatImageBump2[1] = LoadUnalignedSIMD(pFloatImageBump2 + srcTexelOffset + 4 );
			vFloatImageBump2[2] = LoadUnalignedSIMD(pFloatImageBump2 + srcTexelOffset + 8 );
			vFloatImageBump2[3] = LoadUnalignedSIMD(pFloatImageBump2 + srcTexelOffset + 12 );

			vFloatImageBump3[0] = LoadUnalignedSIMD(pFloatImageBump3 + srcTexelOffset );
			vFloatImageBump3[1] = LoadUnalignedSIMD(pFloatImageBump3 + srcTexelOffset + 4 );
			vFloatImageBump3[2] = LoadUnalignedSIMD(pFloatImageBump3 + srcTexelOffset + 8 );
			vFloatImageBump3[3] = LoadUnalignedSIMD(pFloatImageBump3 + srcTexelOffset + 12 );

			// perform an arcane averaging operation upon the bump map values
			// (todo: make this not an inline so it will schedule better -- inlining is 
			//  done by the linker, which is too late for operation scheduling)
			ColorSpace::LinearToBumpedLightmap( vFloatImage[0],	vFloatImageBump1[0],
												vFloatImageBump2[0], vFloatImageBump3[0],
												// transform "in place":
												vFloatImage[0], vFloatImageBump1[0], 
												vFloatImageBump2[0], vFloatImageBump3[0] );
			ColorSpace::LinearToBumpedLightmap( vFloatImage[1],	vFloatImageBump1[1],
												vFloatImageBump2[1], vFloatImageBump3[1],
												// transform "in place":
												vFloatImage[1], vFloatImageBump1[1], 
												vFloatImageBump2[1], vFloatImageBump3[1] );
			ColorSpace::LinearToBumpedLightmap( vFloatImage[2],	vFloatImageBump1[2],
												vFloatImageBump2[2], vFloatImageBump3[2],
												// transform "in place":
												vFloatImage[2], vFloatImageBump1[2], 
												vFloatImageBump2[2], vFloatImageBump3[2] );
			ColorSpace::LinearToBumpedLightmap( vFloatImage[3],	vFloatImageBump1[3],
												vFloatImageBump2[3], vFloatImageBump3[3],
												// transform "in place":
												vFloatImage[3], vFloatImageBump1[3], 
												vFloatImageBump2[3], vFloatImageBump3[3] );
	

			// convert each color to RGB scaled.
			// DO NOT! make this into a for loop. The (April07 XDK) compiler
			// in fact DOES NOT unroll them, and will perform very naive
			// scheduling if you try. 

			// clamp to 0..16 float
			vFloatImage[0]		= MinSIMD(vFloatImage[0], vSixteen);
			vFloatImageBump1[0] = MinSIMD(vFloatImageBump1[0], vSixteen);
			vFloatImageBump2[0] = MinSIMD(vFloatImageBump2[0], vSixteen);
			vFloatImageBump3[0] = MinSIMD(vFloatImageBump3[0], vSixteen);

			vFloatImage[1]		= MinSIMD(vFloatImage[1], vSixteen);
			vFloatImageBump1[1] = MinSIMD(vFloatImageBump1[1], vSixteen);
			vFloatImageBump2[1] = MinSIMD(vFloatImageBump2[1], vSixteen);
			vFloatImageBump3[1] = MinSIMD(vFloatImageBump3[1], vSixteen);

			vFloatImage[2]		= MinSIMD(vFloatImage[2], vSixteen);
			vFloatImageBump1[2] = MinSIMD(vFloatImageBump1[2], vSixteen);
			vFloatImageBump2[2] = MinSIMD(vFloatImageBump2[2], vSixteen);
			vFloatImageBump3[2] = MinSIMD(vFloatImageBump3[2], vSixteen);

			vFloatImage[3]		= MinSIMD(vFloatImage[3], vSixteen);
			vFloatImageBump1[3] = MinSIMD(vFloatImageBump1[3], vSixteen);
			vFloatImageBump2[3] = MinSIMD(vFloatImageBump2[3], vSixteen);
			vFloatImageBump3[3] = MinSIMD(vFloatImageBump3[3], vSixteen);


			// compute the scaling factor, place it in w, and 
			// scale the rest by it. Obliterates whatever was
			// already in alpha.
			// This code is why it is important to not use a for
			// loop: you need to let the compiler keep the value
			// on registers (which it can't do if you use a
			// variable indexed array) and interleave the
			// inlined instructions.

			vFloatImage[0]		= PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImage[0]) );
			vFloatImageBump1[0] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump1[0]) );
			vFloatImageBump2[0] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump2[0]) );
			vFloatImageBump3[0] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump3[0]) );

			vFloatImage[1]		= PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImage[1]) );
			vFloatImageBump1[1] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump1[1]) );
			vFloatImageBump2[1] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump2[1]) );
			vFloatImageBump3[1] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump3[1]) );

			vFloatImage[2]		= PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImage[2]) );
			vFloatImageBump1[2] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump1[2]) );
			vFloatImageBump2[2] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump2[2]) );
			vFloatImageBump3[2] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump3[2]) );

			vFloatImage[3]		= PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImage[3]) );
			vFloatImageBump1[3] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump1[3]) );
			vFloatImageBump2[3] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump2[3]) );
			vFloatImageBump3[3] = PackPixel_BGRA8888( ConvertLightmapColorToRGBScale(vFloatImageBump3[3]) );

			// Each of the registers above contains one RGBA 32-bit struct
			// in their w word. So, combine them such that each of the assignees
			// below contains four RGBAs, in xyzw order (big-endian).

			outBaseMap = __vrlimi(outBaseMap, vFloatImage[0], 8, 3 ); // insert into x
			outBump1 =	 __vrlimi(outBump1, vFloatImageBump1[0], 8, 3 ); // insert into x
			outBump2 =	 __vrlimi(outBump2, vFloatImageBump2[0], 8, 3 ); // insert into x
			outBump3 =	 __vrlimi(outBump3, vFloatImageBump3[0], 8, 3 ); // insert into x

			outBaseMap = __vrlimi(outBaseMap, vFloatImage[1], 4, 2 ); // insert into y
			outBump1 =	 __vrlimi(outBump1, vFloatImageBump1[1], 4, 2 ); // insert into y
			outBump2 =	 __vrlimi(outBump2, vFloatImageBump2[1], 4, 2 ); // insert into y
			outBump3 =	 __vrlimi(outBump3, vFloatImageBump3[1], 4, 2 ); // insert into y

			outBaseMap = __vrlimi(outBaseMap, vFloatImage[2], 2, 1 ); // insert into z
			outBump1 =	 __vrlimi(outBump1, vFloatImageBump1[2], 2, 1 ); // insert into z
			outBump2 =	 __vrlimi(outBump2, vFloatImageBump2[2], 2, 1 ); // insert into z
			outBump3 =	 __vrlimi(outBump3, vFloatImageBump3[2], 2, 1 ); // insert into z

			outBaseMap = __vrlimi(outBaseMap, vFloatImage[3], 1, 0 ); // insert into w
			outBump1 =	 __vrlimi(outBump1, vFloatImageBump1[3], 1, 0 ); // insert into w
			outBump2 =	 __vrlimi(outBump2, vFloatImageBump2[3], 1, 0 ); // insert into w
			outBump3 =	 __vrlimi(outBump3, vFloatImageBump3[3], 1, 0 ); // insert into w

			// push the data through the store-gather buffer.
			storeGather.write(m_LightmapPixelWriter, outBaseMap, outBump1, outBump2, outBump3);

		}

		// Once here, make sure we've committed any leftover changes, then process
		// the remainders singly.
		storeGather.commit(m_LightmapPixelWriter);

		for( ;  // s is where it should be from the loop above
			s < (unsigned int) pLightmapSize[0]; 
			s++, 
				// m_LightmapPixelWriter->SkipBytes(nRewindToNextPixel), // now handled by store-gather
				srcTexelOffset += ( FOUR ))
		{				

			static const fltx4 vSixteen = {16.0f, 16.0f, 16.0f, 16.0f};
			fltx4 vColor[4];
			fltx4 vFloatImage = LoadUnalignedSIMD(&pFloatImage[srcTexelOffset]);
			fltx4 vFloatImageBump1 = LoadUnalignedSIMD(&pFloatImageBump1[srcTexelOffset]);
			fltx4 vFloatImageBump2 = LoadUnalignedSIMD(&pFloatImageBump2[srcTexelOffset]);
			fltx4 vFloatImageBump3 = LoadUnalignedSIMD(&pFloatImageBump3[srcTexelOffset]);

			// perform an arcane averaging operation upon the bump map values
			ColorSpace::LinearToBumpedLightmap( vFloatImage,
				vFloatImageBump1, vFloatImageBump2,
				vFloatImageBump3,
				vColor[0], vColor[1], vColor[2], vColor[3] );		

			// convert each color to RGB scaled.
			// DO NOT! make this into a for loop. The (April07 XDK) compiler
			// in fact DOES NOT unroll them, and will perform very naive
			// scheduling if you try. 

			// clamp to 0..16 float
			vColor[0] = MinSIMD(vColor[0], vSixteen);
			vColor[1] = MinSIMD(vColor[1], vSixteen);
			vColor[2] = MinSIMD(vColor[2], vSixteen);
			vColor[3] = MinSIMD(vColor[3], vSixteen);

			// compute the scaling factor, place it in w, and 
			// scale the rest by it. Obliterates whatever was
			// already in alpha.
			// This code is why it is important to not use a for
			// loop: you need to let the compiler interleave the
			// inlined instructions.
			vColor[0] = ConvertLightmapColorToRGBScale( vColor[0] );
			vColor[1] = ConvertLightmapColorToRGBScale( vColor[1] );
			vColor[2] = ConvertLightmapColorToRGBScale( vColor[2] );
			vColor[3] = ConvertLightmapColorToRGBScale( vColor[3] );


#ifdef X360_DOUBLECHECK_LIGHTMAPS
			unsigned short color[4][4];

			ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
				&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
				&pFloatImageBump3[srcTexelOffset],
				color[0], color[1], color[2], color[3] );
			unsigned short alpha = ColorSpace::LinearToUnsignedShort( pFloatImage[srcTexelOffset+3], 16 );
			color[0][3] = color[1][3] = color[2][3] = color[3][3] = alpha;

			if( IsX360() )
			{
				for( int i = 0; i != 4; ++i )
				{
					Vector4D vRGBScale;

					vRGBScale.x = color[i][0] * (16.0f / 65535.0f);
					vRGBScale.y = color[i][1] * (16.0f / 65535.0f);
					vRGBScale.z = color[i][2] * (16.0f / 65535.0f);
					vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );
					color[i][0] = RoundFloatToByte( vRGBScale.x * 255.0f );
					color[i][1] = RoundFloatToByte( vRGBScale.y * 255.0f );
					color[i][2] = RoundFloatToByte( vRGBScale.z * 255.0f );
					color[i][3] = RoundFloatToByte( vRGBScale.w * 255.0f );
				}						
			}

			/*
			for (int ii = 0; ii < 4; ++ii)
			{
				uint32 pack = (PackPixel_BGRA8888( vColor[ii] ).u[3]);
				if (color[ii][3] != 0)
				Assert(	color[ii][0] == (pack & 0xFF0000) >> 16	&& 
						color[ii][1] == (pack & 0xFF00) >> 8		&& 
						color[ii][2] == (pack & 0xFF)				&& 
						color[ii][3] == (pack & 0xFF000000) >> 24 );
			}
			*/

#endif


				vColor[0] = PackPixel_BGRA8888( vColor[0] );
				vColor[1] = PackPixel_BGRA8888( vColor[1] );
				vColor[2] = PackPixel_BGRA8888( vColor[2] );
				vColor[3] = PackPixel_BGRA8888( vColor[3] );

				storeGather.writeJustW(m_LightmapPixelWriter, vColor[0], vColor[1], vColor[2], vColor[3] );

				/* // here is the old way of writing pixels:
				// now we store-gather this
				m_LightmapPixelWriter->WritePixelNoAdvance_BGRA8888( vColor[0] );
				Assert(*reinterpret_cast<unsigned int *>(m_LightmapPixelWriter->GetCurrentPixel()) == PackPixel_BGRA8888( vColor[0] ).u[3] );
				void * RESTRICT pBits = m_LightmapPixelWriter->SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter->WritePixelNoAdvance_BGRA8888( vColor[1], pBits );
				Assert(*reinterpret_cast<unsigned int *>(m_LightmapPixelWriter->GetCurrentPixel()) == PackPixel_BGRA8888( vColor[1] ).u[3] );
				pBits = m_LightmapPixelWriter->SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter->WritePixelNoAdvance_BGRA8888( vColor[2], pBits );
				Assert(*reinterpret_cast<unsigned int *>(m_LightmapPixelWriter->GetCurrentPixel()) == PackPixel_BGRA8888( vColor[2] ).u[3] );
				pBits = m_LightmapPixelWriter->SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter->WritePixelNoAdvance_BGRA8888( vColor[3], pBits );
				Assert(*reinterpret_cast<unsigned int *>(m_LightmapPixelWriter->GetCurrentPixel()) == PackPixel_BGRA8888( vColor[3] ).u[3] );

				m_LightmapPixelWriter->SkipBytes(nRewindToNextPixel);
				*/
		}

		storeGather.commit(m_LightmapPixelWriter);

	}
}

#endif // _X360

// write bumped lightmap update to HDR integer lightmap
void CMatLightmaps::BumpedLightmapBitsToPixelWriter_HDRI( float* RESTRICT pFloatImage, float * RESTRICT pFloatImageBump1, float * RESTRICT pFloatImageBump2, 
												 float * RESTRICT pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut ) RESTRICT
{
	const int nLightmapSize0 = pLightmapSize[0];
	const int nLightmap0WriterSizeBytes = nLightmapSize0 * m_LightmapPixelWriter.GetPixelSize();
	const int nRewindToNextPixel = -( ( nLightmap0WriterSizeBytes * 3 ) - m_LightmapPixelWriter.GetPixelSize() );

	if( m_LightmapPixelWriter.IsUsingFloatFormat() )
	{
		AssertMsg(!IsX360(), "Tried to use a floating-point pixel format for lightmaps on 360, which is not supported.");
		if (!IsX360())
		{
			for( int t = 0; t < pLightmapSize[1]; t++ )
			{
				int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

				for( int s = 0; 
					s < nLightmapSize0; 
					s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
				{
					unsigned short color[4][4];

					ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
						&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
						&pFloatImageBump3[srcTexelOffset],
						color[0], color[1], color[2], color[3] );
					float alpha = pFloatImage[srcTexelOffset+3];
					Assert( alpha >= 0.0f && alpha <= 1.0f );
					color[0][3] = color[1][3] = color[2][3] = color[3][3] = alpha;

					float toFloat = ( 1.0f / ( float )( 1 << 16 ) );

					/* // This code is now a can't-happen, because we do not allow float formats on 360.
#if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
					if( IsX360() )
					{
						for( int i = 0; i != 4; ++i )
						{
							Vector4D vRGBScale;

							vRGBScale.x = color[i][0] * (16.0f / 65535.0f);
							vRGBScale.y = color[i][1] * (16.0f / 65535.0f);
							vRGBScale.z = color[i][2] * (16.0f / 65535.0f);
							vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );
							color[i][0] = RoundFloatToByte( vRGBScale.x * 255.0f );
							color[i][1] = RoundFloatToByte( vRGBScale.y * 255.0f );
							color[i][2] = RoundFloatToByte( vRGBScale.z * 255.0f );
							color[i][3] = RoundFloatToByte( vRGBScale.w * 255.0f );
						}

						toFloat = ( 1.0f / ( float )( 1 << 8 ) );
					}
#endif
					*/

					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[0][0], toFloat * color[0][1], toFloat * color[0][2], toFloat * color[0][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[1][0], toFloat * color[1][1], toFloat * color[1][2], toFloat * color[1][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[2][0], toFloat * color[2][1], toFloat * color[2][2], toFloat * color[2][3] );

					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvanceF( toFloat * color[3][0], toFloat * color[3][1], toFloat * color[3][2], toFloat * color[3][3] );
				}
			}
		}
	}
	else
	{
#ifndef X360_USE_SIMD_LIGHTMAP
		for( int t = 0; t < pLightmapSize[1]; t++ )
		{
			int srcTexelOffset = ( sizeof( Vector4D ) / sizeof( float ) ) * ( 0 + t * nLightmapSize0 );
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

			for( int s = 0; 
				s < nLightmapSize0; 
				s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += (sizeof(Vector4D)/sizeof(float)))
			{					
				unsigned short color[4][4];

				ColorSpace::LinearToBumpedLightmap( &pFloatImage[srcTexelOffset],
					&pFloatImageBump1[srcTexelOffset], &pFloatImageBump2[srcTexelOffset],
					&pFloatImageBump3[srcTexelOffset],
					color[0], color[1], color[2], color[3] );
				unsigned short alpha = ColorSpace::LinearToUnsignedShort( pFloatImage[srcTexelOffset+3], 16 );
				color[0][3] = color[1][3] = color[2][3] = color[3][3] = alpha;

#if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
				if( IsX360() )
				{
					for( int i = 0; i != 4; ++i )
					{
						Vector4D vRGBScale;

						vRGBScale.x = color[i][0] * (16.0f / 65535.0f);
						vRGBScale.y = color[i][1] * (16.0f / 65535.0f);
						vRGBScale.z = color[i][2] * (16.0f / 65535.0f);
						vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );
						color[i][0] = RoundFloatToByte( vRGBScale.x * 255.0f );
						color[i][1] = RoundFloatToByte( vRGBScale.y * 255.0f );
						color[i][2] = RoundFloatToByte( vRGBScale.z * 255.0f );
						color[i][3] = RoundFloatToByte( vRGBScale.w * 255.0f );
					}						
				}
#endif
				m_LightmapPixelWriter.WritePixelNoAdvance( color[0][0], color[0][1], color[0][2], color[0][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[1][0], color[1][1], color[1][2], color[1][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[2][0], color[2][1], color[2][2], color[2][3] );

				m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
				m_LightmapPixelWriter.WritePixelNoAdvance( color[3][0], color[3][1], color[3][2], color[3][3] );

				// Write data to the bitmapped represenations so that PFM files can be written
				if ( pfmOut )
				{
					PixRGBAF pixelData;
					pixelData.Red = color[0][0];                  
					pixelData.Green = color[0][1];                  
					pixelData.Blue = color[0][2];
					pixelData.Alpha = alpha;
					pfmOut->WritePixelRGBAF(pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData);
				}
			}
		}
#else
		// this is an optimized XBOX implementation. For a clearer
		// presentation of the algorithm, see the PC implementation
		// above.
		// First check for the most common case, using an efficient
		// branch rather than a switch:
		if (m_LightmapPixelWriter.GetFormat() == IMAGE_FORMAT_LINEAR_BGRA8888)
		{
			// broken out into a static to make things more readable
			// and be nicer to the instruction cache
			BumpedLightmapBitsToPixelWriter_HDRI_BGRA_X360( pFloatImage, pFloatImageBump1, pFloatImageBump2, 
				pFloatImageBump3, pLightmapSize, pOffsetIntoLightmapPage, pfmOut, &m_LightmapPixelWriter );
		}
		else
		{	// This case should actually never be hit -- we do not use RGBA.
			for( int t = 0; t < pLightmapSize[1]; t++ )
			{
				// assert that 1 * 4 = 4 
				COMPILE_TIME_ASSERT(sizeof( Vector4D ) == sizeof(float) * 4); 
#define	FOUR (sizeof( Vector4D ) / sizeof( float ))  // in case this ever changes
				int srcTexelOffset = ( FOUR ) * ( 0 + t * nLightmapSize0 );
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );

				for( int s = 0; 
					s < nLightmapSize0; 
					s++, m_LightmapPixelWriter.SkipBytes(nRewindToNextPixel),srcTexelOffset += ( FOUR ))
				{				

					static const fltx4 vSixteen = {16.0f, 16.0f, 16.0f, 16.0f};
					fltx4 vColor[4];
					fltx4 vFloatImage = LoadUnalignedSIMD(&pFloatImage[srcTexelOffset]);
					fltx4 vFloatImageBump1 = LoadUnalignedSIMD(&pFloatImageBump1[srcTexelOffset]);
					fltx4 vFloatImageBump2 = LoadUnalignedSIMD(&pFloatImageBump2[srcTexelOffset]);
					fltx4 vFloatImageBump3 = LoadUnalignedSIMD(&pFloatImageBump3[srcTexelOffset]);
					
					// perform an arcane averaging operation upon the bump map values
					ColorSpace::LinearToBumpedLightmap( vFloatImage,
						vFloatImageBump1, vFloatImageBump2,
						vFloatImageBump3,
						vColor[0], vColor[1], vColor[2], vColor[3] );		

					// convert each color to RGB scaled.
					// DO NOT! make this into a for loop. The (April07 XDK) compiler
					// in fact DOES NOT unroll them, and will perform very naive
					// scheduling if you try. 

					// clamp to 0..16 float
					vColor[0] = MinSIMD(vColor[0], vSixteen);
					vColor[1] = MinSIMD(vColor[1], vSixteen);
					vColor[2] = MinSIMD(vColor[2], vSixteen);
					vColor[3] = MinSIMD(vColor[3], vSixteen);

					// compute the scaling factor, transform the RGB,
					// and place the scale in w. Obliterates whatever was
					// already in alpha.
					// This code is why it is important to not use a for
					// loop: you need to let the compiler interleave the
					// inlined instructions.
					vColor[0] = ConvertLightmapColorToRGBScale( vColor[0] );
					vColor[1] = ConvertLightmapColorToRGBScale( vColor[1] );
					vColor[2] = ConvertLightmapColorToRGBScale( vColor[2] );
					vColor[3] = ConvertLightmapColorToRGBScale( vColor[3] );


					m_LightmapPixelWriter.WritePixelNoAdvance( vColor[0] );
					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvance( vColor[1] );
					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvance( vColor[2] );
					m_LightmapPixelWriter.SkipBytes( nLightmap0WriterSizeBytes );
					m_LightmapPixelWriter.WritePixelNoAdvance( vColor[3] );

					AssertMsg(!pfmOut, "Runtime conversion of lightmaps to files is no longer supported on 360.\n");

					// Write data to the bitmapped represenations so that PFM files can be written
					if ( pfmOut )
					{
						Warning("**************************************************\n"
								"Lightmap output to files on 360 HAS BEEN DISABLED.\n"
								"A grave error has just occurred.\n"
								"**************************************************\n");
						DebuggerBreakIfDebugging();
						/*
						PixRGBAF pixelData;
						pixelData.Red = color[0][0];                  
						pixelData.Green = color[0][1];                  
						pixelData.Blue = color[0][2];
						pixelData.Alpha = alpha;
						pfmOut->WritePixelRGBAF(pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData);
						*/
					}
				}
			}
		}
#endif
	}
}


void CMatLightmaps::LightmapBitsToPixelWriter_LDR( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	// non-HDR lightmap processing
	float *pSrc = pFloatImage;
	for( int t = 0; t < pLightmapSize[1]; ++t )
	{
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
		for( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
		{
			unsigned char color[4];
			ColorSpace::LinearToLightmap( color, pSrc );
			color[3] =  RoundFloatToByte( pSrc[3] * 255.0f );
			m_LightmapPixelWriter.WritePixel( color[0], color[1], color[2], color[3] );

			if ( pfmOut )
			{
				// Write data to the bitmapped represenations so that PFM files can be written
				PixRGBAF pixelData;
				pixelData.Red = color[0];                  
				pixelData.Green = color[1];                  
				pixelData.Blue = color[2];
				pixelData.Alpha = color[3];
				pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
			}
		}
	}
}


void CMatLightmaps::LightmapBitsToPixelWriter_HDRF( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut )
{
	if ( IsX360() )
	{
		// 360 does not support HDR float 
		Assert( 0 );
		return;
	}

	// float HDR lightmap processing
	float *pSrc = pFloatImage;
	for ( int t = 0; t < pLightmapSize[1]; ++t )
	{
		m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
		for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
		{
			m_LightmapPixelWriter.WritePixelF( pSrc[0], pSrc[1], pSrc[2], pSrc[3] );
		}
	}
}

// numbers come in on the domain [0..16]
void CMatLightmaps::LightmapBitsToPixelWriter_HDRI( float* RESTRICT pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t * RESTRICT pfmOut )
{
#ifndef X360_USE_SIMD_LIGHTMAP
	// PC code (and old, pre-SIMD xbox version -- unshippably slow)
	if ( m_LightmapPixelWriter.IsUsingFloatFormat() )
	{
		// integer HDR lightmap processing
		float *pSrc = pFloatImage;
		for ( int t = 0; t < pLightmapSize[1]; ++t )
		{
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
			for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
			{
				int r, g, b, a;

				r = ColorSpace::LinearFloatToCorrectedShort( pSrc[0] );
				g = ColorSpace::LinearFloatToCorrectedShort( pSrc[1] );
				b = ColorSpace::LinearFloatToCorrectedShort( pSrc[2] );
				a = ColorSpace::LinearToUnsignedShort( pSrc[3], 16 );

				float toFloat = ( 1.0f / ( float )( 1 << 16 ) );

#if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
				if( IsX360() )
				{
					Vector4D vRGBScale;

					vRGBScale.x = r * (16.0f / 65535.0f);
					vRGBScale.y = g * (16.0f / 65535.0f);
					vRGBScale.z = b * (16.0f / 65535.0f);
					vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );

					r = RoundFloatToByte( vRGBScale.x * 255.0f );
					g = RoundFloatToByte( vRGBScale.y * 255.0f );
					b = RoundFloatToByte( vRGBScale.z * 255.0f );
					a = RoundFloatToByte( vRGBScale.w * 255.0f );

					toFloat = ( 1.0f / ( float )( 1 << 8 ) );
				}

#endif
				Assert( pSrc[3] >= 0.0f && pSrc[3] <= 1.0f );
				m_LightmapPixelWriter.WritePixelF( r * toFloat, g * toFloat, b * toFloat, pSrc[3] );
			}
		}
	}
	else
	{
		// integer HDR lightmap processing
		float *pSrc = pFloatImage;
		for ( int t = 0; t < pLightmapSize[1]; ++t )
		{
			m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
			for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
			{
				int r, g, b, a;

				r = ColorSpace::LinearFloatToCorrectedShort( pSrc[0] );
				g = ColorSpace::LinearFloatToCorrectedShort( pSrc[1] );
				b = ColorSpace::LinearFloatToCorrectedShort( pSrc[2] );
				a = ColorSpace::LinearToUnsignedShort( pSrc[3], 16 );

#if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
				if( IsX360() )
				{
					Vector4D vRGBScale;

					vRGBScale.x = r * (16.0f / 65535.0f);
					vRGBScale.y = g * (16.0f / 65535.0f);
					vRGBScale.z = b * (16.0f / 65535.0f);
					vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );

					r = RoundFloatToByte( vRGBScale.x * 255.0f );
					g = RoundFloatToByte( vRGBScale.y * 255.0f );
					b = RoundFloatToByte( vRGBScale.z * 255.0f );
					a = RoundFloatToByte( vRGBScale.w * 255.0f );
				}
#endif
				m_LightmapPixelWriter.WritePixel( r, g, b, a );

				if ( pfmOut )
				{
					// Write data to the bitmapped represenations so that PFM files can be written
					PixRGBAF pixelData;
					pixelData.Red = pSrc[0];                  
					pixelData.Green = pSrc[1];                  
					pixelData.Blue = pSrc[2];
					pixelData.Alpha = pSrc[3];
					pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
				}				
			}
		}
	}
#else
	// XBOX360 code
	if ( m_LightmapPixelWriter.IsUsingFloatFormat() )
	{
		if( IsX360() )
		{
			AssertMsg( false, "Float-format pixel writers do not exist on x360." );
		}
		else
		{	// This code is here as an example only, in case floating point
			// format is restored to 360.

			// integer HDR lightmap processing
			float * RESTRICT pSrc = pFloatImage;
			for ( int t = 0; t < pLightmapSize[1]; ++t )
			{
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
				for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += (sizeof(Vector4D)/sizeof(*pSrc)) )
				{
					int r, g, b, a;

					r = ColorSpace::LinearFloatToCorrectedShort( pSrc[0] );
					g = ColorSpace::LinearFloatToCorrectedShort( pSrc[1] );
					b = ColorSpace::LinearFloatToCorrectedShort( pSrc[2] );
					a = ColorSpace::LinearToUnsignedShort( pSrc[3], 16 );

					float toFloat = ( 1.0f / ( float )( 1 << 16 ) );

#if ( defined( USE_32BIT_LIGHTMAPS_ON_360 ) )
					if( IsX360() )
					{
						Vector4D vRGBScale;

						vRGBScale.x = r * (16.0f / 65535.0f);
						vRGBScale.y = g * (16.0f / 65535.0f);
						vRGBScale.z = b * (16.0f / 65535.0f);
						vRGBScale = ConvertLightmapColorToRGBScale( &vRGBScale.x );

						r = RoundFloatToByte( vRGBScale.x * 255.0f );
						g = RoundFloatToByte( vRGBScale.y * 255.0f );
						b = RoundFloatToByte( vRGBScale.z * 255.0f );
						a = RoundFloatToByte( vRGBScale.w * 255.0f );

						toFloat = ( 1.0f / ( float )( 1 << 8 ) );
					}

#endif
					Assert( pSrc[3] >= 0.0f && pSrc[3] <= 1.0f );
					m_LightmapPixelWriter.WritePixelF( r * toFloat, g * toFloat, b * toFloat, pSrc[3] );
				}
			}
		}
	}
	else
	{
		// This is the fast X360 pathway.

		// integer HDR lightmap processing
		float * RESTRICT pSrc = pFloatImage;
		// Assert((reinterpret_cast<unsigned int>(pSrc) & 15) == 0); // 16-byte aligned?
		COMPILE_TIME_ASSERT(sizeof(Vector4D)/sizeof(*pSrc) == 4); // assert that 1 * 4 = 4
#ifndef USE_32BIT_LIGHTMAPS_ON_360 
#pragma error("This function only supports 32 bit lightmaps.")
#endif

		// input numbers from pSrc are on the domain [0..+inf]
		// we clamp them to the range [0..16]
		// output is RGBA 
		// the shader does this: rOut = Rin * Ain * 16.0f 
		// where Rin is [0..1], a float computed from a byte value [0..255]
		// Ain is therefore the brightest channel (say R) divided by 16 and quantized
		// Rin is computed from pSrc->r by dividing by Ain
		
		// rather than switching inside WritePixel for each different format,
		// thus causing a 23-cycle pipeline clear for every pixel, we'll
		// branch on the format here. That will allow us to unroll the inline
		// pixel write functions differently depending on their different 
		// latencies. 

		Assert(!pfmOut); // should never happen on 360.
#ifndef ALLOW_PFM_OUTPUT_ON_360
		if ( pfmOut )
		{
			Warning("*****************************************\n"
					"Lightmap output on 360 HAS BEEN DISABLED.\n"
					"A grave error has just occurred.\n"
					"*****************************************\n");
		}
#endif

		// switch once, here, outside the loop, rather than
		// switching inside each pixel. Switches are not fast
		// on x360: they are usually implemented as jumps 
		// through function tables, which have a 24-cycle
		// stall. 
		switch (m_LightmapPixelWriter.GetFormat())
		{
			// note: format names are low-order-byte first. 
		case IMAGE_FORMAT_RGBA8888:
		case IMAGE_FORMAT_LINEAR_RGBA8888:
		{
			for ( int t = 0; t < pLightmapSize[1]; ++t )
			{
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
				for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += 4 )
				{	
					static const fltx4 vSixteen = {16.0f, 16.0f, 16.0f, 16.0f};
					fltx4 rgba = LoadUnalignedSIMD(pSrc);

					// clamp to 0..16 float
					rgba = MinSIMD(rgba, vSixteen);
					// compute the scaling factor, place it in w, and 
					// scale the rest by it.
					rgba = ConvertLightmapColorToRGBScale( rgba );
					// rgba is now  float 0..255 in each component
					m_LightmapPixelWriter.WritePixelNoAdvance_RGBA8888(rgba);


					/*  // not supported on X360
					if ( pfmOut )
					{
						// Write data to the bitmapped represenations so that PFM files can be written
						PixRGBAF pixelData;
						XMStoreVector4(&pixelData,rgba);
						pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
					}			
					*/
				}
			}
			break;
		}

		case IMAGE_FORMAT_BGRA8888: // NOTE! : the low order bits are first in this naming convention.
		case IMAGE_FORMAT_LINEAR_BGRA8888:
		{			
			for ( int t = 0; t < pLightmapSize[1]; ++t )
			{
				m_LightmapPixelWriter.Seek( pOffsetIntoLightmapPage[0], pOffsetIntoLightmapPage[1] + t );
				for ( int s = 0; s < pLightmapSize[0]; ++s, pSrc += 4 )
				{	
					static const fltx4 vSixteen = {16.0f, 16.0f, 16.0f, 16.0f};
					fltx4 rgba = LoadUnalignedSIMD(pSrc);

					// clamp to 0..16 float
					rgba = MinSIMD(rgba, vSixteen);
					// compute the scaling factor, place it in w, and 
					// scale the rest by it.
					rgba = ConvertLightmapColorToRGBScale( rgba );
					// rgba is now  float 0..255 in each component
					m_LightmapPixelWriter.WritePixelNoAdvance_BGRA8888(rgba);
					// forcibly advance
					m_LightmapPixelWriter.SkipBytes(4);

					/* // not supported on X360
					if ( pfmOut )
					{
						// Write data to the bitmapped represenations so that PFM files can be written
						PixRGBAF pixelData;
						XMStoreVector4(&pixelData,rgba);
						pfmOut->WritePixelRGBAF( pOffsetIntoLightmapPage[0] + s, pOffsetIntoLightmapPage[1] + t, pixelData );
					}			
					*/
				}
			}
			break;
		}

		default:
			AssertMsg1(false,"Unsupported pixel format %d while writing lightmaps!", m_LightmapPixelWriter.GetFormat() );
			Warning("Unsupported pixel format used in lightmap. Lightmaps could not be downloaded.\n");
			break;
		}
	}
#endif
}

void CMatLightmaps::BeginUpdateLightmaps( void )
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatLightmaps::BeginUpdateLightmaps );
		return;
	}

	m_nUpdatingLightmapsStackDepth++;
}

void CMatLightmaps::EndUpdateLightmaps( void )
{
	CMatCallQueue *pCallQueue = GetMaterialSystem()->GetRenderContextInternal()->GetCallQueueInternal();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMatLightmaps::EndUpdateLightmaps );
		return;
	}

	m_nUpdatingLightmapsStackDepth--;
	Assert( m_nUpdatingLightmapsStackDepth >= 0 );
	if( m_nUpdatingLightmapsStackDepth <= 0 && m_nLockedLightmap != -1 )
	{
		g_pShaderAPI->TexUnlock();
		m_nLockedLightmap = -1;
	}
}

int CMatLightmaps::AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID )
{
	// check frameID, fail if current
	for ( int i = 0; i < COUNT_DYNAMIC_LIGHTMAP_PAGES; i++ )
	{
		int dynamicIndex = (m_dynamic.currentDynamicIndex + i) % COUNT_DYNAMIC_LIGHTMAP_PAGES;
		int lightmapPageIndex = m_firstDynamicLightmap + dynamicIndex;
		if ( m_dynamic.lightmapLockFrame[dynamicIndex] != frameID )
		{
			m_dynamic.lightmapLockFrame[dynamicIndex] = frameID;
			m_dynamic.imagePackers[dynamicIndex].Reset( 0, m_pLightmapPages[lightmapPageIndex].m_Width, m_pLightmapPages[lightmapPageIndex].m_Height );
		}

		if ( m_dynamic.imagePackers[dynamicIndex].AddBlock( lightmapSize[0], lightmapSize[1], &pOutOffsetIntoPage[0], &pOutOffsetIntoPage[1] ) )
		{
			return lightmapPageIndex;
		}
	}
	
	return -1;
}

//-----------------------------------------------------------------------------
// Updates the lightmap
//-----------------------------------------------------------------------------
void CMatLightmaps::UpdateLightmap( int lightmapPageID, int lightmapSize[2],
									  int offsetIntoLightmapPage[2], 
									  float *pFloatImage, float *pFloatImageBump1,
									  float *pFloatImageBump2, float *pFloatImageBump3 )
{
	VPROF( "CMatRenderContext::UpdateLightmap" );

	bool hasBump = false;
	int uSize = 1;
	FloatBitMap_t *pfmOut = NULL;
	if ( pFloatImageBump1 && pFloatImageBump2 && pFloatImageBump3 )
	{
		hasBump = true;
		uSize = 4;
	}

	if ( lightmapPageID >= GetNumLightmapPages() || lightmapPageID < 0 )
	{
		Error( "MaterialSystem_Interface_t::UpdateLightmap lightmapPageID=%d out of range\n", lightmapPageID );
		return;
	}
	bool bDynamic = IsDynamicLightmap(lightmapPageID);

	if ( bDynamic )
	{
		int dynamicIndex = lightmapPageID-m_firstDynamicLightmap;
		Assert(dynamicIndex < COUNT_DYNAMIC_LIGHTMAP_PAGES);
		m_dynamic.currentDynamicIndex = (dynamicIndex + 1) % COUNT_DYNAMIC_LIGHTMAP_PAGES;
	}

	if ( mat_lightmap_pfms.GetBool())
	{
		// Allocate and initialize lightmap data that will be written to a PFM file
		if (NULL == m_pLightmapDataPtrArray[lightmapPageID])
		{
			m_pLightmapDataPtrArray[lightmapPageID] = new FloatBitMap_t(m_pLightmapPages[lightmapPageID].m_Width, m_pLightmapPages[lightmapPageID].m_Height);
			m_pLightmapDataPtrArray[lightmapPageID]->Clear(0, 0, 0, 1);
		}
		pfmOut = m_pLightmapDataPtrArray[lightmapPageID];
	}

	// NOTE: Change how the lock is taking place if you ever change how bumped
	// lightmaps are put into the page. Right now, we assume that they're all
	// added to the right of the original lightmap.
	bool bLockSubRect;
	{
		VPROF_( "Locking lightmaps", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 ); // vprof scope

		bLockSubRect = m_nUpdatingLightmapsStackDepth <= 0 && !bDynamic;
		if( bLockSubRect )
		{
			VPROF_INCREMENT_COUNTER( "lightmap subrect texlock", 1 );
			g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[lightmapPageID] );
			if (!g_pShaderAPI->TexLock( 0, 0, offsetIntoLightmapPage[0], offsetIntoLightmapPage[1],
				lightmapSize[0] * uSize, lightmapSize[1], m_LightmapPixelWriter ))
			{
				return;
			}
		}
		else if( lightmapPageID != m_nLockedLightmap )
		{
			if ( !LockLightmap( lightmapPageID ) )
			{
				ExecuteNTimes( 10, Warning( "Failed to lock lightmap\n" ) );
				return;
			}
		}
	}

	int subRectOffset[2] = {0,0};

	{
		// account for the part spent in math:
		VPROF_( "LightmapBitsToPixelWriter", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
		if ( hasBump )
		{
			switch( HardwareConfig()->GetHDRType() )
			{
			case HDR_TYPE_NONE:
				BumpedLightmapBitsToPixelWriter_LDR( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			case HDR_TYPE_INTEGER:
				BumpedLightmapBitsToPixelWriter_HDRI( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			case HDR_TYPE_FLOAT:
				BumpedLightmapBitsToPixelWriter_HDRF( pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3, 
					lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;
			}
		}
		else
		{
			switch ( HardwareConfig()->GetHDRType() )
			{
			case HDR_TYPE_NONE:
				LightmapBitsToPixelWriter_LDR( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			case HDR_TYPE_INTEGER:
				LightmapBitsToPixelWriter_HDRI( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			case HDR_TYPE_FLOAT:
				LightmapBitsToPixelWriter_HDRF( pFloatImage, lightmapSize, bLockSubRect ? subRectOffset : offsetIntoLightmapPage, pfmOut );
				break;

			default:
				Assert( 0 );
				break;
			}
		}
	}

	if( bLockSubRect )
	{
		VPROF_( "Unlocking Lightmaps", 2, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0 );
		g_pShaderAPI->TexUnlock();
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CMatLightmaps::GetNumSortIDs( void )
{
	return m_numSortIDs;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::ComputeSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha )
{
	int lightmapPageID;

	for ( MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal* pMaterial = GetMaterialInternal(i);

		if ( pMaterial->GetMinLightmapPageID() > pMaterial->GetMaxLightmapPageID() )
		{
			continue;
		}
		
		//	const IMaterialVar *pTransVar = pMaterial->GetMaterialProperty( MATERIAL_PROPERTY_OPACITY );
		//	if( ( !alpha && ( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) ||
		//		( alpha && !( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) )
		//	{
		//		return true;
		//	}

	
//		Warning( "sort stuff: %s %s\n", material->GetName(), bAlpha ? "alpha" : "not alpha" );
		
		// fill in the lightmapped materials
		for ( lightmapPageID = pMaterial->GetMinLightmapPageID(); 
			 lightmapPageID <= pMaterial->GetMaxLightmapPageID(); ++lightmapPageID )
		{
			pInfo[sortId].material = pMaterial->GetQueueFriendlyVersion();
			pInfo[sortId].lightmapPageID = lightmapPageID;
#if 0
			char buf[128];
			Q_snprintf( buf, sizeof( buf ), "ComputeSortInfo: %s lightmapPageID: %d sortID: %d\n", pMaterial->GetName(), lightmapPageID, sortId );
			OutputDebugString( buf );
#endif
			++sortId;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::ComputeWhiteLightmappedSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha )
{
	for (MaterialHandle_t i = GetMaterialDict()->FirstMaterial(); i != GetMaterialDict()->InvalidMaterial(); i = GetMaterialDict()->NextMaterial(i) )
	{
		IMaterialInternal* pMaterial = GetMaterialInternal(i);

		// fill in the lightmapped materials that are actually used by this level
		if( pMaterial->GetNeedsWhiteLightmap() && 
			( pMaterial->GetReferenceCount() > 0 ) )
		{
			// const IMaterialVar *pTransVar = pMaterial->GetMaterialProperty( MATERIAL_PROPERTY_OPACITY );
			//		if( ( !alpha && ( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) ||
			//			( alpha && !( pTransVar->GetIntValue() == MATERIAL_TRANSLUCENT ) ) )
			//		{
			//			return true;
			//		}

			pInfo[sortId].material = pMaterial->GetQueueFriendlyVersion();
			if( pMaterial->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS ) )
			{
				pInfo[sortId].lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP;
			}
			else
			{
				pInfo[sortId].lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE;
			}

			sortId++;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::GetSortInfo( MaterialSystem_SortInfo_t *pSortInfoArray )
{
	// sort non-alpha blended materials first
	int sortId = 0;
	ComputeSortInfo( pSortInfoArray, sortId, false );
	ComputeWhiteLightmappedSortInfo( pSortInfoArray, sortId, false );
	Assert( m_numSortIDs == sortId );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatLightmaps::EnableLightmapFiltering( bool enabled )
{
	int i;
	for( i = 0; i < GetNumLightmapPages(); i++ )
	{
		g_pShaderAPI->ModifyTexture( m_LightmapPageTextureHandles[i] );
		if( enabled )
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
		}
		else
		{
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_NEAREST );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_NEAREST );
		}
	}
}


