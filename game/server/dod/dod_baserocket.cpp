//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_baserocket.h"
#include "explode.h"
#include "dod_shareddefs.h"
#include "dod_gamerules.h"
#include "fx_dod_shared.h"


BEGIN_DATADESC( CDODBaseRocket )

	// Function Pointers
	DEFINE_FUNCTION( RocketTouch ),

	DEFINE_THINKFUNC( FlyThink ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST( CDODBaseRocket, DT_DODBaseRocket )
	SendPropVector( SENDINFO( m_vInitialVelocity ), 
		20,		// nbits
		0,		// flags
		-3000,	// low value
		3000	// high value
		)
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( base_rocket, CDODBaseRocket );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDODBaseRocket::CDODBaseRocket()
{
}

CDODBaseRocket::~CDODBaseRocket()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODBaseRocket::Precache( void )
{
	PrecacheScriptSound( "Weapon_Bazooka.Shoot" );	
	PrecacheParticleSystem( "rockettrail" );
}

ConVar mp_rocketdamage( "mp_rocketdamage", "150", FCVAR_GAMEDLL | FCVAR_CHEAT );
ConVar mp_rocketradius( "mp_rocketradius", "200", FCVAR_GAMEDLL | FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODBaseRocket::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );

	Assert( GetModel() );	//derived classes must have set model

	UTIL_SetSize( this, -Vector(2,2,2), Vector(2,2,2) );

	SetTouch( &CDODBaseRocket::RocketTouch );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	
	m_takedamage = DAMAGE_NO;
	SetGravity( 0.1 );
	SetDamage( mp_rocketdamage.GetFloat() );	

	AddFlag( FL_OBJECT );

	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	EmitSound( "Weapon_Bazooka.Shoot" );

	m_flCollideWithTeammatesTime = gpGlobals->curtime + 0.25;
	m_bCollideWithTeammates = false;

	SetThink( &CDODBaseRocket::FlyThink );
	SetNextThink( gpGlobals->curtime );
}

unsigned int CDODBaseRocket::PhysicsSolidMaskForEntity( void ) const
{ 
	int teamContents = 0;

	if ( m_bCollideWithTeammates == false )
	{
		// Only collide with the other team
		teamContents = ( GetTeamNumber() == TEAM_ALLIES ) ? CONTENTS_TEAM1 : CONTENTS_TEAM2;
	}
	else
	{
		// Collide with both teams
		teamContents = CONTENTS_TEAM1 | CONTENTS_TEAM2;
	}

	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX | teamContents;
}

//-----------------------------------------------------------------------------
// Purpose: Stops any kind of tracking and shoots dumb
//-----------------------------------------------------------------------------
void CDODBaseRocket::Fire( void )
{
	SetThink( NULL );
	SetMoveType( MOVETYPE_FLY );

	SetModel("models/weapons/w_missile.mdl");
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	EmitSound( "Weapon_Bazooka.Shoot" );
}

//-----------------------------------------------------------------------------
// The actual explosion 
//-----------------------------------------------------------------------------
void CDODBaseRocket::DoExplosion( trace_t *pTrace )
{
/*
	// Explode
	ExplosionCreate( 
		GetAbsOrigin(),	//DMG_ROCKET
		GetAbsAngles(),
		GetOwnerEntity(),
		GetDamage(),		//magnitude
		mp_rocketradius.GetFloat(),				//radius
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE,
		0.0f,				//explosion force
		this);				//inflictor
*/

	// Pull out of the wall a bit
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
	}

	// Explosion effect on client
	Vector vecOrigin = GetAbsOrigin();
	CPVSFilter filter( vecOrigin );
	TE_DODExplosion( filter, 0.0f, vecOrigin, pTrace->plane.normal );

	CTakeDamageInfo info( this, GetOwnerEntity(), vec3_origin, GetAbsOrigin(), GetDamage(), DMG_BLAST, 0 );
	RadiusDamage( info, vecOrigin, mp_rocketradius.GetFloat() /* GetDamageRadius() */, CLASS_NONE, NULL );

	// stun players in a radius
	const float flStunDamage = 75;
	const float flRadius = 150;

	CTakeDamageInfo stunInfo( this, GetOwnerEntity(), vec3_origin, GetAbsOrigin(), flStunDamage, DMG_STUN );
	DODGameRules()->RadiusStun( stunInfo, GetAbsOrigin(), flRadius );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODBaseRocket::Explode( void )
{
	// Don't explode against the skybox. Just pretend that 
	// the missile flies off into the distance.
	const trace_t &tr = CBaseEntity::GetTouchTrace();
	const trace_t *p = &tr;
	trace_t *newTrace = const_cast<trace_t*>(p);

	DoExplosion( newTrace );

	if ( newTrace->m_pEnt && !newTrace->m_pEnt->IsPlayer() )
		UTIL_DecalTrace( newTrace, "Scorch" );

	StopSound( "Weapon_Bazooka.Shoot" );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CDODBaseRocket::RocketTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	if ( pOther->GetCollisionGroup() == COLLISION_GROUP_WEAPON )
		return;

	// if we hit the skybox, just disappear
	const trace_t &tr = CBaseEntity::GetTouchTrace();

	const trace_t *p = &tr;
	trace_t *newTrace = const_cast<trace_t*>(p);

	if( tr.surface.flags & SURF_SKY )
	{
		UTIL_Remove( this );
		return;
	}

	if( !pOther->IsPlayer() && pOther->m_takedamage == DAMAGE_YES )
	{
		CTakeDamageInfo info;
		info.SetAttacker( this );
		info.SetInflictor( this );
		info.SetDamage( 50 );
		info.SetDamageForce( vec3_origin );	// don't worry about this not having a damage force.
											// It will explode on touch and impart its own forces
		info.SetDamageType( DMG_CLUB );

		Vector dir;
		AngleVectors( GetAbsAngles(), &dir );

		pOther->DispatchTraceAttack( info, dir, newTrace );
		ApplyMultiDamage();

		if( pOther->IsAlive() )
		{
			Explode();
		}

		// if it's not alive, continue flying
	}
	else
	{
		Explode();
	}
}

void CDODBaseRocket::FlyThink( void )
{
	QAngle angles;

	VectorAngles( GetAbsVelocity(), angles );

	SetAbsAngles( angles );

	if ( gpGlobals->curtime > m_flCollideWithTeammatesTime && m_bCollideWithTeammates == false )
	{
		m_bCollideWithTeammates = true;
	}
	
	SetNextThink( gpGlobals->curtime + 0.1f );
}

	
//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : &vecOrigin - 
//			&vecAngles - 
//			NULL - 
//
// Output : CDODBaseRocket
//-----------------------------------------------------------------------------
CDODBaseRocket *CDODBaseRocket::Create( const char *szClassname, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = NULL )
{
	CDODBaseRocket *pMissile = (CDODBaseRocket *) CBaseEntity::Create( szClassname, vecOrigin, vecAngles, pOwner );
	pMissile->SetOwnerEntity( pOwner );
	pMissile->Spawn();
	
	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );

	Vector vRocket = vecForward * 1300;

	pMissile->SetAbsVelocity( vRocket );	
	pMissile->SetupInitialTransmittedGrenadeVelocity( vRocket );

	pMissile->SetAbsAngles( vecAngles );

	// remember what team we should be on
	pMissile->ChangeTeam( pOwner->GetTeamNumber() );

	return pMissile;
}

void CDODBaseRocket::SetupInitialTransmittedGrenadeVelocity( const Vector &velocity )
{
	m_vInitialVelocity = velocity;
}
