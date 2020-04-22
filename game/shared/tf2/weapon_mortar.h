//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_MORTAR_H
#define WEAPON_MORTAR_H
#ifdef _WIN32
#pragma once
#endif

class SmokeTrail;
class CWeaponMortar;

#include "tf_basecombatweapon.h"
#include "particle_smokegrenade.h"
#include "utllinkedlist.h"
#include "tf_obj_mortar.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponMortar : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponMortar, CBaseTFCombatWeapon );
public:
	DECLARE_SERVERCLASS();

	CWeaponMortar();

	virtual void Precache();

	// Input
	void Fire( float flPower, float flAccuracy );
	void PrimaryAttack( void );
	void SecondaryAttack( void );

	// Deployment / Pick-up
	void DeployMortar( CObjectMortar *pMortar );
	void PickupMortar( void );
	void MortarDestroyed( void );
	void MortarObjectRemoved( void );

	// Turning
	void SetYaw( float flYaw );

	// Firing
	virtual void	ItemPostFrame( void );
	virtual float	GetFireRate( void );

	// Ammo
	virtual void	SetRoundType( int iRoundType );

	virtual bool	Deploy( void );
	virtual void	WeaponIdle( void );

	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual void	AddAssociatedObject( CBaseObject *pObject );
	virtual void	RemoveAssociatedObject( CBaseObject *pObject );

	virtual bool	ComputeEMPFireState( void );

	void			MortarIsReloading( void );
	void			MortarFinishedReloading( void );

public:
	// Data
	bool			m_bCarried;
	bool			m_bMortarReloading;
	CHandle< CObjectMortar > m_hDeployedMortar;
	Vector			m_vecMortarOrigin;
	QAngle			m_vecMortarAngles;
	bool			m_bRangeUpgraded;
	bool			m_bAccuracyUpgraded;

	// Firing
	float			m_flFiringPower;
	float			m_flFiringAccuracy;
};

#endif // WEAPON_MORTAR_H
