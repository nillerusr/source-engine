//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_lunchbox.h"
#include "tf_fx_shared.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "entity_healthkit.h"
#include "econ_item_view.h"
#include "econ_item_system.h"
#include "tf_gamestats.h"
#endif

//=============================================================================
//
// Weapon Lunchbox tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFLunchBox, DT_WeaponLunchBox )

BEGIN_NETWORK_TABLE( CTFLunchBox, DT_WeaponLunchBox )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFLunchBox )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_lunchbox, CTFLunchBox );
PRECACHE_WEAPON_REGISTER( tf_weapon_lunchbox );

//=============================================================================

IMPLEMENT_NETWORKCLASS_ALIASED( TFLunchBox_Drink, DT_TFLunchBox_Drink )

BEGIN_NETWORK_TABLE( CTFLunchBox_Drink, DT_TFLunchBox_Drink )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFLunchBox_Drink )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_lunchbox_drink, CTFLunchBox_Drink );
PRECACHE_WEAPON_REGISTER( tf_weapon_lunchbox_drink );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFLunchBox )
END_DATADESC()
#endif

#define LUNCHBOX_DROP_MODEL  "models/items/plate.mdl"
#define LUNCHBOX_STEAK_DROP_MODEL  "models/items/plate_steak.mdl"
#define LUNCHBOX_ROBOT_DROP_MODEL  "models/items/plate_robo_sandwich.mdl"
#define LUNCHBOX_FESTIVE_DROP_MODEL  "models/items/plate_sandwich_xmas.mdl"
#define LUNCHBOX_CHOCOLATE_BAR		"models/props_halloween/halloween_medkit_small.mdl"

#define LUNCHBOX_DROPPED_MINS	Vector( -17, -17, -10 )
#define LUNCHBOX_DROPPED_MAXS	Vector( 17, 17, 10 )

static const char *s_pszLunchboxMaxHealThink = "LunchboxMaxHealThink";

//=============================================================================
//
// Weapon Lunchbox functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFLunchBox::CTFLunchBox()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::UpdateOnRemove( void )
{
#ifndef CLIENT_DLL
	// If we're removed, we remove any dropped powerups. This prevents an exploit
	// where they switch classes away & back to get another lunchbox to drop with.
	if ( m_hThrownPowerup )
	{
		UTIL_Remove( m_hThrownPowerup );
	}
#endif

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::Precache( void )
{
	if ( DropAllowed() )
	{
		PrecacheModel( "models/items/medkit_medium.mdl" );
		PrecacheModel( "models/items/medkit_medium_bday.mdl" );
		PrecacheModel( LUNCHBOX_DROP_MODEL );
		PrecacheModel( LUNCHBOX_STEAK_DROP_MODEL );
		PrecacheModel( LUNCHBOX_ROBOT_DROP_MODEL );
		PrecacheModel( LUNCHBOX_FESTIVE_DROP_MODEL );
		PrecacheModel( LUNCHBOX_CHOCOLATE_BAR );
	}

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::WeaponReset( void )
{
	BaseClass::WeaponReset();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFLunchBox::UsesPrimaryAmmo( void )
{
	return CBaseCombatWeapon::UsesPrimaryAmmo();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFLunchBox::DropAllowed( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( pOwner )
	{
		if ( pOwner->m_Shared.InCond( TF_COND_TAUNTING ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::PrimaryAttack( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return;

	if ( !HasAmmo() )
		return;

#if GAME_DLL
	pOwner->Taunt();
	m_flNextPrimaryAttack = pOwner->GetTauntRemoveTime() + 0.1f;
#else
	m_flNextPrimaryAttack = gpGlobals->curtime + 2.0f; // this will be corrected by the game server
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::SecondaryAttack( void )
{
	if ( !DropAllowed() )
		return;

	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( !HasAmmo() )
		return;

#ifndef CLIENT_DLL

	if ( m_hThrownPowerup )
	{
		UTIL_Remove( m_hThrownPowerup );
	}

	// Throw out the medikit
	Vector vecSrc = pPlayer->EyePosition() + Vector(0,0,-8);
	QAngle angForward = pPlayer->EyeAngles() + QAngle(-10,0,0);

	int nLunchBoxType = GetLunchboxType();

	const char *pszHealthKit;	
	switch ( nLunchBoxType )
	{
	case LUNCHBOX_ADDS_MAXHEALTH:
		pszHealthKit = "item_healthkit_small";
		break;

	case LUNCHBOX_ADDS_AMMO:
		pszHealthKit = "item_healthammokit";
		break;

	default:
		pszHealthKit = "item_healthkit_medium";
	}

	CHealthKit *pMedKit = assert_cast<CHealthKit*>( CBaseEntity::Create( pszHealthKit, vecSrc, angForward, pPlayer ) );

	if ( pMedKit )
	{
		Vector vecForward, vecRight, vecUp;
		AngleVectors( angForward, &vecForward, &vecRight, &vecUp );
		Vector vecVelocity = vecForward * 500.0;
		
		if ( nLunchBoxType == LUNCHBOX_ADDS_MINICRITS )
		{
			pMedKit->SetModel( LUNCHBOX_STEAK_DROP_MODEL );
		}
		else if ( nLunchBoxType == LUNCHBOX_STANDARD_ROBO )
		{
			pMedKit->SetModel( LUNCHBOX_ROBOT_DROP_MODEL );
			pMedKit->m_nSkin = ( pPlayer->GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;
		}
		else if ( nLunchBoxType == LUNCHBOX_STANDARD_FESTIVE )
		{
			pMedKit->SetModel( LUNCHBOX_FESTIVE_DROP_MODEL );
			pMedKit->m_nSkin = ( pPlayer->GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;
		}
		else if ( nLunchBoxType == LUNCHBOX_ADDS_MAXHEALTH )
		{
			pMedKit->SetModel( LUNCHBOX_CHOCOLATE_BAR );
		}
		else
		{
			pMedKit->SetModel( LUNCHBOX_DROP_MODEL );
		}

		// clear out the overrides so the thrown sandvich/steak look correct in either vision mode
		pMedKit->ClearModelIndexOverrides();

		pMedKit->SetAbsAngles( vec3_angle );
		pMedKit->SetSize( LUNCHBOX_DROPPED_MINS, LUNCHBOX_DROPPED_MAXS );

		// the thrower has to wait 0.3 to pickup the powerup (so he can throw it while running forward)
		pMedKit->DropSingleInstance( vecVelocity, pPlayer, 0.3 );
	}

	m_hThrownPowerup = pMedKit;
#endif

	pPlayer->RemoveAmmo( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_iAmmoPerShot, m_iPrimaryAmmoType );
	g_pGameRules->SwitchToNextBestWeapon( pPlayer, this );

	StartEffectBarRegen();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::DrainAmmo( bool bForceCooldown )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return;

#ifdef GAME_DLL

	int iLunchboxType = GetLunchboxType();
	// If we're damaged while eating/taunting, bForceCooldown will be true
	if ( pOwner->IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) )
	{
		if ( pOwner->GetHealth() < pOwner->GetMaxHealth() || GetLunchboxType() == LUNCHBOX_ADDS_MINICRITS || iLunchboxType == LUNCHBOX_ADDS_MAXHEALTH || bForceCooldown )
		{
			StartEffectBarRegen();
		}
		else	// Full health regular sandwhich, I can eat forever
		{	
			return;
		}
	}
	else if ( pOwner->IsPlayerClass( TF_CLASS_SCOUT ) )
	{
		StartEffectBarRegen();
	}

	// Strange Tracking.  Only go through if we have ammo at this point.
	if ( !pOwner->IsBot() && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		EconEntity_OnOwnerKillEaterEventNoPartner( dynamic_cast<CEconEntity *>( this ), pOwner, kKillEaterEvent_FoodEaten );
	}

	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
#else
	
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
	StartEffectBarRegen();

#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::Detach( void )
{
#ifdef GAME_DLL
	// Terrible - but for now, we're the only place that adds this (custom) attribute
	if ( GetLunchboxType() == LUNCHBOX_ADDS_MAXHEALTH )
	{
		CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			// Prevents use-then-switch-class exploit (heavy->scout)
			// Not a big deal in pubs, but it can mess with competitive
			pOwner->RemoveCustomAttribute( "hidden maxhealth non buffed" );
		}
	}
#endif

	BaseClass::Detach();
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFLunchBox::ApplyBiteEffects( CTFPlayer *pPlayer )
{
	int nLunchBoxType = GetLunchboxType();

	if ( nLunchBoxType == LUNCHBOX_ADDS_MAXHEALTH )
	{
		// add 50 health to player for 30 seconds
		pPlayer->AddCustomAttribute( "hidden maxhealth non buffed", 50.f, 30.f );
	}
	else if ( nLunchBoxType == LUNCHBOX_ADDS_MINICRITS )
	{
		static const float s_fSteakSandwichDuration = 16.0f;

		// Steak sandvich.
		pPlayer->m_Shared.AddCond( TF_COND_ENERGY_BUFF, s_fSteakSandwichDuration );
		pPlayer->m_Shared.AddCond( TF_COND_CANNOT_SWITCH_FROM_MELEE, s_fSteakSandwichDuration );
		pPlayer->m_Shared.SetBiteEffectWasApplied();

		return;
	}
	
	// Then heal the player
	int iHeal = ( nLunchBoxType == LUNCHBOX_ADDS_MAXHEALTH ) ? 25 : 75;
	int iHealType = DMG_GENERIC;
	if ( nLunchBoxType == LUNCHBOX_ADDS_MAXHEALTH && pPlayer->GetHealth() < 400 )
	{
		iHealType = DMG_IGNORE_MAXHEALTH;
		iHeal = Min( 25, 400 - pPlayer->GetHealth() );
	}

	float flHealScale = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flHealScale, lunchbox_healing_scale );
	iHeal = iHeal * flHealScale;

	int iHealed = pPlayer->TakeHealth( iHeal, iHealType );

	if ( iHealed > 0 )
	{
		CTF_GameStats.Event_PlayerHealedOther( pPlayer, iHealed );
	}

	// Restore ammo if applicable
	if ( nLunchBoxType == LUNCHBOX_ADDS_AMMO )
	{
		int maxPrimary = pPlayer->GetMaxAmmo( TF_AMMO_PRIMARY );
		pPlayer->GiveAmmo( maxPrimary * 0.25, TF_AMMO_PRIMARY, true );
	}
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose:  Energy Drink
//-----------------------------------------------------------------------------
CTFLunchBox_Drink::CTFLunchBox_Drink()
{
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFLunchBox_Drink::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( pOwner && pOwner->IsLocalPlayer() )
	{
		C_BaseEntity *pParticleEnt = pOwner->GetViewModel(0);
		if ( pParticleEnt )
		{
			pOwner->StopViewModelParticles( pParticleEnt );
		}
	}

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CTFLunchBox_Drink::ModifyEventParticles( const char* token )
{
	if ( GetLunchboxType() == LUNCHBOX_ADDS_MINICRITS )
	{
		if ( FStrEq( token, "energydrink_splash") )
		{
			CEconItemView *pItem = m_AttributeManager.GetItem();
			int iSystems = pItem->GetStaticData()->GetNumAttachedParticles( GetTeamNumber() );
			for ( int i = 0; i < iSystems; i++ )
			{
				attachedparticlesystem_t *pSystem = pItem->GetStaticData()->GetAttachedParticleData( GetTeamNumber(),i );
				if ( pSystem->iCustomType == 1 )
				{
					return pSystem->pszSystemName;
				}
			}
		}
	}

	return BaseClass::ModifyEventParticles( token );
}

#endif // CLIENT_DLL