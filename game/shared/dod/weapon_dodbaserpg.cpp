//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "dod_gamerules.h"
#include "weapon_dodbaserpg.h"


#ifdef CLIENT_DLL

	#include "c_dod_player.h"
	#include "prediction.h"

#else

	#include "dod_player.h"

#endif

IMPLEMENT_NETWORKCLASS_ALIASED( DODBaseRocketWeapon, DT_BaseRocketWeapon )

BEGIN_NETWORK_TABLE( CDODBaseRocketWeapon, DT_BaseRocketWeapon )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO(m_bDeployed) )
#else
	SendPropBool( SENDINFO(m_bDeployed) )
#endif

END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CDODBaseRocketWeapon )
END_PREDICTION_DATA()

#ifndef CLIENT_DLL

BEGIN_DATADESC( CDODBaseRocketWeapon )
END_DATADESC()

#endif

LINK_ENTITY_TO_CLASS( weapon_dodbaserpg, CDODBaseRocketWeapon );

CDODBaseRocketWeapon::CDODBaseRocketWeapon()
{
}

void CDODBaseRocketWeapon::Precache()
{
	BaseClass::Precache();
}

bool CDODBaseRocketWeapon::Reload( void )
{
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
	{
		CDODPlayer *pDODPlayer = ToDODPlayer( pPlayer );
		pDODPlayer->HintMessage( HINT_AMMO_EXHAUSTED );
		return false;
	}

	Activity actReload;

	if( IsDeployed() )
		actReload = ACT_VM_RELOAD_DEPLOYED;
	else
		actReload = ACT_VM_RELOAD;

	int iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), actReload );
	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	// if we don't want the auto-rezoom, undeploy here
	if ( !pPlayer->ShouldAutoRezoom() )
	{
		m_bDeployed = false;
		pPlayer->SetBazookaDeployed( m_bDeployed );
	}

	return true;
}

bool CDODBaseRocketWeapon::ShouldPlayerBeSlow( void )
{
	if( IsDeployed() && !m_bInReload )
	{
		return true;
	}
	else
		return false;
}

void CDODBaseRocketWeapon::Spawn( )
{
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );

	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );

	CDODWeaponInfo *pWeaponInfo = dynamic_cast< CDODWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

	Assert( pWeaponInfo && "Failed to get CDODWeaponInfo in weapon spawn" );
		
	m_pWeaponInfo = pWeaponInfo;

	BaseClass::Spawn();
}

void CDODBaseRocketWeapon::Drop( const Vector &vecVelocity )
{
	SetDeployed( false );

	BaseClass::Drop( vecVelocity );
}

bool CDODBaseRocketWeapon::Deploy( )
{
	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), GetDrawActivity(), (char*)GetAnimPrefix() );
}

Activity CDODBaseRocketWeapon::GetDrawActivity( void )
{
	return ACT_VM_DRAW;
}

bool CDODBaseRocketWeapon::CanHolster( void )
{
	// can't holster if we are delpoyed and not reloading
	if ( IsDeployed() && !m_bInReload )
		return false;

	return true;
}

bool CDODBaseRocketWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	pPlayer->SetBazookaDeployed( false );

#endif

	SetDeployed( false );

	return BaseClass::Holster(pSwitchingTo);
}


void CDODBaseRocketWeapon::PrimaryAttack()
{
	Assert( m_pWeaponInfo );

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	
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

	if( IsDeployed() )
	{
		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		
		FireRocket();

		DoFireEffects();

		m_iClip1--; 

#ifdef CLIENT_DLL
		if ( prediction->IsFirstTimePredicted() )
			pPlayer->DoRecoil( GetWeaponID(), GetRecoil() );
#endif

		if ( m_iClip1 <= 0 && pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 )
		{
			Lower();
		}

		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration() + 0.5;
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration() + 0.5;	//length of the fire anim!

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
	else
	{
#ifdef CLIENT_DLL
		pPlayer->HintMessage( HINT_SHOULDER_WEAPON, true );
#endif

		m_flNextPrimaryAttack = gpGlobals->curtime + 2.0f;
	}
}

void CDODBaseRocketWeapon::FireRocket( void )
{
	Assert( !"Derived classes must implement this." );
}

void CDODBaseRocketWeapon::DoFireEffects()
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	
	if ( pPlayer )
		 pPlayer->DoMuzzleFlash();

	//smoke etc
}

void CDODBaseRocketWeapon::SecondaryAttack()
{
	CBasePlayer *pPlayer = GetPlayerOwner();

	//if we're underwater, lower it
	if( pPlayer->GetWaterLevel() > 2 )
	{
		if( IsDeployed() )
			Lower();
		return;
	}

	if( IsDeployed() )
	{	
		Lower();
	}
	else
	{
		if ( CanAttack() )
            Raise();
	}	
}

void CDODBaseRocketWeapon::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	SendWeaponAnim( GetIdleActivity() );

	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

Activity CDODBaseRocketWeapon::GetIdleActivity( void )
{
	Activity actIdle;

	if( IsDeployed() )
		actIdle = ACT_VM_IDLE_DEPLOYED;
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}

/* Raise the Bazooka to your shoulder */
void CDODBaseRocketWeapon::Raise()
{
	//raise to the shoulder
	SendWeaponAnim( GetRaiseActivity() );

	m_bDeployed = true;

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

/* Lower the bazooka to running position */
bool CDODBaseRocketWeapon::Lower()
{
	SendWeaponAnim( GetLowerActivity() );

	m_bDeployed = false;
	
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	pPlayer->SetBazookaDeployed( m_bDeployed );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

	return true;
}

Activity CDODBaseRocketWeapon::GetLowerActivity( void )
{
	return ACT_VM_UNDEPLOY;
}

Activity CDODBaseRocketWeapon::GetRaiseActivity( void )
{
	return ACT_VM_DEPLOY;
}

#ifdef CLIENT_DLL

ConVar deployed_bazooka_sensitivity( "deployed_bazooka_sensitivity", "0.6", FCVAR_CHEAT, "Mouse sensitivity while deploying a bazooka" );

void CDODBaseRocketWeapon::OverrideMouseInput( float *x, float *y )
{
	if( m_bDeployed )
	{
		float flSensitivity = deployed_bazooka_sensitivity.GetFloat();

		*x *= flSensitivity;
		*y *= flSensitivity;		
	}
}

#endif