//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "mdllib_common.h"
#include "mdllib_stripinfo.h"
#include "mdllib_utils.h"

#include "studio.h"
#include "optimize.h"

#include "smartptr.h"


bool CMdlLib::CreateNewStripInfo( IMdlStripInfo **ppStripInfo )
{
	if ( !ppStripInfo )
		return false;

	if ( *ppStripInfo )
	{
		CMdlStripInfo *pMdlStripInfo = ( CMdlStripInfo * ) ( *ppStripInfo );
		pMdlStripInfo->Reset();
		return true;
	}

	*ppStripInfo = new CMdlStripInfo;
	return ( NULL != *ppStripInfo );
}

//
// StripModelBuffers
//	The main function that strips the model buffers
//		mdlBuffer			- mdl buffer, updated, no size change
//		vvdBuffer			- vvd buffer, updated, size reduced
//		vtxBuffer			- vtx buffer, updated, size reduced
//		ppStripInfo			- if nonzero on return will be filled with the stripping info
//
bool CMdlLib::StripModelBuffers( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, IMdlStripInfo **ppStripInfo )
{
	DECLARE_PTR( byte, mdl, BYTE_OFF_PTR( mdlBuffer.Base(), mdlBuffer.TellGet() ) );
	DECLARE_PTR( byte, vvd, BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ) );
	DECLARE_PTR( byte, vtx, BYTE_OFF_PTR( vtxBuffer.Base(), vtxBuffer.TellGet() ) );

	int vvdLength = vvdBuffer.TellPut() - vvdBuffer.TellGet();
	int vtxLength = vtxBuffer.TellPut() - vtxBuffer.TellGet();

	//
	// ===================
	// =================== Modify the checksum and check if further processing is needed
	// ===================
	//

	DECLARE_PTR( studiohdr_t, mdlHdr, mdl );
	DECLARE_PTR( vertexFileHeader_t, vvdHdr, vvd );
	DECLARE_PTR( OptimizedModel::FileHeader_t, vtxHdr, vtx );
	
	long checksumOld = mdlHdr->checksum;

	// Don't do anything if the checksums don't match
	if ( ( mdlHdr->checksum != vvdHdr->checksum ) ||
		 ( mdlHdr->checksum != vtxHdr->checkSum ) )
	{
		DLog( "mdllib", 1, "ERROR: [StripModelBuffers] checksum mismatch!\n" );
		return false;
	}

	// Modify the checksums
	mdlHdr->checksum ^= ( mdlHdr->checksum * 123457 );
	vvdHdr->checksum ^= ( vvdHdr->checksum * 123457 );
	vtxHdr->checkSum ^= ( vtxHdr->checkSum * 123457 );

	long checksumNew = mdlHdr->checksum;

	// Allocate the model stripping info
	CMdlStripInfo msi;
	CMdlStripInfo *pMsi;

	if ( ppStripInfo )
	{
		if ( *ppStripInfo )
		{
			pMsi = ( CMdlStripInfo * ) ( *ppStripInfo );
			pMsi->Reset();
		}
		else
		{
			*ppStripInfo = pMsi = new CMdlStripInfo;
		}
	}
	else
	{
		pMsi = &msi;
	}

	// Set the basic stripping info settings
	pMsi->m_lChecksumOld = checksumOld;
	pMsi->m_lChecksumNew = checksumNew;

	//
	// Early outs
	//

	if ( !( mdlHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) )
	{
		DLog( "mdllib", 2, "No special stripping - the model is not a static prop.\n" );
		pMsi->m_eMode = CMdlStripInfo::MODE_NO_CHANGE;
		return true;
	}

	if ( vvdHdr->numLODs <= 1 )
	{
		DLog( "mdllib", 2, "No special stripping - the model has only %d lod(s).\n", vvdHdr->numLODs );
		pMsi->m_eMode = CMdlStripInfo::MODE_NO_CHANGE;
		return true;
	}

	if ( mdlHdr->numbones != 1 )
	{
		DLog( "mdllib", 2, "No special stripping - the model has %d bone(s).\n", mdlHdr->numbones );
		pMsi->m_eMode = CMdlStripInfo::MODE_NO_CHANGE;
		return true;
	}

	// Otherwise do stripping
	pMsi->m_eMode = CMdlStripInfo::MODE_STRIP_LOD_1N;

	
	//
	// ===================
	// =================== Build out table of LOD0 vertexes
	// ===================
	//

	CGrowableBitVec &mapVtxIndex = pMsi->m_vtxVerts;
	ITERATE_CHILDREN2( OptimizedModel::BodyPartHeader_t, mstudiobodyparts_t, vtxBodyPart, mdlBodyPart, vtxHdr, mdlHdr, pBodyPart, pBodypart, numBodyParts )
		ITERATE_CHILDREN2( OptimizedModel::ModelHeader_t, mstudiomodel_t, vtxModel, mdlModel, vtxBodyPart, mdlBodyPart, pModel, pModel, numModels )
		
			OptimizedModel::ModelLODHeader_t *vtxLod = CHILD_AT( vtxModel, pLOD, 0 );
			ITERATE_CHILDREN2( OptimizedModel::MeshHeader_t, mstudiomesh_t, vtxMesh, mdlMesh, vtxLod, mdlModel, pMesh, pMesh, numMeshes )
				ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
					ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
					
						if ( !( vtxStrip->flags & OptimizedModel::STRIP_IS_TRILIST ) )
							continue;
						for ( int i = 0; i < vtxStrip->numIndices; ++ i )
						{
							unsigned short *vtxIdx = CHILD_AT( vtxStripGroup, pIndex, vtxStrip->indexOffset + i );
							OptimizedModel::Vertex_t *vtxVertex = CHILD_AT( vtxStripGroup, pVertex, *vtxIdx );
							
							unsigned short usIdx = vtxVertex->origMeshVertID + mdlMesh->vertexoffset;
							mapVtxIndex.GrowSetBit( usIdx );
						}

					ITERATE_END
				ITERATE_END
			ITERATE_END
		
		ITERATE_END
	ITERATE_END

	//
	// Now having the table of which vertexes to keep we will construct a remapping table
	//
	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = pMsi->m_vtxIndices;
	srcIndices.EnsureCapacity( mapVtxIndex.GetNumBits() );
	for ( int iBit = -1; ( iBit = mapVtxIndex.FindNextSetBit( iBit + 1 ) ) >= 0; )
		srcIndices.InsertNoSort( ( unsigned short ) ( unsigned int ) iBit );
	srcIndices.RedoSort(); // - doesn't do anything, just validates the vector

	// Now we have the following questions answered:
	//  - for every index we know if it belongs to lod0 "mapVtxIndex.IsBitSet( oldVertIdx )"
	//  - for every new vertex we know its old index "srcIndices[ newVertIdx ]"
	//  - for every old vertex if it's in lod0 we know its new index "srcIndices.Find( oldVertIdx )"

	
	//
	// ===================
	// =================== Process MDL file
	// ===================
	//

	//
	// Update vertex counts
	//
	int mdlNumVerticesOld = 0;
	CUtlSortVector< CMdlStripInfo::MdlRangeItem, CLessSimple< CMdlStripInfo::MdlRangeItem > > &arrMdlOffsets = pMsi->m_vtxMdlOffsets;
	ITERATE_CHILDREN( mstudiobodyparts_t, mdlBodyPart, mdlHdr, pBodypart, numbodyparts )
		ITERATE_CHILDREN( mstudiomodel_t, mdlModel, mdlBodyPart, pModel, nummodels )
			
			DLog( "mdllib", 3, " Stripped %d lod(s).\n", vvdHdr->numLODs - 1 ),
			DLog( "mdllib", 3, " Stripped %d vertexes (was: %d, now: %d).\n", mdlModel->numvertices - srcIndices.Count(), mdlModel->numvertices, srcIndices.Count() );

			mdlNumVerticesOld = mdlModel->numvertices;
			mdlModel->numvertices = srcIndices.Count();

			mdlModel->vertexdata.pVertexData = BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart );
			mdlModel->vertexdata.pTangentData = BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart );

			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes )
				
				CMdlStripInfo::MdlRangeItem mdlRangeItem( mdlMesh->vertexoffset, mdlMesh->numvertices );
				
				mdlMesh->vertexdata.modelvertexdata = &mdlModel->vertexdata;
				mdlMesh->numvertices = srcIndices.FindLess( mdlMesh->vertexoffset + mdlMesh->numvertices );
				mdlMesh->vertexoffset = srcIndices.FindLess( mdlMesh->vertexoffset ) + 1;
				mdlMesh->numvertices -= mdlMesh->vertexoffset - 1;

				mdlRangeItem.m_offNew = mdlMesh->vertexoffset;
				mdlRangeItem.m_numNew = mdlMesh->numvertices;
				arrMdlOffsets.Insert( mdlRangeItem );

				// Truncate the number of vertexes
				for ( int k = 0; k < ARRAYSIZE( mdlMesh->vertexdata.numLODVertexes ); ++ k )
					mdlMesh->vertexdata.numLODVertexes[ k ] = mdlMesh->numvertices;

			ITERATE_END
		ITERATE_END
	ITERATE_END

	//
	// Update bones not to mention anything below LOD0
	//
	ITERATE_CHILDREN( mstudiobone_t, mdlBone, mdlHdr, pBone, numbones )
		mdlBone->flags &= ( BONE_USED_BY_VERTEX_LOD0 | ~BONE_USED_BY_VERTEX_MASK );
	ITERATE_END

	DLog( "mdllib", 3, " Updated %d bone(s).\n", mdlHdr->numbones );

	
	//
	// ===================
	// =================== Process VVD file
	// ===================
	//

	vvdHdr->numLODs = 1;
	for ( int k = 0; k < ARRAYSIZE( vvdHdr->numLODVertexes ); ++ k )
		vvdHdr->numLODVertexes[ k ] = srcIndices.Count();

	DECLARE_PTR( mstudiovertex_t, vvdVertexSrc, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );
	DECLARE_PTR( Vector4D, vvdTangentSrc, vvdHdr->tangentDataStart ? BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart ) : NULL );
	
	// Apply the fixups first of all
	if ( vvdHdr->numFixups )
	{
		CArrayAutoPtr< byte > memTempVVD( new byte[ vvdLength ] );
		DECLARE_PTR( mstudiovertex_t, vvdVertexNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ) );
		DECLARE_PTR( Vector4D, vvdTangentNew, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->tangentDataStart ) );
		DECLARE_PTR( vertexFileFixup_t, vvdFixup, BYTE_OFF_PTR( vvdHdr, vvdHdr->fixupTableStart ) );
		for ( int k = 0; k < vvdHdr->numFixups; ++ k )
		{
			memcpy( vvdVertexNew, vvdVertexSrc + vvdFixup[ k ].sourceVertexID, vvdFixup[ k ].numVertexes * sizeof( *vvdVertexNew ) );
			vvdVertexNew += vvdFixup[ k ].numVertexes;
			if ( vvdTangentSrc )
			{
				memcpy( vvdTangentNew, vvdTangentSrc + vvdFixup[ k ].sourceVertexID, vvdFixup[ k ].numVertexes * sizeof( *vvdTangentNew ) );
				vvdTangentNew += vvdFixup[ k ].numVertexes;
			}
		}

		// Move back the memory after fixups were applied
		vvdVertexSrc  ? memcpy( vvdVertexSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->vertexDataStart ), mdlNumVerticesOld * sizeof( *vvdVertexSrc ) ) : 0;
		vvdTangentSrc ? memcpy( vvdTangentSrc, BYTE_OFF_PTR( memTempVVD.Get(), vvdHdr->tangentDataStart ), mdlNumVerticesOld * sizeof( *vvdTangentSrc ) ) : 0;
	}
	
	vvdHdr->vertexDataStart -= ALIGN_VALUE( sizeof( vertexFileFixup_t ) * vvdHdr->numFixups, 16 );
	vvdHdr->numFixups = 0;
	DECLARE_PTR( mstudiovertex_t, vvdVertexNew, BYTE_OFF_PTR( vvdHdr, vvdHdr->vertexDataStart ) );
	for ( int k = 0; k < srcIndices.Count(); ++ k )
		vvdVertexNew[ k ] = vvdVertexSrc[ srcIndices[ k ] ];

	size_t newVertexDataSize = srcIndices.Count() * sizeof( mstudiovertex_t );
	int vvdLengthOld = vvdLength;
	vvdLength = vvdHdr->vertexDataStart + newVertexDataSize;

	if ( vvdTangentSrc )
	{
		// Move the tangents
		vvdHdr->tangentDataStart = vvdLength;
		DECLARE_PTR( Vector4D, vvdTangentNew, BYTE_OFF_PTR( vvdHdr, vvdHdr->tangentDataStart ) );

		for ( int k = 0; k < srcIndices.Count(); ++ k )
			vvdTangentNew[ k ] = vvdTangentSrc[ srcIndices[ k ] ];

		vvdLength += srcIndices.Count() * sizeof( Vector4D );
	}
	DLog( "mdllib", 3, " Stripped %d vvd bytes.\n", vvdLengthOld - vvdLength );

	
	//
	// ===================
	// =================== Process VTX file
	// ===================
	//

	size_t vtxOffIndexBuffer = ~size_t(0), vtxOffIndexBufferEnd = 0;
	size_t vtxOffVertexBuffer = ~size_t(0), vtxOffVertexBufferEnd = 0;
	CRemoveTracker vtxRemove;
	CUtlVector< size_t > vtxOffIndex;
	CUtlVector< size_t > vtxOffVertex;

	vtxRemove.RemoveElements( CHILD_AT( vtxHdr, pMaterialReplacementList, 1 ), vtxHdr->numLODs - 1 );
	ITERATE_CHILDREN( OptimizedModel::MaterialReplacementListHeader_t, vtxMatList, vtxHdr, pMaterialReplacementList, numLODs )
		if ( !vtxMatList_idx ) continue;
		vtxRemove.RemoveElements( CHILD_AT( vtxMatList, pMaterialReplacement, 0 ), vtxMatList->numReplacements );
		ITERATE_CHILDREN( OptimizedModel::MaterialReplacementHeader_t, vtxMat, vtxMatList, pMaterialReplacement, numReplacements )
			char const *szName = vtxMat->pMaterialReplacementName();
			vtxRemove.RemoveElements( szName, szName ? strlen( szName ) + 1 : 0 );
		ITERATE_END
	ITERATE_END

	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
		
			vtxRemove.RemoveElements( CHILD_AT( vtxModel, pLOD, 1 ), vtxModel->numLODs - 1 );
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				if ( !vtxLod_idx )	// Process only lod1-N
					continue;
				
				vtxRemove.RemoveElements( CHILD_AT( vtxLod, pMesh, 0 ), vtxLod->numMeshes );
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					vtxRemove.RemoveElements( CHILD_AT( vtxMesh, pStripGroup, 0 ), vtxMesh->numStripGroups );
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						vtxRemove.RemoveElements( CHILD_AT( vtxStripGroup, pStrip, 0 ), vtxStripGroup->numStrips );
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							vtxRemove.RemoveElements( CHILD_AT( vtxStrip, pBoneStateChange, 0 ), vtxStrip->numBoneStateChanges );
						ITERATE_END
					ITERATE_END
				ITERATE_END

			ITERATE_END

			// Use all lods to determine the ranges of vertex and index buffers.
			// We rely on the fact that vertex and index buffers are laid out as one solid memory block for all lods.
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )

						size_t offIndex = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pIndex, 0 ) );
						size_t offIndexEnd = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pIndex, vtxStripGroup->numIndices ) );
						size_t offVertex = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pVertex, 0 ) );
						size_t offVertexEnd = BYTE_DIFF_PTR( vtxHdr, CHILD_AT( vtxStripGroup, pVertex, vtxStripGroup->numVerts ) );

						if ( offIndex < vtxOffIndexBuffer )
							vtxOffIndexBuffer = offIndex;
						if ( offIndexEnd > vtxOffIndexBufferEnd )
							vtxOffIndexBufferEnd = offIndexEnd;
						if ( offVertex < vtxOffVertexBuffer )
							vtxOffVertexBuffer = offVertex;
						if ( offVertexEnd > vtxOffVertexBufferEnd )
							vtxOffVertexBufferEnd = offVertexEnd;

						if ( !vtxLod_idx )
						{
							vtxOffIndex.AddToTail( offIndex );
							vtxOffIndex.AddToTail( offIndexEnd );
							vtxOffVertex.AddToTail( offVertex );
							vtxOffVertex.AddToTail( offVertexEnd );
						}

					ITERATE_END
				ITERATE_END
			ITERATE_END

		ITERATE_END
	ITERATE_END

	// Fixup the vertex buffer
	DECLARE_PTR( OptimizedModel::Vertex_t, vtxVertexBuffer, BYTE_OFF_PTR( vtxHdr, vtxOffVertexBuffer ) );
	DECLARE_PTR( OptimizedModel::Vertex_t, vtxVertexBufferEnd, BYTE_OFF_PTR( vtxHdr, vtxOffVertexBufferEnd ) );
	CUtlVector< int > vtxIndexDeltas;
	vtxIndexDeltas.EnsureCapacity( vtxVertexBufferEnd - vtxVertexBuffer );
	int vtxNumVertexRemoved = 0;
	for ( OptimizedModel::Vertex_t *vtxVertexElement = vtxVertexBuffer; vtxVertexElement < vtxVertexBufferEnd; ++ vtxVertexElement )
	{
		size_t const off = BYTE_DIFF_PTR( vtxHdr, vtxVertexElement );
		bool bUsed = false;
		for ( int k = 0; k < vtxOffVertex.Count(); k += 2 )
		{
			if ( off >= vtxOffVertex[ k ] && off < vtxOffVertex[ k + 1 ] )
			{
				bUsed = true;
				break;
			}
		}
		if ( !bUsed )
		{
			// Index is not in use
			vtxRemove.RemoveElements( vtxVertexElement );
			vtxIndexDeltas.AddToTail( 0 );
			vtxNumVertexRemoved ++;
		}
		else
		{	// Index is in use and must be remapped
			// Find the mesh where this index belongs
			int iMesh = arrMdlOffsets.FindLessOrEqual( CMdlStripInfo::MdlRangeItem( 0, 0, vtxVertexElement - vtxVertexBuffer ) );
			Assert( iMesh >= 0 && iMesh < arrMdlOffsets.Count() );
			
			CMdlStripInfo::MdlRangeItem &mri = arrMdlOffsets[ iMesh ];
			Assert( ( vtxVertexElement - vtxVertexBuffer >= mri.m_offNew ) && ( vtxVertexElement - vtxVertexBuffer < mri.m_offNew + mri.m_numNew ) );
			
			Assert( mapVtxIndex.IsBitSet( vtxVertexElement->origMeshVertID + mri.m_offOld ) );
			vtxVertexElement->origMeshVertID = srcIndices.Find( vtxVertexElement->origMeshVertID + mri.m_offOld ) - mri.m_offNew;
			Assert( vtxVertexElement->origMeshVertID < mri.m_numNew );
			vtxIndexDeltas.AddToTail( vtxNumVertexRemoved );
		}
	}

	// Fixup the index buffer
	DECLARE_PTR( unsigned short, vtxIndexBuffer, BYTE_OFF_PTR( vtxHdr, vtxOffIndexBuffer ) );
	DECLARE_PTR( unsigned short, vtxIndexBufferEnd, BYTE_OFF_PTR( vtxHdr, vtxOffIndexBufferEnd ) );
	for ( unsigned short *vtxIndexElement = vtxIndexBuffer; vtxIndexElement < vtxIndexBufferEnd; ++ vtxIndexElement )
	{
		size_t const off = BYTE_DIFF_PTR( vtxHdr, vtxIndexElement );
		bool bUsed = false;
		for ( int k = 0; k < vtxOffIndex.Count(); k += 2 )
		{
			if ( off >= vtxOffIndex[ k ] && off < vtxOffIndex[ k + 1 ] )
			{
				bUsed = true;
				break;
			}
		}
		if ( !bUsed )
		{
			// Index is not in use
			vtxRemove.RemoveElements( vtxIndexElement );
		}
		else
		{
			// Index is in use and must be remapped
			*vtxIndexElement -= vtxIndexDeltas[ *vtxIndexElement ];
		}
	}

	// By now should have scheduled all removal information
	vtxRemove.Finalize();
	DLog( "mdllib", 3, " Stripped %d vtx bytes.\n", vtxRemove.GetNumBytesRemoved() );

	//
	// Fixup all the offsets
	//
	ITERATE_CHILDREN( OptimizedModel::MaterialReplacementListHeader_t, vtxMatList, vtxHdr, pMaterialReplacementList, numLODs )
		ITERATE_CHILDREN( OptimizedModel::MaterialReplacementHeader_t, vtxMat, vtxMatList, pMaterialReplacement, numReplacements )
			vtxMat->replacementMaterialNameOffset = vtxRemove.ComputeOffset( vtxMat, vtxMat->replacementMaterialNameOffset );
		ITERATE_END
		vtxMatList->replacementOffset = vtxRemove.ComputeOffset( vtxMatList, vtxMatList->replacementOffset );
	ITERATE_END
	ITERATE_CHILDREN( OptimizedModel::BodyPartHeader_t, vtxBodyPart, vtxHdr, pBodyPart, numBodyParts )
		ITERATE_CHILDREN( OptimizedModel::ModelHeader_t, vtxModel, vtxBodyPart, pModel, numModels )
			ITERATE_CHILDREN( OptimizedModel::ModelLODHeader_t, vtxLod, vtxModel, pLOD, numLODs )
				ITERATE_CHILDREN( OptimizedModel::MeshHeader_t, vtxMesh, vtxLod, pMesh, numMeshes )
					ITERATE_CHILDREN( OptimizedModel::StripGroupHeader_t, vtxStripGroup, vtxMesh, pStripGroup, numStripGroups )
						
						ITERATE_CHILDREN( OptimizedModel::StripHeader_t, vtxStrip, vtxStripGroup, pStrip, numStrips )
							vtxStrip->indexOffset =
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset + vtxStrip->indexOffset ) -
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
							vtxStrip->vertOffset =
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset + vtxStrip->vertOffset ) -
								vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
							vtxStrip->boneStateChangeOffset = vtxRemove.ComputeOffset( vtxStrip, vtxStrip->boneStateChangeOffset );
						ITERATE_END

						vtxStripGroup->vertOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->vertOffset );
						vtxStripGroup->indexOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->indexOffset );
						vtxStripGroup->stripOffset = vtxRemove.ComputeOffset( vtxStripGroup, vtxStripGroup->stripOffset );
					ITERATE_END
					vtxMesh->stripGroupHeaderOffset = vtxRemove.ComputeOffset( vtxMesh, vtxMesh->stripGroupHeaderOffset );
				ITERATE_END
				vtxLod->meshOffset = vtxRemove.ComputeOffset( vtxLod, vtxLod->meshOffset );
			ITERATE_END
			vtxModel->lodOffset = vtxRemove.ComputeOffset( vtxModel, vtxModel->lodOffset );
			vtxModel->numLODs = 1;
		ITERATE_END
		vtxBodyPart->modelOffset = vtxRemove.ComputeOffset( vtxBodyPart, vtxBodyPart->modelOffset );
	ITERATE_END
	vtxHdr->materialReplacementListOffset = vtxRemove.ComputeOffset( vtxHdr, vtxHdr->materialReplacementListOffset );
	vtxHdr->bodyPartOffset = vtxRemove.ComputeOffset( vtxHdr, vtxHdr->bodyPartOffset );
	vtxHdr->numLODs = 1;

	// Perform final memory move
	vtxRemove.MemMove( vtxHdr, vtxLength );


	//
	// ===================
	// =================== Truncate buffer sizes
	// ===================
	//

	vvdBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vvdBuffer.TellGet() + vvdLength - vvdBuffer.TellPut() );
	vtxBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vtxBuffer.TellGet() + vtxLength - vtxBuffer.TellPut() );


	DLog( "mdllib", 2, " Reduced model buffers by %d bytes.\n", vtxRemove.GetNumBytesRemoved() + ( vvdLengthOld - vvdLength ) );

	// Done
	return true;
}

