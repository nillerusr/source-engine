//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_riflegrenade_us.h"
#include "dod_shareddefs.h"

#define GRENADE_MODEL "models/Weapons/w_garand_rg_grenade.mdl"

LINK_ENTITY_TO_CLASS( grenade_riflegren_us, CDODRifleGrenadeUS );
PRECACHE_WEAPON_REGISTER( grenade_riflegren_us );

IMPLEMENT_SERVERCLASS_ST( CDODRifleGrenadeUS, DT_DODRifleGrenadeUS )
END_SEND_TABLE()

CDODRifleGrenadeUS* CDODRifleGrenadeUS::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner, 
	float timer,
	DODWeaponID weaponID )
{
	CDODRifleGrenadeUS *pGrenade = (CDODRifleGrenadeUS*)CBaseEntity::Create( "grenade_riflegren_us", position, angles, pOwner );

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

	// How long until we explode
	pGrenade->SetDetonateTimerLength( timer );

	// Save the weapon id for stats purposes
	pGrenade->m_EmitterWeaponID = weaponID;

	return pGrenade;
}

void CDODRifleGrenadeUS::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();
}

void CDODRifleGrenadeUS::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "HEGrenade.Bounce" );
	BaseClass::Precache();
}

//Pass the classname of the exploding version of this grenade.
char *CDODRifleGrenadeUS::GetExplodingClassname()
{
	return "weapon_riflegren_us_live";
}