#ifndef _INCLUDED_ASW_WEAPON_FREEZE_GRENADES_H
#define _INCLUDED_ASW_WEAPON_FREEZE_GRENADES_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Freeze_Grenades C_ASW_Weapon_Freeze_Grenades
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Freeze_Grenades : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Freeze_Grenades, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Freeze_Grenades();
	virtual ~CASW_Weapon_Freeze_Grenades();
	void Precache();

	float	GetFireRate( void ) { return 1.4f; }
	bool	Reload();
	void	ItemPostFrame();
	virtual bool ShouldMarineMoveSlow() { return false; }	// throwing grenades doesn't slow the marine down
	
	Activity	GetPrimaryAttackActivity( void );
	virtual int ASW_SelectWeaponActivity(int idealActivity);
	virtual int AmmoClickPoint() { return 0; }
	virtual float GetThrowGravity() { return 2.0f; }

	void	PrimaryAttack();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}
	virtual bool OffhandActivate();
	virtual void DelayedAttack();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const char* GetPickupClass() { return "asw_pickup_freeze_grenades"; }	
	
	// Get the damage and radius that a grenade thrown by a given marine should 
	// explode with:
	static float GetBoomRadius( CASW_Marine *pMarine );

#else
#endif

	virtual bool IsOffensiveWeapon() { return false; }

	float	m_flSoonestPrimaryAttack;	

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FREEZE_GRENADES; }
};


#endif /* _INCLUDED_ASW_WEAPON_FREEZE_GRENADES_H */
