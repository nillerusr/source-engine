//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for materials.
//
//			Materials are kept in a directory tree containing pairs of VMT
//			and VTF files. Each pair of files represents a material.
//
//=============================================================================//

#include "stdafx.h"
#include <process.h>
#include <afxtempl.h>
#include <io.h>
#include <sys\stat.h>
#include <fcntl.h>
#include "hammer.h"
#include "MapDoc.h"
#include "Material.h"
#include "Options.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "WADTypes.h"
#include "BSPFile.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "bitmap/imageformat.h" // hack : don't want to include this just for ImageFormat
#include "filesystem.h"
#include "StudioModel.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"
#include "TextureSystem.h"
#include "materialproxyfactory_wc.h"
#include "vstdlib/cvar.h"
#include "interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)

#define _GraphicCacheAllocate(n)	malloc(n)


MaterialSystem_Config_t g_materialSystemConfig;
static MaterialHandle_t g_CurrMaterial;

extern void ScaleBitmap(CSize sizeSrc, CSize sizeDest, char *src, char *dest);


struct MaterialCacheEntry_t
{
	char szName[MAX_PATH];		//
	CMaterial *pMaterial;		//
	int nRefCount;				//
};


//-----------------------------------------------------------------------------
// Purpose: 
// This class speeds up the call to IMaterial::GetPreviewImageProperties because
// we call it thousands of times per level load when there are detail props.
//-----------------------------------------------------------------------------
class CPreviewImagePropertiesCache
{
public:
	//-----------------------------------------------------------------------------
	// Purpose: Anyone can call this instead of IMaterial::GetPreviewImageProperties
	// and it'll be a lot faster if there are redundant calls to it.
	//-----------------------------------------------------------------------------
	static PreviewImageRetVal_t GetPreviewImageProperties( IMaterial *pMaterial, int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent )
	{
		int i = s_PreviewImagePropertiesCache.Find( pMaterial );
		if ( i == s_PreviewImagePropertiesCache.InvalidIndex() )
		{
			// Add an entry to the cache.
			CPreviewImagePropertiesCache::CEntry entry;
			entry.m_RetVal = pMaterial->GetPreviewImageProperties( &entry.m_Width, &entry.m_Height, &entry.m_ImageFormat, &entry.m_bIsTranslucent );
			i = s_PreviewImagePropertiesCache.Insert( pMaterial, entry );
		}
		
		CPreviewImagePropertiesCache::CEntry &entry = s_PreviewImagePropertiesCache[i];
		*width = entry.m_Width;
		*height = entry.m_Height;
		*imageFormat = entry.m_ImageFormat;
		*isTranslucent = entry.m_bIsTranslucent;
		
		return entry.m_RetVal;
	}

private:

	class CEntry
	{
	public:
		int m_Width;
		int m_Height;
		ImageFormat m_ImageFormat;
		bool m_bIsTranslucent;
		PreviewImageRetVal_t m_RetVal;
	};
	
	static bool PreviewImageLessFunc( IMaterial* const &a, IMaterial* const &b )
	{
		return a < b;
	}

	static CUtlMap<IMaterial*, CPreviewImagePropertiesCache::CEntry> s_PreviewImagePropertiesCache;
};
CUtlMap<IMaterial*, CPreviewImagePropertiesCache::CEntry> CPreviewImagePropertiesCache::s_PreviewImagePropertiesCache( 64, 64, &CPreviewImagePropertiesCache::PreviewImageLessFunc );


//-----------------------------------------------------------------------------
// Purpose: stuff for caching textures in memory.
//-----------------------------------------------------------------------------
class CMaterialImageCache
{
public:

	CMaterialImageCache(int maxNumGraphicsLoaded);
	~CMaterialImageCache(void);
	void EnCache( CMaterial *pMaterial );

protected:

	CMaterial **pool;
	int cacheSize;
	int currentID;  // next one to get killed.
};


//-----------------------------------------------------------------------------
// Purpose: Constructor. Allocates a pool of material pointers.
// Input  : maxNumGraphicsLoaded - 
//-----------------------------------------------------------------------------
CMaterialImageCache::CMaterialImageCache(int maxNumGraphicsLoaded)
{
	cacheSize = maxNumGraphicsLoaded;
	pool = new CMaterialPtr[cacheSize];
	if (pool != NULL)
	{
		memset(pool, 0, sizeof(CMaterialPtr) * cacheSize);
	}
	currentID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees the pool memory.
//-----------------------------------------------------------------------------
CMaterialImageCache::~CMaterialImageCache(void)
{
	if (pool != NULL)
	{
		delete [] pool;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
void CMaterialImageCache::EnCache( CMaterial *pMaterial )
{
	if (pMaterial->m_pData != NULL)
	{
		// Already cached.
		return;
	}

	// kill currentID
	if ((pool[currentID]) && (pool[currentID]->HasData()))
	{
		pool[currentID]->FreeData();
	}

	pool[currentID] = pMaterial;
	pMaterial->LoadMaterialImage();
	currentID = ( currentID + 1 ) % cacheSize;

#if 0
	OutputDebugString( "CMaterialCache::Encache: " );
	OutputDebugString( pMaterial->m_szName );
	OutputDebugString( "\n" );
#endif
}


static CMaterialImageCache *g_pMaterialImageCache = NULL;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMaterialCache::CMaterialCache(void)
{
	m_pCache = NULL;
	m_nMaxEntries = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMaterialCache::~CMaterialCache(void)
{
	if (m_pCache != NULL)
	{
		delete m_pCache;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allocates cache memory for a given number of materials.
// Input  : nMaxEntries - Maximum number of materials in the cache.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterialCache::Create(int nMaxEntries)
{
	Assert((m_pCache == NULL) && (m_nMaxEntries == 0));

	if (m_pCache != NULL)
	{
		delete m_pCache;
		m_pCache = NULL;
		m_nMaxEntries = 0;
	}

	if (nMaxEntries <= 0)
	{
		nMaxEntries = 500;
	}

	m_pCache = new MaterialCacheEntry_t[nMaxEntries];

	if (m_pCache != NULL)
	{
		memset(m_pCache, 0, sizeof(m_pCache[0]) * nMaxEntries);
		m_nMaxEntries = nMaxEntries;
	}

	return(m_pCache != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Factory. Creates a material by name, first looking in the cache.
// Input  : pszMaterialName - Name of material, ie "brick/brickfloor01".
// Output : Returns a pointer to the new material object, NULL if the given
//			material did not exist.
//-----------------------------------------------------------------------------
CMaterial *CMaterialCache::CreateMaterial(const char *pszMaterialName)
{
	CMaterial *pMaterial = NULL;

	if (pszMaterialName != NULL)
	{
		//
		// Find this material in the cache. If it is here, return it.
		//
		pMaterial = FindMaterial(pszMaterialName);
		if (pMaterial == NULL)
		{
			//
			// Not found in the cache, try to create it.
			//
			pMaterial = CMaterial::CreateMaterial(pszMaterialName, true);
			if (pMaterial != NULL)
			{
				//
				// Success. Add the newly created material to the cache.
				//
				AddMaterial(pMaterial);
				return(pMaterial);
			}
		}
		else
		{
			//
			// Found in the cache, bump the reference count.
			//
			AddRef(pMaterial);
		}
	}

	return(pMaterial);
}


//-----------------------------------------------------------------------------
// Purpose: Finds a material in the cache.
// Input  : char *pszMaterialName - 
// Output : CMaterial
//-----------------------------------------------------------------------------
void CMaterialCache::AddMaterial(CMaterial *pMaterial)
{
	if (pMaterial != NULL)
	{
		Assert(m_nEntries < m_nMaxEntries);

		if (m_nEntries < m_nMaxEntries)
		{
			m_pCache[m_nEntries].pMaterial = pMaterial;
			m_pCache[m_nEntries].nRefCount = 1;
			m_nEntries++;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Increments the reference count on a material in the cache. Called by
//			client code when a pointer to the model is copied, making that
//			reference independent.
// Input  : pModel - Model for which to increment the reference count.
//-----------------------------------------------------------------------------
void CMaterialCache::AddRef(CMaterial *pMaterial)
{
	for (int i = 0; i < m_nEntries; i++)
	{
		if (m_pCache[i].pMaterial == pMaterial)
		{
			m_pCache[i].nRefCount++;
			return;
		}
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Finds a material in the cache by name.
// Input  : char *pszMaterialName - 
// Output : CMaterial
//-----------------------------------------------------------------------------
CMaterial *CMaterialCache::FindMaterial(const char *pszMaterialName)
{
	if (pszMaterialName != NULL)
	{
		for (int i = 0; i < m_nEntries; i++)
		{
			if (!stricmp(m_pCache[i].pMaterial->GetName(), pszMaterialName))
			{
				return(m_pCache[i].pMaterial);
			}
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Decrements the reference count of a material, deleting it and
//			removing it from the cache if its reference count becomes zero.
// Input  : pMaterial - Material to release.
//-----------------------------------------------------------------------------
void CMaterialCache::Release(CMaterial *pMaterial)
{
	if (pMaterial != NULL)
	{
		for (int i = 0; i < m_nEntries; i++)
		{
			if (m_pCache[i].pMaterial == pMaterial)
			{
				m_pCache[i].nRefCount--;
				if (m_pCache[i].nRefCount == 0)
				{
					delete m_pCache[i].pMaterial;

					m_nEntries--;
					m_pCache[i] = m_pCache[m_nEntries];

					memset(&m_pCache[m_nEntries], 0, sizeof(m_pCache[0]));
				}
	
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMaterial::CMaterial(void)
{
	memset(m_szName, 0, sizeof(m_szName));
	memset(m_szFileName, 0, sizeof(m_szFileName));
	memset(m_szKeywords, 0, sizeof(m_szKeywords));

	m_nWidth = 0;
	m_nHeight = 0;
    m_nTextureID = 0;
	m_pData = NULL;
	m_bLoaded = false;
	m_pMaterial = NULL;
	m_TranslucentBaseTexture = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees texture image data and palette.
//-----------------------------------------------------------------------------
CMaterial::~CMaterial(void)
{
	//
	// Free image data.
	//
	if (m_pData != NULL)
	{
		free(m_pData);
		m_pData = NULL;
	}

	/* FIXME: Texture manager shuts down after the material system
	if (m_pMaterial)
	{
		m_pMaterial->DecrementReferenceCount();
		m_pMaterial = NULL;
	}
	*/
}


#define MATERIAL_PREFIX_LEN	10
//-----------------------------------------------------------------------------
// Finds all .VMT files in a particular directory
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialsInDirectory( char const* pDirectoryName, int nDirectoryNameLen,
						IMaterialEnumerator *pEnum, int nContext, int nFlags )
{
	//Assert( Q_strnicmp( pDirectoryName, "materials", 9 ) == 0 );

	char *pWildCard;
	pWildCard = ( char * )stackalloc( nDirectoryNameLen + 7 );
	Q_snprintf( pWildCard, nDirectoryNameLen + 7, "%s/*.vmt", pDirectoryName );

	if ( !g_pFileSystem )
	{
		return false;
	}

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( pWildCard, "GAME", &findHandle );
	while( pFileName )
	{
		if (IsIgnoredMaterial(pFileName))
		{
			pFileName = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		if( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
		{
			// Strip off the 'materials/' part of the material name.
			char *pFileNameWithPath;
			int nAllocSize = nDirectoryNameLen + Q_strlen(pFileName) + 2;
			pFileNameWithPath = (char *)stackalloc( nAllocSize );
			Q_snprintf(	pFileNameWithPath, nAllocSize, "%s/%s", &pDirectoryName[MATERIAL_PREFIX_LEN], pFileName ); 
			Q_strnlwr( pFileNameWithPath, nAllocSize );

			// Strip off the extension...
			char *pExt = Q_strrchr( pFileNameWithPath, '.');
			if (pExt)
				*pExt = 0;

			if (!pEnum->EnumMaterial( pFileNameWithPath, nContext ))
			{
				return false;
			}
		}
		pFileName = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
	return true;
}


//-----------------------------------------------------------------------------
// Discovers all .VMT files lying under a particular directory
// It only finds their names so we can generate shell materials for them
// that we can load up at a later time 
//-----------------------------------------------------------------------------
bool CMaterial::InitDirectoryRecursive( char const* pDirectoryName, 
						IMaterialEnumerator *pEnum, int nContext, int nFlags )
{
	// Make sure this is an ok directory, otherwise don't bother
	if (ShouldSkipMaterial( pDirectoryName + MATERIAL_PREFIX_LEN, nFlags ))
		return true;

	// Compute directory name length
	int nDirectoryNameLen = Q_strlen( pDirectoryName );

	if (!LoadMaterialsInDirectory( pDirectoryName, nDirectoryNameLen, pEnum, nContext, nFlags ))
		return false;

	char *pWildCard = ( char * )stackalloc( nDirectoryNameLen + 5 );
	strcpy(pWildCard, pDirectoryName);
	strcat(pWildCard, "/*.*");
	int nPathStrLen = nDirectoryNameLen + 1;

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( pWildCard, "GAME", &findHandle );
	while( pFileName )
	{
		if (!IsIgnoredMaterial(pFileName))
		{
			if ((pFileName[0] != '.') || (pFileName[1] != '.' && pFileName[1] != 0))
			{
				if( g_pFullFileSystem->FindIsDirectory( findHandle ) )
				{
					int fileNameStrLen = Q_strlen( pFileName );
					char *pFileNameWithPath = ( char * )stackalloc( nPathStrLen + fileNameStrLen + 1 );
					memcpy( pFileNameWithPath, pWildCard, nPathStrLen );
					pFileNameWithPath[nPathStrLen] = '\0';
					Q_strncat( pFileNameWithPath, pFileName, nPathStrLen + fileNameStrLen + 1 );

					if (!InitDirectoryRecursive( pFileNameWithPath, pEnum, nContext, nFlags ))
						return false;
				}
			}
		}
		pFileName = g_pFullFileSystem->FindNext( findHandle );		
	}

	return true;
}


//-----------------------------------------------------------------------------
// Discovers all .VMT files lying under a particular directory
// It only finds their names so we can generate shell materials for them
// that we can load up at a later time 
//-----------------------------------------------------------------------------
void CMaterial::EnumerateMaterials( IMaterialEnumerator *pEnum, const char *szRoot, int nContext, int nFlags )
{
	InitDirectoryRecursive( szRoot, pEnum, nContext, nFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Called from GetFirst/GetNextMaterialName to skip unwanted materials.
// Input  : pszName - Name of material to evaluate.
//			nFlags - One or more of the following:
//				INCLUDE_ALL_MATERIALS
//				INCLUDE_WORLD_MATERIALS
//				INCLUDE_MODEL_MATERIALS
// Output : Returns true to skip, false to not skip this material.
//-----------------------------------------------------------------------------
bool CMaterial::ShouldSkipMaterial(const char *pszName, int nFlags)
{
	static char szStrippedName[MAX_PATH];

	// if NULL skip it
	if( !pszName )
		return true;

	//
	// check against the list of user-defined exclusion directories
	//
	for( int i = 0; i < g_pGameConfig->m_MaterialExcludeCount; i++ )
	{
		// This will guarantee the match is at the start of the string
		const char *pMatchFound = Q_stristr( pszName, g_pGameConfig->m_MaterialExclusions[i].szDirectory );
		if( pMatchFound == pszName )
			return true;
	}
	// also check against any FGD-defined exclusions
	if (pGD != NULL)	
	{
		for( int i = 0; i < pGD->m_FGDMaterialExclusions.Count(); i++ )
		{
			const char *pMatchFound = Q_stristr( pszName, pGD->m_FGDMaterialExclusions[i].szDirectory );
			if( pMatchFound == pszName )
				return true;
		}
	}

	return false;

#if 0
	bool bSkip = false;

	if (pszName != NULL)
	{
		if (!(nFlags & INCLUDE_MODEL_MATERIALS))
		{
			if (_strnicmp(pszName, "models/", 7) == 0)
			{
				bSkip = true;
			}
		}

		if (!(nFlags & INCLUDE_WORLD_MATERIALS))
		{
			if (_strnicmp(pszName, "models/", 7) != 0)
			{
				bSkip = true;
			}
		}
	}
	else
	{
		bSkip = true;
	}

	return(bSkip);
#endif
}	


//-----------------------------------------------------------------------------
// Purpose: Factory. Creates a material by name.
// Input  : pszMaterialName - Name of material, ie "brick/brickfloor01".
// Output : Returns a pointer to the new material object, NULL if the given
//			material did not exist.
//-----------------------------------------------------------------------------
CMaterial *CMaterial::CreateMaterial(const char *pszMaterialName, bool bLoadImmediately, bool* pFound)
{
	Assert (pszMaterialName);

 	CMaterial *pMaterial = new CMaterial;
	Assert( pMaterial );

	// Store off the material name so we can load it later if we need to
	Q_snprintf( pMaterial->m_szFileName, MAX_PATH, pszMaterialName );
	Q_snprintf( pMaterial->m_szName, MAX_PATH, pszMaterialName );

	//
	// Find the material by name and load it.
	//
	if (bLoadImmediately)
	{
		bool bFound = pMaterial->LoadMaterial();

		// Returns if the material was found or not
		if (pFound)
			*pFound = bFound;
	}

	return pMaterial;
}

bool CMaterial::IsIgnoredMaterial( const char *pName )
{
	//TODO: make this a customizable user option?
	if ( !Q_strnicmp(pName, ".svn", 4) || strstr (pName, ".svn") ||
		!Q_strnicmp(pName, "models", 6) || strstr (pName, "models") )
		return true;

	return false;
}
//-----------------------------------------------------------------------------
// Will actually load the material bits
// We don't want to load them all at once because it takes way too long
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterial()
{
	bool bFound = true;
	if (!m_bLoaded)
	{
		if (IsIgnoredMaterial(m_szFileName))
		{
			return false;
		}

		m_bLoaded = true;

		IMaterial *pMat = materials->FindMaterial(m_szFileName, TEXTURE_GROUP_OTHER);
		if ( IsErrorMaterial( pMat ) )
			bFound = false;

		Assert( pMat );

		if (!pMat)
		{
			return false;
		}

		if (!LoadMaterialHeader(pMat))
		{
			// dvs: yeaaaaaaaaah, we're gonna disable this until the spew can be reduced
			//Msg( mwError,"Load material header failed: %s", m_szFileName );

			bFound = false;
			pMat = materials->FindMaterial("debug/debugempty", TEXTURE_GROUP_OTHER);

			if (pMat)
			{
				LoadMaterialHeader(pMat);
			}
		}
	}

	return bFound;
}


//-----------------------------------------------------------------------------
// Reloads owing to a material change
//-----------------------------------------------------------------------------
void CMaterial::Reload( bool bFullReload )
{
	// Don't bother if we're not loaded yet
	if (!m_bLoaded)
		return;

	FreeData();

	if ( m_pMaterial )
	{
		m_pMaterial->DecrementReferenceCount();
	}
	m_pMaterial = materials->FindMaterial(m_szFileName, TEXTURE_GROUP_OTHER);
	Assert( m_pMaterial );

	if ( bFullReload )
		m_pMaterial->Refresh();

	PreviewImageRetVal_t retVal;
	bool translucentBaseTexture;
	ImageFormat eImageFormat;
	int width, height;
	retVal = m_pMaterial->GetPreviewImageProperties(&width, &height, &eImageFormat, &translucentBaseTexture);
	if (retVal == MATERIAL_PREVIEW_IMAGE_BAD)
		return;

	m_nWidth = width;
	m_nHeight = height;
	m_TranslucentBaseTexture = translucentBaseTexture; 

	// Find the keywords for this material from the vmt file.
	bool bFound;
	IMaterialVar *pVar = m_pMaterial->FindVar("%keywords", &bFound, false);
	if (bFound)
	{
		V_strcpy_safe( m_szKeywords, pVar->GetStringValue() );

		// Register the keywords
		g_Textures.RegisterTextureKeywords( this );
	}

	// Make sure to bump the refcount again. Not sure why this wasn't always done (check for leaks).
	if (m_pMaterial)
	{
		m_pMaterial->IncrementReferenceCount();
	}
}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial* CMaterial::GetMaterial( bool bForceLoad )
{
	if ( bForceLoad )
		LoadMaterial();
		
	return m_pMaterial;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterial::DrawIcon( CDC *pDC, CMaterial* pIcon, RECT& dstRect )
{
	if (!pIcon)
		return;

	g_pMaterialImageCache->EnCache(pIcon);

	RECT rect, dst;
	rect.left = 0; rect.right = pIcon->GetWidth();

	// FIXME: Workaround the fact that materials must be power of 2, I want 12 bite
	rect.top = 2; rect.bottom = pIcon->GetHeight() - 2;

	dst = dstRect;
	float dstHeight = dstRect.bottom - dstRect.top;
	float srcAspect = (float)(rect.right - rect.left) / (float)(rect.bottom - rect.top);
	dst.right = dst.left + (dstHeight * srcAspect);
	pIcon->DrawBitmap( pDC, rect, dst );

	dstRect.left += dst.right - dst.left;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
//			dstRect - 
//			detectErrors - 
//-----------------------------------------------------------------------------
void CMaterial::DrawBrowserIcons( CDC *pDC, RECT& dstRect, bool detectErrors )
{
	static CMaterial* pTranslucentIcon = 0;
	static CMaterial* pOpaqueIcon = 0;
	static CMaterial* pSelfIllumIcon = 0;
	static CMaterial* pBaseAlphaEnvMapMaskIcon = 0;
	static CMaterial* pErrorIcon = 0;

	if (!pTranslucentIcon)
	{
		pTranslucentIcon			= CreateMaterial("editor/translucenticon", true);
		pOpaqueIcon					= CreateMaterial("editor/opaqueicon", true);
		pSelfIllumIcon				= CreateMaterial("editor/selfillumicon", true);
		pBaseAlphaEnvMapMaskIcon	= CreateMaterial("editor/basealphaenvmapmaskicon", true);
		pErrorIcon					= CreateMaterial("editor/erroricon", true);

		Assert( pTranslucentIcon && pOpaqueIcon && pSelfIllumIcon && pBaseAlphaEnvMapMaskIcon && pErrorIcon );
	}

	bool error = false;

	IMaterial* pMaterial = GetMaterial();
	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT ) )
	{
		DrawIcon( pDC, pTranslucentIcon, dstRect );
		if (detectErrors)
		{
			error = error || !m_TranslucentBaseTexture; 
		}
	}
	else
	{
		DrawIcon( pDC, pOpaqueIcon, dstRect ); 
	}

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ))
	{
		DrawIcon( pDC, pSelfIllumIcon, dstRect ); 
		if (detectErrors)
		{
			error = error || !m_TranslucentBaseTexture;
		}
	}

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_BASEALPHAENVMAPMASK ))
	{
		DrawIcon( pDC, pBaseAlphaEnvMapMaskIcon, dstRect ); 
		if (detectErrors)
		{
			error = error || !m_TranslucentBaseTexture;
		}
	}

	if (error)
	{
		DrawIcon( pDC, pErrorIcon, dstRect );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
//			srcRect - 
//			dstRect - 
//-----------------------------------------------------------------------------
void CMaterial::DrawBitmap( CDC *pDC, RECT& srcRect, RECT& dstRect )
{
	static struct
	{
		BITMAPINFOHEADER bmih;
		unsigned short colorindex[256];
	} bmi;

	int srcWidth = srcRect.right - srcRect.left;
	int srcHeight = srcRect.bottom - srcRect.top;

	BITMAPINFOHEADER &bmih = bmi.bmih;
	memset(&bmih, 0, sizeof(bmih));
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = srcWidth;
	bmih.biHeight = -srcHeight;
	bmih.biCompression = BI_RGB;

	bmih.biBitCount = 24;
	bmih.biPlanes = 1;

	static BOOL bInit = false;
	if (!bInit)
	{
		bInit = true;
		for (int i = 0; i < 256; i++)
		{
			bmi.colorindex[i] = i;
		}
	}

	int dest_width = dstRect.right - dstRect.left;
	int dest_height = dstRect.bottom - dstRect.top;

	// ** bits **
	SetStretchBltMode(pDC->m_hDC, COLORONCOLOR);
	if (StretchDIBits(pDC->m_hDC, dstRect.left, dstRect.top, dest_width, dest_height, 
		srcRect.left, -srcRect.top, srcWidth, srcHeight, m_pData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR)
	{
		Msg(mwError, "CMaterial::Draw(): StretchDIBits failed.");
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDC - 
//			rect - 
//			iFontHeight - 
//			dwFlags - 
//-----------------------------------------------------------------------------
void CMaterial::Draw(CDC *pDC, RECT& rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData)//, BrowserData_t *pBrowserData)
{
	g_pMaterialImageCache->EnCache(this);
	if (!this->HasData())
	{
		return;
	}
	
	if (m_nWidth <= 0)
	{
NoData:
		// draw "no data"
		CFont *pOldFont = (CFont*) pDC->SelectStockObject(ANSI_VAR_FONT);
		COLORREF cr = pDC->SetTextColor(RGB(0xff, 0xff, 0xff));
		COLORREF cr2 = pDC->SetBkColor(RGB(0, 0, 0));

		// draw black rect first
		pDC->FillRect(&rect, CBrush::FromHandle(HBRUSH(GetStockObject(BLACK_BRUSH))));

		// then text
		pDC->TextOut(rect.left+2, rect.top+2, "No Image", 8);
		pDC->SelectObject(pOldFont);
		pDC->SetTextColor(cr);
		pDC->SetBkColor(cr2);
		return;
	}

	// no data -
	if (!m_pData)
	{
		// try to load -
		if (!Load())
		{
			// can't load -
			goto NoData;
		}
	}

	// Draw the material image
	RECT srcRect, dstRect;
	srcRect.left = 0;
	srcRect.top = 0;
	srcRect.right = m_nWidth;
	srcRect.bottom = m_nHeight;
	dstRect = rect;

	if (DrawTexData.nFlags & drawCaption)
	{
		dstRect.bottom -= iFontHeight +  4;
	}
	if (DrawTexData.nFlags & drawIcons)
	{
		dstRect.bottom -= iIconHeight;
	}

	if (!(DrawTexData.nFlags & drawResizeAlways))
	{
		if (m_nWidth < dstRect.right - dstRect.left )
		{
			dstRect.right = dstRect.left + m_nWidth;
		}

		if (m_nHeight < dstRect.bottom - dstRect.top )
		{
			dstRect.bottom = dstRect.top + m_nHeight;
		}
	}
	DrawBitmap( pDC, srcRect, dstRect );

	// Draw the icons
	if (DrawTexData.nFlags & drawIcons)
	{
		dstRect = rect;
		if (DrawTexData.nFlags & drawCaption)
		{
			dstRect.bottom -= iFontHeight + 5; 
		}
		dstRect.top = dstRect.bottom - iIconHeight; 
		DrawBrowserIcons(pDC, dstRect, (DrawTexData.nFlags & drawErrors) != 0 );
	}

	// ** caption **
	if (DrawTexData.nFlags & drawCaption)
	{
		// draw background for name
		CBrush brCaption(RGB(0, 0, 255));
		CRect rcCaption(rect);
		
		rcCaption.top = rcCaption.bottom - (iFontHeight + 5);
		pDC->FillRect(rcCaption, &brCaption);

		// draw name
		char szShortName[MAX_PATH];
		int iLen = GetShortName(szShortName);
		pDC->TextOut(rect.left, rect.bottom - (iFontHeight + 4), szShortName, iLen);

		// draw usage count
		if (DrawTexData.nFlags & drawUsageCount)
		{
			CString str;
			str.Format("%d", DrawTexData.nUsageCount);
			CSize size = pDC->GetTextExtent(str);
			pDC->TextOut(rect.right - size.cx, rect.bottom - (iFontHeight + 4), str);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterial::FreeData( void )
{
	free( m_pData );
	m_pData = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string of comma delimited keywords associated with this
//			material.
// Input  : pszKeywords - Buffer to receive keywords, NULL to query string length.
// Output : Returns the number of characters in the keyword string.
//-----------------------------------------------------------------------------
int CMaterial::GetKeywords(char *pszKeywords) const
{
	// To access keywords, we have to have the header loaded
	const_cast<CMaterial*>(this)->Load();
	if (pszKeywords != NULL)
	{
		strcpy(pszKeywords, m_szKeywords);
	}

	return(strlen(m_szKeywords));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
// Output : int
//-----------------------------------------------------------------------------
int CMaterial::GetShortName(char *pszName) const
{
	if (pszName != NULL)
	{
		strcpy(pszName, m_szName);
	}

	return(strlen(m_szName));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : material - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialHeader( IMaterial *pMat )
{

	PreviewImageRetVal_t retVal;
	bool translucentBaseTexture;
	ImageFormat eImageFormat;
	int width, height;
	retVal = CPreviewImagePropertiesCache::GetPreviewImageProperties( pMat, &width, &height, &eImageFormat, &translucentBaseTexture);
	if (retVal == MATERIAL_PREVIEW_IMAGE_BAD)
		return false;

	m_pMaterial = pMat;
	m_pMaterial->IncrementReferenceCount();

	m_nWidth = width;
	m_nHeight = height;
	m_TranslucentBaseTexture = translucentBaseTexture; 

	// Find the keywords for this material from the vmt file.
	bool bFound;
	IMaterialVar *pVar = pMat->FindVar("%keywords", &bFound, false);
	if (bFound)
	{
		V_strcpy_safe( m_szKeywords, pVar->GetStringValue() );

		// Register the keywords
		g_Textures.RegisterTextureKeywords( this );
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the full path of the file from which this material was loaded.
//-----------------------------------------------------------------------------
const char *CMaterial::GetFileName( void ) const
{
	return(m_szFileName);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMaterial::IsWater( void ) const
{
	bool bFound;
	IMaterialVar *pVar = m_pMaterial->FindVar( "$surfaceprop", &bFound, false );
	if ( bFound )
	{
		if ( !strcmp( "water", pVar->GetStringValue() ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGB format to the buffer
// Input  : pImageRGB - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGB image in bytes.
//-----------------------------------------------------------------------------
int CMaterial::GetImageDataRGB( void *pImageRGB )
{
	Assert( m_nWidth > 0 );

	if ( pImageRGB != NULL )
	{
		Load();

		g_pMaterialImageCache->EnCache( this );
		if (!this->HasData())
		{
			return(NULL);
		}

		unsigned char *src, *dst;
		src = ( unsigned char * )m_pData;
		dst = (unsigned char *)pImageRGB;
		for( ; src < ( unsigned char * )m_pData + m_nWidth * m_nHeight * 3; src += 3, dst += 3 )
		{
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
		}

		return(	m_nWidth * m_nHeight * 3 );
	}

	return(	m_nWidth * m_nHeight * 3 );
}


//-----------------------------------------------------------------------------
// Purpose: If the buffer pointer passed in is not NULL, copies the image data
//			in RGBA format to the buffer
// Input  : pImageRGBA - Pointer to buffer that receives the image data. If the
//				pointer is NULL, no data is copied, only the data size is returned.
// Output : Returns a the size of the RGBA image in bytes.
//-----------------------------------------------------------------------------
int CMaterial::GetImageDataRGBA(void *pImageRGBA)
{
	Assert( m_nWidth > 0 );

	if (pImageRGBA != NULL)
	{
		Load();

		g_pMaterialImageCache->EnCache(this);
		if (!this->HasData())
		{
			return(NULL);
		}

		unsigned char *src, *dst;
		src = (unsigned char *)m_pData;
		dst = (unsigned char *)pImageRGBA;

		while (src < (unsigned char *)m_pData + m_nWidth * m_nHeight * 4);	
		{
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = 0;

			src += 4;
			dst += 4;
		}
	}

	return(m_nWidth * m_nHeight * 4);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : size - 
//-----------------------------------------------------------------------------
void CMaterial::GetSize(SIZE &size) const
{
	const_cast<CMaterial*>(this)->Load();
	Assert( m_nWidth >= 0 );

	size.cx = m_nWidth;
	size.cy = m_nHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Loads this material's image from disk if it is not already loaded.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::Load( void )
{
	LoadMaterial();
	return true;
}


//-----------------------------------------------------------------------------
// cache in the image size only when we need to
//-----------------------------------------------------------------------------
int CMaterial::GetImageWidth(void) const
{
	const_cast<CMaterial*>(this)->Load();
	return(m_nWidth);
}

int CMaterial::GetImageHeight(void) const
{
	const_cast<CMaterial*>(this)->Load();
	return(m_nHeight);
}

int CMaterial::GetWidth(void) const
{
	const_cast<CMaterial*>(this)->Load();
	return(m_nWidth);
}

int CMaterial::GetHeight(void) const
{
	const_cast<CMaterial*>(this)->Load();
	return(m_nHeight);
}

float CMaterial::GetDecalScale(void) const
{
	const_cast<CMaterial*>(this)->Load();

	IMaterialVar *decalScaleVar;
	bool found;
	
	decalScaleVar = m_pMaterial->FindVar( "$decalScale", &found, false );
	if( !found )
	{
		return 1.0f;
	}
	else
	{
		return decalScaleVar->GetFloatValue();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::LoadMaterialImage( void )
{
	Load();

	if ((!m_nWidth) || (!m_nHeight))
		return(false);

	m_pData = malloc(m_nWidth * m_nHeight * 3);
	Assert(m_pData);

	ImageFormat imageFormat;

//	if( _strnicmp( m_pMaterial->GetName(), "decals", 6 ) == 0 )
//	{
//		imageFormat = IMAGE_FORMAT_BGR888_BLUESCREEN;
//	}
//	else
	{
		imageFormat = IMAGE_FORMAT_BGR888;
	}
	
	PreviewImageRetVal_t retVal;
	retVal = m_pMaterial->GetPreviewImage( (unsigned char *)m_pData, m_nWidth, m_nHeight, imageFormat );
	return (retVal != MATERIAL_PREVIEW_IMAGE_BAD); 
}


static void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
	pConfig->bEditMode = true;
	pConfig->m_nAASamples = 0;
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP, true);
	// When I do this the model browser layout is horked...
	// pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_USING_MULTIPLE_WINDOWS, true );
}


static char const *s_rt_names[]={"_rt_albedo","_rt_normal","_rt_position","_rt_flags",
							   "_rt_accbuf_0","_rt_accbuf_1"};
ImageFormat s_rt_formats[]={ IMAGE_FORMAT_RGBA32323232F, IMAGE_FORMAT_RGBA32323232F, 
							 IMAGE_FORMAT_RGBA32323232F, IMAGE_FORMAT_RGBA32323232F,
							 IMAGE_FORMAT_RGBA16161616F, IMAGE_FORMAT_RGBA16161616F };

// ImageFormat s_rt_formats[]={ 
// 	IMAGE_FORMAT_RGBA16161616F, IMAGE_FORMAT_RGBA16161616F,
// 	IMAGE_FORMAT_RGBA16161616F, IMAGE_FORMAT_RGBA16161616F,
// 	IMAGE_FORMAT_RGBA16161616F, IMAGE_FORMAT_RGBA16161616F,
// 	IMAGE_FORMAT_RGBA16161616F, IMAGE_FORMAT_RGBA16161616F };

static CTextureReference sg_ExtraFP16Targets[NELEMS(s_rt_names)];


void AllocateLightingPreviewtextures(void)
{
	static bool bHaveAllocated=false;
	if (! bHaveAllocated )
	{
		bHaveAllocated = true;
		MaterialSystemInterface()->BeginRenderTargetAllocation();
		for(int idx=0;idx<NELEMS(sg_ExtraFP16Targets);idx++)
			sg_ExtraFP16Targets[idx].Init(
				materials->CreateNamedRenderTargetTextureEx2(
					s_rt_names[idx],
					512, 512, RT_SIZE_DEFAULT, s_rt_formats[idx],
					MATERIAL_RT_DEPTH_SHARED, 
					TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
					CREATERENDERTARGETFLAGS_HDR )
				);
		
		// End block in which all render targets should be allocated (kicking off an Alt-Tab type
		// behavior)
		MaterialSystemInterface()->EndRenderTargetAllocation();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMaterial::Initialize( HWND hwnd )
{
	// NOTE: This gets set to true later upon creating a 3d view.
	g_materialSystemConfig = materials->GetCurrentConfigForVideoCard();
	InitMaterialSystemConfig( &g_materialSystemConfig );

	// Create a cache for material images (for browsing and uploading to the driver).
	if (g_pMaterialImageCache == NULL)
	{
		g_pMaterialImageCache = new CMaterialImageCache(500);
		if (g_pMaterialImageCache == NULL)
			return false ;
	}

	materials->OverrideConfig( g_materialSystemConfig, false );

	// Set the mode
	// When setting the mode, we need to grab the parent window
	// since that's going to enclose all our little render windows
	g_materialSystemConfig.m_VideoMode.m_Width = g_materialSystemConfig.m_VideoMode.m_Height = 0;
	g_materialSystemConfig.m_VideoMode.m_Format = IMAGE_FORMAT_BGRA8888;
	g_materialSystemConfig.m_VideoMode.m_RefreshRate = 0;
	g_materialSystemConfig.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	g_materialSystemConfig.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );


	if (!MaterialSystemInterface()->SetMode( hwnd, g_materialSystemConfig ) )
		return false;

	return true;
}
						    


//-----------------------------------------------------------------------------
// Purpose: Restores the material system to an uninitialized state.
//-----------------------------------------------------------------------------
void CMaterial::ShutDown(void)
{
	for ( int i = 0; i < NELEMS(sg_ExtraFP16Targets); ++i )
	{
		sg_ExtraFP16Targets[i].Shutdown();
	}

	if (materials != NULL)
	{
		materials->UncacheAllMaterials();
	}

	delete g_pMaterialImageCache;
	g_pMaterialImageCache = NULL;
}


