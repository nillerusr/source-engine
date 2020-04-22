//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CResourceZone.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "engine/IEngineSound.h"
#include "c_func_resource.h"
#include "techtree.h"
#include "fx.h"
#include "fx_sparks.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Chunk movement
#define	CHUNK_FLECK_MIN_SPEED		25.0f
#define	CHUNK_FLECK_MAX_SPEED		100.0f
#define	CHUNK_FLECK_GRAVITY			800.0f
#define	CHUNK_FLECK_DAMPEN			0.3f
#define	CHUNK_FLECK_ANGULAR_SPRAY	0.8f

IMPLEMENT_CLIENTCLASS_DT(C_ResourceZone, DT_ResourceZone, CResourceZone)
	RecvPropFloat(RECVINFO(m_flClientResources)),
	RecvPropInt(RECVINFO(m_nResourcesLeft)),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( trigger_resourcezone, C_ResourceZone );
BEGIN_PREDICTION_DATA( C_ResourceZone )
END_PREDICTION_DATA();

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ResourceZone::C_ResourceZone()
{
	CONSTRUCT_MINIMAP_PANEL( "minimap_resource_zone", MINIMAP_RESOURCE_ZONES );
}

//-----------------------------------------------------------------------------
// Add, remove object from the panel 
//-----------------------------------------------------------------------------
void C_ResourceZone::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "resourcezone", (!bDormant && m_flClientResources > 0) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ResourceZone::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + 1.0 );
	}

	// If I've just dried up, remove me from the minimap
	if ( m_flClientResources <= 0 )
	{
		ENTITY_PANEL_ACTIVATE( "resourcezone", false );
		DESTRUCT_MINIMAP_PANEL();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_ResourceZone::GetTargetDescription( void ) const
{
	return "Resource Zone";
}


//==========================================================================================================
// Resource Spawner
//==========================================================================================================
IMPLEMENT_CLIENTCLASS_DT(C_ResourceSpawner, DT_ResourceSpawner, CResourceSpawner)
	RecvPropInt(RECVINFO(m_bActive)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ResourceSpawner::C_ResourceSpawner( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ResourceSpawner::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + random->RandomFloat( 2.0, 4.0 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Receive a spawn message from the server
//-----------------------------------------------------------------------------
void C_ResourceSpawner::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	// Make some particles
	SpawnEffect( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ResourceSpawner::ClientThink( void )
{
	SetNextClientThink( gpGlobals->curtime + random->RandomFloat( 2.0, 10.0 ) );

	// Don't do random puffs if I'm not active
	if ( !m_bActive )
		return;

	// Occasionally spurt as if I was making a chunk
	if ( random->RandomInt(0, 20) == 5 )
	{
		SpawnEffect( true );
	}
	else
	{
		SpawnEffect( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Particle effects created when we spawn a chunk
//-----------------------------------------------------------------------------
void C_ResourceSpawner::SpawnEffect( bool bSpawningChunk )
{
	Vector normal = Vector(0,0,1);
	Vector offset = GetAbsOrigin() + (normal * 16);
	Vector dir;

	float r = sResourceColor.r;
	float g = sResourceColor.g;
	float b = sResourceColor.b;

	// Play a random puff sound
	if ( bSpawningChunk )
	{
		EmitSound( "ResourceSpawner.BigPuff" );
	}
	else
	{
		EmitSound( "ResourceSpawner.Puff" );
	}

	// Chunks o'dirt
	CSmartPtr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "SpawnEffect 1", offset, Vector(5,5,5) );
	if ( !fleckEmitter )
		return;

	// Setup our collision information
	fleckEmitter->m_ParticleCollision.Setup( offset, &normal, CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_MIN_SPEED, CHUNK_FLECK_MAX_SPEED, CHUNK_FLECK_GRAVITY, CHUNK_FLECK_DAMPEN );

	int	numFlecks;
	if ( bSpawningChunk )
		numFlecks = random->RandomInt( 48, 64 );
	else
		numFlecks = random->RandomInt( 1, 3 );

	// Dump out flecks
	int i;
	for ( i = 0; i < numFlecks; i++ )
	{
		FleckParticle *pParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), g_Mat_Fleck_Cement[random->RandomInt(0,1)], offset );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random->RandomFloat(3.0f,5.0f);

		if ( bSpawningChunk )
		{
			pParticle->m_uchSize		= random->RandomInt( 4, 8 );
			dir[0] = normal[0] + random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY );
			dir[1] = normal[1] + random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY );
			dir[2] = normal[2] + random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY );
			pParticle->m_vecVelocity	= dir * ( random->RandomFloat( CHUNK_FLECK_MIN_SPEED, CHUNK_FLECK_MAX_SPEED ) * ( 9 - pParticle->m_uchSize ) );
		}
		else
		{
			pParticle->m_uchSize		= random->RandomInt( 2, 4 );
			dir[0] = normal[0] + (random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY ) * 0.5);
			dir[1] = normal[1] + (random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY ) * 0.5);
			dir[2] = normal[2] + (random->RandomFloat( -CHUNK_FLECK_ANGULAR_SPRAY, CHUNK_FLECK_ANGULAR_SPRAY ) * 0.5);
			pParticle->m_vecVelocity	= dir * ( random->RandomFloat( CHUNK_FLECK_MIN_SPEED, CHUNK_FLECK_MAX_SPEED ) * 3);
		}

		pParticle->m_flRoll		= random->RandomFloat( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( 0, 360 );

		pParticle->m_uchColor[0] = r;
		pParticle->m_uchColor[1] = g;
		pParticle->m_uchColor[2] = b;
	}


	// Create a couple of big, floating smoke clouds
	if ( bSpawningChunk || random->RandomInt(0,10) == 0 )
	{
		CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "SpawnEffect 2" );
		pSmokeEmitter->SetSortOrigin( offset );
		int iSmokeClouds = 2;
		if ( !bSpawningChunk )
			iSmokeClouds = 1;
		for ( i = 0; i < iSmokeClouds; i++ )
		{
			SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], offset );
			if ( pParticle == NULL )
				break;

			pParticle->m_flLifetime	= 0.0f;
			pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 3.0f );

			if ( bSpawningChunk )
			{
				pParticle->m_uchStartSize	= 32;
				pParticle->m_uchEndSize		= 128;
			}
			else
			{
				pParticle->m_uchStartSize	= 16;
				pParticle->m_uchEndSize		= 64;
			}

			dir[0] = normal[0] + random->RandomFloat( -0.4f, 0.4f );
			dir[1] = normal[1] + random->RandomFloat( -0.4f, 0.4f );
			dir[2] = normal[2] + random->RandomFloat( 0, 0.6f );
			pParticle->m_vecVelocity = dir * random->RandomFloat( 2.0f, 24.0f )*(i+1);
			pParticle->m_uchStartAlpha	= 160;
			pParticle->m_uchEndAlpha	= 0;
			pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

			pParticle->m_uchColor[0] = r;
			pParticle->m_uchColor[1] = g;
			pParticle->m_uchColor[2] = b;
		}
	}
}
