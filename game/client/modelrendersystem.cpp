//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// Fast path model rendering
//
//===========================================================================//

#include "cbase.h"
#include "modelrendersystem.h"
#include "model_types.h"
#include "iviewrender.h"
#include "tier3/tier3.h"
#include <algorithm>
#include "tier1/memstack.h"
#include "engine/ivdebugoverlay.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/materialsystemutil.h"
#include "tier0/vprof.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Convars defined by other systems
//-----------------------------------------------------------------------------
ConVar r_lod( "r_lod", "-1" );
//ConVar r_shadowlod( "r_shadowlod", "-1" );
ConVar r_drawmodellightorigin( "r_DrawModelLightOrigin", "0", FCVAR_CHEAT );
extern ConVar g_CV_FlexSmooth;
extern ConVar r_fastzreject;

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class CModelRenderSystem : public CAutoGameSystem, public IModelRenderSystem
{
	// Methods of IModelRenderSystem
public:
	virtual void DrawModels( ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode );
	virtual void ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData );
	virtual void CleanupTranslucentTempData( TranslucentTempData_t *pTempData );
	virtual IMaterial *GetFastPathColorMaterial() { return m_DebugMaterial; }

	// Methods of IGameSystem
public:
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

	// Other public methods
public:
	CModelRenderSystem();
	virtual ~CModelRenderSystem();

private:
	struct ModelListNode_t
	{
		ModelRenderSystemData_t m_Entry;
		int32 m_nInitialListIndex : 24;
		int32 m_bBoneMerge : 1;
		int32 m_nLOD : 7;
		ShaderStencilState_t *m_pStencilState;
		ModelListNode_t *m_pNext;
	};

	struct RenderModelInfo_t : public StudioArrayInstanceData_t
	{
		ModelRenderSystemData_t m_Entry;
		ModelInstanceHandle_t m_hInstance;
		matrix3x4a_t* m_pBoneToWorld;
		uint32 m_nInitialListIndex : 24;
		uint32 m_bSetupBonesOnly : 1;
		uint32 m_bBoneMerge : 1;
	};
						   
	struct ModelListByType_t : public StudioModelArrayInfo_t
	{
		RenderableLightingModel_t m_nLightingModel;
		const model_t *m_pModel;
		ModelListNode_t *m_pFirstNode;
		int m_nCount;
		int m_nSetupBoneCount;
		uint32 m_nParentDepth : 31;
		uint32 m_bWantsStencil : 1;
		RenderModelInfo_t *m_pRenderModels;
		ModelListByType_t *m_pNextLightingModel;

		// speed up std::sort by implementing these
		ModelListByType_t &operator=( const ModelListByType_t &rhs )
		{
			memcpy( this, &rhs, sizeof( ModelListByType_t ) );
			return *this;
		}

		ModelListByType_t() {}

		ModelListByType_t( const ModelListByType_t &rhs )
		{
			memcpy( this, &rhs, sizeof( ModelListByType_t ) );
		}
	};

	struct LightingList_t
	{
		ModelListByType_t *m_pFirstModel;
		int m_nCount;
		int m_nTotalModelCount;
	};

private:
	int BucketModelsByMDL( ModelListByType_t *pModelList, ModelListNode_t *pModelListNodes, ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode, int *pModelsRenderingStencilCountOut );
	bool AddModelToLists( int &nModelTypeCount, ModelListByType_t *pModelList, int &nModelNodeCount, ModelListNode_t *pModelListNodes, int nDataIndex, ModelRenderSystemData_t &data, ModelRenderMode_t renderMode );
	void SortBucketsByDependency( int nModelTypeCount, ModelListByType_t *pModelList, LightingList_t *pLightingList ); 
	void ComputeModelLODs( int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode, ModelRenderMode_t renderMode );
	void SlamModelLODs( int nLOD, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode );
	void SortModels( RenderModelInfo_t *pSortedModelListNode, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode );
	static bool SortLessFunc( const RenderModelInfo_t &left, const RenderModelInfo_t &right );
	void SetupBones( int nModelTypeCount, ModelListByType_t *pModelList );
	void SetupFlexes( int nModelTypeCount, ModelListByType_t *pModelList );
	void ComputeLightingOrigin( ModelListByType_t &list, LightingQuery_t *pLightingQuery, int nQueryStride );
	int SetupLighting( LightingList_t *pLightingList, int nModelTypeCount, ModelListByType_t *pModelList, DataCacheHandle_t *pColorMeshHandles, ModelRenderMode_t renderMode );
	void RenderModels( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, ModelRenderMode_t renderMode );
	void SetupTranslucentData( int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, TranslucentInstanceRenderData_t *pRenderData );
	void SetupFlashlightsAndDecals( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, RenderModelInfo_t *pModelInfo, ModelRenderMode_t renderMode );
	void SetupPerInstanceColorModulation( int nModelTypeCount, ModelListByType_t *pModelList );
	void DebugDrawLightingOrigin( const ModelListByType_t &list, const RenderModelInfo_t &model );
	int BuildLightingList( ModelListByType_t **ppLists, unsigned char *pFlags, int *pTotalModels, const LightingList_t &lightingList );
	int SetupStaticPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandles );
	void SetupStandardLighting( LightingList_t &lightingList );
	int SetupPhysicsPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandles );

	
	void HookUpStaticLightingState( int nCount, ModelListByType_t **ppLists, unsigned char *pFlags, ITexture **ppEnvCubemap, MaterialLightingState_t *pLightingState, MaterialLightingState_t *pDecalLightingState, ColorMeshInfo_t **ppColorMeshInfo );
	void RenderDebugOverlays( int nModelTypeCount, ModelListByType_t *pModelList, ModelRenderMode_t renderMode );
	void RenderVCollideDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList );
	void RenderBBoxDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList );
	int ComputeParentDepth( C_BaseEntity *pEnt );
	static bool DependencySortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right );
	static bool StencilSortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right );

	CMemoryStack m_BoneToWorld;
	CTextureReference m_DefaultCubemap;
	CMaterialReference m_DebugMaterial;
	CMaterialReference m_ShadowBuild;
	IMatRenderContext *m_pRenderContext;

	CUtlMemoryFixedGrowable< DataCacheHandle_t, 1024 > m_ColorMeshHandles;
	CUtlMemoryFixedGrowable< ModelListByType_t, 128 > m_ModelList;
	CUtlMemoryFixedGrowable< ModelListNode_t, 1024 > m_ModelListNode;
	CUtlMemoryFixedGrowable< RenderModelInfo_t, 1024 > m_RenderModelInfo;
	int m_nColorMeshHandles;
	int m_nModelTypeCount;
	int m_nTotalModelCount;
	bool m_bShadowDepth;
	bool m_bHasInstanceData;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CModelRenderSystem s_ModelRenderSystem;
extern IModelRenderSystem *g_pModelRenderSystem = &s_ModelRenderSystem;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CModelRenderSystem::CModelRenderSystem()
{
	m_bHasInstanceData = false;
	m_BoneToWorld.Init( 1 * 1024 * 1024, 32 * 1024, 0, 32 );
}

CModelRenderSystem::~CModelRenderSystem()
{
	m_BoneToWorld.Term();
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CModelRenderSystem::LevelInitPostEntity()
{
	m_DefaultCubemap.Init( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP );
	m_DebugMaterial.Init( "debug/debugempty", TEXTURE_GROUP_OTHER );
	m_ShadowBuild.Init( "engine/shadowbuild", TEXTURE_GROUP_OTHER );
}

void CModelRenderSystem::LevelShutdownPreEntity()
{
	m_DefaultCubemap.Shutdown();
	m_DebugMaterial.Shutdown();
	m_ShadowBuild.Shutdown();
}


//-----------------------------------------------------------------------------
// returns bone setup dependency depth
//-----------------------------------------------------------------------------
int CModelRenderSystem::ComputeParentDepth( C_BaseEntity *pEnt )
{
	if ( !pEnt )
		return 0;

	int nDepth = 0;
	while ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && pEnt->GetParentAttachment() > 0 ) )
	{
		++nDepth;
		pEnt = pEnt->GetMoveParent();
	}

	return nDepth;
}


//-----------------------------------------------------------------------------
// Adds a model to the appropriate render lists
//-----------------------------------------------------------------------------
bool CModelRenderSystem::AddModelToLists( int &nModelTypeCount, ModelListByType_t *pModelList, 
	int &nModelNodeCount, ModelListNode_t *pModelListNodes, int nInitialListIndex, ModelRenderSystemData_t &data, ModelRenderMode_t renderMode )
{
	// NOTE: we actually are bucketing both by model + also by lighting model
	// Bucketing by lighting model is not strictly necessary, but doing so
	// simplifies the code a lot in exchange for having two batches if the
	// same model is used but a different lighting model, something that could
	// theoretically happen if a static prop + physics prop use the same .mdl
	// My thought is that even if this split happens, it will be rare, and
	// we still will get a lot of sharing.

	// L4D2: We'll also bucket by whether the model renders stencil or not.
	// This allows us to keep stenciling models in the fastpath but group them together
	// to be rendered AFTER the 360 Z prepass ends. (360 doesn't allow stencil rendering
	// during the Z prepass).

	const model_t *pModel = data.m_pRenderable->GetModel();
	Assert( modelinfo->GetModelType( pModel ) == mod_studio );

	RenderableLightingModel_t nLightingModel = LIGHTING_MODEL_NONE;
	uint bWantsStencil = 0;
	ShaderStencilState_t tempStencil;
	if ( data.m_pModelRenderable )
	{
		data.m_pModelRenderable->GetRenderData( &nLightingModel, MODEL_DATA_LIGHTING_MODEL );
		if ( renderMode == MODEL_RENDER_MODE_NORMAL )
		{
			// I considered making a MODEL_DATA_STENCIL_ENABLE renderdata type that would only return a bool
			// if stencil was enabled, but it turns out most of the work for MODEL_DATA_STENCIL is computing
			// that bool, so pulling this out into a separate piece didn't turn out to be a perf win.
			bWantsStencil = data.m_pModelRenderable->GetRenderData( &tempStencil, MODEL_DATA_STENCIL ) ? 1 : 0;
		}
	}
	else
	{
		ExecuteOnce( DevWarning( "data.m_pModelRenderable is NULL for %s\n", modelinfo->GetModelName( pModel ) ) );
	}

	bool bRetVal = bWantsStencil ? true : false;

	int j;
	for ( j = 0; j < nModelTypeCount; ++j )
	{
		if ( pModelList[j].m_pModel == pModel &&
			 pModelList[j].m_nLightingModel == nLightingModel &&
			 pModelList[j].m_bWantsStencil == bWantsStencil )
			break;
	}

	if ( j == nModelTypeCount )
	{
		// Bail if we're rendering into shadow depth map and this model doesn't cast shadows
		// NOTE: if m_pModelRenderable is NULL, it's a dependent bone setup so we need to keep it
		studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pModel );
		if ( ( renderMode != MODEL_RENDER_MODE_NORMAL ) && data.m_pModelRenderable && ( ( pStudioHdr->flags & STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS ) != 0 ) )
		{
			return bRetVal;
		}

		MDLHandle_t hMDL = modelinfo->GetCacheHandle( pModel );
		studiohwdata_t *pHardwareData = g_pMDLCache->GetHardwareData( hMDL );

		// This can occur if there was an error loading the model; for instance
		// if the vtx and mdl are out of sync.
		if ( !pHardwareData || !pHardwareData->m_pLODs )
		{
			AssertMsg( 0, UTIL_VarArgs( "%s failed to load and is causing EVIL in the model render system!!", pStudioHdr->name ) );
			return bRetVal;
		}
				  
		ModelListByType_t &list = pModelList[ nModelTypeCount ]; 
		list.m_pModel = pModel;
		list.m_nLightingModel = nLightingModel;
		list.m_bWantsStencil = bWantsStencil;
		list.m_pStudioHdr = pStudioHdr;
		list.m_pHardwareData = pHardwareData;
		list.m_nFlashlightCount = 0;
		list.m_pFlashlights = NULL;
		list.m_nCount = 0;
		list.m_pFirstNode = 0;
		list.m_pRenderModels = 0;
		list.m_nParentDepth = 0;
		list.m_pNextLightingModel = NULL;
		j = nModelTypeCount++;
	}

	C_BaseEntity *pEntity = data.m_pRenderable->GetIClientUnknown()->GetBaseEntity();
	uint nParentDepth = ComputeParentDepth( pEntity );

	ModelListByType_t &list = pModelList[ j ]; 
	ModelListNode_t &node = pModelListNodes[ nModelNodeCount++ ];
	node.m_Entry = data;
	node.m_nInitialListIndex = nInitialListIndex;
	node.m_bBoneMerge = pEntity && pEntity->IsEffectActive( EF_BONEMERGE );

	if ( bWantsStencil && ( renderMode == MODEL_RENDER_MODE_NORMAL ) )
	{
		CMatRenderData< ShaderStencilState_t > rdStencil( m_pRenderContext, 1 );
		memcpy( &rdStencil[0], &tempStencil, sizeof( tempStencil ) );
		node.m_pStencilState = &rdStencil[0];
	}
	else
	{
		node.m_pStencilState = NULL;
	}

	node.m_pNext = list.m_pFirstNode;
	list.m_nParentDepth = MAX( list.m_nParentDepth, nParentDepth );
	list.m_pFirstNode = &node;
	++list.m_nCount;

	return bRetVal;
}


//-----------------------------------------------------------------------------
// bucket models by type, return # of unique types
//-----------------------------------------------------------------------------
int CModelRenderSystem::BucketModelsByMDL( ModelListByType_t *pModelList, ModelListNode_t *pModelListNodes, ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode,
										   int *pModelsRenderingStencilCountOut )
{
	int nModelTypeCount = 0;
	int nModelNodeCount = 0;
	*pModelsRenderingStencilCountOut = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		bool bModelWantsStencil = AddModelToLists( nModelTypeCount, pModelList, nModelNodeCount, pModelListNodes, i, pEntities[i], renderMode );
		*pModelsRenderingStencilCountOut += bModelWantsStencil ? 1 : 0;
	}
	return nModelTypeCount;
}


//-----------------------------------------------------------------------------
// Sort model types function
//-----------------------------------------------------------------------------
inline bool CModelRenderSystem::DependencySortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right )
{
	// Ensures bone setup occurs in the correct order
	if ( left.m_nParentDepth != right.m_nParentDepth )
		return left.m_nParentDepth < right.m_nParentDepth;

	// Ensure stenciling models are at the end of the list.
	// This doesn't guarantee that stencil stuff is at the end because parent depth trumps it,
	// so we'll have to sort again before rendering.
	if ( left.m_bWantsStencil != right.m_bWantsStencil )
	{
		return left.m_bWantsStencil < right.m_bWantsStencil;
	}

	// Keep same models with different lighting types together
	return left.m_pModel < right.m_pModel;
}


//-----------------------------------------------------------------------------
// Sorts so that bone setup occurs in the appropriate order (parents set up first)
//-----------------------------------------------------------------------------
void CModelRenderSystem::SortBucketsByDependency( int nModelTypeCount, ModelListByType_t *pModelList, LightingList_t *pLightingList )
{
	std::sort( pModelList, pModelList + nModelTypeCount, DependencySortLessFunc ); 

	// Assign models to the appropriate lighting list
	for ( int i = nModelTypeCount; --i >= 0; )
	{
		ModelListByType_t &list = pModelList[ i ]; 

		// Hook into lighting list
		if ( list.m_nLightingModel == LIGHTING_MODEL_NONE )
			continue;

		LightingList_t &lightList = pLightingList[ list.m_nLightingModel ];
		list.m_pNextLightingModel = lightList.m_pFirstModel;
		lightList.m_pFirstModel = &list;
		++lightList.m_nCount;
		lightList.m_nTotalModelCount += list.m_nCount;
	}

#ifdef _DEBUG
	// Don't want to allow some MDLs of type A to depend on MDLs of type B
	// and other MDLS of type B to depend on type A because that would 
	// dramatically increase complexity of the system here. With this assumption,
	// we can always have all models of the same type be set up at the same time,
	// also improving cache efficiency.
	for ( int i =0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ]; 
		if ( list.m_nParentDepth == 0 )
			continue;

		for( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			C_BaseEntity *pEnt = pNode->m_Entry.m_pRenderable->GetIClientUnknown()->GetBaseEntity();
			if ( !pEnt )
				continue;

			C_BaseEntity *pTest = pEnt;
			while ( pEnt->IsFollowingEntity() || ( pEnt->GetMoveParent() && pEnt->GetParentAttachment() > 0 ) )
			{
				pEnt = pEnt->GetMoveParent();
				const model_t *pModel = pEnt->GetModel();

				bool bFound = false;
				for ( int j = 0; j < nModelTypeCount; ++j )
				{
					if ( pModelList[j].m_pModel != pModel )
						continue;

					if ( pModelList[j].m_nParentDepth >= list.m_nParentDepth )
					{
						// NOTE: GetClassname() stores the name in a global, hence need to do the warning on 2 lines
						Warning( "Bone setup dependency ordering issue [ent %s ", pTest->GetClassname() );
						Warning( " depends on ent %s]!\n", pEnt->GetClassname() );
					}

					for( ModelListNode_t *pParentNode = pModelList[j].m_pFirstNode; pParentNode; pParentNode = pParentNode->m_pNext )
					{
						if ( pParentNode->m_Entry.m_pRenderable == pEnt->GetClientRenderable() )
						{
							bFound = true;
							break;
						}
					}
				}

				if ( !bFound )
				{
					// NOTE: GetClassname() stores the name in a global, hence need to do the warning on 2 lines
//					Warning( "Missing bone setup dependency [ent %s ", pTest->GetClassname() );
//					Warning( "depends on ent %s]!\n", pEnt->GetClassname() );
				}
			}
		}
	}
#endif
}



//-----------------------------------------------------------------------------
// Slam model LODs to the appropriate level
//-----------------------------------------------------------------------------
void CModelRenderSystem::SlamModelLODs( int nLOD, int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		int nLODCount = list.m_pHardwareData->m_NumLODs;
		int nRootLOD = list.m_pHardwareData->m_RootLOD;
		bool bHasShadowLOD = ( list.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) != 0;
		int nMaxLOD = bHasShadowLOD ? nLODCount - 2 : nLODCount - 1; 

		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
		}
	}
}


//-----------------------------------------------------------------------------
// Compute model LODs
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeModelLODs( int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode, ModelRenderMode_t renderMode )
{
	if ( renderMode == MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		// Slam to shadow lod
		//int nShadowLodConVar = r_shadowlod.GetInt();

		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			int nLODCount = list.m_pHardwareData->m_NumLODs;
			//int nRootLOD = list.m_pHardwareData->m_RootLOD;
			int nMaxLOD = nLODCount - 1; 

			for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
			{
				// Just always use the lowest LOD right now
				//int nLOD = nShadowLodConVar;
				//pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
				pNode->m_nLOD = nMaxLOD;
			}
		}
		return;
	}

	int nLOD = r_lod.GetInt();
	if ( nLOD >= 0 )
	{
		SlamModelLODs( nLOD, nModelTypeCount, pModelList, pModelListNode );
		return;
	}

	ScreenSizeComputeInfo_t info;
	ComputeScreenSizeInfo( &info );

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		int nLODCount = list.m_pHardwareData->m_NumLODs;
		int nRootLOD = list.m_pHardwareData->m_RootLOD;
		int nMaxLOD = nLODCount - 1; 

		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			// FIXME: SIMD-ize, eliminate all extraneous calls (get view render state outside of loop)
			const Vector &vecRenderOrigin = pNode->m_Entry.m_pRenderable->GetRenderOrigin();

			// NOTE: The 2.0 is for legacy reasons
			float flScreenSize = 2.0f * ComputeScreenSize( vecRenderOrigin, 0.5f, info );

			float flMetric = list.m_pHardwareData->LODMetric( flScreenSize );
			nLOD = list.m_pHardwareData->GetLODForMetric( flMetric );
			pNode->m_nLOD = clamp( nLOD, nRootLOD, nMaxLOD );
		}
	}
}


//-----------------------------------------------------------------------------
// Sort models function
//-----------------------------------------------------------------------------
inline bool CModelRenderSystem::SortLessFunc( const RenderModelInfo_t &left, const RenderModelInfo_t &right )
{
	// NOTE: Could do this, but it is not faster, because the cost of an integer multiply is about three
	// times that of a branch penalty:
	// int nLeft = left.m_nSkin * 1000000 + left.m_nLOD * 1000 + left.m_nBody;
	// int nRight = right.m_nSkin * 1000000 + right.m_nLOD * 1000 + right.m_nBody;
	// return nLeft > nRight;
	if ( left.m_bSetupBonesOnly != right.m_bSetupBonesOnly )
		return !left.m_bSetupBonesOnly;
	if ( left.m_nSkin != right.m_nSkin )
		return left.m_nSkin > right.m_nSkin;
	if ( left.m_nLOD != right.m_nLOD )
		return left.m_nLOD > right.m_nLOD;
	return left.m_nBody > right.m_nBody;
}


//-----------------------------------------------------------------------------
// Sort models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SortModels( RenderModelInfo_t *pRenderModelInfo, 
	int nModelTypeCount, ModelListByType_t *pModelList, ModelListNode_t *pModelListNode )
{
	// First place them in arrays
	RenderModelInfo_t *pCurrInfo = pRenderModelInfo;
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		list.m_pRenderModels = pCurrInfo;
		list.m_nSetupBoneCount = 0;
		for ( ModelListNode_t *pNode = list.m_pFirstNode; pNode; pNode = pNode->m_pNext )
		{
			memset( pCurrInfo, 0, sizeof( RenderModelInfo_t ) );
			pCurrInfo->m_Entry = pNode->m_Entry;
			pCurrInfo->m_nLOD = pNode->m_nLOD;
			pCurrInfo->m_nSkin = pNode->m_Entry.m_pRenderable->GetSkin();
			pCurrInfo->m_nBody = pNode->m_Entry.m_pRenderable->GetBody();
			pCurrInfo->m_hInstance = pNode->m_Entry.m_pRenderable->GetModelInstance();
			pCurrInfo->m_Decals = STUDIORENDER_DECAL_INVALID;
			pCurrInfo->m_nInitialListIndex = pNode->m_nInitialListIndex;
			pCurrInfo->m_bBoneMerge = pNode->m_bBoneMerge;
			pCurrInfo->m_bSetupBonesOnly = ( pNode->m_Entry.m_pModelRenderable == NULL );
			pCurrInfo->m_pStencilState = pNode->m_pStencilState;
			list.m_nSetupBoneCount += pCurrInfo->m_bSetupBonesOnly;
			++pCurrInfo;
		}

		// Sort within this model type. skin first, then LOD, then body.
		Assert( pCurrInfo - list.m_pRenderModels == list.m_nCount );
		std::sort( list.m_pRenderModels, list.m_pRenderModels + list.m_nCount, SortLessFunc ); 

		list.m_nCount -= list.m_nSetupBoneCount;
		list.m_nSetupBoneCount += list.m_nCount;
	}
}


//-----------------------------------------------------------------------------
// Sets up bones on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupBones( int nModelTypeCount, ModelListByType_t *pModelList )
{
	// FIXME: Can we make parallel bone setup faster? Yes, we can!
	const float flCurTime = gpGlobals->curtime;
	matrix3x4a_t pPoseToBone[MAXSTUDIOBONES];

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		const int nBoneCount = list.m_pStudioHdr->numbones;

		// Force setup of attachments if we're going to use an illumposition
		const int nAttachmentMask = ( list.m_pStudioHdr->IllumPositionAttachmentIndex() > 0 ) ? BONE_USED_BY_ATTACHMENT : 0;
		for ( int j = 0; j < list.m_nSetupBoneCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			const int nBoneMask = BONE_USED_BY_VERTEX_AT_LOD( pModel->m_nLOD ) | nAttachmentMask;
			pModel->m_pBoneToWorld = (matrix3x4a_t*)m_BoneToWorld.Alloc( nBoneCount * sizeof(matrix3x4a_t) );
			const bool bOk = pModel->m_Entry.m_pRenderable->SetupBones( pModel->m_pBoneToWorld, nBoneCount, nBoneMask, flCurTime );
			if ( !bOk )
			{
				for ( int k = 0; k < nBoneCount; ++k)
				{
					SetIdentityMatrix( pModel->m_pBoneToWorld[k] );
				}
			}
		}

		if ( list.m_nCount == 0 )
			continue;

		// Get the pose to bone for the model
		if ( !list.m_pStudioHdr->pLinearBones() )
		{
			// convert bone to world transformations into pose to world transformations
			for (int k = 0; k < nBoneCount; k++)
			{
				mstudiobone_t *pCurBone = list.m_pStudioHdr->pBone( k );
				MatrixCopy( pCurBone->poseToBone, pPoseToBone[k] );
			}
		}
		else
		{
			mstudiolinearbone_t *pLinearBones = list.m_pStudioHdr->pLinearBones();

			// convert bone to world transformations into pose to world transformations
			for ( int k = 0; k < nBoneCount; k++)
			{
				MatrixCopy( pLinearBones->poseToBone(k), pPoseToBone[k] );
			}
		}

		// Apply the pose-to-bone matrix to all instances
		// NOTE: We should be able to optimize this a ton since it's very parallelizable
		// NOTE: We may well want to compute the aggregate bone to world here also.
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			CMatRenderData< matrix3x4a_t > rdPoseToWorld( m_pRenderContext, nBoneCount );
			pModel->m_pPoseToWorld = rdPoseToWorld.Base();
			for ( int b = 0; b < nBoneCount; b++ )
			{
				ConcatTransforms_Aligned( pModel->m_pBoneToWorld[b], pPoseToBone[b], pModel->m_pPoseToWorld[b] );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sets up flexes on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupFlexes( int nModelTypeCount, ModelListByType_t *pModelList )
{
	bool bUsesDelayedWeights = g_CV_FlexSmooth.GetBool();

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		const int nFlexCount = list.m_pStudioHdr->numflexdesc;
		if ( !nFlexCount )
			continue;

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			CMatRenderData< float > rdFlexWeights( m_pRenderContext );
			CMatRenderData< float > rdDelayedFlexWeights( m_pRenderContext );
			pModel->m_pFlexWeights = rdFlexWeights.Lock( nFlexCount );
			if ( bUsesDelayedWeights )
			{
				pModel->m_pDelayedFlexWeights = rdDelayedFlexWeights.Lock( nFlexCount );
			}
			pModel->m_Entry.m_pRenderable->SetupWeights( pModel->m_pBoneToWorld, nFlexCount, pModel->m_pFlexWeights, pModel->m_pDelayedFlexWeights );
		}
	}
}


//-----------------------------------------------------------------------------
// Draws debugging information for lighting
//-----------------------------------------------------------------------------
void CModelRenderSystem::DebugDrawLightingOrigin( const ModelListByType_t &list, const RenderModelInfo_t &model )
{
	if ( !model.m_pLightingState )
		return;

	const Vector& lightOrigin = model.m_pLightingState->m_vecLightingOrigin;
	const matrix3x4_t &modelToWorld = model.m_Entry.m_pRenderable->RenderableToWorldTransform();

	// draw z planar cross at lighting origin
	Vector pt0;
	Vector pt1;
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.x -= 4;
	pt1.x += 4;
	debugoverlay->AddLineOverlay( pt0, pt1, 0, 255, 0, true, 0.0f );
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.y -= 4;
	pt1.y += 4;
	debugoverlay->AddLineOverlay( pt0, pt1, 0, 255, 0, true, 0.0f );

	// draw lines from the light origin to the hull boundaries to identify model
	Vector pt;
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );

	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_min.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_max.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );
	pt0.x = list.m_pStudioHdr->hull_max.x;
	pt0.y = list.m_pStudioHdr->hull_min.y;
	pt0.z = list.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, modelToWorld, pt1 );
	debugoverlay->AddLineOverlay( lightOrigin, pt1, 100, 100, 150, true, 0.0f );	
}


//-----------------------------------------------------------------------------
// Compute lighting origin on all models
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeLightingOrigin( ModelListByType_t &list, LightingQuery_t *pLightingQueryBase, int nQueryStride )
{
	LightingQuery_t *pLightingQuery = pLightingQueryBase;

	//int nBoneMergeCount = 0;
	int nAttachmentIndex = list.m_pStudioHdr->IllumPositionAttachmentIndex();
	bool bAmbientBoost = ( list.m_pStudioHdr->flags & STUDIOHDR_FLAGS_AMBIENT_BOOST ) != 0;
	if ( nAttachmentIndex <= 0 || nAttachmentIndex > list.m_pStudioHdr->GetNumAttachments() )
	{
		const Vector &vecIllumPosition = list.m_pStudioHdr->illumposition;
		for ( int j = 0; j < list.m_nCount; ++j, pLightingQuery = (LightingQuery_t*)( (unsigned char*)pLightingQuery + nQueryStride ) )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			//nBoneMergeCount += pModel->m_bBoneMerge;
			const matrix3x4_t &renderToWorld = pModel->m_Entry.m_pRenderable->RenderableToWorldTransform();
			pLightingQuery->m_InstanceHandle = pModel->m_hInstance;
			VectorTransform( vecIllumPosition, renderToWorld, pLightingQuery->m_LightingOrigin );
			//pLightingQuery->m_ParentInstanceHandle = MODEL_INSTANCE_INVALID;
			pLightingQuery->m_bAmbientBoost = bAmbientBoost;
		}
	}
	else
	{
		// NOTE: We don't care about orientation here. 
		// Assume the attachment->bone matrix has identity rotation. Given that, 
		// compute attachment->world = bone->world * attachment->bone.
		// We only care about the translation component of attachment->world
		// which can be obtained by transforming the attachment offset by bone->world

		// Oh, and tough noogies if you want an illumposition offset also.
		// Just make an attachment exactly where you want it. Otherwise this is slower.
#ifdef _DEBUG
		if ( list.m_pStudioHdr->illumposition != vec3_origin )
		{
			static int nWarnCount = 0;
			if ( nWarnCount++ < 10 )
			{
				AssertMsg( false, "Model fast path: illumposition must be (0,0,0) if an attachment is being used!\n" );
			}
		}
#endif

		// Attachment index is 1 too large, 0 means no attachment
		--nAttachmentIndex;

		Vector attachmentOffset;
		const mstudioattachment_t &attachment = list.m_pStudioHdr->pAttachment( nAttachmentIndex );
		MatrixGetColumn( attachment.local, 3, attachmentOffset );
		int iBone = list.m_pStudioHdr->GetAttachmentBone( nAttachmentIndex );

		for ( int j = 0; j < list.m_nCount; ++j, pLightingQuery = (LightingQuery_t*)( (unsigned char*)pLightingQuery + nQueryStride ) )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			//nBoneMergeCount += pModel->m_bBoneMerge;
			pLightingQuery->m_InstanceHandle = pModel->m_hInstance;
			VectorTransform( attachmentOffset, pModel->m_pBoneToWorld[iBone], pLightingQuery->m_LightingOrigin );
			//pLightingQuery->m_ParentInstanceHandle = MODEL_INSTANCE_INVALID;
			pLightingQuery->m_bAmbientBoost = bAmbientBoost;
		}
	}

#if 0
	// NOTE: This is more expensive, but hopefully is uncommon
	// Bonemerged models will copy the lighting environment from their parent entity.
	// This fixes issues with L4D2 infected wounds where the wounds would sometimes receive different lighting
	// than the body they're embedded in.
	if ( nBoneMergeCount > 0 )
	{
		pLightingQuery = pLightingQueryBase;
		for ( int j = 0; j < list.m_nCount; ++j, pLightingQuery = (LightingQuery_t*)( (unsigned char*)pLightingQuery + nQueryStride ) )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			if ( !pModel->m_bBoneMerge )
				continue;

			C_BaseEntity *pEnt = pModel->m_Entry.m_pRenderable->GetIClientUnknown()->GetBaseEntity();
			C_BaseEntity *pParent = pEnt->GetMoveParent();
			if ( !pParent )
				continue;

			pLightingQuery->m_ParentInstanceHandle = pParent->GetModelInstance();
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Builds the model lighting list
//-----------------------------------------------------------------------------
enum
{
	LIGHTING_USES_ENV_CUBEMAP = 0x1,
	LIGHTING_IS_VERTEX_LIT = 0x2,
	LIGHTING_IS_STATIC_LIT = 0x4,
};

int CModelRenderSystem::BuildLightingList( ModelListByType_t **ppLists, unsigned char *pFlags, int *pTotalModels, const LightingList_t &lightingList )
{
	// FIXME: This may be better placed in the engine to avoid all the virtual calls?
	int nSetupCount = 0;
	*pTotalModels = 0;
	for ( ModelListByType_t* pList = lightingList.m_pFirstModel; pList; pList = pList->m_pNextLightingModel )
	{
		// FIXME: Under what conditions can the static prop skip lighting? [unlit materials]
		bool bIsLit = modelinfo->IsModelVertexLit( pList->m_pModel );
		bool bUsesEnvCubemap = modelinfo->UsesEnvCubemap( pList->m_pModel );
		bool bIsStaticLit = modelinfo->UsesStaticLighting( pList->m_pModel );
		if ( !bIsLit && !bUsesEnvCubemap && !bIsStaticLit )
			continue;
		ppLists[ nSetupCount ] = pList;
		pFlags[ nSetupCount ] = ( bIsStaticLit << 2 ) | ( bIsLit << 1 ) | ( bUsesEnvCubemap << 0 );
		*pTotalModels += pList->m_nCount;
		++nSetupCount;
	}

	return nSetupCount;
}


//-----------------------------------------------------------------------------
// Hook up computed lighting state
//-----------------------------------------------------------------------------
void CModelRenderSystem::HookUpStaticLightingState( int nCount, ModelListByType_t **ppLists, 
	unsigned char *pFlags, ITexture **ppEnvCubemap, MaterialLightingState_t *pLightingState,
	MaterialLightingState_t *pDecalLightingState, ColorMeshInfo_t **ppColorMeshInfo )
{
	// FIXME: This has got to be more efficient that this
	for ( int i = 0; i < nCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		if ( pFlags[i] & LIGHTING_USES_ENV_CUBEMAP )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pEnvCubemapTexture = ppEnvCubemap[j] ? ppEnvCubemap[j] : m_DefaultCubemap;
			}
		}

		if ( pFlags[i] & LIGHTING_IS_VERTEX_LIT )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pLightingState = &pLightingState[j];
				pModel->m_pDecalLightingState = &pDecalLightingState[j];
			}
		}

		if ( pFlags[i] & LIGHTING_IS_STATIC_LIT )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pColorMeshInfo = ppColorMeshInfo[j];
			}
		}

		ppEnvCubemap += list.m_nCount;
		pLightingState += list.m_nCount;
		pDecalLightingState += list.m_nCount;
		ppColorMeshInfo += list.m_nCount;
	}
}


//-----------------------------------------------------------------------------
// Sets up lighting on all models
//-----------------------------------------------------------------------------
int CModelRenderSystem::SetupStaticPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandle )
{
	if ( lightingList.m_nCount == 0 )
		return 0;

	// Build list of everything that needs lighting
	int nTotalModels;
	ModelListByType_t **ppLists = (ModelListByType_t**)stackalloc( lightingList.m_nCount * sizeof(ModelListByType_t*) );
	unsigned char *pFlags = (unsigned char*)stackalloc( lightingList.m_nCount * sizeof(unsigned char) );
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return 0;

	// Build queries used to compute lighting
	StaticLightingQuery_t *pLightingQuery = (StaticLightingQuery_t*)stackalloc( nTotalModels * sizeof(StaticLightingQuery_t) );
	int nOffset = 0;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );

		for ( int j = 0; j < list.m_nCount; ++j, ++nOffset )
		{
			pLightingQuery[ nOffset ].m_pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			pLightingQuery[ nOffset ].m_InstanceHandle = list.m_pRenderModels[j].m_hInstance;
			pLightingQuery[ nOffset ].m_bAmbientBoost = false;
		}
	}

	// Compute lighting origins
	staticpropmgr->GetLightingOrigins( &pLightingQuery[0].m_LightingOrigin, 
		sizeof(StaticLightingQuery_t), nTotalModels, &pLightingQuery[0].m_pRenderable, sizeof(StaticLightingQuery_t) );
			   
	// Does all lighting computations for all models
	ColorMeshInfo_t **ppColorMeshInfo = (ColorMeshInfo_t**)stackalloc( nTotalModels * sizeof(ColorMeshInfo_t*) );
	ITexture **ppEnvCubemap = (ITexture**)stackalloc( nTotalModels * sizeof(ITexture*) );

	CMatRenderData< MaterialLightingState_t > rdLightingState( m_pRenderContext, 2 * nTotalModels );
	MaterialLightingState_t *pLightingState = rdLightingState.Base();
	MaterialLightingState_t *pDecalLightingState = &rdLightingState[ nTotalModels ];
	modelrender->ComputeStaticLightingState( nTotalModels, pLightingQuery, pLightingState, pDecalLightingState, ppColorMeshInfo, ppEnvCubemap, pColorMeshHandle );

	// Hook up pointers
	HookUpStaticLightingState( nSetupCount, ppLists, pFlags, ppEnvCubemap, pLightingState, pDecalLightingState, ppColorMeshInfo );

	return nTotalModels;
}


void CModelRenderSystem::SetupStandardLighting( LightingList_t &lightingList )
{
	if ( lightingList.m_nCount == 0 )
		return;

	// Determine which groups need lighting
	ModelListByType_t **ppLists = (ModelListByType_t**)stackalloc( lightingList.m_nCount * sizeof(ModelListByType_t*) );
	unsigned char *pFlags = (unsigned char*)stackalloc( lightingList.m_nCount * sizeof(unsigned char) );
	int nTotalModels = 0;
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return;

	// Compute data necessary for lighting computations
	int nOffset = 0;
	LightingQuery_t *pLightingQuery = (LightingQuery_t*)stackalloc( nTotalModels * sizeof(LightingQuery_t) );
	CMatRenderData<MaterialLightingState_t> rdLightingState( m_pRenderContext, nTotalModels );
	MaterialLightingState_t *pLightingState = rdLightingState.Base();
	memset( pLightingState, 0, nTotalModels * sizeof(MaterialLightingState_t) );
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		ComputeLightingOrigin( list, &pLightingQuery[nOffset], sizeof(LightingQuery_t) );
		nOffset += list.m_nCount;
	}

	// Does all lighting computations for all models
	ITexture **ppEnvCubemap = (ITexture**)stackalloc( nTotalModels * sizeof(ITexture*) );
	modelrender->ComputeLightingState( nTotalModels, pLightingQuery, pLightingState, ppEnvCubemap );

	// Hook up pointers
	MaterialLightingState_t *pCurrState = pLightingState;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );
		if ( pFlags[i] & 0x1 )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pEnvCubemapTexture = ppEnvCubemap[j] ? ppEnvCubemap[j] : m_DefaultCubemap;
			}
		}

		if ( pFlags[i] & 0x2 )
		{
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
				pModel->m_pLightingState = &pCurrState[j];
			}
		}

		ppEnvCubemap += list.m_nCount;
		pCurrState += list.m_nCount;
	}
}

int CModelRenderSystem::SetupPhysicsPropLighting( LightingList_t &lightingList, DataCacheHandle_t *pColorMeshHandle )
{
	if ( lightingList.m_nCount == 0 )
		return 0;

	// NOTE: Physics prop lighting is the same as static prop lighting, only
	// the static lighting is *always* used (the system goes to the standard path
	// for physics props which are moving or which use bumpmapping).
	ModelListByType_t **ppLists = (ModelListByType_t**)stackalloc( lightingList.m_nCount * sizeof(ModelListByType_t*) );
	unsigned char *pFlags = (unsigned char*)stackalloc( lightingList.m_nCount * sizeof(unsigned char) );
	int nTotalModels = 0;
	int nSetupCount = BuildLightingList( ppLists, pFlags, &nTotalModels, lightingList );
	if ( nSetupCount == 0 )
		return 0;

	StaticLightingQuery_t *pLightingQuery = (StaticLightingQuery_t*)stackalloc( nTotalModels * sizeof(StaticLightingQuery_t) );
	int nOffset = 0;
	for ( int i = 0; i < nSetupCount; ++i )
	{
		ModelListByType_t &list = *( ppLists[i] );

		ComputeLightingOrigin( list, &pLightingQuery[nOffset], sizeof(StaticLightingQuery_t) );
		for ( int j = 0; j < list.m_nCount; ++j, ++nOffset )
		{
			pLightingQuery[ nOffset ].m_pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
		}
	}

	// Does all lighting computations for all models
	ColorMeshInfo_t **ppColorMeshInfo = (ColorMeshInfo_t**)stackalloc( nTotalModels * sizeof(ColorMeshInfo_t*) );
	ITexture **ppEnvCubemap = (ITexture**)stackalloc( nTotalModels * sizeof(ITexture*) );
	CMatRenderData< MaterialLightingState_t > rdLightingState( m_pRenderContext, 2 * nTotalModels );
	MaterialLightingState_t *pLightingState = rdLightingState.Base();
	MaterialLightingState_t *pDecalLightingState = &pLightingState[ nTotalModels ];
	modelrender->ComputeStaticLightingState( nTotalModels, pLightingQuery, pLightingState, pDecalLightingState, ppColorMeshInfo, ppEnvCubemap, pColorMeshHandle );

	// Hook up pointers
	HookUpStaticLightingState( nSetupCount, ppLists, pFlags, ppEnvCubemap, pLightingState, pDecalLightingState, ppColorMeshInfo );
	return nTotalModels;
}

int CModelRenderSystem::SetupLighting( LightingList_t *pLightingList, int nModelTypeCount, ModelListByType_t *pModelList, DataCacheHandle_t *pColorMeshHandles, ModelRenderMode_t renderMode )
{
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
	{
		return 0;
	}

	int nCount = SetupStaticPropLighting( pLightingList[ LIGHTING_MODEL_STATIC_PROP ], pColorMeshHandles );
	pColorMeshHandles += nCount;
	SetupStandardLighting( pLightingList[ LIGHTING_MODEL_STANDARD ] );
	nCount += SetupPhysicsPropLighting( pLightingList[ LIGHTING_MODEL_PHYSICS_PROP ], pColorMeshHandles );

	// Debugging info
	if ( r_drawmodellightorigin.GetBool() )
	{
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			const ModelListByType_t &list = pModelList[ i ];
			if ( list.m_nLightingModel == LIGHTING_MODEL_NONE )
				continue;
			for ( int j = 0; j < list.m_nCount; ++j )
			{
				const RenderModelInfo_t &info = list.m_pRenderModels[j];
				DebugDrawLightingOrigin( list, info );
			}
		}
	}

	return nCount;
}


//-----------------------------------------------------------------------------
// Setup render state related to flashlights and decals
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupFlashlightsAndDecals( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, RenderModelInfo_t *pRenderModels, ModelRenderMode_t renderMode )
{
	// Skip lighting + decals if we don't need it
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
		return;

	ShadowHandle_t pFlashlights[MAX_FLASHLIGHTS_PER_INSTANCE_DRAW_CALL];
	int nInstCount = 0;
	ModelInstanceHandle_t *pModelInstanceHandle = (ModelInstanceHandle_t*)stackalloc( nTotalModelCount * sizeof(ModelInstanceHandle_t) );
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			pModelInstanceHandle[nInstCount++] = pModel->m_hInstance;
		}		
	}

	Assert( nTotalModelCount == nInstCount );

	if ( nTotalModelCount != nInstCount )
	{
		DevWarning( "CModelRenderSystem::SetupFlashlightsAndDecals sorted model list count incorrect! A model was probably unable to load!" );
		nTotalModelCount = nInstCount;
	}

	// Gets all decals
	StudioDecalHandle_t *pDecals = &pRenderModels->m_Decals;
	modelrender->GetModelDecalHandles( pDecals, sizeof(RenderModelInfo_t), nTotalModelCount, pModelInstanceHandle );

	// Builds a list of all flashlights affecting this model
	uint32 *pFlashlightUsage = &pRenderModels->m_nFlashlightUsage;
	pInfo->m_nFlashlightCount = shadowmgr->SetupFlashlightRenderInstanceInfo( pFlashlights, pFlashlightUsage, sizeof(RenderModelInfo_t), nTotalModelCount, pModelInstanceHandle );
	if ( pInfo->m_nFlashlightCount )
	{
		// Copy over the flashlight state
		// FIXME: Should we do this over the entire list of all instances?
		// There's going to be a fair amount of copying of flashlight_ts
		CMatRenderData< FlashlightInstance_t > rdFlashlights( m_pRenderContext, pInfo->m_nFlashlightCount );
		pInfo->m_pFlashlights = rdFlashlights.Base();
		shadowmgr->GetFlashlightRenderInfo( pInfo->m_pFlashlights, pInfo->m_nFlashlightCount, pFlashlights );
	}
	else
	{
		pInfo->m_pFlashlights = NULL;
	}

	// FIXME: Hack!
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		list.m_nFlashlightCount = pInfo->m_nFlashlightCount;
		list.m_pFlashlights = pInfo->m_pFlashlights;
	}
}

void CModelRenderSystem::SetupPerInstanceColorModulation( int nModelTypeCount, ModelListByType_t *pModelList )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[ i ];
		if ( !list.m_nCount )
			continue;

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t *pModel = &list.m_pRenderModels[j];
			IClientRenderable *pRenderable = pModel->m_Entry.m_pRenderable;
#if 0 
			Vector diffuseModulation;
			pRenderable->GetColorModulation( diffuseModulation.Base() );
			pModel->m_DiffuseModulation.x = diffuseModulation.x;
			pModel->m_DiffuseModulation.y = diffuseModulation.y;
			pModel->m_DiffuseModulation.z = diffuseModulation.z;
#else		// preferred to do it this way, because it avoids a load-hit-store on 360
			pRenderable->GetColorModulation( pModel->m_DiffuseModulation.AsVector3D().Base() );
#endif
			pModel->m_DiffuseModulation.w = pModel->m_Entry.m_InstanceData.m_nAlpha * ( 1.0f / 255.0f );
		}
	}
}


//-----------------------------------------------------------------------------
// Call into studiorender
//-----------------------------------------------------------------------------
ConVar cl_colorfastpath( "cl_colorfastpath", "0" );
void CModelRenderSystem::RenderModels( StudioModelArrayInfo2_t *pInfo, int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, ModelRenderMode_t renderMode )
{
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		bool bColorize = cl_colorfastpath.GetBool();
		if ( bColorize )
		{
			g_pStudioRender->ForcedMaterialOverride( m_DebugMaterial );
		}

		const int nFlags = STUDIORENDER_DRAW_OPAQUE_ONLY;
		CMatRenderData< StudioArrayData_t > rdArray( m_pRenderContext, nModelTypeCount );

#ifdef _DEBUG
		bool bFoundStencil = false;
#endif

		int nNonStencilModelTypeCount = 0;
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			rdArray[ i ].m_pStudioHdr = list.m_pStudioHdr;
			rdArray[ i ].m_pHardwareData = list.m_pHardwareData;
			rdArray[ i ].m_pInstanceData = list.m_pRenderModels;
			rdArray[ i ].m_nCount = list.m_nCount;
			nNonStencilModelTypeCount += list.m_bWantsStencil ? 0 : 1;

#ifdef _DEBUG
			if ( list.m_bWantsStencil )
			{
				bFoundStencil = true;
			}
			else
			{
				Assert( !bFoundStencil );
			}
#endif
		}
		if ( IsX360() && r_fastzreject.GetBool() && ( nNonStencilModelTypeCount != nModelTypeCount ) )
		{
			// Render all models without stencil
			g_pStudioRender->DrawModelArray( *pInfo, nNonStencilModelTypeCount, rdArray.Base(), sizeof(RenderModelInfo_t), nFlags );

			#ifdef _X360
				// end z prepass here
				CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
				pRenderContext->End360ZPass();
			#endif

			// Render all models with stencil
			g_pStudioRender->DrawModelArray( *pInfo, nModelTypeCount - nNonStencilModelTypeCount, rdArray.Base() + nNonStencilModelTypeCount,
				sizeof(RenderModelInfo_t), nFlags );
		}
		else
		{
			// PC renders all models in one go regardless of stencil state
			g_pStudioRender->DrawModelArray( *pInfo, nModelTypeCount, rdArray.Base(), sizeof(RenderModelInfo_t), nFlags );
		}
		g_pStudioRender->ForcedMaterialOverride( NULL );
	}
	else if ( renderMode == MODEL_RENDER_MODE_SHADOW_DEPTH )
	{
		// NOTE: Use this path because we can aggregate draw calls across mdls

		const int nFlags = STUDIORENDER_SHADOWDEPTHTEXTURE | STUDIORENDER_DRAW_OPAQUE_ONLY;
		CMatRenderData< StudioArrayData_t > rdShadow( m_pRenderContext, nModelTypeCount );
		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			rdShadow[ i ].m_pStudioHdr = list.m_pStudioHdr;
			rdShadow[ i ].m_pHardwareData = list.m_pHardwareData;
			rdShadow[ i ].m_pInstanceData = list.m_pRenderModels;
			rdShadow[ i ].m_nCount = list.m_nCount;
		}
		g_pStudioRender->DrawModelShadowArray( nModelTypeCount, rdShadow.Base(), sizeof(RenderModelInfo_t), nFlags );
	}
	else if ( renderMode == MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		// shouldn't get here unless the code is ported from l4d2 to drive this properly.
		Assert(0);
#if 0
		// HACK: Assume all models in this batch use the same material. This only works because we submit batches of 1 model from the client shadow manager at the moment
		IMaterial* pShadowDrawMaterial = pModelList[0].m_pFirstNode->m_Entry.m_pRenderable->GetShadowDrawMaterial();
		g_pStudioRender->ForcedMaterialOverride( pShadowDrawMaterial ? pShadowDrawMaterial : m_ShadowBuild, OVERRIDE_BUILD_SHADOWS );

		for ( int i = 0; i < nModelTypeCount; ++i )
		{
			ModelListByType_t &list = pModelList[i];
			g_pStudioRender->DrawModelArray( list, list.m_nCount, list.m_pRenderModels, sizeof(RenderModelInfo_t), STUDIORENDER_DRAW_OPAQUE_ONLY );
		}

		g_pStudioRender->ForcedMaterialOverride( NULL );
#endif
	}
}


//-----------------------------------------------------------------------------
// Call into studiorender
//-----------------------------------------------------------------------------
void CModelRenderSystem::SetupTranslucentData( int nModelTypeCount, ModelListByType_t *pModelList, int nTotalModelCount, TranslucentInstanceRenderData_t *pRenderData )
{
	memset( pRenderData, 0, nTotalModelCount * sizeof( TranslucentInstanceRenderData_t ) );
	CMatRenderData< StudioModelArrayInfo_t > arrayInfo( m_pRenderContext, nModelTypeCount );
	CMatRenderData< StudioArrayInstanceData_t > instanceData( m_pRenderContext, nTotalModelCount );

	int nCurInstance = 0;
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		StudioModelArrayInfo_t *pModelInfo = &arrayInfo[i];
		memcpy( pModelInfo, &list, sizeof( StudioModelArrayInfo_t ) );

		for ( int j = 0; j < list.m_nCount; ++j )
		{
			RenderModelInfo_t &info = list.m_pRenderModels[j];

			StudioArrayInstanceData_t *pInstanceData = &instanceData[nCurInstance++];
			memcpy( pInstanceData, &info, sizeof( StudioArrayInstanceData_t ) );

			TranslucentInstanceRenderData_t &data = pRenderData[ info.m_nInitialListIndex ];
			data.m_pModelInfo = pModelInfo;
			data.m_pInstanceData = pInstanceData;
		}
	}
}


//-----------------------------------------------------------------------------
// Renders debug overlays
//-----------------------------------------------------------------------------
void CModelRenderSystem::RenderVCollideDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList )
{
	if ( !vcollide_wireframe.GetBool() )
		return;

	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			IClientRenderable *pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			C_BaseAnimating *pAnim = dynamic_cast< C_BaseAnimating * >( pRenderable );
			if ( pAnim && pAnim->IsRagdoll() )
			{
				pAnim->m_pRagdoll->DrawWireframe();
				continue;
			}
			
			ICollideable *pCollideable = pRenderable->GetIClientUnknown()->GetCollideable();
			if ( pCollideable && ( pCollideable->GetSolid() == SOLID_VPHYSICS ) &&
				IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
			{
				vcollide_t *pCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
				if ( pCollide && pCollide->solidCount == 1 )
				{
					static color32 debugColor = {0,255,255,0};
					engine->DebugDrawPhysCollide( pCollide->solids[0], NULL, pCollideable->CollisionToWorldTransform(), debugColor );

					C_BaseEntity *pEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
					if ( pEntity && pEntity->VPhysicsGetObject() )
					{
						static color32 debugColorPhys = {255,0,0,0};
						matrix3x4_t matrix;
						pEntity->VPhysicsGetObject()->GetPositionMatrix( &matrix );
						engine->DebugDrawPhysCollide( pCollide->solids[0], NULL, matrix, debugColorPhys );
					}
				}
				continue;
			}
		}
	}
}


void CModelRenderSystem::RenderBBoxDebugOverlay( int nModelTypeCount, ModelListByType_t *pModelList )
{
	for ( int i = 0; i < nModelTypeCount; ++i )
	{
		ModelListByType_t &list = pModelList[i];
		for ( int j = 0; j < list.m_nCount; ++j )
		{
			IClientRenderable *pRenderable = list.m_pRenderModels[j].m_Entry.m_pRenderable;
			if ( !pRenderable->GetIClientUnknown() )
				continue;
			C_BaseEntity *pEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
			if ( !pEntity )
				continue;

			pEntity->DrawBBoxVisualizations();
		}
	}
}


//-----------------------------------------------------------------------------
// Renders debug overlays
//-----------------------------------------------------------------------------
void CModelRenderSystem::RenderDebugOverlays( int nModelTypeCount, ModelListByType_t *pModelList, ModelRenderMode_t renderMode )
{
	if ( renderMode != MODEL_RENDER_MODE_NORMAL )
	{
		return;
	}

	RenderVCollideDebugOverlay( nModelTypeCount, pModelList );
	RenderBBoxDebugOverlay( nModelTypeCount, pModelList );
}


//-----------------------------------------------------------------------------
// Sort model types function
//-----------------------------------------------------------------------------
inline bool CModelRenderSystem::StencilSortLessFunc( const ModelListByType_t &left, const ModelListByType_t &right )
{
	// Ensure stenciling models are at the end of the list
	if ( left.m_bWantsStencil != right.m_bWantsStencil )
	{
		return left.m_bWantsStencil < right.m_bWantsStencil;
	}

	// Keep same models with different lighting types together
	return left.m_pModel < right.m_pModel;
}

//-----------------------------------------------------------------------------
// Draw models
//-----------------------------------------------------------------------------
static ConVar cl_skipfastpath( "cl_skipfastpath", "0", FCVAR_CHEAT, "Set to 1 to stop all models that go through the model fast path from rendering" );
void CModelRenderSystem::DrawModels( ModelRenderSystemData_t *pEntities, int nCount, ModelRenderMode_t renderMode )
{
	if ( nCount == 0 || cl_skipfastpath.GetInt() )
		return;

	VPROF_BUDGET( "CModelRenderSystem::DrawModels", VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING );
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	// While doing this, we need materialsystem to keep around its temp allocations
	// which we use for bone matrices + flexes
	CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
	m_pRenderContext = matRenderContext;

	PIXEVENT( m_pRenderContext, "CModelRenderSystem::DrawModels (FASTPATH)" );

	CMatRenderDataReference rdLock( m_pRenderContext );

	// FIXME: This is infected-specific for perf test reasons.
	// Will break into a more fixed pipeline at a later date
	DataCacheHandle_t *pColorMeshHandles = NULL;
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		pColorMeshHandles = (DataCacheHandle_t*)stackalloc( nCount * sizeof(DataCacheHandle_t) );
	}
	ModelListByType_t *pModelList = (ModelListByType_t*)stackalloc( nCount * sizeof(ModelListByType_t) );
	ModelListNode_t *pModelListNode = (ModelListNode_t*)stackalloc( nCount * sizeof(ModelListNode_t) );
	int nModelsRenderingStencilCount = 0;
	int nModelTypeCount = BucketModelsByMDL( pModelList, pModelListNode, pEntities, nCount, renderMode, &nModelsRenderingStencilCount );

	LightingList_t pLightingList[ LIGHTING_MODEL_COUNT ];
	memset( pLightingList, 0, LIGHTING_MODEL_COUNT * sizeof(LightingList_t) );
	SortBucketsByDependency( nModelTypeCount, pModelList, pLightingList );

	// Compute LODs for each model
	ComputeModelLODs( nModelTypeCount, pModelList, pModelListNode, renderMode );

	// Sort processing list by body, lod, skin, etc.
	CMatRenderData< RenderModelInfo_t > rdRenderModelInfo( m_pRenderContext, nCount );
	RenderModelInfo_t *pSortedModelListNode = rdRenderModelInfo.Base();	
	SortModels( pSortedModelListNode, nModelTypeCount, pModelList, pModelListNode );

	// Setup bones
	SetupBones( nModelTypeCount, pModelList );

	// Setup flexes
	if ( renderMode != MODEL_RENDER_MODE_RTT_SHADOWS )
	{
		SetupFlexes( nModelTypeCount, pModelList );
	}

	// Setup lighting
	int nColorMeshHandles = SetupLighting( pLightingList, nModelTypeCount, pModelList, pColorMeshHandles, renderMode );

	// Setup flashlights + decals
	StudioModelArrayInfo2_t info;
	SetupFlashlightsAndDecals( &info, nModelTypeCount, pModelList, nCount, pSortedModelListNode, renderMode );

	// Setup per-instance color modulation
	SetupPerInstanceColorModulation( nModelTypeCount, pModelList );

	// Setup per-instance wound data
	//SetupInfectedWoundRenderData( nModelTypeCount, pModelList, nCount, renderMode );

	if ( IsX360() && ( renderMode == MODEL_RENDER_MODE_NORMAL ) && ( nModelsRenderingStencilCount > 0) )
	{
		// resort here to make sure all models rendering stencil come last
		std::sort( pModelList, pModelList + nModelTypeCount, StencilSortLessFunc );
	}

	// Draw models
	RenderModels( &info, nModelTypeCount, pModelList, nCount, renderMode );

	rdLock.Release();
	
	if ( renderMode == MODEL_RENDER_MODE_NORMAL )
	{
		modelrender->CleanupStaticLightingState( nColorMeshHandles, pColorMeshHandles );
		stackfree( pColorMeshHandles );
	}

	// Blat out temporary memory for bone-to-world transforms
	m_BoneToWorld.FreeAll( false );

	RenderDebugOverlays( nModelTypeCount, pModelList, renderMode );

	m_pRenderContext = NULL;
}


//-----------------------------------------------------------------------------
// Computes per-instance data for fast path rendering
//-----------------------------------------------------------------------------
void CModelRenderSystem::ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData )
{
	if ( nCount == 0 )
	{
		pTempData->m_nColorMeshHandleCount = 0;
		pTempData->m_bReleaseRenderData = false;
		return;
	}
						 
	VPROF_BUDGET( "CModelRenderSystem::ComputeTranslucentRenderData", VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING );
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	// While doing this, we need materialsystem to keep around its temp allocations
	// which we use for bone matrices + flexes
	CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
	m_pRenderContext = matRenderContext;

	PIXEVENT( m_pRenderContext, "CModelRenderSystem::ComputeTranslucentRenderData (FASTPATH)" );

	m_pRenderContext->AddRefRenderData();
	pTempData->m_bReleaseRenderData = true;

	ModelRenderMode_t renderMode = MODEL_RENDER_MODE_NORMAL;

	// FIXME: This is infected-specific for perf test reasons.
	// Will break into a more fixed pipeline at a later date
	DataCacheHandle_t *pColorMeshHandles = pTempData->m_pColorMeshHandles;
	ModelListByType_t *pModelList = (ModelListByType_t*)stackalloc( nCount * sizeof(ModelListByType_t) );
	ModelListNode_t *pModelListNode = (ModelListNode_t*)stackalloc( nCount * sizeof(ModelListNode_t) );
	int nModelsRenderingStencilCount = 0;
	int nModelTypeCount = BucketModelsByMDL( pModelList, pModelListNode, pModels, nCount, renderMode, &nModelsRenderingStencilCount );

	LightingList_t pLightingList[ LIGHTING_MODEL_COUNT ];
	memset( pLightingList, 0, LIGHTING_MODEL_COUNT * sizeof(LightingList_t) );
	SortBucketsByDependency( nModelTypeCount, pModelList, pLightingList );

	// Compute LODs for each model
	ComputeModelLODs( nModelTypeCount, pModelList, pModelListNode, renderMode );

	// Sort processing list by body, lod, skin, etc.
	RenderModelInfo_t *pSortedModelListNode = (RenderModelInfo_t*)stackalloc( nCount * sizeof(RenderModelInfo_t) );
	SortModels( pSortedModelListNode, nModelTypeCount, pModelList, pModelListNode );

	// Setup bones
	SetupBones( nModelTypeCount, pModelList );

	// Setup flexes
	SetupFlexes( nModelTypeCount, pModelList );

	// Setup lighting
	pTempData->m_nColorMeshHandleCount = SetupLighting( pLightingList, nModelTypeCount, pModelList, pColorMeshHandles, renderMode );

	// Setup flashlights + decals
	StudioModelArrayInfo2_t info;
	SetupFlashlightsAndDecals( &info, nModelTypeCount, pModelList, nCount, pSortedModelListNode, renderMode );

	// Setup per-instance color modulation
	SetupPerInstanceColorModulation( nModelTypeCount, pModelList );

	// Setup per-instance wound data
	// SetupInfectedWoundRenderData( nModelTypeCount, pModelList, nCount, renderMode );

	// Draw models
	SetupTranslucentData( nModelTypeCount, pModelList, nCount, pRenderData );
	
	// Blat out temporary memory for bone-to-world transforms
	m_BoneToWorld.FreeAll( false );

	RenderDebugOverlays( nModelTypeCount, pModelList, renderMode );

	m_pRenderContext = NULL;
}

void CModelRenderSystem::CleanupTranslucentTempData( TranslucentTempData_t *pTempData )
{
	if ( pTempData->m_bReleaseRenderData )
	{
		modelrender->CleanupStaticLightingState( pTempData->m_nColorMeshHandleCount, pTempData->m_pColorMeshHandles );
		CMatRenderContextPtr matRenderContext( g_pMaterialSystem );
		matRenderContext->ReleaseRenderData();
	}
}
