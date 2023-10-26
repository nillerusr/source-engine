#ifndef _INCLUDED_ASW_WEAPON_FLASHLIGHT_H
#define _INCLUDED_ASW_WEAPON_FLASHLIGHT_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Flashlight C_ASW_Weapon_Flashlight
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Flashlight : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Flashlight, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Flashlight();
	virtual ~CASW_Weapon_Flashlight();
	void Precache();
	
	Activity	GetPrimaryAttackActivity( void );

	void	PrimaryAttack();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		// todo: stop AI attacking with this?
		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_flashlight"; }

		// for toggling the flashlight effect when we take/drop this weapon
		virtual void MarineDropped(CASW_Marine* pMarine);
		virtual void Equip( CBaseCombatCharacter *pOwner );		
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	//virtual bool ASWCanBeSelected() { return false; }	// no selecting this

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FLASHLIGHT; }
};


#endif /* _INCLUDED_ASW_WEAPON_FLASHLIGHT_H */
