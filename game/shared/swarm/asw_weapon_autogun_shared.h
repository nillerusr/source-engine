#ifndef _INCLUDED_ASW_WEAPON_AUTOGUN_H
#define _INCLUDED_ASW_WEAPON_AUTOGUN_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Autogun C_ASW_Weapon_Autogun
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"

class C_ASW_Gun_Smoke_Emitter;
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_Autogun : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Autogun, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();

	CASW_Weapon_Autogun();
	virtual ~CASW_Weapon_Autogun();
	void Precache();

	//float	GetFireRate( void ) { return 0.1f; }

	virtual const float GetAutoAimAmount() { return 0.36f; }	
	virtual const float GetAutoAimRadiusScale() { return 1.5f; }
	virtual bool ShouldFlareAutoaim() { return true; }
	virtual const Vector& GetBulletSpread( void );

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
			

		virtual const char* GetPickupClass() { return "asw_pickup_autogun"; }
		virtual void Spawn();
		virtual void SecondaryAttack();		
		virtual float GetMadFiringBias() { return 1.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
	#else
		DECLARE_PREDICTABLE();
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual float GetMuzzleFlashScale();
		virtual bool GetMuzzleFlashRed();
		virtual void OnMuzzleFlashed();
		virtual void ReachedEndOfSequence();
		float m_flLastMuzzleFlashTime;
		virtual const char* GetPartialReloadSound(int iPart);

		// gunsmoke
		virtual void OnDataChanged( DataUpdateType_t updateType );
		virtual void UpdateOnRemove();
		void CreateGunSmoke();
		CHandle<C_ASW_Gun_Smoke_Emitter> m_hGunSmoke;

		virtual const char* GetTracerEffectName() { return "tracer_autogun"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_autogun"; }	// particle effect name
		
	#endif
	virtual float GetWeaponDamage();
	virtual float GetMovementScale();
	virtual bool SupportsBayonet();
	virtual void OnStoppedFiring();

	virtual bool SupportsGroundShooting() { return false; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AUTOGUN; }
};


#endif /* _INCLUDED_ASW_WEAPON_AUTOGUN_H */
