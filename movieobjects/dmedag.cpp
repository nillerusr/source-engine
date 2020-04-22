//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeshape.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects_interfaces.h"
#include "movieobjects/dmedrawsettings.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDag, CDmeDag );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CUtlStack<CDmeDag::TransformInfo_t> CDmeDag::s_TransformStack;
bool CDmeDag::s_bDrawUsingEngineCoordinates = false;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeDag::OnConstruction()
{
	m_Transform.InitAndCreate( this, "transform", GetName() );
	m_Shape.Init( this, "shape" );
	m_Visible.InitAndSet( this, "visible", true, FATTRIB_HAS_CALLBACK );
	m_Children.Init( this, "children" );
}

void CDmeDag::OnDestruction()
{
	g_pDataModel->DestroyElement( m_Transform.Get() );
}


//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
CDmeTransform *CDmeDag::GetTransform()
{
	return m_Transform.GetElement();
}

CDmeShape *CDmeDag::GetShape()
{
	return m_Shape.GetElement();
}

void CDmeDag::SetShape( CDmeShape *pShape )
{
	m_Shape = pShape;
}


bool CDmeDag::IsVisible() const
{
	return m_Visible;
}

void CDmeDag::SetVisible( bool bVisible )
{
	m_Visible = bVisible;
}


//-----------------------------------------------------------------------------
// Returns the visibility attribute for DmeRenderable support
//-----------------------------------------------------------------------------
CDmAttribute *CDmeDag::GetVisibilityAttribute() 
{ 
	return m_Visible.GetAttribute(); 
}


//-----------------------------------------------------------------------------
// child helpers
//-----------------------------------------------------------------------------
const CUtlVector< DmElementHandle_t > &CDmeDag::GetChildren() const
{
	return m_Children.Get();
}

int CDmeDag::GetChildCount() const
{
	return m_Children.Count();
}

CDmeDag *CDmeDag::GetChild( int i ) const
{
	if ( i < 0 || i >= m_Children.Count() )
		return NULL;

	return m_Children.Get( i );
}

void CDmeDag::AddChild( CDmeDag* pDag )
{
	m_Children.AddToTail( pDag );
}

void CDmeDag::RemoveChild( int i )
{
	m_Children.FastRemove( i );
}

void CDmeDag::RemoveChild( const CDmeDag *pChild, bool bRecurse )
{
	int i = FindChild( pChild );
	if ( i >= 0 )
	{
		RemoveChild( i );
	}
}

int CDmeDag::FindChild( const CDmeDag *pChild ) const
{
	return m_Children.Find( pChild->GetHandle() );
}

// recursive
int CDmeDag::FindChild( CDmeDag *&pParent, const CDmeDag *pChild )
{
	int index = FindChild( pChild );
	if ( index >= 0 )
	{
		pParent = this;
		return index;
	}

	int nChildren = m_Children.Count();
	for ( int ci = 0; ci < nChildren; ++ci )
	{
		index = m_Children[ ci ]->FindChild( pParent, pChild );
		if ( index >= 0 )
			return index;
	}

	pParent = NULL;
	return -1;
}

int CDmeDag::FindChild( const char *name ) const
{
	int nChildren = m_Children.Count();
	for ( int ci = 0; ci < nChildren; ++ci )
	{
		if ( V_strcmp( m_Children[ ci ]->GetName(), name ) == 0 )
			return ci;
	}
	return -1;
}

CDmeDag *CDmeDag::FindOrAddChild( const char *name )
{
	int i = FindChild( name );
	if ( i >= 0 )
		return GetChild( i );

	CDmeDag *pChild = CreateElement< CDmeDag >( name, GetFileId() );
	AddChild( pChild );
	return pChild;
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::PushDagTransform()
{
	int i = s_TransformStack.Push();
	TransformInfo_t &info = s_TransformStack[i];
	info.m_pTransform = GetTransform();
	info.m_bComputedDagToWorld = false;
}

void CDmeDag::PopDagTransform()
{
	Assert( s_TransformStack.Top().m_pTransform == GetTransform() );
	s_TransformStack.Pop();
}


//-----------------------------------------------------------------------------
// Transform from DME to engine coordinates
//-----------------------------------------------------------------------------
void CDmeDag::DmeToEngineMatrix( matrix3x4_t& dmeToEngine )
{
	VMatrix rotation, rotationZ;
	MatrixBuildRotationAboutAxis( rotation, Vector( 1, 0, 0 ), 90 );
	MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 1, 0 ), 90 );
	ConcatTransforms( rotation.As3x4(), rotationZ.As3x4(), dmeToEngine );
}

//-----------------------------------------------------------------------------
// Transform from engine to DME coordinates
//-----------------------------------------------------------------------------
void CDmeDag::EngineToDmeMatrix( matrix3x4_t& engineToDme )
{
	VMatrix rotation, rotationZ;
	MatrixBuildRotationAboutAxis( rotation, Vector( 1, 0, 0 ), -90 );
	MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 1, 0 ), -90 );
	ConcatTransforms( rotationZ.As3x4(), rotation.As3x4(), engineToDme );
}


void CDmeDag::GetShapeToWorldTransform( matrix3x4_t &mat )
{
	int nCount = s_TransformStack.Count();
	if ( nCount == 0 )
	{
		if ( !s_bDrawUsingEngineCoordinates )
		{
			SetIdentityMatrix( mat );
		}
		else
		{
			DmeToEngineMatrix( mat );
		}
		return;
	}

	if ( s_TransformStack.Top().m_bComputedDagToWorld )
	{
		MatrixCopy( s_TransformStack.Top().m_DagToWorld, mat );
		return;
	}

	// Compute all uncomputed dag to worls
	int i;
	for ( i = 0; i < nCount; ++i )
	{
		TransformInfo_t &info = s_TransformStack[i];
		if ( !info.m_bComputedDagToWorld )
			break;
	}

	// Set up the initial transform
	if ( i == 0 )
	{
		if ( !s_bDrawUsingEngineCoordinates )
		{
			SetIdentityMatrix( mat );
		}
		else
		{
			DmeToEngineMatrix( mat );
		}
	}
	else
	{
		MatrixCopy( s_TransformStack[i-1].m_DagToWorld, mat );
	}

	// Compute all transforms
	for ( ; i < nCount; ++i )
	{
		matrix3x4_t localToParent;
		TransformInfo_t &info = s_TransformStack[i];
		info.m_pTransform->GetTransform( localToParent );
		ConcatTransforms( mat, localToParent, info.m_DagToWorld );
		info.m_bComputedDagToWorld = true;
		MatrixCopy( info.m_DagToWorld, mat );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetLocalMatrix( matrix3x4_t &m )
{
	CDmeTransform *pTransform = GetTransform();
	if ( pTransform )
	{
		pTransform->GetTransform( m );
	}
	else
	{
		SetIdentityMatrix( m );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetWorldMatrix( matrix3x4_t &m )
{
	GetLocalMatrix( m );
	const static UtlSymId_t symChildren = g_pDataModel->GetSymbol( "children" );
	CDmeDag *pParent = FindReferringElement< CDmeDag >( this, symChildren );
	if ( pParent )
	{
		matrix3x4_t localMatrix;
		GetLocalMatrix( localMatrix );

		matrix3x4_t parentWorldMatrix;
		pParent->GetWorldMatrix( parentWorldMatrix );
		ConcatTransforms( parentWorldMatrix, localMatrix, m );
	}
	else
	{
		GetLocalMatrix( m );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetParentWorldMatrix( matrix3x4_t &m )
{
	const static UtlSymId_t symChildren = g_pDataModel->GetSymbol( "children" );
	CDmeDag *pParent = FindReferringElement< CDmeDag >( this, symChildren );
	if ( pParent )
	{
		pParent->GetWorldMatrix( m );
	}
	else
	{
		SetIdentityMatrix( m );
	}
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::DrawUsingEngineCoordinates( bool bEnable )
{
	s_bDrawUsingEngineCoordinates = bEnable;
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::Draw( CDmeDrawSettings *pDrawSettings )
{
	if ( !m_Visible )
		return;

	PushDagTransform();

	CDmeShape *pShape = GetShape();
	if ( pShape )
	{
		matrix3x4_t shapeToWorld;
		GetShapeToWorldTransform( shapeToWorld );
		pShape->Draw( shapeToWorld, pDrawSettings );
	}

	uint cn = m_Children.Count();
	for ( uint ci = 0; ci < cn; ++ci )
	{
		m_Children[ ci ]->Draw( pDrawSettings );
	}

	PopDagTransform();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetBoundingSphere( Vector &c0, float &r0, const matrix3x4_t &pMat ) const
{
	matrix3x4_t lMat;
	m_Transform.GetElement()->GetTransform( lMat );
	matrix3x4_t wMat;
	ConcatTransforms( pMat, lMat, wMat );

	c0.Zero();
	r0 = 0.0f;

	const CDmeShape *pShape = m_Shape.GetElement();
	if ( pShape )
	{
		pShape->GetBoundingSphere( c0, r0 );
	}

	// No scale in Dme! :)
	Vector vTemp;
	VectorTransform( c0, pMat, vTemp );

	const int nChildren = m_Children.Count();
	if ( nChildren > 0 )
	{
		Vector c1;	// Child center
		float r1;	// Child radius

		Vector v01;	// c1 - c0
		float l01;	// |v01|

		for ( int i = 0; i < nChildren; ++i )
		{
			m_Children[ i ]->GetBoundingSphere( c1, r1, wMat );

			if ( r0 == 0.0f )
			{
				c0 = c1;
				r0 = r1;
				continue;
			}

			v01 = c1 - c0;
			l01 = v01.NormalizeInPlace();

			if ( r0 < l01 + r1 )
			{
				// Current sphere doesn't contain both spheres
				if ( r1 < l01 + r0 )
				{
					// Child sphere doesn't contain both spheres
					c0 = c0 + 0.5f * ( r1 + l01 - r0 ) * v01;
					r0 = 0.5f * ( r0 + l01 + r1 );
				}
				else
				{
					// Child sphere contains both spheres
					c0 = c1;
					r0 = r1;
				}
			}
		}
	}
}