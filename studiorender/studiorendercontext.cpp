//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#include "tier0/platform.h"
#include "studiorendercontext.h"
#include "optimize.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imorph.h"
#include "materialsystem/ivballoctracker.h"
#include "vstdlib/random.h"
#include "tier0/tslist.h"
#include "tier0/platform.h"
#include "tier1/refcount.h"
#include "tier1/callqueue.h"
#include "cmodel.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// garymcthack - this should go elsewhere
#define MAX_NUM_BONE_INDICES 4


//-----------------------------------------------------------------------------
// Toggles studio queued mode
//-----------------------------------------------------------------------------
void StudioChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	// NOTE: This is necessary to flush the queued thread when this value changes
	MaterialLock_t hLock = g_pMaterialSystem->Lock();
	g_pMaterialSystem->Unlock( hLock );
}

static ConVar studio_queue_mode( "studio_queue_mode", "1", 0, "", StudioChangeCallback );


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static float s_pZeroFlexWeights[MAXSTUDIOFLEXDESC];


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
IStudioDataCache *g_pStudioDataCache = NULL;
static CStudioRenderContext s_StudioRenderContext;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CStudioRenderContext, IStudioRender, 
						STUDIO_RENDER_INTERFACE_VERSION, s_StudioRenderContext );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CStudioRenderContext::CStudioRenderContext()
{
	// Initialize render context
	m_RC.m_pForcedMaterial = NULL;
	m_RC.m_nForcedMaterialType = OVERRIDE_NORMAL;
	m_RC.m_ColorMod[0] = m_RC.m_ColorMod[1] = m_RC.m_ColorMod[2] = 1.0f;
	m_RC.m_AlphaMod = 1.0f;
	m_RC.m_ViewOrigin.Init();
	m_RC.m_ViewRight.Init();
	m_RC.m_ViewUp.Init();
	m_RC.m_ViewPlaneNormal.Init();
	m_RC.m_Config.m_bEnableHWMorph = true;
	m_RC.m_Config.m_bStatsMode = false;

	m_RC.m_NumLocalLights = 0;
	for ( int i = 0; i < 6; ++i )
	{
		m_RC.m_LightBoxColors[i].Init( 0, 0, 0 );
	}
}

CStudioRenderContext::~CStudioRenderContext()
{
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CStudioRenderContext::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pStudioDataCache = ( IStudioDataCache * )factory( STUDIO_DATA_CACHE_INTERFACE_VERSION, NULL );
	if ( !g_pMaterialSystem || !g_pMaterialSystemHardwareConfig || !g_pStudioDataCache )
	{
		Msg("StudioRender failed to connect to a required system\n" );
	}
	return ( g_pMaterialSystem && g_pMaterialSystemHardwareConfig && g_pStudioDataCache );
}

void CStudioRenderContext::Disconnect()
{
	g_pStudioDataCache = NULL;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Here's where systems can access other interfaces implemented by this object
// Returns NULL if it doesn't implement the requested interface
//-----------------------------------------------------------------------------
void *CStudioRenderContext::QueryInterface( const char *pInterfaceName )
{
	// Loading the studiorender DLL mounts *all* interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CStudioRenderContext::Init()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	if( !g_pMaterialSystem || !g_pMaterialSystemHardwareConfig )
		return INIT_FAILED;

	return g_pStudioRenderImp->Init();
}

void CStudioRenderContext::Shutdown( void )
{
	g_pStudioRenderImp->Shutdown();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Used to activate the stub material system.
//-----------------------------------------------------------------------------
void CStudioRenderContext::Mat_Stub( IMaterialSystem *pMatSys )
{
	g_pMaterialSystem = pMatSys;
}


//-----------------------------------------------------------------------------
// Determines material flags
//-----------------------------------------------------------------------------
void CStudioRenderContext::ComputeMaterialFlags( studiohdr_t *phdr, studioloddata_t &lodData, IMaterial *pMaterial )
{
	// requesting info forces the initial material precache (and its build out)
	if ( pMaterial->UsesEnvCubemap() )
	{
		phdr->flags |= STUDIOHDR_FLAGS_USES_ENV_CUBEMAP;
	}
	if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture( false ) ) // The false checks if it will ever need the frame buffer, not just this frame
	{
		phdr->flags |= STUDIOHDR_FLAGS_USES_FB_TEXTURE;
	}

	// FIXME: I'd rather know that the material is definitely using the bumpmap.
	// It could be in the file without actually being used.
	static unsigned int bumpvarCache = 0;
	IMaterialVar *pBumpMatVar = pMaterial->FindVarFast( "$bumpmap", &bumpvarCache );
	if ( pBumpMatVar && pBumpMatVar->IsDefined() && pMaterial->NeedsTangentSpace() )
	{
		phdr->flags |= STUDIOHDR_FLAGS_USES_BUMPMAPPING;
	}

	// Make sure material is treated as bump mapped if phong is set
	static unsigned int phongVarCache = 0;
	IMaterialVar *pPhongMatVar = pMaterial->FindVarFast( "$phong", &phongVarCache );
	if ( pPhongMatVar && pPhongMatVar->IsDefined() && ( pPhongMatVar->GetIntValue() != 0 ) )
	{
		phdr->flags |= STUDIOHDR_FLAGS_USES_BUMPMAPPING;
	}
}


//-----------------------------------------------------------------------------
// Does this material use a mouth shader?
//-----------------------------------------------------------------------------
static bool UsesMouthShader( IMaterial *pMaterial )
{
	// FIXME: hack, needs proper client side material system interface
	static unsigned int clientShaderCache = 0;
	IMaterialVar *clientShaderVar = pMaterial->FindVarFast( "$clientShader", &clientShaderCache );
	if ( clientShaderVar )
		return ( Q_stricmp( clientShaderVar->GetStringValue(), "MouthShader" ) == 0 );
	return false;
}


//-----------------------------------------------------------------------------
// Returns the actual texture name to use on the model
//-----------------------------------------------------------------------------
static const char *GetTextureName( studiohdr_t *phdr, OptimizedModel::FileHeader_t *pVtxHeader, 
								  int lodID, int inMaterialID )
{
	OptimizedModel::MaterialReplacementListHeader_t *materialReplacementList = 
		pVtxHeader->pMaterialReplacementList( lodID );
	int i;
	for( i = 0; i < materialReplacementList->numReplacements; i++ )
	{
		OptimizedModel::MaterialReplacementHeader_t *materialReplacement =
			materialReplacementList->pMaterialReplacement( i );
		if( materialReplacement->materialID == inMaterialID )
		{
			const char *str = materialReplacement->pMaterialReplacementName();
			return str;
		}
	}
	return phdr->pTexture( inMaterialID )->pszName();
}


//-----------------------------------------------------------------------------
// Loads materials associated with a particular LOD of a model
//-----------------------------------------------------------------------------
void CStudioRenderContext::LoadMaterials( studiohdr_t *phdr, 
	OptimizedModel::FileHeader_t *pVtxHeader, studioloddata_t &lodData, int lodID )
{
	typedef IMaterial *IMaterialPtr;
	Assert( phdr );

	lodData.numMaterials = phdr->numtextures;
	if ( lodData.numMaterials == 0 )
	{
		lodData.ppMaterials = NULL;
		return;
	}

	lodData.ppMaterials = new IMaterialPtr[lodData.numMaterials];
	Assert( lodData.ppMaterials );

	lodData.pMaterialFlags = new int[lodData.numMaterials];
	Assert( lodData.pMaterialFlags );

	int i, j;

	// get index of each material
	// set the runtime studiohdr flags that are material derived
	if ( phdr->textureindex == 0 )
		return;

	for ( i = 0; i < phdr->numtextures; i++ )
	{
		char szPath[MAX_PATH];
		IMaterial *pMaterial = NULL;

		// search through all specified directories until a valid material is found
		for ( j = 0; j < phdr->numcdtextures && IsErrorMaterial( pMaterial ); j++ )
		{
			// If we don't do this, we get filenames like "materials\\blah.vmt".
			const char *textureName = GetTextureName( phdr, pVtxHeader, lodID, i );
			if ( textureName[0] == CORRECT_PATH_SEPARATOR || textureName[0] == INCORRECT_PATH_SEPARATOR )
				++textureName;

			// This prevents filenames like /models/blah.vmt.
			const char *pCdTexture = phdr->pCdtexture( j );
			if ( pCdTexture[0] == CORRECT_PATH_SEPARATOR || pCdTexture[0] == INCORRECT_PATH_SEPARATOR )
				++pCdTexture;

			V_ComposeFileName( pCdTexture, textureName, szPath, sizeof( szPath ) );

			if ( phdr->flags & STUDIOHDR_FLAGS_OBSOLETE )
			{
				pMaterial = g_pMaterialSystem->FindMaterial( "models/obsolete/obsolete", TEXTURE_GROUP_MODEL, false );
				if ( IsErrorMaterial( pMaterial ) )
				{
					Warning( "StudioRender: OBSOLETE material missing: \"models/obsolete/obsolete\"\n" );
				}
			}
			else
			{
				pMaterial = g_pMaterialSystem->FindMaterial( szPath, TEXTURE_GROUP_MODEL, false );
			}
		}
		if ( IsErrorMaterial( pMaterial ) )
		{
			// hack - if it isn't found, go through the motions of looking for it again
			// so that the materialsystem will give an error.
			char szPrefix[256];
			Q_strncpy( szPrefix, phdr->pszName(), sizeof( szPrefix ) );
			Q_strncat( szPrefix, " : ", sizeof( szPrefix ), COPY_ALL_CHARACTERS );
			for ( j = 0; j < phdr->numcdtextures; j++ )
			{
				Q_strncpy( szPath, phdr->pCdtexture( j ), sizeof( szPath ) );
				const char *textureName = GetTextureName( phdr, pVtxHeader, lodID, i );
				Q_strncat( szPath, textureName, sizeof( szPath ), COPY_ALL_CHARACTERS );
				Q_FixSlashes( szPath, CORRECT_PATH_SEPARATOR );
				g_pMaterialSystem->FindMaterial( szPath, TEXTURE_GROUP_MODEL, true, szPrefix );
			}
		}

		lodData.ppMaterials[i] = pMaterial;
		if ( pMaterial )
		{
			// Increment the reference count for the material.
			pMaterial->IncrementReferenceCount();
			ComputeMaterialFlags( phdr, lodData, pMaterial );
			lodData.pMaterialFlags[i] = UsesMouthShader( pMaterial ) ? 1 : 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Suppresses all hw morphs on a model
//-----------------------------------------------------------------------------
static void SuppressAllHWMorphs( mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD )
{
	for ( int k = 0; k < pModel->nummeshes; ++k )
	{
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);
		for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);
			if ( ( pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED ) )
			{
				pStripGroup->flags |= OptimizedModel::STRIPGROUP_SUPPRESS_HW_MORPH;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Computes the total flexes on a model
//-----------------------------------------------------------------------------
static int ComputeTotalFlexCount( mstudiomodel_t *pModel )
{
	int nFlexCount = 0;
	for ( int k = 0; k < pModel->nummeshes; ++k )
	{
		mstudiomesh_t* pMesh = pModel->pMesh(k);
		nFlexCount += pMesh->numflexes;
	}
	return nFlexCount;
}


//-----------------------------------------------------------------------------
// Count deltas affecting a particular stripgroup
//-----------------------------------------------------------------------------
int CStudioRenderContext::CountDeltaFlexedStripGroups( mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD )
{
	int nFlexedStripGroupCount = 0;
	for ( int k = 0; k < pModel->nummeshes; ++k )
	{
		Assert( pModel->nummeshes == pVtxLOD->numMeshes );
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);
		for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);
			if ( ( pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED ) == 0 )
				continue;
			++nFlexedStripGroupCount;
		}
	}
	return nFlexedStripGroupCount;
}


//-----------------------------------------------------------------------------
// Count vertices affected by deltas in a particular strip group
//-----------------------------------------------------------------------------
int CStudioRenderContext::CountFlexedVertices( mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t* pStripGroup )
{
	if ( !pMesh->numflexes )
		return 0;

	// an inverse mapping from mesh index to strip group index
	unsigned short *pMeshIndexToGroupIndex = (unsigned short*)_alloca( pMesh->pModel()->numvertices * sizeof(unsigned short) );
	memset( pMeshIndexToGroupIndex, 0xFF, pMesh->pModel()->numvertices * sizeof(unsigned short) );
	for ( int i = 0; i < pStripGroup->numVerts; ++i )
	{
		int nMeshVert = pStripGroup->pVertex(i)->origMeshVertID;
		pMeshIndexToGroupIndex[ nMeshVert ] = (unsigned short)i;
	}

	int nFlexVertCount = 0;
	for ( int i = 0; i < pMesh->numflexes; ++i )
	{
		mstudioflex_t *pFlex = pMesh->pFlex( i );
		byte *pVAnim = pFlex->pBaseVertanim();
		int nVAnimSizeBytes = pFlex->VertAnimSizeBytes();
		for ( int j = 0; j < pFlex->numverts; ++j )
		{
			mstudiovertanim_t *pAnim = (mstudiovertanim_t*)( pVAnim + j * nVAnimSizeBytes );
			int nMeshVert = pAnim->index;
			unsigned short nGroupVert = pMeshIndexToGroupIndex[nMeshVert];

			// In this case, this vertex is not part of this meshgroup. Ignore it.
			if ( nGroupVert != 0xFFFF )
			{
				// Only count it once
				pMeshIndexToGroupIndex[nMeshVert] = 0xFFFF;
				++nFlexVertCount;
			}
		}
	}

	return nFlexVertCount;
}


//-----------------------------------------------------------------------------
// Determine if any strip groups shouldn't be morphed
//-----------------------------------------------------------------------------
static int* s_pVertexCount;
static int SortVertCount( const void *arg1, const void *arg2 )
{
	/* Compare all of both strings: */
	return s_pVertexCount[*( const int* )arg2] - s_pVertexCount[*( const int* )arg1];
}

#define MIN_HWMORPH_FLEX_COUNT 200

void CStudioRenderContext::DetermineHWMorphing( mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD )
{
	if ( !g_pMaterialSystemHardwareConfig->HasFastVertexTextures() )
		return;

	// There is fixed cost to using HW morphing in the form of setting rendertargets.
	// Therefore if there is a low chance of there being enough work, then do it in software.
	int nTotalFlexCount = ComputeTotalFlexCount( pModel );
	if ( nTotalFlexCount == 0 )
		return;

	if ( nTotalFlexCount < MIN_HWMORPH_FLEX_COUNT )
	{
		SuppressAllHWMorphs( pModel, pVtxLOD );
		return;
	}

	// If we have less meshes than the most morphs we can do in a batch, we're done.
	int nMaxHWMorphBatchCount = g_pMaterialSystemHardwareConfig->MaxHWMorphBatchCount();
	bool bHWMorph = ( pModel->nummeshes <= nMaxHWMorphBatchCount );
	if ( bHWMorph )
		return;

	// If we have less flexed strip groups than the most we can do in a batch, we're done.
	int nFlexedStripGroup = CountDeltaFlexedStripGroups( pModel, pVtxLOD );
	if ( nFlexedStripGroup <= nMaxHWMorphBatchCount )
		return;

	// Finally, the expensive method. Do HW morphing on the N most expensive strip groups

	// FIXME: We should do this at studiomdl time?
	// Certainly counting the # of flexed vertices can be done at studiomdl time.
	int *pVertexCount = (int*)_alloca( nFlexedStripGroup * sizeof(int) );
	int nCount = 0;
	for ( int k = 0; k < pModel->nummeshes; ++k )
	{
		Assert( pModel->nummeshes == pVtxLOD->numMeshes );
		mstudiomesh_t* pMesh = pModel->pMesh(k);
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);
		for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);
			if ( ( pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED ) == 0 )
				continue;

			pVertexCount[nCount++] = CountFlexedVertices( pMesh, pStripGroup );
		}
	}

	int *pSortedVertexIndices = (int*)_alloca( nFlexedStripGroup * sizeof(int) );
	for ( int i = 0; i < nFlexedStripGroup; ++i )
	{
		pSortedVertexIndices[i] = i;
	}
	s_pVertexCount = pVertexCount;
	qsort( pSortedVertexIndices, nCount, sizeof(int), SortVertCount );

	bool *pSuppressHWMorph = (bool*)_alloca( nFlexedStripGroup * sizeof(bool) ); 
	memset(	pSuppressHWMorph, 1, nFlexedStripGroup * sizeof(bool) );
	for ( int i = 0; i < nMaxHWMorphBatchCount; ++i )
	{
		pSuppressHWMorph[pSortedVertexIndices[i]] = false;
	}

	// Bleah. Pretty lame. We should change StripGroupHeader_t to store the flex vertex count
	int nIndex = 0;
	for ( int k = 0; k < pModel->nummeshes; ++k )
	{
		Assert( pModel->nummeshes == pVtxLOD->numMeshes );
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);
		for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);
			if ( ( pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED ) == 0 )
				continue;

			if ( pSuppressHWMorph[nIndex] )
			{
				pStripGroup->flags |= OptimizedModel::STRIPGROUP_SUPPRESS_HW_MORPH;
			}
			++nIndex;
		}
	}
}


//-----------------------------------------------------------------------------
// Adds a vertex to the meshbuilder.  Returns false if boneweights did not sum to 1.0
//-----------------------------------------------------------------------------
template <VertexCompressionType_t T> bool CStudioRenderContext::R_AddVertexToMesh( const char *pModelName, bool bNeedsTangentSpace, CMeshBuilder& meshBuilder, 
	OptimizedModel::Vertex_t* pVertex, mstudiomesh_t* pMesh, const mstudio_meshvertexdata_t *vertData, bool hwSkin )
{
	bool bOK = true;
	int idx = pVertex->origMeshVertID;

	mstudiovertex_t &vert = *vertData->Vertex( idx );

	// FIXME: if this ever becomes perf-critical... these writes are not in memory-ascending order,
	//        which hurts since VBs are in write-combined memory (See WriteCombineOrdering_t)
	meshBuilder.Position3fv( vert.m_vecPosition.Base() );
	meshBuilder.CompressedNormal3fv<T>( vert.m_vecNormal.Base() );
	/*
	if( vert.m_vecNormal.Length() < .9f || vert.m_vecNormal.Length() > 1.1f )
	{
	static CUtlStringMap<bool> errorMessages;
	if( !errorMessages.Defined( pModelName ) )
	{
	errorMessages[pModelName] = true;
	Warning( "MODELBUG %s: bad normal\n", pModelName );
	Warning( "\tnormal %0.1f %0.1f %0.1f pos: %0.1f %0.1f %0.1f\n", 
	vert.m_vecNormal.x, vert.m_vecNormal.y, vert.m_vecNormal.z, 
	vert.m_vecPosition.x, vert.m_vecPosition.y, vert.m_vecPosition.z );
	}
	}
	*/
	meshBuilder.TexCoord2fv( 0, vert.m_vecTexCoord.Base() );

	if (vertData->HasTangentData())
	{
		/*
		if( bNeedsTangentSpace && pModelName && vertData->TangentS( idx ) )
		{
		const Vector4D &tangentS = *vertData->TangentS( idx );
		float w = tangentS.w;
		if( !( w == 1.0f || w == -1.0f ) )
		{
		static CUtlStringMap<bool> errorMessages;
		if( !errorMessages.Defined( pModelName ) )
		{
		errorMessages[pModelName] = true;
		Warning( "MODELBUG %s: bad tangent sign\n", pModelName );
		Warning( "\tsign %0.1f at position %0.1f %0.1f %0.1f\n", 
		w, vert.m_vecPosition.x, vert.m_vecPosition.y, vert.m_vecPosition.z );
		}
		}

		float len = tangentS.AsVector3D().Length();
		if( len < .9f || len > 1.1f )
		{
		static CUtlStringMap<bool> errorMessages;
		if( !errorMessages.Defined( pModelName ) )
		{
		errorMessages[pModelName] = true;
		Warning( "MODELBUG %s: bad tangent vector\n", pModelName );
		Warning( "\ttangent: %0.1f %0.1f %0.1f with length %0.1f at position %0.1f %0.1f %0.1f\n", 
		tangentS.x, tangentS.y, tangentS.z, 
		len, 
		vert.m_vecPosition.x, vert.m_vecPosition.y, vert.m_vecPosition.z );
		}
		}

		#if 0
		float dot = DotProduct( vert.m_vecNormal, tangentS.AsVector3D() );
		if( dot > .95 || dot < -.95 )
		{
		static CUtlStringMap<bool> errorMessages;
		if( !errorMessages.Defined( pModelName ) )
		{
		errorMessages[pModelName] = true;
		// this is crashing for some reason. .need to investigate.
		Warning( "MODELBUG %s: nearly colinear tangentS (%f %f %f) and normal (%f %f %f) at position %f %f %f Probably have 2 or more texcoords that are the same on a triangle.\n", 
		pModelName, tangentS.x, tangentS.y, tangentS.y, vert.m_vecNormal.x, vert.m_vecNormal.y, vert.m_vecNormal.z, vert.m_vecPosition.x, vert.m_vecPosition.y, vert.m_vecPosition.z );
		}
		}
		#endif
		}
		*/

		// send down tangent S as a 4D userdata vect.
		meshBuilder.CompressedUserData<T>( (*vertData->TangentS( idx )).Base() );
	}

	// Just in case we get hooked to a material that wants per-vertex color
	meshBuilder.Color4ub( 255, 255, 255, 255 );

	float boneWeights[ MAX_NUM_BONE_INDICES ];
	if ( hwSkin )
	{
		// sum up weights..
		int i;

		// We have to do this because since we're potentially dropping bones
		// to get them to fit in hardware, we'll need to renormalize based on
		// the actual total.
		mstudioboneweight_t *pBoneWeight = vertData->BoneWeights(idx);

		// NOTE: We use pVertex->numbones because that's the number of bones actually influencing this
		// vertex. Note that pVertex->numBones is not necessary the *desired* # of bones influencing this
		// vertex; we could have collapsed some of those bones out. pBoneWeight->numbones stures the desired #
		float totalWeight = 0;
		for (i = 0; i < pVertex->numBones; ++i)
		{
			totalWeight += pBoneWeight->weight[pVertex->boneWeightIndex[i]];
		}

		// The only way we should not add up to 1 is if there's more than 3 *desired* bones
		// and more than 1 *actual* bone (we can have 0	vertex bones in the case of static props
		if ( (pVertex->numBones > 0) && (pBoneWeight->numbones <= 3) && fabs(totalWeight - 1.0f) > 1e-3 )
		{
			// force them to re-normalize
			bOK = false;
			totalWeight = 1.0f;
		}

		// Fix up the static prop case
		if ( totalWeight == 0.0f )
		{
			totalWeight = 1.0f;
		}

		float invTotalWeight = 1.0f / totalWeight;

		// It is essential to iterate over all actual bones so that the bone indices
		// are set correctly, even though the last bone weight is computed in a shader program
		for (i = 0; i < pVertex->numBones; ++i)
		{
			if ( pVertex->boneID[i] == -1 )
			{
				boneWeights[ i ] = 0.0f;
				meshBuilder.BoneMatrix( i, BONE_MATRIX_INDEX_INVALID );
			}
			else
			{
				float weight = pBoneWeight->weight[pVertex->boneWeightIndex[i]];
				boneWeights[ i ] = weight * invTotalWeight;
				meshBuilder.BoneMatrix( i, pVertex->boneID[i] );
			}
		}
		for( ; i < MAX_NUM_BONE_INDICES; i++ )
		{
			boneWeights[ i ] = 0.0f;
			meshBuilder.BoneMatrix( i, BONE_MATRIX_INDEX_INVALID );
		}
	}
	else
	{
		for (int i = 0; i < MAX_NUM_BONE_INDICES; ++i)
		{
			boneWeights[ i ] = (i == 0) ? 1.0f : 0.0f;
			meshBuilder.BoneMatrix( i, BONE_MATRIX_INDEX_INVALID );
		}
	}

	// Set all the weights at once (the meshbuilder performs additional, post-compression, normalization):
	Assert( pVertex->numBones <= 3 );

	if ( pVertex->numBones > 0 )
	{
		meshBuilder.CompressedBoneWeight3fv<T>( &( boneWeights[ 0 ] ) );
	}

	meshBuilder.AdvanceVertex();

	return bOK;
}

// Get (uncompressed) vertex data from a mesh, if available
inline const mstudio_meshvertexdata_t * GetFatVertexData( mstudiomesh_t * pMesh, studiohdr_t * pStudioHdr )
{
	if ( !pMesh->pModel()->CacheVertexData( pStudioHdr ) )
	{
		// not available yet
		return NULL;
	}
	const mstudio_meshvertexdata_t *pVertData = pMesh->GetVertexData( pStudioHdr );
	Assert( pVertData );
	if ( !pVertData )
	{
		static unsigned int warnCount = 0;
		if ( warnCount++ < 20 )
			Warning( "ERROR: model verts have been compressed, cannot render! (use \"-no_compressed_vvds\")" );
	}
	return pVertData;
}

//-----------------------------------------------------------------------------
// Builds the group
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioBuildMeshGroup( const char *pModelName, bool bNeedsTangentSpace, studiomeshgroup_t* pMeshGroup,
										   OptimizedModel::StripGroupHeader_t *pStripGroup, mstudiomesh_t* pMesh,
										   studiohdr_t *pStudioHdr, VertexFormat_t vertexFormat )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// We have to do this here because of skinning; there may be any number of
	// materials that are applied to this mesh.
	// Copy over all the vertices + indices in this strip group
	pMeshGroup->m_pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_MODELS );

	VertexCompressionType_t compressionType = CompressionType( vertexFormat );

	pMeshGroup->m_ColorMeshID = -1;

	bool hwSkin = (pMeshGroup->m_Flags & MESHGROUP_IS_HWSKINNED) != 0;

	// This mesh could have tristrips or trilists in it
	CMeshBuilder meshBuilder;
	meshBuilder.SetCompressionType( compressionType );
	meshBuilder.Begin( pMeshGroup->m_pMesh, MATERIAL_HETEROGENOUS, 
		hwSkin ? pStripGroup->numVerts : 0, pStripGroup->numIndices );

	int i;
	bool bBadBoneWeights = false;
	if ( hwSkin )
	{
		const mstudio_meshvertexdata_t *vertData = GetFatVertexData( pMesh, pStudioHdr );
		Assert( vertData );

		for ( i = 0; i < pStripGroup->numVerts; ++i )
		{
			bool success;
			switch ( compressionType )
			{
			case VERTEX_COMPRESSION_ON:
				success = R_AddVertexToMesh<VERTEX_COMPRESSION_ON>( pModelName, bNeedsTangentSpace, meshBuilder, pStripGroup->pVertex(i), pMesh, vertData, hwSkin );
				break;
			case VERTEX_COMPRESSION_NONE:
			default:
				success = R_AddVertexToMesh<VERTEX_COMPRESSION_NONE>( pModelName, bNeedsTangentSpace, meshBuilder, pStripGroup->pVertex(i), pMesh, vertData, hwSkin );
				break;
			}
			if ( !success )
			{
				bBadBoneWeights = true;
			}
		}
	}

	if ( bBadBoneWeights )
	{
		mstudiomodel_t* pModel = pMesh->pModel();
		ConMsg( "Bad data found in model \"%s\" (bad bone weights)\n", pModel->pszName() );
	}

	for (i = 0; i < pStripGroup->numIndices; ++i)
	{
		meshBuilder.Index( *pStripGroup->pIndex(i) );
		meshBuilder.AdvanceIndex();
	}

	meshBuilder.End();

	// Copy over the strip indices. We need access to the indices for decals
	pMeshGroup->m_pIndices = new unsigned short[ pStripGroup->numIndices ];
	memcpy( pMeshGroup->m_pIndices, pStripGroup->pIndex(0), 
		pStripGroup->numIndices * sizeof(unsigned short) );

	// Compute the number of non-degenerate trianges in each strip group
	// for statistics gathering
	pMeshGroup->m_pUniqueTris = new int[ pStripGroup->numStrips ];
	for (i = 0; i < pStripGroup->numStrips; ++i )
	{
		int numUnique = 0;
		if (pStripGroup->pStrip(i)->flags & OptimizedModel::STRIP_IS_TRISTRIP) 
		{
			int last[2] = {-1, -1};
			int curr = pStripGroup->pStrip(i)->indexOffset;
			int end = curr + pStripGroup->pStrip(i)->numIndices;
			while (curr != end)
			{
				int idx = *pStripGroup->pIndex(curr);
				if (idx != last[0] && idx != last[1] && last[0] != last[1] && last[0] != -1)
					++numUnique;
				last[0] = last[1];
				last[1] = idx;
				++curr;
			}
		}
		else
		{
			numUnique = pStripGroup->pStrip(i)->numIndices / 3;
		}
		pMeshGroup->m_pUniqueTris[i] = numUnique;
	}
}

//-----------------------------------------------------------------------------
// Builds the group
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioBuildMorph( studiohdr_t *pStudioHdr, 
	studiomeshgroup_t* pMeshGroup, mstudiomesh_t* pMesh, 
	OptimizedModel::StripGroupHeader_t *pStripGroup )
{
	if ( !g_pMaterialSystemHardwareConfig->HasFastVertexTextures() || 
		( ( pMeshGroup->m_Flags & MESHGROUP_IS_DELTA_FLEXED ) == 0 ) ||
		( ( pStripGroup->flags & OptimizedModel::STRIPGROUP_SUPPRESS_HW_MORPH ) != 0 ) )
	{
		pMeshGroup->m_pMorph = NULL;
		return;
	}

	// Build an inverse mapping from mesh index to strip group index
	unsigned short *pMeshIndexToGroupIndex = (unsigned short*)_alloca( pMesh->pModel()->numvertices * sizeof(unsigned short) );
	memset( pMeshIndexToGroupIndex, 0xFF, pMesh->pModel()->numvertices * sizeof(unsigned short) );
	for ( int i = 0; i < pStripGroup->numVerts; ++i )
	{
		int nMeshVert = pStripGroup->pVertex(i)->origMeshVertID;
		pMeshIndexToGroupIndex[ nMeshVert ] = (unsigned short)i;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	MorphFormat_t morphType = MORPH_POSITION | MORPH_NORMAL | MORPH_SPEED | MORPH_SIDE; 
	for ( int i = 0; i < pMesh->numflexes; ++i )
	{
		if ( pMesh->pFlex( i )->vertanimtype == STUDIO_VERT_ANIM_WRINKLE )
		{
			morphType |= MORPH_WRINKLE;
			break;
		}
	}

	char pTemp[256];
	Q_snprintf( pTemp, sizeof(pTemp), "%s [%p]", pStudioHdr->pszName(), pMeshGroup );
	pMeshGroup->m_pMorph = pRenderContext->CreateMorph( morphType, pTemp );

	const float flVertAnimFixedPointScale = pStudioHdr->VertAnimFixedPointScale();

	CMorphBuilder morphBuilder;
	morphBuilder.Begin( pMeshGroup->m_pMorph, 1.0f / flVertAnimFixedPointScale ); 

	for ( int i = 0; i < pMesh->numflexes; ++i )
	{
		mstudioflex_t *pFlex = pMesh->pFlex( i );
		byte *pVAnim = pFlex->pBaseVertanim();
		int nVAnimSizeBytes = pFlex->VertAnimSizeBytes();
		for ( int j = 0; j < pFlex->numverts; ++j )
		{
			mstudiovertanim_t *pAnim = (mstudiovertanim_t*)( pVAnim + j * nVAnimSizeBytes );
			int nMeshVert = pAnim->index;
			unsigned short nGroupVert = pMeshIndexToGroupIndex[nMeshVert];

			// In this case, this vertex is not part of this meshgroup. Ignore it.
			if ( nGroupVert == 0xFFFF )
				continue;

			morphBuilder.PositionDelta3( pAnim->GetDeltaFixed( flVertAnimFixedPointScale ) ); 
			morphBuilder.NormalDelta3( pAnim->GetNDeltaFixed( flVertAnimFixedPointScale ) ); 
			morphBuilder.Speed1f( pAnim->speed / 255.0f );
			morphBuilder.Side1f( pAnim->side / 255.0f );
			if ( pFlex->vertanimtype == STUDIO_VERT_ANIM_WRINKLE )
			{
				mstudiovertanim_wrinkle_t *pWrinkleAnim = static_cast<mstudiovertanim_wrinkle_t*>( pAnim );
				morphBuilder.WrinkleDelta1f( pWrinkleAnim->GetWrinkleDeltaFixed( flVertAnimFixedPointScale ) ); 
			}
			else
			{
				morphBuilder.WrinkleDelta1f( 0.0f ); 
			}

			morphBuilder.AdvanceMorph( nGroupVert, i );
		}
	}

	morphBuilder.End();
}


//-----------------------------------------------------------------------------
// Builds the strip data
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioBuildMeshStrips( studiomeshgroup_t* pMeshGroup,
											OptimizedModel::StripGroupHeader_t *pStripGroup )
{
	// FIXME: This is bogus
	// Compute the amount of memory we need to store the strip data
	int i;
	int stripDataSize = 0;
	for( i = 0; i < pStripGroup->numStrips; ++i )
	{
		stripDataSize += sizeof(OptimizedModel::StripHeader_t);
		stripDataSize += pStripGroup->pStrip(i)->numBoneStateChanges *
			sizeof(OptimizedModel::BoneStateChangeHeader_t);
	}

	pMeshGroup->m_pStripData = (OptimizedModel::StripHeader_t*)malloc(stripDataSize);

	// Copy over the strip info
	int boneStateChangeOffset = pStripGroup->numStrips * sizeof(OptimizedModel::StripHeader_t);
	for( i = 0; i < pStripGroup->numStrips; ++i )
	{
		memcpy( &pMeshGroup->m_pStripData[i], pStripGroup->pStrip(i),
			sizeof( OptimizedModel::StripHeader_t ) );

		// Fixup the bone state change offset, since we have it right after the strip data
		pMeshGroup->m_pStripData[i].boneStateChangeOffset = boneStateChangeOffset -
			i * sizeof(OptimizedModel::StripHeader_t);

		// copy over bone state changes
		int boneWeightSize = pMeshGroup->m_pStripData[i].numBoneStateChanges * 
			sizeof(OptimizedModel::BoneStateChangeHeader_t);

		if (boneWeightSize != 0)
		{
			unsigned char* pBoneStateChange = (unsigned char*)pMeshGroup->m_pStripData + boneStateChangeOffset;
			memcpy( pBoneStateChange, pStripGroup->pStrip(i)->pBoneStateChange(0), boneWeightSize);

			boneStateChangeOffset += boneWeightSize;
		}
	}
	pMeshGroup->m_NumStrips = pStripGroup->numStrips;
}


//-----------------------------------------------------------------------------
// Determine the max. number of bone weights used by a stripgroup
//-----------------------------------------------------------------------------
int CStudioRenderContext::GetNumBoneWeights( const OptimizedModel::StripGroupHeader_t *pGroup )
{
	int nBoneWeightsMax = 0;

	for (int i = 0;i < pGroup->numStrips; i++)
	{
		OptimizedModel::StripHeader_t * pStrip = pGroup->pStrip( i );
		nBoneWeightsMax = max( nBoneWeightsMax, (int)pStrip->numBones );
	}

	return nBoneWeightsMax;
}

//-----------------------------------------------------------------------------
// Determine an actual model vertex format for a mesh based on its material usage.
// Bypasses the homegenous model vertex format in favor of the actual format.
// Ideally matches 1:1 the shader's data requirements without any bloat.
//-----------------------------------------------------------------------------
VertexFormat_t CStudioRenderContext::CalculateVertexFormat( const studiohdr_t *pStudioHdr, const studioloddata_t *pStudioLodData,
													 const mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t *pGroup, bool bIsHwSkinned )
{
	bool bSkinnedMesh = ( pStudioHdr->numbones > 1 );
	int  nBoneWeights = GetNumBoneWeights( pGroup );

	bool bIsDX7 = !g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders();
	bool bIsDX8 = ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 );
	if ( bIsDX7 )
	{
		// FIXME: this is untested (as of June '07, the engine currently doesn't work with "-dxlevel 70")
		if ( bSkinnedMesh )
			return MATERIAL_VERTEX_FORMAT_MODEL_SKINNED_DX7;
		else
			return MATERIAL_VERTEX_FORMAT_MODEL_DX7;
	}
	else if ( bIsDX8 )
	{
		if ( bSkinnedMesh )
			return MATERIAL_VERTEX_FORMAT_MODEL_SKINNED;
		else
			return MATERIAL_VERTEX_FORMAT_MODEL;
	}
	else
	{
		// DX9+ path (supports vertex compression)

		// iterate each skin table
		// determine aggregate vertex format for specified mesh's material
		VertexFormat_t newVertexFormat = 0;
		//bool bBumpmapping = false;
		short *pSkinref	= pStudioHdr->pSkinref( 0 );
		for ( int i = 0; i < pStudioHdr->numskinfamilies; i++ )
		{
			// FIXME: ### MATERIAL VERTEX FORMATS ARE UNRELIABLE! ###
			//
			//	IMaterial* pMaterial = pStudioLodData->ppMaterials[ pSkinref[ pMesh->material ] ];
			//	Assert( pMaterial );
			//	VertexFormat_t vertexFormat = pMaterial->GetVertexFormat();
			//	newVertexFormat &= ~VERTEX_FORMAT_COMPRESSED; // Decide whether to compress below
			//
			// FIXME: ### MATERIAL VERTEX FORMATS ARE UNRELIABLE! ###
			//        we need to go through all the shader CPP code and make sure that the correct vertex format
			//        is being specified for every single shader combo! We don't have time to fix that before
			//        shipping Ep2, but should fix it ASAP afterwards. To make catching such errors easier, we
			//        should Assert in draw calls that the vertexdecl matches vertex shader inputs (note that D3D
			//        debug DLLs will do that on PC, though it's not as informative as if we do it ourselves).
			//        So, in the absence of reliable material vertex formats, use the old 'standard' elements
			//        (we can still omit skinning data - and COLOR for DX8+, where it should come from the
			//        second static lighting stream):
			VertexFormat_t vertexFormat = bIsDX7 ? MATERIAL_VERTEX_FORMAT_MODEL_DX7 : ( MATERIAL_VERTEX_FORMAT_MODEL & ~VERTEX_COLOR );

			// aggregate single bit settings
			newVertexFormat |= vertexFormat & ( ( 1 << VERTEX_LAST_BIT ) - 1 );
			
			int nUserDataSize = UserDataSize( vertexFormat );
			if ( nUserDataSize > UserDataSize( newVertexFormat ) )
			{
				newVertexFormat &= ~USER_DATA_SIZE_MASK;
				newVertexFormat |= VERTEX_USERDATA_SIZE( nUserDataSize );
			}

			for (int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j)
			{
				int nSize = TexCoordSize( j, vertexFormat );
				if ( nSize > TexCoordSize( j, newVertexFormat ) )
				{
					newVertexFormat &= ~VERTEX_TEXCOORD_SIZE( j, 0x7 );
					newVertexFormat |= VERTEX_TEXCOORD_SIZE( j, nSize );
				}
			}

			// FIXME: re-enable this test, fix it to work and see how much memory we save (Q: why is this different to CStudioRenderContext::MeshNeedsTangentSpace ?)
			/*if ( !bBumpmapping && pMaterial->NeedsTangentSpace() )
			{
				bool bFound = false;
				IMaterialVar *pEnvmapMatVar = pMaterial->FindVar( "$envmap", &bFound, false );
				if ( bFound && pEnvmapMatVar->IsDefined() )
				{
					IMaterialVar *pBumpMatVar = pMaterial->FindVar( "$bumpmap", &bFound, false );
					if ( bFound && pBumpMatVar->IsDefined() )
					{
						bBumpmapping = true;
					}
				}
			} */

			pSkinref += pStudioHdr->numskinref;
		}

		// Add skinning elements for non-rigid models (with more than one bone weight)
		if ( bSkinnedMesh )
		{
			if ( nBoneWeights > 0 )
			{
				// Always exactly zero or two weights
				newVertexFormat |= VERTEX_BONEWEIGHT( 2 );
			}
			newVertexFormat |= VERTEX_BONE_INDEX;
		}


		// FIXME: re-enable this (see above)
		/*if ( !bBumpmapping )
		{
			// no bumpmapping, user data not needed
			newVertexFormat &= ~USER_DATA_SIZE_MASK;
		}*/

		// materials on models should never have tangent space as they use userdata
		Assert( !(newVertexFormat & VERTEX_TANGENT_SPACE) );

		// Don't compress the mesh unless it is HW-skinned (we only want to compress static
		// VBs, not dynamic ones - that would slow down the MeshBuilder in dynamic use cases).
		// Also inspect the vertex data to see if it's appropriate for the vertex element
		// compression techniques that we do (e.g. look at UV ranges).
		if ( //IsX360() && // Disabled until the craziness is banished
			 bIsHwSkinned &&
			( g_pMaterialSystemHardwareConfig->SupportsCompressedVertices() == VERTEX_COMPRESSION_ON ) )
		{
			// this mesh is appropriate for vertex compression
			newVertexFormat |= VERTEX_FORMAT_COMPRESSED;
		}

		return newVertexFormat;
	}
}

bool CStudioRenderContext::MeshNeedsTangentSpace( studiohdr_t *pStudioHdr, studioloddata_t *pStudioLodData, mstudiomesh_t* pMesh )
{
	// iterate each skin table
	if( !pStudioHdr || !pStudioHdr->pSkinref( 0 ) || !pStudioHdr->numskinfamilies )
	{
		return false;
	}
	short *pSkinref	= pStudioHdr->pSkinref( 0 );
	for ( int i=0; i<pStudioHdr->numskinfamilies; i++)
	{
		IMaterial* pMaterial = pStudioLodData->ppMaterials[pSkinref[pMesh->material]];
		Assert( pMaterial );
		if( !pMaterial )
		{
			continue;
		}

		//		Warning( "*****%s needstangentspace: %d\n", pMaterial->GetName(), pMaterial->NeedsTangentSpace() ? 1 : 0 );
		if( pMaterial->NeedsTangentSpace() )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Creates a single mesh
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioCreateSingleMesh( studiohdr_t *pStudioHdr, studioloddata_t *pStudioLodData,
											 mstudiomesh_t* pMesh, OptimizedModel::MeshHeader_t* pVtxMesh, int numBones, 
											 studiomeshdata_t* pMeshData, int *pColorMeshID )
{
	// Here are the cases where we don't use any meshes at all...
	// In the case of eyes, we're just gonna use dynamic buffers
	// because it's the fastest solution (prevents lots of locks)

	bool bNeedsTangentSpace = MeshNeedsTangentSpace( pStudioHdr, pStudioLodData, pMesh );

	// Each strip group represents a locking group, it's a set of vertices
	// that are locked together, and, potentially, software light + skinned together
	pMeshData->m_NumGroup = pVtxMesh->numStripGroups;
	pMeshData->m_pMeshGroup = new studiomeshgroup_t[pVtxMesh->numStripGroups];

	for (int i = 0; i < pVtxMesh->numStripGroups; ++i )
	{
		OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(i);
		studiomeshgroup_t* pMeshGroup = &pMeshData->m_pMeshGroup[i];

		pMeshGroup->m_MeshNeedsRestore = false;

		// Set the flags...
		pMeshGroup->m_Flags = 0;
		if (pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_FLEXED)
		{
			pMeshGroup->m_Flags |= MESHGROUP_IS_FLEXED;
		}

		if (pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED)
		{
			pMeshGroup->m_Flags |= MESHGROUP_IS_DELTA_FLEXED;
		}

		bool bIsHwSkinned = !!(pStripGroup->flags & OptimizedModel::STRIPGROUP_IS_HWSKINNED);
		if ( bIsHwSkinned )
		{
			pMeshGroup->m_Flags |= MESHGROUP_IS_HWSKINNED;
		}

		// get the minimal vertex format for this mesh
		VertexFormat_t vertexFormat = CalculateVertexFormat( pStudioHdr, pStudioLodData, pMesh, pStripGroup, bIsHwSkinned );

		// Build the vertex + index buffers
		R_StudioBuildMeshGroup( pStudioHdr->pszName(), bNeedsTangentSpace, pMeshGroup, pStripGroup, pMesh, pStudioHdr, vertexFormat );

		// Copy over the tristrip and triangle list data
		R_StudioBuildMeshStrips( pMeshGroup, pStripGroup );

		// Builds morph targets
		R_StudioBuildMorph( pStudioHdr, pMeshGroup, pMesh, pStripGroup );

		// Build the mapping from strip group vertex idx to actual mesh idx
		pMeshGroup->m_pGroupIndexToMeshIndex = new unsigned short[pStripGroup->numVerts + PREFETCH_VERT_COUNT];
		pMeshGroup->m_NumVertices = pStripGroup->numVerts;

		int j;
		for ( j = 0; j < pStripGroup->numVerts; ++j )
		{
			pMeshGroup->m_pGroupIndexToMeshIndex[j] = pStripGroup->pVertex(j)->origMeshVertID;
		}

		// Extra copies are for precaching...
		for ( j = pStripGroup->numVerts; j < pStripGroup->numVerts + PREFETCH_VERT_COUNT; ++j )
		{
			pMeshGroup->m_pGroupIndexToMeshIndex[j] = pMeshGroup->m_pGroupIndexToMeshIndex[pStripGroup->numVerts - 1];
		}

		// assign the possibly used color mesh id now
		pMeshGroup->m_ColorMeshID = (*pColorMeshID)++;
	}
}


//-----------------------------------------------------------------------------
// Creates static meshes
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioCreateStaticMeshes( studiohdr_t *pStudioHdr, 
	OptimizedModel::FileHeader_t *pVtxHdr, studiohwdata_t *pStudioHWData, int nLodID, int *pColorMeshID )
{
	int i, j, k;

	Assert( pStudioHdr && pVtxHdr && pStudioHWData );

	pStudioHWData->m_pLODs[nLodID].m_pMeshData = new studiomeshdata_t[pStudioHWData->m_NumStudioMeshes];

	// Iterate over every body part...
	for ( i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart(i);
		OptimizedModel::BodyPartHeader_t* pVtxBodyPart = pVtxHdr->pBodyPart(i);

		// Iterate over every submodel...
		for ( j = 0; j < pBodyPart->nummodels; ++j )
		{
			mstudiomodel_t* pModel = pBodyPart->pModel(j);
			OptimizedModel::ModelHeader_t* pVtxModel = pVtxBodyPart->pModel(j);
			OptimizedModel::ModelLODHeader_t *pVtxLOD = pVtxModel->pLOD( nLodID );

			// Determine which meshes should be hw morphed
			DetermineHWMorphing( pModel, pVtxLOD );

			// Support tracking of VB allocations
			// FIXME: categorise studiomodel allocs more precisely
			if ( g_VBAllocTracker )
			{
				if ( ( pStudioHdr->numbones > 8 ) || ( pStudioHdr->numflexdesc > 0 ) )
				{
					g_VBAllocTracker->TrackMeshAllocations( "R_StudioCreateStaticMeshes (character)" );
				}
				else
				{
					if ( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP )
					{
						g_VBAllocTracker->TrackMeshAllocations( "R_StudioCreateStaticMeshes (prop_static)" );
					}
					else
					{
						g_VBAllocTracker->TrackMeshAllocations( "R_StudioCreateStaticMeshes (prop_dynamic)" );
					}
				}
			}

			// Iterate over all the meshes....
			for ( k = 0; k < pModel->nummeshes; ++k )
			{
				Assert( pModel->nummeshes == pVtxLOD->numMeshes );
				mstudiomesh_t* pMesh = pModel->pMesh(k);
				OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);

				Assert( pMesh->meshid < pStudioHWData->m_NumStudioMeshes );
				R_StudioCreateSingleMesh( pStudioHdr, &pStudioHWData->m_pLODs[nLodID],
					pMesh, pVtxMesh, pVtxHdr->maxBonesPerVert, 
					&pStudioHWData->m_pLODs[nLodID].m_pMeshData[pMesh->meshid], pColorMeshID );
			}

			if ( g_VBAllocTracker )
			{
				g_VBAllocTracker->TrackMeshAllocations( NULL );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Destroys static meshes
//-----------------------------------------------------------------------------
void CStudioRenderContext::R_StudioDestroyStaticMeshes( int numStudioMeshes, studiomeshdata_t **ppStudioMeshes )
{
	if( !*ppStudioMeshes)
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Iterate over every body mesh...
	for ( int i = 0; i < numStudioMeshes; ++i )
	{
		studiomeshdata_t* pMesh = &((*ppStudioMeshes)[i]);

		for (int j = 0; j < pMesh->m_NumGroup; ++j)
		{
			studiomeshgroup_t* pGroup = &pMesh->m_pMeshGroup[j];
			if (pGroup->m_pGroupIndexToMeshIndex)
			{
				delete[] pGroup->m_pGroupIndexToMeshIndex;
				pGroup->m_pGroupIndexToMeshIndex = 0;
			}

			if (pGroup->m_pUniqueTris)
			{
				delete [] pGroup->m_pUniqueTris;
				pGroup->m_pUniqueTris = 0;
			}

			if (pGroup->m_pIndices)
			{
				delete [] pGroup->m_pIndices;
				pGroup->m_pIndices = 0;
			}

			if (pGroup->m_pMesh)
			{
				pRenderContext->DestroyStaticMesh( pGroup->m_pMesh );
				pGroup->m_pMesh = 0;
			}

			if (pGroup->m_pMorph)
			{
				pRenderContext->DestroyMorph( pGroup->m_pMorph );
				pGroup->m_pMorph = 0;
			}

			if (pGroup->m_pStripData)
			{
				free( pGroup->m_pStripData );
				pGroup->m_pStripData = 0;
			}
		}

		if (pMesh->m_pMeshGroup)
		{
			delete[] pMesh->m_pMeshGroup;
			pMesh->m_pMeshGroup = 0;
		}
	}

	if ( *ppStudioMeshes )
	{
		delete 	*ppStudioMeshes;
		*ppStudioMeshes = 0;
	}
}


//-----------------------------------------------------------------------------
// Builds the decal bone remap for a particular mesh
//-----------------------------------------------------------------------------
void CStudioRenderContext::BuildDecalBoneMap( studiohdr_t *pStudioHdr, int *pUsedBones, int *pBoneRemap, int *pMaxBoneCount, mstudiomesh_t* pMesh, OptimizedModel::StripGroupHeader_t* pStripGroup )
{
	const mstudio_meshvertexdata_t *pVertData = GetFatVertexData( pMesh, pStudioHdr );
	Assert( pVertData );
	for ( int i = 0; i < pStripGroup->numVerts; ++i )
	{
		int nMeshVert = pStripGroup->pVertex( i )->origMeshVertID;
		mstudioboneweight_t &boneWeight = pVertData->Vertex( nMeshVert )->m_BoneWeights;
		int nBoneCount = boneWeight.numbones;
		for ( int j = 0; j < nBoneCount; ++j )
		{
			if ( boneWeight.weight[j] == 0.0f )
				continue;

			if ( pBoneRemap[ (unsigned)boneWeight.bone[j] ] >= 0 )
				continue;

			pBoneRemap[ (unsigned)boneWeight.bone[j] ] = *pUsedBones;
			*pUsedBones = *pUsedBones + 1;
		}
	}

	for ( int i = 0; i < pStripGroup->numStrips; ++i )
	{
		if ( pStripGroup->pStrip(i)->numBones > *pMaxBoneCount )
		{
			*pMaxBoneCount = pStripGroup->pStrip(i)->numBones;
		}
	}
}


//-----------------------------------------------------------------------------
// For decals on hardware morphing, we must actually do hardware skinning
// because the flex must occur before skinning.
// For this to work, we have to hope that the total # of bones used by
// hw flexed verts is < than the max possible for the dx level we're running under
//-----------------------------------------------------------------------------
void CStudioRenderContext::ComputeHWMorphDecalBoneRemap( studiohdr_t *pStudioHdr, OptimizedModel::FileHeader_t *pVtxHdr, studiohwdata_t *pStudioHWData, int nLOD )
{
	if ( pStudioHdr->numbones == 0 )
		return;

	// Remaps sw bones to hw bones during decal rendering
	// NOTE: Only bones affecting vertices which have hw flexes will be add to this map.
	int nBufSize = pStudioHdr->numbones * sizeof(int);
	int *pBoneRemap = (int*)_alloca( nBufSize );
	memset( pBoneRemap, 0xFF, nBufSize );
	int nMaxBoneCount = 0;

	// NOTE: HW bone index 0 is always the identity transform during decals.
	pBoneRemap[0] = 0;	// necessary for unused bones in a vertex
	int nUsedBones = 1;

	studioloddata_t *pStudioLOD	= &pStudioHWData->m_pLODs[nLOD];
	for ( int i = 0; i < pStudioHdr->numbodyparts; ++i )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart(i);
		OptimizedModel::BodyPartHeader_t* pVtxBodyPart = pVtxHdr->pBodyPart(i);

		// Iterate over every submodel...
		for ( int j = 0; j < pBodyPart->nummodels; ++j )
		{
			mstudiomodel_t* pModel = pBodyPart->pModel(j);
			OptimizedModel::ModelHeader_t* pVtxModel = pVtxBodyPart->pModel(j);
			OptimizedModel::ModelLODHeader_t *pVtxLOD = pVtxModel->pLOD( nLOD );

			// Iterate over all the meshes....
			for ( int k = 0; k < pModel->nummeshes; ++k )
			{
				Assert( pModel->nummeshes == pVtxLOD->numMeshes );
				mstudiomesh_t* pMesh = pModel->pMesh(k);
				OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);

				studiomeshdata_t* pMeshData = &pStudioLOD->m_pMeshData[pMesh->meshid];
				for ( int l = 0; l < pVtxMesh->numStripGroups; ++l )
				{
					studiomeshgroup_t* pMeshGroup = &pMeshData->m_pMeshGroup[l];
					if ( !pMeshGroup->m_pMorph )
						continue;

					OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(l);
					BuildDecalBoneMap( pStudioHdr, &nUsedBones, pBoneRemap, &nMaxBoneCount, pMesh, pStripGroup );
				}
			}
		}
	}

	if ( nUsedBones > 1 )
	{
		if ( nUsedBones > g_pMaterialSystemHardwareConfig->MaxVertexShaderBlendMatrices() )
		{
			Warning( "Hardware morphing of decals will be busted! Too many unique bones on flexed vertices!\n" );
		}

		pStudioLOD->m_pHWMorphDecalBoneRemap = new int[ pStudioHdr->numbones ];
		memcpy( pStudioLOD->m_pHWMorphDecalBoneRemap, pBoneRemap, nBufSize ); 
		pStudioLOD->m_nDecalBoneCount = nMaxBoneCount;
	}
}


//-----------------------------------------------------------------------------
// Hook needed by mdlcache to load the vertex data
//-----------------------------------------------------------------------------
const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void *pModelData )
{
	// make requested data resident
	return g_pStudioDataCache->CacheVertexData( (studiohdr_t *)pModelData );
}


//-----------------------------------------------------------------------------
// Loads, unloads models
//-----------------------------------------------------------------------------
bool CStudioRenderContext::LoadModel( studiohdr_t *pStudioHdr, void *pVtxBuffer, studiohwdata_t *pStudioHWData )
{
	int	i;
	int	j;

	Assert( pStudioHdr );
	Assert( pVtxBuffer );
	Assert( pStudioHWData );

	if ( !pStudioHdr || !pVtxBuffer || !pStudioHWData )
		return false;

	// NOTE: This must be called *after* Mod_LoadStudioModel
	OptimizedModel::FileHeader_t* pVertexHdr = (OptimizedModel::FileHeader_t*)pVtxBuffer; 

	if ( pVertexHdr->checkSum != pStudioHdr->checksum )
	{								  
		ConDMsg("Error! Model %s .vtx file out of synch with .mdl\n", pStudioHdr->pszName() );
		return false;
	}

	pStudioHWData->m_NumStudioMeshes = 0;
	for ( i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart(i);
		for (j = 0; j < pBodyPart->nummodels; j++)
		{
			pStudioHWData->m_NumStudioMeshes += pBodyPart->pModel(j)->nummeshes;
		}
	}

	// Create static meshes
	Assert( pVertexHdr->numLODs );
	pStudioHWData->m_RootLOD = min( (int)pStudioHdr->rootLOD, pVertexHdr->numLODs-1 );
	pStudioHWData->m_NumLODs = pVertexHdr->numLODs;
	pStudioHWData->m_pLODs   = new studioloddata_t[pVertexHdr->numLODs];
	memset( pStudioHWData->m_pLODs, 0, pVertexHdr->numLODs * sizeof( studioloddata_t ));

	// reset the runtime flags
	pStudioHdr->flags &= ~STUDIOHDR_FLAGS_USES_ENV_CUBEMAP;
	pStudioHdr->flags &= ~STUDIOHDR_FLAGS_USES_FB_TEXTURE;
	pStudioHdr->flags &= ~STUDIOHDR_FLAGS_USES_BUMPMAPPING;

#ifdef _DEBUG
	int totalNumMeshGroups = 0;
#endif
	int nColorMeshID = 0;
	int nLodID;
	for ( nLodID = pStudioHWData->m_RootLOD; nLodID < pStudioHWData->m_NumLODs; nLodID++ )
	{
		// Load materials and determine material dependent mesh requirements
		LoadMaterials( pStudioHdr, pVertexHdr, pStudioHWData->m_pLODs[nLodID], nLodID );

		// build the meshes
		R_StudioCreateStaticMeshes( pStudioHdr, pVertexHdr, pStudioHWData, nLodID, &nColorMeshID );

		// Build the hardware bone remap for decal rendering using HW morphing
		ComputeHWMorphDecalBoneRemap( pStudioHdr, pVertexHdr, pStudioHWData, nLodID );

		// garymcthack - need to check for NULL here.
		// save off the lod switch point
		pStudioHWData->m_pLODs[nLodID].m_SwitchPoint = pVertexHdr->pBodyPart( 0 )->pModel( 0 )->pLOD( nLodID )->switchPoint;

#ifdef _DEBUG
		studioloddata_t *pLOD = &pStudioHWData->m_pLODs[nLodID];
		for ( int meshID = 0; meshID < pStudioHWData->m_NumStudioMeshes; ++meshID )
		{
			totalNumMeshGroups += pLOD->m_pMeshData[meshID].m_NumGroup;
		}
#endif
	}

#ifdef _DEBUG
	Assert( nColorMeshID == totalNumMeshGroups );
#endif

	return true;
}


void CStudioRenderContext::UnloadModel( studiohwdata_t *pHardwareData )
{
	int i;
	for ( i = pHardwareData->m_RootLOD; i < pHardwareData->m_NumLODs; i++ )
	{
		int j;
		for ( j = 0; j < pHardwareData->m_pLODs[i].numMaterials; j++ )
		{
			if ( pHardwareData->m_pLODs[i].ppMaterials[j] )
			{
				pHardwareData->m_pLODs[i].ppMaterials[j]->DecrementReferenceCount();
			}
		}
		delete [] pHardwareData->m_pLODs[i].ppMaterials;
		delete [] pHardwareData->m_pLODs[i].pMaterialFlags;
		pHardwareData->m_pLODs[i].ppMaterials = NULL;
		pHardwareData->m_pLODs[i].pMaterialFlags = NULL;
	}
	for ( i = pHardwareData->m_RootLOD; i < pHardwareData->m_NumLODs; i++ )
	{
		R_StudioDestroyStaticMeshes( pHardwareData->m_NumStudioMeshes, &pHardwareData->m_pLODs[i].m_pMeshData );
	}
	delete[] pHardwareData->m_pLODs;
	pHardwareData->m_pLODs = NULL;
}


//-----------------------------------------------------------------------------
// Refresh the studiohdr since it was lost...
//-----------------------------------------------------------------------------
void CStudioRenderContext::RefreshStudioHdr( studiohdr_t* pStudioHdr, studiohwdata_t* pHardwareData )
{
}

//-----------------------------------------------------------------------------
// Set the eye view target
//-----------------------------------------------------------------------------
void CStudioRenderContext::SetEyeViewTarget( const studiohdr_t *pStudioHdr, int nBodyIndex, const Vector& viewtarget )
{
	VectorCopy( viewtarget, m_RC.m_ViewTarget );
}


//-----------------------------------------------------------------------------
// Returns information about the ambient light samples
//-----------------------------------------------------------------------------
static TableVector s_pAmbientLightDir[6] = 
{
	{  1,  0,  0 }, 
	{ -1,  0,  0 },
	{  0,  1,  0 }, 
	{  0, -1,  0 }, 
	{  0,  0,  1 }, 
	{  0,  0, -1 }, 
};

int CStudioRenderContext::GetNumAmbientLightSamples()
{
	return 6;
}

const Vector *CStudioRenderContext::GetAmbientLightDirections()
{
	return (const Vector*)s_pAmbientLightDir;
}


//-----------------------------------------------------------------------------
// Methods related to LOD
//-----------------------------------------------------------------------------
int CStudioRenderContext::GetNumLODs( const studiohwdata_t &hardwareData ) const
{
	return hardwareData.m_NumLODs;
}

float CStudioRenderContext::GetLODSwitchValue( const studiohwdata_t &hardwareData, int nLOD ) const
{
	return hardwareData.m_pLODs[nLOD].m_SwitchPoint;
}

void CStudioRenderContext::SetLODSwitchValue( studiohwdata_t &hardwareData, int nLOD, float flSwitchValue )
{
	// NOTE: This must block the hardware thread since it reads this data.
	// This method is only used in tools, though.
	MaterialLock_t hLock = g_pMaterialSystem->Lock();
	hardwareData.m_pLODs[nLOD].m_SwitchPoint = flSwitchValue;
	g_pMaterialSystem->Unlock( hLock );
}


//-----------------------------------------------------------------------------
// Returns the first n materials. The studiohdr material list is the superset
// for all lods.
//-----------------------------------------------------------------------------
int CStudioRenderContext::GetMaterialList( studiohdr_t *pStudioHdr, int count, IMaterial** ppMaterials )
{
	AssertMsg( pStudioHdr, "Don't ignore this assert! CStudioRenderContext::GetMaterialList() has null pStudioHdr." );

	if ( !pStudioHdr )
		return 0;

	if ( pStudioHdr->textureindex == 0 )
		return 0;

	// iterate each texture
	int	i;
	int	j;
	int found = 0;
	for ( i = 0; i < pStudioHdr->numtextures; i++ )
	{
		char szPath[MAX_PATH];
		IMaterial *pMaterial = NULL;

		// iterate quietly through all specified directories until a valid material is found
		for ( j = 0; j < pStudioHdr->numcdtextures && IsErrorMaterial( pMaterial ); j++ )
		{
			// If we don't do this, we get filenames like "materials\\blah.vmt".
			const char *textureName = pStudioHdr->pTexture( i )->pszName();
			if ( textureName[0] == CORRECT_PATH_SEPARATOR || textureName[0] == INCORRECT_PATH_SEPARATOR )
				++textureName;

			// This prevents filenames like /models/blah.vmt.
			const char *pCdTexture = pStudioHdr->pCdtexture( j );
			if ( pCdTexture[0] == CORRECT_PATH_SEPARATOR || pCdTexture[0] == INCORRECT_PATH_SEPARATOR )
				++pCdTexture;

			V_ComposeFileName( pCdTexture, textureName, szPath, sizeof( szPath ) );

			if ( pStudioHdr->flags & STUDIOHDR_FLAGS_OBSOLETE )
			{
				pMaterial = g_pMaterialSystem->FindMaterialEx( "models/obsolete/obsolete", TEXTURE_GROUP_MODEL, MATERIAL_FINDCONTEXT_ISONAMODEL, false );
			}
			else
			{
				pMaterial = g_pMaterialSystem->FindMaterialEx( szPath, TEXTURE_GROUP_MODEL, MATERIAL_FINDCONTEXT_ISONAMODEL, false );
			}
		}

		if ( !pMaterial )
			continue;

		if ( found < count )
		{
			int k;
			for ( k=0; k<found; k++ )
			{
				if ( ppMaterials[k] == pMaterial )
					break;
			}
			if ( k >= found )
			{
				// add uniquely
				ppMaterials[found++] = pMaterial;
			}
		}
		else
		{
			break;
		}
	}

	return found;
}


int CStudioRenderContext::GetMaterialListFromBodyAndSkin( MDLHandle_t studio, int nSkin, int nBody, int nCountOutputMaterials, IMaterial** ppOutputMaterials )
{
	int found = 0;

	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( studio );
	if ( pStudioHWData == NULL )
		return 0;

	for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; lodID++ )
	{
		studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( studio );
		IMaterial **ppInputMaterials = pStudioHWData->m_pLODs[lodID].ppMaterials;

		if ( nSkin >= pStudioHdr->numskinfamilies )
		{
			nSkin = 0;
		}

		short *pSkinRef	= pStudioHdr->pSkinref( nSkin * pStudioHdr->numskinref );

		for (int i=0 ; i < pStudioHdr->numbodyparts ; i++) 
		{
			mstudiomodel_t *pModel = NULL;
			R_StudioSetupModel( i, nBody, &pModel, pStudioHdr );

			// Iterate over all the meshes.... each mesh is a new material
			for( int k = 0; k < pModel->nummeshes; ++k )
			{
				mstudiomesh_t *pMesh = pModel->pMesh(k);
				IMaterial *pMaterial = ppInputMaterials[pSkinRef[pMesh->material]];
				Assert( pMaterial );

				int m;
				for ( m=0; m<found; m++ )
				{
					if ( ppOutputMaterials[m] == pMaterial )
						break;
				}
				if ( m >= found )
				{
					// add uniquely
					ppOutputMaterials[found++] = pMaterial;

					// No more room to store additional materials!
					if ( found >= nCountOutputMaterials )
						return found;
				}
			}
		}
	}

	return found;
}


//-----------------------------------------------------------------------------
// Returns perf stats about a particular model
//-----------------------------------------------------------------------------
void CStudioRenderContext::GetPerfStats( DrawModelResults_t *pResults, const DrawModelInfo_t &info, CUtlBuffer *pSpewBuf ) const
{
	pResults->m_ActualTriCount = pResults->m_TextureMemoryBytes = 0;
	pResults->m_Materials.RemoveAll();

	Assert( info.m_Lod >= 0 );
	if ( info.m_Lod < 0 || !info.m_pHardwareData->m_pLODs )
		return;

	studiomeshdata_t *pStudioMeshes = info.m_pHardwareData->m_pLODs[info.m_Lod].m_pMeshData;

	// Set up an array that keeps up with the number of used hardware bones in the models.
	CUtlVector<bool> hardwareBonesUsed;
	hardwareBonesUsed.EnsureCount( info.m_pStudioHdr->numbones );
	int i;
	for( i = 0; i < info.m_pStudioHdr->numbones; i++ )
	{
		hardwareBonesUsed[i] = false;
	}

	//	Warning( "\n\n\n" );
	pResults->m_NumMaterials = 0;
	int numBoneStateChangeBatches = 0;
	int numBoneStateChanges = 0;
	// Iterate over every submodel...
	IMaterial **ppMaterials = info.m_pHardwareData->m_pLODs[info.m_Lod].ppMaterials;

	int nSkin = info.m_Skin;
	if ( nSkin >= info.m_pStudioHdr->numskinfamilies )
	{
		nSkin = 0;
	}
	short *pSkinRef	= info.m_pStudioHdr->pSkinref( nSkin * info.m_pStudioHdr->numskinref );

	pResults->m_NumBatches = 0;

	for (i=0 ; i < info.m_pStudioHdr->numbodyparts ; i++) 
	{
		mstudiomodel_t *pModel = NULL;
		R_StudioSetupModel( i, info.m_Body, &pModel, info.m_pStudioHdr );

		// Iterate over all the meshes.... each mesh is a new material
		int k;
		for( k = 0; k < pModel->nummeshes; ++k )
		{
			mstudiomesh_t *pMesh = pModel->pMesh(k);
			IMaterial *pMaterial = ppMaterials[pSkinRef[pMesh->material]];
			Assert( pMaterial );
			studiomeshdata_t *pMeshData = &pStudioMeshes[pMesh->meshid];
			if( pMeshData->m_NumGroup == 0 )
				continue;

			Assert( pResults->m_NumMaterials == pResults->m_Materials.Count() );
			pResults->m_NumMaterials++;
			if( pResults->m_NumMaterials < MAX_DRAW_MODEL_INFO_MATERIALS )
			{
				pResults->m_Materials.AddToTail( pMaterial );
			}
			else
			{
				Assert( 0 );
			}
			if( pSpewBuf )
			{
				pSpewBuf->Printf( "    material: %s\n", pMaterial->GetName() );
			}
			int numPasses = m_RC.m_pForcedMaterial ? m_RC.m_pForcedMaterial->GetNumPasses() : pMaterial->GetNumPasses();
			if( pSpewBuf )
			{
				pSpewBuf->Printf( "        numPasses:%d\n", numPasses );
			}
			int bytes = pMaterial->GetTextureMemoryBytes();
			pResults->m_TextureMemoryBytes += bytes;
			if( pSpewBuf )
			{
				pSpewBuf->Printf( "        texture memory: %d (Only valid in a rendering app)\n", bytes );
			}

			// Iterate over all stripgroups
			int stripGroupID;
			for( stripGroupID = 0; stripGroupID < pMeshData->m_NumGroup; stripGroupID++ )
			{
				studiomeshgroup_t *pMeshGroup = &pMeshData->m_pMeshGroup[stripGroupID];
				bool bIsFlexed = ( pMeshGroup->m_Flags & MESHGROUP_IS_FLEXED ) != 0;
				bool bIsHWSkinned = ( pMeshGroup->m_Flags & MESHGROUP_IS_HWSKINNED ) != 0;

				if( pSpewBuf )
				{
					pSpewBuf->Printf( "        %d batch(es):\n", ( int )pMeshGroup->m_NumStrips );
				}
				// Iterate over all strips. . . each strip potentially changes bones states.
				int stripID;
				for( stripID = 0; stripID < pMeshGroup->m_NumStrips; stripID++ )
				{
					pResults->m_NumBatches++;

					OptimizedModel::StripHeader_t *pStripData = &pMeshGroup->m_pStripData[stripID];
					numBoneStateChangeBatches++;
					numBoneStateChanges += pStripData->numBoneStateChanges;

					if( bIsHWSkinned )
					{
						// Only count bones as hardware bones if we are using hardware skinning here.
						int boneID;
						for( boneID = 0; boneID < pStripData->numBoneStateChanges; boneID++ )
						{
							OptimizedModel::BoneStateChangeHeader_t *pBoneStateChange = pStripData->pBoneStateChange( boneID );
							hardwareBonesUsed[pBoneStateChange->newBoneID] = true;
						}
					}

					if( pStripData->flags & OptimizedModel::STRIP_IS_TRILIST )
					{
						// TODO: need to factor in bIsFlexed and bIsHWSkinned
						int numTris = pStripData->numIndices / 3;
						if( pSpewBuf )
						{
							pSpewBuf->Printf( "            %s%s", bIsFlexed ? "flexed " : "nonflexed ",
								bIsHWSkinned ? "hwskinned " : "swskinned " );
							pSpewBuf->Printf( "tris: %d ", numTris );
							pSpewBuf->Printf( "bone changes: %d bones/strip: %d\n", pStripData->numBoneStateChanges,
								( int )pStripData->numBones );
						}
						pResults->m_ActualTriCount += numTris * numPasses;
					}
					else if( pStripData->flags & OptimizedModel::STRIP_IS_TRISTRIP )
					{
						Assert( 0 ); // FIXME: fill this in when we start using strips again.
					}
					else
					{
						Assert( 0 );
					}
				}
			}
		}
	}
	if( pSpewBuf )
	{
		char nil = '\0';
		pSpewBuf->Put( &nil, 1 );;
	}

	pResults->m_NumHardwareBones = 0;
	for( i = 0; i < info.m_pStudioHdr->numbones; i++ )
	{
		if( hardwareBonesUsed[i] )
		{
			pResults->m_NumHardwareBones++;
		}
	}
}


//-----------------------------------------------------------------------------
// Begin/end frame
//-----------------------------------------------------------------------------
static ConVar r_hwmorph( "r_hwmorph", "1", FCVAR_CHEAT );

void CStudioRenderContext::BeginFrame( void )
{
	// Cache a few values here so I don't have to in software inner loops:
	Assert( g_pMaterialSystemHardwareConfig );
	m_RC.m_Config.m_bSupportsVertexAndPixelShaders = g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders();
	m_RC.m_Config.m_bSupportsOverbright = g_pMaterialSystemHardwareConfig->SupportsOverbright();
	m_RC.m_Config.m_bEnableHWMorph = r_hwmorph.GetInt() != 0;

	// Haven't implemented the hw morph with threading yet
	if ( g_pMaterialSystem->GetThreadMode() != MATERIAL_SINGLE_THREADED )
	{
		m_RC.m_Config.m_bEnableHWMorph = false;
	}

	m_RC.m_Config.m_bStatsMode = false;

	g_pStudioRenderImp->PrecacheGlint();
}

void CStudioRenderContext::EndFrame( void )
{
}


//-----------------------------------------------------------------------------
// Methods related to config
//-----------------------------------------------------------------------------
void CStudioRenderContext::UpdateConfig( const StudioRenderConfig_t& config )
{
	memcpy( &m_RC.m_Config, &config, sizeof( StudioRenderConfig_t ) );
}

void CStudioRenderContext::GetCurrentConfig( StudioRenderConfig_t& config )
{
	memcpy( &config, &m_RC.m_Config, sizeof( StudioRenderConfig_t ) );
}


//-----------------------------------------------------------------------------
// Material overrides
//-----------------------------------------------------------------------------
void CStudioRenderContext::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType )
{
	m_RC.m_pForcedMaterial = newMaterial;
	m_RC.m_nForcedMaterialType = nOverrideType;
}

//-----------------------------------------------------------------------------
// Return the material overrides
//-----------------------------------------------------------------------------
void CStudioRenderContext::GetMaterialOverride( IMaterial** ppOutForcedMaterial, OverrideType_t* pOutOverrideType )
{
	Assert( ppOutForcedMaterial != NULL && pOutOverrideType != NULL );
	*ppOutForcedMaterial = m_RC.m_pForcedMaterial;
	*pOutOverrideType = m_RC.m_nForcedMaterialType;
}

//-----------------------------------------------------------------------------
// Sets the view state
//-----------------------------------------------------------------------------
void CStudioRenderContext::SetViewState( const Vector& viewOrigin, 
	const Vector& viewRight, const Vector& viewUp, const Vector& viewPlaneNormal )
{
	VectorCopy( viewOrigin, m_RC.m_ViewOrigin );
	VectorCopy( viewRight, m_RC.m_ViewRight );
	VectorCopy( viewUp, m_RC.m_ViewUp );
	VectorCopy( viewPlaneNormal, m_RC.m_ViewPlaneNormal );
}


//-----------------------------------------------------------------------------
// Sets lighting state
//-----------------------------------------------------------------------------
void CStudioRenderContext::SetAmbientLightColors( const Vector *pColors )
{
	for( int i = 0; i < 6; i++ )
	{
		VectorCopy( pColors[i], m_RC.m_LightBoxColors[i].AsVector3D() );
		m_RC.m_LightBoxColors[i][3] = 1.0f;
	}

	// FIXME: Would like to get this into the render thread, but there's systemic confusion
	// about whether to set lighting state here or in the material system
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetAmbientLightCube( m_RC.m_LightBoxColors );
}

void CStudioRenderContext::SetAmbientLightColors( const Vector4D *pColors )
{
	memcpy( m_RC.m_LightBoxColors, pColors, 6 * sizeof(Vector4D) );

	// FIXME: Would like to get this into the render thread, but there's systemic confusion
	// about whether to set lighting state here or in the material system
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetAmbientLightCube( m_RC.m_LightBoxColors );
}

void CStudioRenderContext::SetLocalLights( int nLightCount, const LightDesc_t *pLights )
{
	m_RC.m_NumLocalLights = CopyLocalLightingState( MAXLOCALLIGHTS, m_RC.m_LocalLights, nLightCount, pLights );

	// FIXME: Would like to get this into the render thread, but there's systemic confusion
	// about whether to set lighting state here or in the material system
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( m_RC.m_Config.bSoftwareLighting || m_RC.m_NumLocalLights == 0 )
	{
		pRenderContext->DisableAllLocalLights();
	}
	else
	{
		int i;
		int nMaxLightCount = g_pMaterialSystemHardwareConfig->MaxNumLights();
		int nLightCount = min( m_RC.m_NumLocalLights, nMaxLightCount );
		for( i = 0; i < nLightCount; i++ )
		{
			pRenderContext->SetLight( i, m_RC.m_LocalLights[i] );
		}
		for( ; i < nMaxLightCount; i++ )
		{
			LightDesc_t desc;
			desc.m_Type = MATERIAL_LIGHT_DISABLE;
			pRenderContext->SetLight( i, desc );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the color modulation
//-----------------------------------------------------------------------------
void CStudioRenderContext::SetColorModulation( const float* pColor )
{
	VectorCopy( pColor, m_RC.m_ColorMod );
}

void CStudioRenderContext::SetAlphaModulation( float alpha )
{
	m_RC.m_AlphaMod = alpha;
}


//-----------------------------------------------------------------------------
// Used to set bone-to-world transforms.
// FIXME: Should this be a lock/unlock pattern so we can't read after unlock?
//-----------------------------------------------------------------------------
matrix3x4_t* CStudioRenderContext::LockBoneMatrices( int nCount )
{
	MEM_ALLOC_CREDIT_( "CStudioRenderContext::m_BoneToWorldMatrices" );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	CMatRenderData<matrix3x4_t> rdMatrix( pRenderContext );
	matrix3x4_t *pDest = rdMatrix.Lock( nCount );
	return pDest;
}

void CStudioRenderContext::UnlockBoneMatrices()
{
}


//-----------------------------------------------------------------------------
// Allocates flex weights
//-----------------------------------------------------------------------------
void CStudioRenderContext::LockFlexWeights( int nWeightCount, float **ppFlexWeights, float **ppFlexDelayedWeights )
{
	MEM_ALLOC_CREDIT_( "CStudioRenderContext::m_FlexWeights" );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CMatRenderData<float> rdFlex( pRenderContext );
	CMatRenderData<float> rdFlexDelayed( pRenderContext );
	float *pFlexOut = rdFlex.Lock( nWeightCount ); 
	for ( int i = 0; i < nWeightCount; i++ )
	{
		pFlexOut[i] = 0.0f;
	}
	*ppFlexWeights = pFlexOut;
	if ( ppFlexDelayedWeights )
	{
		pFlexOut = rdFlexDelayed.Lock( nWeightCount );
		for ( int i = 0; i < nWeightCount; i++ )
		{
			pFlexOut[i] = 0.0f;
		}
		*ppFlexDelayedWeights = pFlexOut;
	}
}

void CStudioRenderContext::UnlockFlexWeights()
{
}


//-----------------------------------------------------------------------------
// Methods related to flex weights
//-----------------------------------------------------------------------------
static ConVar r_randomflex( "r_randomflex", "0", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
// This will generate random flex data that has a specified # of non-zero values
//-----------------------------------------------------------------------------
void CStudioRenderContext::GenerateRandomFlexWeights( int nWeightCount, float* pWeights, float *pDelayedWeights )
{
	int nRandomFlex = r_randomflex.GetInt();
	if ( nRandomFlex <= 0 || !pWeights )
		return;

	if ( nRandomFlex > nWeightCount )
	{
		nRandomFlex = nWeightCount;
	}

	int *pIndices = (int*)_alloca( nWeightCount * sizeof(int) );
	for ( int i = 0; i < nWeightCount; ++i )
	{
		pIndices[i] = i;
	}

	// Shuffle
	for ( int i = 0; i < nWeightCount; ++i )
	{
		int n = RandomInt( 0, nWeightCount-1 );
		int nTemp = pIndices[n];
		pIndices[n] = pIndices[i];
		pIndices[i] = nTemp;
	}

	memset( pWeights, 0, nWeightCount * sizeof(float) );
	for ( int i = 0; i < nRandomFlex; ++i )
	{
		pWeights[ pIndices[i] ] = RandomFloat( 0.0f, 1.0f );
	}
	if ( pDelayedWeights )
	{
		memset( pDelayedWeights, 0, nWeightCount * sizeof(float) );
		for ( int i = 0; i < nRandomFlex; ++i )
		{
			pDelayedWeights[ pIndices[i] ] = RandomFloat( 0.0f, 1.0f );
		}
	}
}


//-----------------------------------------------------------------------------
// Computes LOD
//-----------------------------------------------------------------------------
int CStudioRenderContext::ComputeRenderLOD( IMatRenderContext *pRenderContext, 
	const DrawModelInfo_t& info, const Vector &origin, float *pMetric )
{
	int lod = info.m_Lod;
	int lastlod = info.m_pHardwareData->m_NumLODs - 1;

	if ( pMetric )
	{
		*pMetric = 0.0f;
	}

	if ( lod == USESHADOWLOD )
		return lastlod;
	
	if ( lod != -1 )
		return clamp( lod, info.m_pHardwareData->m_RootLOD, lastlod );

	float screenSize = pRenderContext->ComputePixelWidthOfSphere( origin, 0.5f );
	lod = ComputeModelLODAndMetric( info.m_pHardwareData, screenSize, pMetric ); 

	// make sure we have a valid lod
	if ( info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD )
	{
		lastlod--;
	}

	lod = clamp( lod, info.m_pHardwareData->m_RootLOD, lastlod );
	return lod;
}


//-----------------------------------------------------------------------------
// This invokes proxies of all materials that are queued to be rendered
// It has the effect of ensuring the material vars are in the correct state
// since material var sets generated by the proxy bind are queued.
//-----------------------------------------------------------------------------
void CStudioRenderContext::InvokeBindProxies( const DrawModelInfo_t &info )
{
	if ( m_RC.m_pForcedMaterial )
	{
		if ( m_RC.m_nForcedMaterialType == OVERRIDE_NORMAL && m_RC.m_pForcedMaterial->HasProxy() )
		{
			m_RC.m_pForcedMaterial->CallBindProxy( info.m_pClientEntity );
		}
		return;
	}

	// get skinref array
	int nSkin = ( m_RC.m_Config.skin > 0 ) ? m_RC.m_Config.skin : info.m_Skin;
	short *pSkinRef	= info.m_pStudioHdr->pSkinref( 0 );
	if ( nSkin > 0 && nSkin < info.m_pStudioHdr->numskinfamilies )
	{
		pSkinRef += ( nSkin * info.m_pStudioHdr->numskinref );
	}

	// This is used to ensure proxies are only called once
	int nBufSize = info.m_pStudioHdr->numtextures * sizeof(bool);
	bool *pProxyCalled = (bool*)stackalloc( nBufSize );
	memset( pProxyCalled, 0, nBufSize );

	IMaterial **ppMaterials = info.m_pHardwareData->m_pLODs[ info.m_Lod ].ppMaterials;
	mstudiomodel_t *pModel;
	for ( int i=0 ; i < info.m_pStudioHdr->numbodyparts; ++i ) 
	{
		R_StudioSetupModel( i, info.m_Body, &pModel, info.m_pStudioHdr );
		for ( int somethingOtherThanI = 0; somethingOtherThanI < pModel->nummeshes; ++somethingOtherThanI)
		{
			mstudiomesh_t *pMesh = pModel->pMesh(somethingOtherThanI);
			int nMaterialIndex = pSkinRef[ pMesh->material ];
			if ( pProxyCalled[ nMaterialIndex ] )
				continue;
			pProxyCalled[ nMaterialIndex ] = true;
			IMaterial* pMaterial = ppMaterials[ nMaterialIndex ]; 
			if ( pMaterial && pMaterial->HasProxy() )
			{
				pMaterial->CallBindProxy( info.m_pClientEntity );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Draws a model
//-----------------------------------------------------------------------------
void CStudioRenderContext::DrawModel( DrawModelResults_t *pResults, const DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, float *pFlexWeights, float *pFlexDelayedWeights, const Vector &origin, int flags )
{
	// Set to zero in case we don't render anything.
	if ( pResults )
	{
		pResults->m_ActualTriCount = pResults->m_TextureMemoryBytes = 0;
	}

	if( !info.m_pStudioHdr || !info.m_pHardwareData || 
		!info.m_pHardwareData->m_NumLODs || !info.m_pHardwareData->m_pLODs )
	{
		return;
	} 

	// Replace the flex weight data with random data for testing
	GenerateRandomFlexWeights( info.m_pStudioHdr->numflexdesc, pFlexWeights, pFlexDelayedWeights );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	float flMetric;
	const_cast<DrawModelInfo_t*>( &info )->m_Lod = ComputeRenderLOD( pRenderContext, info, origin, &flMetric );
	if ( pResults )
	{
		pResults->m_nLODUsed = info.m_Lod;
		pResults->m_flLODMetric = flMetric;
	}

	MaterialLock_t hLock = 0;
	if ( flags & STUDIORENDER_DRAW_ACCURATETIME )
	{
		VPROF("STUDIORENDER_DRAW_ACCURATETIME");

		// Flush the material system before timing this model:
		hLock = g_pMaterialSystem->Lock();
		g_pMaterialSystem->Flush(true);
	}

	if ( pResults )
	{
		pResults->m_RenderTime.Start();
	}

	FlexWeights_t flex;
	flex.m_pFlexWeights = pFlexWeights ? pFlexWeights : s_pZeroFlexWeights;
	flex.m_pFlexDelayedWeights = pFlexDelayedWeights ? pFlexDelayedWeights : flex.m_pFlexWeights;

	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( !pCallQueue || studio_queue_mode.GetInt() == 0 )
	{
		g_pStudioRenderImp->DrawModel( info, m_RC, pBoneToWorld, flex, flags );
	}
	else
	{
		CMatRenderData<matrix3x4_t> rdMatrix( pRenderContext, info.m_pStudioHdr->numbones, pBoneToWorld );
		CMatRenderData<float> rdFlex( pRenderContext );
		CMatRenderData<float> rdFlexDelayed( pRenderContext );

		InvokeBindProxies( info );
		pBoneToWorld = rdMatrix.Base();
		if ( info.m_pStudioHdr->numflexdesc != 0 )
		{
			rdFlex.Lock( info.m_pStudioHdr->numflexdesc, flex.m_pFlexWeights );
			flex.m_pFlexWeights = rdFlex.Base();
			if ( !pFlexDelayedWeights )
			{
				flex.m_pFlexDelayedWeights = flex.m_pFlexWeights;
			}
			else
			{
				rdFlexDelayed.Lock( info.m_pStudioHdr->numflexdesc, flex.m_pFlexDelayedWeights );
				flex.m_pFlexDelayedWeights = rdFlexDelayed.Base();
			}
		}
		pCallQueue->QueueCall( g_pStudioRenderImp, &CStudioRender::DrawModel, info, m_RC, pBoneToWorld, flex, flags );
	}

	if( flags & STUDIORENDER_DRAW_ACCURATETIME )
	{
		VPROF( "STUDIORENDER_DRAW_ACCURATETIME" );

		// Make sure this model is completely drawn before ending the timer:
		g_pMaterialSystem->Flush(true);
		g_pMaterialSystem->Flush(true);
		g_pMaterialSystem->Unlock( hLock );
	}

	if ( pResults )
	{
		pResults->m_RenderTime.End();
		if( flags & STUDIORENDER_DRAW_GET_PERF_STATS )
		{
			GetPerfStats( pResults, info, 0 );
		}
	}
}


void CStudioRenderContext::DrawModelArray( const DrawModelInfo_t &drawInfo, int arrayCount, model_array_instance_t *pInstanceData, int instanceStride, int flags )
{
	// UNDONE: Support queue mode?
	g_pStudioRenderImp->DrawModelArray( drawInfo, m_RC, arrayCount, pInstanceData, instanceStride, flags );
}

//-----------------------------------------------------------------------------
// Methods related to rendering static props
//-----------------------------------------------------------------------------
void CStudioRenderContext::DrawModelStaticProp( const DrawModelInfo_t& info, const matrix3x4_t &modelToWorld, int flags )
{
	if ( info.m_Lod < info.m_pHardwareData->m_RootLOD )
	{
		const_cast< DrawModelInfo_t* >( &info )->m_Lod = info.m_pHardwareData->m_RootLOD;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( !pCallQueue || studio_queue_mode.GetInt() == 0 )
	{
		g_pStudioRenderImp->DrawModelStaticProp( info, m_RC, modelToWorld, flags );
	}
	else
	{
		InvokeBindProxies( info );
		pCallQueue->QueueCall( g_pStudioRenderImp, &CStudioRender::DrawModelStaticProp, info, m_RC, modelToWorld, flags );
	}
}

void CStudioRenderContext::DrawStaticPropDecals( const DrawModelInfo_t &info, const matrix3x4_t &modelToWorld )
{
	QUEUE_STUDIORENDER_CALL( DrawStaticPropDecals, CStudioRender, g_pStudioRenderImp, info, m_RC, modelToWorld );
}

void CStudioRenderContext::DrawStaticPropShadows( const DrawModelInfo_t &info, const matrix3x4_t &modelToWorld, int flags )
{
	QUEUE_STUDIORENDER_CALL( DrawStaticPropShadows, CStudioRender, g_pStudioRenderImp, info, m_RC, modelToWorld, flags );
}


//-----------------------------------------------------------------------------
// Methods related to shadows
//-----------------------------------------------------------------------------
void CStudioRenderContext::AddShadow( IMaterial* pMaterial, void* pProxyData, 
	FlashlightState_t *pFlashlightState, VMatrix *pWorldToTexture, ITexture *pFlashlightDepthTexture )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( !pCallQueue || studio_queue_mode.GetInt() == 0 )
	{
		g_pStudioRenderImp->AddShadow( pMaterial, pProxyData, pFlashlightState, pWorldToTexture, pFlashlightDepthTexture );
	}
	else
	{
		// NOTE: We don't need to make proxies work, because proxies are only ever used
		// when casting shadows onto props, which we don't do..that feature is disabled.
		// When casting flashlights onto mdls, which we *do* use, the proxy is NULL.
		Assert( pProxyData == NULL );
		if ( pProxyData != NULL )
		{
			Warning( "Cannot call CStudioRenderContext::AddShadows w/ proxies in queued mode!\n" );
			return;
		}

		CMatRenderData< FlashlightState_t > rdFlashlight( pRenderContext, 1, pFlashlightState );
		CMatRenderData< VMatrix > rdMatrix( pRenderContext, 1, pWorldToTexture );
		pCallQueue->QueueCall( g_pStudioRenderImp, &CStudioRender::AddShadow, pMaterial, 
			(void*)NULL, rdFlashlight.Base(), rdMatrix.Base(), pFlashlightDepthTexture );
	}
}

void CStudioRenderContext::ClearAllShadows()
{
	QUEUE_STUDIORENDER_CALL( ClearAllShadows, CStudioRender, g_pStudioRenderImp );
}


//-----------------------------------------------------------------------------
// Methods related to decals
//-----------------------------------------------------------------------------
void CStudioRenderContext::DestroyDecalList( StudioDecalHandle_t handle )
{
	QUEUE_STUDIORENDER_CALL( DestroyDecalList, CStudioRender, g_pStudioRenderImp, handle );
}

void CStudioRenderContext::AddDecal( StudioDecalHandle_t handle, studiohdr_t *pStudioHdr, 
	matrix3x4_t *pBoneToWorld, const Ray_t& ray, const Vector& decalUp, 
	IMaterial* pDecalMaterial, float radius, int body, bool noPokethru, int maxLODToDecal )
{
	// This substition always has to be done in the main thread, so do it here.
	pDecalMaterial = GetModelSpecificDecalMaterial( pDecalMaterial );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	Assert( pRenderContext->IsRenderData( pBoneToWorld ) );
	QUEUE_STUDIORENDER_CALL_RC( AddDecal, CStudioRender, g_pStudioRenderImp, pRenderContext, 
		handle, m_RC, pBoneToWorld, pStudioHdr, ray, decalUp, pDecalMaterial, radius, 
		body, noPokethru, maxLODToDecal );
}

// Function to do replacement because we always need to do this from the main thread.
IMaterial* GetModelSpecificDecalMaterial( IMaterial* pDecalMaterial )
{
	Assert( ThreadInMainThread() );
	// Since we're adding this to a studio model, check the decal to see if 
	// there's an alternate form used for static props...
	bool found;
	IMaterialVar* pModelMaterialVar = pDecalMaterial->FindVar( "$modelmaterial", &found, false );
	if ( found )
	{
		IMaterial* pModelMaterial = g_pMaterialSystem->FindMaterial( pModelMaterialVar->GetStringValue(), TEXTURE_GROUP_DECAL, false );
		if ( !IsErrorMaterial( pModelMaterial ) )
		{
			return pModelMaterial;
		}
	}

	return pDecalMaterial;
}


