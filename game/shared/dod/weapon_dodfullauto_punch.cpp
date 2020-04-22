//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "weapon_dodfullauto_punch.h"
#include "in_buttons.h"
#include "dod_shareddefs.h"

#ifndef CLIENT_DLL
#include "dod_player.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( DODFullAutoPunchWeapon, DT_FullAutoPunchWeapon )

BEGIN_NETWORK_TABLE( CDODFullAutoPunchWeapon, DT_FullAutoPunchWeapon )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CDODFullAutoPunchWeapon )
END_PREDICTION_DATA()
#endif

void CDODFullAutoPunchWeapon::Spawn( void )
{
	m_iAltFireHint = HINT_USE_MELEE;

	BaseClass::Spawn();
}

void CDODFullAutoPunchWeapon::SecondaryAttack( void )
{
	if ( m_bInReload )
	{
		m_bInReload = false;
		GetPlayerOwner()->m_flNextAttack = gpGlobals->curtime;
	}
	else if ( GetPlayerOwner()->m_flNextAttack > gpGlobals->curtime )
	{
		return;
	}

	Punch();

	// start calling ItemPostFrame
	GetPlayerOwner()->m_flNextAttack = gpGlobals->curtime;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack;

#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();
	if ( pPlayer )
	{
		pPlayer->RemoveHintTimer( m_iAltFireHint );
	}
#endif
}

bool CDODFullAutoPunchWeapon::Reload( void )
{
	bool bSuccess = BaseClass::Reload();

	if ( bSuccess )
	{
		m_flNextSecondaryAttack = gpGlobals->curtime;
	}

	return bSuccess;
}

void CDODFullAutoPunchWeapon::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();

	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer && (pPlayer->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		SecondaryAttack();
		pPlayer->m_nButtons &= ~IN_ATTACK2;
	}
}
