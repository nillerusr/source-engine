//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_MEDIKIT_H
#define WEAPON_TFC_MEDIKIT_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"
#include "weapon_tfc_crowbar.h"


#if defined( CLIENT_DLL )

	#define CTFCMedikit C_TFCMedikit

#endif


// ----------------------------------------------------------------------------- //
// CTFCMedikit class definition.
// ----------------------------------------------------------------------------- //

class CTFCMedikit : public CTFCCrowbar
{
public:
	DECLARE_CLASS( CTFCMedikit, CTFCCrowbar );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCMedikit();

	virtual void Precache();

	virtual TFCWeaponID GetWeaponID( void ) const;

#ifdef CLIENT_DLL
#else

	virtual void AxeHit( CBaseEntity *pHit, bool bFirstSwing, trace_t &tr, float *flDamage, bool *bDoEffects );

#endif

private:
	CTFCMedikit( const CTFCMedikit & ) {}
};


#endif // WEAPON_TFC_MEDIKIT_H
