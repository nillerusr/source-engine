//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasemelee.h"
#include "dod_gamerules.h"

#if defined( CLIENT_DLL )
	#include "c_dod_player.h"
#else
	#include "dod_player.h"
	#include "ilagcompensationmanager.h"
#endif

#include "effect_dispatch_data.h"


#define	KNIFE_BODYHIT_VOLUME 128
#define	KNIFE_WALLHIT_VOLUME 512

// ----------------------------------------------------------------------------- //
// CWeaponDODBaseMelee tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDODBaseMelee, DT_WeaponDODBaseMelee )

BEGIN_NETWORK_TABLE_NOBASE( CWeaponDODBaseMelee, DT_LocalActiveWeaponBaseMeleeData )
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CWeaponDODBaseMelee, DT_WeaponDODBaseMelee )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponDODBaseMelee )
	DEFINE_PRED_FIELD( m_flSmackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_dod_base_melee, CWeaponDODBaseMelee );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CWeaponDODBaseMelee )
		DEFINE_FUNCTION( Smack )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponDODBaseMelee implementation.
// ----------------------------------------------------------------------------- //

CWeaponDODBaseMelee::CWeaponDODBaseMelee()
{
}

void CWeaponDODBaseMelee::Spawn()
{
	Precache();

	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );

	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );

	CDODWeaponInfo *pWeaponInfo = dynamic_cast< CDODWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

	Assert( pWeaponInfo && "Failed to get CDODWeaponInfo in melee weapon spawn" );
		
	m_pWeaponInfo = pWeaponInfo;

	Assert( m_pWeaponInfo );

	m_iClip1 = -1;
	BaseClass::Spawn();
}

void CWeaponDODBaseMelee::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > gpGlobals->curtime )
		return;

	SendWeaponAnim( ACT_VM_IDLE );

	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

void CWeaponDODBaseMelee::PrimaryAttack()
{
	MeleeAttack( 60, MELEE_DMG_EDGE, 0.2f, 0.4f );
}

CBaseEntity *CWeaponDODBaseMelee::MeleeAttack( int iDamageAmount, int iDamageType, float flDmgDelay, float flAttackDelay )
{
	if ( !CanAttack() )
		return NULL;

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	Vector vForward, vRight, vUp;
	AngleVectors( pPlayer->EyeAngles(), &vForward, &vRight, &vUp );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * 48;

	CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );

	int iTraceMask = MASK_SOLID | CONTENTS_HITBOX | CONTENTS_DEBRIS;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &filter, &tr );

	const float rayExtension = 40.0f;
	UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vForward * rayExtension, iTraceMask, &filter, &tr );

	if ( tr.fraction >= 1.0 )
	{
		Vector head_hull_mins( -16, -16, -18 );
		Vector head_hull_maxs( 16, 16, 18 );

		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, &filter, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)

			// Make sure it is in front of us
			Vector vecToEnd = vecEnd - vecSrc;
			VectorNormalize( vecToEnd );

			// if zero length, always hit
			if ( vecToEnd.Length() > 0 )
			{
				float dot = DotProduct( vForward, vecToEnd );

				// sanity that our hit is within range
				if ( abs(dot) < 0.95 )
				{
					// fake that we actually missed
					tr.fraction = 1.0;
				}
			}
		}
	}

	bool bDidHit = ( tr.fraction < 1.0f );

	bool bDoStrongAttack = false;

	if ( bDidHit && tr.m_pEnt->IsPlayer() && tr.m_pEnt->m_takedamage != DAMAGE_YES )
	{
		bDidHit = 0;	// still play the animation, we just dont attempt to damage this player
	}

	if ( bDidHit )	//if the swing hit 
	{	
		// delay the decal a bit
		m_trHit = tr;

		// Store the ent in an EHANDLE, just in case it goes away by the time we get into our think function.
		m_pTraceHitEnt = tr.m_pEnt; 

		m_iSmackDamage = iDamageAmount;
		m_iSmackDamageType = iDamageType;

		m_flSmackTime = gpGlobals->curtime + flDmgDelay;

		int iOwnerTeam = pPlayer->GetTeamNumber();
		int iVictimTeam = tr.m_pEnt->GetTeamNumber();

		// do the mega attack if its a player, and we would do damage
		if ( tr.m_pEnt->IsPlayer() &&
			tr.m_pEnt->m_takedamage == DAMAGE_YES && 
			( iVictimTeam != iOwnerTeam || ( iVictimTeam == iOwnerTeam && friendlyfire.GetBool() ) ) )
		{
			CDODPlayer *pVictim = ToDODPlayer( tr.m_pEnt );

			Vector victimForward;
			AngleVectors( pVictim->GetAbsAngles(), &victimForward );

			if ( DotProduct( victimForward, vForward ) > 0.3 )
			{
				bDoStrongAttack = true;
			}
		}
	}

	if ( bDoStrongAttack )
	{
		m_iSmackDamage = 300;
		flAttackDelay = 0.9f;
		m_flSmackTime = gpGlobals->curtime + 0.4f;

		m_iSmackDamageType = MELEE_DMG_EDGE | MELEE_DMG_STRONGATTACK;

		// play a "Strong" attack
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	}
	else
	{
		WeaponSound( MELEE_MISS );
		SendWeaponAnim( GetMeleeActivity() );
	}

	// player animation
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SECONDARY_ATTACK );

	m_flNextPrimaryAttack = gpGlobals->curtime + flAttackDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + flAttackDelay;
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();


#ifndef CLIENT_DLL
	IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_weapon_attack" );
	if ( event )
	{
		event->SetInt( "attacker", pPlayer->GetUserID() );
		event->SetInt( "weapon", GetStatsWeaponID() );

		gameeventmanager->FireEvent( event );
	}

	lagcompensation->FinishLagCompensation( pPlayer );
#endif	//CLIENT_DLL

	return tr.m_pEnt;
}