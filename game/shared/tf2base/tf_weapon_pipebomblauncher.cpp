//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_pipebomblauncher.h"
#include "tf_fx_shared.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "in_buttons.h"
#include "datacache/imdlcache.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "prediction.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#endif

// Delete me and put in script
extern ConVar tf_grenadelauncher_livetime;

// hard code these eventually
#define TF_PIPEBOMB_MIN_CHARGE_VEL 900
#define TF_PIPEBOMB_MAX_CHARGE_VEL 2400
#define TF_PIPEBOMB_MAX_CHARGE_TIME 4.0f

//=============================================================================
//
// Weapon Pipebomb Launcher tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFPipebombLauncher, DT_WeaponPipebombLauncher )

BEGIN_NETWORK_TABLE_NOBASE( CTFPipebombLauncher, DT_PipebombLauncherLocalData )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iPipebombCount ) ),
#else
	SendPropInt( SENDINFO( m_iPipebombCount ), 5, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()


BEGIN_NETWORK_TABLE( CTFPipebombLauncher, DT_WeaponPipebombLauncher )
#ifdef CLIENT_DLL
	RecvPropDataTable( "PipebombLauncherLocalData", 0, 0, &REFERENCE_RECV_TABLE( DT_PipebombLauncherLocalData ) ),
#else
	SendPropDataTable( "PipebombLauncherLocalData", 0, &REFERENCE_SEND_TABLE( DT_PipebombLauncherLocalData ), SendProxy_SendLocalWeaponDataTable ),
#endif	
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CTFPipebombLauncher )
	DEFINE_FIELD(  m_flChargeBeginTime, FIELD_FLOAT )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( tf_weapon_pipebomblauncher, CTFPipebombLauncher );
PRECACHE_WEAPON_REGISTER( tf_weapon_pipebomblauncher );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFPipebombLauncher )
END_DATADESC()
#endif

//=============================================================================
//
// Weapon Pipebomb Launcher functions.
//

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPipebombLauncher::CTFPipebombLauncher()
{
	m_bReloadsSingly = true;
	m_flLastDenySoundTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPipebombLauncher::~CTFPipebombLauncher()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::Spawn( void )
{
	m_iAltFireHint = HINT_ALTFIRE_PIPEBOMBLAUNCHER;
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Reset the charge when we holster
//-----------------------------------------------------------------------------
bool CTFPipebombLauncher::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_flChargeBeginTime = 0;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Reset the charge when we deploy
//-----------------------------------------------------------------------------
bool CTFPipebombLauncher::Deploy( void )
{
	m_flChargeBeginTime = 0;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::WeaponReset( void )
{
	BaseClass::WeaponReset();

#ifndef CLIENT_DLL
	DetonateRemotePipebombs( true );
#endif

	m_flChargeBeginTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::PrimaryAttack( void )
{
	// Check for ammunition.
	if ( m_iClip1 <= 0 && m_iClip1 != -1 )
		return;

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	if ( !CanAttack() )
	{
		m_flChargeBeginTime = 0;
		return;
	}

	if ( m_flChargeBeginTime <= 0 )
	{
		// Set the weapon mode.
		m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

		// save that we had the attack button down
		m_flChargeBeginTime = gpGlobals->curtime;

		SendWeaponAnim( ACT_VM_PULLBACK );
	}
	else
	{
		float flTotalChargeTime = gpGlobals->curtime - m_flChargeBeginTime;

		if ( flTotalChargeTime >= TF_PIPEBOMB_MAX_CHARGE_TIME )
		{
			LaunchGrenade();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::WeaponIdle( void )
{
	if ( m_flChargeBeginTime > 0 && m_iClip1 > 0 )
	{
		if ( m_iClip1 > 0 )
		{
			LaunchGrenade();
		}
	}
	else
	{
		BaseClass::WeaponIdle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::LaunchGrenade( void )
{
	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	CalcIsAttackCritical();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );

	CTFGrenadePipebombProjectile *pProjectile = static_cast<CTFGrenadePipebombProjectile*>( FireProjectile( pPlayer ) );
	if ( pProjectile )
	{
		// Save the charge time to scale the detonation timer.
		pProjectile->SetChargeTime( gpGlobals->curtime - m_flChargeBeginTime );
	}
#if !defined( CLIENT_DLL ) 
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#endif

	// Set next attack times.
	m_flNextPrimaryAttack = gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;
	m_flLastDenySoundTime = gpGlobals->curtime;

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	// Check the reload mode and behave appropriately.
	if ( m_bReloadsSingly )
	{
		m_iReloadMode.Set( TF_RELOAD_START );
	}

	m_flChargeBeginTime = 0;
}

float CTFPipebombLauncher::GetProjectileSpeed( void )
{
	float flForwardSpeed = RemapValClamped( ( gpGlobals->curtime - m_flChargeBeginTime ),
		0.0f,
		TF_PIPEBOMB_MAX_CHARGE_TIME,
		TF_PIPEBOMB_MIN_CHARGE_VEL,
		TF_PIPEBOMB_MAX_CHARGE_VEL );

	return flForwardSpeed;
}

void CTFPipebombLauncher::AddPipeBomb( CTFGrenadePipebombProjectile *pBomb )
{
	PipebombHandle hHandle;
	hHandle = pBomb;
	m_Pipebombs.AddToTail( hHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Add pipebombs to our list as they're fired
//-----------------------------------------------------------------------------
CBaseEntity *CTFPipebombLauncher::FireProjectile( CTFPlayer *pPlayer )
{
	CBaseEntity *pProjectile = BaseClass::FireProjectile( pPlayer );
	if ( pProjectile )
	{
#ifdef GAME_DLL
		// If we've gone over the max pipebomb count, detonate the oldest
		if ( m_Pipebombs.Count() >= TF_WEAPON_PIPEBOMB_COUNT )
		{
			CTFGrenadePipebombProjectile *pTemp = m_Pipebombs[0];
			if ( pTemp )
			{
				pTemp->SetTimer( gpGlobals->curtime ); // explode NOW
			}

			m_Pipebombs.Remove(0);
		}

		CTFGrenadePipebombProjectile *pPipebomb = (CTFGrenadePipebombProjectile*)pProjectile;
		pPipebomb->SetLauncher( this );

		PipebombHandle hHandle;
		hHandle = pPipebomb;
		m_Pipebombs.AddToTail( hHandle );

		m_iPipebombCount = m_Pipebombs.Count();
 #endif
	}

	return pProjectile;
}

//-----------------------------------------------------------------------------
// Purpose: Detonate this demoman's pipebombs if secondary fire is down.
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::ItemBusyFrame( void )
{
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->m_nButtons & IN_ATTACK2 )
	{
		// We need to do this to catch the case of player trying to detonate
		// pipebombs while in the middle of reloading.
		SecondaryAttack();
	}
#endif

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Detonate active pipebombs
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::SecondaryAttack( void )
{
	if ( !CanAttack() )
		return;

	if ( m_iPipebombCount )
	{
		// Get a valid player.
		CTFPlayer *pPlayer = ToTFPlayer( GetOwner() );
		if ( !pPlayer )
			return;

		//If one or more pipebombs failed to detonate then play a sound.
		if ( DetonateRemotePipebombs( false ) == true )
		{
			if ( m_flLastDenySoundTime <= gpGlobals->curtime )
			{
				// Deny!
				m_flLastDenySoundTime = gpGlobals->curtime + 1;
				WeaponSound( SPECIAL2 );
				return;
			}
		}
		else
		{
			// Play a detonate sound.
			WeaponSound( SPECIAL3 );
		}
	}
}

//=============================================================================
//
// Server specific functions.
//
#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::UpdateOnRemove(void)
{
	// If we just died, we want to fizzle our pipebombs.
	// If the player switched classes, our pipebombs have already been removed.
	DetonateRemotePipebombs( true );

	BaseClass::UpdateOnRemove();
}


#endif


//-----------------------------------------------------------------------------
// Purpose: If a pipebomb has been removed, remove it from our list
//-----------------------------------------------------------------------------
void CTFPipebombLauncher::DeathNotice( CBaseEntity *pVictim )
{
	Assert( dynamic_cast<CTFGrenadePipebombProjectile*>(pVictim) );

	PipebombHandle hHandle;
	hHandle = (CTFGrenadePipebombProjectile*)pVictim;
	m_Pipebombs.FindAndRemove( hHandle );

	m_iPipebombCount = m_Pipebombs.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Remove *with* explosions
//-----------------------------------------------------------------------------
bool CTFPipebombLauncher::DetonateRemotePipebombs( bool bFizzle )
{
	bool bFailedToDetonate = false;

	int count = m_Pipebombs.Count();

	for ( int i = 0; i < count; i++ )
	{
		CTFGrenadePipebombProjectile *pTemp = m_Pipebombs[i];
		if ( pTemp )
		{
			//This guy will die soon enough.
			if ( pTemp->IsEffectActive( EF_NODRAW ) )
				continue;
#ifdef GAME_DLL
			if ( bFizzle )
			{
				pTemp->Fizzle();
			}
#endif

			if ( bFizzle == false )
			{
				if ( ( gpGlobals->curtime - pTemp->m_flCreationTime ) < tf_grenadelauncher_livetime.GetFloat() )
				{
					bFailedToDetonate = true;
					continue;
				}
			}
#ifdef GAME_DLL
			
			pTemp->Detonate();
#endif
		}
	}

	return bFailedToDetonate;
}


float CTFPipebombLauncher::GetChargeMaxTime( void )
{
	return TF_PIPEBOMB_MAX_CHARGE_TIME;
}


bool CTFPipebombLauncher::Reload( void )
{
	if ( m_flChargeBeginTime > 0 )
		return false;

	return BaseClass::Reload();
}
