//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_SHIELD_H
#define WEAPON_SHIELD_H
#ifdef _WIN32
#pragma once
#endif

//#include "tf_player.h"
#include "basetfcombatweapon_shared.h"
#include "tf_shieldshared.h"


#if defined( CLIENT_DLL )
#define CWeaponShield C_WeaponShield
#define CShield C_Shield
#endif

class CShield;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponShield : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponShield, CBaseTFCombatWeapon );

public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponShield();
	
	virtual void UpdateOnRemove( void );

	// Firing
	virtual void	ItemPostFrame( void );
	virtual float	GetFireRate( void );
	virtual float	GetDamage( float flDistance, int iLocation );

	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	WeaponIdle( void );
	virtual bool	VisibleInWeaponSelection( void ) { return false; }

	// All predicted weapons need to implement and return true
	virtual bool IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif

private:
	CWeaponShield( const CWeaponShield & );

	// Lock the projected shield
	void SetShieldPositionLocked( bool bLocked );

	// Input
	void PrimaryAttack( void );

private:
	// Data
	CNetworkVar( CHandle<CShield>, m_hDeployedShield );
	bool m_bShieldPositionLocked;
	bool m_bIsDeployed;
};

#endif // WEAPON_SHIELD_H
