//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a joint of a skeletal model (gets compiled into a MDL)
//
//=============================================================================
#include "movieobjects/dmejoint.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeJoint, CDmeJoint );


//-----------------------------------------------------------------------------
// Should I draw joints? 
//-----------------------------------------------------------------------------
bool CDmeJoint::s_bDrawJoints = false;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeJoint::OnConstruction()
{
	if ( !g_pMaterialSystem )
		return;

	KeyValues *pVMTKeyValues = new KeyValues( "wireframe" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_JointMaterial.Init( "__DmeJointMaterial", pVMTKeyValues );
}

void CDmeJoint::OnDestruction()
{
	if ( !g_pMaterialSystem )
		return;

	m_JointMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
// Activate, deactivate joint drawing 
//-----------------------------------------------------------------------------
void CDmeJoint::DrawJointHierarchy( bool bDrawJoints )
{
	s_bDrawJoints = bDrawJoints;
}


//-----------------------------------------------------------------------------
// For rendering joints
//-----------------------------------------------------------------------------
#define AXIS_SIZE 3.0f

void CDmeJoint::DrawJoints( )
{
	if ( !g_pMaterialSystem )
		return;

	int cn = GetChildCount();

	// Draw the joint hierarchy
	PushDagTransform();
	matrix3x4_t shapeToWorld;
	GetShapeToWorldTransform( shapeToWorld );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadMatrix( shapeToWorld );

	pRenderContext->Bind( m_JointMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 + cn );

	for ( int ci = 0; ci < cn; ++ci )
	{
		CDmeJoint *pJoint = CastElement<CDmeJoint>( GetChild( ci ) );
		if ( !pJoint )
			continue;

		Vector vecChildPosition = pJoint->GetTransform()->GetPosition();

		meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
		meshBuilder.Color4ub( 128, 128, 128, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( vecChildPosition.Base() );
		meshBuilder.Color4ub( 128, 128, 128, 255 );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( AXIS_SIZE, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, AXIS_SIZE, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, AXIS_SIZE );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	PopDagTransform();
}


//-----------------------------------------------------------------------------
// Rendering method for the dag
//-----------------------------------------------------------------------------
void CDmeJoint::Draw( CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	if ( s_bDrawJoints && IsVisible() )
	{
		DrawJoints();
	}

	BaseClass::Draw( pDrawSettings );
}


