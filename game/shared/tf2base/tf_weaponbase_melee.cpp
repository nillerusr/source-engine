//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weaponbase_melee.h"
#include "effect_dispatch_data.h"
#include "tf_gamerules.h"

// Server specific.
#if !defined( CLIENT_DLL )
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
// Client specific.
#else
#include "c_tf_player.h"
#endif

//=============================================================================
//
// TFWeaponBase Melee tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponBaseMelee, DT_TFWeaponBaseMelee )

BEGIN_NETWORK_TABLE( CTFWeaponBaseMelee, DT_TFWeaponBaseMelee )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponBaseMelee )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weaponbase_melee, CTFWeaponBaseMelee );

// Server specific.
#if !defined( CLIENT_DLL ) 
BEGIN_DATADESC( CTFWeaponBaseMelee )
DEFINE_FUNCTION( Smack )
END_DATADESC()
#endif

#ifndef CLIENT_DLL
ConVar tf_meleeattackforcescale( "tf_meleeattackforcescale", "80.0", FCVAR_CHEAT | FCVAR_GAMEDLL | FCVAR_DEVELOPMENTONLY );
#endif

ConVar tf_weapon_criticals_melee( "tf_weapon_criticals_melee", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Controls random crits for melee weapons.\n0 - Melee weapons do not randomly crit. \n1 - Melee weapons can randomly crit only if tf_weapon_criticals is also enabled. \n2 - Melee weapons can always randomly crit regardless of the tf_weapon_criticals setting.", true, 0, true, 2 );
extern ConVar tf_weapon_criticals;

//=============================================================================
//
// TFWeaponBase Melee functions.
//

// -----------------------------------------------------------------------------
// Purpose: Constructor.
// -----------------------------------------------------------------------------
CTFWeaponBaseMelee::CTFWeaponBaseMelee()
{
	WeaponReset();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	m_flSmackTime = -1.0f;
	m_bConnected = false;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Precache()
{
	BaseClass::Precache();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Spawn()
{
	Precache();

	// Get the weapon information.
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );
	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );
	CTFWeaponInfo *pWeaponInfo = dynamic_cast< CTFWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	Assert( pWeaponInfo && "Failed to get CTFWeaponInfo in melee weapon spawn" );
	m_pWeaponInfo = pWeaponInfo;
	Assert( m_pWeaponInfo );

	// No ammo.
	m_iClip1 = -1;

	BaseClass::Spawn();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CTFWeaponBaseMelee::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_flSmackTime = -1.0f;
	if ( GetPlayerOwner() )
	{
		GetPlayerOwner()->m_flNextAttack = gpGlobals->curtime + 0.5;
	}
	return BaseClass::Holster( pSwitchingTo );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::PrimaryAttack()
{
	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !CanAttack() )
		return;

	// Set the weapon usage mode - primary, secondary.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	m_bConnected = false;

	// Swing the weapon.
	Swing( pPlayer );

#if !defined( CLIENT_DLL ) 
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACritical() );
#endif
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::SecondaryAttack()
{
	// semi-auto behaviour
	if ( m_bInAttack2 )
		return;

	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	pPlayer->DoClassSpecialSkill();

	m_bInAttack2 = true;

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Swing( CTFPlayer *pPlayer )
{
	CalcIsAttackCritical();

	// Play the melee swing and miss (whoosh) always.
	SendPlayerAnimEvent( pPlayer );

	DoViewModelAnimation();

	// Set next attack times.
	m_flNextPrimaryAttack = gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;

	SetWeaponIdleTime( m_flNextPrimaryAttack + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeIdleEmpty );
	
	if ( IsCurrentAttackACrit() )
	{
		WeaponSound( BURST );
	}
	else
	{
		WeaponSound( MELEE_MISS );
	}

	m_flSmackTime = gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flSmackDelay;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::DoViewModelAnimation( void )
{
	Activity act = ( m_iWeaponMode == TF_WEAPON_PRIMARY_MODE ) ? ACT_VM_HITCENTER : ACT_VM_SWINGHARD;
	SendWeaponAnim( act );
}

//-----------------------------------------------------------------------------
// Purpose: Allow melee weapons to send different anim events
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::SendPlayerAnimEvent( CTFPlayer *pPlayer )
{
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::ItemPostFrame()
{
	// Check for smack.
	if ( m_flSmackTime > 0.0f && gpGlobals->curtime > m_flSmackTime )
	{
		Smack();
		m_flSmackTime = -1.0f;
	}

	BaseClass::ItemPostFrame();
}

bool CTFWeaponBaseMelee::DoSwingTrace( trace_t &trace )
{
	// Setup a volume for the melee weapon to be swung - approx size, so all melee behave the same.
	static Vector vecSwingMins( -18, -18, -18 );
	static Vector vecSwingMaxs( 18, 18, 18 );

	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	// Setup the swing range.
	Vector vecForward; 
	AngleVectors( pPlayer->EyeAngles(), &vecForward );
	Vector vecSwingStart = pPlayer->Weapon_ShootPosition();
	Vector vecSwingEnd = vecSwingStart + vecForward * 48;

	// See if we hit anything.
	UTIL_TraceLine( vecSwingStart, vecSwingEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
	if ( trace.fraction >= 1.0 )
	{
		UTIL_TraceHull( vecSwingStart, vecSwingEnd, vecSwingMins, vecSwingMaxs, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = trace.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
			{
				// Why duck hull min/max?
				FindHullIntersection( vecSwingStart, trace, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			}

			// This is the point on the actual surface (the hull could have hit space)
			vecSwingEnd = trace.endpos;	
		}
	}

	return ( trace.fraction < 1.0f );
}

// -----------------------------------------------------------------------------
// Purpose:
// Note: Think function to delay the impact decal until the animation is finished 
//       playing.
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Smack( void )
{
	trace_t trace;

	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	// We hit, setup the smack.
	if ( DoSwingTrace( trace ) )
	{
		// Hit sound - immediate.
		if( trace.m_pEnt->IsPlayer()  )
		{
			WeaponSound( MELEE_HIT );
		}
		else
		{
			WeaponSound( MELEE_HIT_WORLD );
		}

		// Get the current player.
		CTFPlayer *pPlayer = GetTFPlayerOwner();
		if ( !pPlayer )
			return;

		Vector vecForward; 
		AngleVectors( pPlayer->EyeAngles(), &vecForward );
		Vector vecSwingStart = pPlayer->Weapon_ShootPosition();
		Vector vecSwingEnd = vecSwingStart + vecForward * 48;

#ifndef CLIENT_DLL
		// Do Damage.
		int iCustomDamage = TF_DMG_CUSTOM_NONE;
		float flDamage = GetMeleeDamage( trace.m_pEnt, iCustomDamage );
		int iDmgType = DMG_BULLET | DMG_NEVERGIB | DMG_CLUB;
		if ( IsCurrentAttackACrit() )
		{
			// TODO: Not removing the old critical path yet, but the new custom damage is marking criticals as well for melee now.
			iDmgType |= DMG_CRITICAL;
		}
		CTakeDamageInfo info( pPlayer, pPlayer, flDamage, iDmgType, iCustomDamage );
		CalculateMeleeDamageForce( &info, vecForward, vecSwingEnd, 1.0f / flDamage * tf_meleeattackforcescale.GetFloat() );
		trace.m_pEnt->DispatchTraceAttack( info, vecForward, &trace ); 
		ApplyMultiDamage();

		OnEntityHit( trace.m_pEnt );
#endif
		// Don't impact trace friendly players or objects
		if ( trace.m_pEnt && trace.m_pEnt->GetTeamNumber() != pPlayer->GetTeamNumber() )
		{
#ifdef CLIENT_DLL
			UTIL_ImpactTrace( &trace, DMG_CLUB );
#endif
			m_bConnected = true;
		}

	}

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CTFWeaponBaseMelee::GetMeleeDamage( CBaseEntity *pTarget, int &iCustomDamage )
{
	return static_cast<float>( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage );
}

void CTFWeaponBaseMelee::OnEntityHit( CBaseEntity *pEntity )
{
	NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseMelee::CalcIsAttackCriticalHelper( void )
{
	int nCvarValue = tf_weapon_criticals_melee.GetInt();
	if ( nCvarValue == 0 )
		return false;

	if ( nCvarValue == 1 && !tf_weapon_criticals.GetBool() )
		return false;

	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	float flPlayerCritMult = pPlayer->GetCritMult();

	return ( RandomInt( 0, WEAPON_RANDOM_RANGE-1 ) <= ( TF_DAMAGE_CRIT_CHANCE_MELEE * flPlayerCritMult ) * WEAPON_RANDOM_RANGE );
}
