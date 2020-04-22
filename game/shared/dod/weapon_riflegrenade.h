//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"  
#include "shake.h" 
#include "weapon_dodbasegun.h"

#if defined( CLIENT_DLL )
#define CWeaponBaseRifleGrenade C_WeaponBaseRifleGrenade
#endif

class CWeaponBaseRifleGrenade : public CWeaponDODBaseGun
{
public:
	DECLARE_CLASS( CWeaponBaseRifleGrenade, CWeaponDODBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponBaseRifleGrenade();

	virtual void Spawn();
	virtual void PrimaryAttack( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void WeaponIdle( void );

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }
	

	virtual Activity GetDrawActivity( void );
	virtual Activity GetIdleActivity( void );

	virtual bool CanDeploy( void );

	virtual const char *GetCompanionWeaponName( void );

	virtual float GetRecoil( void );

#ifndef CLIENT_DLL
	void ShootGrenade( void );
	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime );
#endif

private:
	CWeaponBaseRifleGrenade( const CWeaponBaseRifleGrenade & );
};