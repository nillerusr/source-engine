//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is the Shotgun weapon
//
// $Workfile:     $
// $Date:         $
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
#include "gamerules.h"		// For g_pGameRules
#include "in_buttons.h"
//#include "soundent.h"
#include "vstdlib/random.h"


#ifdef CLIENT_DLL
#define CWeaponShotgun C_WeaponShotgun
#endif

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN	Vector( 0.08716, 0.04362, 0.00  )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN Vector( 0.17365, 0.04362, 0.00 ) // 20 degrees by 5 degrees


class CWeaponShotgun : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeaponShotgun, CBaseHL1MPCombatWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

private:
//	float	m_flPumpTime;
//	int		m_fInSpecialReload;

	CNetworkVar( float, m_flPumpTime);
	CNetworkVar( int, m_fInSpecialReload );

public:
	void	Precache( void );

	bool Reload( void );
	void FillClip( void );
	void WeaponIdle( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void DryFire( void );

//	DECLARE_SERVERCLASS();
//	DECLARE_DATADESC();

	CWeaponShotgun(void);

//#ifndef CLIENT_DLL
//	DECLARE_ACTTABLE();
//#endif
};


IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShotgun, DT_WeaponShotgun );

BEGIN_NETWORK_TABLE( CWeaponShotgun, DT_WeaponShotgun )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flPumpTime ) ),
	RecvPropInt( RECVINFO( m_fInSpecialReload ) ),
#else
	SendPropFloat( SENDINFO( m_flPumpTime ) ),
	SendPropInt( SENDINFO( m_fInSpecialReload ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponShotgun )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flPumpTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fInSpecialReload, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_shotgun, CWeaponShotgun );
PRECACHE_WEAPON_REGISTER(weapon_shotgun);

//IMPLEMENT_SERVERCLASS_ST( CWeaponShotgun, DT_WeaponShotgun )
//END_SEND_TABLE()

//BEGIN_DATADESC( CWeaponShotgun )
//END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponShotgun::CWeaponShotgun( void )
{
	m_bReloadsSingly	= true;
	m_bFiresUnderwater	= false;
	m_flPumpTime		= 0.0;
	m_fInSpecialReload	= 0;
}


void CWeaponShotgun::Precache( void )
{
	BaseClass::Precache();
}

void CWeaponShotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 <= 0 )
			DryFire( );

		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound( SINGLE );

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;
	m_iClip1 -= 1;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

	if ( g_pGameRules->IsMultiplayer() )
	{
		FireBulletsInfo_t info( 4, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
		info.m_pAttacker = pPlayer;

		pPlayer->FireBullets( info );
	}
	else
	{
		FireBulletsInfo_t info( 6, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
		info.m_pAttacker = pPlayer;

		pPlayer->FireBullets( info );

//		pPlayer->FireBullets( 6, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
	}

	EjectShell( pPlayer, 1 );

#if !defined(CLIENT_DLL)
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );
#endif

	pPlayer->ViewPunch( QAngle( -5, 0, 0 ) );

//	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2 );
	WeaponSound( SINGLE );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	if ( m_iClip1 > 0 )
	{
		m_flPumpTime = gpGlobals->curtime + 0.5;
	}

	m_fInSpecialReload = 0;
}


void CWeaponShotgun::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	if ( m_iClip1 <= 1 )
	{
		Reload();
		if ( m_iClip1 <= 0 )
			DryFire( );

		return;
	}

	if ( pPlayer->GetWaterLevel() == 3 )
	{
		// This weapon doesn't fire underwater
		WeaponSound(EMPTY);
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		m_flNextSecondaryAttack	= gpGlobals->curtime + 0.2;
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound( WPN_DOUBLE );

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack	= gpGlobals->curtime + 1.5;
	m_flNextSecondaryAttack	= gpGlobals->curtime + 1.5;

	m_iClip1 -= 2;	// Shotgun uses same clip for primary and secondary attacks

	Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

	// Fire the bullets
	if ( g_pGameRules->IsMultiplayer() )
	{
		FireBulletsInfo_t info( 8, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
		info.m_pAttacker = pPlayer;

		pPlayer->FireBullets( info );

//		pPlayer->FireBullets( 8, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
	}
	else
	{
		FireBulletsInfo_t info( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );

		pPlayer->FireBullets( info );
//		pPlayer->FireBullets( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
	}

	EjectShell( pPlayer, 1 );
	EjectShell( pPlayer, 1 );

	pPlayer->ViewPunch( QAngle( -10, 0, 0 ) );
#if !defined(CLIENT_DLL)
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );
#endif

//	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 800, 0.2 );
	WeaponSound( SINGLE );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	if ( m_iClip1 > 0 )
	{
		m_flPumpTime = gpGlobals->curtime + 0.5;
	}

	m_fInSpecialReload = 0;
}


bool CWeaponShotgun::Reload( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return false;

	if ( m_iClip1 >= GetMaxClip1() )
		return false;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return false;

	// check to see if we're ready to reload
	if ( m_fInSpecialReload == 0 )
	{
		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_fInSpecialReload = 1;

		pOwner->m_flNextAttack	= gpGlobals->curtime + 0.6;
		SetWeaponIdleTime( gpGlobals->curtime + 0.6 );
		m_flNextPrimaryAttack	= gpGlobals->curtime + 1.0;
		m_flNextSecondaryAttack	= gpGlobals->curtime + 1.0;

		return true;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( !HasWeaponIdleTimeElapsed() )
			return false;

		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		// Play reload on different channel as otherwise steals channel away from fire sound
		WeaponSound( RELOAD );
		SendWeaponAnim( ACT_VM_RELOAD );

		SetWeaponIdleTime( gpGlobals->curtime + 0.5 );
	}
	else
	{
		FillClip();
		m_fInSpecialReload = 1;
	}

	return true;
}


void CWeaponShotgun::FillClip( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	// Add them to the clip
	m_iClip1++;
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}


void CWeaponShotgun::DryFire( void )
{
	WeaponSound( EMPTY );
	m_flNextPrimaryAttack	= gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack	= gpGlobals->curtime + 0.75;
}


void CWeaponShotgun::WeaponIdle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( pPlayer == NULL )
		return;

	pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flPumpTime && m_flPumpTime < gpGlobals->curtime )
	{
		// play pumping sound
		WeaponSound( SPECIAL1 );
		m_flPumpTime = 0;
	}

	if ( HasWeaponIdleTimeElapsed() )
	{
		if ( m_iClip1 == 0 && m_fInSpecialReload == 0 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
		{
			Reload();
		}
		else if ( m_fInSpecialReload != 0 )
		{
			if ( m_iClip1 != 8 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
			{
				Reload( );
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim( ACT_SHOTGUN_PUMP );
				
				// play cocking sound
				WeaponSound( SPECIAL1 );
				m_fInSpecialReload = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5 );
			}
		}
		else
		{
			int		iAnim;
			float	flRand = random->RandomFloat( 0, 1 );

			if ( flRand <= 0.8 )
			{
				iAnim = ACT_SHOTGUN_IDLE_DEEP;
			}
			else if ( flRand <= 0.95 )
			{
				iAnim = ACT_VM_IDLE;
			}
			else
			{
				iAnim = ACT_SHOTGUN_IDLE4;
			}

			SendWeaponAnim( iAnim );
		}
	}
}
