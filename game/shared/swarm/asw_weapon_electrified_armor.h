#ifndef _INCLUDED_ASW_WEAPON_ELECTRIFIED_ARMOR_H
#define _INCLUDED_ASW_WEAPON_ELECTRIFIED_ARMOR_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Electrified_Armor C_ASW_Weapon_Electrified_Armor
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

class CASW_Weapon_Electrified_Armor : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Electrified_Armor, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Electrified_Armor();
	virtual ~CASW_Weapon_Electrified_Armor();
	void Precache();
	
	virtual int ASW_SelectWeaponActivity(int idealActivity);

	void	PrimaryAttack();
	bool	OffhandActivate();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing doesn't slow the marine down

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_electrified_armor"; }		
		void ShockNearbyAliens();
	#endif
	virtual bool IsOffensiveWeapon() { return false; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_ELECTRIFIED_ARMOR; }
};


#endif /* _INCLUDED_ASW_WEAPON_ELECTRIFIED_ARMOR_H */
