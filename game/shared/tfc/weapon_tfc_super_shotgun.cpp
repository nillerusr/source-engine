//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_super_shotgun.h"
#include "decals.h"
#include "in_buttons.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


// ----------------------------------------------------------------------------- //
// CTFCSuperShotgun tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCSuperShotgun, DT_WeaponSuperShotgun )

BEGIN_NETWORK_TABLE( CTFCSuperShotgun, DT_WeaponSuperShotgun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCSuperShotgun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_super_shotgun, CTFCSuperShotgun );
PRECACHE_WEAPON_REGISTER( weapon_super_shotgun );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCSuperShotgun )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCSuperShotgun implementation.
// ----------------------------------------------------------------------------- //

CTFCSuperShotgun::CTFCSuperShotgun()
{
	m_iShellsReloaded = 2;
}


TFCWeaponID CTFCSuperShotgun::GetWeaponID() const
{
	return WEAPON_SUPER_SHOTGUN;
}


void CTFCSuperShotgun::PrimaryAttack()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	Assert( Clip1() > 0 );

	// If we've only 1 shell left, fire a single shot
	if ( Clip1() == 1 )
	{
		BaseClass::PrimaryAttack();
		return;
	}

	WeaponSound( SINGLE );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );

	
	// Shoot!	
	FireBulletsInfo_t info;
	info.m_vecSrc = pOwner->Weapon_ShootPosition();
	info.m_vecDirShooting = pOwner->GetAutoaimVector( AUTOAIM_5DEGREES );
	info.m_iShots = 14;
	info.m_flDistance = 2048;
	info.m_iAmmoType = GetPrimaryAmmoType();
	info.m_vecSpread = VECTOR_CONE_TF_SHOTGUN;
	info.m_iTracerFreq = 4;
	info.m_flDamage = 4;
	pOwner->FireBullets( info );

	m_iClip1 -= 2;
	m_flTimeWeaponIdle = gpGlobals->curtime + 5.0;
	m_fInSpecialReload = 0;

	// Setup fire delays
	if ( Clip1() != 0 )
		m_flPumpTime = gpGlobals->curtime + 0.7;
	
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.7;
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
