//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_knife.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


// ----------------------------------------------------------------------------- //
// CTFCKnife tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCKnife, DT_WeaponKnife )

BEGIN_NETWORK_TABLE( CTFCKnife, DT_WeaponKnife )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCKnife )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_knife, CTFCKnife );
PRECACHE_WEAPON_REGISTER( weapon_knife );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCKnife )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCKnife implementation.
// ----------------------------------------------------------------------------- //

CTFCKnife::CTFCKnife()
{
}


void CTFCKnife::Precache()
{
	BaseClass::Precache();
}


TFCWeaponID CTFCKnife::GetWeaponID( void ) const
{ 
	return WEAPON_KNIFE;
}
