//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "weapon_dodsemiauto.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODSemiAutoWeapon, DT_SemiAutoWeapon )

BEGIN_NETWORK_TABLE( CDODSemiAutoWeapon, DT_SemiAutoWeapon )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CDODSemiAutoWeapon )
END_PREDICTION_DATA()
#endif

CDODSemiAutoWeapon::CDODSemiAutoWeapon()
{
}

void CDODSemiAutoWeapon::Spawn()
{
	BaseClass::Spawn();
}

void CDODSemiAutoWeapon::PrimaryAttack( void )
{
	//Don't attack more than once on the same button press.
	//m_bInAttack is set to false when the attack button is released

	m_bInAttack = true;

	BaseClass::PrimaryAttack();
}