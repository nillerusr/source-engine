//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_syringegun.h"
#include "tf_fx_shared.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#endif

//=============================================================================
//
// Weapon Syringe Gun tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFSyringeGun, DT_WeaponSyringeGun )

BEGIN_NETWORK_TABLE( CTFSyringeGun, DT_WeaponSyringeGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFSyringeGun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_syringegun_medic, CTFSyringeGun );
PRECACHE_WEAPON_REGISTER( tf_weapon_syringegun_medic );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFSyringeGun )
END_DATADESC()
#endif

//=============================================================================
//
// Weapon SyringeGun functions.
//
void CTFSyringeGun::Precache()
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	PrecacheParticleSystem( "nailtrails_medic_blue_crit" );
	PrecacheParticleSystem( "nailtrails_medic_blue" );
	PrecacheParticleSystem( "nailtrails_medic_red_crit" );
	PrecacheParticleSystem( "nailtrails_medic_red" );
#endif
}
