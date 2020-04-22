//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef CSTUDIORENDER_H
#define CSTUDIORENDER_H
#ifdef _WIN32
#pragma once
#endif

#include "istudiorender.h"
#include "studio.h"
#include "materialsystem/imaterialsystem.h" // for LightDesc_t
// wouldn't have to include these if it weren't for inlines.
#include "materialsystem/imaterial.h"
#include "mathlib/mathlib.h"
#include "utllinkedlist.h"
#include "utlvector.h"
#include "tier1/utllinkedlist.h"
#include "flexrenderdata.h"
#include "mathlib/compressed_vector.h"
#include "r_studiolight.h"
#if defined( _WIN32 ) && !defined( _X360 )
#include <xmmintrin.h>
#endif
#include "tier0/dbg.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITexture;
class CPixelWriter;
class CMeshBuilder;
class IMaterialVar;
struct mstudioeyeball_t;
struct eyeballstate_t;
struct lightpos_t;
struct dworldlight_t;
struct DecalClipState_t;
class CStudioRender;
struct StudioRenderContext_t;
struct FlexWeights_t;

namespace OptimizedModel
{
	struct FileHeader_t;
	struct MeshHeader_t;
	struct StripGroupHeader_t;
	struct Vertex_t;
	struct ModelLODHeader_t;
}


//-----------------------------------------------------------------------------
// FIXME: Remove
//-----------------------------------------------------------------------------
class IStudioDataCache;
extern IStudioDataCache *g_pStudioDataCache;


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern CStudioRender g_StudioRender;


//-----------------------------------------------------------------------------
// Defines + structs
//-----------------------------------------------------------------------------
#define MAXLOCALLIGHTS 4
#define MAXLIGHTCOMPUTE 16

enum StudioModelLighting_t
{
	LIGHTING_HARDWARE = 0,
	LIGHTING_SOFTWARE,
	LIGHTING_MOUTH
};

struct lightpos_t
{
	Vector	delta;		// unit vector from vertex to light
	float	falloff;	// light distance falloff
	float	dot;		// light direction * delta;

	lightpos_t() {}

private:
	// Copy constructors are not allowed
	lightpos_t( const lightpos_t& src );
};

struct eyeballstate_t
{
	const mstudioeyeball_t *peyeball;

	matrix3x4_t	mat;

	Vector	org;		// world center of eyeball
	Vector	forward;	
	Vector	right;
	Vector	up;
	
	Vector	cornea;		// world center of cornea

	eyeballstate_t() {}

private:
	// Copy constructors are not allowed
	eyeballstate_t( const eyeballstate_t& src );
};


//-----------------------------------------------------------------------------
// Store decal vertex data here 
//-----------------------------------------------------------------------------
#pragma pack(1)
struct DecalVertex_t
{
	mstudiomesh_t *GetMesh( studiohdr_t *pHdr )
	{
		if ((m_Body == 0xFFFF) || (m_Model == 0xFFFF) || (m_Mesh == 0xFFFF))
			return NULL;

		mstudiobodyparts_t *pBody = pHdr->pBodypart( m_Body );
		mstudiomodel_t *pModel = pBody->pModel( m_Model );
		return pModel->pMesh( m_Mesh );
	}

	IMorph *GetMorph( studiohdr_t *pHdr, studiomeshdata_t *pStudioMeshes )
	{
		if ( (m_Body == 0xFFFF) || (m_Model == 0xFFFF) || (m_Mesh == 0xFFFF) || (m_Group == 0xFFFF) )
			return NULL;

		mstudiobodyparts_t *pBody = pHdr->pBodypart( m_Body );
		mstudiomodel_t *pModel = pBody->pModel( m_Model );
		mstudiomesh_t *pMesh = pModel->pMesh( m_Mesh );
		studiomeshdata_t* pMeshData = &pStudioMeshes[pMesh->meshid];
		studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[m_Group];
		return pGroup->m_pMorph;
	}

	// NOTE: m_Group + m_GroupIndex is necessary only for decals on
	// hardware morphs. If COMPACT_DECAL_VERT is for console, we
	// could remove group index + group
#ifdef COMPACT_DECAL_VERT
	Vector			m_Position;			// 12
	Vector2d32		m_TexCoord;			// 16
	Vector48		m_Normal;			// 22 (packed to m_Body)

	byte			m_Body;				// 24
	byte			m_Model;
	unsigned short	m_MeshVertexIndex;	// index into the mesh's vertex list
	unsigned short	m_Mesh;
	unsigned short	m_GroupIndex;		// index into the mesh's vertex list
	unsigned short	m_Group;
#else
	Vector m_Position;
	Vector m_Normal;
	Vector2D m_TexCoord;

	unsigned short	m_MeshVertexIndex;	// index into the mesh's vertex list
	unsigned short	m_Body;
	unsigned short	m_Model;
	unsigned short	m_Mesh;
	unsigned short	m_GroupIndex;		// index into the group's index list
	unsigned short	m_Group;
#endif

	DecalVertex_t() {}
	DecalVertex_t( const DecalVertex_t& src )
	{
		m_Position = src.m_Position;
		m_Normal = src.m_Normal;
		m_TexCoord = src.m_TexCoord;
		m_MeshVertexIndex = src.m_MeshVertexIndex;
		m_Body = src.m_Body;
		m_Model = src.m_Model;
		m_Mesh = src.m_Mesh;
		m_GroupIndex = src.m_GroupIndex;
		m_Group = src.m_Group;
	}
};
#pragma pack()


//-----------------------------------------------------------------------------
// Temporary meshes
//-----------------------------------------------------------------------------
struct MeshVertexInfo_t
{
	mstudiomesh_t *m_pMesh;
	int m_nIndex;
};


//-----------------------------------------------------------------------------
// Vertex prefetch count for software skinning
//-----------------------------------------------------------------------------
enum
{
	PREFETCH_VERT_COUNT = 4
};


//-----------------------------------------------------------------------------
// Class that actually renders stuff
//-----------------------------------------------------------------------------
class CStudioRender
{
public:
	CStudioRender();
	~CStudioRender();

	// Init, shutdown
	InitReturnVal_t Init();
	void Shutdown( void );

	void EnableScissor( FlashlightState_t *state );
	void DisableScissor();

	void DrawModel( const DrawModelInfo_t& info, const StudioRenderContext_t& rc, matrix3x4_t *pBoneToWorld, const FlexWeights_t& flex, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
	void DrawModelArray( const DrawModelInfo_t &drawInfo, const StudioRenderContext_t &rc, int arrayCount, model_array_instance_t *pInstanceData, int instanceStride, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );

	// Static-prop related draw methods
	void DrawModelStaticProp( const DrawModelInfo_t& info, const StudioRenderContext_t &rc, const matrix3x4_t &modelToWorld, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
	void DrawStaticPropShadows( const DrawModelInfo_t &drawInfo, const StudioRenderContext_t &rc, const matrix3x4_t &modelToWorld, int flags );
	void DrawStaticPropDecals( const DrawModelInfo_t &drawInfo, const StudioRenderContext_t &rc, const matrix3x4_t &modelToWorld );

	void ModelStats( const DrawModelInfo_t& info, const StudioRenderContext_t &rc, matrix3x4_t *pBoneToWorld, const FlexWeights_t &flex, int flags );

	// Create, destroy list of decals for a particular model
	StudioDecalHandle_t CreateDecalList( studiohwdata_t *pHardwareData );
	void DestroyDecalList( StudioDecalHandle_t handle );

	// Add decals to a decal list by doing a planar projection along the ray
	void AddDecal( StudioDecalHandle_t handle, const StudioRenderContext_t& rc, matrix3x4_t *pBoneToWorld, studiohdr_t *pStudioHdr, 
			const Ray_t & ray, const Vector& decalUp, IMaterial* pDecalMaterial, 
			float radius, int body, bool noPokethru, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );

	// Shadow state (affects the models as they are rendered)
	void AddShadow( IMaterial* pMaterial, void* pProxyData, FlashlightState_t *pFlashlightState, VMatrix *pWorldToTexture, ITexture *pFlashlightDepthTexture );
	void ClearAllShadows();

	// Release/restore material system objects
	void PrecacheGlint();
	void UncacheGlint();

	// Get the config
	void R_MouthComputeLightingValues( float& fIllum, Vector& forward );
	void R_MouthLighting( float fIllum, const Vector& normal, const Vector& forward, Vector& light );

	// Performs the lighting computation
	inline void R_ComputeLightAtPoint3( const Vector &pos, const Vector &norm, Vector &color );

#if defined( _WIN32 ) && !defined( _X360 )
	// sse-ized lighting pipeline. lights 4 vertices at once
	inline void R_ComputeLightAtPoints3( const FourVectors &pos, const FourVectors &norm, FourVectors &color );
	void R_MouthLighting( __m128 fIllum, const FourVectors& normal, const FourVectors& forward, FourVectors& light );
#endif

private:
	enum
	{
		DECAL_DYNAMIC = 0x1,
		DECAL_SECONDPASS = 0x2,
	};

	typedef unsigned short DecalId_t;

	struct Decal_t
	{
		int m_IndexCount;
		int m_VertexCount;
		float m_FadeStartTime;
		float m_FadeDuration;
		int	m_Flags;
	};

	struct DecalHistory_t
	{
		unsigned short m_Material;
		unsigned short m_Decal;
		DecalId_t m_nId;
		unsigned short m_nPad;
	};

	typedef CUtlLinkedList<DecalVertex_t, unsigned short> DecalVertexList_t;

	typedef CUtlVector<unsigned short> DecalIndexList_t;
	typedef CUtlLinkedList<Decal_t, unsigned short> DecalList_t;
	typedef CUtlLinkedList<DecalHistory_t, unsigned short> DecalHistoryList_t;

	struct DecalMaterial_t
	{
		IMaterial*			m_pMaterial;
		DecalIndexList_t	m_Indices;
		DecalVertexList_t	m_Vertices;
		DecalList_t			m_Decals;
	};

	struct DecalLod_t
	{
		unsigned short m_FirstMaterial;
		DecalHistoryList_t	m_DecalHistory;
	};

	struct DecalModelList_t
	{
		studiohwdata_t* m_pHardwareData;
		DecalLod_t* m_pLod;
		int m_nLods; // need to retain because hardware data could be flushed
	};

	// A temporary structure used to figure out new decal verts
	struct DecalBuildVertexInfo_t
	{
		enum
		{
			FRONT_FACING = 0x1,
			VALID_AREA = 0x2,		// If you change this, change ProjectDecalOntoMesh
		};

		Vector2D		m_UV;
		unsigned short	m_VertexIndex;	// index into the DecalVertex_t list
		unsigned char	m_UniqueID;
		unsigned char	m_Flags;

	private:
		// No copy constructors
		DecalBuildVertexInfo_t( const DecalBuildVertexInfo_t &src );
	};

	struct DecalBuildInfo_t
	{
		IMaterial						**m_ppMaterials;
		studiohdr_t						*m_pStudioHdr;
		mstudiomesh_t					*m_pMesh;
		studiomeshdata_t				*m_pMeshData;
		DecalMaterial_t					*m_pDecalMaterial;
		MeshVertexInfo_t				*m_pMeshVertices;
		const mstudio_meshvertexdata_t	*m_pMeshVertexData;
		const thinModelVertices_t		*m_pMeshThinVertexData;
		int								m_nGlobalMeshIndex;
		DecalBuildVertexInfo_t			*m_pVertexBuffer;
		float							m_Radius;
		DecalBuildVertexInfo_t			*m_pVertexInfo;
		int								m_Body;
		int								m_Model;
		int								m_Mesh;
		int								m_Group;
		DecalVertexList_t::IndexType_t	m_FirstVertex;
		unsigned short					m_VertexCount;
		bool							m_UseClipVert;
		bool							m_NoPokeThru;
	};

	struct ShadowState_t
	{
		IMaterial*			m_pMaterial;
		void*				m_pProxyData;
		FlashlightState_t * m_pFlashlightState;
		VMatrix *			m_pWorldToTexture;
		ITexture *			m_pFlashlightDepthTexture;
	};

	struct BodyPartInfo_t
	{
		int m_nSubModelIndex;
		mstudiomodel_t *m_pSubModel;
	};

	struct GlintRenderData_t
	{
		Vector2D m_vecPosition;
		Vector m_vecIntensity;
	};

	// Global LRU for model decals
	struct DecalLRU_t
	{
		StudioDecalHandle_t m_hDecalHandle;
		DecalId_t m_nDecalId;
	};

	typedef CUtlFixedLinkedList< DecalLRU_t >::IndexType_t DecalLRUListIndex_t;

private:
	void SetLightingRenderState();

	int R_StudioRenderModel( IMatRenderContext *pRenderContext, int skin, int body, int hitboxset, void /*IClientEntity*/ *pEntity, 
		IMaterial **ppMaterials, int *pMaterialFlags, int flags, int boneMask, int lod, ColorMeshInfo_t *pColorMeshes = NULL );
	IMaterial* R_StudioSetupSkinAndLighting( IMatRenderContext *pRenderContext, int index, IMaterial **ppMaterials, int materialFlags,
		void /*IClientEntity*/ *pClientEntity, ColorMeshInfo_t *pColorMeshes, StudioModelLighting_t &lighting );
	int R_StudioDrawEyeball( IMatRenderContext *pRenderContext, mstudiomesh_t* pmesh,  studiomeshdata_t* pMeshData,
		StudioModelLighting_t lighting, IMaterial *pMaterial, int lod );
	int R_StudioDrawPoints( IMatRenderContext *pRenderContext, int skin, void /*IClientEntity*/ *pClientEntity, 
		IMaterial **ppMaterials, int *pMaterialFlags, int boneMask, int lod, ColorMeshInfo_t *pColorMeshes );
	int R_StudioDrawMesh( IMatRenderContext *pRenderContext, mstudiomesh_t* pmesh, studiomeshdata_t* pMeshData,
		  				   StudioModelLighting_t lighting, IMaterial *pMaterial, ColorMeshInfo_t *pColorMeshes, int lod );
	int R_StudioRenderFinal( IMatRenderContext *pRenderContext, 
		int skin, int nBodyPartCount, BodyPartInfo_t *pBodyPartInfo, void /*IClientEntity*/ *pClientEntity,
		IMaterial **ppMaterials, int *pMaterialFlags, int boneMask, int lod, ColorMeshInfo_t *pColorMeshes = NULL );
	int R_StudioDrawStaticMesh( IMatRenderContext *pRenderContext, mstudiomesh_t* pmesh, 
		studiomeshgroup_t* pGroup, StudioModelLighting_t lighting, float r_blend, IMaterial* pMaterial,
		int lod, ColorMeshInfo_t *pColorMeshes );
	int R_StudioDrawDynamicMesh( IMatRenderContext *pRenderContext, mstudiomesh_t* pmesh, 
				studiomeshgroup_t* pGroup, StudioModelLighting_t lighting, 
				float r_blend, IMaterial* pMaterial, int lod );
	int R_StudioDrawGroupHWSkin( IMatRenderContext *pRenderContext, studiomeshgroup_t* pGroup, IMesh* pMesh, ColorMeshInfo_t *pColorMeshInfo = NULL );
	int R_StudioDrawGroupSWSkin( studiomeshgroup_t* pGroup, IMesh* pMesh );
	void R_StudioDrawHulls( int hitboxset, bool translucent );
	void R_StudioDrawBones (void);
	void R_StudioVertBuffer( void );
	void DrawNormal( const Vector& pos, float scale, const Vector& normal, const Vector& color );
	void BoneMatToMaterialMat( matrix3x4_t& boneMat, float materialMat[4][4] );

	// Various inner-loop methods
	void R_StudioSoftwareProcessMesh( mstudiomesh_t* pmesh, CMeshBuilder& meshBuilder, 
			int numVertices, unsigned short* pGroupToMesh, StudioModelLighting_t lighting, bool doFlex, float r_blend,
			bool bNeedsTangentSpace, bool bDX8Vertex, IMaterial *pMaterial );

	void R_StudioSoftwareProcessMesh_Normals( mstudiomesh_t* pmesh, CMeshBuilder& meshBuilder, 
			int numVertices, unsigned short* pGroupToMesh, StudioModelLighting_t lighting, bool doFlex, float r_blend,
			bool bShowNormals, bool bShowTangentFrame );

	template< class T >
	void ComputeFlexedVertex_StreamOffset( mstudioflex_t *pflex, T *pvanim, int vertCount, float w1, float w2, float w3, float w4 );

	void R_StudioProcessFlexedMesh_StreamOffset( mstudiomesh_t* pmesh, int lod );

	template <VertexCompressionType_t T> void FillFlexMeshGroupVB( CMeshBuilder & meshBuilder, studiomeshgroup_t *pGroup );
	void R_StudioFlexMeshGroup( studiomeshgroup_t *pGroup );

	template<VertexCompressionType_t T> void R_StudioRestoreMesh( mstudiomesh_t* pmesh, studiomeshgroup_t* pMeshData );
	void R_StudioProcessFlexedMesh( mstudiomesh_t* pmesh, CMeshBuilder& meshBuilder, 
		int numVertices, unsigned short* pGroupToMesh );

	// Eye rendering using vertex shaders
	void SetEyeMaterialVars( IMaterial* pMaterial, mstudioeyeball_t* peyeball, 
		const Vector& eyeOrigin, const matrix3x4_t& irisTransform, const matrix3x4_t& glintTransform );

	void ComputeEyelidStateFACS( mstudiomodel_t *pSubModel );

	void R_StudioEyelidFACS( const mstudioeyeball_t *peyeball, const eyeballstate_t *pstate );

	void R_StudioEyeballPosition( const mstudioeyeball_t *peyeball, eyeballstate_t *pstate );

	// Computes the texture projection matrix for the glint texture
	void ComputeGlintTextureProjection( eyeballstate_t const* pState, 
		const Vector& vright, const Vector& vup, matrix3x4_t& mat );

	void R_StudioEyeballGlint( const eyeballstate_t *pstate, IMaterialVar *pGlintTextureVar, 
								const Vector& vright, const Vector& vup, const Vector& r_origin );
	ITexture* RenderGlintTexture( const eyeballstate_t *pstate,
		const Vector& vright, const Vector& vup, const Vector& r_origin );

	int BuildGlintRenderData( GlintRenderData_t *pData, int nMaxGlints,
		const eyeballstate_t *pstate, const Vector& vright, const Vector& vup, const Vector& r_origin );
	void R_MouthSetupVertexShader( IMaterial* pMaterial );

	// Computes a vertex format to use
	VertexFormat_t ComputeSWSkinVertexFormat( IMaterial *pMaterial ) const;

	inline bool R_TeethAreVisible( void )
	{
		return true;
		/*
		// FIXME: commented out until Gary can change them to just draw black
		mstudiomouth_t *pMouth = m_pStudioHdr->pMouth( 0 ); 
		float fIllum = m_FlexWeights[pMouth->flexdesc];
		return fIllum > 0.0f;
		*/
	}

	inline StudioModelLighting_t R_StudioComputeLighting( IMaterial *pMaterial, int materialFlags, ColorMeshInfo_t *pColorMeshes );
	inline void R_StudioTransform( Vector& in1, mstudioboneweight_t *pboneweight, Vector& out1 );
	inline void R_StudioRotate( Vector& in1, mstudioboneweight_t *pboneweight, Vector& out1 );
	inline void R_StudioRotate( Vector4D& in1, mstudioboneweight_t *pboneweight, Vector4D& out1 );
	inline void R_StudioEyeballNormal( mstudioeyeball_t const* peyeball, Vector& org, 
									Vector& pos, Vector& normal );
	void MaterialPlanerProjection( const matrix3x4_t& mat, int count, const Vector *psrcverts, Vector2D *pdesttexcoords );
	void AddGlint( CPixelWriter &pixelWriter, float x, float y, const Vector& color );

	// Methods associated with lighting
	int R_LightGlintPosition( int index, const Vector& org, Vector& delta, Vector& intensity );
	void R_LightEffectsWorld( const lightpos_t *light, const Vector& normal, const Vector &src, Vector &dest );

	void R_GatherStats( studiomeshgroup_t *pGroup, CMeshBuilder &MeshBuilder, IMesh *pMesh, IMaterial *pMaterial );

public:
	// NJS: Messy, but needed for an externally optimized routine to set up the lighting.
	void R_InitLightEffectsWorld3();
	void (FASTCALL *R_LightEffectsWorld3)( const LightDesc_t *pLightDesc, const lightpos_t *light, const Vector& normal, Vector &dest );

private:
	inline float R_WorldLightAngle( const LightDesc_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta );

	void InitDebugMaterials( void );
	void ShutdownDebugMaterials( void );
	int SortMeshes( int* pIndices, IMaterial **ppMaterials, short* pskinref, const Vector& vforward, const Vector& r_origin );

	// Computes pose to decal space transforms for decal creation 
	// returns false if it can't for some reason.
	bool ComputePoseToDecal( Ray_t const& ray, const Vector& up );

	bool AddDecalToModel( DecalBuildInfo_t& buildInfo );

	// Helper methods for decal projection, projects pose space vertex data
	bool			TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, mstudioboneweight_t *pboneweight, Vector2D& uv );
	bool			ProjectDecalOntoMesh( DecalBuildInfo_t& build, DecalBuildVertexInfo_t* pVertexInfo, mstudiomesh_t *pMesh );
	bool			IsFrontFacing( const Vector * norm, const mstudioboneweight_t *pboneweight );
	int				ComputeClipFlags( DecalBuildVertexInfo_t* pVertexInfo, int i );
	void			ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex, int nGroupIndex = 0xFFFF );
	unsigned short	AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex, int nGroupIndex = 0xFFFF );
	unsigned short	AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert );
	void			AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState );
	bool			ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags );
	void			AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int gi1, int gi2, int gi3 );
	void			AddDecalToMesh( DecalBuildInfo_t& build );
	int				GetDecalMaterial( DecalLod_t& decalLod, IMaterial* pDecalMaterial );
	int				AddDecalToMaterialList( DecalMaterial_t* pMaterial );

	// Total number of meshes we have to deal with
	int ComputeTotalMeshCount( int iRootLOD, int iMaxLOD, int body ) const;

	// Project decals onto all meshes
	void ProjectDecalsOntoMeshes( DecalBuildInfo_t& build, int nMeshCount );

	// Set up the locations for vertices to use
	int ComputeVertexAllocation( int iMaxLOD, int body, studiohwdata_t *pHardwareData, MeshVertexInfo_t *pVertexInfo );

	// Removes a decal and associated vertices + indices from the history list
	void RetireDecal( DecalModelList_t &list, DecalId_t nDecalID, int iLOD, int iMaxLOD );

	// Helper methods related to drawing decals
	void DrawSingleBoneDecals( CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial );
	bool DrawMultiBoneDecals( CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr );
	void DrawSingleBoneFlexedDecals( IMatRenderContext *pRenderContext, CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial );
	bool DrawMultiBoneFlexedDecals( IMatRenderContext *pRenderContext, CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr, studioloddata_t *pStudioLOD );
	void DrawDecalMaterial( IMatRenderContext *pRenderContext, DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr, studioloddata_t *pStudioLOD );
	void DrawDecal( const DrawModelInfo_t &drawInfo, int lod, int body );
	bool PreDrawDecal( IMatRenderContext *pRenderContext, const DrawModelInfo_t &drawInfo );
	
	// Draw shadows
	void DrawShadows( const DrawModelInfo_t& info, int flags, int boneMask );

	// Draw flashlight lighting on decals.
	void DrawFlashlightDecals( const DrawModelInfo_t& info, int lod );
	
	// Helper methods related to extracting and balancing
	float RampFlexWeight( mstudioflex_t &flex, float w );

	// Remove decal from LRU
	void RemoveDecalListFromLRU( StudioDecalHandle_t h );

	// Helper methods related to flexing vertices
	void R_StudioFlexVerts( mstudiomesh_t *pmesh, int lod );

	// Flex stats
	void GetFlexStats( );

	// Sets up the hw flex mesh
	void ComputeFlexWeights( int nFlexCount, mstudioflex_t *pFlex, MorphWeight_t *pWeights );

	// Generate morph accumulator
	void GenerateMorphAccumulator( mstudiomodel_t *pSubModel );

	// Computes eyeball state
	void ComputeEyeballState( mstudiomodel_t *pSubModel );

	// Avoid some warnings...
	CStudioRender( CStudioRender const& );

public:
	// Render context (comes from queue)
	StudioRenderContext_t *m_pRC;

private:
	// Stores all decals for a particular material and lod
	CUtlLinkedList< DecalMaterial_t, unsigned short, true >	m_DecalMaterial;

	// Stores all decal lists that have been made
	CUtlFixedLinkedList< DecalModelList_t >	m_DecalList;
	CThreadFastMutex m_DecalMutex;

	// Stores all shadows to be cast on the current object
	CUtlVector<ShadowState_t>	m_ShadowState;

	matrix3x4_t m_StaticPropRootToWorld;
	matrix3x4_t	*m_pBoneToWorld;	// bone transformation matrix( comes from queue )

	matrix3x4_t *m_PoseToWorld;	// bone transformation matrix
	matrix3x4_t *m_PoseToDecal;	// bone transformation matrix

	// Flex state, comes from queue
	float *m_pFlexWeights;
	float *m_pFlexDelayedWeights;

	studiohdr_t *m_pStudioHdr;
	mstudiomodel_t *m_pSubModel;
	studiomeshdata_t *m_pStudioMeshes;

	eyeballstate_t m_pEyeballState[16]; // MAXSTUDIOEYEBALLS

	// debug materials
	IMaterial		*m_pMaterialMRMWireframe;
	IMaterial		*m_pMaterialMRMWireframeZBuffer;
	IMaterial		*m_pMaterialMRMNormals;
	IMaterial		*m_pMaterialTangentFrame;
	IMaterial		*m_pMaterialTranslucentModelHulls;
	IMaterial		*m_pMaterialSolidModelHulls;
	IMaterial		*m_pMaterialAdditiveVertexColorVertexAlpha;
	IMaterial		*m_pMaterialModelBones;
	IMaterial		*m_pMaterialWorldWireframe;
	IMaterial		*m_pMaterialModelEnvCubemap;

	// Depth override material
	IMaterial		*m_pDepthWrite[2][2];
	IMaterial		*m_pSSAODepthWrite[2][2];

	// GLINT data
	ITexture* m_pGlintTexture;
	ITexture* m_pGlintLODTexture;
	IMaterial *m_pGlintBuildMaterial;
	short	m_GlintWidth;
	short	m_GlintHeight;

	// Flex data
	CCachedRenderData	m_VertexCache;

	// Cached variables:
	bool m_bSkippedMeshes : 1;
	bool m_bDrawTranslucentSubModels : 1;

	DecalId_t	m_nDecalId;
	CUtlFixedLinkedList< DecalLRU_t > m_DecalLRU;

	friend class CGlintTextureRegenerator;
	friend struct mstudiomodel_t;
	friend class CStudioRenderContext;
};


//-----------------------------------------------------------------------------
// Converts matrices to a format material system wants
//-----------------------------------------------------------------------------

/*
================
R_StudioTransform
================
*/
inline void CStudioRender::R_StudioTransform( Vector& in1, mstudioboneweight_t *pboneweight, Vector& out1 )
{
//	MEASURECODE( "R_StudioTransform" );

	Vector out2;
	switch( pboneweight->numbones )
	{
	case 1:
		VectorTransform( in1, m_PoseToWorld[(unsigned)pboneweight->bone[0]], out1 );
		break;
/*
	case 2:
		VectorTransform( in1, m_PoseToWorld[pboneweight->bone[0]], out1 );
		out1 *= pboneweight->weight[0];
		VectorTransform( in1, m_PoseToWorld[pboneweight->bone[1]], out2 );
		VectorMA( out1, pboneweight->weight[1], out2, out1 );
		break;

	case 3:
		VectorTransform( in1, m_PoseToWorld[pboneweight->bone[0]], out1 );
		out1 *= pboneweight->weight[0];
		VectorTransform( in1, m_PoseToWorld[pboneweight->bone[1]], out2 );
		VectorMA( out1, pboneweight->weight[1], out2, out1 );
		VectorTransform( in1, m_PoseToWorld[pboneweight->bone[2]], out2 );
		VectorMA( out1, pboneweight->weight[2], out2, out1 );
		break;
*/
	default:
		VectorFill( out1, 0 );
		for (int i = 0; i < pboneweight->numbones; i++)
		{
			VectorTransform( in1, m_PoseToWorld[(unsigned)pboneweight->bone[i]], out2 );
			VectorMA( out1, pboneweight->weight[i], out2, out1 );
		}
		break;
	}
}


/*
================
R_StudioRotate
================
*/
inline void CStudioRender::R_StudioRotate( Vector& in1, mstudioboneweight_t *pboneweight, Vector& out1 )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to world

	if (pboneweight->numbones == 1)
	{
		VectorRotate( in1, m_PoseToWorld[(unsigned)pboneweight->bone[0]], out1 );
	}
	else
	{
		Vector out2;

		VectorFill( out1, 0 );

		for (int i = 0; i < pboneweight->numbones; i++)
		{
			VectorRotate( in1, m_PoseToWorld[(unsigned)pboneweight->bone[i]], out2 );
			VectorMA( out1, pboneweight->weight[i], out2, out1 );
		}
		VectorNormalize( out1 );
	}
}

inline void CStudioRender::R_StudioRotate( Vector4D& realIn1, mstudioboneweight_t *pboneweight, Vector4D& realOut1 )
{
	// garymcthack - god this sucks.
	Vector in1( realIn1[0], realIn1[1], realIn1[2] );
	Vector out1;
	if (pboneweight->numbones == 1)
	{
		VectorRotate( in1, m_PoseToWorld[(unsigned)pboneweight->bone[0]], out1 );
	}
	else
	{
		Vector out2;

		VectorFill( out1, 0 );

		for (int i = 0; i < pboneweight->numbones; i++)
		{
			VectorRotate( in1, m_PoseToWorld[(unsigned)pboneweight->bone[i]], out2 );
			VectorMA( out1, pboneweight->weight[i], out2, out1 );
		}
		VectorNormalize( out1 );
	}
	realOut1.Init( out1[0], out1[1], out1[2], realIn1[3] );
}


//-----------------------------------------------------------------------------
// Compute the contribution of a light depending on it's angle
//-----------------------------------------------------------------------------
/*
  light_normal (lights normal translated to same space as other normals)
  surface_normal
  light_direction_normal | (light_pos - vertex_pos) |
*/

template< int nLightType >
class CWorldLightAngleWrapper
{
public:
	FORCEINLINE static float WorldLightAngle( const LightDesc_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta )
	{
		float dot, dot2, ratio;

		switch (nLightType)	
		{
			case MATERIAL_LIGHT_POINT:
#if 1
				// half-lambert
				dot = DotProduct( snormal, delta );
				if (dot < 0.f)
					return 0.f;
#else
				dot = DotProduct( snormal, delta ) * 0.5 + 0.5;
				dot = dot * dot;
#endif
				return dot;

			case MATERIAL_LIGHT_SPOT:
#if 1
				// half-lambert
				dot = DotProduct( snormal, delta );
				if (dot < 0.)
					return 0.f;
#else
				dot = DotProduct( snormal, delta ) * 0.5 + 0.5;
				dot = dot * dot;
#endif

 				dot2 = -DotProduct (delta, lnormal);
				if (dot2 <= wl->m_PhiDot)
					return 0.f; // outside light cone

				ratio = dot;
				if (dot2 >= wl->m_ThetaDot)
					return ratio;	// inside inner cone

 				if ((wl->m_Falloff == 1.f) || (wl->m_Falloff == 0.f))
 				{
 					ratio *= (dot2 - wl->m_PhiDot) / (wl->m_ThetaDot - wl->m_PhiDot);
 				}
 				else
 				{
 					ratio *= pow((dot2 - wl->m_PhiDot) / (wl->m_ThetaDot - wl->m_PhiDot), wl->m_Falloff );
 				}
				return ratio;

			case MATERIAL_LIGHT_DIRECTIONAL:
#if 1
				// half-lambert
				dot2 = -DotProduct( snormal, lnormal );
				if (dot2 < 0.f)
					return 0.f;
#else
				dot2 = -DotProduct( snormal, lnormal ) * 0.5 + 0.5;
				dot2 = dot2 * dot2;
#endif
				return dot2;

			case MATERIAL_LIGHT_DISABLE:
				return 0.f;

			NO_DEFAULT;
		} 
	}
};

template< int nLightType >
class CWorldLightAngleWrapperConstDirectional
{
public:
	FORCEINLINE static float WorldLightAngle( const LightDesc_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta, float directionalamount )
	{
		float dot, dot2, ratio;

		// directional amount is constant
		dot = directionalamount;
		if (dot < 0.f)
			return 0.f;

		switch (nLightType)	
		{
		case MATERIAL_LIGHT_POINT:
		case MATERIAL_LIGHT_DIRECTIONAL:
			return dot;

		case MATERIAL_LIGHT_SPOT:
			dot2 = -DotProduct (delta, lnormal);
			if (dot2 <= wl->m_PhiDot)
				return 0.f; // outside light cone

			ratio = dot;
			if (dot2 >= wl->m_ThetaDot)
				return ratio;	// inside inner cone

			if ((wl->m_Falloff == 1.f) || (wl->m_Falloff == 0.f))
			{
				ratio *= (dot2 - wl->m_PhiDot) / (wl->m_ThetaDot - wl->m_PhiDot);
			}
			else
			{
				ratio *= pow((dot2 - wl->m_PhiDot) / (wl->m_ThetaDot - wl->m_PhiDot), wl->m_Falloff );
			}
			return ratio;

		case MATERIAL_LIGHT_DISABLE:
			return 0.f;

			NO_DEFAULT;
		} 
	}
};

inline float CStudioRender::R_WorldLightAngle( const LightDesc_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta )
{
	switch (wl->m_Type) 
	{
		case MATERIAL_LIGHT_DISABLE:		return CWorldLightAngleWrapper<MATERIAL_LIGHT_DISABLE>::WorldLightAngle( wl, lnormal, snormal, delta );
		case MATERIAL_LIGHT_POINT:			return CWorldLightAngleWrapper<MATERIAL_LIGHT_POINT>::WorldLightAngle( wl, lnormal, snormal, delta );
		case MATERIAL_LIGHT_DIRECTIONAL:	return CWorldLightAngleWrapper<MATERIAL_LIGHT_DIRECTIONAL>::WorldLightAngle( wl, lnormal, snormal, delta );
		case MATERIAL_LIGHT_SPOT:			return CWorldLightAngleWrapper<MATERIAL_LIGHT_SPOT>::WorldLightAngle( wl, lnormal, snormal, delta );
		NO_DEFAULT;
	}
}


//-----------------------------------------------------------------------------
// Draws eyeballs
//-----------------------------------------------------------------------------
inline void CStudioRender::R_StudioEyeballNormal( mstudioeyeball_t const* peyeball, Vector& org, 
									Vector& pos, Vector& normal )
{
	// inside of a flattened torus
	VectorSubtract( pos, org, normal );
	float flUpAmount =  DotProduct( normal, peyeball->up );
	VectorMA( normal, -0.5 * flUpAmount, peyeball->up, normal );
	VectorNormalize( normal );
}


//-----------------------------------------------------------------------------
//
// Stateless utility methods
//
//-----------------------------------------------------------------------------

// Computes the submodel for a specified body + bodypart
int R_StudioSetupModel( int nBodyPart, int nBody, mstudiomodel_t **pSubModel, const studiohdr_t *pStudioHdr );

// Computes PoseToWorld from BoneToWorld
void ComputePoseToWorld( matrix3x4_t *pPoseToWorld, studiohdr_t *pStudioHdr, int boneMask, const Vector& vecViewOrigin, const matrix3x4_t *pBoneToWorld );

// Computes the model LOD
inline int ComputeModelLODAndMetric( studiohwdata_t *pHardwareData, float flUnitSphereSize, float *pMetric )
{
	// NOTE: This function was split off since CStudioRender needs it also.
	float flMetric = pHardwareData->LODMetric( flUnitSphereSize );
	if ( pMetric )
	{
		*pMetric = flMetric;
	}
	return pHardwareData->GetLODForMetric( flMetric );
}



#endif // CSTUDIORENDER_H
