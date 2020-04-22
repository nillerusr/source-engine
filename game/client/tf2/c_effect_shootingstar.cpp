//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's Meteor
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_effect_shootingstar.h"
#include "clienteffectprecachesystem.h"

//=============================================================================
//
// Shooting Star Spawner Functionality
//

IMPLEMENT_CLIENTCLASS_DT( C_ShootingStarSpawner, DT_ShootingStarSpawner, CShootingStarSpawner )
	RecvPropFloat( RECVINFO( m_flSpawnInterval ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ShootingStarSpawner::C_ShootingStarSpawner( void )
{
	SetNextClientThink( gpGlobals->curtime + m_flSpawnInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ShootingStarSpawner::ClientThink( void )
{
	// Spawn a number of shooting stars.
	SpawnShootingStars();

	// Randomly generate a next think time.
	SetNextClientThink( gpGlobals->curtime + random->RandomFloat( 0.25, m_flSpawnInterval ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ShootingStarSpawner::SpawnShootingStars( void )
{
	C_ShootingStar *pShootingStar = new C_ShootingStar;
	if ( pShootingStar )
	{
		// In Space.
		pShootingStar->SetFriction( 1.0f );
		pShootingStar->SetGravity( 0.0f );

		// Randomize the velocity. -- This isn't right, but works for the test!
		Vector vecVelocity;
		vecVelocity.x = ( GetAbsAngles().x ) * random->RandomFloat( 1.0f, 10.0f );
		vecVelocity.y = ( GetAbsAngles().y ) * random->RandomFloat( 1.0f, 10.0f );
		vecVelocity.z = ( GetAbsAngles().z ) * random->RandomFloat( 1.0f, 10.0f );

		pShootingStar->Init( GetAbsOrigin(), vecVelocity, random->RandomFloat( 10.0f, 100.0f ), random->RandomFloat( 10.0f, 30.0f ) );
	}
}

//=============================================================================
//
// Shooting Star Functionality
//

//Precahce the effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectShootingStars )
CLIENTEFFECT_MATERIAL( "effects/redflare" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_ShootingStar::C_ShootingStar( void ) : CSimpleEmitter( "ShootingStar" )
{
	m_flScale = 1.0f;
	SetDynamicallyAllocated( false );
}


//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
C_ShootingStar::~C_ShootingStar( void )
{
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
void C_ShootingStar::Init( const Vector vecOrigin, const Vector vecVelocity, int nSize,
						   float flLifeTime )
{
	// Set the sort origin.
	SetSortOrigin( vecOrigin );

	// Create the initial particle and set the data.
	SimpleParticle *pParticle = ( SimpleParticle* )AddParticle( sizeof( SimpleParticle ), GetPMaterial( "effects/redflare" ), vecOrigin );
	if ( pParticle )
	{
		pParticle->m_vecVelocity = vecVelocity;

		pParticle->m_uchColor[0] = pParticle->m_uchColor[1] = pParticle->m_uchColor[2] = 255;

		pParticle->m_flRoll = random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta = random->RandomFloat( 1.0f, 4.0f );

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime = flLifeTime;

		pParticle->m_uchStartAlpha = 255;
		pParticle->m_uchEndAlpha = 0;
		pParticle->m_uchStartSize = nSize;
		pParticle->m_uchEndSize = ( nSize / 3 );
	}

	int iParticle = m_aParticles.AddToTail(); 
	m_aParticles[iParticle] = pParticle;
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
void C_ShootingStar::Destroy( void )
{
	// Destroy shooting star particles.
	int nParticleCount = m_aParticles.Count();
	for ( int iParticle = ( nParticleCount - 1 ); iParticle >= 0; iParticle-- )
	{
		SimpleParticle *pParticle = m_aParticles[iParticle];
		m_aParticles.Remove( iParticle );
		CSimpleEmitter::NotifyDestroyParticle( pParticle );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ShootingStar::SetSortOrigin( const Vector &vSortOrigin )
{
	CSimpleEmitter::SetSortOrigin( vSortOrigin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timeDelta - 
//-----------------------------------------------------------------------------
void C_ShootingStar::Update( float timeDelta )
{
	// Parent update.
	CSimpleEmitter::Update( timeDelta );

	// Don't update if the console is down.
	if ( timeDelta <= 0.0f )
		return;

	// Are we still alive? Get the tail of the shooting star (last valid index)
	// and test.
	int nParticleCount = m_aParticles.Count();
	if ( nParticleCount <= 0 )
		return;

	SimpleParticle *pParticle = m_aParticles[nParticleCount-1];
	if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
	{
		Destroy();
		return;
	}

	// Update the particles lifetime.
	pParticle->m_flLifetime += timeDelta;	

	// Update the particle position.
	pParticle->m_Pos += ( pParticle->m_vecVelocity * timeDelta );

	SetLocalOrigin( pParticle->m_Pos );
	SetSortOrigin( GetAbsOrigin() );
}
