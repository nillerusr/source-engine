#ifndef _INCLUDED_ASW_WEAPON_RAILGUN_H
#define _INCLUDED_ASW_WEAPON_RAILGUN_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Railgun C_ASW_Weapon_Railgun
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_Railgun : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Railgun, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Railgun();
	virtual ~CASW_Weapon_Railgun();
	void Precache();

	virtual bool SupportsBayonet();
	virtual void GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool &bOldReload, bool& bOldAttack1 );
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual void CreateRailgunBeam( const Vector &vecStartPoint, const Vector &vecEndPoint );
	virtual const char* GetUTracerType() { return "ASWUTracerRG"; }

	float	GetFireRate( void );
	virtual const Vector& GetBulletSpread( void ) { 	return VECTOR_CONE_PRECALCULATED; 	}
	virtual const float GetAutoAimAmount() { return 0.24f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	virtual bool ShouldMarineMoveSlow();
	virtual float GetMovementScale();
	virtual int AmmoClickPoint() { return 0; }

	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual bool Reload();
	virtual float GetWeaponDamage();
	virtual int ASW_SelectWeaponActivity(int idealActivity);

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		virtual const char* GetPickupClass() { return "asw_pickup_railgun"; }
		virtual bool IsRapidFire() { return false; }
		virtual float GetMadFiringBias() { return 2.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
				// NOTE: Railgun isn't really meant to do killing sprees, since it fires so slowly
	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
		virtual const char* GetTracerEffectName() { return "tracer_railgun"; }	// particle effect name
		virtual const char* GetMuzzleEffectName() { return "muzzle_railgun"; }	// particle effect name
	#endif

	// aiming grenades at the ground
	virtual bool SupportsGroundShooting() { return false; }

	CNetworkVar(float, m_fSlowTime);		// marine moves slow until this moment
	CNetworkVar(int, m_iRailBurst);

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_RAILGUN; }
};

#ifndef CLIENT_DLL
class CTraceFilterRG : public CTraceFilterEntitiesOnly
{
public:
	DECLARE_CLASS_NOBASE( CTraceFilterRG );
	
	CTraceFilterRG( const IHandleEntity *passentity, int collisionGroup, CTakeDamageInfo *dmgInfo, bool bFFDamage )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_dmgInfo(dmgInfo), m_pHit(NULL), m_bFFDamage(bFFDamage)
	{
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CTakeDamageInfo		*m_dmgInfo;
	CBaseEntity			*m_pHit;
	bool				m_bFFDamage;
};
#endif

#endif /* _INCLUDED_ASW_WEAPON_RAILGUN_H */
