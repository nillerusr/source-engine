//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_ROCKETPACK_H
#define TF_WEAPON_ROCKETPACK_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_melee.h"
#include "tf_shareddefs.h"
#include "tf_viewmodel.h"

#ifdef CLIENT_DLL
#define CTFRocketPack C_TFRocketPack
#endif

class CTFRocketPack : public CTFWeaponBaseMelee
{
public:
	DECLARE_CLASS( CTFRocketPack, CTFWeaponBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFRocketPack();

	virtual void		PreCache( void );
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_ROCKETPACK; }
	virtual void		PrimaryAttack( void );
	virtual bool		Deploy( void );
	virtual bool		Holster( CBaseCombatWeapon *pSwitchingTo );

#ifdef GAME_DLL
	bool				CanFire( void );
#endif // GAME_DLL

	virtual float		InternalGetEffectBarRechargeTime( void ) { return 5.0; }
	virtual int			GetEffectBarAmmo( void ) { return TF_AMMO_GRENADES1; }
	float				GetProgress( void ) { return GetEffectBarProgress(); }
	const char*			GetEffectLabelText( void ) { return "FUEL"; }
	float				GetEffectMeterTime( void );

private:
	CTFRocketPack( const CTFRocketPack & ) {}

	float				m_flRefireTime;
};
#endif // TF_WEAPON_ROCKETPACK_H
