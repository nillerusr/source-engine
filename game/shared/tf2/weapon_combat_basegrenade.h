//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_COMBAT_BASEGRENADE_H
#define WEAPON_COMBAT_BASEGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"
#include "basegrenade_shared.h"

#if defined( CLIENT_DLL )
#define CWeaponCombatBaseGrenade C_WeaponCombatBaseGrenade
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombatBaseGrenade : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatBaseGrenade, CWeaponCombatUsedWithShieldBase );
public:
	CWeaponCombatBaseGrenade();

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float	GetFireRate( void );
	virtual void	ThrowGrenade( void );

	// Custom grenade types
	virtual CBaseGrenade *CreateGrenade( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner ) { return NULL; }

	/*
	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
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
	*/

public:
	CNetworkVar( float, m_flStartedThrowAt );

private:														
	CWeaponCombatBaseGrenade( const CWeaponCombatBaseGrenade & );						
};

#endif // WEAPON_COMBAT_BASEGRENADE_H
