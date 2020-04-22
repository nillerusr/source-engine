//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_shotgun.h"
#include "decals.h"
#include "in_buttons.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


#define RE_SHOTGUN_TIME 2


// ----------------------------------------------------------------------------- //
// CTFCShotgun tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCShotgun, DT_WeaponShotgun )

BEGIN_NETWORK_TABLE( CTFCShotgun, DT_WeaponShotgun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCShotgun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_shotgun, CTFCShotgun );
PRECACHE_WEAPON_REGISTER( weapon_shotgun );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCShotgun )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCShotgun implementation.
// ----------------------------------------------------------------------------- //

CTFCShotgun::CTFCShotgun()
{
	m_fReloadTime = 0.25;	// Time to insert an individual shell
	m_iShellsReloaded = 1;	// Number of shells inserted each reload
	m_flPumpTime = -1;
	m_fInSpecialReload = 0;
	m_flNextReload = -1;
}


TFCWeaponID CTFCShotgun::GetWeaponID() const
{
	return WEAPON_SHOTGUN;
}


void CTFCShotgun::PrimaryAttack()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	Assert( Clip1() != 0 );

	WeaponSound( SINGLE );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );

	// Shoot!	
	FireBulletsInfo_t info;
	info.m_vecSrc = pOwner->Weapon_ShootPosition();
	info.m_vecDirShooting = pOwner->GetAutoaimVector( AUTOAIM_5DEGREES );
	info.m_iShots = 6;
	info.m_flDistance = 2048;
	info.m_iAmmoType = GetPrimaryAmmoType();
	info.m_vecSpread = VECTOR_CONE_TF_SHOTGUN;
	info.m_iTracerFreq = 0;
	info.m_flDamage = 4;
	pOwner->FireBullets( info );

	m_flTimeWeaponIdle = gpGlobals->curtime + 5.0;
	m_fInSpecialReload = 0;
	
	pOwner->m_Shared.RemoveStateFlags( TFSTATE_RELOADING );

	// Setup fire delays
	m_iClip1 -= 1;
	if ( Clip1() != 0 )
		m_flPumpTime = gpGlobals->curtime + 0.5;
	
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
}


void CTFCShotgun::SecondaryAttack()
{
	return;
}


bool CTFCShotgun::Reload()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return false;

	if ( pOwner->GetAmmoCount( GetPrimaryAmmoType() ) < m_iShellsReloaded || Clip1() == GetMaxClip1() )
	{
		return false;
	}

	if (gpGlobals->curtime < m_flNextReload)
	{
		return false;
	}

	// don't reload until recoil is done
	if (gpGlobals->curtime < m_flNextPrimaryAttack)
	{
		return false;
	}

	// check to see if we're ready to reload
	if (m_fInSpecialReload == 0)
	{
		SendWeaponAnim( ACT_VM_PULLBACK ); // (ie: start reload)
		pOwner->m_Shared.AddStateFlags( TFSTATE_RELOADING );
		m_fInSpecialReload = 1;
		m_flTimeWeaponIdle = gpGlobals->curtime + 0.1; // 0.6;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.1; // 1.0;
	}
	else if (m_fInSpecialReload == 1)
	{
		if ( m_flTimeWeaponIdle > gpGlobals->curtime )
		{
			return false;
		}
		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		WeaponSound( RELOAD );

		m_flNextReload = gpGlobals->curtime + m_fReloadTime; //0.5;
		m_flTimeWeaponIdle = gpGlobals->curtime + m_fReloadTime; //0.5;
	}
	else
	{
		// Add them to the clip
		m_iClip1 += m_iShellsReloaded;
		pOwner->RemoveAmmo( m_iShellsReloaded, GetPrimaryAmmoType() );

		m_fInSpecialReload = 1;
		pOwner->m_Shared.RemoveStateFlags( TFSTATE_RELOADING );
	}
	
	return true;
}


void CTFCShotgun::WeaponIdle()
{
	CTFCPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	// After firing, pump the shotgun.
	if ( m_flPumpTime != -1 && gpGlobals->curtime >= m_flPumpTime )
	{
		SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
		WeaponSound( SPECIAL1 );	// Pump sound.
		
		m_flPumpTime = -1;
	}

	if ( gpGlobals->curtime > m_flTimeWeaponIdle)
	{
		if (Clip1() == 0 && m_fInSpecialReload == 0 && pOwner->GetAmmoCount( GetPrimaryAmmoType() ) >= m_iShellsReloaded)
		{
			Reload( );
		}
		else if (m_fInSpecialReload != 0)
		{
			if (Clip1() != GetMaxClip1() && pOwner->GetAmmoCount( GetPrimaryAmmoType() ) >= m_iShellsReloaded)
			{
				Reload( );
			}
			else
			{
				// play pumping sound
				SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
				WeaponSound( SPECIAL1 );	// Pump sound.

				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = gpGlobals->curtime + 1.5;
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
			m_flTimeWeaponIdle = gpGlobals->curtime + GetViewModelSequenceDuration();
		}
	}
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
