#ifndef _INCLUDED_ASW_WEAPON_SHOTGUN_H
#define _INCLUDED_ASW_WEAPON_SHOTGUN_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Shotgun C_ASW_Weapon_Shotgun
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Shotgun_Pellet;

class CASW_Weapon_Shotgun : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Shotgun, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Shotgun();
	virtual ~CASW_Weapon_Shotgun();
	void Precache();

	float	GetFireRate( void );
	virtual bool SupportsBayonet();
	virtual float GetWeaponDamage();
	virtual int GetNumPellets();
	const char *GetTracerType( void ) { return "ASWTracer"; }
	virtual int AmmoClickPoint() { return 0; }
	
	Activity	GetPrimaryAttackActivity( void );
	virtual bool ShouldMarineMoveSlow();

	void	PrimaryAttack();
	virtual int ASW_SelectWeaponActivity(int idealActivity);

	virtual const float GetAutoAimAmount() { return 0.26; }
	virtual bool ShouldFlareAutoaim() { return true; }

	virtual const Vector& GetBulletSpread()
	{
		static Vector cone = VECTOR_CONE_20DEGREES;
		return cone;
	}

	virtual const Vector& GetAngularBulletSpread()
	{
		static Vector cone(22,22,22);
		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_shotgun"; }

		virtual bool IsRapidFire() { return false; }
		virtual float GetMadFiringBias() { return 1.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon

		virtual CASW_Shotgun_Pellet*  CreatePellet(Vector vecSrc, Vector newVel, CASW_Marine *pMarine);
	#else
		void OnMuzzleFlashed();
		bool ShouldUseFastReloadAnim();
		virtual const char* GetPartialReloadSound(int iPart);
		virtual bool HasMuzzleFlashSmoke() { return true; }
		virtual bool PrefersFlatAiming() const { return true; }
		virtual const char* GetTracerEffectName() { return "tracer_shotgun"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_shotgun"; }	// particle effect name
		virtual float GetLaserPointerRange( void ) { return 360; }
		virtual void ClientThink();

		float m_flEjectBrassTime;
		int m_nEjectBrassCount;
	#endif

	const Vector& ApplySpread( const Vector& vecShotDirection, const Vector &vecSpread, float bias = 1.0f);
	Vector m_vecSpreadResult;


	CNetworkVar(float, m_fSlowTime);		// marine moves slow until this moment

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SHOTGUN; }
};


#endif /* _INCLUDED_ASW_WEAPON_SHOTGUN_H */
