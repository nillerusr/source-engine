//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_COMBAT_USEDWITHSHIELDBASE_H
#define WEAPON_COMBAT_USEDWITHSHIELDBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfcombatweapon_shared.h"

class CBasePlayer;

#if defined( CLIENT_DLL )
#define CWeaponCombatUsedWithShieldBase C_WeaponCombatUsedWithShieldBase
#endif

class CWeaponCombatUsedWithShieldBase : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponCombatUsedWithShieldBase, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatUsedWithShieldBase( void ) {}

	virtual bool	CanDeploy( void );
	virtual int		UpdateClientData( CBasePlayer *pPlayer );
	virtual bool	SupportsTwoHanded( void ) { return true; };
	void			AllowShieldPostFrame( bool allow );
	virtual int		GetShieldState( void );

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

private:														
	CWeaponCombatUsedWithShieldBase( const CWeaponCombatUsedWithShieldBase & );						

};
#endif // WEAPON_COMBAT_USEDWITHSHIELDBASE_H
