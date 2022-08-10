//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//=============================================================================
#ifndef TF_WEAPON_REVOLVER_H
#define TF_WEAPON_REVOLVER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFRevolver C_TFRevolver
#endif

//=============================================================================
//
// TF Weapon Revolver.
//
class CTFRevolver : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CTFRevolver, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFRevolver() {}
	~CTFRevolver() {}

	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_REVOLVER; }

	virtual bool DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

private:

	CTFRevolver( const CTFRevolver & ) {}
};

#endif // TF_WEAPON_REVOLVER_H