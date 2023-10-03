#ifndef _INLCUDED_ASW_WEAPON_PDW_H
#define _INLCUDED_ASW_WEAPON_PDW_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_PDW C_ASW_Weapon_PDW
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_PDW : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_PDW, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_PDW();
	virtual ~CASW_Weapon_PDW();
	void Precache();

	float	GetFireRate( void );
	virtual float GetWeaponDamage();

	virtual const float GetAutoAimAmount() { return AUTOAIM_2DEGREES; }	
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES; //VECTOR_CONE_20DEGREES;
		return cone;
	}
	virtual bool ShouldFlareAutoaim() { return true; }

	virtual void PrimaryAttack();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual const char* GetPickupClass() { return "asw_pickup_pdw"; }
		virtual void Spawn();
		virtual void SecondaryAttack();

		virtual float GetMadFiringBias() { return 2.5f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon

	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual void OnMuzzleFlashed();
		virtual float GetMuzzleFlashScale();
		virtual bool GetMuzzleFlashRed();
		virtual bool DisplayClipsDoubled() { return true; }    // dual weilded guns should show ammo doubled up to complete the illusion of holding two guns
		virtual const char* GetTracerEffectName() { return "tracer_pdw"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_pdw"; }	// particle effect name
	#endif

	virtual const char* GetUTracerType();
	virtual int ASW_SelectWeaponActivity(int idealActivity);
	virtual bool SupportsGroundShooting() { return false; }

	CNetworkVar(bool, m_bBulletMod);	// used to skip ammo consumption on every other shot

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PDW; }
};


#endif /* _INLCUDED_ASW_WEAPON_PDW_H */
