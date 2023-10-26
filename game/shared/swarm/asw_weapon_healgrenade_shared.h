#ifndef _ASW_WEAPON_HEALGRENADE_H
#define _ASW_WEAPON_HEALGRENADE_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#include "c_asw_aoegrenade_projectile.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_HealGrenade C_ASW_Weapon_HealGrenade
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "asw_aoegrenade_projectile.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

#ifdef CLIENT_DLL
class C_ASW_HealGrenade_Projectile : public C_ASW_AOEGrenade_Projectile
{
public:
	DECLARE_CLASS( C_ASW_HealGrenade_Projectile, C_ASW_AOEGrenade_Projectile );
	DECLARE_CLIENTCLASS();

	virtual Color GetGrenadeColor( void );
	virtual const char* GetIdleLoopSoundName( void ) { return "ASW_MedGrenade.ActiveLoop"; }
	virtual const char* GetLoopSoundName( void ) { return "ASW_MedGrenade.BuffLoop"; }
	virtual const char* GetStartSoundName( void ) { return "ASW_MedGrenade.StartBuff"; }
	virtual const char* GetActivateSoundName( void ) { return "ASW_MedGrenade.GrenadeActivate"; }
	virtual const char* GetPingEffectName( void ) { return "medgrenade_pulse"; }
	virtual const char* GetArcEffectName( void ) { return "medgrenade_attach_arc"; }
	virtual const char* GetArcAttachmentName( void ) { return "zipline"; }
	virtual bool ShouldSpawnSphere( void ) { return true; }
	virtual float GetSphereScale( void ) { return 0.9f; }
	virtual int GetSphereSkin( void ) { return 1; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_HEALGRENADE_PROJECTILE; }
};
#else

DECLARE_AUTO_LIST( IHealGrenadeAutoList );

class CASW_HealGrenade_Projectile : public CASW_AOEGrenade_Projectile, public IHealGrenadeAutoList
{
	DECLARE_CLASS( CASW_HealGrenade_Projectile, CASW_AOEGrenade_Projectile );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:

	CASW_HealGrenade_Projectile();

	void Precache( void );

	static CASW_AOEGrenade_Projectile* Grenade_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity,
		const AngularImpulse &angVelocity, CBaseEntity *pOwner,
		float flHealPerSecond, float flInfestationCureAmount, float flRadius, float flDuration, float flTotalHealAmount );

	virtual float GetInfestationCureAmount( void );

	virtual float GetGrenadeGravity( void );

	virtual void OnBurnout( void );

	virtual bool ShouldTouchEntity( CBaseEntity *pEntity );

	virtual void DoAOE( CBaseEntity *pEntity );

	virtual float GetDoAOEDelayTime( void ) { return 1.0f; }
	
	IMPLEMENT_AUTO_LIST_GET();		// IHealGrenadeAutoList

	CUtlVector< EHANDLE >	m_hHealedEntities;

	float m_flHealPerSecond;
	float m_flInfestationCureAmount;
	float m_flHealAmountLeft;
	float m_flHealAmountTotal;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_HEALGRENADE_PROJECTILE; }
};

#endif

class CASW_Weapon_HealGrenade : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_HealGrenade, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	virtual void Precache();
	virtual bool ShouldMarineMoveSlow() { return false; }	// firing doesn't slow the marine down
	virtual bool Reload() { return false; }
	virtual void PrimaryAttack();
	float	GetFireRate( void ) { return 3.0f; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		cone = Vector(0,0,0);
		return cone;
	}

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual CASW_AOEGrenade_Projectile* CreateProjectile( const Vector &vecSrc, const QAngle &angles, const Vector &vecVel, 
							const AngularImpulse &rotSpeed, CBaseEntity *pOwner );

		virtual const char* GetPickupClass() { return "asw_pickup_heal_grenade"; }	
	#else
	
	#endif

	virtual bool IsOffensiveWeapon() { return false; }
	virtual bool OffhandActivate();

	virtual float GetRefireTime( void );

	float	m_flSoonestPrimaryAttack;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_HEALGRENADE; }
};


#endif /* _ASW_WEAPON_HEALGRENADE_H */
