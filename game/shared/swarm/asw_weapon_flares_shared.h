#ifndef _INCLUDED_ASW_WEAPON_FLARES_H
#define _INCLUDED_ASW_WEAPON_FLARES_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Flares C_ASW_Weapon_Flares
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Flares : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Flares, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Flares();
	virtual ~CASW_Weapon_Flares();
	void Precache();

	float	GetFireRate( void ) { return 1.4f; }
	bool	Reload();
	void	ItemPostFrame();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing doesn't slow the marine down
	
	Activity	GetPrimaryAttackActivity( void ) { return ACT_VM_PRIMARYATTACK; }
	virtual int ASW_SelectWeaponActivity(int idealActivity);
	virtual int AmmoClickPoint() { return 0; }
	virtual float GetThrowGravity() { return 1.15f; }

	void	PrimaryAttack();
	void	DelayedAttack();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}
	virtual bool OffhandActivate();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_flares"; }
	#else
		virtual void ClientThink();
		virtual void OnDataChanged(DataUpdateType_t updateType);
		void ShowThrowArc();
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	float	m_flSoonestPrimaryAttack;

	// Classification
	virtual Class_T	Classify( void ) { return (Class_T) CLASS_ASW_FLARES; }
};


#endif /* _INCLUDED_ASW_WEAPON_FLARES_H */
