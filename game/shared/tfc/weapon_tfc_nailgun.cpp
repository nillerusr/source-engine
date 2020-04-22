//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_nailgun.h"
#include "decals.h"
#include "in_buttons.h"
#include "nailgun_nail.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


// ----------------------------------------------------------------------------- //
// CTFCNailgun tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCNailgun, DT_WeaponNailgun )

BEGIN_NETWORK_TABLE( CTFCNailgun, DT_WeaponNailgun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCNailgun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_nailgun, CTFCNailgun );
PRECACHE_WEAPON_REGISTER( weapon_nailgun );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCNailgun )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCNailgun implementation.
// ----------------------------------------------------------------------------- //

CTFCNailgun::CTFCNailgun()
{
}


TFCWeaponID CTFCNailgun::GetWeaponID() const
{
	return WEAPON_NAILGUN;
}


void CTFCNailgun::PrimaryAttack()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	Assert( HasPrimaryAmmo() );

	WeaponSound( SINGLE );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );

	// Fire the nail
#ifdef GAME_DLL
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	CTFNailgunNail *pNail = CTFNailgunNail::CreateNail( 
		false, 
		vecSrc, 
		pOwner->EyeAngles(), 
		pOwner, 
		this, 
		true );
	pNail=pNail; // avoid compiler warning..
#endif

	pOwner->RemoveAmmo( 1, GetPrimaryAmmoType() );
	
	// Setup fire delays
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.1;
	m_flTimeWeaponIdle = gpGlobals->curtime + 10;
}


void CTFCNailgun::SecondaryAttack()
{
	return;
}


#ifdef CLIENT_DLL
	
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// CLIENT DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //


#else

	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// GAME DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //

#endif
