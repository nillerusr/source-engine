//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_handgrenade.h"
#include "dod_shareddefs.h"

#define GRENADE_MODEL "models/Weapons/w_frag.mdl"

LINK_ENTITY_TO_CLASS( grenade_frag_us, CDODHandGrenade );
PRECACHE_WEAPON_REGISTER( grenade_frag_us );

CDODHandGrenade* CDODHandGrenade::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner, 
	float timer,
	DODWeaponID weaponID )
{
	CDODHandGrenade *pGrenade = (CDODHandGrenade*)CBaseEntity::Create( "grenade_frag_us", position, angles, pOwner );
	
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

void CDODHandGrenade::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();
}

void CDODHandGrenade::Precache()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "HEGrenade.Bounce" );

	BaseClass::Precache();
}

void CDODHandGrenade::BounceSound( void )
{
	EmitSound( "HEGrenade.Bounce" );
}

//Pass the classname of the exploding version of this grenade.
char *CDODHandGrenade::GetExplodingClassname()
{
	return "weapon_frag_us_live";
}