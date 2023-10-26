#ifndef _INCLUDED_ASW_WEAPON_FLAMER_H
#define _INCLUDED_ASW_WEAPON_FLAMER_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Flamer C_ASW_Weapon_Flamer
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class C_ASW_Emitter;

class CASW_Weapon_Flamer : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Flamer, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Flamer();
	virtual ~CASW_Weapon_Flamer();
	void Precache();

	virtual void Spawn();

	float	GetFireRate( void );
	virtual bool SupportsBayonet();
	virtual float GetWeaponDamage();
	
	Activity	GetPrimaryAttackActivity( void );
	virtual void	SecondaryAttack();
	virtual void	PrimaryAttack();
	virtual void	ClearIsFiring();
	virtual void	ItemPostFrame();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
		//cone = Vector( 200,200,200 );
		//cone = VECTOR_CONE_20DEGREES;
		//cone.z *= 0.5f;
		cone = Vector(0,0,0);

		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_flamer"; }
	#else
		virtual bool ShouldMarineFlame(); // if true, the marine emits flames from his flame emitter
		virtual bool ShouldMarineFireExtinguish();
		virtual const char* GetPartialReloadSound(int iPart);
		virtual bool Simulate();

		virtual float GetLaserPointerRange( void ) { return 280; }

		virtual void OnDataChanged( DataUpdateType_t updateType );
		virtual void UpdateOnRemove();
		virtual void ClientThink();
	#endif

	float m_flLastFireTime;
	CNetworkVar(bool, m_bBulletMod);	// used to skip ammo consumption on every other shot
	CNetworkVar(bool, m_bIsSecondaryFiring);

	enum 
	{	// namespaced immediate constant:
		FLAMER_PROJECTILE_AIR_VELOCITY = 600,
		EXTINGUISHER_PROJECTILE_AIR_VELOCITY = 400,
	};

#ifdef CLIENT_DLL
	CUtlReference<CNewParticleEffect> pEffect;	
	CUtlReference<CNewParticleEffect> pExtinguishEffect;	
	CUtlReference<CNewParticleEffect> m_hPilotLight;
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FLAMER; }
};


#endif /* _INCLUDED_ASW_WEAPON_FLAMER_H */
