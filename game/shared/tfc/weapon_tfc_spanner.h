//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_SPANNER_H
#define WEAPON_TFC_SPANNER_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"
#include "weapon_tfc_crowbar.h"


#if defined( CLIENT_DLL )

	#define CTFCSpanner C_TFCSpanner

#endif


// ----------------------------------------------------------------------------- //
// CTFCSpanner class definition.
// ----------------------------------------------------------------------------- //

class CTFCSpanner : public CTFCCrowbar
{
public:
	DECLARE_CLASS( CTFCSpanner, CTFCCrowbar );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif


public:
	
	CTFCSpanner();

	virtual void Precache();
	virtual TFCWeaponID GetWeaponID( void ) const;


// CTFCCrowbar overrides.
public:

#ifdef GAME_DLL
	virtual void AxeHit( CBaseEntity *pHit, bool bFirstSwing, trace_t &tr, float *flDamage, bool *bDoEffects );
#endif


private:
	
	CTFCSpanner( const CTFCSpanner & ) {}
};


#endif // WEAPON_TFC_SPANNER_H
