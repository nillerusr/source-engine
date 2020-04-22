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
#include "vcdblocktool.h"
#include "tier1/KeyValues.h"

// for tracing
#include "cmodel.h"
#include "engine/ienginetrace.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SPHERE_RADIUS 16

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeVMFEntity, CDmeVMFEntity );


//-----------------------------------------------------------------------------
// Used to store the next unique entity id;
//-----------------------------------------------------------------------------
int CDmeVMFEntity::s_nNextEntityId;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeVMFEntity::OnConstruction()
{
	m_ClassName.Init( this, "classname", FATTRIB_HAS_CALLBACK );
	m_TargetName.Init( this, "targetname" );
	m_bIsPlaceholder.InitAndSet( this, "_placeholder", false, FATTRIB_DONTSAVE );
	m_vecLocalOrigin.Init( this, "origin" );
	m_vecLocalAngles.Init( this, "angles" );
	m_bIsDeleted.Init( this, "deleted" );

	if ( m_Name.Length() == 0 )
	{
		// Assign a unique ID to the name
		char pNameString[128];
		Q_snprintf( pNameString, sizeof(pNameString), "%d", GetNextEntityId() );
		m_Name = pNameString;
	}

	// See if we need to bump the unique id up
	int nEntityId = GetEntityId();
	if ( s_nNextEntityId <= nEntityId )
	{
		s_nNextEntityId = nEntityId + 1;
	}

	// Get called back when the name changes
	m_Name.GetAttribute()->AddFlag( FATTRIB_HAS_CALLBACK );

	// Used to make sure these aren't saved if they aren't changed
	//m_TargetName.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );
	//m_vecLocalAngles.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );

	m_bIsDirty = false;
	m_bInfoTarget = false;
	m_hEngineEntity = HTOOLHANDLE_INVALID;

	m_Wireframe.Init( "debug/debugwireframevertexcolor", "editor" );

	// FIXME: Need to abstract out rendering into a separate class
	// based on information parsed from the FGD
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "editor/info_target" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	m_InfoTargetSprite.Init( "__vcdblock_info_target", pVMTKeyValues );

	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetString( "$color", "{64 64 64}" );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$additive", 1 );
	m_SelectedInfoTarget.Init( "__selected_vcdblock_info_target", pVMTKeyValues );
}

void CDmeVMFEntity::OnDestruction()
{
	// Unhook it from the engine
	AttachToEngineEntity( false );
	m_Wireframe.Shutdown();
	m_SelectedInfoTarget.Shutdown();
	m_InfoTargetSprite.Shutdown();
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

	if ( pAttribute == m_ClassName.GetAttribute() )
	{
		m_bInfoTarget = !Q_strncmp( m_ClassName, "info_target", 11 );

		// FIXME: Change the model based on the current class
		SetModelName( NULL );
		return;
	}

	// Make sure we have unique ids for all entities
	if ( pAttribute == m_Name.GetAttribute() )
	{
		int nEntityId = GetEntityId();
		if ( s_nNextEntityId <= nEntityId )
		{
			s_nNextEntityId = nEntityId + 1;
		}
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
// Returns the next available entity id
//-----------------------------------------------------------------------------
int CDmeVMFEntity::GetNextEntityId()
{
	return s_nNextEntityId;
}

void CDmeVMFEntity::SetNextEntityId( int nEntityId )
{
	s_nNextEntityId = nEntityId;
}


//-----------------------------------------------------------------------------
// Mark the entity as being dirty
//-----------------------------------------------------------------------------
void CDmeVMFEntity::MarkDirty( bool bDirty )
{
	m_bIsDirty = bDirty;

	// FIXME: this is doing two operations!!
	CopyToServer();
}


//-----------------------------------------------------------------------------
// Is the renderable transparent?
//-----------------------------------------------------------------------------
bool CDmeVMFEntity::IsTransparent( void )
{
	return m_bIsDirty || m_bInfoTarget || BaseClass::IsTransparent();
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
void CDmeVMFEntity::AttachToEngineEntity( HTOOLHANDLE hToolHandle )
{
	if ( m_hEngineEntity != HTOOLHANDLE_INVALID )
	{
		clienttools->SetEnabled( m_hEngineEntity, true );
	}
	m_hEngineEntity = hToolHandle;
	if ( m_hEngineEntity != HTOOLHANDLE_INVALID )
	{
		clienttools->SetEnabled( m_hEngineEntity, false );
	}
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
	return *(QAngle *)(&m_vecLocalAngles);
}

void CDmeVMFEntity::GetRenderBounds( Vector& mins, Vector& maxs )
{ 
	if ( !m_bInfoTarget )
	{
		BaseClass::GetRenderBounds( mins, maxs );
		return;
	}

	mins.Init( -SPHERE_RADIUS, -SPHERE_RADIUS, -SPHERE_RADIUS );
	maxs.Init( SPHERE_RADIUS, SPHERE_RADIUS, SPHERE_RADIUS );
}


//-----------------------------------------------------------------------------
// Update renderable position
//-----------------------------------------------------------------------------
void CDmeVMFEntity::SetRenderOrigin( const Vector &vecOrigin )
{
	m_vecLocalOrigin = vecOrigin;
	clienttools->MarkClientRenderableDirty( this );
}

void CDmeVMFEntity::SetRenderAngles( const QAngle &angles )
{
	m_vecLocalAngles.Set( Vector( angles.x, angles.y, angles.z ) ); // FIXME: angles is a vector due to the vmf "angles" having a problem parsing...
	clienttools->MarkClientRenderableDirty( this );
}


//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
void CDmeVMFEntity::DrawSprite( IMaterial *pMaterial )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	float t = 0.5f * sin( Plat_FloatTime() * M_PI / 1.0f ) + 0.5f;

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 4, 4 );

	unsigned char nBaseR = 255;
	unsigned char nBaseG = 255;
	unsigned char nBaseB = 255;
	unsigned char nAlpha = m_bIsDirty ? (unsigned char)(255 * t) : 255;

	meshBuilder.Position3f( -SPHERE_RADIUS, -SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( SPHERE_RADIUS, -SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( SPHERE_RADIUS, SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -SPHERE_RADIUS, SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.FastIndex( 0 );
	meshBuilder.FastIndex( 1 );
	meshBuilder.FastIndex( 3 );
	meshBuilder.FastIndex( 2 );

	meshBuilder.End();
	pMesh->Draw();
}



//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
void CDmeVMFEntity::DrawDragHelpers( IMaterial *pMaterial )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	float t = 0.5f * sin( Plat_FloatTime() * M_PI / 1.0f ) + 0.5f;

	VMatrix worldToCamera;
	// pRenderContext->GetMatrix( MATERIAL_VIEW, &worldToCamera );
	worldToCamera.Identity();
	worldToCamera.SetTranslation( m_vecLocalOrigin );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( worldToCamera );

	pRenderContext->FogMode( MATERIAL_FOG_NONE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

	unsigned char nBaseR = 255;
	unsigned char nBaseG = 255;
	unsigned char nBaseB = 255;
	unsigned char nAlpha = m_bIsDirty ? (unsigned char)(255 * t) : 255;

	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( SPHERE_RADIUS * 10, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( 0, SPHERE_RADIUS * 10, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.Position3f( 0, 0, SPHERE_RADIUS * 10 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}



//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
void CDmeVMFEntity::DrawFloorTarget( IMaterial *pMaterial )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	float t = 0.5f * sin( Plat_FloatTime() * M_PI / 1.0f ) + 0.5f;

	// test movement
	Ray_t ray;
	CTraceFilterWorldAndPropsOnly traceFilter;
	CBaseTrace tr;
	ray.Init( m_vecLocalOrigin.Get()+ Vector( 0, 0, 10 ), m_vecLocalOrigin.Get() + Vector( 0,0, -128 ), Vector( -13, -13, 0 ), Vector( 13, 13, 10 ) );
	enginetools->TraceRayServer( ray, MASK_OPAQUE, &traceFilter, &tr );


	VMatrix worldToCamera;
	// pRenderContext->GetMatrix( MATERIAL_VIEW, &worldToCamera );
	worldToCamera.Identity();
	worldToCamera.SetTranslation( tr.endpos );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( worldToCamera );

	pRenderContext->FogMode( MATERIAL_FOG_NONE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

	unsigned char nBaseR = 255;
	unsigned char nBaseG = 255;
	unsigned char nBaseB = 0;
	unsigned char nAlpha = m_bIsDirty ? (unsigned char)(255 * t) : 255;

	float block[4][2][3] = { 
		{ { -13, -13, 0 }, { -13, -13, 10 } },
		{ {  13, -13, 0 }, {  13, -13, 10 } },
		{ {  13,  13, 0 }, {  13,  13, 10 } },
		{ { -13,  13, 0 }, { -13,  13, 10 } } };

	for (int i = 0; i < 4; i++)
	{
		int j = (i + 1) % 4;
		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[i][0][0], block[i][0][1], block[i][0][2] );
		meshBuilder.AdvanceVertex();
		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[i][1][0], block[i][1][1], block[i][1][2] );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[i][0][0], block[i][0][1], block[i][0][2] );
		meshBuilder.AdvanceVertex();
		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[j][0][0], block[j][0][1], block[j][0][2] );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[i][1][0], block[i][1][1], block[i][1][2] );
		meshBuilder.AdvanceVertex();
		meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
		meshBuilder.Position3f( block[j][1][0], block[j][1][1], block[j][1][2] );
		meshBuilder.AdvanceVertex();
	}

	// positive X
	meshBuilder.Color4ub( 255, 0, 0, nAlpha );
	meshBuilder.Position3f( 0, 0, 10 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( 255, 0, 0, nAlpha );
	meshBuilder.Position3f( 10, 0, 10 );
	meshBuilder.AdvanceVertex();

	// positive Y
	meshBuilder.Color4ub( 0, 255, 0, nAlpha );
	meshBuilder.Position3f( 0, 0, 10 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( 0, 255, 0, nAlpha );
	meshBuilder.Position3f( 0, 10, 10 );
	meshBuilder.AdvanceVertex();

	// just Z
	meshBuilder.Color4ub( 255, 255, 255, nAlpha );
	meshBuilder.Position3f( 0, 0, 10 );
	meshBuilder.AdvanceVertex();
	meshBuilder.Color4ub( 255, 255, 255, nAlpha );
	meshBuilder.Position3f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}


//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
int CDmeVMFEntity::DrawModel( int flags )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	bool bSelected = ( g_pVcdBlockTool->GetCurrentEntity().Get() == this );
	if ( !m_bInfoTarget )
	{
		// If we have a visible engine entity, we don't need to draw it here
		// info targets always draw though, because they have no visible model.
		CDisableUndoScopeGuard guard;
		float t = 0.5f * sin( Plat_FloatTime() * M_PI / 1.0f ) + 0.5f;
		unsigned char nAlpha = m_bIsDirty ? (unsigned char)(255 * t) : 255;
		if ( bSelected )
		{
			GetMDL()->m_Color.SetColor( 255, 64, 64, nAlpha );
		}
		else
		{
			GetMDL()->m_Color.SetColor( 255, 255, 255, nAlpha );
		}
		return BaseClass::DrawModel( flags );
	}

	Assert( IsDrawingInEngine() );

	matrix3x4_t mat;
	VMatrix worldToCamera, cameraToWorld;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &worldToCamera );
	MatrixInverseTR( worldToCamera, cameraToWorld );
	MatrixCopy( cameraToWorld.As3x4(), mat );
	MatrixSetColumn( m_vecLocalOrigin, 3, mat );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( mat );

	pRenderContext->FogMode( MATERIAL_FOG_NONE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	DrawSprite( m_InfoTargetSprite );
	if ( bSelected )
	{
		DrawSprite( m_SelectedInfoTarget );

		DrawFloorTarget( m_Wireframe );

		if (g_pVcdBlockTool->IsInNodeDrag()) 
		{
			DrawDragHelpers( m_Wireframe );
		}
		// DrawLine( Vector( 0, 0, 0 ), Vector( 10, 0, 0 ), 0, 255, 255, 255 );
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 1; 
}


bool CDmeVMFEntity::CopyFromServer( CBaseEntity *pServerEnt )
{
	CopyFromServer( pServerEnt, "targetname" );
	CopyFromServer( pServerEnt, "classname" );
	CopyFromServer( pServerEnt, "origin" );
	CopyFromServer( pServerEnt, "angles" );

	return true;
}

bool CDmeVMFEntity::CopyFromServer( CBaseEntity *pServerEnt, const char *szField )
{
	return CopyFromServer( pServerEnt, szField, szField );
}

bool CDmeVMFEntity::CopyFromServer( CBaseEntity *pServerEnt, const char *szSrcField, const char *szDstField )
{
	char text[256];
	if ( servertools->GetKeyValue( pServerEnt, szSrcField, text, sizeof( text ) ) )
	{
		SetValueFromString( szDstField, text );
		return true;
	}
	return false;
}

bool CDmeVMFEntity::CopyToServer( void )
{
	if (GetEntityId() != 0)
	{
		CBaseEntity *pServerEntity = servertools->FindEntityByHammerID( GetEntityId() );
		if (pServerEntity != NULL)
		{
			servertools->SetKeyValue( pServerEntity, "origin", m_vecLocalOrigin.Get() );
			// FIXME: isn't there a string to vector conversion?
			Vector tmp( m_vecLocalAngles.Get().x, m_vecLocalAngles.Get().y, m_vecLocalAngles.Get().z );
			servertools->SetKeyValue( pServerEntity, "angles", tmp );
			return true;
		}
		else
		{
			// FIXME: does one need to be spawned?
		}
	}
	return false;
}

bool CDmeVMFEntity::IsSameOnServer( CBaseEntity *pServerEntity )
{
	char text[256];

	if (!pServerEntity)
	{
		return false;
	}

	// FIXME: check targetname?  Can it be edited?
	Vector mapOrigin;
	servertools->GetKeyValue( pServerEntity, "origin", text, sizeof( text ) );
	sscanf( text, "%f %f %f", &mapOrigin.x, &mapOrigin.y, &mapOrigin.z );
	Vector mapAngles;
	servertools->GetKeyValue( pServerEntity, "angles", text, sizeof( text ) );
	sscanf( text, "%f %f %f", &mapAngles.x, &mapAngles.y, &mapAngles.z );

	return ( mapOrigin == m_vecLocalOrigin.Get() && mapAngles == m_vecLocalAngles.Get() );
}

bool CDmeVMFEntity::CreateOnServer( void )
{
	CBaseEntity *pServerEntity = servertools->CreateEntityByName( m_ClassName.Get() );

	if (pServerEntity)
	{
		// test movement
		Ray_t ray;
		CTraceFilterWorldAndPropsOnly traceFilter;
		CBaseTrace tr;
		ray.Init( m_vecLocalOrigin.Get()+ Vector( 0, 0, 10 ), m_vecLocalOrigin.Get() + Vector( 0,0, -1000 ) );
		enginetools->TraceRayServer( ray, MASK_OPAQUE, &traceFilter, &tr );
		m_vecLocalOrigin.Set( tr.endpos );

		servertools->SetKeyValue( pServerEntity, "hammerid", GetEntityId() );
		servertools->SetKeyValue( pServerEntity, "targetname", m_TargetName.Get() );
		servertools->SetKeyValue( pServerEntity, "origin", m_vecLocalOrigin.Get() );
		servertools->SetKeyValue( pServerEntity, "angles", m_vecLocalAngles.Get() );
		servertools->DispatchSpawn( pServerEntity );

		return true;
	}
	return false;
}
