//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of IEditorTexture interface for materials.
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATERIAL_H
#define MATERIAL_H
#pragma once


#include "IEditorTexture.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"


class IMaterial;
class CMaterialCache;
class IMaterialSystem;
class IMaterialSystemHardwareConfig;
struct MaterialSystem_Config_t;
struct MaterialCacheEntry_t;


#define INCLUDE_MODEL_MATERIALS		0x01
#define INCLUDE_WORLD_MATERIALS		0x02
#define INCLUDE_ALL_MATERIALS		0xFFFFFFFF


//-----------------------------------------------------------------------------
// Inherit from this to enumerate materials 
//-----------------------------------------------------------------------------
class IMaterialEnumerator
{
public:
	virtual bool EnumMaterial( const char *pMaterialName, int nContext ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMaterial : public IEditorTexture
{
public:
	static bool Initialize( HWND hwnd );
	static void ShutDown(void);
	static void	EnumerateMaterials( IMaterialEnumerator *pEnum, const char *szRoot, int nContext, int nFlags = INCLUDE_ALL_MATERIALS );
	static CMaterial *CreateMaterial( const char *pszMaterialName, bool bLoadImmediately, bool* pFound = 0 );

	virtual ~CMaterial(void);

	void Draw(CDC *pDC, RECT& rect, int iFontHeight, int iIconHeight, DrawTexData_t &DrawTexData); //DWORD dwFlags = (drawCaption|drawIcons));

	void FreeData(void);

	inline const char *GetName(void) const
	{
		return(m_szName);
	}
	int GetShortName(char *pszName) const;

	int GetKeywords(char *pszKeywords) const;

	void GetSize(SIZE &size) const;

	int GetImageDataRGB(void *pImageRGB);
	int GetImageDataRGBA(void *pImageRGBA);

	// Image dimensions
	int GetImageWidth(void) const;
	int GetImageHeight(void) const;
	int GetWidth(void) const;
	int GetHeight(void) const;

	float GetDecalScale(void) const;
	
	const char *GetFileName(void) const;

	inline CPalette *GetPalette(void) const
	{
		return(NULL);
	}

	inline int GetSurfaceAttributes(void) const
	{
		return(0);
	}

	inline int GetSurfaceContents(void) const
	{
		return(0);
	}

	inline int GetSurfaceValue(void) const
	{
		return(0);
	}

	inline TEXTUREFORMAT GetTextureFormat(void) const
	{
		return(tfVMT);
	}

	inline int GetTextureID(void) const
	{
		return(m_nTextureID);
	}

	bool HasAlpha(void) const
	{
		return(false);
	}

	inline bool HasData(void) const
	{
		return((m_nWidth != 0) && (m_nHeight != 0));
	}

	inline bool HasPalette(void) const
	{
		return(false);
	}

	inline bool IsDummy(void) const
	{
		return(false);
	}

	bool Load(void);
	void Reload( bool bFullReload );

	inline bool IsLoaded() const
	{
		return m_bLoaded;
	}

	inline void SetTextureID(int nTextureID)
	{
		m_nTextureID = nTextureID;
	}

	bool IsWater( void ) const;

	virtual IMaterial* GetMaterial( bool bForceLoad=true );

protected:
	// Used to draw the bitmap for the texture browser
	void DrawBitmap( CDC *pDC, RECT& srcRect, RECT& dstRect );
	void DrawBrowserIcons( CDC *pDC, RECT& dstRect, bool detectErrors );
	void DrawIcon( CDC *pDC, CMaterial* pIcon, RECT& dstRect );

	static bool ShouldSkipMaterial(const char *pszName, int nFlags);

	// Finds all .VMT files in a particular directory
	static bool LoadMaterialsInDirectory( char const* pDirectoryName, int nDirectoryNameLen, 
						IMaterialEnumerator *pEnum, int nContext, int nFlags );

	// Discovers all .VMT files lying under a particular directory recursively
	static bool InitDirectoryRecursive( char const* pDirectoryName, 
						IMaterialEnumerator *pEnum, int nContext, int nFlags );

	CMaterial(void);
	bool LoadMaterialHeader(IMaterial *material);
	bool LoadMaterialImage();

	static bool IsIgnoredMaterial( const char *pName );

	// Will actually load the material bits
	// We don't want to load them all at once because it takes way too long
	bool LoadMaterial();

	char m_szName[MAX_PATH];
	char m_szFileName[MAX_PATH];
	char m_szKeywords[MAX_PATH];

	int m_nTextureID;			// Uniquely identifies this texture in all 3D renderers.

	int m_nWidth;				// Texture width in texels.
	int m_nHeight;				// Texture height in texels.
	bool m_TranslucentBaseTexture;
	bool m_bLoaded;				// We don't load these immediately; only when needed..

	void *m_pData;				// Loaded texel data (NULL if not loaded).

	IMaterial *m_pMaterial;

	friend class CMaterialImageCache;
};


typedef CMaterial *CMaterialPtr;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMaterialCache
{
	public:

		CMaterialCache(void);
		~CMaterialCache(void);

		inline bool CacheExists(void);
		bool Create(int nMaxEntries);

		CMaterial *CreateMaterial(const char *pszMaterialName);
		void AddRef(CMaterial *pMaterial);
		void Release(CMaterial *pMaterial);

	protected:

		CMaterial *FindMaterial(const char *pszMaterialName);
		void AddMaterial(CMaterial *pMaterial);

		MaterialCacheEntry_t *m_pCache;
		int m_nMaxEntries;
		int m_nEntries;
};


//-----------------------------------------------------------------------------
// Purpose: Returns true if the cache has been allocated, false if not.
//-----------------------------------------------------------------------------
inline bool CMaterialCache::CacheExists(void)
{
	return((m_pCache != NULL) && (m_nMaxEntries > 0));
}


//-----------------------------------------------------------------------------
// returns the material system interface + config
//-----------------------------------------------------------------------------

inline IMaterialSystem *MaterialSystemInterface()
{
	return materials;
}

inline MaterialSystem_Config_t& MaterialSystemConfig()
{
	extern MaterialSystem_Config_t g_materialSystemConfig;
	return g_materialSystemConfig;
}

inline IMaterialSystemHardwareConfig* MaterialSystemHardwareConfig()
{
	extern IMaterialSystemHardwareConfig* g_pMaterialSystemHardwareConfig;
	return g_pMaterialSystemHardwareConfig;
}

//--------------------------------------------------------------------------------
// call AllocateLightingPreviewtextures to make sure necessary rts are allocated
//--------------------------------------------------------------------------------
void AllocateLightingPreviewtextures(void);

#endif // MATERIAL_H
