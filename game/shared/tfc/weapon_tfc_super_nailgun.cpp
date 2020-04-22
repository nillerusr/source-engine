//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_super_nailgun.h"
#include "decals.h"
#include "in_buttons.h"
#include "nailgun_nail.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


// ----------------------------------------------------------------------------- //
// CTFCSuperNailgun tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCSuperNailgun, DT_WeaponSuperNailgun )

BEGIN_NETWORK_TABLE( CTFCSuperNailgun, DT_WeaponSuperNailgun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCSuperNailgun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_super_nailgun, CTFCSuperNailgun );
PRECACHE_WEAPON_REGISTER( weapon_super_nailgun );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCSuperNailgun )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCSuperNailgun implementation.
// ----------------------------------------------------------------------------- //

CTFCSuperNailgun::CTFCSuperNailgun()
{
}


TFCWeaponID CTFCSuperNailgun::GetWeaponID() const
{
	return WEAPON_SUPER_NAILGUN;
}


void CTFCSuperNailgun::PrimaryAttack()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	Assert( HasPrimaryAmmo() );

	// Effects.
	WeaponSound( SINGLE );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );

	// Create the nail.
	int iCurrentAmmoCount = pOwner->GetAmmoCount( GetPrimaryAmmoType() );

#ifdef GAME_DLL // TFCTODO: predict this
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	CTFNailgunNail *pNail = NULL;
	if ( iCurrentAmmoCount < 4 )
		 pNail = CTFNailgunNail::CreateNail( false, vecSrc, pOwner->EyeAngles(), pOwner, this, true );
	else
		pNail = CTFNailgunNail::CreateSuperNail( vecSrc, pOwner->EyeAngles(), pOwner, this );
#endif

	// Uses 2 nails if it can
	pOwner->RemoveAmmo( MIN( 2, iCurrentAmmoCount ), GetPrimaryAmmoType() );
	
	// Setup fire delays
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.1;
	m_flTimeWeaponIdle = gpGlobals->curtime + 10;
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
