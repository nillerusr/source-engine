//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODBASE_GUN_H
#define WEAPON_DODBASE_GUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_dodbase.h"

// This is the base class for pistols and rifles.
#if defined( CLIENT_DLL )

	//#include "particles_simple.h"
	//#include "particles_localspace.h"
	#include "fx.h"
	#include "fx_dod_muzzleflash.h"

	#define CWeaponDODBaseGun C_WeaponDODBaseGun

#else
#endif

class CWeaponDODBaseGun : public CWeaponDODBase
{
public:
	
	DECLARE_CLASS( CWeaponDODBaseGun, CWeaponDODBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponDODBaseGun();

	virtual void Spawn();
	virtual void Precache();
	virtual void PrimaryAttack();
	virtual bool Reload();

	// Derived classes must call this from inside their Spawn() function instead of 
	// just chaining the Spawn() call down.
	void DODBaseGunSpawn( void );
	
	// Derived classes call this to fire a bullet.
	bool DODBaseGunFire( void );

	// Usually plays the shot sound. Guns with silencers can play different sounds.
	virtual void DoFireEffects();
	virtual float GetFireDelay( void );

	virtual float GetWeaponAccuracy( float flPlayerSpeed );

	//Pure animation calls - inheriting classes can override with specific
	//logic, eg if the idle changes depending on the gun being empty or not
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetReloadActivity( void );
	virtual Activity GetDrawActivity( void );

	virtual bool CanDrop( void ) { return m_pWeaponInfo->m_bCanDrop; }
	
	void SetZoomed( bool bZoomed ) { m_bZoomed = bZoomed; }
	bool IsSniperZoomed( void ) { return m_bZoomed; }
	CNetworkVar( bool, m_bZoomed );

protected:
	CDODWeaponInfo *m_pWeaponInfo;

private:
	CWeaponDODBaseGun( const CWeaponDODBaseGun & );

#ifndef CLIENT_DLL

	DECLARE_DATADESC();

#endif
};


#endif // WEAPON_DODBASE_GUN_H
