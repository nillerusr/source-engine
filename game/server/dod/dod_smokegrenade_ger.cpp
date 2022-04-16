//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_smokegrenade_ger.h"

#define GRENADE_MODEL "models/Weapons/w_smoke_ger.mdl"

LINK_ENTITY_TO_CLASS( grenade_smoke_ger, CDODSmokeGrenadeGER );
PRECACHE_WEAPON_REGISTER( grenade_smoke_ger );

CDODSmokeGrenadeGER* CDODSmokeGrenadeGER::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner )
{
	CDODSmokeGrenadeGER *pGrenade = (CDODSmokeGrenadeGER*)CBaseEntity::Create( "grenade_smoke_ger", position, angles, pOwner );

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

void CDODSmokeGrenadeGER::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();
}

void CDODSmokeGrenadeGER::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	BaseClass::Precache();
}