#ifndef _INCLUDED_ASW_WEAPON_FIRE_EXTINGUISHER_H
#define _INCLUDED_ASW_WEAPON_FIRE_EXTINGUISHER_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_FireExtinguisher C_ASW_Weapon_FireExtinguisher
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

class CASW_Weapon_FireExtinguisher : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_FireExtinguisher, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_FireExtinguisher();
	virtual ~CASW_Weapon_FireExtinguisher();
	void Precache();

	virtual void Spawn();
#ifdef CLIENT_DLL
	virtual void ClientThink();
#endif

	//float	GetFireRate( void ) { return 0.1f; }
	
	Activity	GetPrimaryAttackActivity( void );

	void	PrimaryAttack();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		
		//cone = Vector( 200,200,200 );
		//cone = VECTOR_CONE_20DEGREES;
		//cone.z *= 0.5f;
		cone = Vector(0,0,0);

		return cone;
	}

	enum
	{
		EXTINGUISHER_PROJECTILE_AIR_VELOCITY = 400,
	};

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_fire_extinguisher"; }
	#else
		virtual bool ShouldMarineFireExtinguish(); // if true, the marine emits flames from his flame emitter
		virtual float GetLaserPointerRange( void ) { return 240; }
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	float m_flLastFireTime;

#ifdef CLIENT_DLL
	CUtlReference<CNewParticleEffect> pEffect;	
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FIRE_EXTINGUISHER; }
};


#endif /* _INCLUDED_ASW_WEAPON_FIRE_EXTINGUISHER_H */
