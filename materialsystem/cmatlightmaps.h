//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CMATERIALLIGHTMAPS_H
#define CMATERIALLIGHTMAPS_H

#include "tier1/utlvector.h"
#include "imagepacker.h"

#ifndef MATSYS_INTERNAL
#error "This file is private to the implementation of IMaterialSystem/IMaterialSystemInternal"
#endif

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------

class CMaterialSystem;
class IMatRenderContextInternal;
class CMaterialDict;
class IMaterial;
class IMaterialInternal;
class FloatBitMap_t;
typedef int ShaderAPITextureHandle_t;
struct MaterialSystem_SortInfo_t;
typedef unsigned short MaterialHandle_t;

//-----------------------------------------------------------------------------
// Manager of material system lightmaps
//-----------------------------------------------------------------------------
const int COUNT_DYNAMIC_LIGHTMAP_PAGES = 1;

class CMatLightmaps
{
public:
	CMatLightmaps();

	void Shutdown( );

	//------------------------------------------------------------
	// Methods exposed in IMaterialSystem
	//------------------------------------------------------------
	void		BeginLightmapAllocation( void );
	void		EndLightmapAllocation( void );

	int			AllocateLightmap( int width, int height, 
		                                  int offsetIntoLightmapPage[2],
										  IMaterial *pMaterial );
	int			AllocateWhiteLightmap( IMaterial *pMaterial );
	// NOTE: This returns a lightmap page ID, not a sortID like AllocateLightmap!!!!
	int			AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID );

	int			GetNumSortIDs( void );
	void		GetSortInfo( MaterialSystem_SortInfo_t *sortInfoArray );

	void		UpdateLightmap( int lightmapPageID, int lightmapSize[2],
								int offsetIntoLightmapPage[2], 
								float *pFloatImage, float *pFloatImageBump1,
								float *pFloatImageBump2, float *pFloatImageBump3 );

	void		GetLightmapPageSize( int lightmapPageID, int *width, int *height ) const;

	void		ResetMaterialLightmapPageInfo( void );

	//------------------------------------------------------------
	// Methods exposed in IMaterialSystemInternal
	//------------------------------------------------------------
	int			GetLightmapWidth( int lightmap ) const;
	int			GetLightmapHeight( int lightmap ) const;

	//------------------------------------------------------------
	// Methods used by other material system components
	//------------------------------------------------------------

	// Releases/restores lightmap pages
	void		ReleaseLightmapPages();
	void		RestoreLightmapPages();

	void		EnableLightmapFiltering( bool enabled );

	int			GetNumLightmapPages() const									{ return m_NumLightmapPages;}
	ShaderAPITextureHandle_t GetLightmapPageTextureHandle( int lightmap )	{ return m_LightmapPageTextureHandles[lightmap];	}
	bool		IsDynamicLightmap( int lightmap ) const { return (lightmap >= m_firstDynamicLightmap ) ? true : false; }

	CMaterialSystem *GetMaterialSystem() const;

	void		BeginUpdateLightmaps();
	void		EndUpdateLightmaps();

private:

	// Gets the maximum lightmap page size...
	int			GetMaxLightmapPageWidth() const;
	int			GetMaxLightmapPageHeight() const;

	void		CleanupLightmaps();

	// Allocate lightmap textures in D3D
	void		AllocateLightmapTexture( int lightmap );

	// Initializes lightmap bits
	void		InitLightmapBits( int lightmap );

	// assumes m_LightmapPixelWriter is already set up - results written there
	void		BumpedLightmapBitsToPixelWriter_LDR( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
													float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );
	void		BumpedLightmapBitsToPixelWriter_HDRF( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
		float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );

	void		BumpedLightmapBitsToPixelWriter_HDRI( float* pFloatImage, float *pFloatImageBump1, float *pFloatImageBump2, 
		float *pFloatImageBump3, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );

	void		LightmapBitsToPixelWriter_LDR( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );
	void		LightmapBitsToPixelWriter_HDRF( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );
	void		LightmapBitsToPixelWriter_HDRI( float* pFloatImage, int pLightmapSize[2], int pOffsetIntoLightmapPage[2], FloatBitMap_t *pfmOut );

	// For computing sort info
	void ComputeSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha );
	void ComputeWhiteLightmappedSortInfo( MaterialSystem_SortInfo_t* pInfo, int& sortId, bool alpha );

	void		EnumerateMaterials( void );

	// Lock a lightmap for update.
	bool		LockLightmap( int lightmap );



	//------------------------------------------------------------
	// Accessor helpers
	//------------------------------------------------------------

	IMaterialInternal* GetCurrentMaterialInternal() const;
	void SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial);
	IMaterialInternal*  GetMaterialInternal( MaterialHandle_t idx ) const;
	const IMatRenderContextInternal *GetRenderContextInternal() const;
	IMatRenderContextInternal *GetRenderContextInternal();
	const CMaterialDict *GetMaterialDict() const;
	CMaterialDict *GetMaterialDict();

	//------------------------------------------------------------

	struct LightmapPageInfo_t
	{
		unsigned short m_Width;
		unsigned short m_Height;
		int	m_Flags;
	};

	struct dynamiclightmap_t
	{
		void Init()
		{
			lightmapLocked = -1;
			frameID = 0;
			currentDynamicIndex = 0;
			for ( int i = 0; i < COUNT_DYNAMIC_LIGHTMAP_PAGES; i++ )
			{
				lightmapLockFrame[i] = 0;
			}
		}

		int lightmapLocked;
		int frameID;
		int currentDynamicIndex;
		int lightmapLockFrame[COUNT_DYNAMIC_LIGHTMAP_PAGES];
		CImagePacker imagePackers[COUNT_DYNAMIC_LIGHTMAP_PAGES];
	};


	CUtlVector<CImagePacker>			m_ImagePackers;
	int									m_numSortIDs;
	IMaterialInternal					*m_currentWhiteLightmapMaterial;

	LightmapPageInfo_t					*m_pLightmapPages;
	CUtlVector<ShaderAPITextureHandle_t> m_LightmapPageTextureHandles;
	int									m_NumLightmapPages;
	int									m_nUpdatingLightmapsStackDepth;
	int									m_firstDynamicLightmap;
	CPixelWriter						m_LightmapPixelWriter;
	int									m_nLockedLightmap; // -1 for nothing locked.
	dynamiclightmap_t					m_dynamic;
	FloatBitMap_t						**m_pLightmapDataPtrArray;

	enum LightmapsState
	{
		STATE_DEFAULT,		//	Lightmaps in default state - all operations allowed
		STATE_RELEASED		//	Lightmaps in released state - usually due to lost/released D3D device, D3D operations disallowed, Restore() call to follow
	};
	LightmapsState						m_eLightmapsState;
};

//-----------------------------------------------------------------------------

#endif // CMATERIALLIGHTMAPS_H
