#ifndef _INCLUDED_ASW_WEAPON_GRENADE_LAUNCHER_H
#define _INCLUDED_ASW_WEAPON_GRENADE_LAUNCHER_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Grenade_Launcher C_ASW_Weapon_Grenade_Launcher
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Grenade_Launcher : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Grenade_Launcher, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Grenade_Launcher();
	virtual ~CASW_Weapon_Grenade_Launcher();
	void Precache();

	bool	Reload();
	void	ItemPostFrame();
	virtual bool ShouldMarineMoveSlow() { return false; }	// throwing grenades doesn't slow the marine down
	
	virtual int AmmoClickPoint() { return 0; }
	virtual float GetFireRate() { return 0.4f; }

	void	PrimaryAttack();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	#else
	
	#endif

	virtual bool IsOffensiveWeapon() { return true; }

	float	m_flSoonestPrimaryAttack;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_GRENADE_LAUNCHER; }

};


#endif /* _INCLUDED_ASW_WEAPON_GRENADE_LAUNCHER_H */
