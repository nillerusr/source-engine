//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_MINIGUN_H
#define WEAPON_TFC_MINIGUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"


#if defined( CLIENT_DLL )

	#define CTFCMinigun C_TFCMinigun

#endif


enum MinigunState_t
{
// assault cannon firing states
	AC_STATE_IDLE=0,
	AC_STATE_STARTFIRING,
	AC_STATE_FIRING,
	AC_STATE_SPINNING
};

// ----------------------------------------------------------------------------- //
// CTFCMinigun class definition.
// ----------------------------------------------------------------------------- //

class CTFCMinigun : public CWeaponTFCBase
{
public:
	DECLARE_CLASS( CTFCMinigun, CWeaponTFCBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCMinigun();

	// We say yes to this so the weapon system lets us switch to it.
	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	virtual void Precache();
	virtual void PrimaryAttack();
	virtual void WeaponIdle();
	virtual bool Deploy();
	virtual bool SendWeaponAnim( int iActivity );
	virtual void HandleFireOnEmpty();
	
	virtual TFCWeaponID GetWeaponID( void ) const;


// Overrideables.
public:

private:
	
	CTFCMinigun( const CTFCMinigun & ) {}

	void WindUp();
	void Spin();
	void Fire();
	void StartSpin();
	void WindDown( bool bFromHolster );


private:

	MinigunState_t m_iWeaponState;
};


#endif // WEAPON_TFC_MINIGUN_H
