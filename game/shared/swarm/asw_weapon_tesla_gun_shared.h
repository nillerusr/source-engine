#ifndef _INCLUDED_ASW_WEAPON_TESLA_GUN_SHARED_H
#define _INCLUDED_ASW_WEAPON_TESLA_GUN_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// Mining Laser
//   "Weapon" that causes heat damage at short range.  Can be used to blast through certain kinds of rocks.

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon_Tesla_Gun C_ASW_Weapon_Tesla_Gun
#else
#include "asw_weapon.h"
#endif

enum ASW_Weapon_TeslaGunFireState_t { 
	ASW_TG_FIRE_OFF, 
	ASW_TG_FIRE_DISCHARGE, 
	ASW_TG_FIRE_ATTACHED,
	ASW_TG_NUM_FIRE_STATES
};

class CASW_Weapon_Tesla_Gun : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Tesla_Gun, CASW_Weapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

    CASW_Weapon_Tesla_Gun(void);

	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack() { PrimaryAttack(); }
	virtual bool SecondaryAttackUsesPrimaryAmmo() { return true; }
    virtual void    Precache( void );

	virtual bool Reload( void );

	virtual bool DestroyOnButtonRelease() { return true; }  // whether this item should destroy on release of the fire button or not

#ifdef CLIENT_DLL
	virtual void ClientThink( void );
	virtual const char* GetPartialReloadSound( int iPart );
	virtual void UpdateEffects();	
	virtual float GetLaserPointerRange( void ) { return 240; }// Give a chance for non-local weapons to update their effects on the client
#else
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual void GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 );
	float m_fLastForcedFireTime;
#endif

	virtual const float GetAutoAimAmount() { return AUTOAIM_2DEGREES; };
	virtual bool ShouldFlareAutoaim() { return true; }

	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void	Drop( const Vector &vecVelocity );
	virtual bool HasAmmo();
	virtual bool ShouldMarineMoveSlow();

	virtual float GetWeaponDamage( void );
	virtual float GetWeaponRange( void ) { return 240; }

	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_TESLA_GUN; }

private:
	void	Attack( void );
	void	EndAttack( void );
	void	Fire( const Vector &vecOrigSrc, const Vector &vecDir );

	void	SetFiringState( ASW_Weapon_TeslaGunFireState_t state );
	
	void	ShockEntity();
#ifdef GAME_DLL
	void	DoArcingShock( float flBaseDamage, CBaseEntity *pLastShocked );
#endif

	bool	ShockAttach( CBaseEntity *pEntity );
	void	ShockDetach();

#ifdef CLIENT_DLL
	CUtlReference<CNewParticleEffect>	m_pDischargeEffect;
#endif

	CNetworkVar(unsigned char, m_FireState);	// one of the ASW_Weapon_TeslaGunFireState_t enums
	CNetworkVar(float, m_fSlowTime);			// marine moves slow until this moment
	CNetworkVar(EHANDLE, m_hShockEntity);		// entity we're attached too and shocking
	CNetworkVar(Vector, m_vecShockPos);			// place we're zapping

	float	m_flLastDischargeTime;
	trace_t m_AttackTrace;
	float	m_flLastShockTime;
};

#endif // _INCLUDED_ASW_WEAPON_TESLA_GUN_SHARED_H