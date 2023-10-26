#ifndef _INCLUDED_ASW_WEAPON_STIM_H
#define _INCLUDED_ASW_WEAPON_STIM_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Stim C_ASW_Weapon_Stim
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Stim : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Stim, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Stim();
	virtual ~CASW_Weapon_Stim();
	void Precache();
	
	Activity	GetPrimaryAttackActivity( void );
	virtual int ASW_SelectWeaponActivity(int idealActivity);

	void	PrimaryAttack();
	void	InjectStim();
	bool	OffhandActivate();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing stims doesn't slow the marine down

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_stim"; }		
	#endif
	virtual bool IsOffensiveWeapon() { return false; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_STIM; }
};


#endif /* _INCLUDED_ASW_WEAPON_STIM_H */
