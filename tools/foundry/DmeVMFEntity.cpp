//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "DmeVMFEntity.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "toolframework/itoolentity.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "engine/iclientleafsystem.h"
#include "toolutils/enginetools_int.h"
#include "foundrytool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SPHERE_RADIUS 16

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeVMFEntity, CDmeVMFEntity );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeVMFEntity::OnConstruction()
{
	m_ClassName.Init( this, "classname" );
	m_TargetName.Init( this, "targetname" );
	m_bIsPlaceholder.InitAndSet( this, "_placeholder", false, FATTRIB_DONTSAVE );
	m_vecLocalOrigin.Init( this, "origin" );
	m_vecLocalAngles.Init( this, "angles" );

	// Used to make sure these aren't saved if they aren't changed
	m_TargetName.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );
	m_vecLocalAngles.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );

	m_hEngineEntity = HTOOLHANDLE_INVALID;

	m_Wireframe.Init( "debug/debugwireframe", "editor" );
}

void CDmeVMFEntity::OnDestruction()
{
	// Unhook it from the engine
	AttachToEngineEntity( false );
	m_Wireframe.Shutdown();
}


//-----------------------------------------------------------------------------
// Called whem attributes change
//-----------------------------------------------------------------------------
void CDmeVMFEntity::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	// Once these have changed, then save them out, and don't bother calling back
	if ( pAttribute == m_TargetName.GetAttribute() ||
		 pAttribute == m_vecLocalAngles.GetAttribute() )
	{
		pAttribute->RemoveFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );
		return;
	}
}

	
//-----------------------------------------------------------------------------
// Returns the entity ID
//-----------------------------------------------------------------------------
int CDmeVMFEntity::GetEntityId() const
{
	return atoi( GetName() );
}


//-----------------------------------------------------------------------------
// Entity Key iteration
//-----------------------------------------------------------------------------
bool CDmeVMFEntity::IsEntityKey( CDmAttribute *pEntityKey )
{
	return pEntityKey->IsFlagSet( FATTRIB_USERDEFINED );
}

CDmAttribute *CDmeVMFEntity::FirstEntityKey()
{
	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsEntityKey( pAttribute ) )
			return pAttribute;
	}
	return NULL;
}

CDmAttribute *CDmeVMFEntity::NextEntityKey( CDmAttribute *pEntityKey )
{
	if ( !pEntityKey )
		return NULL;

	for ( CDmAttribute *pAttribute = pEntityKey->NextAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsEntityKey( pAttribute ) )
			return pAttribute;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Attach/detach from an engine entity with the same editor index
//-----------------------------------------------------------------------------
void CDmeVMFEntity::AttachToEngineEntity( bool bAttach )
{
	if ( !bAttach )
	{
		m_hEngineEntity = HTOOLHANDLE_INVALID;
	}
	else
	{
	}
}

	
//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
int CDmeVMFEntity::DrawModel( int flags )
{
	Assert( IsDrawingInEngine() );

	matrix3x4_t mat;
	AngleMatrix( m_vecLocalAngles, m_vecLocalOrigin, mat );

	CMatRenderContextPtr rc( g_pMaterialSystem->GetRenderContext() );

	rc->MatrixMode( MATERIAL_MODEL );
	rc->PushMatrix();
	rc->LoadMatrix( mat );

	int nTheta = 20, nPhi = 20;
	float flRadius = SPHERE_RADIUS;
	int nVertices =  nTheta * nPhi;
	int nIndices = 2 * ( nTheta + 1 ) * ( nPhi - 1 );
	
	rc->FogMode( MATERIAL_FOG_NONE );
	rc->SetNumBoneWeights( 0 );
	rc->Bind( m_Wireframe );
	rc->CullMode( MATERIAL_CULLMODE_CW );

	IMesh* pMesh = rc->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, nVertices, nIndices );

	//
	// Build the index buffer.
	//
	int i, j;
	for ( i = 0; i < nPhi; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			float u = j / ( float )(nTheta - 1);
			float v = i / ( float )(nPhi - 1);
			float theta = ( j != nTheta-1 ) ? 2.0f * M_PI * u : 0.0f;
			float phi = M_PI * v;

			Vector vecPos;
			vecPos.x = flRadius * sin(phi) * cos(theta);
			vecPos.y = flRadius * cos(phi);
			vecPos.z = -flRadius * sin(phi) * sin(theta); 
			    
			unsigned char red = (int)( u * 255.0f );
			unsigned char green = (int)( v * 255.0f );
			unsigned char blue = (int)( v * 255.0f );
			unsigned char alpha = (int)( v * 255.0f );

			meshBuilder.Position3fv( vecPos.Base() );
			meshBuilder.Color4ub( red, green, blue, alpha );
			meshBuilder.TexCoord2f( 0, u, v );
			meshBuilder.BoneWeight( 0, 1.0f );
			meshBuilder.BoneMatrix( 0, 0 );
			meshBuilder.AdvanceVertex();
		}
	}

	//
	// Emit the triangle strips.
	//
	int idx = 0;
	for ( i = 0; i < nPhi - 1; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			idx = nTheta * i + j;

			meshBuilder.FastIndex( idx );
			meshBuilder.FastIndex( idx + nTheta );
		}

		//
		// Emit a degenerate triangle to skip to the next row without
		// a connecting triangle.
		//
		if ( i < nPhi - 2 )
		{
			meshBuilder.FastIndex( idx + 1 );
			meshBuilder.FastIndex( idx + 1 + nTheta );
		}
	}

	meshBuilder.End();
	pMesh->Draw();

	rc->CullMode( MATERIAL_CULLMODE_CCW );
	rc->MatrixMode( MATERIAL_MODEL );
	rc->PopMatrix();

	return 0; 
}


//-----------------------------------------------------------------------------
// Position and bounds for the model
//-----------------------------------------------------------------------------
const Vector &CDmeVMFEntity::GetRenderOrigin( void )
{
	return m_vecLocalOrigin;
}

const QAngle &CDmeVMFEntity::GetRenderAngles( void )
{
	return m_vecLocalAngles;
}

void CDmeVMFEntity::GetRenderBounds( Vector& mins, Vector& maxs )
{ 
	mins.Init( -SPHERE_RADIUS, -SPHERE_RADIUS, -SPHERE_RADIUS );
	maxs.Init( SPHERE_RADIUS, SPHERE_RADIUS, SPHERE_RADIUS );
}
