#ifndef _INCLUDED_ASW_WEAPON_BLINK_H
#define _INCLUDED_ASW_WEAPON_BLINK_H
#pragma once

#include "asw_shareddefs.h"

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Blink C_ASW_Weapon_Blink
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"

class CASW_Weapon_Blink : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Blink, CASW_Weapon );
	DECLARE_NETWORKCLASS();

#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif

	CASW_Weapon_Blink();
	virtual ~CASW_Weapon_Blink();
	void Precache();

	float	GetFireRate( void ) { return 1.4f; }
	virtual void Spawn();
	bool	Reload();
	void	ItemPostFrame();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing doesn't slow the marine down
	
	Activity	GetPrimaryAttackActivity( void ) { return ACT_VM_PRIMARYATTACK; }
	virtual int ASW_SelectWeaponActivity(int idealActivity);
	virtual int AmmoClickPoint() { return 0; }
	virtual bool WantsOffhandPostFrame() { return true; }
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BLINK; }
	virtual int GetChargesForHUD() { return (int) GetPower(); }
	virtual void UpdatePower();
	virtual void HandleFireOnEmpty();

	void	PrimaryAttack();
	
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}
	virtual bool OffhandActivate();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
		bool	SetBlinkDestination();

		Vector m_vecInvalidDestination;
	#else
	#endif

	void DoBlink();
	virtual bool IsOffensiveWeapon() { return false; }

	float GetPower() { return m_flPower.Get(); }
	virtual float GetBatteryCharge();
	virtual float GetMinBatteryChargeToActivate();

	float	m_flSoonestPrimaryAttack;

	CNetworkVar( float, m_flPower );
	CNetworkVar( Vector, m_vecAbilityDestination );
};


#endif /* _INCLUDED_ASW_WEAPON_BLINK_H */
