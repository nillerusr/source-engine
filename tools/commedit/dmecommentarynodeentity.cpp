//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmecommentarynodeentity.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "toolframework/itoolentity.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "engine/iclientleafsystem.h"
#include "toolutils/enginetools_int.h"
#include "commedittool.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SPHERE_RADIUS 16

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCommentaryNodeEntity, CDmeCommentaryNodeEntity );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCommentaryNodeEntity::OnConstruction()
{
	m_ClassName.InitAndSet( this, "classname", false, FATTRIB_HAS_CALLBACK );
	m_TargetName.Init( this, "targetname" );
	m_bIsPlaceholder.InitAndSet( this, "_placeholder", false, FATTRIB_DONTSAVE );
	m_vecLocalOrigin.Init( this, "origin" );
	m_vecLocalAngles.Init( this, "angles" );

	// Used to make sure these aren't saved if they aren't changed
	m_TargetName.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );
	m_vecLocalAngles.GetAttribute()->AddFlag( FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );

	m_bInfoTarget = false;
	m_bIsDirty = false;
	m_hEngineEntity = HTOOLHANDLE_INVALID;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "editor/info_target" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	m_InfoTargetSprite.Init( "__commentary_info_target", pVMTKeyValues );

	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetString( "$color", "{255 0 0}" );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$additive", 1 );
	m_SelectedInfoTarget.Init( "__selected_commentary_info_target", pVMTKeyValues );
}

void CDmeCommentaryNodeEntity::OnDestruction()
{
	// Unhook it from the engine
	AttachToEngineEntity( HTOOLHANDLE_INVALID );
	m_SelectedInfoTarget.Shutdown();
	m_InfoTargetSprite.Shutdown();
}


//-----------------------------------------------------------------------------
// Called whem attributes change
//-----------------------------------------------------------------------------
void CDmeCommentaryNodeEntity::OnAttributeChanged( CDmAttribute *pAttribute )
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
		if ( !Q_stricmp( m_ClassName, "point_commentary_node" ) )
		{
			SetModelName( "models/extras/info_speech.mdl" );
			GetMDL()->m_flPlaybackRate = 0.0f;
		}
		else
		{
			SetModelName( NULL );
		}
		return;
	}
}

	
//-----------------------------------------------------------------------------
// Returns the entity ID
//-----------------------------------------------------------------------------
int CDmeCommentaryNodeEntity::GetEntityId() const
{
	return atoi( GetName() );
}


//-----------------------------------------------------------------------------
// Mark the entity as being dirty
//-----------------------------------------------------------------------------
void CDmeCommentaryNodeEntity::MarkDirty( bool bDirty )
{
	m_bIsDirty = bDirty;
}


//-----------------------------------------------------------------------------
// Is the renderable transparent?
//-----------------------------------------------------------------------------
bool CDmeCommentaryNodeEntity::IsTransparent( void )
{
	return m_bIsDirty || m_bInfoTarget || BaseClass::IsTransparent();
}

	
//-----------------------------------------------------------------------------
// Entity Key iteration
//-----------------------------------------------------------------------------
bool CDmeCommentaryNodeEntity::IsEntityKey( CDmAttribute *pEntityKey )
{
	return pEntityKey->IsFlagSet( FATTRIB_USERDEFINED );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDmAttribute *CDmeCommentaryNodeEntity::FirstEntityKey()
{
	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsEntityKey( pAttribute ) )
			return pAttribute;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDmAttribute *CDmeCommentaryNodeEntity::NextEntityKey( CDmAttribute *pEntityKey )
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
void CDmeCommentaryNodeEntity::AttachToEngineEntity( HTOOLHANDLE hToolHandle )
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
const Vector &CDmeCommentaryNodeEntity::GetRenderOrigin( void )
{
	return m_vecLocalOrigin;
}

const QAngle &CDmeCommentaryNodeEntity::GetRenderAngles( void )
{
	return *(QAngle*)(&m_vecLocalAngles.Get());
}


//-----------------------------------------------------------------------------
// Draws the helper for the entity
//-----------------------------------------------------------------------------
void CDmeCommentaryNodeEntity::DrawSprite( IMaterial *pMaterial )
{
	float t = 0.5f * sin( Plat_FloatTime() * M_PI / 1.0f ) + 0.5f;

	CMatRenderContextPtr pRenderContext( materials );
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
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( SPHERE_RADIUS, -SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( SPHERE_RADIUS, SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -SPHERE_RADIUS, SPHERE_RADIUS, 0.0f );
	meshBuilder.Color4ub( nBaseR, nBaseG, nBaseB, nAlpha );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
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
int CDmeCommentaryNodeEntity::DrawModel( int flags )
{  
	bool bSelected = ( g_pCommEditTool->GetCurrentEntity().Get() == this );
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

	CMatRenderContextPtr pRenderContext( materials );
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
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 1; 
}


//-----------------------------------------------------------------------------
// Position and bounds for the model
//-----------------------------------------------------------------------------
void CDmeCommentaryNodeEntity::GetRenderBounds( Vector& mins, Vector& maxs )
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
void CDmeCommentaryNodeEntity::SetRenderOrigin( const Vector &vecOrigin )
{
	m_vecLocalOrigin = vecOrigin;
	clienttools->MarkClientRenderableDirty( this );
}

void CDmeCommentaryNodeEntity::SetRenderAngles( const QAngle &angles )
{
	m_vecLocalAngles = *(Vector*)&angles;
	clienttools->MarkClientRenderableDirty( this );
}


