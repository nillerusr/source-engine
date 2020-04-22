//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "weapon_combatshield.h"
#include "weapon_gas_can.h"


LINK_ENTITY_TO_CLASS( weapon_gas_can, CWeaponGasCan );

PRECACHE_WEAPON_REGISTER( weapon_flame_thrower );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGasCan, DT_WeaponGasCan )

BEGIN_NETWORK_TABLE( CWeaponGasCan, DT_WeaponGasCan )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGasCan )
END_PREDICTION_DATA()


// ------------------------------------------------------------------------------------------------ //
// CWeaponGasCan implementation.
// ------------------------------------------------------------------------------------------------ //

CWeaponGasCan::CWeaponGasCan() : CWeaponFlameThrower( true )
{
}


