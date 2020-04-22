//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef STUDIORENDERCONTEXT_H
#define STUDIORENDERCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

#include "istudiorender.h"
#include "tier3/tier3.h"
#include "studio.h"
#include "tier1/delegates.h"
#include "tier1/memstack.h"
#include "studiorender.h"


//-----------------------------------------------------------------------------
// Foward declarations
//-----------------------------------------------------------------------------
class IStudioDataCache;
class CStudioRender;


//-----------------------------------------------------------------------------
// Global interfaces
//-----------------------------------------------------------------------------
extern IStudioDataCache *g_pStudioDataCache;
extern CStudioRender *g_pStudioRenderImp;

IMaterial* GetModelSpecificDecalMaterial( IMaterial* pDecalMaterial );

//-----------------------------------------------------------------------------
// Internal config structure
//-----------------------------------------------------------------------------
struct StudioRenderConfigInternal_t : public StudioRenderConfig_t
{
	bool m_bSupportsVertexAndPixelShaders : 1;
	bool m_bSupportsOverbright : 1;
	bool m_bEnableHWMorph : 1;
	bool m_bStatsMode : 1;
};


//-----------------------------------------------------------------------------
// All the data needed to render a studiomodel
//-----------------------------------------------------------------------------
struct FlexWeights_t
{
	float *m_pFlexWeights;
	float *m_pFlexDelayedWeights;
};

struct StudioRenderContext_t
{
	StudioRenderConfigInternal_t	m_Config;
	Vector					m_ViewTarget;
	Vector					m_ViewOrigin;
	Vector					m_ViewRight;
	Vector					m_ViewUp;
	Vector					m_ViewPlaneNormal;
	Vector4D				m_LightBoxColors[6];
	LightDesc_t				m_LocalLights[MAXLOCALLIGHTS];
	int						m_NumLocalLights;
	float					m_ColorMod[3];
	float					m_AlphaMod;
	IMaterial*				m_pForcedMaterial;
	OverrideType_t			m_nForcedMaterialType;
};


//-----------------------------------------------------------------------------
// Helper to queue up calls if necessary
//-----------------------------------------------------------------------------
#define QUEUE_STUDIORENDER_CALL( FuncName, ClassName, pObject, ... )	\
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );			\
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();			\
	if ( !pCallQueue || studio_queue_mode.GetInt() == 0 )				\
	{																	\
		pObject->FuncName( __VA_ARGS__ );								\
	}																	\
	else																\
	{																	\
		pCallQueue->QueueCall( pObject, &ClassName::FuncName, ##__VA_ARGS__ );	\
	}

#define QUEUE_STUDIORENDER_CALL_RC( FuncName, ClassName, pObject, pRenderContext, ... )	\
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();			\
	if ( !pCallQueue || studio_queue_mode.GetInt() == 0 )				\
	{																	\
		pObject->FuncName( __VA_ARGS__ );								\
	}																	\
	else																\
	{																	\
		pCallQueue->QueueCall( pObject, &ClassName::FuncName, ##__VA_ARGS__ );	\
	}


//-----------------------------------------------------------------------------
// Implementation of IStudioRender
//-----------------------------------------------------------------------------
class CStudioRenderContext : public CTier3AppSystem< IStudioRender >
{
	typedef CTier3AppSystem< IStudioRender > BaseClass;

	// Methods of IAppSystem 
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IStudioRender
public:
	virtual void BeginFrame( void );
	virtual void EndFrame( void );
	virtual void Mat_Stub( IMaterialSystem *pMatSys );
	virtual void UpdateConfig( const StudioRenderConfig_t& config );
	virtual void GetCurrentConfig( StudioRenderConfig_t& config );
	virtual bool LoadModel(studiohdr_t *pStudioHdr, void *pVtxData, studiohwdata_t *pHardwareData);
	virtual void UnloadModel( studiohwdata_t *pHardwareData );
	virtual void RefreshStudioHdr( studiohdr_t* pStudioHdr, studiohwdata_t* pHardwareData );
	virtual void SetEyeViewTarget( const studiohdr_t *pStudioHdr, int nBodyIndex, const Vector& worldPosition );
	virtual void SetAmbientLightColors( const Vector *pAmbientOnlyColors );
	virtual void SetAmbientLightColors( const Vector4D *pAmbientOnlyColors );
	virtual void SetLocalLights( int numLights, const LightDesc_t *pLights );
	virtual int GetNumAmbientLightSamples();
	virtual const Vector *GetAmbientLightDirections();
	virtual void SetViewState( const Vector& viewOrigin, const Vector& viewRight, const Vector& viewUp, const Vector& viewPlaneNormal );
	virtual int GetNumLODs( const studiohwdata_t &hardwareData ) const;
	virtual float GetLODSwitchValue( const studiohwdata_t &hardwareData, int lod ) const;
	virtual void SetLODSwitchValue( studiohwdata_t &hardwareData, int lod, float switchValue );
	virtual void SetColorModulation( const float* pColor );
	virtual void SetAlphaModulation( float alpha );
	virtual void DrawModel( DrawModelResults_t *pResults, const DrawModelInfo_t& info, matrix3x4_t *pCustomBoneToWorld, float *pFlexWeights, float *pFlexDelayedWeights, const Vector& origin, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
	virtual void DrawModelArray( const DrawModelInfo_t &drawInfo, int arrayCount, model_array_instance_t *pInstanceData, int instanceStride, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
	virtual void DrawModelStaticProp( const DrawModelInfo_t& info, const matrix3x4_t &modelToWorld, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
	virtual void DrawStaticPropDecals( const DrawModelInfo_t &drawInfo, const matrix3x4_t &modelToWorld );
	virtual void DrawStaticPropShadows( const DrawModelInfo_t &drawInfo, const matrix3x4_t &modelToWorld, int flags );
	virtual void ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType = OVERRIDE_NORMAL );
	DELEGATE_TO_OBJECT_1( StudioDecalHandle_t, CreateDecalList, studiohwdata_t *, g_pStudioRenderImp );
	virtual void DestroyDecalList( StudioDecalHandle_t handle );
	virtual void AddDecal( StudioDecalHandle_t handle, studiohdr_t *pStudioHdr, matrix3x4_t *pBoneToWorld, const Ray_t & ray, const Vector& decalUp, IMaterial* pDecalMaterial, float radius, int body, bool noPokethru, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );
	virtual void ComputeLighting( const Vector* pAmbient, int lightCount, LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting );
	virtual void ComputeLightingConstDirectional( const Vector* pAmbient, int lightCount, LightDesc_t* pLights, const Vector& pt, const Vector& normal, Vector& lighting, float flDirectionalAmount );
	virtual void AddShadow( IMaterial* pMaterial, void* pProxyData, FlashlightState_t *pFlashlightState, VMatrix *pWorldToTexture, ITexture *pFlashlightDepthTexture );
	virtual void ClearAllShadows();
	virtual int ComputeModelLod( studiohwdata_t* pHardwareData, float flUnitSphereSize, float *pMetric = NULL );
	virtual void GetPerfStats( DrawModelResults_t *pResults, const DrawModelInfo_t &info, CUtlBuffer *pSpewBuf = NULL ) const;
	virtual void GetTriangles( const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, GetTriangles_Output_t &out );
	virtual int GetMaterialList( studiohdr_t *pStudioHdr, int count, IMaterial** ppMaterials );
	virtual int GetMaterialListFromBodyAndSkin( MDLHandle_t studio, int nSkin, int nBody, int nCountOutputMaterials, IMaterial** ppOutputMaterials );
	virtual matrix3x4_t* LockBoneMatrices( int nCount );
	virtual void UnlockBoneMatrices();
	virtual void LockFlexWeights( int nWeightCount, float **ppFlexWeights, float **ppFlexDelayedWeights = NULL );
	virtual void UnlockFlexWeights();
	virtual void GetMaterialOverride( IMaterial** ppOutForcedMaterial, OverrideType_t* pOutOverrideType );

	// Other public methods
public:
	CStudioRenderContext();
	virtual ~CStudioRenderContext();

private:
	// Load, unload materials
	void LoadMaterials( studiohdr_t *phdr, OptimizedModel::FileHeader_t *, studioloddata_t &lodData, int lodID );

	// Determines material flags
	void ComputeMaterialFlags( studiohdr_t *phdr, studioloddata_t &lodData, IMaterial *pMaterial );

	// Creates, destroys static meshes
	void R_StudioCreateStaticMeshes( studiohdr_t *pStudioHdr, OptimizedModel::FileHeader_t* pVtxHdr,
		studiohwdata_t *pStudioHWData, int lodID, int *pColorMeshID );
	void R_StudioCreateSingleMesh( studiohdr_t *pStudioHdr, studioloddata_t *pStudioLodData, 
		mstudiomesh_t* pMesh, OptimizedModel::MeshHeader_t* pVtxMesh, int numBones, 
		studiomeshdata_t* pMeshData, int *pColorMeshID );
	void R_StudioDestroyStaticMeshes( int numStudioMeshes, studiomeshdata_t **ppStudioMeshes );

	// Determine if any strip groups shouldn't be morphed
	void DetermineHWMorphing( mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD );

	// Count deltas affecting a particular stripgroup
	int CountDeltaFlexedStripGroups( mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD );

	// Count vertices affected by deltas in a particular strip group
	int CountFlexedVertices( mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t* pStripGroup );

	// Builds morph data
	void R_StudioBuildMorph( studiohdr_t *pStudioHdr, studiomeshgroup_t* pMeshGroup,	mstudiomesh_t* pMesh,
		OptimizedModel::StripGroupHeader_t *pStripGroup );

	// Builds the decal bone remap for a particular mesh
	void ComputeHWMorphDecalBoneRemap( studiohdr_t *pStudioHdr, OptimizedModel::FileHeader_t *pVtxHdr, studiohwdata_t *pStudioHWData, int nLOD );
	void BuildDecalBoneMap( studiohdr_t *pStudioHdr, int *pUsedBones, int *pBoneRemap, int *pMaxBoneCount, mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t* pStripGroup );

	// Helper methods used to construct static meshes
	int GetNumBoneWeights( const OptimizedModel::StripGroupHeader_t *pGroup );
	VertexFormat_t CalculateVertexFormat( const studiohdr_t *pStudioHdr, const studioloddata_t *pStudioLodData,
																const mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t *pGroup, bool bIsHwSkinned );
	bool MeshNeedsTangentSpace( studiohdr_t *pStudioHdr, studioloddata_t *pStudioLodData, mstudiomesh_t* pMesh );
	void R_StudioBuildMeshGroup( const char *pModelName, bool bNeedsTangentSpace, studiomeshgroup_t* pMeshGroup,
		OptimizedModel::StripGroupHeader_t *pStripGroup, mstudiomesh_t* pMesh,
		studiohdr_t *pStudioHdr, VertexFormat_t vertexFormat );
	void R_StudioBuildMeshStrips( studiomeshgroup_t* pMeshGroup,
		OptimizedModel::StripGroupHeader_t *pStripGroup );
	template <VertexCompressionType_t T> bool R_AddVertexToMesh( const char *pModelName, bool bNeedsTangentSpace, CMeshBuilder& meshBuilder, 
		OptimizedModel::Vertex_t* pVertex, mstudiomesh_t* pMesh, const mstudio_meshvertexdata_t *vertData, bool hwSkin );

	// This will generate random flex data that has a specified # of non-zero values
	void GenerateRandomFlexWeights( int nWeightCount, float* pWeights, float *pDelayedWeights );

	// Computes LOD
	int ComputeRenderLOD( IMatRenderContext *pRenderContext, const DrawModelInfo_t& info, const Vector &origin, float *pMetric );

	// This invokes proxies of all materials that are queued to be rendered
	void InvokeBindProxies( const DrawModelInfo_t &info );

	// Did this matrix come from our allocator?
	bool IsInternallyAllocated( const matrix3x4_t *pBoneToWorld );

	// Did this flex weights come from our allocator?
	bool IsInternallyAllocated( const float *pFlexWeights );

private:
	StudioRenderContext_t m_RC;

	// Used by the lighting computation methods,
	// this is only here to prevent constructors in lightpos_t from being repeatedly run
	lightpos_t m_pLightPos[MAXLIGHTCOMPUTE];
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline int CStudioRenderContext::ComputeModelLod( studiohwdata_t *pHardwareData, float flUnitSphereSize, float *pMetric )
{
	return ComputeModelLODAndMetric( pHardwareData, flUnitSphereSize, pMetric );
}


#endif // STUDIORENDERCONTEXT_H
