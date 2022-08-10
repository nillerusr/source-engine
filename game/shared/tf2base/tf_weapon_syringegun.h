//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_SYRINGEGUN_H
#define TF_WEAPON_SYRINGEGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFSyringeGun C_TFSyringeGun
#endif

//=============================================================================
//
// TF Weapon Syringe gun.
//
class CTFSyringeGun : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CTFSyringeGun, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFSyringeGun() {}
	~CTFSyringeGun() {}

	virtual void Precache();
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_SYRINGEGUN_MEDIC; }

private:

	CTFSyringeGun( const CTFSyringeGun & ) {}
};

#endif // TF_WEAPON_SYRINGEGUN_H
