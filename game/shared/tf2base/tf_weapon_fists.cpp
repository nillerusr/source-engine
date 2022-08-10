//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_fists.h"
#include "decals.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#endif

//=============================================================================
//
// Weapon Fists tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFFists, DT_TFWeaponFists )

BEGIN_NETWORK_TABLE( CTFFists, DT_TFWeaponFists )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFFists )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_fists, CTFFists );
PRECACHE_WEAPON_REGISTER( tf_weapon_fists );

//=============================================================================
//
// Weapon Fists functions.
//


// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFFists::PrimaryAttack()
{
	if ( !CanAttack() )
		return;

	// Set the weapon usage mode - primary, secondary.
	// reversed for 360 because the primary attack is on the right side of the controller
	if ( IsX360() )
	{
		m_iWeaponMode = TF_WEAPON_SECONDARY_MODE;
	}
	else
	{
		m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	}

	Punch();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFFists::SecondaryAttack()
{
	if ( !CanAttack() )
		return;

	// Set the weapon usage mode - primary, secondary.
	if ( IsX360() )
	{
		m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	}
	else
	{
		m_iWeaponMode = TF_WEAPON_SECONDARY_MODE;
	}

	Punch();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFFists::Punch( void )
{
	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	// Swing the weapon.
	Swing( pPlayer );

	m_flNextSecondaryAttack = m_flNextPrimaryAttack;

#if !defined( CLIENT_DLL ) 
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACritical() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Allow melee weapons to send different anim events
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFFists::SendPlayerAnimEvent( CTFPlayer *pPlayer )
{
	if ( IsCurrentAttackACrit() )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_SECONDARY );
	}
	else
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFists::DoViewModelAnimation( void )
{
	Activity act;

	if ( IsCurrentAttackACritical() )
	{
		act = ACT_VM_SWINGHARD;
	}
	else
	{
		act = ( m_iWeaponMode == TF_WEAPON_PRIMARY_MODE ) ? ACT_VM_HITLEFT : ACT_VM_HITRIGHT;
	}

	SendWeaponAnim( act );
}