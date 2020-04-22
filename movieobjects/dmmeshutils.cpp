//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Functions which do things to a DmeMesh
//
//=============================================================================


// Valve includes
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmmeshcomp.h"	// TODO: This has to be included before dmmeshutils.h
#include "movieobjects/dmmeshutils.h"
#include "tier1/utlstack.h"
#include "tier2/p4helpers.h"
#include "tier1/utlstring.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDmMeshUtils::RemoveLargeAxisAlignedPlanarFaces( CDmeMesh *pMesh )
{
	CDmeVertexData *pBase( pMesh->FindBaseState( "bind" ) );
	if ( !pBase )
		return false;

	const int posIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( posIndex < 0 )
		return false;

	const CUtlVector< Vector > &posData( CDmrArrayConst< Vector >( pBase->GetVertexData( posIndex ) ).Get() );
	if ( posData.Count() <= 0 )
		return false;

	const CUtlVector< int > &posIndices( CDmrArrayConst< int >( pBase->GetIndexData( posIndex ) ).Get() );
	if ( posIndices.Count() <= 0 )
		return false;

	bool bMeshChanged = false;

	CUtlVector< int > emptyFaceSets;

	int faceStartIndex = 0;
	int faceCurrentIndex = 0;

	int faceVertexCount = 0;

	bool bPlanarX = true;
	bool bPlanarY = true;
	bool bPlanarZ = true;

	Vector p;

	CUtlVector< int > removeStart;
	CUtlVector< int > removeCount;

	const int nFaceSets = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		const int nFaceIndices = pFaceSet->NumIndices();
		if ( nFaceIndices <= 0 )
			continue;

		faceStartIndex = 0;

		faceCurrentIndex = pFaceSet->GetIndex( 0 );
		if ( faceCurrentIndex < 0 )
			continue;

		faceVertexCount = 0;

		bPlanarX = true;
		bPlanarY = true;
		bPlanarZ = true;

		removeStart.RemoveAll();
		removeCount.RemoveAll();
		
		p = posData[ posIndices[ faceCurrentIndex ] ];

		for ( int j = 1; j < nFaceIndices; ++j )
		{
			faceCurrentIndex = pFaceSet->GetIndex( j );

			if ( faceCurrentIndex < 0 )
			{
				// End of a face

				if ( faceVertexCount > 4 && ( bPlanarX || bPlanarY || bPlanarZ ) )
				{
					removeStart.AddToTail( faceStartIndex );
					removeCount.AddToTail( j - faceStartIndex + 1 );
				}

				faceStartIndex = j + 1;

				if ( faceStartIndex < nFaceIndices )
				{
					p = posData[ posIndices[ pFaceSet->GetIndex( faceStartIndex ) ] ];
				}

				faceVertexCount = 0;

				bPlanarX = true;
				bPlanarY = true;
				bPlanarZ = true;

				continue;
			}

			Assert( faceCurrentIndex < posIndices.Count() );
			Assert( posIndices[ faceCurrentIndex ] < posData.Count() );
			const Vector &vPos = posData[ posIndices[ faceCurrentIndex ] ];

			if ( vPos.x != p.x )
				bPlanarX = false;

			if ( vPos.y != p.y )
				bPlanarY = false;

			if ( vPos.z != p.z )
				bPlanarZ = false;

			++faceVertexCount;
		}

		Assert( removeStart.Count() == removeCount.Count() );
		for ( int j = removeStart.Count() - 1; j >= 0; --j )
		{
			pFaceSet->RemoveMultiple( removeStart[ j ], removeCount[ j ] );
			bMeshChanged = true;
		}

		if ( pFaceSet->GetIndexCount() == 0 )
		{
			emptyFaceSets.AddToTail( i );
		}
	}

	for ( int i = emptyFaceSets.Count() - 1; i >= 0; --i )
	{
		pMesh->RemoveFaceSet( emptyFaceSets[ i ] );
		bMeshChanged = true;
	}

	if ( bMeshChanged )
	{
		PurgeUnusedData( pMesh );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::RemoveFacesWithMaterial( CDmeMesh *pMesh, const char *pMaterialName )
{
	bool bMeshChanged = false;

	CUtlVector< int > emptyFaceSets;

	const int nFaceSets = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		if ( !Q_strcmp( pFaceSet->GetMaterial()->GetMaterialName(), pMaterialName ) )
		{
			emptyFaceSets.AddToTail( i );
			bMeshChanged = true;
		}
	}

	for ( int i = emptyFaceSets.Count() - 1; i >= 0; --i )
	{
		pMesh->RemoveFaceSet( emptyFaceSets[ i ] );
		bMeshChanged = true;
	}

	if ( bMeshChanged )
	{
		PurgeUnusedData( pMesh );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::RemoveFacesWithMoreThanNVerts( CDmeMesh *pMesh, const int nVertexCount )
{
	CDmeVertexData *pBase( pMesh->FindBaseState( "bind" ) );
	if ( !pBase )
		return false;

	bool bMeshChanged = false;

	CUtlVector< int > emptyFaceSets;

	int faceStartIndex = 0;
	int faceCurrentIndex = 0;

	int faceVertexCount = 0;

	CUtlVector< int > removeStart;
	CUtlVector< int > removeCount;

	const int nFaceSets = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		const int nFaceIndices = pFaceSet->NumIndices();
		if ( nFaceIndices <= 0 )
			continue;

		faceStartIndex = 0;

		faceCurrentIndex = pFaceSet->GetIndex( 0 );
		if ( faceCurrentIndex < 0 )
			continue;

		faceVertexCount = 0;

		removeStart.RemoveAll();
		removeCount.RemoveAll();
		
		for ( int j = 1; j < nFaceIndices; ++j )
		{
			faceCurrentIndex = pFaceSet->GetIndex( j );

			if ( faceCurrentIndex < 0 )
			{
				// End of a face

				if ( faceVertexCount > nVertexCount )
				{
					removeStart.AddToTail( faceStartIndex );
					removeCount.AddToTail( j - faceStartIndex + 1 );
				}

				faceStartIndex = j + 1;

				faceVertexCount = 0;

				continue;
			}

			++faceVertexCount;
		}

		Assert( removeStart.Count() == removeCount.Count() );
		for ( int j = removeStart.Count() - 1; j >= 0; --j )
		{
			pFaceSet->RemoveMultiple( removeStart[ j ], removeCount[ j ] );
			bMeshChanged = true;
		}

		if ( pFaceSet->GetIndexCount() == 0 )
		{
			emptyFaceSets.AddToTail( i );
		}
	}

	for ( int i = emptyFaceSets.Count() - 1; i >= 0; --i )
	{
		pMesh->RemoveFaceSet( emptyFaceSets[ i ] );
		bMeshChanged = true;
	}

	if ( bMeshChanged )
	{
		PurgeUnusedData( pMesh );
		return true;
	}

	// Nothing remove
	return false;
}


//-----------------------------------------------------------------------------
// Figures out which vertexIndices are missing
// Returned list will be in sorted order
//-----------------------------------------------------------------------------
void ComputeVertexIndexMap( CDmeMesh *pMesh, int nMaxVertexCount, CUtlVector< int > &vertexIndexMap )
{
	bool *pVertexFound = reinterpret_cast< bool * >( alloca( nMaxVertexCount * sizeof( bool ) ) );
	memset( pVertexFound, 0, nMaxVertexCount * sizeof( bool ) );

	// Loop through all the face sets to find out the highest vertex index
	const int nFaceSetCount = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		const CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		const int nFaceSetIndices = pFaceSet->NumIndices();
		for ( int j = 0; j < nFaceSetIndices; ++j )
		{
			const int &nIndex = pFaceSet->GetIndex( j );
			if ( nIndex >= 0 )
			{
				Assert( nIndex < nMaxVertexCount );
				pVertexFound[ nIndex ] = true;
			}
		}
	}

	int nMissingCount = 0;
	for ( int i = 0; i < nMaxVertexCount; ++i )
	{
		if ( !pVertexFound[ i ] )
		{
			++nMissingCount;
		}
	}

	vertexIndexMap.SetSize( nMaxVertexCount );
	for ( int i = 0; i < nMaxVertexCount; ++i )
	{
		vertexIndexMap[ i ] = i;
	}

	for ( int i = nMaxVertexCount - 1; i >= 0; --i )
	{
		if ( !pVertexFound[ i ] )
		{
			vertexIndexMap.Remove( i );
		}
	}

	// Build up the reverse map
	int *pReverseVertexIndexMap = reinterpret_cast< int * >( alloca( nMaxVertexCount * sizeof( int ) ) );
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		pReverseVertexIndexMap[ i ] = -1;
	}

	for ( int i = vertexIndexMap.Count() - 1; i >= 0; --i )
	{
		pReverseVertexIndexMap[ vertexIndexMap[ i ] ] = i;
	}

	// Fix up the face set indices to compensate for the ones which are going to be removed
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		const int nFaceSetIndices = pFaceSet->NumIndices();
		for ( int j = 0; j < nFaceSetIndices; ++j )
		{
			const int &nIndex = pFaceSet->GetIndex( j );
			if ( nIndex >= 0 )
			{
				Assert( pReverseVertexIndexMap[ nIndex ] >= 0 );
				pFaceSet->SetIndex( j, pReverseVertexIndexMap[ nIndex ] );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the highest vertex index used by the face sets of the mesh + 1
//-----------------------------------------------------------------------------
int GetMaxVertexCount( const CDmeMesh *pMesh )
{
	int nMaxVertexIndex = 0;

	// Loop through all the face sets to find out the highest vertex index
	const int nFaceSetCount = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		const CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		const int nFaceSetIndices = pFaceSet->NumIndices();
		for ( int j = 0; j < nFaceSetIndices; ++j )
		{
			const int &nIndex = pFaceSet->GetIndex( j );

			if ( nIndex > nMaxVertexIndex )
			{
				nMaxVertexIndex = nIndex;
			}
		}
	}

	return nMaxVertexIndex + 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void RemapData(
	CDmrArray< T_t > data,
	const CUtlVector< int > &newToOldMap )
{
	const int nNewToOldMapCount = newToOldMap.Count();

	T_t *pNewData = reinterpret_cast< T_t * >( alloca( nNewToOldMapCount * sizeof( T_t ) ) );
	for ( int i = 0; i < nNewToOldMapCount; ++i )
	{
		pNewData[ i ] = data.Get( newToOldMap[ i ] );
	}

	data.RemoveMultiple( nNewToOldMapCount, data.Count() - nNewToOldMapCount );
	data.SetMultiple( 0, nNewToOldMapCount, pNewData );
}


//-----------------------------------------------------------------------------
// Computes the map of new data indices to old data indices
//-----------------------------------------------------------------------------
void RemoveUnusedData(
	CDmeMesh *pMesh,
	CDmeVertexData *pVertexData,
	bool bBind,
	const char *pFieldName,
	int *pIndices,
	int nIndicesCount,
	CDmrGenericArray &data )
{
	const int nDataCount = data.Count();

	bool *pDataIndexFound = reinterpret_cast< bool * >( alloca( nDataCount * sizeof( bool ) ) );
	memset( pDataIndexFound, 0, nDataCount * sizeof( bool ) );

	// Figure out which data is used
	for ( int i = 0; i < nIndicesCount; ++i )
	{
		Assert( pIndices[ i ] >= 0 && pIndices[ i ] < nDataCount );
		pDataIndexFound[ pIndices[ i ] ] = true;
	}

	int nMissingCount = 0;
	for ( int i = 0; i < nDataCount; ++i )
	{
		if ( !pDataIndexFound[ i ] )
		{
			++nMissingCount;
		}
	}

	// Compute the New to Old data map
	CUtlVector< int > newToOldDataMap;
	newToOldDataMap.SetSize( nDataCount );
	for ( int i = 0; i < nDataCount; ++i )
	{
		newToOldDataMap[ i ] = i;
	}

	for ( int i = nDataCount - 1; i >= 0; --i )
	{
		if ( !pDataIndexFound[ i ] )
		{
			newToOldDataMap.Remove( i );
		}
	}

	// Fix up the data
	CDmAttribute *pDataAttr = data.GetAttribute();
	const DmAttributeType_t dataAttrType = pDataAttr->GetType();
	switch ( dataAttrType )
	{
	case AT_FLOAT_ARRAY:
		RemapData( CDmrArray< float >( pDataAttr ), newToOldDataMap );
		break;
	case AT_VECTOR2_ARRAY:
		RemapData( CDmrArray< Vector2D >( pDataAttr ), newToOldDataMap );
		break;
	case AT_VECTOR3_ARRAY:
		RemapData( CDmrArray< Vector >( pDataAttr ), newToOldDataMap );
		break;
	case AT_VECTOR4_ARRAY:
		RemapData( CDmrArray< Vector4D >( pDataAttr ), newToOldDataMap );
		break;
	case AT_QUATERNION_ARRAY:
		RemapData( CDmrArray< Quaternion >( pDataAttr ), newToOldDataMap );
		break;
	case AT_COLOR_ARRAY:
		RemapData( CDmrArray< Color >( pDataAttr ), newToOldDataMap );
		break;
	default:
		Assert( 0 );
		break;
	}

	// Compute Old To New Data Map
	int *pOldToNewDataMap = reinterpret_cast< int * >( alloca( nDataCount * sizeof( int ) ) );
	for ( int i = 0; i < nDataCount; ++i )
	{
		pOldToNewDataMap[ i ] = -1;
	}

	for ( int i = newToOldDataMap.Count() - 1; i >= 0; --i )
	{
		pOldToNewDataMap[ newToOldDataMap[ i ] ] = i;
	}

	// Fix up the indices
	for ( int i = 0; i < nIndicesCount; ++i )
	{
		pIndices[ i ] = pOldToNewDataMap[ pIndices[ i ] ];
	}

	// TODO: Fix up "jointWeight & "jointIndices" if this is "position"
	if ( !Q_strcmp( pFieldName, "position" ) )
	{
		const int nFields = pVertexData->FieldCount();
		for ( int i = 0; i < nFields; ++i )
		{

		}
	}

	// If this is the bind state then fix up any delta states
	if ( !bBind )
		return;

	// Fix up any Delta states
	const int nDeltaStateCount = pMesh->DeltaStateCount();
	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		CDmeVertexDeltaData *pDelta = pMesh->GetDeltaState( i );
		const int nDeltaFieldCount = pDelta->FieldCount();
		for ( int j = 0; j < nDeltaFieldCount; ++j )
		{
			if ( !Q_strcmp( pFieldName, pDelta->FieldName( j ) ) )
			{
				CDmrArray< int > deltaIndices = pDelta->GetIndexData( j );
				CDmrGenericArray deltaData = pDelta->GetVertexData( j );
				Assert( deltaIndices.Count() == deltaData.Count() );

				for ( int k = deltaIndices.Count() - 1; k >= 0; --k )
				{
					const int oldIndex = deltaIndices.Get( k );
					const int &newIndex = pOldToNewDataMap[ oldIndex ];
					if ( newIndex < 0 )
					{
						deltaIndices.Remove( k );
						deltaData.Remove( k );
					}
					else if ( newIndex != oldIndex )
					{
						deltaIndices.Set( k, newIndex );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void RemoveUnusedVerticesFromBaseState(
	CDmeMesh *pMesh,
	CDmeVertexData *pVertexData,
	const CUtlVector< int > &newToOldIndexMap )
{
	const int nNewToOldIndexMapCount = newToOldIndexMap.Count();
	int *pNewVertexIndices = reinterpret_cast< int * >( alloca( nNewToOldIndexMapCount * sizeof( int ) ) );

	// See if this is the bind state for the mesh
	const bool bBind = !Q_strcmp( pVertexData->GetName(), "bind" );

	const int nFieldCount = pVertexData->FieldCount();
	for ( int i = 0; i < nFieldCount; ++i )
	{
		const char *pFieldName = pVertexData->FieldName( i );
		// TODO: Checking by name is lame... should be a lookup to map fieldIndex to a standard field index
		if ( !Q_strcmp( pFieldName, "jointWeights" ) || !Q_strcmp( pFieldName, "jointIndices" ) )
		{
			// TODO: Handle when positions are Remapped
			continue;
		}

		CDmrArray< int > indices = pVertexData->GetIndexData( i );

		// Create the new index array accounting for missing indices
		for ( int j = 0; j < nNewToOldIndexMapCount; ++j )
		{
			Assert( newToOldIndexMap[ j ] < indices.Count() );
			pNewVertexIndices[ j ] = indices.Get( newToOldIndexMap[ j ] );
		}

		CDmrGenericArray data = pVertexData->GetVertexData( i );

		// This will also update pNewVertexIndices
		RemoveUnusedData( pMesh, pVertexData, bBind, pFieldName, pNewVertexIndices, nNewToOldIndexMapCount, CDmrGenericArray( pVertexData->GetVertexData( i ) ) );

		// Shrink the indices array
		indices.RemoveMultiple( nNewToOldIndexMapCount, indices.Count() - nNewToOldIndexMapCount );

		// Set the new index values
		indices.SetMultiple( 0, nNewToOldIndexMapCount, pNewVertexIndices );
	}

	// Update the vertex count
	pVertexData->Resolve();
}


//-----------------------------------------------------------------------------
// Removes unused data from the mesh
// Unused means a 'vertex' that isn't referred to by any face
// Once all unused vertices are removed, unused data is removed from each
// bit of data
// TODO: Also loop through each field of data, see which ones are no longer
//       being referred to and then purge the data as well
//       Would also have to purge delta data at the same time
//       Would also have to purge joints at the same time (for position)
//-----------------------------------------------------------------------------
bool CDmMeshUtils::PurgeUnusedData( CDmeMesh *pMesh )
{
	// Get the maximum vertex index of the mesh
	const int nMaxVertexCount = GetMaxVertexCount( pMesh );

	// Now find any missing indices
	CUtlVector< int > vertexIndexMap;
	ComputeVertexIndexMap( pMesh, nMaxVertexCount, vertexIndexMap );

	// Remove the redundant vertices from all base states 
	for ( int i = pMesh->BaseStateCount() - 1; i >= 0; --i )
	{
		RemoveUnusedVerticesFromBaseState( pMesh, pMesh->GetBaseState( i ), vertexIndexMap );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::Mirror( CDmeMesh *pMesh, int axis /*= kXAxis */ )
{
	CDmeVertexData *pBase( pMesh->FindBaseState( "bind" ) );
	if ( !pBase )
		return false;

	CUtlVector< int > mirrorMap;
	if ( !MirrorVertices( pMesh, pBase, axis, mirrorMap ) )
		return false;

	int vertexIndex;
	int faceStart = 0;

	CUtlVector< int > newFaceIndices;

	const int nFaceSets = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pSrcFaceSet = pMesh->GetFaceSet( i );
		const int nFaceSetIndices = pSrcFaceSet->NumIndices();
		if ( nFaceSetIndices <= 0 )
			continue;

		CDmeFaceSet *pDstFaceSet = pSrcFaceSet;

		// See if a new face set needs to be created

		CDmeMaterial *pSrcMaterial = pSrcFaceSet->GetMaterial();
		const char *pSrcMaterialName = pSrcMaterial->GetMaterialName();
		const int nNameLen = Q_strlen( pSrcMaterialName );
		if ( nNameLen >= 2 )
		{
			CUtlString materialName;

			if ( !Q_stricmp( pSrcMaterialName + nNameLen - 2, "_l" ) )
			{
				materialName = pSrcMaterialName;
				materialName.SetLength( nNameLen - 2 );
				materialName += "_r";
			}
			else if ( !Q_stricmp( pSrcMaterialName + nNameLen - 2, "_r" ) )
			{
				materialName = pSrcMaterialName;
				materialName.SetLength( nNameLen - 2 );
				materialName += "_l";
			}
			else if ( nNameLen >= 5 && !Q_stricmp( pSrcMaterialName + nNameLen - 5, "_left" ) )
			{
				materialName = pSrcMaterialName;
				materialName.SetLength( nNameLen - 5 );
				materialName += "_right";
			}
			else if ( nNameLen >= 6 && !Q_stricmp( pSrcMaterialName + nNameLen - 6, "_right" ) )
			{
				materialName = pSrcMaterialName;
				materialName.SetLength( nNameLen - 6 );
				materialName += "_left";
			}

			if ( materialName.Length() )
			{
				pDstFaceSet = CreateElement< CDmeFaceSet >( materialName, pMesh->GetFileId() );
				CDmeMaterial *pDstMaterial = CreateElement< CDmeMaterial >( materialName, pDstFaceSet->GetFileId() );
				pDstMaterial->SetMaterial( materialName );
				pDstFaceSet->SetMaterial( pDstMaterial );
				pMesh->AddFaceSet( pDstFaceSet );
			}
		}

		faceStart = 0;

		for ( int j = 0; j < nFaceSetIndices; ++j )
		{
			vertexIndex = pSrcFaceSet->GetIndex( j );

			if ( vertexIndex < 0 )
			{
				newFaceIndices.RemoveAll();

				for ( int k = j - 1; k >= faceStart; --k )
				{
					newFaceIndices.AddToTail( mirrorMap[ pSrcFaceSet->GetIndex( k ) ] );
				}
				newFaceIndices.AddToTail( -1 );

				const int oldNumIndices = pDstFaceSet->NumIndices();

				pDstFaceSet->AddIndices( newFaceIndices.Count() );
				pDstFaceSet->SetIndices( oldNumIndices, newFaceIndices.Count(), newFaceIndices.Base() );

				// End of face
				faceStart = j + 1;
				continue;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Initializes the CUtlVector to a linear ramp where utlVector[ i ] == i
//-----------------------------------------------------------------------------
template < typename T_t >
void RampInit( CUtlVector< T_t > &utlVector, const int nCount )
{
	utlVector.SetCount( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		utlVector[ i ] = i;
	}
}


//-----------------------------------------------------------------------------
// Build Data Mirror Map
// Returns a pointer to the memory holding the indices for the map or NULL
//-----------------------------------------------------------------------------
const int *CDmMeshUtils::BuildDataMirrorMap( CDmeVertexData *pBase, int axis, CDmeVertexData::StandardFields_t standardField, CUtlVector< int > &dataMirrorMap )
{
	const FieldIndex_t fieldIndex = pBase->FindFieldIndex( standardField );
	if ( fieldIndex < 0 )
		return NULL;

	const CUtlVector< int > &indices( CDmrArrayConst< int >( pBase->GetIndexData( fieldIndex ) ).Get() );

	CDmAttribute *pData = pBase->GetVertexData( fieldIndex );
	if ( standardField == CDmeVertexData::FIELD_POSITION || standardField == CDmeVertexData::FIELD_NORMAL )
	{
		const Vector mirrorOrigin( 0.0f, 0.0f, 0.0f );
		const float mirrorAxisVal = mirrorOrigin[ axis ];

		CDmrArray< Vector > data( pBase->GetVertexData( fieldIndex ) );
		Vector v;

		const int nDataCount = data.Count();
		dataMirrorMap.SetCount( nDataCount );

		int nMirrorDataCount = nDataCount;
		for ( int i = 0; i < nDataCount; ++i )
		{
			if ( fabs( data[ i ][ axis ] - mirrorAxisVal ) > FLT_EPSILON * 1000.0f )
			{
				dataMirrorMap[ i ] = nMirrorDataCount++;
			}
			else
			{
				dataMirrorMap[ i ] = i;
				v = data[ i ];
				v[ axis ] = mirrorOrigin[ axis ];
				data.Set( i, v );
			}
		}
	}
	else if ( standardField == CDmeVertexData::FIELD_TEXCOORD )
	{
		const Vector2D mirrorOrigin( 0.5f, 0.5f );
		const float mirrorAxisVal = mirrorOrigin[ axis % 2 ];

		const CUtlVector< Vector2D > &data( CDmrArrayConst< Vector2D >( pBase->GetVertexData( fieldIndex ) ).Get() );
		const int nDataCount = data.Count();
		dataMirrorMap.SetCount( nDataCount );

		int nMirrorDataCount = nDataCount;
		for ( int i = 0; i < nDataCount; ++i )
		{
			if ( fabs( data[ i ][ axis ] - mirrorAxisVal ) > FLT_EPSILON * 1000.0f )
			{
				dataMirrorMap[ i ] = nMirrorDataCount++;
			}
			else
			{
				dataMirrorMap[ i ] = i;
			}
		}
	}
	else
	{
		RampInit( dataMirrorMap, CDmrGenericArrayConst( pData ).Count() );
	}

	return indices.Base();
}


//-----------------------------------------------------------------------------
// y = mirrorMap[ x ] means that if y < 0 then original position x is not
// mirrored.  Otherwise y is the index into the vertex indices of the mirrored
// version of vertex 
//-----------------------------------------------------------------------------
bool CDmMeshUtils::MirrorVertices( CDmeMesh *pMesh, CDmeVertexData *pBase, int axis, CUtlVector< int > &mirrorMap )
{
	mirrorMap.RemoveAll();

	if ( !pMesh || !pBase || axis < kXAxis || axis > kZAxis )
		return false;

	const int posIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( posIndex < 0 )
		return false;

	const CUtlVector< int > &posIndices( CDmrArrayConst< int >( pBase->GetIndexData( posIndex ) ).Get() );
	const int nIndices = posIndices.Count();
	Assert( nIndices == pBase->VertexCount() );
	CUtlVector< int > posMirrorMap;

	if ( !BuildDataMirrorMap( pBase, axis, CDmeVertexData::FIELD_POSITION, posMirrorMap ) )
		return false;

	CUtlVector< int > normalMirrorMap;
	const int *pNormalIndices = BuildDataMirrorMap( pBase, axis, CDmeVertexData::FIELD_NORMAL, normalMirrorMap );

	CUtlVector< int > uvMirrorMap;
	const int *pUVIndices = BuildDataMirrorMap( pBase, axis, CDmeVertexData::FIELD_TEXCOORD, uvMirrorMap );

	RampInit( mirrorMap, nIndices );
	int mirrorCount = 0;
	{
		bool mirror;

		Vector tmpVec;
		Vector2D tmpVec2D;

		for ( int i = 0; i < nIndices; ++i )
		{
			mirror = false;

			if ( posMirrorMap[ posIndices[ i ] ] != posIndices[ i ] )
			{
				mirror = true;
			}

			if ( pNormalIndices && normalMirrorMap[ pNormalIndices[ i ] ] != pNormalIndices[ i ] )
			{
				mirror = true;
			}

			if ( pUVIndices && uvMirrorMap[ pUVIndices[ i ] ] != pUVIndices[ i ] )
			{
				mirror = true;
			}

			if ( mirror )
			{
				mirrorMap[ i ] = nIndices + mirrorCount;
				++mirrorCount;
			}
		}
	}

	const int nBaseState = pMesh->BaseStateCount();
	for ( int i = 0; i < nBaseState; ++i )
	{
		pBase = pMesh->GetBaseState( i );
		const int nVertexCount = pBase->VertexCount();
		MirrorVertices( pBase, axis, nVertexCount, mirrorCount, mirrorMap, posMirrorMap, normalMirrorMap, uvMirrorMap );
	}

	const int nDeltaState = pMesh->DeltaStateCount();
	for ( int i = 0; i < nDeltaState; ++i )
	{
		CDmeVertexDeltaData *pDelta = pMesh->GetDeltaState( i );
		MirrorDelta( pDelta, axis, posMirrorMap, normalMirrorMap, uvMirrorMap );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
inline void MirrorData( Vector &d, const int &axis )
{
	d[ axis ] *= -1.0f;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
inline void MirrorData( Vector2D &d, const int &axis )
{
	d[ axis ] = ( d[ axis ] - 0.5f ) * -1.0f + 0.5f;
}


//-----------------------------------------------------------------------------
// Mirror 3D things like positions & normals
//-----------------------------------------------------------------------------
template < class T_t >
void MirrorVertexData(
	CDmeVertexData *pBase,
	FieldIndex_t fieldIndex,
	int axis,
	int nOrigVertexCount,
	int nMirrorCount,
	const CDmrArrayConst< T_t > &origData,
	const CUtlVector< int > &origIndices,
	const CUtlVector< int > &mirrorMap,
	const CUtlVector< int > &dataMirrorMap )
{
	if ( nMirrorCount <= 0 )
		return;

	Assert( origIndices.Count() == nOrigVertexCount + nMirrorCount );
	Assert( mirrorMap.Count() == nOrigVertexCount );
	Assert( dataMirrorMap.Count() == origData.Count() );

	const int nData = origData.Count();
	T_t *pMirrorData = reinterpret_cast< T_t * >( alloca( nMirrorCount * sizeof( T_t ) ) );
	int *pMirrorIndices = reinterpret_cast< int * >( alloca( nMirrorCount * sizeof( int ) ) );

	T_t mirrorData;
	int nMirrorIndex = 0;
	int nMirrorDataCount = -1;
	for ( int i = 0; i < nOrigVertexCount; ++i )
	{
		if ( mirrorMap[ i ] != i )
		{
			// Vertex must be mirrored

			if ( dataMirrorMap[ origIndices[ i ] ] != origIndices[ i ] )
			{
				// Data referred to by vertex i must be mirror (this may be done a redundant number of times)
				const T_t &origDataRef( origData[ origIndices[ i ] ] );
				mirrorData = origDataRef;
				MirrorData( mirrorData, axis );
				pMirrorData[ dataMirrorMap[ origIndices[ i ] ] - nData ] = mirrorData;
				if ( ( dataMirrorMap[ origIndices[ i ] ] - nData ) > nMirrorDataCount )
				{
					nMirrorDataCount = dataMirrorMap[ origIndices[ i ] ] - nData;
				}
				pMirrorIndices[ nMirrorIndex ] = dataMirrorMap[ origIndices[ i ] ];
			}
			else
			{
				// The data does not need to be mirrored
				pMirrorIndices[ nMirrorIndex ] = origIndices[ i ];
			}

			++nMirrorIndex;
		}
		else
		{
			Assert( dataMirrorMap[ origIndices[ i ] ] == origIndices[ i ] );
		}
	}
	++nMirrorDataCount;

	Assert( nMirrorCount == nMirrorIndex );
	Assert( nMirrorDataCount <= nMirrorCount );

	const DmAttributeType_t dmAttributeType = ArrayTypeToValueType( origData.GetAttribute()->GetType() );

	pBase->AddVertexData( fieldIndex, nMirrorDataCount );
	pBase->SetVertexData( fieldIndex, nData, nMirrorDataCount, dmAttributeType, pMirrorData );

	pBase->SetVertexIndices( fieldIndex, nOrigVertexCount, nMirrorCount, pMirrorIndices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::MirrorVertices(
	CDmeVertexData *pBase,
	int axis,
	int nOldVertexCount,
	int nMirrorCount,
	const CUtlVector< int > &mirrorMap,
	const CUtlVector< int > &posMirrorMap,
	const CUtlVector< int > &normalMirrorMap,
	const CUtlVector< int > &uvMirrorMap )
{
	if ( !pBase || axis < kXAxis || axis > kZAxis )
		return false;

	pBase->AddVertexIndices( nMirrorCount );

	const int posFieldIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	const int normalFieldIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	const int uvFieldIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );

	const int nFields = pBase->FieldCount();
	for ( int i = 0; i < nFields; ++i )
	{
		CDmAttribute *pBaseData( pBase->GetVertexData( i ) );
		const CUtlVector< int > &baseIndices( pBase->GetVertexIndexData( i ) );
		Assert( baseIndices.Count() == nOldVertexCount + nMirrorCount );
		Assert( mirrorMap.Count() == nOldVertexCount );

		switch ( pBaseData->GetType() )
		{
		case AT_VECTOR2_ARRAY:
			if ( i == uvFieldIndex )
			{
				MirrorVertexData( pBase, i, axis % 2, nOldVertexCount, nMirrorCount, CDmrArrayConst< Vector2D >( pBaseData ), baseIndices, mirrorMap, uvMirrorMap );
				continue;
			}
			break;
		case AT_VECTOR3_ARRAY:
			if ( i == posFieldIndex )
			{
				MirrorVertexData( pBase, i, axis, nOldVertexCount, nMirrorCount, CDmrArrayConst< Vector >( pBaseData ), baseIndices, mirrorMap, posMirrorMap );
				continue;
			}
			else if ( i == normalFieldIndex )
			{
				MirrorVertexData( pBase, i, axis, nOldVertexCount, nMirrorCount, CDmrArrayConst< Vector >( pBaseData ), baseIndices, mirrorMap, normalMirrorMap );
				continue;
			}
			break;
		default:
			break;
		}

		MirrorVertices( pBase, i, nOldVertexCount, nMirrorCount, baseIndices, mirrorMap );
	}

	return true;
}


//-----------------------------------------------------------------------------
// This does the default case of mirroring which is no mirroring at all!
// No data is changed, the extra indices are added to the index
//-----------------------------------------------------------------------------
void CDmMeshUtils::MirrorVertices(
	CDmeVertexData *pBase,
	FieldIndex_t fieldIndex,
	int nOldVertexCount,
	int nMirrorCount,
	const CUtlVector< int > &baseIndices,
	const CUtlVector< int > &mirrorMap )
{
	if ( nMirrorCount <= 0 )
		return;

	Assert( baseIndices.Count() == nOldVertexCount + nMirrorCount );
	Assert( mirrorMap.Count() == nOldVertexCount );
	int *pIndices = reinterpret_cast< int * >( alloca( nMirrorCount * sizeof( int ) ) );

	{
		int pIndex = 0;
		for ( int i = 0; i < nOldVertexCount; ++i )
		{
			if ( mirrorMap[ i ] != i )
			{
				pIndices[ pIndex ] = baseIndices[ mirrorMap[ i ] - nOldVertexCount ];
				++pIndex;
			}
		}
		Assert( pIndex == nMirrorCount );
	}

	pBase->SetVertexIndices( fieldIndex, nOldVertexCount, nMirrorCount, pIndices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void MirrorDeltaData(
	CDmeVertexDeltaData *pDelta,
	FieldIndex_t fieldIndex,
	int axis,
	const CDmrArrayConst< T_t > &origData,
	const CUtlVector< int > &origIndices,
	const CUtlVector< int > &dataMap )
{
	Assert( origData.Count() == origIndices.Count() );

	const int nOrigDataCount = origData.Count();

	T_t *pMirrorData = reinterpret_cast< T_t * >( alloca( nOrigDataCount * sizeof( T_t ) ) );
	int *pMirrorIndices = reinterpret_cast< int * >( alloca( nOrigDataCount * sizeof( int ) ) );

	int nMirrorDataCount = 0;
	for ( int i = 0; i < nOrigDataCount; ++i )
	{
		if ( dataMap[ origIndices[ i ] ] != origIndices[ i ] )
		{
			pMirrorData[ nMirrorDataCount ] = origData[ i ];
			MirrorData( pMirrorData[ nMirrorDataCount ], axis );
			pMirrorIndices[ nMirrorDataCount ] = dataMap[ origIndices[ i ] ];
			++nMirrorDataCount;
		}
	}

	const DmAttributeType_t dmAttributeType = ArrayTypeToValueType( origData.GetAttribute()->GetType() );

	pDelta->AddVertexData( fieldIndex, nMirrorDataCount );
	pDelta->SetVertexData( fieldIndex, nOrigDataCount, nMirrorDataCount, dmAttributeType, pMirrorData );
	pDelta->SetVertexIndices( fieldIndex, nOrigDataCount, nMirrorDataCount, pMirrorIndices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::MirrorDelta(
	CDmeVertexDeltaData *pDelta,
	int axis,
	const CUtlVector< int > &posMirrorMap,
	const CUtlVector< int > &normalMirrorMap,
	const CUtlVector< int > &uvMirrorMap )
{
	if ( !pDelta || axis < kXAxis || axis > kZAxis )
		return false;

	const int posFieldIndex = pDelta->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	const int normalFieldIndex = pDelta->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	const int uvFieldIndex = pDelta->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );

	const int nFields = pDelta->FieldCount();
	for ( int i = 0; i < nFields; ++i )
	{
		CDmAttribute *pDeltaData( pDelta->GetVertexData( i ) );
		const CUtlVector< int > &deltaIndices( pDelta->GetVertexIndexData( i ) );

		switch ( pDeltaData->GetType() )
		{
		case AT_VECTOR2_ARRAY:
			if ( i == uvFieldIndex )
			{
				MirrorDeltaData( pDelta, i, axis % 2, CDmrArrayConst< Vector2D >( pDeltaData ), deltaIndices, uvMirrorMap );
				continue;
			}
			break;
		case AT_VECTOR3_ARRAY:
			if ( i == posFieldIndex )
			{
				MirrorDeltaData( pDelta, i, axis, CDmrArrayConst< Vector >( pDeltaData ), deltaIndices, posMirrorMap );
				continue;
			}
			else if ( i == normalFieldIndex )
			{
				MirrorDeltaData( pDelta, i, axis, CDmrArrayConst< Vector >( pDeltaData ), deltaIndices, normalMirrorMap );
				continue;
			}
			break;
		default:
			break;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Finds all materials bound to the mesh and replaces ones which match the
// source name with the destination name
//-----------------------------------------------------------------------------
bool CDmMeshUtils::RemapMaterial( CDmeMesh *pMesh, const CUtlString &src, const CUtlString &dst )
{
	bool retVal = false;

	char srcName[ MAX_PATH ];
	char matName[ MAX_PATH ];
	char dstName[ MAX_PATH ];

	Q_StripExtension( src.Get(), srcName, sizeof( srcName ) );
	Q_FixSlashes( srcName, '/' );

	Q_strncpy( dstName, dst.Get(), sizeof( dstName ) );
	Q_FixSlashes( dstName, '/' );

	const int nFaceSets = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		if ( !pFaceSet )
			continue;

		CDmeMaterial *pMaterial = pFaceSet->GetMaterial();
		if ( !pMaterial )
			continue;

		const char *pMaterialName = pMaterial->GetMaterialName();
		Q_StripExtension( pMaterialName, matName, sizeof( matName ) );
		Q_FixSlashes( matName, '/' );

		// TODO: Regular expressions or at least glob style matching would be cool
		if ( !Q_stricmp( srcName, matName ) )
		{
			pMaterial->SetMaterial( dstName );
			pMaterial->SetName( dstName );
			retVal = true;
		}
	}

	return retVal;
}


//-----------------------------------------------------------------------------
// Replaces the nth material found with the specified material name
//-----------------------------------------------------------------------------
bool CDmMeshUtils::RemapMaterial( CDmeMesh *pMesh, const int nMaterialIndex, const CUtlString &dst )
{
	const int nFaceSets = pMesh->FaceSetCount();
	if ( nMaterialIndex >= nFaceSets )
		return false;

	CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( nMaterialIndex );
	if ( !pFaceSet )
		return false;

	CDmeMaterial *pMaterial = pFaceSet->GetMaterial();
	if ( !pMaterial )
		return false;

	pMaterial->SetMaterial( dst );
	pMaterial->SetName( dst );

	return true;
}


//-----------------------------------------------------------------------------
// Finds the "socket" on which to base the mesh merge
// This is defined as the vertices along the two meshes
// Returns the index into srcBorderEdgesList of the edge list that is found
// -1 if not found
//-----------------------------------------------------------------------------
int CDmMeshUtils::FindMergeSocket(
	const CUtlVector< CUtlVector< CDmMeshComp::CEdge * > > &srcBorderEdgesList,
	CDmeMesh *pDstMesh )
{
	CDmMeshComp dstComp( pDstMesh );

	const CUtlVector< CDmMeshComp::CEdge * > &edgeList = dstComp.m_edges;

	for ( int i = srcBorderEdgesList.Count() - 1; i >= 0; --i )
	{
		const CUtlVector< CDmMeshComp::CEdge * > &srcBorderEdges = srcBorderEdgesList[ i ];

		int nEdgeMatch = 0;

		for ( int j = 0; j != edgeList.Count(); j++ )
		{
			const CDmMeshComp::CEdge &e = *edgeList[ j ];

			for ( int k = srcBorderEdges.Count() - 1; k >= 0; --k )
			{
				if ( e == *srcBorderEdges[ k ] )
				{
					++nEdgeMatch;
					break;
				}
			}
		}

		if ( nEdgeMatch == srcBorderEdges.Count() )
		{
			return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Merge by finding the two meshes in the scene which are joined at a socket
// A socket being defined as a group of border edges that match exactly
// between two meshes
//-----------------------------------------------------------------------------
bool CDmMeshUtils::Merge( CDmeMesh *pSrcMesh, CDmElement *pRoot )
{
	CDmMeshComp srcComp( pSrcMesh );

	CUtlVector< CUtlVector< CDmMeshComp::CEdge * > > srcBorderEdgesList;
	if ( srcComp.GetBorderEdges( srcBorderEdgesList ) == 0 )
		return false;

	CDmeMesh *pDstMesh = NULL;

	// Find each mesh under pRoot
	CDmeDag *pModel = pRoot->GetValueElement< CDmeDag >( "model" );
	if ( !pModel )
		return false;

	CUtlStack< CDmeDag * > traverseStack;
	traverseStack.Push( pModel );

	CDmeDag *pDag;
	CDmeMesh *pMesh;

	Vector srcCenter;
	float srcRadius;

	Vector dstCenter;
	float dstRadius;

	float sqDist = FLT_MAX;
	pSrcMesh->GetBoundingSphere( srcCenter, srcRadius );

	int nEdgeListIndex = -1;

	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDag );
		if ( !pDag )
			continue;

		// Push all children onto stack in reverse order
		for ( int nChildIndex = pDag->GetChildCount() - 1; nChildIndex >= 0; --nChildIndex )
		{
			traverseStack.Push( pDag->GetChild( nChildIndex ) );
		}

		// See if there's a mesh associated with this dag
		pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
		if ( !pMesh )
			continue;

		int eli = FindMergeSocket( srcBorderEdgesList, pMesh );
		if ( eli < 0 )
			continue;

		pMesh->GetBoundingSphere( dstCenter, dstRadius );
		dstRadius = dstCenter.DistToSqr( srcCenter );

		if ( dstRadius < sqDist )
		{
			sqDist = dstRadius;
			pDstMesh = pMesh;
			nEdgeListIndex = eli;
		}
	}

	if ( pDstMesh )
	{
		return Merge( srcComp, srcBorderEdgesList[ nEdgeListIndex ], pDstMesh );
	}

	Msg( "Error: Merge() - No Merge Socket Found - i.e. A Set Of Border Edges On The Source Model That Are Found On The Merge Model" );

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void AppendData(
	const CDmrArrayConst< T_t > &srcData,
	CDmrArray< T_t > &dstData,
	const matrix3x4_t *pMat = NULL )
{
	const int nSrcCount = srcData.Count();
	const int nDstCount = dstData.Count();

	dstData.AddMultipleToTail( nSrcCount );
	dstData.SetMultiple( nDstCount, nSrcCount, srcData.Base() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void AppendData(
	const CDmrArrayConst< Vector > &srcData,
	CDmrArray< Vector > &dstData,
	const matrix3x4_t *pMat )
{
	const int nSrcCount = srcData.Count();
	const int nDstCount = dstData.Count();

	dstData.AddMultipleToTail( nSrcCount );

	if ( pMat )
	{
		Vector v;
		for ( int i = 0; i < nSrcCount; ++i )
		{
			v = srcData.Get( i );
			VectorTransform( srcData.Get( i ), *pMat, v );
			dstData.Set( nDstCount + i, v );
		}
	}
	else
	{
		dstData.SetMultiple( nDstCount, nSrcCount, srcData.Base() );
	}
}


//-----------------------------------------------------------------------------
// Merge data from a base state on one DmeMesh into another DmeMesh
// Preserve positions and normals by transforming them with the
// positionMatrix & normalMatrix
//
// Return the number of new vertices in the mesh
//-----------------------------------------------------------------------------
int MergeBaseState(
	CDmeVertexData *pSrcBase,
	CDmeVertexData *pDstBase,
	const matrix3x4_t &pMat,
	const matrix3x4_t &nMat,
	int nSkinningJointIndex,
	int &nPositionOffset,
	int &nNormalOffset,
	int &nWrinkleOffset )
{
	int nRetVal = -1;

	const int nSrcPositionIndex = pSrcBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	const int nSrcNormalIndex = pSrcBase->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	const int nSrcWrinkleIndex = pSrcBase->FindFieldIndex( CDmeVertexData::FIELD_WRINKLE );
	const int nSrcJointWeightsIndex = pSrcBase->FindFieldIndex( CDmeVertexData::FIELD_JOINT_WEIGHTS );
	const int nSrcJointIndicesIndex = pSrcBase->FindFieldIndex( CDmeVertexData::FIELD_JOINT_INDICES );

	const int nDstJointWeightsIndex = pDstBase->FindFieldIndex( CDmeVertexData::FIELD_JOINT_WEIGHTS );
	const int nDstJointIndicesIndex = pDstBase->FindFieldIndex( CDmeVertexData::FIELD_JOINT_INDICES );

	// Handle skinning the new mesh data to a single joint if the destination mesh
	// is already skinned.  If the destination mesh is skinned but there is no
	// specific joint specified to skin to, the first joint is used and a warning issued

	if ( nDstJointWeightsIndex >= 0 && nDstJointIndicesIndex >= 0 )
	{
		if ( nSkinningJointIndex < 0 )
		{
			Msg( "Warning: Destination mesh is skinned but no valid joint specified to skin to, using first joint\n" );
			nSkinningJointIndex = 0;
		}

		const int nJointCount = pDstBase->JointCount();

		CDmrGenericArray srcPos( pSrcBase->GetVertexData( nSrcPositionIndex ) );
		const int nSrcPosCount = srcPos.Count();

		CDmrArray< float > dstWeights( pDstBase->GetVertexData( nDstJointWeightsIndex ) );
		CDmrArray< int > dstIndices( pDstBase->GetVertexData( nDstJointIndicesIndex ) );

		const int nDstCount = dstWeights.Count();
		Assert( nDstCount == dstIndices.Count() );

		dstWeights.AddMultipleToTail( nSrcPosCount * nJointCount );
		dstIndices.AddMultipleToTail( nSrcPosCount * nJointCount );

		// Since there can be more than 1 joint per vertex, specify 1
		// for the first joint and 0 for the rest but use the same joint
		const int nEnd = nDstCount + nSrcPosCount * nJointCount;
		for ( int i = nDstCount; i < nEnd; i += nJointCount )
		{
			dstWeights.Set( i, 1.0f );
			dstIndices.Set( i, nSkinningJointIndex );
		}

		for ( int i = 1; i < nJointCount; ++i )
		{
			for ( int j = nDstCount + i; j < nEnd; j += nJointCount )
			{
				dstWeights.Set( j, 0.0f );
				dstIndices.Set( j, nSkinningJointIndex );
			}
		}
	}

	// Handling merging all fields that match
	int nIndexPadCount = -1;

	for ( int i = 0; i < pSrcBase->FieldCount(); ++i )
	{
		bool bMerged = false;

		for ( int j = 0; j < pDstBase->FieldCount(); ++j )
		{
			if ( i == nSrcJointWeightsIndex || i == nSrcJointIndicesIndex || Q_strcmp( pSrcBase->FieldName( i ), pDstBase->FieldName( j ) ) )
				continue;

			bMerged = true;

			CDmAttribute *pSrcData = pSrcBase->GetVertexData( i );
			CDmAttribute *pDstData = pDstBase->GetVertexData( j );

			const int nOffset = CDmrGenericArray( pDstData ).Count();

			switch ( pSrcData->GetType() )
			{
			case AT_FLOAT_ARRAY:
				AppendData( CDmrArrayConst< float >( pSrcData ), CDmrArray< float >( pDstData ) );
				break;
			case AT_VECTOR2_ARRAY:
				AppendData( CDmrArrayConst< Vector2D >( pSrcData ), CDmrArray< Vector2D >( pDstData ) );
				break;
			case AT_VECTOR3_ARRAY:
				if ( i == nSrcPositionIndex )
				{
					AppendData( CDmrArrayConst< Vector >( pSrcData ), CDmrArray< Vector >( pDstData ), &pMat );
				}
				else if ( i == nSrcNormalIndex )
				{
					AppendData( CDmrArrayConst< Vector >( pSrcData ), CDmrArray< Vector >( pDstData ), &nMat );
				}
				else
				{
					AppendData( CDmrArrayConst< Vector >( pSrcData ), CDmrArray< Vector >( pDstData ) );
				}
				break;
			case AT_VECTOR4_ARRAY:
				AppendData( CDmrArrayConst< Vector4D >( pSrcData ), CDmrArray< Vector4D >( pDstData ) );
				break;
			case AT_QUATERNION_ARRAY:
				AppendData( CDmrArrayConst< Quaternion >( pSrcData ), CDmrArray< Quaternion >( pDstData ) );
				break;
			case AT_COLOR_ARRAY:
				AppendData( CDmrArrayConst< Color >( pSrcData ), CDmrArray< Color >( pDstData ) );
				break;
			default:
				Assert( 0 );
				break;
			}

			CDmrArray< int > srcIndices( pSrcBase->GetIndexData( i ) );
			CDmrArray< int > dstIndices( pDstBase->GetIndexData( j ) );

			const int nSrcIndexCount = srcIndices.Count();
			const int nDstIndexCount = dstIndices.Count();

			if ( nRetVal < 0 )
			{
				nRetVal = nDstIndexCount;
			}
			Assert( nRetVal == nDstIndexCount );

			dstIndices.AddMultipleToTail( nSrcIndexCount );
			if ( nIndexPadCount < 0 )
			{
				nIndexPadCount = nSrcIndexCount;
			}
			Assert( nIndexPadCount == nSrcIndexCount );

			for ( int k = 0; k < nSrcIndexCount; ++k )
			{
				dstIndices.Set( nDstIndexCount + k, srcIndices.Get( k ) + nOffset );
			}

			if ( i == nSrcPositionIndex )
			{
				nPositionOffset = nOffset;
			}
			else if ( i == nSrcNormalIndex )
			{
				nNormalOffset = nOffset;
			}
			else if ( i == nSrcWrinkleIndex )
			{
				nWrinkleOffset = nOffset;
			}
		}

		if ( !bMerged )
		{
			Msg( "Warning: Not merging base data %s\n", pSrcBase->FieldName( i ) );
		}
	}

	const int nDstSpeedIndex = pDstBase->FindFieldIndex( CDmeVertexData::FIELD_MORPH_SPEED );

	// Handle all fields on the destination mesh that weren't on the source mesh
	for ( int i = 0; i < pDstBase->FieldCount(); ++i )
	{
		bool bFound = false;

		if ( i == nDstJointWeightsIndex || i == nDstJointIndicesIndex )
			continue;

		for ( int j = 0; j < pSrcBase->FieldCount(); ++j )
		{
			if ( Q_strcmp( pDstBase->FieldName( i ), pSrcBase->FieldName( j ) ) )
				continue;

			bFound = true;
			break;
		}

		if ( !bFound )
		{
			int nDstIndex = -1;

			if ( i == nDstSpeedIndex )
			{
				// Pad data with a 1
				nDstIndex = CDmrArray< float >( pDstBase->GetVertexData( i ) ).AddToTail( 1.0f );
			}
			else
			{
				// Pad data with a 0
				nDstIndex = CDmrGenericArray( pDstBase->GetVertexData( i ) ).AddToTail();
			}

			// Pad data indices with index to that extra data value
			CDmrArray< int > dstIndices( pDstBase->GetIndexData( i ) );
			const int nStart = dstIndices.Count();
			const int nEnd = dstIndices.Count() + nIndexPadCount;
			dstIndices.AddMultipleToTail( nIndexPadCount );
			for ( int k = nStart; k < nEnd; ++k )
			{
				dstIndices.Set( k, nDstIndex );
			}
		}
	}

	pDstBase->Resolve();

	return nRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MergeDeltaState( CDmeMesh *pDmeMesh, CDmeVertexDeltaData *pSrcDelta, CDmeVertexDeltaData *pDstDelta, int &nPositionOffset, int &nNormalOffset, int &nWrinkleOffset )
{
	if ( !pDstDelta )
	{
		// No destination delta... copy it
		pDstDelta = pDmeMesh->FindOrCreateDeltaState( pSrcDelta->GetName() );
		if ( !pDstDelta )
			return;
	}

	for ( int i = 0; i < pSrcDelta->FieldCount(); ++i )
	{
		bool bFound = false;

		for ( int j = 0; j < pDstDelta->FieldCount(); ++j )
		{
			if ( Q_strcmp( pSrcDelta->FieldName( i ), pDstDelta->FieldName( j ) ) )
				continue;

			bFound = true;
			break;
		}

		if ( !bFound )
		{
			// Make an empty one, data will be added below
			CDmAttribute *pSrcData = pSrcDelta->GetVertexData( i );
			pDstDelta->CreateField( pSrcDelta->FieldName( i ), pSrcData->GetType() );
		}
	}

	const int nSrcPositionIndex = pSrcDelta->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	const int nSrcNormalIndex = pSrcDelta->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	const int nSrcWrinkleIndex = pSrcDelta->FindFieldIndex( CDmeVertexData::FIELD_WRINKLE );

	for ( int i = 0; i < pSrcDelta->FieldCount(); ++i )
	{
		int nOffset = 0;

		if ( i == nSrcPositionIndex )
		{
			nOffset = nPositionOffset;
		}
		else if ( i == nSrcNormalIndex )
		{
			nOffset = nNormalOffset;
		}
		else if ( i == nSrcWrinkleIndex )
		{
			nOffset = nWrinkleOffset;
		}

		if ( nOffset < 0 )
		{
			nOffset = 0;
		}

		for ( int j = 0; j < pDstDelta->FieldCount(); ++j )
		{
			if ( Q_strcmp( pSrcDelta->FieldName( i ), pDstDelta->FieldName( j ) ) )
				continue;

			CDmAttribute *pSrcData = pSrcDelta->GetVertexData( i );
			CDmAttribute *pDstData = pDstDelta->GetVertexData( j );

			switch ( pSrcData->GetType() )
			{
			case AT_FLOAT_ARRAY:
				AppendData( CDmrArrayConst< float >( pSrcData ), CDmrArray< float >( pDstData ) );
				break;
			case AT_VECTOR2_ARRAY:
				AppendData( CDmrArrayConst< Vector2D >( pSrcData ), CDmrArray< Vector2D >( pDstData ) );
				break;
			case AT_VECTOR3_ARRAY:
				AppendData( CDmrArrayConst< Vector >( pSrcData ), CDmrArray< Vector >( pDstData ) );
				break;
			case AT_VECTOR4_ARRAY:
				AppendData( CDmrArrayConst< Vector4D >( pSrcData ), CDmrArray< Vector4D >( pDstData ) );
				break;
			case AT_QUATERNION_ARRAY:
				AppendData( CDmrArrayConst< Quaternion >( pSrcData ), CDmrArray< Quaternion >( pDstData ) );
				break;
			case AT_COLOR_ARRAY:
				AppendData( CDmrArrayConst< Color >( pSrcData ), CDmrArray< Color >( pDstData ) );
				break;
			default:
				Assert( 0 );
				break;
			}

			CDmrArray< int > srcIndices( pSrcDelta->GetIndexData( i ) );
			CDmrArray< int > dstIndices( pDstDelta->GetIndexData( j ) );

			const int nSrcIndexCount = srcIndices.Count();
			const int nDstIndexCount = dstIndices.Count();

			dstIndices.AddMultipleToTail( nSrcIndexCount );

			for ( int k = 0; k < nSrcIndexCount; ++k )
			{
				dstIndices.Set( nDstIndexCount + k, srcIndices.Get( k ) + nOffset );
			}

			break;
		}
	}

	// TODO: Centralize all of the '_' for corrector business...
	const char *pszDeltaName = pDstDelta->GetName();
	if ( strchr( pszDeltaName, '_' ) )
		return;	// No controls for deltas with '_''s

	if ( !pDmeMesh )
		return;

	CDmeCombinationOperator *pDmeCombo = FindReferringElement< CDmeCombinationOperator >( pDmeMesh, "targets" );
	if ( !pDmeCombo )
		return;

	if ( pDmeCombo->HasRawControl( pszDeltaName ) )
		return;

	pDmeCombo->FindOrCreateControl( pDstDelta->GetName(), false, true );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetAbsTransform( CDmeDag *pDmeDag, matrix3x4_t &m )
{
	matrix3x4_t mParentAbsTransform;
	pDmeDag->GetParentWorldMatrix( mParentAbsTransform );

	matrix3x4_t mLocal;
	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
	if ( pDmeTransform )
	{
		pDmeTransform->GetTransform( mLocal );
	}
	else
	{
		SetIdentityMatrix( mLocal );
	}

	ConcatTransforms( mParentAbsTransform, mLocal, m );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::Merge( CDmeMesh *pSrcMesh, CDmeMesh *pDstMesh, int nSkinningJointIndex )
{
	if ( !pSrcMesh || !pDstMesh )
		return false;

	CDmeDag *pSrcDag = FindReferringElement< CDmeDag >( pSrcMesh, "shape", true );
	CDmeDag *pDstDag = FindReferringElement< CDmeDag >( pDstMesh, "shape", true );

	if ( !pSrcDag || !pDstDag )
		return false;

	matrix3x4_t nMat;
	GetAbsTransform( pSrcDag, nMat );
	matrix3x4_t pMat;
	GetAbsTransform( pDstDag, pMat );
	matrix3x4_t dMatInv;

	MatrixInvert( pMat, dMatInv );
	MatrixMultiply( dMatInv, nMat, pMat );
	MatrixInverseTranspose( pMat, nMat );

	int nPositionOffset = -1;
	int nNormalOffset = -1;
	int nWrinkleOffset = -1;

	int nVertexOffset = -1;

	for ( int i = 0; i < pSrcMesh->BaseStateCount(); ++i )
	{
		CDmeVertexData *pSrcBase = pSrcMesh->GetBaseState( i );
		bool bMerged = false;

		for ( int j = 0; j < pDstMesh->BaseStateCount(); ++j )
		{
			CDmeVertexData *pDstBase = pDstMesh->GetBaseState( j );

			if ( Q_strcmp( pSrcBase->GetName(), pDstBase->GetName() ) )
				continue;

			bMerged = true;
			const int nTmpVertexOffset = MergeBaseState( pSrcBase, pDstBase, pMat, nMat, nSkinningJointIndex, nPositionOffset, nNormalOffset, nWrinkleOffset );
			if ( nVertexOffset < 0 )
			{
				nVertexOffset = nTmpVertexOffset;
			}

			Assert( nVertexOffset == nTmpVertexOffset );
		}

		if ( !bMerged )
		{
			Msg( "Error: Merge( %s, %s ) - Can't Find Base State %s On %s\n", pSrcMesh->GetName(), pDstMesh->GetName(), pSrcBase->GetName(), pDstMesh->GetName() );
		}
	}

	// Merge Face Sets

	int nFaceSetIndex;

	for ( int i = 0; i < pSrcMesh->FaceSetCount(); ++i )
	{
		CDmeFaceSet *pFaceSet = pSrcMesh->GetFaceSet( i )->Copy();
		pFaceSet->SetFileId( pDstMesh->GetFileId(), TD_DEEP );
		const int nFaceSetIndexCount = pFaceSet->NumIndices();
		for ( int j = 0; j < nFaceSetIndexCount; ++j )
		{
			nFaceSetIndex = pFaceSet->GetIndex( j );
			if ( nFaceSetIndex >= 0 )
			{
				pFaceSet->SetIndex( j, nFaceSetIndex + nVertexOffset );
			}
		}
		pDstMesh->AddFaceSet( pFaceSet );
	}

	// Merge Deltas

	for ( int i = 0; i < pSrcMesh->DeltaStateCount(); ++i )
	{
		CDmeVertexDeltaData *pSrcDelta = pSrcMesh->GetDeltaState( i );
		CDmeVertexDeltaData *pDstDelta = pDstMesh->FindDeltaState( pSrcDelta->GetName() );
		MergeDeltaState( pDstMesh, pSrcDelta, pDstDelta, nPositionOffset, nNormalOffset, nWrinkleOffset );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
struct VertexWeightMap_s
{
	struct VertexWeight_s
	{
		int m_vertexDataIndex;						// Index into the CDmeVertexData data (only used for joint weights & indices)
		const CUtlVector< int > *m_pVertexIndices;	// Index into the CDmeVertexData vertex indices
		float m_vertexWeight;
	};

	int m_nVertexWeights;
	VertexWeight_s m_vertexWeights[ 5 ];
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CopyJointWeights(
	CDmeVertexData *pSrcData,
	CDmeVertexData *pDstData,
	const CUtlVector< VertexWeightMap_s > &vertexWeightMap )
{
	const int nJointCount = pSrcData->GetValue< int >( "jointCount" );

	const FieldIndex_t nSrcJointWeightsField = pSrcData->FindFieldIndex( CDmeVertexData::FIELD_JOINT_WEIGHTS );
	const FieldIndex_t nSrcJointIndicesField = pSrcData->FindFieldIndex( CDmeVertexData::FIELD_JOINT_INDICES );

	if ( nJointCount <= 0 || nSrcJointWeightsField < 0 || nSrcJointIndicesField < 0 )
		return false;

	const CUtlVector< float > &srcJointWeights = CDmrArrayConst< float >( pSrcData->GetVertexData( nSrcJointWeightsField ) ).Get();
	const float *const pSrcJointWeights = srcJointWeights.Base();

	const CUtlVector< int > &srcJointIndices = CDmrArrayConst< int >( pSrcData->GetVertexData( nSrcJointIndicesField ) ).Get();
	const int *const pSrcJointIndices = srcJointIndices.Base();

	FieldIndex_t nDstJointWeightsField;
	FieldIndex_t nDstJointIndicesField;

	pDstData->CreateJointWeightsAndIndices( nJointCount, &nDstJointWeightsField, &nDstJointIndicesField );

	const int nDstCount = vertexWeightMap.Count();

	float *pDstJointWeights = reinterpret_cast< float * >( alloca( nDstCount * nJointCount * sizeof( float ) ) );
	memset( pDstJointWeights, 0, nDstCount * nJointCount );

	int *pDstJointIndices = reinterpret_cast< int * >( alloca( nDstCount * nJointCount * sizeof( int ) ) );
	memset( pDstJointIndices, 0, nDstCount * nJointCount );

	for ( int i = 0; i < nDstCount; ++i )
	{
		const VertexWeightMap_s &vertexWeight = vertexWeightMap[ i ];
		const int nVertexWeights = vertexWeight.m_nVertexWeights;

		if ( nVertexWeights > 0 )
		{
			// TODO: Find the best weights to use! For now, use the first one
			int nMatchIndex = vertexWeight.m_vertexWeights[ 0 ].m_vertexDataIndex;

			memcpy( pDstJointWeights + i * nJointCount, pSrcJointWeights + nMatchIndex * nJointCount, nJointCount * sizeof( float ) );
			memcpy( pDstJointIndices + i * nJointCount, pSrcJointIndices + nMatchIndex * nJointCount, nJointCount * sizeof( int ) );
		}
	}

	pDstData->AddVertexData( nDstJointIndicesField, nDstCount * nJointCount );
	pDstData->SetVertexData( nDstJointIndicesField, 0, nDstCount * nJointCount, AT_INT, pDstJointIndices );

	pDstData->AddVertexData( nDstJointWeightsField, nDstCount * nJointCount );
	pDstData->SetVertexData( nDstJointWeightsField, 0, nDstCount * nJointCount, AT_FLOAT, pDstJointWeights );

	return true;
}


//-----------------------------------------------------------------------------
// Replaces the DstMesh with the SrcMesh
//-----------------------------------------------------------------------------
CDmeMesh *ReplaceMesh(
	CDmeMesh *pSrcMesh,
	CDmeMesh *pDstMesh )
{
	if ( !pSrcMesh || !pDstMesh )
		return NULL;

	CDmeDag *pSrcDag = pSrcMesh->GetParent();
	CDmeDag *pDstDag = pDstMesh->GetParent();

	if ( !pSrcDag || !pDstDag )
		return NULL;

	// Fix up the transform
	matrix3x4_t inclusiveMat;
	matrix3x4_t localMat;

	pDstDag->GetShapeToWorldTransform( inclusiveMat );
	pDstDag->GetTransform()->GetTransform( localMat );

	matrix3x4_t inverseMat;
	MatrixInvert( localMat, inverseMat );

	matrix3x4_t exclusiveMat;
	MatrixMultiply( inclusiveMat, inverseMat, exclusiveMat );
	MatrixInvert( exclusiveMat, inverseMat );

	pSrcDag->GetShapeToWorldTransform( inclusiveMat );
	MatrixMultiply( inverseMat, inclusiveMat, localMat );

	pDstDag->GetTransform()->SetTransform( localMat );

	// Duplicate the mesh
	CDmeMesh *pNewMesh = pSrcMesh->Copy();
	pNewMesh->SetFileId( pDstMesh->GetFileId(), TD_DEEP );

	// A bit of cleanup
	pNewMesh->RemoveAttribute( "selection" );
	pNewMesh->SetCurrentBaseState( "bind" );
	pNewMesh->DeleteBaseState( "__dmxEdit_work" );

	// Replace the DstMesh with the SrcMesh
	pDstDag->SetShape( pNewMesh );

	// Replace the combination operators, if applicable
	CDmeCombinationOperator *pSrcComboOp = FindReferringElement< CDmeCombinationOperator >( pSrcMesh, "targets" );
	if ( pSrcComboOp )
	{
		CDmeCombinationOperator *pDstComboOp = FindReferringElement< CDmeCombinationOperator >( pDstMesh, "targets" );
		CDmElement *pDstRoot = NULL;
		if ( pDstComboOp )
		{
			// Find the root the easy way
			pDstRoot = FindReferringElement< CDmElement >( pDstComboOp, "combinationOperator" );

			// Delete the old busted combination operator
			g_pDataModel->DestroyElement( pDstComboOp->GetHandle() );
		}
		else
		{
			// Find the root the hard way
			CDmeDag *pDmeDag = pDstDag;
			for ( ;; )
			{
				// Walk backwards via "children" attribute
				CDmeDag *pNextDag = FindReferringElement< CDmeDag >( pDmeDag, "children" );
				if ( pNextDag )
				{
					pDmeDag = pNextDag;
				}
				else
				{
					// Can't find anyone referring to this via "children" so, hopefully it's the DmeModel referred to by "model"
					pDstRoot = FindReferringElement< CDmElement >( pDmeDag, "model" );
					break;
				}
			}
		}

		if ( pDstRoot )
		{
			// Install the shiny new combination operator
			CDmeCombinationOperator *pNewComboOp = pSrcComboOp->Copy();
			pNewComboOp->SetFileId( pDstRoot->GetFileId(), TD_DEEP );
			pDstRoot->SetValue( "combinationOperator", pNewComboOp );
			pNewComboOp->RemoveAllTargets();
			pNewComboOp->AddTarget( pNewMesh );
			pNewComboOp->GenerateWrinkleDeltas( false );

		}
	}

	return pNewMesh;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void CopyFieldData(
	const CDmrArrayConst< T_t > &srcData,
	const CUtlVector< int > &srcIndices,
	CDmeVertexData *pDstVertexData,
	FieldIndex_t dstFieldIndex,
	const CUtlVector< VertexWeightMap_s > &vertexWeightMap )
{
	const int nDstData = vertexWeightMap.Count();

	T_t sum;

	T_t *pDstData = reinterpret_cast< T_t * >( alloca( nDstData * sizeof( T_t ) ) );

	for ( int i = 0; i < nDstData; ++i )
	{
		CDmAttributeInfo< T_t >::SetDefaultValue( pDstData[ i ] );

		const VertexWeightMap_s &vertexWeight = vertexWeightMap[ i ];
		for ( int j = 0; j < vertexWeight.m_nVertexWeights; ++j )
		{
			const VertexWeightMap_s::VertexWeight_s &vWeight = vertexWeight.m_vertexWeights[ j ];

			CDmAttributeInfo< T_t >::SetDefaultValue( sum );

			const CUtlVector< int > &vertexList = *vWeight.m_pVertexIndices;
			for ( int k = 0; k < vertexList.Count(); ++k )
			{
				sum += srcData[ srcIndices[ vertexList[ k ] ] ];
			}
			sum /= static_cast< float >( vertexList.Count() );

			pDstData[ i ] += sum * vWeight.m_vertexWeight;
		}
	}

	const DmAttributeType_t dmAttributeType = ArrayTypeToValueType( srcData.GetAttribute()->GetType() );
	CDmrArray< T_t > dstData( pDstVertexData->GetVertexData( dstFieldIndex ) );
	dstData.EnsureCount( nDstData );
	pDstVertexData->SetVertexData( dstFieldIndex, 0, nDstData, dmAttributeType, pDstData );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CopyField(
	CDmeVertexData::StandardFields_t field,
	CDmeVertexData *pSrcData,
	CDmeVertexData *pDstData,
	const CUtlVector< VertexWeightMap_s > &vertexWeightMap )
{
	FieldIndex_t srcFieldIndex = pSrcData->FindFieldIndex( field );
	if ( srcFieldIndex < 0 )
		return false;

	FieldIndex_t dstFieldIndex = pDstData->CreateField( field );
	if ( dstFieldIndex < 0 )
		return false;

	CDmAttribute *pSrcVertexData = pSrcData->GetVertexData( srcFieldIndex );
	const CUtlVector< int > &srcIndices = pSrcData->GetVertexIndexData( srcFieldIndex );

	// Everything on dst has to be indexed the same as position
	const CUtlVector< int > &dstPosIndices = pDstData->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	CDmrArray< int > dstIndices( pDstData->GetIndexData( dstFieldIndex ) );
	dstIndices.EnsureCount( dstPosIndices.Count() );
	pDstData->SetVertexIndices( dstFieldIndex, 0, dstPosIndices.Count(), dstPosIndices.Base() );


	switch ( pSrcVertexData->GetType() )
	{
	case AT_FLOAT_ARRAY:
		CopyFieldData( CDmrArrayConst< float >( pSrcVertexData ), srcIndices, pDstData, dstFieldIndex, vertexWeightMap );
		break;
	case AT_VECTOR2_ARRAY:
		CopyFieldData( CDmrArrayConst< Vector2D >( pSrcVertexData ), srcIndices, pDstData, dstFieldIndex, vertexWeightMap );
		break;
	case AT_VECTOR3_ARRAY:
		CopyFieldData( CDmrArrayConst< Vector >( pSrcVertexData ), srcIndices, pDstData, dstFieldIndex, vertexWeightMap );
		break;
	default:
		break;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::Merge(
	CDmMeshComp &srcComp,
	const CUtlVector< CDmMeshComp::CEdge * > &edgeList,
	CDmeMesh *pDstMesh )
{
	CDmeMesh *pSrcMesh = srcComp.m_pMesh;
	if ( !pSrcMesh || !pDstMesh )
		return false;

	CDmeVertexData *pSrcData = pSrcMesh->FindBaseState( "bind" );
	CDmeVertexData *pDstData = pDstMesh->FindBaseState( "bind" );

	if ( !pSrcData || !pDstData )
		return false;

	const CUtlVector< Vector > &srcPosData = pSrcData->GetPositionData();
	const int nSrcCount = srcPosData.Count();

	const CUtlVector< Vector > &dstPosData = pDstData->GetPositionData();
	const int nDstCount = dstPosData.Count();

	if ( nSrcCount <= 0 || nDstCount <= 0 )
		return false;

	CUtlVector< VertexWeightMap_s > vertexWeightMap;
	vertexWeightMap.SetSize( nSrcCount );

	for ( int i = 0; i < nSrcCount; ++i )
	{
		int nClosestIndex = -1;
		float closest = FLT_MAX;

		VertexWeightMap_s &vertexWeight = vertexWeightMap[ i ];
		vertexWeight.m_nVertexWeights = 0;

		const Vector &vSrc = srcPosData[ i ];

		for ( int j = 0; j < nDstCount; ++j )
		{
			const Vector &vDst = dstPosData[ j ];
			if ( vSrc.DistToSqr( vDst ) < FLT_EPSILON * 10.0f )
			{
				vertexWeight.m_nVertexWeights = 1;
				vertexWeight.m_vertexWeights[ 0 ].m_vertexDataIndex = j;
				vertexWeight.m_vertexWeights[ 0 ].m_vertexWeight = 1.0f;
				vertexWeight.m_vertexWeights[ 0 ].m_pVertexIndices = &pDstData->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, j );
				break;
			}

			float distance = vSrc.DistToSqr( vDst );
			if ( distance < closest )
			{
				closest = distance;
				nClosestIndex = j;
			}
		}

		if ( vertexWeight.m_nVertexWeights == 0 )
		{
			Warning( "Warning: Merge() - No Match For Src Vertex: %f %f %f, Using Closest: %f %f %f\n",
				vSrc.x, vSrc.y, vSrc.z,
				dstPosData[ nClosestIndex ].x, dstPosData[ nClosestIndex ].y, dstPosData[ nClosestIndex ].z );

			// TODO: Loop through and find up to n closest vertices by position

			vertexWeight.m_nVertexWeights = 1;
			vertexWeight.m_vertexWeights[ 0 ].m_vertexDataIndex = nClosestIndex;
			vertexWeight.m_vertexWeights[ 0 ].m_vertexWeight = 1.0f;
			vertexWeight.m_vertexWeights[ 0 ].m_pVertexIndices = &pDstData->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, nClosestIndex );

			//			Assert( vertexWeight.m_nVertexWeights );
			//			return false;
		}
	}

	CDmeMesh *pNewMesh = ReplaceMesh( pSrcMesh, pDstMesh );
	if ( !pNewMesh )
	{
		Error( "Error: Merge() - Couldn't Replace Mesh %s With %s\n", pDstMesh->GetName(), pSrcMesh->GetName() );
		return false;
	}

	CDmeVertexData *pNewData = pNewMesh->FindBaseState( "bind" );
	if ( pNewData )
	{
		CopyJointWeights( pDstData, pNewData, vertexWeightMap );

		CopyField( CDmeVertexData::FIELD_BALANCE, pDstData, pNewData, vertexWeightMap );

		CopyField( CDmeVertexData::FIELD_MORPH_SPEED, pDstData, pNewData, vertexWeightMap );

		if ( pNewData->FindFieldIndex( CDmeVertexData::FIELD_MORPH_SPEED ) >= 0 )
		{
			CDmeCombinationOperator *pComboOp( FindReferringElement< CDmeCombinationOperator >( pNewMesh, "targets" ) );
			if ( pComboOp )
			{
				pComboOp->UsingLaggedData( true );
			}
		}
	}

	// Destroy the old busted mesh
	g_pDataModel->DestroyElement( pDstMesh->GetHandle() );

	return true;
}


//-----------------------------------------------------------------------------
// Returns a guaranteed unique DmFileId_t
//-----------------------------------------------------------------------------
DmFileId_t CreateUniqueFileId()
{
	DmFileId_t fileId = DMFILEID_INVALID;

	UniqueId_t uniqueId;
	char fileIdBuf[ MAX_PATH ];

	do 
	{
		CreateUniqueId( &uniqueId );
		UniqueIdToString( uniqueId, fileIdBuf, sizeof( fileIdBuf ) );

		fileId = g_pDataModel->GetFileId( fileIdBuf );
	} while( fileId != DMFILEID_INVALID );

	return g_pDataModel->FindOrCreateFileId( fileIdBuf );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CreateExpressionFile( const char *pExpressionFile, const CUtlVector< CUtlString > *pPurgeAllButThese, CDmeCombinationOperator *pComboOp, CDmePresetGroup *pPresetGroup )
{
	if ( !pPresetGroup )
		return false;

	Assert( pExpressionFile && pComboOp );

	const int nControlCount = pComboOp->GetControlCount();

	const CDmaElementArray< CDmePreset > &presets = pPresetGroup->GetPresets();
	const int nPresetsCount = presets.Count();

	if ( nControlCount <= 0 || nPresetsCount <= 0 )
		return false;

	char expName[ MAX_PATH ];
	Q_FileBase( pExpressionFile, expName, sizeof( expName ) );

	CDmePresetGroup *pDstPresetGroup = CreateElement< CDmePresetGroup >( expName, CreateUniqueFileId() );
	if ( !pDstPresetGroup )
		return false;

	for ( int i = 0; i < nPresetsCount; ++i )
	{
		CDmePreset *pPreset = presets[ i ];
		const char *pPresetName = pPreset->GetName();
		CDmePreset *pDstPreset = pDstPresetGroup->FindOrAddPreset( pPresetName );

		const CDmaElementArray< CDmElement > &controlValues = pPreset->GetControlValues();
		const int nControlValueCount = controlValues.Count();

		for ( int j = 0; j < nControlCount; ++j )
		{
			// Figure out if this preset is used
			bool bFound = false;	// Used for two things
			const char *pControlName = pComboOp->GetControlName( j );

			if ( pPurgeAllButThese )
			{
				for ( int k = 0; k < pPurgeAllButThese->Count(); ++k )
				{
					if ( !Q_strcmp( pControlName, pPurgeAllButThese->Element( k ).Get() ) )
					{
						bFound = true;
						break;
					}
				}
			}

			if ( !bFound && pPresetGroup->FindPreset( pControlName ) )
				bFound = true;

			if ( !bFound )
				continue;

			CDmElement *pDstControlValue = NULL;

			const bool bStereo = pComboOp->IsStereoControl( j );
			const bool bMulti = pComboOp->IsMultiControl( j );

			if ( !Q_strcmp( pControlName, pPresetName ) )
			{
				pDstControlValue = pDstPreset->FindOrAddControlValue( pControlName );
				pDstControlValue->SetValue( "value", 1.0f );

				// These shouldn't really happen because these are presets which were made
				// into deltas so they are never stereo nor multi-controls
				if ( bStereo )
				{
					pDstControlValue->SetValue( "balance", 0.5f );
				}
				if ( bStereo )
				{
					pDstControlValue->SetValue( "multilevel", 0.5f );
				}
				continue;
			}

			for ( int k = 0; k < nControlValueCount; ++k )
			{
				CDmElement *pControlPreset = controlValues[ k ];

				if ( !Q_strcmp( pControlName, pControlPreset->GetName() ) )
				{
					pDstControlValue = pDstPreset->FindOrAddControlValue( pControlName );
					pDstControlValue->SetValue( "value", pControlPreset->GetValue( "value", 0.0f ) );
					if ( bStereo )
					{
						pDstControlValue->SetValue( "balance", pControlPreset->GetValue( "balance", 0.5f ) );
					}
					if ( bMulti )
					{
						pDstControlValue->SetValue( "multilevel", pControlPreset->GetValue( "multilevel", 0.5f ) );
					}
					break;
				}
			}

			if ( !pDstControlValue )
			{
				pDstControlValue = pDstPreset->FindOrAddControlValue( pControlName );
				pDstControlValue->SetValue( "value", pComboOp->GetControlDefaultValue( j ) );

				if ( bStereo )
				{
					pDstControlValue->SetValue( "balance", 0.5f );
				}
				if ( bMulti )
				{
					pDstControlValue->SetValue( "multilevel", 0.5f );
				}
			}
		}
	}

	char buf[ MAX_PATH ];
	char buf1[ MAX_PATH ];
	Q_strncpy( buf, pExpressionFile, sizeof( buf ) );
	Q_SetExtension( buf, ".txt", sizeof( buf ) );
	Q_ExtractFilePath( buf, buf1, sizeof( buf1 ) );
	Q_FixSlashes( buf1 );
	g_pFullFileSystem->CreateDirHierarchy( buf1 );

	if ( !g_p4factory->AccessFile( buf )->Edit() )
	{
		g_p4factory->AccessFile( buf )->Add();
	}

	pDstPresetGroup->ExportToTXT( buf, NULL, pComboOp );

	Q_SetExtension( buf, ".vfe", sizeof( buf ) );
	Q_ExtractFilePath( buf, buf1, sizeof( buf1 ) );
	Q_FixSlashes( buf1 );
	g_pFullFileSystem->CreateDirHierarchy( buf1 );

	if ( !g_p4factory->AccessFile( buf )->Edit() )
	{
		g_p4factory->AccessFile( buf )->Add();
	}

	pDstPresetGroup->ExportToVFE( buf, NULL, pComboOp );

	g_pDataModel->UnloadFile( pDstPresetGroup->GetFileId() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::CreateDeltasFromPresets(
	CDmeMesh *pMesh,
	CDmeVertexData *pPassedDst,
	const CUtlStringMap< CUtlString > &presetExpressionMap,
	bool bPurge,
	const CUtlVector< CUtlString > *pPurgeAllButThese /*= NULL */ )
{
	if ( !pMesh )
		return false;

	CDisableUndoScopeGuard sgDisableUndo;

	CUtlStringMap< CDmePreset * > presetMap;
	CUtlStringMap< CUtlString > conflictingNames;

	CDmeVertexData *pDst = pPassedDst ? pPassedDst : pMesh->GetCurrentBaseState();
	CDmeVertexData *pBind = pMesh->FindBaseState( "bind" );
	if ( !pDst || !pBind || pDst == pBind )
		return false;

	CDmeCombinationOperator *pComboOp = FindReferringElement< CDmeCombinationOperator >( pMesh, "targets" );
	if ( !pComboOp )
		return false;

	const bool bSavedUsingLagged = pComboOp->IsUsingLaggedData();

	CUtlVector< CDmePresetGroup * > presetGroups;

	for ( int i = 0; i < presetExpressionMap.GetNumStrings(); ++i )
	{
		const char *pPresetFilename = presetExpressionMap.String( i );

		// Load the preset file
		CDmElement *pRoot = NULL;
		g_p4factory->AccessFile( pPresetFilename )->Add();
		g_pDataModel->RestoreFromFile( pPresetFilename, NULL, NULL, &pRoot );
		CDmePresetGroup *pPresetGroup = CastElement< CDmePresetGroup >( pRoot );

		presetGroups.AddToTail( pPresetGroup );

		if ( !pPresetGroup )
			continue;

		CreateDeltasFromPresetGroup( pPresetGroup, pComboOp, pPurgeAllButThese, pMesh, pDst, conflictingNames, presetMap );
	}

	if ( bPurge )
	{
		PurgeUnreferencedDeltas( pMesh, presetMap, pPurgeAllButThese, pComboOp );
	}

	for ( int i = 0; i < presetMap.GetNumStrings(); ++i )
	{
		const char *pPresetName = presetMap[ i ]->GetName();
		const int nControlIndex = pComboOp->FindControlIndex( pPresetName );
		if ( nControlIndex < 0 )
		{
			pComboOp->FindOrCreateControl( pPresetName, false, true );
		}
		else
		{
			bool bFound = false;

			if ( bPurge )
			{
				pComboOp->RemoveAllRawControls( nControlIndex );
			}
			else
			{
				const int nRawControls = pComboOp->GetRawControlCount( nControlIndex );
				for ( int j = 0; j < nRawControls; ++j )
				{
					if ( !Q_strcmp( pComboOp->GetRawControlName( nControlIndex, j ), pPresetName ) )
					{
						bFound = true;
						break;
					}
				}
			}

			if ( !bFound )
			{
				pComboOp->AddRawControl( nControlIndex, pPresetName );
			}
		}
	}

	pComboOp->UsingLaggedData( bSavedUsingLagged );
	pComboOp->SetToDefault();

	for ( int i = 0; i < presetExpressionMap.GetNumStrings(); ++i )
	{
		const CUtlString &expressionFile = presetExpressionMap[ i ];
		if ( expressionFile.IsEmpty() )
			continue;

		CreateExpressionFile( expressionFile.Get(), pPurgeAllButThese, pComboOp, presetGroups[ i ] );
	}

	for ( int i = 0; i < presetGroups.Count(); ++i )
	{
		CDmePresetGroup *pPresetGroup = presetGroups[ i ];
		if ( !pPresetGroup )
			continue;

		g_pDataModel->UnloadFile( pPresetGroup->GetFileId() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Removes any deltas from the specified mesh which are not referred to by
// any rule of the combination operator driving the mesh
//-----------------------------------------------------------------------------
bool CDmMeshUtils::PurgeUnusedDeltas( CDmeMesh *pMesh )
{
	// Disable for now
	// This code will delete all corrective delta states, i.e. deltas named A_B
	return true;

	if ( !pMesh )
		return false;

	CDmeCombinationOperator *pCombo = FindReferringElement< CDmeCombinationOperator >( pMesh, "targets" );
	if ( !pCombo )
		return false;

	const int nControlCount = pCombo->GetControlCount();

	CUtlVector< CDmeMesh::DeltaComputation_t > compList;
	pMesh->ComputeDependentDeltaStateList( compList );
	const int nDeltaCount = compList.Count();

	Assert( nDeltaCount == pMesh->DeltaStateCount() );

	CUtlVector< bool > deltasToKeep;
	deltasToKeep.EnsureCount( nDeltaCount );
	memset( deltasToKeep.Base(), 0, sizeof( bool ) * nDeltaCount );

	for ( int i = 0; i < nControlCount; ++i )
	{
		const int nRawControlCount = pCombo->GetRawControlCount( i );
		for ( int j = 0; j < nRawControlCount; ++j )
		{
			const int nDeltaIndex = pMesh->FindDeltaStateIndex( pCombo->GetRawControlName( i, j ) );
			const CDmeMesh::DeltaComputation_t &deltaComp = compList[ nDeltaIndex ];
			deltasToKeep[ deltaComp.m_nDeltaIndex ] = true;
			for ( int k = 0; k < deltaComp.m_DependentDeltas.Count(); ++k )
			{
				deltasToKeep[ deltaComp.m_DependentDeltas[ k ] ] = true;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshUtils::CreateWrinkleDeltaFromBaseState(
	CDmeVertexDeltaData *pDelta,
	float flScale /* = 1.0f */,
	WrinkleOp wrinkleOp /* = kReplace */,
	CDmeMesh *pPassedMesh /* = NULL */,
	CDmeVertexData *pPassedBind /* = NULL */,
	CDmeVertexData *pPassedCurrent /* = NULL */ )
{
	CDmeVertexData *pBind = pPassedBind ? pPassedBind : pPassedMesh ? pPassedMesh->GetBindBaseState() : NULL;
	CDmeVertexData *pCurr = pPassedCurrent ? pPassedCurrent : pPassedMesh ? pPassedMesh->GetCurrentBaseState() : NULL;

	const CDmeMesh *pMesh = pPassedMesh ? pPassedMesh : pBind ? FindReferringElement< CDmeMesh >( pBind, "baseStates" ) : NULL;
	const CDmeMesh *pBindMesh = pBind ? FindReferringElement< CDmeMesh >( pBind, "baseStates" ) : NULL;
	const CDmeMesh *pCurrMesh = pCurr ? FindReferringElement< CDmeMesh >( pCurr, "baseStates", false ) : NULL;
	const CDmeMesh *pDeltaMesh = pDelta ? FindReferringElement< CDmeMesh >( pDelta, "deltaStates" ) : NULL;

	if ( !pDelta || !pBind || !pCurr || pBind == pCurr || !pMesh || pMesh != pBindMesh || pMesh != pCurrMesh || pMesh != pDeltaMesh )
	{
		return false;
	}

	const FieldIndex_t nBindPosIndex = pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	const FieldIndex_t nBindTexIndex = pBind->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	const FieldIndex_t nCurrPosIndex = pCurr->FindFieldIndex( CDmeVertexData::FIELD_POSITION );

	if ( nBindPosIndex < 0 || nBindTexIndex < 0 || nCurrPosIndex < 0 )
		return false;

	const CUtlVector< Vector > &bindPos = CDmrArrayConst< Vector >( pBind->GetVertexData( nBindPosIndex ) ).Get();
	const CUtlVector< Vector > &currPos = CDmrArrayConst< Vector >( pCurr->GetVertexData( nCurrPosIndex ) ).Get();
	const CUtlVector< int > &baseTexCoordIndices = pBind->GetVertexIndexData( nBindTexIndex );

	const int nPosCount = bindPos.Count();
	if ( nPosCount != currPos.Count() )
		return false;

	const CDmrArrayConst< Vector2D > texData( pBind->GetVertexData( nBindTexIndex ) );
	const int nBaseTexCoordCount = texData.Count();

	FieldIndex_t nWrinkleIndex = pDelta->FindFieldIndex( CDmeVertexDeltaData::FIELD_WRINKLE );
	if ( nWrinkleIndex < 0 )
	{
		nWrinkleIndex = pDelta->CreateField( CDmeVertexDeltaData::FIELD_WRINKLE );
	}

	float *pOldWrinkleData = NULL;

	if ( wrinkleOp == kAdd )
	{
		// Copy the old wrinkle data
		CDmAttribute *pWrinkleDeltaAttr = pDelta->GetVertexData( nWrinkleIndex );
		if ( pWrinkleDeltaAttr )
		{
			CDmrArrayConst< float > wrinkleDeltaArray( pWrinkleDeltaAttr );
			if ( wrinkleDeltaArray.Count() )
			{
				const CUtlVector< int > &wrinkleDeltaIndices = pDelta->GetVertexIndexData( nWrinkleIndex );
				Assert( wrinkleDeltaIndices.Count() == wrinkleDeltaArray.Count() );

				pOldWrinkleData = reinterpret_cast< float * >( alloca( nBaseTexCoordCount * sizeof( float ) ) );
				memset( pOldWrinkleData, 0, nBaseTexCoordCount * sizeof( float ) );

				for ( int i = 0; i < wrinkleDeltaIndices.Count(); ++i )
				{
					if ( i < nPosCount )
					{
						*( pOldWrinkleData + wrinkleDeltaIndices[i]) = wrinkleDeltaArray[ i ];
					}
				}
			}
		}
	}

	pDelta->RemoveAllVertexData( nWrinkleIndex );
	if ( flScale == 0.0f && wrinkleOp != kAdd )
		return true;

	float flMaxDeflection = 0.0f;
	int *pWrinkleIndices = reinterpret_cast< int * >( alloca( nPosCount * sizeof( int ) ) );
	float *pWrinkleDelta = reinterpret_cast< float * >( alloca( nPosCount * sizeof( float ) ) );
	int nWrinkleCount = 0;

	float flDelta;
	Vector v;

	if ( pOldWrinkleData )
	{
		for ( int i = 0; i < nPosCount; ++i )
		{
			v = bindPos[ i ] - currPos[ i ];

			// Figure out the texture indices for this position index
			const CUtlVector< int > &baseVerts = pBind->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, i );

			for ( int j = 0; j < baseVerts.Count(); ++j )
			{
				// See if we have a delta for this texcoord...
				const int nTexCoordIndex = baseTexCoordIndices[ baseVerts[ j ] ];

				if ( fabs( pOldWrinkleData[ nTexCoordIndex ] ) > 0.0001 || fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) || fabs( v.z ) >= ( 1 / 4096.0f ) )
				{
					flDelta = v.Length();
					if ( flDelta > flMaxDeflection )
					{
						flMaxDeflection = flDelta;
					}
					pWrinkleDelta[ nWrinkleCount ] = flDelta;
					pWrinkleIndices[ nWrinkleCount ] = i;
					++nWrinkleCount;
					break;
				}
			}
		}
	}
	else
	{
		for ( int i = 0; i < nPosCount; ++i )
		{
			v = bindPos[ i ] - currPos[ i ];
			if ( fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) || fabs( v.z ) >= ( 1 / 4096.0f ) )
			{
				flDelta = v.Length();
				if ( flDelta > flMaxDeflection )
				{
					flMaxDeflection = flDelta;
				}
				pWrinkleDelta[ nWrinkleCount ] = flDelta;
				pWrinkleIndices[ nWrinkleCount ] = i;
				++nWrinkleCount;
			}
		}
	}

	if ( flMaxDeflection == 0.0f )
		return true;

	const double scaledInverseMaxDeflection = static_cast< double >( flScale ) / static_cast< double >( flMaxDeflection );

	const int nBufSize = ( ( nBaseTexCoordCount + 7 ) >> 3 );
	unsigned char * const pUsedBits = reinterpret_cast< unsigned char* >( alloca( nBufSize * sizeof( unsigned char ) ) );
	memset( pUsedBits, 0, nBufSize );

	for ( int i = 0; i < nWrinkleCount; ++i )
	{
		float flWrinkleDelta = static_cast< float >( static_cast< double >( pWrinkleDelta[ i ] ) * scaledInverseMaxDeflection );

		Assert( fabs( flWrinkleDelta ) <= fabs( flScale ) );

		// NOTE: This will produce bad behavior in cases where two positions share the
		// same texcoord, which shouldn't theoretically happen.
		const CUtlVector< int > &baseVerts = pBind->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, pWrinkleIndices[ i ] );
		const int nBaseVertCount = baseVerts.Count();
		for ( int j = 0; j < nBaseVertCount; ++j )
		{
			// See if we have a delta for this texcoord...
			int nTexCoordIndex = baseTexCoordIndices[ baseVerts[j] ];
			if ( pUsedBits[ nTexCoordIndex >> 3 ] & ( 1 << ( nTexCoordIndex & 0x7 ) ) )
				continue;

			pUsedBits[ nTexCoordIndex >> 3 ] |= 1 << ( nTexCoordIndex & 0x7 );

			if ( pOldWrinkleData )
			{
				flWrinkleDelta += pOldWrinkleData[ nTexCoordIndex ];
			}

			int nDeltaIndex = pDelta->AddVertexData( nWrinkleIndex, 1 );
			pDelta->SetVertexIndices( nWrinkleIndex, nDeltaIndex, 1, &nTexCoordIndex );
			pDelta->SetVertexData( nWrinkleIndex, nDeltaIndex, 1, AT_FLOAT, &flWrinkleDelta );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshFaceIt::CDmMeshFaceIt( const CDmeMesh *pMesh, const CDmeVertexData *pVertexData /* = NULL */ )
{
	Reset( pMesh, pVertexData );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::Reset( const CDmeMesh *pMesh, const CDmeVertexData *pVertexData /* = NULL */ )
{
	m_nFaceIndex = 0;

	if ( pMesh )
	{
		m_pMesh = pMesh;
		m_pVertexData = pVertexData ? pVertexData : m_pVertexData ? m_pVertexData : m_pMesh->GetCurrentBaseState();

		m_nFaceSetCount = 0;
		m_nFaceSetIndex = 0;

		m_pFaceSet = NULL;

		m_nFaceSetIndexCount = 0;
		m_nFaceSetIndexIndex = 0;

		m_nFaceCount = 0;

		// Get number of face sets in current mesh
		m_nFaceSetCount = m_pMesh->FaceSetCount();
		if ( m_nFaceSetCount <= 0 )
			return false;

		// Get number of faces in current mesh
		for ( m_nFaceSetIndex = 0; m_nFaceSetIndex < m_nFaceSetCount; ++m_nFaceSetIndex )
		{
			const CDmeFaceSet *pFaceSet = m_pMesh->GetFaceSet( m_nFaceSetIndex );
			m_nFaceCount += pFaceSet->GetFaceCount();
		}
	}
	else if ( !m_pMesh )
	{
		return false;
	}

	// Set indices to point to first index of first face of first face set, accounting for
	// NULL face sets and NULL faces
	for ( m_nFaceSetIndex = 0; m_nFaceSetIndex < m_nFaceSetCount; ++m_nFaceSetIndex )
	{
		if ( SetFaceSet() )
			return true;
	}

	// All face sets were empty or full of nothing but -1's
	Assert( m_nFaceSetIndex == m_nFaceSetCount );
	Assert( m_nFaceCount == 0 );

	m_pFaceSet = NULL;

	m_nFaceSetIndexCount = 0;
	m_nFaceSetIndexIndex = 0;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshFaceIt::Count() const
{
	return m_nFaceCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshFaceIt::VertexCount() const
{
	if ( IsDone() )
		return 0;

	return m_pFaceSet->GetNextPolygonVertexCount( m_nFaceSetIndexIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::IsDone() const
{
	if ( m_nFaceIndex < m_nFaceCount )
	{
		Assert( m_nFaceSetIndex < m_nFaceSetCount );
		Assert( m_nFaceSetIndexIndex < m_nFaceSetIndexCount );
	}
	else
	{
		Assert( m_nFaceSetIndex >= m_nFaceSetCount );
		Assert( m_nFaceSetIndexIndex >= m_nFaceSetIndexCount );
	}

	return m_nFaceIndex >= m_nFaceCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::Next()
{
	// Set indices to point to first index of first face of first face set, accounting for
	// NULL face sets and NULL faces

	while ( m_nFaceSetIndex < m_nFaceSetCount )
	{
		// Skip to next -1 face delimiter
		while ( m_nFaceSetIndexIndex < m_nFaceSetIndexCount )
		{
			if ( m_pFaceSet->GetIndex( m_nFaceSetIndexIndex ) >= 0 )
				break;

			++m_nFaceSetIndexIndex;
		}

		// Skip to next face index
		while ( m_nFaceSetIndexIndex < m_nFaceSetIndexCount )
		{
			if ( m_pFaceSet->GetIndex( m_nFaceSetIndexIndex ) < 0 )
				break;

			++m_nFaceSetIndexIndex;
		}

		if ( m_nFaceSetIndexIndex < m_nFaceSetIndexCount )
		{
			++m_nFaceIndex;
			Assert( m_nFaceIndex < m_nFaceCount );
			return true;
		}

		// Must increment the face set
		++m_nFaceSetIndex;
		SetFaceSet();
	}

	// At the end of the iteration
	Assert( IsDone() );

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::SetFaceSet()
{
	if ( !m_pMesh )
	{
		m_pFaceSet = NULL;
		m_nFaceSetIndexCount = 0;
		m_nFaceSetIndexIndex = 0;

		return false;
	}

	if ( m_nFaceSetIndex >= m_nFaceSetCount )
	{
		m_pFaceSet = NULL;
		m_nFaceSetIndexCount = 0;
		m_nFaceSetIndexIndex = 0;

		return false;
	}

	m_pFaceSet = m_pMesh->GetFaceSet( m_nFaceSetIndex );
	m_nFaceSetIndexCount = m_pFaceSet->NumIndices();
	m_nFaceSetIndexIndex = 0;

	// Skip to the first valid face index
	for ( m_nFaceSetIndexIndex = 0; m_nFaceSetIndexIndex < m_nFaceSetIndexCount; ++m_nFaceSetIndexIndex )
	{
		if ( m_pFaceSet->GetIndex( m_nFaceSetIndex ) >= 0 )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::GetVertexIndices( int *pIndices, int nIndices ) const
{
	if ( IsDone() || nIndices != VertexCount() )
	{
		memset( pIndices, 0, nIndices * sizeof( int ) );
		return false;
	}

	int vertexIndex;

	for ( int i = m_nFaceSetIndexIndex; i < m_nFaceSetIndexCount; ++i )
	{
		vertexIndex = m_pFaceSet->GetIndex( i );
		if ( vertexIndex < 0 )
		{
			Assert( i == m_nFaceSetIndexIndex + VertexCount() );
			return true;
		}

		Assert( i < m_nFaceSetIndexIndex + VertexCount() );

		*pIndices = vertexIndex;
		++pIndices;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshFaceIt::GetVertexIndices( CUtlVector< int > &vertexIndices ) const
{
	vertexIndices.SetCount( VertexCount() );

	if ( IsDone() )
	{
		memset( vertexIndices.Base(), 0, vertexIndices.Count() * sizeof( int ) );
		return false;
	}

	return GetVertexIndices( vertexIndices.Base(), vertexIndices.Count() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshFaceIt::GetVertexIndex( int nFaceRelativeVertexIndex ) const
{
	if ( IsDone() )
		return -1;

	const int nVertexCount = VertexCount();
	if ( nVertexCount <= 0 || nFaceRelativeVertexIndex < 0 || nFaceRelativeVertexIndex >= nVertexCount )
		return -1;

	int *pVertexIndices = reinterpret_cast< int * >( alloca( nVertexCount * sizeof( int ) ) );
	if ( !GetVertexIndices( pVertexIndices, nVertexCount ) )
		return -1;

	return pVertexIndices[ nFaceRelativeVertexIndex ];
}


//-----------------------------------------------------------------------------
// Copied from dmeanimationset.cpp, remove this function
// after further integrations
//-----------------------------------------------------------------------------
ControlIndex_t FindComboOpControlIndexForAnimSetControl( CDmeCombinationOperator *pComboOp, const char *pControlName, bool *pIsMulti /*= NULL*/ )
{
	const char *pMultiControlBaseName = pControlName ? StringAfterPrefix( pControlName, "multi_" ) : NULL;
	if ( pIsMulti )
	{
		*pIsMulti = pMultiControlBaseName != NULL;
	}

	if ( !pComboOp || !pControlName )
		return -1;

	ControlIndex_t index = pComboOp->FindControlIndex( pControlName );
	if ( index >= 0 )
		return index;

	if ( !pMultiControlBaseName )
		return -1;

	index = pComboOp->FindControlIndex( pMultiControlBaseName );
	if ( index < 0 )
		return -1;

	Assert( pComboOp->IsMultiControl( index ) );

	return index;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmMeshUtils::CreateDeltasFromPresetGroup(
	CDmePresetGroup *pPresetGroup,
	CDmeCombinationOperator * pComboOp,
	const CUtlVector< CUtlString > *pPurgeAllButThese,
	CDmeMesh *pMesh,
	CDmeVertexData *pDst,
	CUtlStringMap< CUtlString > &conflictingNames,
	CUtlStringMap< CDmePreset * > &presetMap )
{
	const CDmaElementArray< CDmePreset > &presets = pPresetGroup->GetPresets();
	const int nPresetsCount = presets.Count();

	if ( nPresetsCount <= 0 )
		return;

	for ( int i = 0; i < nPresetsCount; ++i )
	{
		pComboOp->SetToBase();
		CDmePreset *pPreset = presets[ i ];

		CDmaElementArray< CDmElement > &controlValues = pPreset->GetControlValues();
		const int nControlValues = controlValues.Count();
		for ( int j = 0; j < nControlValues; ++j )
		{
			CDmElement *pControlPreset = controlValues[ j ];

			const ControlIndex_t nControlIndex = pComboOp->FindControlIndex( pControlPreset->GetName() );
			if ( nControlIndex < 0 )
				continue;

			bool bSkip = false;

			if ( pPurgeAllButThese )
			{
				for ( int k = 0; k < pPurgeAllButThese->Count(); ++k )
				{
					if ( !Q_strcmp( pControlPreset->GetName(), pPurgeAllButThese->Element( k ).Get() ) )
					{
						bSkip = true;
					}
				}
			}

			if ( bSkip )
				continue;

			if ( pComboOp->IsStereoControl( nControlIndex ) )
			{
				pComboOp->SetControlValue(
					nControlIndex,
					pControlPreset->GetValue< float >( "value", 0.0 ),
					pControlPreset->GetValue< float >( "balance", 0.5 ) );
			}
			else
			{
				pComboOp->SetControlValue(
					nControlIndex,
					pControlPreset->GetValue< float >( "value", 0.0 ) );
			}

			if ( pComboOp->IsMultiControl( nControlIndex ) )
			{
				pComboOp->SetMultiControlLevel(
					nControlIndex,
					pControlPreset->GetValue< float >( "multilevel", 0.5 ) );
			}
		}

		// Pass the control data from the DmeCombinationOperator into the mesh
		pComboOp->Resolve();
		pComboOp->Operate();

		pMesh->Resolve();
		pMesh->SetBaseStateToDeltas( pDst );

		CUtlString presetName = pPreset->GetName();

		// Look for any conflicting pre-existing names
		for ( int presetSuffix = 1; pComboOp->FindControlIndex( presetName ) >= 0 || pMesh->FindDeltaState( presetName ) != NULL || conflictingNames.Defined( presetName ) || presetMap.Defined( presetName ); ++presetSuffix )
		{
			presetName = pPreset->GetName();
			presetName += presetSuffix;
		}

		if ( Q_strcmp( pPreset->GetName(), presetName ) )
		{
			// Had to rename preset... save name for later renaming back
			conflictingNames[ presetName ] = pPreset->GetName();
		}

		presetMap[ presetName ] = pPreset;

		pMesh->ModifyOrCreateDeltaStateFromBaseState( presetName, pDst, true );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmMeshUtils::PurgeUnreferencedDeltas( CDmeMesh *pMesh, CUtlStringMap< CDmePreset * > &presetMap, const CUtlVector< CUtlString > *pPurgeAllButThese, CDmeCombinationOperator *pComboOp )
{
	// Loop because deleting changes indexing
	bool bDeleted = false;
	do 
	{
		bDeleted = false;
		for ( int i = 0; i < pMesh->DeltaStateCount(); ++i )
		{
			const char *pDeltaStateName = pMesh->GetDeltaState( i )->GetName();

			if ( presetMap.Defined( pDeltaStateName ) )
				continue;

			bool bDelete = true;

			if ( pPurgeAllButThese )
			{
				for ( int j = 0; j < pPurgeAllButThese->Count(); ++j )
				{
					if ( !Q_strcmp( pDeltaStateName, pPurgeAllButThese->Element( j ).Get() ) )
					{
						bDelete = false;
						break;
					}

					const ControlIndex_t nControlIndex = pComboOp->FindControlIndex( pPurgeAllButThese->Element( j ) );
					if ( nControlIndex < 0 )
						continue;

					for ( int k = 0; k < pComboOp->GetRawControlCount( nControlIndex ); ++k )
					{
						if ( !Q_strcmp( pDeltaStateName, pComboOp->GetRawControlName( nControlIndex, k ) ) )
						{
							bDelete = false;
							break;
						}
					}
				}
			}

			if ( bDelete )
			{
				pMesh->DeleteDeltaState( pDeltaStateName );
				bDeleted = true;
				break;
			}
		}
	} while( bDeleted );

	// Loop because deleting changes indexing
	do 
	{
		bDeleted = false;
		for ( int i = 0; i < pComboOp->GetControlCount(); ++i )
		{
			const char *pControlName = pComboOp->GetControlName( i );

			if ( presetMap.Defined( pControlName ) )
				continue;

			bool bDelete = true;

			if ( pPurgeAllButThese )
			{
				for ( int j = 0; j < pPurgeAllButThese->Count(); ++j )
				{
					if ( !Q_strcmp( pControlName, pPurgeAllButThese->Element( j ) ) )
					{
						bDelete = false;
						break;
					}
				}
			}

			if ( bDelete )
			{
				pComboOp->RemoveControl( pControlName );
				bDeleted = true;
				break;
			}
		}
	} while( bDeleted );

	// Rename any that can be renamed... which should be all of them
	for ( int i = 0; i < presetMap.GetNumStrings(); ++i )
	{
		const char *pPresetName = presetMap.String( i );
		CDmePreset *pPreset = presetMap[ i ];

		if ( Q_strcmp( pPreset->GetName(), pPresetName ) )
		{
			const ControlIndex_t nOrigIndex = pComboOp->FindControlIndex( pPreset->GetName() );
			const ControlIndex_t nRenamedIndex = pComboOp->FindControlIndex( pPresetName );
			CDmeVertexDeltaData *pOrigDelta = pMesh->FindDeltaState( pPreset->GetName() );
			CDmeVertexDeltaData *pRenamedDelta = pMesh->FindDeltaState( pPresetName );

			if ( nOrigIndex < 0 && nRenamedIndex >= 0 && pOrigDelta == NULL && pRenamedDelta != NULL )
			{
				pComboOp->RemoveControl( pPresetName );
				pRenamedDelta->SetName( pPreset->GetName() );
			}
		}
	}
}
