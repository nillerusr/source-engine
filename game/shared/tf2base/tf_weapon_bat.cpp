//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_bat.h"
#include "decals.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#endif

//=============================================================================
//
// Weapon Bat tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFBat, DT_TFWeaponBat )

BEGIN_NETWORK_TABLE( CTFBat, DT_TFWeaponBat )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFBat )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_bat, CTFBat );
PRECACHE_WEAPON_REGISTER( tf_weapon_bat );

//=============================================================================
//
// Weapon Bat functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFBat::CTFBat()
{
}

