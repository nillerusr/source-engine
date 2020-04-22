//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rocket Launcher (Weapon)
//
//=============================================================================//

#ifndef WEAPON_ROCKETLAUNCHER_H
#define WEAPON_ROCKETLAUNCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfcombatweapon_shared.h"

#if defined( CLIENT_DLL )
// Client Only
	#define CWeaponRocketLauncher C_WeaponRocketLauncher
#endif

//=============================================================================
//
// Rocket Launcher (Weapon)
//
class CWeaponRocketLauncher : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponRocketLauncher, CWeaponCombatUsedWithShieldBase );

public:
// Client & Server

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponRocketLauncher();
	~CWeaponRocketLauncher();

	void	Spawn( void );

	// All predicted weapons need to implement and return true
	bool	IsPredicted( void ) const { return true; }
	void	PrimaryAttack( void );
	void	ItemPostFrame( void );
	float	GetFireRate( void );
	bool	ComputeEMPFireState( void );

#if defined( CLIENT_DLL )
// Client Only

	bool	ShouldPredict( void );
#endif

private:
// Client & Server

	CWeaponRocketLauncher( const CWeaponRocketLauncher& );
};

#endif // WEAPON_ROCKETLAUNCHER_H