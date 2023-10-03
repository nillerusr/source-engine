#ifndef _INCLUDED_ASW_WEAPON_MINIGUN_H
#define _INCLUDED_ASW_WEAPON_MINIGUN_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Minigun C_ASW_Weapon_Minigun
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"

class C_ASW_Gun_Smoke_Emitter;
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_Minigun : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Minigun, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();

	CASW_Weapon_Minigun();
	virtual ~CASW_Weapon_Minigun();
	void Precache();

	virtual void PrimaryAttack();
	//float	GetFireRate( void ) { return 0.07f; }	

	virtual const float GetAutoAimAmount() { return 0.0f; }
	virtual const float GetAutoAimRadiusScale() { return 1.0f; }
	virtual const Vector& GetBulletSpread( void );
	virtual void ItemPostFrame();
	virtual void ItemBusyFrame();
	bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void UpdateSpinRate();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
			

		virtual const char* GetPickupClass() { return "asw_pickup_minigun"; }
		virtual void Spawn();
		virtual void SecondaryAttack();		
		virtual float GetMadFiringBias() { return 1.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
		virtual void Drop( const Vector &vecVelocity );

	#else
		DECLARE_PREDICTABLE();

		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual float GetMuzzleFlashScale();
		virtual bool GetMuzzleFlashRed();
		virtual void OnMuzzleFlashed();
		virtual void ReachedEndOfSequence();
		float m_flLastMuzzleFlashTime;
		virtual const char* GetPartialReloadSound(int iPart);

		virtual void ClientThink();
		void UpdateSpinningBarrel();

		virtual const char* GetTracerEffectName() { return "tracer_minigun"; }

		// gunsmoke
		virtual void OnDataChanged( DataUpdateType_t updateType );
		virtual void UpdateOnRemove();
		virtual void SetDormant( bool bDormant );
		void CreateGunSmoke();
		CHandle<C_ASW_Gun_Smoke_Emitter> m_hGunSmoke;

		CSoundPatch		*m_pBarrelSpinSound;
	#endif
	virtual float GetWeaponDamage();
	virtual float GetMovementScale();
	virtual bool ShouldMarineMoveSlow();
	virtual bool SupportsBayonet() { return false; }

	virtual bool SupportsGroundShooting() { return false; }

	float GetSpinRate() { return m_flSpinRate.Get(); }

	CNetworkVar( float, m_flSpinRate );		// barrel spin speed
	CNetworkVar( float, m_flPartialBullets );	// we fire a few shots per ammo bullet count

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MINIGUN; }
};


#endif /* _INCLUDED_ASW_WEAPON_MINIGUN_H */
