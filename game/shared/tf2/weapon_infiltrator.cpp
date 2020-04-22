//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Infiltrator's Weapon
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_player.h"
#include "tf_defines.h"
#include "in_buttons.h"
#include "weapon_infiltrator.h"
#include "gamerules.h"
#include "ammodef.h"

#define INFILTRATOR_STAB_RANGE 64.0f

// Damage CVars
ConVar	weapon_infiltrator_damage( "weapon_infiltrator_damage","0", FCVAR_NONE, "Infiltrator backstab damage" );
ConVar	weapon_infiltrator_range( "weapon_infiltrator_range","0", FCVAR_NONE, "Infiltrator backstab range" );

//=====================================================================================================
// INFILTRATOR WEAPON
//=====================================================================================================
IMPLEMENT_SERVERCLASS_ST(CWeaponInfiltrator, DT_WeaponInfiltrator)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_infiltrator, CWeaponInfiltrator );
PRECACHE_WEAPON_REGISTER(weapon_infiltrator);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponInfiltrator::CWeaponInfiltrator( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponInfiltrator::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponInfiltrator::ComputeEMPFireState( void )
{
	if (IsOwnerEMPed())
	{
		// FIXME: Need a sound
		//UTIL_EmitSound( pPlayer->pev, CHAN_WEAPON, g_pszEMPGatlingFizzle, 1.0, ATTN_NORM );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponInfiltrator::Deploy( void )
{
	// Play a shing! sound
	WeaponSound( SPECIAL1 );
	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_DRAW, (char*)GetAnimPrefix() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponInfiltrator::PrimaryAttack( void )
{
	CBaseTFPlayer *pTarget = GetAssassinationTarget();
	if ( pTarget == NULL )
		return;
	
	WeaponSound( SINGLE );
	PlayAttackAnimation( ACT_VM_PRIMARYATTACK );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration( m_nSequence ) * 0.5;

	// Instant kill
	pTarget->TakeDamage( CTakeDamageInfo( this, GetOwner(), 500.0f, DMG_SLASH ) );
	// Gag until respawn
	pTarget->SetGagged( true );

	CheckRemoveDisguise();
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the hold-down healing
//-----------------------------------------------------------------------------
void CWeaponInfiltrator::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Stab 'em
	if (( pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		PrimaryAttack();
	}

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: See if there's an assassination target in front of the player.
// Output : Returns true if a target's found.
//-----------------------------------------------------------------------------
CBaseTFPlayer *CWeaponInfiltrator::GetAssassinationTarget( void )
{
	if ( !ComputeEMPFireState() )
		return NULL;

	CBaseTFPlayer *pPlayer = static_cast< CBaseTFPlayer * >( GetOwner() );
	if ( !pPlayer )
		return NULL;

	trace_t tr;
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	Vector vecOrigin = pPlayer->EyePosition();
	UTIL_TraceLine( vecOrigin, vecOrigin + INFILTRATOR_STAB_RANGE * vecForward, MASK_SHOT, pPlayer, GetCollisionGroup(), &tr );
	if ( tr.fraction == 1.0f || !tr.m_pEnt )
		return NULL;
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;
	if ( !pEntity->IsPlayer() )
		return NULL;

	// Don't target friendlies
	if ( pEntity->InSameTeam( pPlayer ) )
		return NULL;

	// See if the player is facing the right direction
	Vector fwd1, fwd2;
	AngleVectors( pPlayer->GetAbsAngles(), &fwd1, NULL, NULL );
	AngleVectors( pEntity->GetAbsAngles(), &fwd2, NULL, NULL );

	float dot = fwd1.Dot( fwd2 );
	if ( dot <= 0.0f )
		return NULL;

	return (CBaseTFPlayer*)pEntity;
}