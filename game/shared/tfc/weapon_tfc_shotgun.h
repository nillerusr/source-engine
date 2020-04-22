//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_SHOTGUN_H
#define WEAPON_TFC_SHOTGUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"


#if defined( CLIENT_DLL )

	#define CTFCShotgun C_TFCShotgun

#endif


#define VECTOR_CONE_TF_SHOTGUN	Vector( 0.04, 0.04, 0.00 )


// ----------------------------------------------------------------------------- //
// CTFCShotgun class definition.
// ----------------------------------------------------------------------------- //

class CTFCShotgun : public CWeaponTFCBase
{
public:
	DECLARE_CLASS( CTFCShotgun, CWeaponTFCBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCShotgun();

	virtual TFCWeaponID GetWeaponID( void ) const;

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Reload();
	virtual void WeaponIdle();


protected:

	int m_iShellsReloaded;
	float m_flPumpTime;
	float m_fReloadTime;
	int m_fInSpecialReload;
	float m_flNextReload;


private:
	
	CTFCShotgun( const CTFCShotgun & ) {}
};


#endif // WEAPON_TFC_SHOTGUN_H
