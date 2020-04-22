//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_GAS_CAN_H
#define WEAPON_GAS_CAN_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_flame_thrower.h"

#if defined( CLIENT_DLL )
#define CWeaponGasCan C_WeaponGasCan
#endif

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponGasCan : public CWeaponFlameThrower
{
	DECLARE_CLASS( CWeaponGasCan, CWeaponFlameThrower );
public:
	CWeaponGasCan();

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();


private:

	CWeaponGasCan( const CWeaponGasCan & );
};

#endif // WEAPON_GAS_CAN_H
