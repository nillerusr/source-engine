//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "fx_dod_shared.h"
#include "weapon_dodfireselect.h"

#ifdef CLIENT_DLL
	#include "prediction.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( DODFireSelectWeapon, DT_FireSelectWeapon )


BEGIN_NETWORK_TABLE( CDODFireSelectWeapon, DT_FireSelectWeapon )
#ifdef CLIENT_DLL	
	RecvPropBool( RECVINFO( m_bSemiAuto ) )
#else
	SendPropBool( SENDINFO( m_bSemiAuto ) )
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CDODFireSelectWeapon )
END_PREDICTION_DATA()


CDODFireSelectWeapon::CDODFireSelectWeapon()
{
}

void CDODFireSelectWeapon::Spawn( void )
{
	m_bSemiAuto = false;

#ifdef CLIENT_DLL
	ResetViewModelAnimDir();
#endif

	m_iAltFireHint = HINT_USE_SEMI_AUTO;

	BaseClass::Spawn();
}

void CDODFireSelectWeapon::PrimaryAttack( void )
{
	if ( IsSemiAuto() )
	{
		// If semi auto, set this flag which will prevent us from
		// attacking again until the button is released.
		m_bInAttack = true;
	}

	BaseClass::PrimaryAttack();
}

float CDODFireSelectWeapon::GetFireDelay( void )
{
	if ( IsSemiAuto() )
	{
		return m_pWeaponInfo->m_flSecondaryFireDelay;
	}
	else
	{
		return m_pWeaponInfo->m_flFireDelay;
	}	
}

void CDODFireSelectWeapon::SecondaryAttack( void )
{
	// toggle fire mode ( full auto, semi auto )
	m_bSemiAuto = !m_bSemiAuto;

#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();
	if ( pPlayer )
	{
		pPlayer->RemoveHintTimer( m_iAltFireHint );
	}
#endif

	if ( m_bSemiAuto )
	{
#ifdef CLIENT_DLL
		if ( prediction->IsFirstTimePredicted() )
		{
			m_flPosChangeTimer = gpGlobals->curtime;
			m_bAnimToSemiAuto = false;
		}
#endif

		SendWeaponAnim( ACT_VM_UNDEPLOY );
	}
	else
	{
#ifdef CLIENT_DLL
		if ( prediction->IsFirstTimePredicted() )
		{
			m_flPosChangeTimer = gpGlobals->curtime;
			m_bAnimToSemiAuto = true;
		}
#endif

		SendWeaponAnim( ACT_VM_DEPLOY );
	}

#ifdef CLIENT_DLL
	if ( prediction->IsFirstTimePredicted() )
	{
		m_flPosChangeTimer = gpGlobals->curtime;
	}
#endif

	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

bool CDODFireSelectWeapon::IsSemiAuto( void )
{
	return m_bSemiAuto;
}

float CDODFireSelectWeapon::GetWeaponAccuracy( float flPlayerSpeed )
{
	float flSpread;
	
	if ( IsSemiAuto() )
		flSpread = m_pWeaponInfo->m_flSecondaryAccuracy;
	else
		flSpread = m_pWeaponInfo->m_flAccuracy;

	if( flPlayerSpeed > 45 )
		flSpread += m_pWeaponInfo->m_flAccuracyMovePenalty;

	return flSpread;
}

Activity CDODFireSelectWeapon::GetIdleActivity( void )
{
	if ( !IsSemiAuto() )
		return ACT_VM_IDLE_DEPLOYED;

	return BaseClass::GetIdleActivity();
}

Activity CDODFireSelectWeapon::GetPrimaryAttackActivity( void )
{
	if ( !IsSemiAuto() )
		return ACT_VM_PRIMARYATTACK_DEPLOYED;

	return BaseClass::GetPrimaryAttackActivity();
}

Activity CDODFireSelectWeapon::GetReloadActivity( void )
{
	if ( !IsSemiAuto() )
		return ACT_VM_RELOAD_DEPLOYED;

	return BaseClass::GetReloadActivity();
}

Activity CDODFireSelectWeapon::GetDrawActivity( void )
{
	if ( !IsSemiAuto() )
		return ACT_VM_DRAW_DEPLOYED;

	return BaseClass::GetDrawActivity();
}

void CDODFireSelectWeapon::Drop( const Vector &vecVelocity )
{
	// always full auto when you pick up a weapon
	m_bSemiAuto = false;

	return BaseClass::Drop( vecVelocity );
}

#ifdef CLIENT_DLL
	Vector CDODFireSelectWeapon::GetDesiredViewModelOffset( C_DODPlayer *pOwner )
	{
		Vector viewOffset = pOwner->GetViewOffset();

		float flPercent = ( viewOffset.z - VEC_PRONE_VIEW_SCALED( pOwner ).z ) / ( VEC_VIEW_SCALED( pOwner ).z - VEC_PRONE_VIEW_SCALED( pOwner ).z );

		Vector offset = ( flPercent * GetDODWpnData().m_vecViewNormalOffset +
			( 1.0 - flPercent ) * GetDODWpnData().m_vecViewProneOffset );

		static float flLastPercent = 0;

		if ( prediction->InPrediction() )
		{
			return ( flLastPercent * offset +
				( 1.0 - flLastPercent ) * GetDODWpnData().m_vecViewProneOffset );
		}

		float timer = gpGlobals->curtime - m_flPosChangeTimer;

		// how long since we changed iron sight mode
		float flPosChangePercent = clamp( ( timer / ( 0.3 ) ), 0.0, 1.0 );

		float flZoomPercent = ( m_bAnimToSemiAuto ? ( 1.0 - flPosChangePercent ) : flPosChangePercent );

		// store this value to use when called in prediction
		flLastPercent = flZoomPercent;

		return ( flZoomPercent * GetDODWpnData().m_vecIronSightOffset +
			( 1.0 - flZoomPercent ) * offset );
	}
#endif
