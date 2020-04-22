//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rockets (Weapon)
//
//=============================================================================//

#include "cbase.h"
#include "weapon_grenade_rocket.h"

#if defined( CLIENT_DLL )
// Client Only
#include "hud.h"
#include "particles_simple.h"
#else
// Server Only
#include "gameinterface.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "tf_team.h"
#include "env_laserdesignation.h"
#include "iservervehicle.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_grenade_rocket, CWeaponGrenadeRocket );
BEGIN_PREDICTION_DATA( CWeaponGrenadeRocket )
END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGrenadeRocket, DT_WeaponGrenadeRocket )
BEGIN_NETWORK_TABLE( CWeaponGrenadeRocket, DT_WeaponGrenadeRocket )
//#if !defined( CLIENT_DLL )
//#else
//#endif
END_NETWORK_TABLE()

#define WEAPON_GRENADE_ROCKET_VELOCITY			1000

#if !defined( CLIENT_DLL )
// Server Only
ConVar weapon_grenade_rocket_track_range_mod( "weapon_grenade_rocket_track_range_mod","1.5", FCVAR_NONE, "Range multiplier when a rocket's tracking a designated target." );
ConVar weapon_grenade_rocket_force( "weapon_grenade_rocket_force","150.0", FCVAR_NONE, "Rocket force modifier." ); 
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGrenadeRocket::CWeaponGrenadeRocket()
{
	m_flDamage = 100.0f;

#if !defined( CLIENT_DLL )
	// Server Only
	UseClientSideAnimation();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CWeaponGrenadeRocket::~CWeaponGrenadeRocket()
{
#if defined( CLIENT_DLL )
	StopSound( entindex(), "GrenadeRocket.FlyLoop" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Create a weapon grenade rocket
//-----------------------------------------------------------------------------
CWeaponGrenadeRocket *CWeaponGrenadeRocket::Create( const Vector &vecOrigin, const Vector &vecForward, float flMaxRange, CBaseEntity *pOwner )
{
#if !defined( CLIENT_DLL )
	CWeaponGrenadeRocket *pRocket = ( CWeaponGrenadeRocket* )CreateEntityByName( "weapon_grenade_rocket" );

	UTIL_SetOrigin( pRocket, vecOrigin );
	QAngle angles;
	VectorAngles( vecForward, angles );
	pRocket->SetLocalAngles( angles );
	pRocket->Spawn();
	pRocket->SetOwnerEntity( pOwner );
	pRocket->ChangeTeam( pOwner->GetTeamNumber() );
	pRocket->SetMaxRange( flMaxRange );

	return pRocket;
#else
	return NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::SetMaxRange( float flRange )
{
	m_flMaxRange = flRange;
	m_flFallingSpeed = 200;	// Initial falling speed

	// Reduce max range for upward shots
	Vector vecForward;
	GetVectors( &vecForward, NULL, NULL );
	if ( vecForward.z > 0 )
	{
		m_flMaxRange = MAX(1, m_flMaxRange - (vecForward.z * 1200));
	}
	else 
	{
		m_flMaxRange -= (vecForward.z * 2400);
	}

#if !defined( CLIENT_DLL )
	if ( m_flMaxRange )
	{
		float flSpeed = GetLocalVelocity().Length();
		Assert( flSpeed );
		m_flExceedRangeTime = gpGlobals->curtime + (m_flMaxRange / flSpeed);

		// Start looking for designators
		SetThink( TrackThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::Spawn( void )
{
#if !defined( CLIENT_DLL )
	Precache();

	m_flRadius = 100;
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	SetModel( "models/weapons/w_missile.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );

	// Forward!
	Vector forward;
	AngleVectors( GetLocalAngles(), &forward, NULL, NULL );
	SetAbsVelocity( forward * WEAPON_GRENADE_ROCKET_VELOCITY );
	
	SetTouch( RocketTouch );
#else
	// Start our flying sound loop
	CPASAttenuationFilter filter( this );
	filter.MakeReliable();
	EmitSound( filter, entindex(), "GrenadeRocket.FlyLoop" );
#endif
}

#if !defined( CLIENT_DLL )
// Server Only
	
//-----------------------------------------------------------------------------
// Purpose: Return my owner as my scorer
//-----------------------------------------------------------------------------
CBasePlayer *CWeaponGrenadeRocket::GetScorer( void )
{
	return ToBasePlayer( m_hOwner );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::Precache( void )
{
	PrecacheModel( "models/weapons/w_missile.mdl" );

	PrecacheScriptSound( "GrenadeRocket.FlyLoop" );
}

//-----------------------------------------------------------------------------
// Purpose: We've exceeded this rocket's range, start heading downward, randomly
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::ExceededRangeThink( void )
{
	Vector vecZ( 0,0,1 );
	Vector vecPerp;

	// Weave drunkely and head down
	Vector vecVelocity = GetLocalVelocity();
	CrossProduct( vecVelocity, vecZ, vecPerp );
	VectorNormalize( vecPerp );
	vecPerp *= random->RandomFloat(-100,100);
	vecVelocity += vecPerp;
	vecVelocity.z -= m_flFallingSpeed;
	m_flFallingSpeed += 50;
	SetLocalVelocity( vecVelocity );

	SetAnglesToMatchVelocity();
	SetNextThink( gpGlobals->curtime + 0.3 );
}

//-----------------------------------------------------------------------------
// Purpose: Track towards my my designated target
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::TrackThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.3 );

	// Have I exceeded my range?
	if ( gpGlobals->curtime > m_flExceedRangeTime )
	{
		SetThink( ExceededRangeThink );
		// Start falling immediately
		ExceededRangeThink();
		return;
	}

	// Look for any laser designators in tracking view
	int iTeam = GetTeamNumber();
	int iCount = CEnvLaserDesignation::GetNumLaserDesignators( iTeam );
	if ( !iCount )
		return;

	// Get the potential lock range
	float flIncreasedMaxRange = m_flMaxRange * weapon_grenade_rocket_track_range_mod.GetFloat();
	float flNearestDot = 0.95;
	Vector vecNearestTarget = vec3_origin;
	bool bFoundOne = false;

	// Any valid designated targets?
	for ( int i = 0; i < iCount; i++ )
	{
		Vector vecTarget;
		if ( !CEnvLaserDesignation::GetLaserDesignation( iTeam, i, &vecTarget ) )
			continue;
	
		// Check validity of designated target
		Vector vecToTarget = ( vecTarget - GetAbsOrigin() );
		float flDistanceSqr = vecToTarget.LengthSqr();
		// Make sure it's not too far
		if ( flDistanceSqr > (flIncreasedMaxRange*flIncreasedMaxRange) )
			continue;

		// Make sure it's near my flight path
		VectorNormalize(vecToTarget);
		Vector vecForward;
		GetVectors( &vecForward, NULL, NULL );
		float flDot = DotProduct( vecToTarget, vecForward );
		if ( flDot < flNearestDot )
			continue;

		flNearestDot = flDot;
		vecNearestTarget = vecTarget;
		bFoundOne = true;
	}

	// No valid targets
	if ( !bFoundOne )
		return;

	SetNextThink( gpGlobals->curtime + 0.1f );

	// Turn towards my target
	Vector vecToTarget = (vecNearestTarget - GetAbsOrigin());
	VectorNormalize(vecToTarget);

	// Shamelessly ripped from HL1 RPG
	float flSpeed = GetAbsVelocity().Length();
	SetAbsVelocity( (GetAbsVelocity() * 0.5) + (vecToTarget * flSpeed * 0.498) );

	SetAnglesToMatchVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: Set angles to match our velocity
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::SetAnglesToMatchVelocity( void )
{
	QAngle angles;
	VectorAngles( GetAbsVelocity(), angles );
	SetLocalAngles( angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::RocketTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// Apply forces to vehicles.
	if ( pOther->GetServerVehicle() )
	{
		ApplyForcesToVehicle( pOther );
	}

	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( filter, 0.0,	&GetAbsOrigin(), g_sModelIndexFireball, 2.0, 15, TE_EXPLFLAG_NONE, 100, m_flDamage );

	// Use the owner's position as the reported position
	Vector vecReported = vec3_origin;
	if ( GetOwnerEntity() )
	{
		vecReported = GetOwnerEntity()->GetAbsOrigin();
	}
	RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), vec3_origin, GetAbsOrigin(), GetDamage(), GetDamageType(), 0, &vecReported ), GetAbsOrigin(), GetDamageRadius(), CLASS_NONE, NULL );

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::ApplyForcesToVehicle( CBaseEntity *pEntity )
{
	// Check team - don't apply forces to our own team's vehicles.
	if ( pEntity->GetTeam() == GetTeam() )
		return;

	IServerVehicle *pVehicle = pEntity->GetServerVehicle();
	if ( !pVehicle )
		return;

	IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
	if ( pPhysObject )
	{
		//------------------------------------------------------------
		// Rocket the vehicle in the direction of the incoming rocket.
		//------------------------------------------------------------	
		Vector vecForceDir = GetAbsVelocity();
		vecForceDir.z = 0.0f;
		VectorNormalize( vecForceDir );

		float flForce = pPhysObject->GetMass();
		flForce += ( 4.0f * 100.0f );				// Wheels
		flForce *= weapon_grenade_rocket_force.GetFloat();

		vecForceDir *= flForce;

		pPhysObject->ApplyForceOffset( vecForceDir, GetAbsOrigin() );
	}
}

#else
// Client Only

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Only think when "rocketing."
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Spawn rocket effects!
//-----------------------------------------------------------------------------
void CWeaponGrenadeRocket::ClientThink( void )
{
	// Fire smoke puffs out the side
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "C_GrenadeRocket::Effect" );
	pSmokeEmitter->SetSortOrigin( GetAbsOrigin() );
	PMaterialHandle	hSphereMaterial = pSmokeEmitter->GetPMaterial( "particle/particle_noisesphere" );
	int iSmokeClouds = random->RandomInt( 1,2 );
	for ( int i = 0; i < iSmokeClouds; i++ )
	{
		SimpleParticle *pParticle = ( SimpleParticle* ) pSmokeEmitter->AddParticle( sizeof( SimpleParticle ), hSphereMaterial, GetAbsOrigin() );
		if ( !pParticle )
			return;

		// Particle data.
		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random->RandomFloat( 0.1f, 0.3f );

		pParticle->m_uchStartSize	= 10;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize + 2;

		pParticle->m_vecVelocity	= GetAbsVelocity();
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 64;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0]	= 50;
		pParticle->m_uchColor[1]	= 250;
		pParticle->m_uchColor[2]	= 50;
	}
}

#endif

