//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "mdllib_common.h"
#include "mdllib_stripinfo.h"
#include "mdllib_utils.h"

#include "studio.h"
#include "optimize.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/hardwareverts.h"

#include "smartptr.h"



//////////////////////////////////////////////////////////////////////////
//
// CMdlStripInfo implementation
//
//////////////////////////////////////////////////////////////////////////


CMdlStripInfo::CMdlStripInfo() :
	m_eMode( MODE_UNINITIALIZED ),
	m_lChecksumOld( 0 ),
	m_lChecksumNew( 0 )
{
	NULL;
}


bool CMdlStripInfo::Serialize( CUtlBuffer &bufStorage ) const
{
	char chHeader[ 4 ] = { 'M', 'A', 'P', m_eMode };
	bufStorage.Put( chHeader, sizeof( chHeader ) );

	switch ( m_eMode )
	{
	default:
	case MODE_UNINITIALIZED:
		return true;
	
	case MODE_NO_CHANGE:
		bufStorage.PutInt( m_lChecksumOld );
		bufStorage.PutInt( m_lChecksumNew );
		return true;

	case MODE_STRIP_LOD_1N:
		bufStorage.PutInt( m_lChecksumOld );
		bufStorage.PutInt( m_lChecksumNew );

		bufStorage.PutInt( m_vtxVerts.GetNumBits() );
		for ( uint32 const *pdwBase = m_vtxVerts.Base(), *pdwEnd = pdwBase + m_vtxVerts.GetNumDWords();
			pdwBase < pdwEnd; ++ pdwBase )
			bufStorage.PutUnsignedInt( *pdwBase );

		bufStorage.PutInt( m_vtxIndices.Count() );
		for ( unsigned short const *pusBase = m_vtxIndices.Base(), *pusEnd = pusBase + m_vtxIndices.Count();
			pusBase < pusEnd; ++ pusBase )
			bufStorage.PutUnsignedShort( *pusBase );

		bufStorage.PutInt( m_vtxMdlOffsets.Count() );
		for ( MdlRangeItem const *pmri = m_vtxMdlOffsets.Base(), *pmriEnd = pmri + m_vtxMdlOffsets.Count();
			pmri < pmriEnd; ++ pmri )
			bufStorage.PutInt( pmri->m_offOld ),
			bufStorage.PutInt( pmri->m_offNew ),
			bufStorage.PutInt( pmri->m_numOld ),
			bufStorage.PutInt( pmri->m_numNew );

		return true;
	}
}

bool CMdlStripInfo::UnSerialize( CUtlBuffer &bufData )
{
	char chHeader[ 4 ];
	bufData.Get( chHeader, sizeof( chHeader ) );

	if ( memcmp( chHeader, "MAP", 3 ) )
		return false;

	switch ( chHeader[3] )
	{
	default:
		return false;
	
	case MODE_UNINITIALIZED:
		m_eMode = MODE_UNINITIALIZED;
		m_lChecksumOld = 0;
		m_lChecksumNew = 0;
		return true;

	case MODE_NO_CHANGE:
		m_eMode = MODE_NO_CHANGE;
		m_lChecksumOld = bufData.GetInt();
		m_lChecksumNew = bufData.GetInt();
		return true;

	case MODE_STRIP_LOD_1N:
		m_eMode = MODE_STRIP_LOD_1N;
		m_lChecksumOld = bufData.GetInt();
		m_lChecksumNew = bufData.GetInt();

		m_vtxVerts.Resize( bufData.GetInt(), true );
		for ( uint32 *pdwBase = m_vtxVerts.Base(), *pdwEnd = pdwBase + m_vtxVerts.GetNumDWords();
			pdwBase < pdwEnd; ++ pdwBase )
			*pdwBase = bufData.GetUnsignedInt();

		m_vtxIndices.SetCount( bufData.GetInt() );
		for ( unsigned short *pusBase = m_vtxIndices.Base(), *pusEnd = pusBase + m_vtxIndices.Count();
			pusBase < pusEnd; ++ pusBase )
			*pusBase = bufData.GetUnsignedShort();

		m_vtxMdlOffsets.SetCount( bufData.GetInt() );
		for ( MdlRangeItem *pmri = m_vtxMdlOffsets.Base(), *pmriEnd = pmri + m_vtxMdlOffsets.Count();
			pmri < pmriEnd; ++ pmri )
			pmri->m_offOld = bufData.GetInt(),
			pmri->m_offNew = bufData.GetInt(),
			pmri->m_numOld = bufData.GetInt(),
			pmri->m_numNew = bufData.GetInt();

		return true;
	}
}


// Returns the checksums that the stripping info was generated for:
//	plChecksumOriginal		if non-NULL will hold the checksum of the original model submitted for stripping
//	plChecksumStripped		if non-NULL will hold the resulting checksum of the stripped model
bool CMdlStripInfo::GetCheckSum( long *plChecksumOriginal, long *plChecksumStripped ) const
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	if ( plChecksumOriginal )
		*plChecksumOriginal = m_lChecksumOld;

	if ( plChecksumStripped )
		*plChecksumStripped = m_lChecksumNew;

	return true;
}


//
// StripHardwareVertsBuffer
//	The main function that strips the vhv buffer
//		vhvBuffer		- vhv buffer, updated, size reduced
//
bool CMdlStripInfo::StripHardwareVertsBuffer( CUtlBuffer &vhvBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vhv header
	//
	DECLARE_PTR( HardwareVerts::FileHeader_t, vhvHdr, BYTE_OFF_PTR( vhvBuffer.Base(), vhvBuffer.TellGet() ) );
	int vhvLength = vhvBuffer.TellPut() - vhvBuffer.TellGet();

	if ( vhvHdr->m_nChecksum != m_lChecksumOld )
	{
		DLog( "mdllib", 1, "ERROR: [StripHardwareVertsBuffer] checksum mismatch!\n" );
		return false;
	}

	vhvHdr->m_nChecksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;


	Assert( m_eMode == MODE_STRIP_LOD_1N );

	//
	// Now reconstruct the vhv structures to do the mapping
	//

	CRemoveTracker vhvRemove;
	size_t vhvVertOffset = ~size_t( 0 ), vhvEndMeshOffset = sizeof( HardwareVerts::FileHeader_t );
	int numMeshesRemoved = 0, numVertsRemoved = 0;

	ITERATE_CHILDREN( HardwareVerts::MeshHeader_t, vhvMesh, vhvHdr, pMesh, m_nMeshes )
		if ( vhvMesh->m_nOffset < vhvVertOffset )
			vhvVertOffset = vhvMesh->m_nOffset;
		if ( BYTE_DIFF_PTR( vhvHdr, vhvMesh + 1 ) > vhvEndMeshOffset )
			vhvEndMeshOffset = BYTE_DIFF_PTR( vhvHdr, vhvMesh + 1 );
		if ( !vhvMesh->m_nLod )
			continue;
		vhvRemove.RemoveBytes( BYTE_OFF_PTR( vhvHdr, vhvMesh->m_nOffset ), vhvMesh->m_nVertexes * vhvHdr->m_nVertexSize );
		vhvRemove.RemoveElements( vhvMesh );
		numVertsRemoved += vhvMesh->m_nVertexes;
		++ numMeshesRemoved;
	ITERATE_END
	vhvRemove.RemoveBytes( BYTE_OFF_PTR( vhvHdr, vhvEndMeshOffset ), vhvVertOffset - vhvEndMeshOffset );	// Padding
	vhvRemove.RemoveBytes( BYTE_OFF_PTR( vhvHdr, vhvVertOffset + vhvHdr->m_nVertexes * vhvHdr->m_nVertexSize ), vhvLength - ( vhvVertOffset + vhvHdr->m_nVertexes * vhvHdr->m_nVertexSize ) );

	vhvRemove.Finalize();
	DLog( "mdllib", 3, " Stripped %d vhv bytes.\n", vhvRemove.GetNumBytesRemoved() );

	// Verts must be aligned from hdr, length must be aligned from hdr
	size_t vhvNewVertOffset = vhvRemove.ComputeOffset( vhvHdr, vhvVertOffset );
	size_t vhvAlignedVertOffset = ALIGN_VALUE( vhvNewVertOffset, 4 );

	ITERATE_CHILDREN( HardwareVerts::MeshHeader_t, vhvMesh, vhvHdr, pMesh, m_nMeshes )
		vhvMesh->m_nOffset = vhvRemove.ComputeOffset( vhvHdr, vhvMesh->m_nOffset ) + vhvAlignedVertOffset - vhvNewVertOffset;
	ITERATE_END
	vhvHdr->m_nMeshes -= numMeshesRemoved;
	vhvHdr->m_nVertexes -= numVertsRemoved;

	// Remove the memory
	vhvRemove.MemMove( vhvHdr, vhvLength );	// All padding has been removed

	size_t numBytesNewLength = vhvLength + vhvAlignedVertOffset - vhvNewVertOffset;
	size_t numAlignedNewLength = ALIGN_VALUE( numBytesNewLength, 4 );

	// Now reinsert the padding
	CInsertionTracker vhvInsertPadding;
	vhvInsertPadding.InsertBytes( BYTE_OFF_PTR( vhvHdr, vhvNewVertOffset ), vhvAlignedVertOffset - vhvNewVertOffset );
	vhvInsertPadding.InsertBytes( BYTE_OFF_PTR( vhvHdr, vhvLength ), numAlignedNewLength - numBytesNewLength );
	
	vhvInsertPadding.Finalize();
	DLog( "mdllib", 3, " Inserted %d alignment bytes.\n", vhvInsertPadding.GetNumBytesInserted() );

	vhvInsertPadding.MemMove( vhvHdr, vhvLength );


	// Update the buffer length
	vhvBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vhvBuffer.TellGet() + vhvLength - vhvBuffer.TellPut() );

	DLog( "mdllib", 2, " Reduced vhv buffer by %d bytes.\n", vhvRemove.GetNumBytesRemoved() - vhvInsertPadding.GetNumBytesInserted() );

	// Done
	return true;
}

//
// StripModelBuffer
//	The main function that strips the mdl buffer
//		mdlBuffer		- mdl buffer, updated
//
bool CMdlStripInfo::StripModelBuffer( CUtlBuffer &mdlBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover mdl header
	//
	DECLARE_PTR( studiohdr_t, mdlHdr, BYTE_OFF_PTR( mdlBuffer.Base(), mdlBuffer.TellGet() ) );

	if ( mdlHdr->checksum != m_lChecksumOld )
	{
		DLog( "mdllib", 1, "ERROR: [StripModelBuffer] checksum mismatch!\n" );
		return false;
	}

	mdlHdr->checksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;


	Assert( m_eMode == MODE_STRIP_LOD_1N );


	//
	// Do the model buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;

	ITERATE_CHILDREN( mstudiobodyparts_t, mdlBodyPart, mdlHdr, pBodypart, numbodyparts )
		ITERATE_CHILDREN( mstudiomodel_t, mdlModel, mdlBodyPart, pModel, nummodels )
			
			DLog( "mdllib", 3, " Stripped %d vertexes (was: %d, now: %d).\n", mdlModel->numvertices - srcIndices.Count(), mdlModel->numvertices, srcIndices.Count() );

			mdlModel->numvertices = srcIndices.Count();

			ITERATE_CHILDREN( mstudiomesh_t, mdlMesh, mdlModel, pMesh, nummeshes )
				
				mdlMesh->numvertices = srcIndices.FindLess( mdlMesh->vertexoffset + mdlMesh->numvertices );
				mdlMesh->vertexoffset = srcIndices.FindLess( mdlMesh->vertexoffset ) + 1;
				mdlMesh->numvertices -= mdlMesh->vertexoffset - 1;

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

	return true;
}


//
// StripVertexDataBuffer
//	The main function that strips the vvd buffer
//		vvdBuffer		- vvd buffer, updated, size reduced
//
bool CMdlStripInfo::StripVertexDataBuffer( CUtlBuffer &vvdBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vvd header
	//
	DECLARE_PTR( vertexFileHeader_t, vvdHdr, BYTE_OFF_PTR( vvdBuffer.Base(), vvdBuffer.TellGet() ) );
	int vvdLength = vvdBuffer.TellPut() - vvdBuffer.TellGet();

	if ( vvdHdr->checksum != m_lChecksumOld )
	{
		DLog( "mdllib", 1, "ERROR: [StripVertexDataBuffer] checksum mismatch!\n" );
		return false;
	}

	vvdHdr->checksum = m_lChecksumNew;


	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;


	Assert( m_eMode == MODE_STRIP_LOD_1N );


	//
	// Do the vertex data buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;
	int mdlNumVerticesOld = vvdHdr->numLODVertexes[ 0 ];

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
	
	vvdBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vvdBuffer.TellGet() + vvdLength - vvdBuffer.TellPut() );

	DLog( "mdllib", 3, " Stripped %d vvd bytes.\n", vvdLengthOld - vvdLength );

	return true;
}

//
// StripOptimizedModelBuffer
//	The main function that strips the vtx buffer
//		vtxBuffer		- vtx buffer, updated, size reduced
//
bool CMdlStripInfo::StripOptimizedModelBuffer( CUtlBuffer &vtxBuffer )
{
	if ( m_eMode == MODE_UNINITIALIZED )
		return false;

	//
	// Recover vtx header
	//
	DECLARE_PTR( OptimizedModel::FileHeader_t, vtxHdr, BYTE_OFF_PTR( vtxBuffer.Base(), vtxBuffer.TellGet() ) );
	int vtxLength = vtxBuffer.TellPut() - vtxBuffer.TellGet();

	if ( vtxHdr->checkSum != m_lChecksumOld )
	{
		DLog( "mdllib", 1, "ERROR: [StripOptimizedModelBuffer] checksum mismatch!\n" );
		return false;
	}

	vtxHdr->checkSum = m_lChecksumNew;

	// No remapping required
	if ( m_eMode == MODE_NO_CHANGE )
		return true;

	Assert( m_eMode == MODE_STRIP_LOD_1N );


	//
	// Do the optimized model buffer stripping
	//

	CUtlSortVector< unsigned short, CLessSimple< unsigned short > > &srcIndices = m_vtxIndices;
	CUtlSortVector< CMdlStripInfo::MdlRangeItem, CLessSimple< CMdlStripInfo::MdlRangeItem > > &arrMdlOffsets = m_vtxMdlOffsets;

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
			int iMesh = arrMdlOffsets.FindLessOrEqual( MdlRangeItem( 0, 0, vtxVertexElement - vtxVertexBuffer ) );
			Assert( iMesh >= 0 && iMesh < arrMdlOffsets.Count() );
			
			MdlRangeItem &mri = arrMdlOffsets[ iMesh ];
			Assert( ( vtxVertexElement - vtxVertexBuffer >= mri.m_offNew ) && ( vtxVertexElement - vtxVertexBuffer < mri.m_offNew + mri.m_numNew ) );
			
			Assert( m_vtxVerts.IsBitSet( vtxVertexElement->origMeshVertID + mri.m_offOld ) );
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

	vtxBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, vtxBuffer.TellGet() + vtxLength - vtxBuffer.TellPut() );

	return true;
}





//////////////////////////////////////////////////////////////////////////
//
// Auxilliary methods
//
//////////////////////////////////////////////////////////////////////////

void CMdlStripInfo::DeleteThis()
{
	delete this;
}

void CMdlStripInfo::Reset()
{
	m_eMode = MODE_UNINITIALIZED;
	m_lChecksumOld = 0;
	m_lChecksumNew = 0;

	m_vtxVerts.Resize( 0 );
	m_vtxIndices.RemoveAll();
}

