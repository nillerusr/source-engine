#ifndef _INCLUDED_ASW_WEAPON_BUFF_GRENADE_H
#define _INCLUDED_ASW_WEAPON_BUFF_GRENADE_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Buff_Grenade C_ASW_Weapon_Buff_Grenade
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

class CASW_Weapon_Buff_Grenade : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Buff_Grenade, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Buff_Grenade();
	virtual ~CASW_Weapon_Buff_Grenade();
	void Precache();

	float	GetFireRate( void ) { return 1.4f; }
	bool	Reload();
	void	ItemPostFrame();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing mines doesn't slow the marine down
	
	Activity	GetPrimaryAttackActivity( void );

	void	PrimaryAttack();
	bool	OffhandActivate();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_buff_grenade"; }		
	#else
	
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	float	m_flSoonestPrimaryAttack;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BUFF_GRENADE; }
};


#endif /* _INCLUDED_ASW_WEAPON_BUFF_GRENADE_H */
