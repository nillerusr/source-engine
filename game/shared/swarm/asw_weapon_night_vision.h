#ifndef _INCLUDED_ASW_WEAPON_NIGHT_VISION_H
#define _INCLUDED_ASW_WEAPON_NIGHT_VISION_H
#pragma once

#include "asw_shareddefs.h"

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Night_Vision C_ASW_Weapon_Night_Vision
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"

class CASW_Weapon_Night_Vision : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Night_Vision, CASW_Weapon );
	DECLARE_NETWORKCLASS();

#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif

	CASW_Weapon_Night_Vision();
	virtual ~CASW_Weapon_Night_Vision();
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
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_NIGHT_VISION; }
	virtual int GetChargesForHUD() { return (int) GetPower(); }
	virtual void UpdateVisionPower();
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

		virtual const char* GetPickupClass() { return "asw_pickup_night_vision"; }
	#else
		float UpdateVisionAlpha();
		float UpdateFlashAlpha();

		float m_flVisionAlpha;
		float m_flFlashAlpha;
		bool m_bOldVisionActive;
	#endif

	virtual bool IsOffensiveWeapon() { return false; }
	bool IsVisionActive() { return m_bVisionActive.Get(); }
	float GetPower() { return m_flPower.Get(); }
	virtual float GetBatteryCharge();

	float	m_flSoonestPrimaryAttack;

	CNetworkVar( float, m_flPower );
	CNetworkVar( bool, m_bVisionActive );
};


#endif /* _INCLUDED_ASW_WEAPON_NIGHT_VISION_H */
