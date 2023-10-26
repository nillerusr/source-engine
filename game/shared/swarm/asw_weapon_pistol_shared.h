#ifndef _DEFINED_ASW_WEAPON_PISTOL_H
#define _DEFINED_ASW_WEAPON_PISTOL_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Pistol C_ASW_Weapon_Pistol
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

enum ASW_Weapon_PistolHand_t
{
	ASW_WEAPON_PISTOL_RIGHT,
	ASW_WEAPON_PISTOL_LEFT
};

class CASW_Weapon_Pistol : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Pistol, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Pistol();
	virtual ~CASW_Weapon_Pistol();
	void Precache();

	float	GetFireRate( void );
	
	void ItemPostFrame();
	Activity	GetPrimaryAttackActivity( void );
	virtual bool ShouldMarineMoveSlow();

	void	PrimaryAttack();
	int ASW_SelectWeaponActivity(int idealActivity);
	virtual float GetWeaponDamage();
	virtual int AmmoClickPoint() { return 2; }

	virtual const float GetAutoAimAmount() { return 0.26f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = vec3_origin;
		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_pistol"; }

		virtual bool IsRapidFire() { return false; }
		virtual float GetMadFiringBias() { return 0.2f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
	#else
		virtual const char* GetPartialReloadSound(int iPart);
		//virtual const char* GetTracerEffectName() { return "tracer_pistol"; }	// particle effect name
		//virtual const char* GetMuzzleEffectName() { return "muzzle_pistol"; }	// particle effect name

		virtual bool DisplayClipsDoubled() { return false; }
		virtual const char* GetTracerEffectName() { return "tracer_pistol"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_pistol"; }	// particle effect name
	#endif

	virtual const char* GetUTracerType();
	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PISTOL; }

protected:
	CNetworkVar( float, m_fSlowTime );	// marine moves slow until this moment
	float	m_flSoonestPrimaryAttack;
	ASW_Weapon_PistolHand_t m_currentPistol;
};


#endif /* _DEFINED_ASW_WEAPON_PISTOL_H */
