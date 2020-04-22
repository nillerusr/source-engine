//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegun.h"
#include "fx_dod_shared.h"
//#include "effect_dispatch_data.h"

#ifdef CLIENT_DLL
	#include "c_dod_player.h"
	#include "prediction.h"
#else
	#include "dod_player.h"
	#include "te_effect_dispatch.h"
	#include "util.h"
	#include "ndebugoverlay.h"
#endif

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Only send to local player if this weapon is the active weapon
// Input  : *pStruct - 
//			*pVarData - 
//			*pRecipients - 
//			objectID - 
// Output : void*
//-----------------------------------------------------------------------------
void* SendProxy_SendActiveLocalBaseGunDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Get the weapon entity
	CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon*)pVarData;
	if ( pWeapon )
	{
		// Only send this chunk of data to the player carrying this weapon
		CBasePlayer *pPlayer = ToBasePlayer( pWeapon->GetOwner() );
		if ( pPlayer )
		{
			pRecipients->SetOnly( pPlayer->GetClientIndex() );
			return (void*)pVarData;
		}
	}

	return NULL;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendActiveLocalBaseGunDataTable );
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDODBaseGun, DT_WeaponDODBaseGun )

BEGIN_NETWORK_TABLE_NOBASE( CWeaponDODBaseGun, DT_LocalActiveWeaponBaseGunData )
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CWeaponDODBaseGun, DT_WeaponDODBaseGun )
	#ifdef CLIENT_DLL
		RecvPropDataTable("LocalActiveWeaponBaseGunData", 0, 0, &REFERENCE_RECV_TABLE(DT_LocalActiveWeaponBaseGunData))
	#else
		SendPropDataTable("LocalActiveWeaponBaseGunData", 0, &REFERENCE_SEND_TABLE(DT_LocalActiveWeaponBaseGunData), SendProxy_SendActiveLocalBaseGunDataTable )
	#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL

	BEGIN_PREDICTION_DATA( CWeaponDODBaseGun )
	END_PREDICTION_DATA()

#else

	BEGIN_DATADESC( CWeaponDODBaseGun )
	END_DATADESC()

#endif

LINK_ENTITY_TO_CLASS( weapon_dod_base_gun, CWeaponDODBaseGun );

extern ConVar friendlyfire;

CWeaponDODBaseGun::CWeaponDODBaseGun()
{
}


void CWeaponDODBaseGun::Spawn()
{
	DODBaseGunSpawn();

	SetZoomed( false );
}

void CWeaponDODBaseGun::Precache()
{
	BaseClass::Precache();

	// Precache all weapon ejections, since every weapon will appear in the game.
	PrecacheModel( "models/shells/shell_small.mdl" );
	PrecacheModel( "models/shells/shell_medium.mdl" );
	PrecacheModel( "models/shells/shell_large.mdl" );
	PrecacheModel( "models/shells/garand_clip.mdl" );
}

void CWeaponDODBaseGun::PrimaryAttack()
{
	Assert( m_pWeaponInfo );

	DODBaseGunFire();
}

void CWeaponDODBaseGun::DODBaseGunSpawn( void )
{
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );

	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );

	CDODWeaponInfo *pWeaponInfo = dynamic_cast< CDODWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

	Assert( pWeaponInfo && "Failed to get CDODWeaponInfo in weapon spawn" );
		
	m_pWeaponInfo = pWeaponInfo;

	BaseClass::Spawn();
}

float CWeaponDODBaseGun::GetWeaponAccuracy( float flPlayerSpeed )
{
	//snipers and deployable weapons inherit this and override when we need a different accuracy

	float flSpread = m_pWeaponInfo->m_flAccuracy;

	if( flPlayerSpeed > 45 )
		flSpread += m_pWeaponInfo->m_flAccuracyMovePenalty;

	return flSpread;
}

bool CWeaponDODBaseGun::DODBaseGunFire()
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	Assert( pPlayer );
	
	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		if (m_bFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}

		return false;
	}

	if( pPlayer->GetWaterLevel() > 2 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
		return false;
	}
	
	/* decrement before calling PlayPrimaryAttackAnim, so we can play the empty anim if necessary */
	m_iClip1--;

	SendWeaponAnim( GetPrimaryAttackActivity() );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + pPlayer->GetPunchAngle(),
		GetWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetWeaponAccuracy( pPlayer->GetAbsVelocity().Length2D() ) );

	DoFireEffects();

#ifndef CLIENT_DLL
	IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_weapon_attack" );
	if ( event )
	{
		event->SetInt( "attacker", pPlayer->GetUserID() );
		event->SetInt( "weapon", GetStatsWeaponID() );

		gameeventmanager->FireEvent( event );
	}
#endif	//CLIENT_DLL

#ifdef CLIENT_DLL
	CDODPlayer *p = ToDODPlayer( GetPlayerOwner() );

	if ( prediction->IsFirstTimePredicted() )
		p->DoRecoil( GetWeaponID(), GetRecoil() );
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetFireDelay();

	m_flTimeWeaponIdle = gpGlobals->curtime + m_pWeaponInfo->m_flTimeToIdleAfterFire;
	return true;
}

float CWeaponDODBaseGun::GetFireDelay( void )
{
	return m_pWeaponInfo->m_flFireDelay;
}

void CWeaponDODBaseGun::DoFireEffects()
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	
	if ( pPlayer )
		 pPlayer->DoMuzzleFlash();
}


bool CWeaponDODBaseGun::Reload()
{
	CBasePlayer *pPlayer = GetPlayerOwner();

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 && m_iClip1 <= 0 )
	{
		CDODPlayer *pDODPlayer = ToDODPlayer( pPlayer );
		pDODPlayer->HintMessage( HINT_AMMO_EXHAUSTED );
		return false;
	}

	int iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), GetReloadActivity() );
	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	return true;
}

Activity CWeaponDODBaseGun::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

Activity CWeaponDODBaseGun::GetReloadActivity( void )
{
	return ACT_VM_RELOAD;
}

Activity CWeaponDODBaseGun::GetDrawActivity( void )
{
	return ACT_VM_DRAW;
}
