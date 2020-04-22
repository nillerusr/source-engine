//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		357 - hand gun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "hl1mp_basecombatweapon_shared.h"
#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#else
#include "player.h"
#endif
#include "gamerules.h"
#include "in_buttons.h"
#ifndef CLIENT_DLL
#include "soundent.h"
#include "game.h"
#endif
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeapon357, CBaseHL1CombatWeapon );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeapon357( void );

	void	Precache( void );
	bool	Deploy( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	bool	Reload( void );
	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

//	DECLARE_SERVERCLASS();
//	DECLARE_DATADESC();

private:
	void	ToggleZoom( void );

private:
	CNetworkVar( float, m_fInZoom );
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon357, DT_Weapon357 );

BEGIN_NETWORK_TABLE( CWeapon357, DT_Weapon357 )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_fInZoom ) ),
#else
	SendPropFloat( SENDINFO( m_fInZoom ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeapon357 )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_fInZoom, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_357, CWeapon357 );

PRECACHE_WEAPON_REGISTER( weapon_357 );

//IMPLEMENT_SERVERCLASS_ST( CWeapon357, DT_Weapon357 )
//END_SEND_TABLE()

//BEGIN_DATADESC( CWeapon357 )
//END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= false;
	m_fInZoom			= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon357::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "ammo_357" );
#endif
}

bool CWeapon357::Deploy( void )
{
// Bodygroup stuff not currently working correctly
//	if ( g_pGameRules->IsMultiplayer() )
//	{
		// enable laser sight geometry.
//		SetBodygroup( 4, 1 );
//	}
//	else
//	{
//		SetBodygroup( 4, 0 );
//	}

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack( void )
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
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	m_iClip1--;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

//	pPlayer->FireBullets( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );

	FireBulletsInfo_t info( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;

	pPlayer->FireBullets( info );

#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
#endif

	pPlayer->ViewPunch( QAngle( -10, 0, 0 ) );

#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2 );
#endif

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::SecondaryAttack( void )
{
	// only in multiplayer
	if ( !g_pGameRules->IsMultiplayer() )
	{
#ifndef CLIENT_DLL
		// unless we have cheats on
		if ( !sv_cheats->GetBool() )
		{
			return;
		}
#endif
	}

	ToggleZoom();

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
}


bool CWeapon357::Reload( void )
{
	bool fRet;

	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		if ( m_fInZoom )
		{
			ToggleZoom();
		}
	}

	return fRet;
}


void CWeapon357::WeaponIdle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	}

	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( random->RandomFloat( 0, 1 ) <= 0.9 )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
	else
	{
		SendWeaponAnim( ACT_VM_FIDGET );
	}
}


bool CWeapon357::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_fInZoom )
	{
		ToggleZoom();
	}

	return BaseClass::Holster( pSwitchingTo );
}


void CWeapon357::ToggleZoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

#ifndef CLIENT_DLL
	if ( m_fInZoom )
	{
		if ( pPlayer->SetFOV( this, 0 ) )
		{
			m_fInZoom = false;
			pPlayer->ShowViewModel( true );
		}
	}
	else
	{
		if ( pPlayer->SetFOV( this, 40 ) )
		{
			m_fInZoom = true;
			pPlayer->ShowViewModel( false );
		}
	}
#endif
}
