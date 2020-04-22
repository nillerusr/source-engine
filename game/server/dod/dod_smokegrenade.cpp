//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_smokegrenade.h"
#include "particle_parse.h"

LINK_ENTITY_TO_CLASS( grenade_smoke, CDODSmokeGrenade );
PRECACHE_WEAPON_REGISTER( grenade_smoke );

BEGIN_DATADESC( CDODSmokeGrenade )
	DEFINE_THINKFUNC( Think_Emit ),
	DEFINE_THINKFUNC( Think_Fade ),
	DEFINE_THINKFUNC( Think_Remove )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CDODSmokeGrenade, DT_DODSmokeGrenade )
	SendPropTime(SENDINFO(m_flSmokeSpawnTime) ),
END_SEND_TABLE()

void CDODSmokeGrenade::Spawn()
{
	BaseClass::Spawn();

	SetThink( &CDODSmokeGrenade::Think_Emit );
	SetNextThink( gpGlobals->curtime + 0.5 );

	m_bInitialSmoke = false;
	m_flRemoveTime = -1;
	m_flSmokeSpawnTime = 0;
}

void CDODSmokeGrenade::Precache()
{
	PrecacheScriptSound( "SmokeGrenade.Bounce" );
	PrecacheParticleSystem( "smokegrenade" );
	PrecacheParticleSystem( "smokegrenade_jet" );
	BaseClass::Precache();
}

void CDODSmokeGrenade::BounceSound( void )
{
	EmitSound( "SmokeGrenade.Bounce" );
}

void CDODSmokeGrenade::Think_Emit( void )
{
	// if we're stationary and have not yet created smoke, do so now
	Vector vel;
	AngularImpulse a;
	VPhysicsGetObject()->GetVelocity( &vel, &a );

	if ( vel.Length() < 15.0 && !m_bInitialSmoke )
	{
		VPhysicsGetObject()->EnableMotion( false );

		// Smoke Cloud
		DispatchParticleEffect( "smokegrenade", GetAbsOrigin(), vec3_angle );

		// Smoke Jet
		DispatchParticleEffect( "smokegrenade_jet", PATTACH_POINT, this, "jet" );

		EmitSound( "BaseSmokeEffect.Sound" );

		m_flRemoveTime = gpGlobals->curtime + 10;

		m_bInitialSmoke = true;

		m_flSmokeSpawnTime = gpGlobals->curtime;
	}

	// if its past our bedtime, fade out
	if ( m_flRemoveTime > 0 && gpGlobals->curtime > m_flRemoveTime )
	{
		m_nRenderMode = kRenderTransColor;
		SetThink( &CDODSmokeGrenade::Think_Fade );
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

// Fade the projectile out over time before making it disappear
void CDODSmokeGrenade::Think_Fade()
{
	m_bFading = true;

	SetNextThink( gpGlobals->curtime );

	color32 c = GetRenderColor();
	c.a -= 1;
	SetRenderColor( c.r, c.b, c.g, c.a );

	if ( !c.a )
	{
		SetModelName( NULL_STRING );//invisible
		SetNextThink( gpGlobals->curtime + 10 );
		SetThink( &CDODSmokeGrenade::Think_Remove );	// Spit out smoke for 10 seconds.
		SetSolid( SOLID_NONE );
	}
}

void CDODSmokeGrenade::Think_Remove()
{
	// stop all effects
	StopParticleEffects( this );

	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	UTIL_Remove( this );
}


void CDODSmokeGrenade::Detonate( void )
{	
	// Intentionally blank - our detonate does nothing
}