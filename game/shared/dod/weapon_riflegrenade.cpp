//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "fx_dod_shared.h"
#include "weapon_riflegrenade.h"

#ifdef CLIENT_DLL
	#include "prediction.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseRifleGrenade, DT_WeaponBaseRifleGrenade )

BEGIN_NETWORK_TABLE( CWeaponBaseRifleGrenade, DT_WeaponBaseRifleGrenade )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponBaseRifleGrenade )
END_PREDICTION_DATA()


CWeaponBaseRifleGrenade::CWeaponBaseRifleGrenade()
{
}

void CWeaponBaseRifleGrenade::Spawn()
{
	BaseClass::Spawn();
}

void CWeaponBaseRifleGrenade::PrimaryAttack( void )
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
		return;
	}

	if( pPlayer->GetWaterLevel() > 2 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
		return;
	}

	// decrement before calling PlayPrimaryAttackAnim, so we can play the empty anim if necessary
	m_iClip1--;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifdef CLIENT_DLL
	if( pPlayer && !pPlayer->IsDormant() )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#else
	if( pPlayer )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#endif	

#ifndef CLIENT_DLL
	ShootGrenade();
#endif

	WeaponSound( SINGLE );

	DoFireEffects();

#ifdef CLIENT_DLL
	CDODPlayer *p = ToDODPlayer( GetPlayerOwner() );

	if ( prediction->IsFirstTimePredicted() )
		p->DoRecoil( GetWeaponID(), GetRecoil() );
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetFireDelay();
	m_flTimeWeaponIdle = gpGlobals->curtime + m_pWeaponInfo->m_flTimeToIdleAfterFire;

#ifndef CLIENT_DLL
	IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_weapon_attack" );
	if ( event )
	{
		event->SetInt( "attacker", pPlayer->GetUserID() );
		event->SetInt( "weapon", GetStatsWeaponID() );

		gameeventmanager->FireEvent( event );
	}
#endif	//CLIENT_DLL
}

Activity CWeaponBaseRifleGrenade::GetDrawActivity( void )
{
	return ( (m_iClip1 <= 0 ) ? ACT_VM_DRAW_EMPTY : ACT_VM_DRAW );
}

Activity CWeaponBaseRifleGrenade::GetIdleActivity( void )
{
	return ( (m_iClip1 <= 0 ) ? ACT_VM_IDLE_EMPTY : ACT_VM_IDLE );
}

bool CWeaponBaseRifleGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	// If they attempt to switch weapons before the throw animation is done, 
	// allow it, but kill the weapon if we have to.
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if( pPlayer && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 && m_iClip1 <= 0 )
	{
		pPlayer->Weapon_Drop( this, NULL, NULL );
		UTIL_Remove(this);
		return true;	
	}
#endif

	return BaseClass::Holster( pSwitchingTo );
}

// weapon idle to destroy weapon if empty
void CWeaponBaseRifleGrenade::WeaponIdle( void )
{
#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 && m_iClip1 <= 0)
	{
		pPlayer->Weapon_Drop( this, NULL, NULL );
		UTIL_Remove(this);
		return;
	}
#endif

	BaseClass::WeaponIdle();
}

#define DOD_RIFLEGRENADE_SPEED	2000
extern ConVar dod_grenadegravity;

#ifndef CLIENT_DLL

	void CWeaponBaseRifleGrenade::ShootGrenade( void )
	{
		CDODPlayer *pPlayer = ToDODPlayer( GetOwner() );
		if ( !pPlayer )
		{
			Assert( false );
			return;
		}

		QAngle angThrow = pPlayer->LocalEyeAngles();

		Vector vForward, vRight, vUp;
		AngleVectors( angThrow, &vForward, &vRight, &vUp );

		Vector eyes = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
		Vector vecSrc = eyes; 	
		Vector vecThrow = vForward * DOD_RIFLEGRENADE_SPEED;

		// raise origin enough so that you can't shoot it straight down through the world
		if ( pPlayer->m_Shared.IsProne() )
		{
			vecSrc.z += 16;
		}

		SetCollisionGroup( COLLISION_GROUP_WEAPON );

		QAngle angles;
		VectorAngles( -vecThrow, angles );

		// estimate angular velocity based on initial conditions
		// uses a constant ang velocity instead of the derivative of the flight path, oh well.
		AngularImpulse angImpulse;
		angImpulse.Init();

		if ( vecThrow.z > 0 )
            angImpulse.y = -angles.x / ( sqrt( (-2 * vecThrow.z) / dod_grenadegravity.GetFloat() ));
		else
			angImpulse.y = -10;
		
		EmitGrenade( vecSrc, angles, vecThrow, angImpulse, pPlayer, RIFLEGRENADE_FUSE_LENGTH );
	}

	void CWeaponBaseRifleGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime )
	{
		Assert( !"Deriving classes should define this" );
	}

#endif

bool CWeaponBaseRifleGrenade::CanDeploy( void )
{
	// does the player own the weapon that fires this grenade?
	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return false;

	CBaseCombatWeapon *pOwnedWeapon = pPlayer->Weapon_OwnsThisType( GetCompanionWeaponName() );

	if ( pOwnedWeapon == NULL )
		return false;

	return BaseClass::CanDeploy();
}

const char *CWeaponBaseRifleGrenade::GetCompanionWeaponName( void )
{
	Assert( !"Should not call this version. Inheriting classes must implement." );
	return "";
}

float CWeaponBaseRifleGrenade::GetRecoil( void )
{
	CDODPlayer *p = ToDODPlayer( GetPlayerOwner() );

	if( p && p->m_Shared.IsInMGDeploy() )
	{
		return 0.0f;
	}

	return 10;
}