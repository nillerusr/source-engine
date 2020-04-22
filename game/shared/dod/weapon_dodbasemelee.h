//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODBASE_MELEE_H
#define WEAPON_DODBASE_MELEE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_dodbase.h"

#if defined( CLIENT_DLL )
	#define CWeaponDODBaseMelee C_WeaponDODBaseMelee
#endif

// ----------------------------------------------------------------------------- //
// class definition.
// ----------------------------------------------------------------------------- //

class CWeaponDODBaseMelee : public CWeaponDODBase
{
public:
	DECLARE_CLASS( CWeaponDODBaseMelee, CWeaponDODBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	CWeaponDODBaseMelee();

	virtual void Spawn();
	virtual void PrimaryAttack();
	virtual void WeaponIdle();

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }
	virtual bool ShouldDrawCrosshair( void ) { return false; }
	virtual bool HasPrimaryAmmo() { return true; }
	virtual bool CanBeSelected() { return true; }

	virtual CBaseEntity *MeleeAttack( int iDamageAmount, int iDamageType, float flDmgDelay, float flAttackDelay );

	//virtual const char *GetSecondaryDeathNoticeName( void ) { return "stab"; }

public:	
	CDODWeaponInfo *m_pWeaponInfo;

private:
	CWeaponDODBaseMelee( const CWeaponDODBaseMelee & ) {}
};


#endif // WEAPON_DODBASE_MELEE_H
