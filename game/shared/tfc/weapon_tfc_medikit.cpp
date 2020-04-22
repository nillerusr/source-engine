//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_medikit.h"
#include "tfc_gamerules.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
	#include "tfc_timer.h"
#endif


// ----------------------------------------------------------------------------- //
// CTFCMedikit tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCMedikit, DT_WeaponMedikit )

BEGIN_NETWORK_TABLE( CTFCMedikit, DT_WeaponMedikit )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCMedikit )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_medikit, CTFCMedikit );
PRECACHE_WEAPON_REGISTER( weapon_medikit );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCMedikit )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCMedikit implementation.
// ----------------------------------------------------------------------------- //

CTFCMedikit::CTFCMedikit()
{
}


void CTFCMedikit::Precache()
{
	BaseClass::Precache();
}


TFCWeaponID CTFCMedikit::GetWeaponID( void ) const
{ 
	return WEAPON_MEDIKIT;
}


#ifdef CLIENT_DLL
	
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// CLIENT DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //


#else

	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// GAME DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	
	void CTFCMedikit::AxeHit( CBaseEntity *pHit, bool bFirstSwing, trace_t &tr, float *flDamage, bool *bDoEffects )
	{	
		*flDamage = 0;
		*bDoEffects = false;

		CTFCPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return;

		// We only care about players.
		CTFCPlayer *pTarget = ToTFCPlayer( pHit );
		if ( !pTarget )
			return;

		// If other player is dead ( just changed teams? ) don't hit
		if ( !pTarget->IsAlive() )
			return;
	
		Vector vAttackDir = tr.endpos - tr.startpos;
		VectorNormalize( vAttackDir );

		// Heal if we're on the same team, Infect if not
		if ( pPlayer->IsAlly( pTarget ) )
		{
			// Heal Concussion
			CTimer *pTimer = Timer_FindTimer( pTarget, TF_TIMER_CONCUSSION );
			if ( pTimer )
			{
				// Tell everyone
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Medic_cureconc", pPlayer->GetPlayerName(), pTarget->GetPlayerName() );

				// Give the medic a frag if it was an enemy concussion
				if ( pTimer->GetTeamNumber() != pPlayer->GetTeamNumber() )
					pPlayer->TF_AddFrags(1);

				Timer_Remove( pTimer );
			}

			// Heal Hallucination
			if ( pTarget->m_Shared.GetStateFlags() & TFSTATE_HALLUCINATING )
			{
				pTimer = Timer_FindTimer( pTarget, TF_TIMER_HALLUCINATION );
				ASSERT(pTimer != NULL);
				
				// Tell everyone
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Medic_curehalluc", pPlayer->GetPlayerName(), pTarget->GetPlayerName() );

				// Give the medic a frag if it was an enemy hallucination
				if (pTimer->GetTeamNumber() != pPlayer->GetTeamNumber())
					pPlayer->TF_AddFrags(1);

				pTarget->m_Shared.RemoveStateFlags( TFSTATE_HALLUCINATING );
				Timer_Remove( pTimer );
			}

			// Heal Tranquilisation
			if (pTarget->m_Shared.GetStateFlags() & TFSTATE_TRANQUILISED)
			{
				pTimer = Timer_FindTimer( pTarget, TF_TIMER_TRANQUILISATION );
				ASSERT(pTimer != NULL);

				// Tell everyone
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Medic_curetranq", pPlayer->GetPlayerName(), pTarget->GetPlayerName() );

				// Give the medic a frag if it was an enemy tranquilisation
				if (pTimer->GetTeamNumber() != pPlayer->GetTeamNumber())
					pPlayer->TF_AddFrags(1);

				pTarget->m_Shared.RemoveStateFlags( TFSTATE_TRANQUILISED );
				// TFCTODO: ((CBasePlayer*)pTarget)->TeamFortress_SetSpeed();
				Timer_Remove( pTimer );
			}

			// Heal Infection
			if (pTarget->m_Shared.GetStateFlags() & TFSTATE_INFECTED)
			{
				int fDmg = pTarget->GetHealth() / 2;
				SpawnBlood(pTarget->GetAbsOrigin(), vAttackDir, pTarget->BloodColor(), fDmg);

#if !defined( CLIENT_DLL )
				pTimer = Timer_FindTimer( pTarget, TF_TIMER_INFECTION );
				ASSERT(pTimer != NULL);

				// Tell everyone
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Medic_cureinfection", pPlayer->GetPlayerName(), pTarget->GetPlayerName() );

				// Give the medic a frag if it was an enemy tranquilisation
				if ( pTimer->GetTeamNumber() != pPlayer->GetTeamNumber() )
					pPlayer->TF_AddFrags(1);

				// The operation removes half of the player's health
				CBaseEntity *pOwner = pTimer->m_hOwner;
				pTarget->TakeDamage( CTakeDamageInfo( this, pOwner, fDmg, DMG_IGNOREARMOR | DMG_NERVEGAS ) );

				pTarget->m_Shared.RemoveStateFlags( TFSTATE_INFECTED );
				Timer_Remove( pTimer );
#endif
			}

			// Extinguish Flames
			if ( pTarget->GetNumFlames() > 0 )
			{
				// Tell everyone
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Medic_curefire", pPlayer->GetPlayerName(), pTarget->GetPlayerName() );

				pTarget->SetNumFlames( 0 );
				//PLAYBACK_EVENT( 0, pPlayer->edict(), m_usSteamShot );
			}

			// Heal
			if ( pTarget->GetHealth() < pTarget->GetMaxHealth() )
			{
				//PLAYBACK_EVENT( 0, pPlayer->edict(), m_usNormalShot );

				pTarget->TakeHealth( WEAP_MEDIKIT_HEAL, DMG_GENERIC );		// Heal fully in one hit
			}
			else if (pTarget->GetHealth() < pTarget->GetMaxHealth() + WEAP_MEDIKIT_OVERHEAL)
			{
				// "Heal" over and above the player's max health
				//PLAYBACK_EVENT( 0, pPlayer->edict(), m_usSuperShot );

				pTarget->TakeHealth( 5, DMG_GENERIC | DMG_IGNORE_MAXHEALTH);

				if ( !(pTarget->m_Shared.GetItemFlags() & IT_SUPERHEALTH) )
				{
					pTarget->m_Shared.AddItemFlags( IT_SUPERHEALTH );

	#if !defined( CLIENT_DLL )
					// Start Superhealth rot timer
					Timer_CreateTimer( pTarget, TF_TIMER_ROTHEALTH );
	#endif
				}
			}
		}
		else
		{
			Vector vecDir;
			AngleVectors( pPlayer->EyeAngles(), &vecDir );

			// Damage Target
			ClearMultiDamage();
			pTarget->TraceAttack( CTakeDamageInfo( pPlayer, pPlayer, 10, DMG_CLUB ), vecDir, &tr ); 
			ApplyMultiDamage();

			// Don't infect if the player's already infected, a medic, or we're still in prematch
			if ( (pTarget->m_Shared.GetStateFlags() & TFSTATE_INFECTED) || 
				(pTarget->m_Shared.GetPlayerClass() == PC_MEDIC) || 
				TFCGameRules()->IsInPreMatch() )
			{
				return;
			}

			// Infect
			pTarget->m_Shared.AddStateFlags( TFSTATE_INFECTED );
	#if !defined( CLIENT_DLL )

			// we don't want to create another timer if the player we're attacking
			// with the medikit is currently dying ( already "killed" but still playing
			// their death animation )
			if ( pTarget->m_lifeState != LIFE_ALIVE )
				return;

			CTimer *pTimer = Timer_CreateTimer( pTarget, TF_TIMER_INFECTION );
			pTimer->m_hEnemy = pPlayer;
	#endif
		}
	}

#endif
