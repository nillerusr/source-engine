//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_BASECOMBATOBJECT_H
#define WEAPON_BASECOMBATOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfcombatweapon_shared.h"

class CBaseTFPlayer;

#if defined( CLIENT_DLL )

#define CWeaponBaseCombatObject C_WeaponBaseCombatObject

#endif

//-----------------------------------------------------------------------------
// Purpose: Base class for combat object weapons
//-----------------------------------------------------------------------------
class CWeaponBaseCombatObject : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponBaseCombatObject, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponBaseCombatObject();

	virtual void	PrimaryAttack( void );
	virtual bool	GetPlacePosition( CBaseTFPlayer *pBuilder, Vector *vecPlaceOrigin, QAngle *angPlaceAngles );
	virtual void	PlaceCombatObject( CBaseTFPlayer *pBuilder, Vector vecOrigin, QAngle angles );
	virtual float	GetFireRate( void );

protected:
	char	*m_szObjectName;
	Vector	m_vecBuildMins;
	Vector	m_vecBuildMaxs;

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
	CWeaponBaseCombatObject( const CWeaponBaseCombatObject & );						
};

#endif // WEAPON_BASECOMBATOBJECT_H
