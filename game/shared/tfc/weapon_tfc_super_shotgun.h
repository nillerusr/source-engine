//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_SUPER_SHOTGUN_H
#define WEAPON_TFC_SUPER_SHOTGUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfc_shotgun.h"


#if defined( CLIENT_DLL )

	#define CTFCSuperShotgun C_TFCSuperShotgun

#endif


// ----------------------------------------------------------------------------- //
// CTFCSuperShotgun class definition.
// ----------------------------------------------------------------------------- //

class CTFCSuperShotgun : public CTFCShotgun
{
public:
	DECLARE_CLASS( CTFCSuperShotgun, CTFCShotgun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCSuperShotgun();

	virtual TFCWeaponID GetWeaponID( void ) const;
	virtual void PrimaryAttack();


// Overrideables.
public:

private:
	
	CTFCSuperShotgun( const CTFCSuperShotgun & ) {}

};


#endif // WEAPON_TFC_SUPER_SHOTGUN_H
