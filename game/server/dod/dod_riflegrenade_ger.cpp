//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_riflegrenade_ger.h"
#include "dod_shareddefs.h"

#define GRENADE_MODEL "models/Weapons/w_k98_rg_grenade.mdl"

LINK_ENTITY_TO_CLASS( grenade_riflegren_ger, CDODRifleGrenadeGER );
PRECACHE_WEAPON_REGISTER( grenade_riflegren_ger );

IMPLEMENT_SERVERCLASS_ST( CDODRifleGrenadeGER, DT_DODRifleGrenadeGER )
END_SEND_TABLE()

CDODRifleGrenadeGER* CDODRifleGrenadeGER::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner, 
	float timer,
	DODWeaponID weaponID )
{
	CDODRifleGrenadeGER *pGrenade = (CDODRifleGrenadeGER*)CBaseEntity::Create( "grenade_riflegren_ger", position, angles, pOwner );

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

void CDODRifleGrenadeGER::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();
}

void CDODRifleGrenadeGER::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "HEGrenade.Bounce" );
	BaseClass::Precache();
}

//Pass the classname of the exploding version of this grenade.
char *CDODRifleGrenadeGER::GetExplodingClassname()
{
	return "weapon_riflegren_ger_live";
}