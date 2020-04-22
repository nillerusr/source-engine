//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "C_Env_Meteor.h"
#include "fx_explosion.h"
#include "tempentity.h"
#include "c_tracer.h"

//=============================================================================
//
// Meteor Factory Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_MeteorFactory::CreateMeteor( int nID, int iType,
								   const Vector &vecPosition, const Vector &vecDirection, 
		                           float flSpeed, float flStartTime, float flDamageRadius,
								   const Vector &vecTriggerMins, const Vector &vecTriggerMaxs )
{
	C_EnvMeteor::Create( nID, iType, vecPosition, vecDirection, flSpeed, flStartTime, flDamageRadius,
		                 vecTriggerMins, vecTriggerMaxs );
}


//=============================================================================
//
// Meteor Spawner Functions
//

void RecvProxy_MeteorTargetPositions( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CEnvMeteorSpawnerShared *pSpawner = ( CEnvMeteorSpawnerShared* )pStruct;
	pSpawner->m_aTargets[pData->m_iElement].m_vecPosition.x = pData->m_Value.m_Vector[0];
	pSpawner->m_aTargets[pData->m_iElement].m_vecPosition.y = pData->m_Value.m_Vector[1];
	pSpawner->m_aTargets[pData->m_iElement].m_vecPosition.z = pData->m_Value.m_Vector[2];
}

void RecvProxy_MeteorTargetRadii( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CEnvMeteorSpawnerShared *pSpawner = ( CEnvMeteorSpawnerShared* )pStruct;
	pSpawner->m_aTargets[pData->m_iElement].m_flRadius = pData->m_Value.m_Float;
}

void RecvProxyArrayLength_MeteorTargets( void *pStruct, int objectID, int currentArrayLength )
{
	CEnvMeteorSpawnerShared *pSpawner = ( CEnvMeteorSpawnerShared* )pStruct;
	if ( pSpawner->m_aTargets.Count() < currentArrayLength )
	{
		pSpawner->m_aTargets.SetSize( currentArrayLength );
	}
}


BEGIN_RECV_TABLE_NOBASE( CEnvMeteorSpawnerShared, DT_EnvMeteorSpawnerShared )
	// Setup (read from) Worldcraft.
	RecvPropInt		( RECVINFO( m_iMeteorType ) ),
	RecvPropInt     ( RECVINFO( m_bSkybox ) ),
	RecvPropFloat	( RECVINFO( m_flMinSpawnTime ) ),	
	RecvPropFloat	( RECVINFO( m_flMaxSpawnTime ) ),
	RecvPropInt     ( RECVINFO( m_nMinSpawnCount ) ),
	RecvPropInt     ( RECVINFO( m_nMaxSpawnCount ) ),
	RecvPropFloat	( RECVINFO( m_flMinSpeed ) ),	
	RecvPropFloat	( RECVINFO( m_flMaxSpeed ) ),
	
	// Setup through Init.
	RecvPropFloat	( RECVINFO( m_flStartTime ) ),
	RecvPropInt		( RECVINFO( m_nRandomSeed ) ),
	RecvPropVector	( RECVINFO( m_vecMinBounds ) ),
	RecvPropVector  ( RECVINFO( m_vecMaxBounds ) ),
	RecvPropVector  ( RECVINFO( m_vecTriggerMins ) ),
	RecvPropVector  ( RECVINFO( m_vecTriggerMaxs ) ),

	// Target List
	RecvPropArray2( RecvProxyArrayLength_MeteorTargets,
		            RecvPropVector( "meteortargetposition_array_element", 0, 0, 0, RecvProxy_MeteorTargetPositions ), 
		            16, 0, "meteortargetposition_array" ),

	RecvPropArray2( RecvProxyArrayLength_MeteorTargets,
		            RecvPropFloat( "meteortargetradius_array_element", 0, 0, 0, RecvProxy_MeteorTargetRadii ), 
		            16, 0, "meteortargetradius_array" )
END_RECV_TABLE()

// This table encodes the CBaseEntity data.
IMPLEMENT_CLIENTCLASS_DT( C_EnvMeteorSpawner, DT_EnvMeteorSpawner, CEnvMeteorSpawner )
	RecvPropDataTable	( RECVINFO_DT( m_SpawnerShared ), 0, &REFERENCE_RECV_TABLE( DT_EnvMeteorSpawnerShared ) ),
	RecvPropInt			( RECVINFO( m_fDisabled ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteorSpawner::C_EnvMeteorSpawner()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorSpawner::OnDataChanged( DataUpdateType_t updateType )
{
	// Initialize the client side spawner.
	m_SpawnerShared.Init( &m_Factory, m_SpawnerShared.m_nRandomSeed, m_SpawnerShared.m_flStartTime, 
						  m_SpawnerShared.m_vecMinBounds, m_SpawnerShared.m_vecMaxBounds,
						  m_SpawnerShared.m_vecTriggerMins, m_SpawnerShared.m_vecTriggerMaxs );

	// Set the next think to be the next spawn interval.
	if ( !m_fDisabled )
	{
		SetNextClientThink( m_SpawnerShared.m_flNextSpawnTime );
	}
}

#if 0
// Will probably be used later!!
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorSpawner::ReceiveMessage( int classID,  bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	m_SpawnerShared.m_flStartTime = msg.ReadLong();
	m_SpawnerShared.m_flNextSpawnTime = msg.ReadLong();
	SetNextClientThink( m_SpawnerShared.m_flNextSpawnTime );
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorSpawner::ClientThink( void )
{
	SetNextClientThink( m_SpawnerShared.MeteorThink( gpGlobals->curtime ) );
}

//=============================================================================
//
// Meteor Tail Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteorHead::C_EnvMeteorHead()
{
	m_vecPos.Init();
	m_vecPrevPos.Init();

	m_flParticleScale = 1.0f;

	m_pSmokeEmitter = NULL;
	m_flSmokeSpawnInterval = 0.0f;
	m_hSmokeMaterial = INVALID_MATERIAL_HANDLE;
	m_flSmokeLifetime = 2.5f;
	m_bEmitSmoke = true;

	m_hFlareMaterial = INVALID_MATERIAL_HANDLE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteorHead::~C_EnvMeteorHead()
{
	Destroy();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorHead::Start( const Vector &vecOrigin, const Vector &vecDirection )
{
	// Emitters.
	m_pSmokeEmitter = CSimpleEmitter::Create( "MeteorTrail" );
//	m_pFireEmitter = CSimpleEmitter::Create( "MeteorFire" );
	if ( !m_pSmokeEmitter /*|| !m_pFireEmitter*/ )
		return;

	// Smoke
	m_pSmokeEmitter->SetSortOrigin( vecOrigin );
	m_hSmokeMaterial = m_pSmokeEmitter->GetPMaterial( "particle/SmokeStack" );
	Assert( m_hSmokeMaterial != INVALID_MATERIAL_HANDLE );

	// Fire
//	m_pFireEmitter->SetSortOrigin( vecOrigin );
//	m_hFireMaterial = m_pFireEmitter->GetPMaterial( "particle/particle_fire" );
//	Assert( m_hFireMaterial != INVALID_MATERIAL_HANDLE );

	// Flare
//	m_hFlareMaterial = m_ParticleEffect.FindOrAddMaterial( "effects/redflare" );

	VectorCopy( vecDirection, m_vecDirection );
	VectorCopy( vecOrigin, m_vecPos );

	m_bInitThink = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorHead::Destroy( void )
{
	m_pSmokeEmitter = NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorHead::MeteorHeadThink( const Vector &vecOrigin, float flTime )
{
	if ( m_bInitThink )
	{
		VectorCopy( vecOrigin, m_vecPrevPos );
		m_bInitThink = false;
	}

	// Update the position of the emitters.
	VectorCopy( vecOrigin, m_vecPos );

	// Update Smoke
	if ( m_pSmokeEmitter.IsValid() && m_bEmitSmoke )
	{
		m_pSmokeEmitter->SetSortOrigin( m_vecPos );

		// Get distance covered
		Vector vecDelta;
		VectorSubtract( m_vecPos, m_vecPrevPos, vecDelta );
		float flLength = vecDelta.Length();

		int nParticleCount = flLength / 35.0f;
		if ( nParticleCount < 1 )
		{
			nParticleCount = 1;
		}

		flLength /= nParticleCount;

		Vector vecPos;
		for( int iParticle = 0; iParticle < nParticleCount; ++iParticle )
		{
			vecPos = m_vecPrevPos + ( m_vecDirection * ( flLength * iParticle ) );

			// Add some noise to the position.
			Vector vecPosOffset;
			vecPosOffset.Random( -m_flSmokeSpawnRadius, m_flSmokeSpawnRadius );
			VectorAdd( vecPosOffset, vecPos, vecPosOffset );
		

			SimpleParticle *pParticle = ( SimpleParticle* )m_pSmokeEmitter->AddParticle( sizeof( SimpleParticle ),
				                                                                         m_hSmokeMaterial,
				                                                                         vecPosOffset );
			if ( pParticle )
			{
				pParticle->m_flLifetime	= 0.0f;
				pParticle->m_flDieTime = m_flSmokeLifetime;
				
				// Add just a little movement.
				pParticle->m_vecVelocity.Random( -5.0f, 5.0f );
				
				pParticle->m_uchColor[0] = 255.0f;
				pParticle->m_uchColor[1] = 255.0f;
				pParticle->m_uchColor[2] = 255.0f;
				

				pParticle->m_uchStartSize = 70 * m_flParticleScale;
				pParticle->m_uchEndSize = 25 * m_flParticleScale;
				
				float flAlpha = random->RandomFloat( 0.5f, 1.0f );
				pParticle->m_uchStartAlpha = flAlpha * 255; 
				pParticle->m_uchEndAlpha = 0;
				
				pParticle->m_flRoll	= random->RandomInt( 0, 360 );
				pParticle->m_flRollDelta = random->RandomFloat( -1.0f, 1.0f );
			}
		}
	}

	// Update Fire
//	if ( m_pFireEmitter && m_bEmitFire )
//	{
//	}

	// Flare

	// Save off position.
	VectorCopy( m_vecPos, m_vecPrevPos );
}

//=============================================================================
//
// Meteor Tail Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteorTail::C_EnvMeteorTail()
{
	m_TailMaterialHandle = INVALID_MATERIAL_HANDLE;

	m_pParticleMgr = NULL;
	m_pParticle = NULL;

	m_flFadeTime = 0.5f;
	m_flWidth = 3.0f;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteorTail::~C_EnvMeteorTail()
{
	Destroy();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorTail::Start( const Vector &vecOrigin, const Vector &vecDirection,
							 float flSpeed )
{
	// Set the particle manager.
	m_pParticleMgr = ParticleMgr();
	m_pParticleMgr->AddEffect( &m_ParticleEffect, this );
	m_TailMaterialHandle = m_ParticleEffect.FindOrAddMaterial( "particle/guidedplasmaprojectile" );
	m_pParticle = m_ParticleEffect.AddParticle( sizeof( StandardParticle_t ), m_TailMaterialHandle );
	if ( m_pParticle )
	{
		m_pParticle->m_Pos = vecOrigin;
	}

	VectorCopy( vecDirection, m_vecDirection );
	m_flSpeed = flSpeed;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorTail::Destroy( void )
{
	if ( m_pParticleMgr ) 
	{ 
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
		m_pParticleMgr = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteorTail::DrawFragment( ParticleDraw* pDraw, 
								    const Vector &vecStart, const Vector &vecDelta, 
									const Vector4D &vecStartColor, const Vector4D &vecEndColor, 
									float flStartV, float flEndV )
{
	if( !pDraw->GetMeshBuilder() )
		return;
	
	// Clip the fragment.
	Vector vecVerts[4];
	if ( !Tracer_ComputeVerts( vecStart, vecDelta, m_flWidth, vecVerts ) )
		return;

	// NOTE: Gotta get the winding right so it's not backface culled
	// (we need to turn of backface culling for these bad boys)
	CMeshBuilder* pMeshBuilder = pDraw->GetMeshBuilder();

	pMeshBuilder->Position3f( vecVerts[0].x, vecVerts[0].y, vecVerts[0].z );
	pMeshBuilder->TexCoord2f( 0, 0.0f, flStartV );
	pMeshBuilder->Color4fv( vecStartColor.Base() );
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( vecVerts[1].x, vecVerts[1].y, vecVerts[1].z );
	pMeshBuilder->TexCoord2f( 0, 1.0f, flStartV );
	pMeshBuilder->Color4fv( vecStartColor.Base() );
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( vecVerts[3].x, vecVerts[3].y, vecVerts[3].z );
	pMeshBuilder->TexCoord2f( 0, 1.0f, flEndV );
	pMeshBuilder->Color4fv( vecEndColor.Base() );
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( vecVerts[2].x, vecVerts[2].y, vecVerts[2].z );
	pMeshBuilder->TexCoord2f( 0, 0.0f, flEndV );
	pMeshBuilder->Color4fv( vecEndColor.Base() );
	pMeshBuilder->AdvanceVertex();
}

void C_EnvMeteorTail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	Particle *pParticle = (Particle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Update the particle position.
		pParticle->m_Pos = GetLocalOrigin();

		pParticle = (Particle*)pIterator->GetNext();
	}
}


void C_EnvMeteorTail::RenderParticles( CParticleRenderIterator *pIterator )
{
	const Particle *pParticle = (const Particle *)pIterator->GetFirst();
	while ( pParticle )
	{
		// Now draw the tail fragments...
		Vector4D vecStartColor( 1.0f, 1.0f, 1.0f, 1.0f );
		Vector4D vecEndColor( 1.0f, 1.0f, 1.0f, 0.0f );
		Vector vecDelta, vecStartPos, vecEndPos;

		// Calculate the tail.
		Vector vecTailEnd;
		vecTailEnd = GetLocalOrigin() + ( m_vecDirection * -m_flSpeed );

		// Transform particles into camera space.
 		TransformParticle( m_pParticleMgr->GetModelView(), GetLocalOrigin(), vecStartPos );
 		TransformParticle( m_pParticleMgr->GetModelView(), vecTailEnd, vecEndPos );
		float sortKey = vecStartPos.z;

		// Draw the tail fragment.
		VectorSubtract( vecStartPos, vecEndPos, vecDelta );
		DrawFragment( pIterator->GetParticleDraw(), vecEndPos, vecDelta, vecEndColor, vecStartColor,  
					  1.0f - vecEndColor[3], 1.0f - vecStartColor[3] ); 

		pParticle = (const Particle *)pIterator->GetNext( sortKey );
	}
}


//=============================================================================
//
// Meteor Functions
//

static g_MeteorCounter = 0;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteor::C_EnvMeteor()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteor::~C_EnvMeteor()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::ClientThink( void )
{
	// Get the current time.
	float flTime = gpGlobals->curtime;

	// Update the meteor.
	if ( m_Meteor.IsInSkybox( flTime ) )
	{
		if ( m_Meteor.m_nLocation == METEOR_LOCATION_WORLD )
		{
			WorldToSkyboxThink( flTime );
		}
		else
		{
			SkyboxThink( flTime );
		}
	}
	else
	{
		if ( m_Meteor.m_nLocation == METEOR_LOCATION_SKYBOX )
		{
			SkyboxToWorldThink( flTime );
		}
		else
		{
			WorldThink( flTime );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::SkyboxThink( float flTime )
{
	float flDeltaTime = flTime - m_Meteor.m_flStartTime;
	if ( flDeltaTime > METEOR_MAX_LIFETIME )
	{
		Destroy( this );
		return;
	}

	// Check to see if the object is passive or not - act accordingly!
	if ( !m_Meteor.IsPassive( flTime ) )
	{
		// Update meteor position.
		Vector origin;
		m_Meteor.GetPositionAtTime( flTime, origin );
		SetLocalOrigin( origin );

		// Update the position of the tail effect.
		m_TailEffect.SetLocalOrigin( GetLocalOrigin() );
		m_HeadEffect.MeteorHeadThink( GetLocalOrigin(), flTime );
	}

	// Add the entity to the active list - update!
	AddEntity();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::WorldToSkyboxThink( float flTime )
{
	// Move the meteor from the world into the skybox.
	m_Meteor.ConvertFromWorldToSkybox();

	// Destroy the head effect.  Recreate it.
	m_HeadEffect.Destroy();	
	m_HeadEffect.Start( m_Meteor.m_vecStartPosition, m_vecTravelDir );
	m_HeadEffect.SetSmokeEmission( true );
	m_HeadEffect.SetParticleScale( 1.0f / 16.0f );	
	m_HeadEffect.m_bInitThink = true;

	// Update to world model.
	SetModel( "models/props/common/meteorites/meteor05.mdl" );

	// Update the meteor position (move into the skybox!)
	SetLocalOrigin( m_Meteor.m_vecStartPosition );
	
	// Update (think).
	SkyboxThink( flTime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::SkyboxToWorldThink( float flTime )
{
	// Move the meteor from the skybox into the world.
	m_Meteor.ConvertFromSkyboxToWorld();
	
	// Destroy the head effect.  Recreate it.
	m_HeadEffect.Destroy();	
	m_HeadEffect.Start( m_Meteor.m_vecStartPosition, m_vecTravelDir );
	m_HeadEffect.SetSmokeEmission( true );
	m_HeadEffect.SetParticleScale( 1.0f );
	m_HeadEffect.m_bInitThink = true;
	
	// Update to world model.
	SetModel( "models/props/common/meteorites/meteor04.mdl" );

	SetLocalOrigin( m_Meteor.m_vecStartPosition );

	// Update (think).
	WorldThink( flTime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::WorldThink( float flTime )
{
	// Update meteor position.
	Vector vecEndPosition;
	m_Meteor.GetPositionAtTime( flTime, vecEndPosition );

	// m_Meteor must return the end position in world space for the trace to work.
	Assert( GetMoveParent() == NULL );

//	Msg( "Client: Time = %lf, Position: %4.2f %4.2f %4.2f\n", flTime, vecEndPosition.x, vecEndPosition.y, vecEndPosition.z );

	// Check to see if we struck the world.  If so, cause an explosion.
	trace_t trace;

	Vector vecMin, vecMax;
	GetRenderBounds( vecMin, vecMax );

	// NOTE: This code works only if we aren't in hierarchy!!!
	Assert( !GetMoveParent() );

	CTraceFilterWorldOnly traceFilter;
	UTIL_TraceHull( GetAbsOrigin(), vecEndPosition, vecMin, vecMax,
		              MASK_SOLID_BRUSHONLY, &traceFilter, &trace ); 
	
	// Collision.
	if ( ( trace.fraction < 1.0f ) && !( trace.surface.flags & SURF_SKY ) )
	{
		// Move up to the end.
		Vector vecEnd = GetAbsOrigin() + ( ( vecEndPosition - GetAbsOrigin() ) * trace.fraction );
		
		// Create an explosion effect!
		BaseExplosionEffect().Create( vecEnd, 10, 32, TE_EXPLFLAG_NONE );

		// Debugging Info!!!!
//		debugoverlay->AddBoxOverlay( vecEnd, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ), QAngle( 0.0f, 0.0f, 0.0f ), 255, 0, 0, 0, 100 );

		Destroy( this );
		return;
	}
	else
	{
		// Move to the end.
		SetLocalOrigin( vecEndPosition );
	}

	m_TailEffect.SetLocalOrigin( GetLocalOrigin() );
	m_HeadEffect.MeteorHeadThink( GetLocalOrigin(), flTime );

	// Add the entity to the active list - update!
	AddEntity();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
C_EnvMeteor *C_EnvMeteor::Create( int nID, int iMeteorType, const Vector &vecOrigin,
								  const Vector &vecDirection, float flSpeed, float flStartTime,
								  float flDamageRadius,
								  const Vector &vecTriggerMins, const Vector &vecTriggerMaxs )
{
	C_EnvMeteor *pMeteor = new C_EnvMeteor;
	if ( pMeteor )
	{
		pMeteor->m_Meteor.Init( nID, flStartTime, METEOR_PASSIVE_TIME, vecOrigin, vecDirection, flSpeed, flDamageRadius,
			                    vecTriggerMins, vecTriggerMaxs );

		// Initialize the meteor.
		pMeteor->InitializeAsClientEntity( "models/props/common/meteorites/meteor05.mdl", RENDER_GROUP_OPAQUE_ENTITY );

		// Handle forward simulation.
		if ( ( pMeteor->m_Meteor.m_flStartTime + METEOR_MAX_LIFETIME ) < gpGlobals->curtime )
		{
			Destroy( pMeteor );
		}

		// Meteor Head and Tail
		pMeteor->SetTravelDirection( vecDirection );

		pMeteor->m_HeadEffect.SetSmokeEmission( true );
		pMeteor->m_HeadEffect.Start( vecOrigin, vecDirection );
		pMeteor->m_HeadEffect.SetParticleScale( 1.0f / 16.0f );

		pMeteor->m_TailEffect.Start( vecOrigin, vecDirection, flSpeed );

		pMeteor->SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	return pMeteor;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_EnvMeteor::Destroy( C_EnvMeteor *pMeteor )
{
	Assert( pMeteor->GetClientHandle() != INVALID_CLIENTENTITY_HANDLE );
	ClientThinkList()->AddToDeleteList( pMeteor->GetClientHandle() );
}
