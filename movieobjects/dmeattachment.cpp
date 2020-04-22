//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a model attachment point
//
//=============================================================================
#include "movieobjects/dmeattachment.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAttachment, CDmeAttachment );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAttachment::OnConstruction()
{
	if ( !g_pMaterialSystem )
		return;

	m_bIsRigid.Init( this, "isRigid" );
	m_bIsWorldAligned.Init( this, "isWorldAligned" );

	KeyValues *pVMTKeyValues = new KeyValues( "wireframe" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 0 );
	m_AttachmentMaterial.Init( "__DmeJointMaterial", pVMTKeyValues );
}

void CDmeAttachment::OnDestruction()
{
	if ( !g_pMaterialSystem )
		return;

	m_AttachmentMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
// For rendering joints
//-----------------------------------------------------------------------------
#define AXIS_SIZE 6.0f

//-----------------------------------------------------------------------------
// Rendering method for the dag
//-----------------------------------------------------------------------------
void CDmeAttachment::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( shapeToWorld );

	pRenderContext->Bind( m_AttachmentMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

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
	pRenderContext->PopMatrix();
}


