//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODFIRESELECT_H
#define WEAPON_DODFIRESELECT_H

#include "cbase.h"
#include "weapon_dodbasegun.h"

#if defined( CLIENT_DLL )
#define CDODFireSelectWeapon C_DODFireSelectWeapon

#include "c_dod_player.h"
#endif

class CDODFireSelectWeapon : public CWeaponDODBaseGun
{
public:
	DECLARE_CLASS( CDODFireSelectWeapon, CWeaponDODBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CDODFireSelectWeapon();

	virtual void Spawn();
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual float GetWeaponAccuracy( float flPlayerSpeed );
	virtual float GetFireDelay( void );
	virtual void Drop( const Vector &vecVelocity );

	bool IsSemiAuto( void );

	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetReloadActivity( void );
	virtual Activity GetDrawActivity( void );

#ifdef CLIENT_DLL
	virtual Vector GetDesiredViewModelOffset( C_DODPlayer *pOwner );

	void ResetViewModelAnimDir( void )
	{
		m_bAnimToSemiAuto = true;
		m_flPosChangeTimer = 0;
	}

	virtual void OnWeaponDropped( void )
	{
		ResetViewModelAnimDir();

		m_bSemiAuto = false;

		BaseClass::OnWeaponDropped();
	}
#endif

private:
	CNetworkVar( bool, m_bSemiAuto );

#ifdef CLIENT_DLL
	bool m_bAnimToSemiAuto;
	float m_flPosChangeTimer;
#endif

private:
	CDODFireSelectWeapon( const CDODFireSelectWeapon & );
};

#endif //WEAPON_DODFIRESELECT_H