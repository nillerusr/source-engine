//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "studiorendercontext.h"
#include "optimize.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CStudioRenderContext::GetTriangles( const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, GetTriangles_Output_t &out )
{
	VPROF( "CStudioRender::GetTriangles");

	out.m_MaterialBatches.RemoveAll(); // clear out data.

	if( !info.m_pStudioHdr || !info.m_pHardwareData || 
		!info.m_pHardwareData->m_NumLODs || !info.m_pHardwareData->m_pLODs )
	{
		return;
	} 

	int lod = info.m_Lod;
	int lastlod = info.m_pHardwareData->m_NumLODs - 1;

	if ( lod == USESHADOWLOD )
	{
		lod = lastlod;
	} 
	else
	{
		lod = clamp( lod, 0, lastlod );
	}

	// clamp to root lod
	if ( lod < info.m_pHardwareData->m_RootLOD)
	{
		lod = info.m_pHardwareData->m_RootLOD;
	}

	int nSkin = info.m_Skin;
	if ( nSkin >= info.m_pStudioHdr->numskinfamilies )
	{
		nSkin = 0;
	}
	short *pSkinRef	= info.m_pStudioHdr->pSkinref( nSkin * info.m_pStudioHdr->numskinref );

	studiomeshdata_t *pStudioMeshes = info.m_pHardwareData->m_pLODs[lod].m_pMeshData;
	IMaterial **ppMaterials = info.m_pHardwareData->m_pLODs[lod].ppMaterials;

	// Bone to world must be set before calling this function; it uses it here
	int boneMask = BONE_USED_BY_VERTEX_AT_LOD(lod);
	ComputePoseToWorld( out.m_PoseToWorld, info.m_pStudioHdr, boneMask, m_RC.m_ViewOrigin, pBoneToWorld );

	int i;
	for (i=0 ; i < info.m_pStudioHdr->numbodyparts ; i++) 
	{
		mstudiomodel_t *pModel = NULL;
		R_StudioSetupModel( i, info.m_Body, &pModel, info.m_pStudioHdr );

		// Iterate over all the meshes.... each mesh is a new material
		int k;
		for ( k = 0; k < pModel->nummeshes; ++k )
		{
			GetTriangles_MaterialBatch_t &materialBatch = out.m_MaterialBatches[out.m_MaterialBatches.AddToTail()];
			mstudiomesh_t *pMesh = pModel->pMesh(k);

			if ( !pModel->CacheVertexData( info.m_pStudioHdr ) )
			{
				// not available yet
				continue;
			}
			const mstudio_meshvertexdata_t *vertData = pMesh->GetVertexData( info.m_pStudioHdr );
			Assert( vertData ); // This can only return NULL on X360 for now

			// add the verts from this mesh to the materialBatch
			materialBatch.m_Verts.SetCount( pMesh->numvertices );
			for ( int vertID = 0; vertID < pMesh->numvertices; vertID++ )
			{
				GetTriangles_Vertex_t& vert = materialBatch.m_Verts[vertID];

				vert.m_Position = *vertData->Position( vertID );
				vert.m_Normal   = *vertData->Normal( vertID );
				vert.m_TexCoord = *vertData->Texcoord( vertID );

				if (vertData->HasTangentData())
				{
					vert.m_TangentS = *vertData->TangentS( vertID );
				}
#if _DEBUG
				else
				{
					// ensure any unintended access faults
					vert.m_TangentS.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
				}
#endif				
				vert.m_NumBones = vertData->BoneWeights( vertID )->numbones;
				int j;
				for ( j = 0; j < vert.m_NumBones; j++ )
				{
					vert.m_BoneWeight[j] = vertData->BoneWeights( vertID )->weight[j];
					vert.m_BoneIndex[j] = vertData->BoneWeights( vertID )->bone[j];
				}
			}

			IMaterial *pMaterial = ppMaterials[pSkinRef[pMesh->material]];
			Assert( pMaterial );
			materialBatch.m_pMaterial = pMaterial;
			studiomeshdata_t *pMeshData = &pStudioMeshes[pMesh->meshid];
			if ( pMeshData->m_NumGroup == 0 )
				continue;

			// Clear out indices
			materialBatch.m_TriListIndices.SetCount( 0 );

			// Iterate over all stripgroups
			int stripGroupID;
			for ( stripGroupID = 0; stripGroupID < pMeshData->m_NumGroup; stripGroupID++ )
			{
				studiomeshgroup_t *pMeshGroup = &pMeshData->m_pMeshGroup[stripGroupID];
//				bool bIsFlexed = ( pMeshGroup->m_Flags & MESHGROUP_IS_FLEXED ) != 0;
//				bool bIsHWSkinned = ( pMeshGroup->m_Flags & MESHGROUP_IS_HWSKINNED ) != 0;
				
				// Iterate over all strips. . . each strip potentially changes bones states.
				int stripID;
				for ( stripID = 0; stripID < pMeshGroup->m_NumStrips; stripID++ )
				{
					OptimizedModel::StripHeader_t *pStripData = &pMeshGroup->m_pStripData[stripID];
//					int boneID;
//					for( boneID = 0; boneID < pStripData->numBoneStateChanges; boneID++ )
//					{
//						OptimizedModel::BoneStateChangeHeader_t *pBoneStateChange = pStripData->pBoneStateChange( boneID );
//						hardwareBoneToGlobalBone[pBoneStateChange->hardwareID] = pBoneStateChange->newBoneID;
//					}
					if ( pStripData->flags & OptimizedModel::STRIP_IS_TRILIST )
					{
						for ( int i = 0; i < pStripData->numIndices; i += 3 )
						{
							int idx = pStripData->indexOffset + i;
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx ) );
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx + 1 ) );
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx + 2 ) );
						}
					}
					else
					{
						Assert( pStripData->flags & OptimizedModel::STRIP_IS_TRISTRIP );
						for (int i = 0; i < pStripData->numIndices - 2; ++i)
						{
							int idx = pStripData->indexOffset + i;
							bool ccw = (i & 0x1) == 0;
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx ) );
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx + 1 + ccw ) );
							materialBatch.m_TriListIndices.AddToTail( pMeshGroup->MeshIndex( idx + 2 - ccw ) );
						}
					}
				}
			}
		}
	}
}
