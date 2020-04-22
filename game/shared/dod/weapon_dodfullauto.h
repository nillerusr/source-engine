//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODFULLAUTO_H
#define WEAPON_DODFULLAUTO_H

#include "cbase.h" 
#include "shake.h" 
#include "weapon_dodbasegun.h"

#if defined( CLIENT_DLL )
	#define CDODFullAutoWeapon C_DODFullAutoWeapon
#endif

class CDODFullAutoWeapon : public CWeaponDODBaseGun
{
public:
	DECLARE_CLASS( CDODFullAutoWeapon, CWeaponDODBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CDODFullAutoWeapon();

	virtual void Spawn();
	virtual void PrimaryAttack( void );

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }

private:
	CDODFullAutoWeapon( const CDODFullAutoWeapon & );
};

#endif //WEAPON_DODFULLAUTO_H