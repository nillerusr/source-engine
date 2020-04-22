//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "fx_dod_shared.h"
#include "weapon_dodfullauto.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODFullAutoWeapon, DT_FullAutoWeapon )

BEGIN_NETWORK_TABLE( CDODFullAutoWeapon, DT_FullAutoWeapon )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CDODFullAutoWeapon )
END_PREDICTION_DATA()


CDODFullAutoWeapon::CDODFullAutoWeapon()
{
}

void CDODFullAutoWeapon::Spawn()
{
	BaseClass::Spawn();
}

void CDODFullAutoWeapon::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();
}