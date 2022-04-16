//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifdef CLIENT_DLL
	#include "c_portal_player.h"
#else
	#include "portal_player.h"
#endif

#include "weapon_portalbase.h"

#ifndef WEAPON_BASEPORTALCOMBATWEAPON_SHARED_H
#define WEAPON_BASEPORTALCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CBasePortalCombatWeapon C_BasePortalCombatWeapon
#endif

class CBasePortalCombatWeapon : public CWeaponPortalBase
{
#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	DECLARE_CLASS( CBasePortalCombatWeapon, CWeaponPortalBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBasePortalCombatWeapon();

	virtual bool	WeaponShouldBeLowered( void );

			bool	CanLower( void );
	virtual bool	Ready( void );
	virtual bool	Lower( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	WeaponIdle( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	virtual Vector	GetBulletSpread( WeaponProficiency_t proficiency );
	virtual float	GetSpreadBias( WeaponProficiency_t proficiency );

	virtual const	WeaponProficiencyInfo_t *GetProficiencyValues();
	static const	WeaponProficiencyInfo_t *GetDefaultProficiencyValues();

	virtual void	ItemHolsterFrame( void );

protected:

	bool			m_bLowered;			// Whether the viewmodel is raised or lowered
	float			m_flRaiseTime;		// If lowered, the time we should raise the viewmodel
	float			m_flHolsterTime;	// When the weapon was holstered

private:
	
	CBasePortalCombatWeapon( const CBasePortalCombatWeapon & );
};

#endif // WEAPON_BASEPORTALCOMBATWEAPON_SHARED_H
