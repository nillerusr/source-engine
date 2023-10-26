#ifndef _INCLUDED_ASW_WEAPON_PRIFLE_H
#define _INCLUDED_ASW_WEAPON_PRIFLE_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_PRifle C_ASW_Weapon_PRifle
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_PRifle : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_PRifle, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_PRifle();
	virtual ~CASW_Weapon_PRifle();
	void Precache();

	virtual const float GetAutoAimAmount() { return 0.26f; }
	virtual const float GetAutoAimRadiusScale() { return 1.5f; }
	virtual float GetWeaponDamage();
	virtual void SecondaryAttack();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual const char* GetPickupClass() { return "asw_pickup_prifle"; }		
	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual const char* GetPartialReloadSound(int iPart);

		virtual const char* GetTracerEffectName() { return "tracer_proto_rifle"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_proto_rifle"; }	// particle effect name
	#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PRIFLE; }
};


#endif /* _INCLUDED_ASW_WEAPON_PRIFLE_H */
