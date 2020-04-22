//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_DODBASERPG_H
#define WEAPON_DODBASERPG_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_dodbase.h"
 

#if defined( CLIENT_DLL )

	#define CDODBaseRocketWeapon C_DODBaseRocketWeapon

#endif

//-----------------------------------------------------------------------------
// RPG
//-----------------------------------------------------------------------------
class CDODBaseRocketWeapon : public CWeaponDODBase
{
public:
	DECLARE_CLASS( CDODBaseRocketWeapon, CWeaponDODBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CDODBaseRocketWeapon();

	virtual void Spawn();
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Deploy();
	virtual bool CanHolster();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool Reload();
	virtual void WeaponIdle();
	virtual void Drop( const Vector &vecVelocity );

	virtual bool CanDrop( void ) { return ( IsDeployed() == false ); }

	void		DoFireEffects();

	void		Precache( void );

	void		Raise();
	bool		Lower();

	virtual Activity GetDrawActivity( void );
	virtual Activity GetIdleActivity( void );
	virtual Activity GetLowerActivity( void );
	virtual Activity GetRaiseActivity( void );

	virtual	void FireRocket( void );

	inline bool IsDeployed() { return m_bDeployed; }
	inline void SetDeployed( bool bDeployed ) { m_bDeployed = bDeployed; }
	
	bool		ShouldPlayerBeSlow( void );

	virtual bool ShouldAutoEjectBrass( void ) { return false; }

#ifdef CLIENT_DLL
	virtual void OverrideMouseInput( float *x, float *y );
#endif

	virtual float GetRecoil( void ) { return 10.0f; }

protected:
	CNetworkVar( bool, m_bDeployed );

	CDODWeaponInfo *m_pWeaponInfo;

private:
	CDODBaseRocketWeapon( const CDODBaseRocketWeapon & );
	
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
};

#endif // WEAPON_DODBASERPG_H