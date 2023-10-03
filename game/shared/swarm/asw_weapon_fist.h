#ifndef _INCLUDED_ASW_WEAPON_FIST_H
#define _INCLUDED_ASW_WEAPON_FIST_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Fist C_ASW_Weapon_Fist
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

extern ConVar asw_fist_passive_damage_scale;

// passive offhand item that boosts melee damage
class CASW_Weapon_Fist : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Fist, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Fist();
	virtual ~CASW_Weapon_Fist();
	
	void	PrimaryAttack();
	
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing doesn't slow the marine down

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }	
	#endif

	virtual bool IsOffensiveWeapon() { return false; }
	virtual float GetPassiveMeleeDamageScale() { return asw_fist_passive_damage_scale.GetFloat(); }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FIST; }
};


#endif /* _INCLUDED_ASW_WEAPON_FIST_H */
