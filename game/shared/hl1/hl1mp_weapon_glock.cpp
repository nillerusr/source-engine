//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Glock - hand gun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "hl1mp_basecombatweapon_shared.h"
//#include "basecombatcharacter.h"
//#include "AI_BaseNPC.h"
#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#else
#include "player.h"
#endif
#include "gamerules.h"
#include "in_buttons.h"
#ifdef CLIENT_DLL
#else
#include "soundent.h"
#include "game.h"
#endif
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

#ifdef CLIENT_DLL
#define CWeaponGlock C_WeaponGlock
#endif

class CWeaponGlock : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeaponGlock, CBaseHL1MPCombatWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

public:

	CWeaponGlock(void);

	void	Precache( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	bool	Reload( void );
	void	WeaponIdle( void );
	void	DryFire( void );

private:
	void	GlockFire( float flSpread , float flCycleTime, bool fUseAutoAim );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGlock, DT_WeaponGlock );

BEGIN_NETWORK_TABLE( CWeaponGlock, DT_WeaponGlock )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_glock, CWeaponGlock );

BEGIN_PREDICTION_DATA( CWeaponGlock )
END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER( weapon_glock );


CWeaponGlock::CWeaponGlock( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
}


void CWeaponGlock::Precache( void )
{

	BaseClass::Precache();
}


void CWeaponGlock::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
		
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
}


void CWeaponGlock::PrimaryAttack( void )
{
	GlockFire( 0.01, 0.3, TRUE );
}


void CWeaponGlock::SecondaryAttack( void )
{
	GlockFire( 0.1, 0.2, FALSE );
}


void CWeaponGlock::GlockFire( float flSpread , float flCycleTime, bool fUseAutoAim )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			DryFire();
		}

		return;
	}

	WeaponSound( SINGLE );

	pPlayer->DoMuzzleFlash();

	m_iClip1--;

	if ( m_iClip1 == 0 )
		SendWeaponAnim( ACT_GLOCK_SHOOTEMPTY );
	else
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack	= gpGlobals->curtime + flCycleTime;
	m_flNextSecondaryAttack	= gpGlobals->curtime + flCycleTime;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming;
	
	if ( fUseAutoAim )
	{
		vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );	
	}
	else
	{
		vecAiming = pPlayer->GetAutoaimVector( 0 );	
	}

//	pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
	FireBulletsInfo_t info( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;
	pPlayer->FireBullets( info );

	EjectShell( pPlayer, 0 );

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );
#if !defined(CLIENT_DLL)
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 400, 0.2 );
#endif

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	SetWeaponIdleTime( gpGlobals->curtime + random->RandomFloat( 10, 15 ) );
}


bool CWeaponGlock::Reload( void )
{
	bool iResult;

	if ( m_iClip1 == 0 )
		iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_GLOCK_SHOOT_RELOAD );
	else
		iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );

	return iResult;
}



void CWeaponGlock::WeaponIdle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	}

	// only idle if the slid isn't back
	if ( m_iClip1 != 0 )
	{
		BaseClass::WeaponIdle();
	}
}
