//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_smokegrenade_us.h"

#define GRENADE_MODEL "models/Weapons/w_smoke_us.mdl"

LINK_ENTITY_TO_CLASS( grenade_smoke_us, CDODSmokeGrenadeUS );
PRECACHE_WEAPON_REGISTER( grenade_smoke_us );

CDODSmokeGrenadeUS* CDODSmokeGrenadeUS::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner )
{
	CDODSmokeGrenadeUS *pGrenade = (CDODSmokeGrenadeUS*)CBaseEntity::Create( "grenade_smoke_us", position, angles, pOwner );

	Assert( pGrenade );

	if( !pGrenade )
		return NULL;

	IPhysicsObject *pPhysicsObject = pGrenade->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}

	// Who threw this grenade
	pGrenade->SetThrower( pOwner ); 

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

	return pGrenade;
}

void CDODSmokeGrenadeUS::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();
}

void CDODSmokeGrenadeUS::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	BaseClass::Precache();
}