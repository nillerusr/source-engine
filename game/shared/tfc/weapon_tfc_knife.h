//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_KNIFE_H
#define WEAPON_TFC_KNIFE_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"
#include "weapon_tfc_crowbar.h"


#if defined( CLIENT_DLL )

	#define CTFCKnife C_TFCKnife

#endif


// ----------------------------------------------------------------------------- //
// CTFCKnife class definition.
// ----------------------------------------------------------------------------- //

class CTFCKnife : public CTFCCrowbar
{
public:
	DECLARE_CLASS( CTFCKnife, CTFCCrowbar );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCKnife();

	virtual void Precache();

	virtual TFCWeaponID GetWeaponID( void ) const;


private:
	CTFCKnife( const CTFCKnife & ) {}
};


#endif // WEAPON_TFC_KNIFE_H
