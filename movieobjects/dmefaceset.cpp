//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "tier0/dbg.h"
#include "UtlBuffer.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFaceSet, CDmeFaceSet );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeFaceSet::OnConstruction()
{
	m_indices.Init( this, "faces" );
	m_material.Init( this, "material" );
}

void CDmeFaceSet::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
CDmeMaterial *CDmeFaceSet::GetMaterial()
{
	return m_material.GetElement();
}

void CDmeFaceSet::SetMaterial( CDmeMaterial *pMaterial )
{
	m_material = pMaterial;
}

int CDmeFaceSet::AddIndices( int nCount )
{
	int nCurrentCount = m_indices.Count();
	m_indices.EnsureCount( nCount + nCurrentCount );
	return nCurrentCount;
}

void CDmeFaceSet::SetIndices( int nFirstIndex, int nCount, int *pIndices )
{
	m_indices.SetMultiple( nFirstIndex, nCount, pIndices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFaceSet::SetIndex( int i, int nValue )
{
	m_indices.Set( i, nValue );
}


//-----------------------------------------------------------------------------
// Returns the number of triangulated indices
//-----------------------------------------------------------------------------
int CDmeFaceSet::GetNextPolygonVertexCount( int nFirstIndex ) const
{
	int nCurrIndex = nFirstIndex;
	int nTotalCount = m_indices.Count();
	while( nCurrIndex < nTotalCount )
	{
		if ( m_indices[nCurrIndex] == -1 )
			break;
		++nCurrIndex;
	}

	return nCurrIndex - nFirstIndex;
}


//-----------------------------------------------------------------------------
// Returns the number of triangulated indices total
//-----------------------------------------------------------------------------
int CDmeFaceSet::GetTriangulatedIndexCount() const
{
	int nIndexCount = 0;
	int nVertexCount = 0;
	int nTotalCount = m_indices.Count();
	for ( int nCurrIndex = 0; nCurrIndex < nTotalCount; ++nCurrIndex )
	{
		if ( m_indices[nCurrIndex] == -1 )
		{
			if ( nVertexCount >= 3 )
			{
				nIndexCount += ( nVertexCount - 2 ) * 3;
			}
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	if ( nVertexCount >= 3 )
	{
		nIndexCount += ( nVertexCount - 2 ) * 3;
	}

	return nIndexCount;
}


//-----------------------------------------------------------------------------
// Returns the number of indices total
//-----------------------------------------------------------------------------
int CDmeFaceSet::GetIndexCount() const
{
	int nIndexCount = 0;
	int nVertexCount = 0;
	int nTotalCount = m_indices.Count();

	for ( int nCurrIndex = 0; nCurrIndex < nTotalCount; ++nCurrIndex )
	{
		if ( m_indices[nCurrIndex] == -1 )
		{
			nIndexCount += nVertexCount;
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	nIndexCount += nVertexCount;

	return nIndexCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFaceSet::RemoveMultiple( int elem, int num )
{
	m_indices.RemoveMultiple( elem, num );
}


//-----------------------------------------------------------------------------
// Returns the number of faces in the face set
//-----------------------------------------------------------------------------
int CDmeFaceSet::GetFaceCount() const
{
	int nFaceCount = 0;
	int nVertexCount = 0;

	const int nIndexCount = NumIndices();
	for ( int i = 0; i < nIndexCount; ++i )
	{
		if ( GetIndex( i ) < 0 )
		{
			if ( nVertexCount > 0 )
			{
				++nFaceCount;
			}
			nVertexCount = 0;
			continue;
		}

		++nVertexCount;
	}

	if ( nVertexCount > 0 )
	{
		++nFaceCount;
	}

	return nFaceCount;
}