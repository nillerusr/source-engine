//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Base class for hand-thrown grenades that work with the handheld shield
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combat_basegrenade.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"

LINK_ENTITY_TO_CLASS( weapon_combat_basegrenade, CWeaponCombatBaseGrenade );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatBaseGrenade, DT_WeaponCombatBaseGrenade )

BEGIN_NETWORK_TABLE( CWeaponCombatBaseGrenade, DT_WeaponCombatBaseGrenade )
#if !defined( CLIENT_DLL )
	SendPropTime( SENDINFO( m_flStartedThrowAt ) ),
#else
	RecvPropTime( RECVINFO( m_flStartedThrowAt ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatBaseGrenade  )

	DEFINE_PRED_FIELD_TOL( m_flStartedThrowAt, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCombatBaseGrenade::CWeaponCombatBaseGrenade( void )
{
	m_flStartedThrowAt = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatBaseGrenade::GetFireRate( void )
{	
	return 2.0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBaseGrenade::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
		return;

	AllowShieldPostFrame( !m_flStartedThrowAt );

	// Look for button downs
	if ( (pOwner->m_nButtons & IN_ATTACK) && GetShieldState() == SS_DOWN && !m_flStartedThrowAt && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		m_flStartedThrowAt = gpGlobals->curtime;

		SendWeaponAnim( ACT_VM_DRAW );
	}

	// Look for button ups
	if ( (pOwner->m_afButtonReleased & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && m_flStartedThrowAt )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
		PrimaryAttack();
		m_flStartedThrowAt = 0;
	}

	//  No buttons down?
	if ( !((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)) )
	{
		WeaponIdle( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBaseGrenade::PrimaryAttack( void )
{
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !ComputeEMPFireState() )
		return;

	// player "shoot" animation
	PlayAttackAnimation( ACT_VM_THROW );

	ThrowGrenade();

	// Setup for refire
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
	CheckRemoveDisguise();

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SelectLastItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBaseGrenade::ThrowGrenade( void )
{
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>( GetOwner() );
	if ( !pPlayer )
		return;

	BaseClass::WeaponSound(WPN_DOUBLE);

	// Calculate launch velocity (3 seconds for max distance)
	float flThrowTime = MIN( (gpGlobals->curtime - m_flStartedThrowAt), 3.0 );
	float flSpeed = 650 + (175 * flThrowTime);

	// If the player's crouched, roll the grenade
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		// Launch the grenade
		Vector vecForward;
		QAngle vecAngles = pPlayer->EyeAngles();
		// Throw it up just a tad
		vecAngles.x = -1;
		AngleVectors( vecAngles, &vecForward, NULL, NULL);
		Vector vecOrigin;
		VectorLerp( pPlayer->EyePosition(), pPlayer->GetAbsOrigin(), 0.25f, vecOrigin );
		vecOrigin += (vecForward * 16);
		vecForward = vecForward * flSpeed;
		CreateGrenade(vecOrigin, vecForward, pPlayer );
	}
	else
	{
		// Launch the grenade
		Vector vecForward;
		QAngle vecAngles = pPlayer->EyeAngles();
		AngleVectors( vecAngles, &vecForward, NULL, NULL);
		Vector vecOrigin = pPlayer->EyePosition();
		vecOrigin += (vecForward * 16);
		vecForward = vecForward * flSpeed;
		CreateGrenade(vecOrigin, vecForward, pPlayer );
	}

	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
}
