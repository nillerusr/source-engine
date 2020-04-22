//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_INFILTRATOR_H
#define WEAPON_INFILTRATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_basecombatweapon.h"

//-----------------------------------------------------------------------------
// Purpose: Infiltrator's weapon
//-----------------------------------------------------------------------------
class CWeaponInfiltrator : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponInfiltrator, CBaseTFCombatWeapon );
public:
	DECLARE_SERVERCLASS();

	CWeaponInfiltrator();

	virtual void	Precache( void );
	virtual void	PrimaryAttack( void );
	virtual bool	ComputeEMPFireState( void );
	virtual bool	Deploy( void );
	virtual void	ItemPostFrame( void );

	CBaseTFPlayer	*GetAssassinationTarget( void );
};

#endif // WEAPON_INFILTRATOR_H
