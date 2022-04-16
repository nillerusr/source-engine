//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Hornetgun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "hl1mp_basecombatweapon_shared.h"
//#include "basecombatcharacter.h"
//#include "AI_BaseNPC.h"
#include "gamerules.h"
#include "in_buttons.h"
#ifdef CLIENT_DLL
#include "hl1/c_hl1mp_player.h"
#else
#include "hl1mp_player.h"
#include "soundent.h"
#include "game.h"
#endif
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#if !defined(CLIENT_DLL)
#include "hl1_npc_hornet.h"
#endif


//-----------------------------------------------------------------------------
// CWeaponHgun
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CWeaponHgun C_WeaponHgun
#endif

class CWeaponHgun : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeaponHgun, CBaseHL1MPCombatWeapon );
public:

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponHgun( void );

	void	Precache( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool	Reload( void );

	virtual void ItemPostFrame( void );

//	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:

//	float	m_flRechargeTime;
//	int		m_iFirePhase;

	CNetworkVar( float,	m_flRechargeTime );
	CNetworkVar( int,	m_iFirePhase );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHgun, DT_WeaponHgun );

BEGIN_NETWORK_TABLE( CWeaponHgun, DT_WeaponHgun )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flRechargeTime ) ),
	RecvPropInt( RECVINFO( m_iFirePhase ) ),
#else
	SendPropFloat( SENDINFO( m_flRechargeTime ) ),
	SendPropInt( SENDINFO( m_iFirePhase ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHgun )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flRechargeTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iFirePhase, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_hornetgun, CWeaponHgun );

PRECACHE_WEAPON_REGISTER( weapon_hornetgun );

//IMPLEMENT_SERVERCLASS_ST( CWeaponHgun, DT_WeaponHgun )
//END_SEND_TABLE()

BEGIN_DATADESC( CWeaponHgun )
	DEFINE_FIELD( m_flRechargeTime,	FIELD_TIME ),
	DEFINE_FIELD( m_iFirePhase,		FIELD_INTEGER ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponHgun::CWeaponHgun( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;

	m_flRechargeTime = 0.0;
	m_iFirePhase = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHgun::Precache( void )
{
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "hornet" );
#endif

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponHgun::PrimaryAttack( void )
{
	CHL1_Player *pPlayer = ToHL1Player( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		return;
	}

	WeaponSound( SINGLE );
#if !defined(CLIENT_DLL)
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );
#endif
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector	vForward, vRight, vUp;
	QAngle	vecAngles;

	pPlayer->EyeVectors( &vForward, &vRight, &vUp );
	VectorAngles( vForward, vecAngles );

#if !defined(CLIENT_DLL)
	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", pPlayer->Weapon_ShootPosition() + vForward * 16 + vRight * 8 + vUp * -12, vecAngles, pPlayer );
	pHornet->SetAbsVelocity( vForward * 300 );
#endif

	m_flRechargeTime = gpGlobals->curtime + 0.5;
	
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );

	m_flNextPrimaryAttack = m_flNextPrimaryAttack + 0.25;

	if (m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.25;
	}

	SetWeaponIdleTime( random->RandomFloat( 10, 15 ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponHgun::SecondaryAttack( void )
{
	CHL1_Player *pPlayer = ToHL1Player( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		return;
	}

	WeaponSound( SINGLE );
#if !defined(CLIENT_DLL)
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 200, 0.2 );
#endif
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	CBaseEntity *pHornet;
	Vector vecSrc;

	Vector	vForward, vRight, vUp;
	QAngle	vecAngles;

	pPlayer->EyeVectors( &vForward, &vRight, &vUp );
	VectorAngles( vForward, vecAngles );

	vecSrc = pPlayer->Weapon_ShootPosition() + vForward * 16 + vRight * 8 + vUp * -12;

	m_iFirePhase++;
	switch ( m_iFirePhase )
	{
	case 1:
		vecSrc = vecSrc + vUp * 8;
		break;
	case 2:
		vecSrc = vecSrc + vUp * 8;
		vecSrc = vecSrc + vRight * 8;
		break;
	case 3:
		vecSrc = vecSrc + vRight * 8;
		break;
	case 4:
		vecSrc = vecSrc + vUp * -8;
		vecSrc = vecSrc + vRight * 8;
		break;
	case 5:
		vecSrc = vecSrc + vUp * -8;
		break;
	case 6:
		vecSrc = vecSrc + vUp * -8;
		vecSrc = vecSrc + vRight * -8;
		break;
	case 7:
		vecSrc = vecSrc + vRight * -8;
		break;
	case 8:
		vecSrc = vecSrc + vUp * 8;
		vecSrc = vecSrc + vRight * -8;
		m_iFirePhase = 0;
		break;
	}

#ifdef CLIENT_DLL
	pHornet = NULL;
#else
	pHornet = CBaseEntity::Create( "hornet", vecSrc, vecAngles, pPlayer );
	pHornet->SetAbsVelocity( vForward * 1200 );
	pHornet->SetThink( &CNPC_Hornet::StartDart );
#endif

	m_flRechargeTime = gpGlobals->curtime + 0.5;

	pPlayer->ViewPunch( QAngle( -2, 0, 0 ) );
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.1;
	SetWeaponIdleTime( random->RandomFloat( 10, 15 ) );
}

void CWeaponHgun::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	int iAnim;
	float flRand = random->RandomFloat( 0, 1 );
	if ( flRand <= 0.75 )
	{
		iAnim = ACT_VM_IDLE;
	}
	else
	{
		iAnim = ACT_VM_FIDGET;
	}

	SendWeaponAnim( iAnim );
}

bool CWeaponHgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	bool bRet;

	bRet = BaseClass::Holster( pSwitchingTo );

	if ( bRet )
	{
		CHL1_Player *pPlayer = ToHL1Player( GetOwner() );
		if ( pPlayer )
		{
#if !defined(CLIENT_DLL)            
			//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
            int iCount = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
            if ( iCount <= 0 )
            {
                pPlayer->GiveAmmo( iCount+1, m_iPrimaryAmmoType, true );
            }
#endif            
		}
	}

	return bRet;
}

bool CWeaponHgun::Reload( void )
{
	if ( m_flRechargeTime >= gpGlobals->curtime )
	{
		return true;
	}

	CHL1_Player *pPlayer = ToHL1Player( GetOwner() );
	if ( !pPlayer )
	{
		return true;
	}

#ifdef CLIENT_DLL
#else
	if ( !g_pGameRules->CanHaveAmmo( pPlayer, m_iPrimaryAmmoType ) )
		return true;

	while ( ( m_flRechargeTime < gpGlobals->curtime ) && g_pGameRules->CanHaveAmmo( pPlayer, m_iPrimaryAmmoType ) )
	{
		pPlayer->GiveAmmo( 1, m_iPrimaryAmmoType, true );
		m_flRechargeTime += 0.5;
	}
#endif

	return true;
}

void CWeaponHgun::ItemPostFrame( void )
{
	Reload();

	BaseClass::ItemPostFrame();
}
