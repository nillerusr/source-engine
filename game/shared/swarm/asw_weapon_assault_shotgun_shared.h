#ifndef _INCLUDED_ASW_WEAPON_SHOTGUN_ASSAULT_H
#define _INCLUDED_ASW_WEAPON_SHOTGUN_ASSAULT_H
#pragma once

#include "asw_weapon_shotgun_shared.h"

#ifdef CLIENT_DLL
	#define CASW_Weapon_Assault_Shotgun C_ASW_Weapon_Assault_Shotgun
#else
	#include "npc_combine.h"
#endif

class CASW_Weapon_Assault_Shotgun : public CASW_Weapon_Shotgun
{
public:
	DECLARE_CLASS( CASW_Weapon_Assault_Shotgun, CASW_Weapon_Shotgun );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Assault_Shotgun();
	virtual ~CASW_Weapon_Assault_Shotgun();
	void Precache();

	//float	GetFireRate( void ) { return 0.65f; }
	virtual float GetWeaponDamage();
	virtual int GetNumPellets();
	virtual int AmmoClickPoint() { return 2; }

	virtual void SecondaryAttack();	

	virtual const float GetAutoAimAmount() { return 0.26f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual const char* GetPickupClass() { return "asw_pickup_vindicator"; }

		virtual float GetMadFiringBias() { return 1.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
	#else
		virtual bool HasSecondaryExplosive( void ) const { return true; }
		virtual const char* GetPartialReloadSound(int iPart);
		virtual float GetMuzzleFlashScale();
		virtual const char* GetTracerEffectName() { return "tracer_vindicator"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_vindicator"; }	// particle effect name
		virtual float GetLaserPointerRange( void ) { return 360; }
	#endif

	virtual bool ShouldMarineMoveSlow();

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_ASSAULT_SHOTGUN; }
};


#endif /* _INCLUDED_ASW_WEAPON_SHOTGUN_ASSAULT_H */
