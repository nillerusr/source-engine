//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rocket Launcher (Weapon)
//
//=============================================================================//

#include "cbase.h"
#include "weapon_combatshield.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_rocketlauncher.h"
#include "in_buttons.h"

#include "plasmaprojectile.h"
#include "weapon_grenade_rocket.h"

#if !defined( CLIENT_DLL )
// Server Only
#include "grenade_rocket.h"
#else
// Client Only
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

 
PRECACHE_WEAPON_REGISTER( weapon_rocket_launcher );
LINK_ENTITY_TO_CLASS( weapon_rocket_launcher, CWeaponRocketLauncher );

BEGIN_PREDICTION_DATA( CWeaponRocketLauncher )
END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponRocketLauncher, DT_WeaponRocketLauncher )
BEGIN_NETWORK_TABLE( CWeaponRocketLauncher, DT_WeaponRocketLauncher )
//#if !defined( CLIENT_DLL )
//#else
//#endif
END_NETWORK_TABLE()

#if !defined( CLIENT_DLL )
// Server Only
ConVar weapon_rocket_launcher_damage( "weapon_rocket_launcher_damage","300", FCVAR_NONE, "Rocker launcher damage" );
ConVar weapon_rocket_launcher_range( "weapon_rocket_launcher_range", "768", FCVAR_NONE, "Rocket launcher range" );
#endif

#define WEAPON_ROCKET_LAUNCHER_FIRERATE		1.0f
#define WEAPON_ROCKET_LAUNCHER_START_AMMO	1

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponRocketLauncher::CWeaponRocketLauncher()
{
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CWeaponRocketLauncher::~CWeaponRocketLauncher()
{
}

//-----------------------------------------------------------------------------/
// Purpose: Don't fire when EMPed
//-----------------------------------------------------------------------------
bool CWeaponRocketLauncher::ComputeEMPFireState( void )
{
	if ( IsOwnerEMPed() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponRocketLauncher::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Handle deploying, undeploying, firing, etc.
// TODO: Add a deploy to the firing!  Currently no reloading!
//-----------------------------------------------------------------------------
void CWeaponRocketLauncher::ItemPostFrame( void )
{
	// Get the player.
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

#if !defined( CLIENT_DLL )
	if ( !HasPrimaryAmmo() && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
	{
		pPlayer->SwitchToNextBestWeapon( NULL );
	}
#endif

	// Handle Firing
	if ( GetShieldState() == SS_DOWN && !m_bInReload )
	{
		// Attempting to fire.
		if ( ( pPlayer->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
		{
			if ( m_iClip1 > 0 )
			{
				PrimaryAttack();
			}
			else
			{
				Reload();
			}
		}

		// Reload button (or fire button when we're out of ammo)
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime ) 
		{
			if ( pPlayer->m_nButtons & IN_RELOAD ) 
			{
				Reload();
			}
			else if ( !((pPlayer->m_nButtons & IN_ATTACK) || (pPlayer->m_nButtons & IN_ATTACK2) || (pPlayer->m_nButtons & IN_RELOAD)) )
			{
				if ( !m_iClip1 && HasPrimaryAmmo() )
				{
					Reload();
				}
			}
		}
	}

	// Prevent shield post frame if we're not ready to attack, or we're charging
	AllowShieldPostFrame( m_flNextPrimaryAttack <= gpGlobals->curtime || m_bInReload );
}

//-----------------------------------------------------------------------------
// Purpose: Firing
//-----------------------------------------------------------------------------
void CWeaponRocketLauncher::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = ( CBaseTFPlayer* )GetOwner();
	if ( !pPlayer )
		return;

	if ( !ComputeEMPFireState() )
		return;

	// Weapon "Fire" sound.
	WeaponSound( SINGLE );

	// Play the attack animation (need one for rocket launcher - deploy?)
	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Fire the rocket (Get the position and angles).
	Vector vecFirePos = pPlayer->Weapon_ShootPosition();
	Vector vecFireAng;
	pPlayer->EyeVectors( &vecFireAng );

	// Shift it down a bit so the firer can see it
	Vector vecRight;
	AngleVectors( pPlayer->EyeAngles() + pPlayer->m_Local.m_vecPunchAngle, NULL, &vecRight, NULL );
	vecFirePos += Vector( 0, 0, -8 ) + vecRight * 12;

	// Create the rocket.
#if !defined( CLIENT_DLL )
	CWeaponGrenadeRocket *pRocket = CWeaponGrenadeRocket::Create( vecFirePos, vecFireAng, weapon_rocket_launcher_range.GetFloat(), pPlayer );
#else
	CWeaponGrenadeRocket *pRocket = CWeaponGrenadeRocket::Create( vecFirePos, vecFireAng, 0, pPlayer );
#endif
	if ( pRocket )
	{
		pRocket->SetRealOwner( pPlayer );
#if !defined( CLIENT_DLL )
		pRocket->SetDamage( weapon_rocket_launcher_damage.GetFloat() );
#endif
	}

	// Essentially you are done!
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: Set the rocket launcher firing rate.
//-----------------------------------------------------------------------------
float CWeaponRocketLauncher::GetFireRate( void )
{
	return WEAPON_ROCKET_LAUNCHER_FIRERATE;
}

#if defined( CLIENT_DLL )
// Client Only

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponRocketLauncher::ShouldPredict( void )
{
	if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
		return true;
	
	return BaseClass::ShouldPredict();
}

#endif
