//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeshape.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects_interfaces.h"
#include "movieobjects/dmedag.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeShape, CDmeShape );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeShape::OnConstruction()
{
	m_visible.InitAndSet( this, "visible", true );
}

void CDmeShape::OnDestruction()
{
}

void CDmeShape::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	Assert( 0 );
}


//-----------------------------------------------------------------------------
// The default bounding sphere is empty at the origin
//-----------------------------------------------------------------------------
void CDmeShape::GetBoundingSphere( Vector &c, float &r ) const
{
	c.Zero();
	r = 0.0f;
}
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeShape::GetParentCount() const
{
	int nReferringDags = 0;

	DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
	while ( i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );
		CDmeDag *pDag = CastElement< CDmeDag >( pAttribute->GetOwner() );
		const static UtlSymId_t symShape = g_pDataModel->GetSymbol( "shape" );
		if ( pDag && pAttribute->GetNameSymbol() == symShape && pDag->GetFileId() == GetFileId()  )
		{
			++nReferringDags;
		}

		i = g_pDataModel->NextAttributeReferencingElement( i );
	}

	return nReferringDags;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeDag *CDmeShape::GetParent( int nParentIndex /*= 0 */ ) const
{
	int nReferringDags = 0;

	DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
	while ( i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );
		CDmeDag *pDag = CastElement< CDmeDag >( pAttribute->GetOwner() );
		const static UtlSymId_t symShape = g_pDataModel->GetSymbol( "shape" );
		if ( pDag && pAttribute->GetNameSymbol() == symShape && pDag->GetFileId() == GetFileId()  )
		{
			if ( nReferringDags == nParentIndex )
				return pDag;

			++nReferringDags;
		}

		i = g_pDataModel->NextAttributeReferencingElement( i );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeShape::GetShapeToWorldTransform( matrix3x4_t &mat, int nParentIndex /*= 0 */ ) const
{
	CDmeDag *pDag = GetParent( nParentIndex );

	if ( pDag )
	{
		pDag->GetShapeToWorldTransform( mat );
	}
	else
	{
		SetIdentityMatrix( mat );
	}
}
