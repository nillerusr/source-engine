//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmemorphoperator.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMorphOperator, CDmeMorphOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMorphOperator::OnConstruction()
{
	m_mesh.Init( this, "mesh", FATTRIB_HAS_CALLBACK );
	m_deltaStateWeights.Init( this, "deltaStateWeights", FATTRIB_MUSTCOPY );
	m_baseStateName.Init( this, "baseStateName", FATTRIB_TOPOLOGICAL );
}

void CDmeMorphOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
uint CDmeMorphOperator::NumDeltaStateWeights()
{
	return m_deltaStateWeights.Count();
}

CDmElement *CDmeMorphOperator::GetDeltaStateWeight( uint i )
{
	return m_deltaStateWeights[ i ];
}

CDmeMesh *CDmeMorphOperator::GetMesh()
{
	return m_mesh.GetElement();
}


//-----------------------------------------------------------------------------
// This function gets called whenever an attribute changes
//-----------------------------------------------------------------------------
void CDmeMorphOperator::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( pAttribute == m_mesh.GetAttribute() )
	{
		CDmeMesh *pMesh = GetMesh();
		if ( pMesh )
		{
#if 0 // right now, the file already contains these weights, and re-creating them breaks the channel connections
			m_deltaStateWeights.RemoveAll();

			uint dn = pMesh->NumDeltaStates();
			for ( uint di = 0; di < dn; ++di )
			{
				CDmElement *pDeltaState = pMesh->GetDeltaState( di );
				const char *name = pDeltaState->GetName();

				CDmElement *pDeltaWeight = CreateElement< CDmElement >( name, GetFileId() );
				pDeltaWeight->SetAttributeValue( "weight", 0.0f );

				m_deltaStateWeights.AddToTail( pDeltaWeight->GetHandle() );
			}
#endif
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMorphOperator::Operate()
{
	CDmeMesh *mesh = GetMesh();

	uint mn = NumDeltaStateWeights();
	for ( uint mi = 0; mi < mn; ++mi )
	{
		CDmElement *pDeltaState = GetDeltaStateWeight( mi );
		const char *deltaName = pDeltaState->GetName();
		float deltaWeight = pDeltaState->GetValue< float >( "weight" );

		int di = mesh->FindDeltaStateIndex( deltaName );
		if ( di != -1 )
		{
			mesh->SetDeltaStateWeight( di, MESH_DELTA_WEIGHT_NORMAL, deltaWeight );
		}
		else
		{
			Msg( "MorphOperator::Operate: invalid delta state name: %s\n", deltaName );
		}
	}
}

// hack to avoid MSVC complaining about multiply defined symbols
namespace MorphOp
{
void AddAttr( CUtlVector< CDmAttribute * > &attrs, CDmAttribute *pAttr )
{
	if ( pAttr == NULL )
		return;
	attrs.AddToTail( pAttr );
}

void AddVertexAttributes( CUtlVector< CDmAttribute * > &attrs, CDmElement *pObject )
{
	AddAttr( attrs, pObject->GetAttribute( "coordinates" ) );
	AddAttr( attrs, pObject->GetAttribute( "normals" ) );
	AddAttr( attrs, pObject->GetAttribute( "textureCoordinates" ) );
	// TODO - add colors, occlusionFactors, boneIndices*, boneWeights*, tangents
}
};
using namespace MorphOp;

void CDmeMorphOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	uint nWeights = NumDeltaStateWeights();
	for ( uint wi = 0; wi < nWeights; ++wi )
	{
		CDmElement *pDelta = GetDeltaStateWeight( wi );
		AddAttr( attrs, pDelta->GetAttribute( "weight" ) );
	}

	CDmeMesh *pMesh = GetMesh();
	CDmeVertexData *pBaseState = pMesh->FindBaseState( m_baseStateName.Get() );
	AddVertexAttributes( attrs, pBaseState );

	uint nDeltas = pMesh->DeltaStateCount();
	for ( uint di = 0; di < nDeltas; ++di )
	{
		CDmElement *pDeltaState = pMesh->GetDeltaState( di );
		AddAttr( attrs, pDeltaState->GetAttribute( "indices" ) );
		AddVertexAttributes( attrs, pDeltaState );
	}
}

void CDmeMorphOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	AddVertexAttributes( attrs, GetMesh() );
}
