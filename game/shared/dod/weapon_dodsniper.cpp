//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "fx_dod_shared.h"
#include "weapon_dodsniper.h"

#ifdef CLIENT_DLL
	#include "prediction.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( DODSniperWeapon, DT_SniperWeapon )

#ifdef CLIENT_DLL

	void RecvProxy_AnimStart( const CRecvProxyData *pData, void *pStruct, void *pOut )
	{
		CDODSniperWeapon *pWpn = (CDODSniperWeapon *) pStruct;
		pWpn->m_bDoViewAnim =  ( pData->m_Value.m_Int > 0 );
	}

#endif

BEGIN_NETWORK_TABLE( CDODSniperWeapon, DT_SniperWeapon )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bZoomed ) ),
		RecvPropInt( RECVINFO( m_bDoViewAnim ), 0, RecvProxy_AnimStart )
	#else
		SendPropBool( SENDINFO( m_bZoomed ) ),
		SendPropBool( SENDINFO( m_bDoViewAnim ) )
	#endif
END_NETWORK_TABLE()


#ifdef CLIENT_DLL

	BEGIN_PREDICTION_DATA( CDODSniperWeapon )
		DEFINE_PRED_FIELD( m_bZoomed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE )
	END_PREDICTION_DATA()

#else

	BEGIN_DATADESC( CDODSniperWeapon )
	END_DATADESC()

#endif


CDODSniperWeapon::CDODSniperWeapon()
{
}

void CDODSniperWeapon::Spawn( void )
{
	BaseClass::Spawn();

	ResetTimers();

#ifdef CLIENT_DLL
	m_bDoViewAnimCache = false;
	m_flZoomPercent = 0.0f;
#endif

	m_bShouldRezoomAfterShot = true;
}

void CDODSniperWeapon::ResetTimers( void )
{
	m_flUnzoomTime = -1;
	m_flRezoomTime = -1;
	m_flZoomOutTime = -1;
	m_flZoomInTime = -1;
	m_bRezoomAfterShot = false;
	m_bRezoomAfterReload = false;
}

bool CDODSniperWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	ZoomOut();	

	ResetTimers();
#endif

	return BaseClass::Holster( pSwitchingTo );
}

void CDODSniperWeapon::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();

	CDODPlayer *pPlayer = ToDODPlayer( GetOwner() );

	if ( IsZoomed() && ShouldZoomOutBetweenShots() )
	{
		//If we have more bullets, zoom out, play the bolt animation and zoom back in
		if( m_iClip1 > 0 && m_bShouldRezoomAfterShot && ( pPlayer && pPlayer->ShouldAutoRezoom() ) )
		{
			SetRezoom( true, 0.5f );	// zoom out in 0.5 seconds, then rezoom
		}
		else	//just zoom out
		{
			SetRezoom( false, 0.5f );	// just zoom out in 0.5 seconds
		}
	}

	// overwrite the next secondary attack, so we can zoom sooned after we've fired
	if ( m_bRezoomAfterShot && m_iClip1 > 0 )
		m_flNextSecondaryAttack = gpGlobals->curtime + 2.0;
	else
		m_flNextSecondaryAttack = SequenceDuration();
}

void CDODSniperWeapon::SecondaryAttack( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	Assert( pPlayer );

	if ( !pPlayer )
		return;

	ToggleZoom();
}

bool CDODSniperWeapon::Reload( void )
{
	bool bSuccess = BaseClass::Reload();

	if ( bSuccess && IsZoomed() )
	{
		//if ( ShouldRezoomAfterReload() )
		//	m_bRezoomAfterReload = true;

		ZoomOut();
	}

	if ( !bSuccess )
		m_bRezoomAfterReload = false;

	return bSuccess;
}

void CDODSniperWeapon::FinishReload( void )
{
	BaseClass::FinishReload();

	if ( m_bRezoomAfterReload )
	{
		ZoomIn();
		m_bRezoomAfterReload = false;
	}
}

float CDODSniperWeapon::GetWeaponAccuracy( float flPlayerSpeed )
{
	//snipers and deployable weapons inherit this and override when we need a different accuracy

	float flSpread = m_pWeaponInfo->m_flAccuracy;

	if ( IsZoomed() && ( gpGlobals->curtime - m_flZoomChangeTime ) > DOD_SNIPER_ZOOM_CHANGE_TIME )
	{
		flSpread = m_pWeaponInfo->m_flSecondaryAccuracy;
	}

	if( flPlayerSpeed > 45 )
		flSpread += m_pWeaponInfo->m_flAccuracyMovePenalty;

	return flSpread;
}

bool CDODSniperWeapon::IsZoomed( void )
{
	// check the player?
	return IsSniperZoomed();
}

bool CDODSniperWeapon::ShouldDrawCrosshair( void )
{
	//if ( IsFullyZoomed() )
	//	return false;

	// don't draw if we are zoomed at all
	if ( m_bZoomed )
		return false;

	return BaseClass::ShouldDrawCrosshair();
}

#include "in_buttons.h"

void CDODSniperWeapon::ItemPostFrame( void )
{
	if ( m_flUnzoomTime > 0 && gpGlobals->curtime > m_flUnzoomTime )
	{
		if ( m_bRezoomAfterShot )
		{
			ZoomOutIn();
			m_bRezoomAfterShot = false;
		}
		else
		{
			ZoomOut();
		}

		m_flUnzoomTime = -1;
	}

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	Assert( pPlayer );

	if ( m_flRezoomTime > 0 )
	{
		// if the player is sprinting and moving at all, cancel the rezoom
		if ( ( pPlayer->m_nButtons & IN_SPEED ) > 0 &&
			pPlayer->GetAbsVelocity().Length2D() > 20 )
		{
			m_flRezoomTime = -1;
		}
		else if ( gpGlobals->curtime > m_flRezoomTime )
		{
            ZoomIn();
			m_flRezoomTime = -1;
		}
	}

	if ( m_flZoomInTime > 0 && gpGlobals->curtime > m_flZoomInTime )
	{
#ifndef CLIENT_DLL
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
		Assert( pPlayer );
		pPlayer->SetFOV( pPlayer, GetZoomedFOV(), 0.1f );
#endif
		m_flZoomInTime = -1;
		m_flZoomOutTime = -1;
	}

	if ( m_flZoomInTime > 0 && (pPlayer->m_nButtons & IN_ATTACK) )
	{
		m_bInAttack = true;
	}

	BaseClass::ItemPostFrame();
}

void CDODSniperWeapon::Drop( const Vector &vecVelocity )
{
	// If a player is killed while deployed, this resets the weapon state
	if ( IsZoomed() )
		ZoomOut();

	ResetTimers();

	BaseClass::Drop( vecVelocity );
}

void CDODSniperWeapon::ToggleZoom( void )
{
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( !pPlayer->m_Shared.IsJumping() )
	{
		if( !IsZoomed() )
		{
			if ( CanAttack() )
			{
#ifndef CLIENT_DLL
				pPlayer->RemoveHintTimer( m_iAltFireHint );
#endif

				ZoomIn();
			}
		}
		else
		{
			ZoomOut();
		}
	}	
}

void CDODSniperWeapon::ZoomIn( void )
{
	m_flZoomInTime = gpGlobals->curtime + DOD_SNIPER_ZOOM_CHANGE_TIME;

#ifndef CLIENT_DLL
	SetZoomed( true );

	m_bDoViewAnim = !m_bDoViewAnim;
#endif

	m_flNextPrimaryAttack = MAX( gpGlobals->curtime + 0.5, m_flNextPrimaryAttack );
	m_flNextSecondaryAttack = MAX( gpGlobals->curtime + 0.5, m_flNextSecondaryAttack );
	m_flTimeWeaponIdle = gpGlobals->curtime + m_pWeaponInfo->m_flTimeToIdleAfterFire;

	m_flZoomChangeTime = gpGlobals->curtime;
}

void CDODSniperWeapon::ZoomOut( void )
{
	bool bWasZoomed = IsZoomed();

#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	Assert( pPlayer );

	pPlayer->SetFOV( pPlayer, 90, 0.1f );
	SetZoomed( false );

	m_bDoViewAnim = !m_bDoViewAnim;
#endif	

	if ( bWasZoomed )
	{
		m_flNextPrimaryAttack = MAX( gpGlobals->curtime + 0.5, m_flNextPrimaryAttack );
		m_flNextSecondaryAttack = MAX( gpGlobals->curtime + 0.5, m_flNextSecondaryAttack );
		m_flTimeWeaponIdle = gpGlobals->curtime + m_pWeaponInfo->m_flTimeToIdleAfterFire;
	}

	m_flZoomChangeTime = gpGlobals->curtime;

	// if we are thinking about zooming, cancel it
	m_flZoomInTime = -1;
	m_flUnzoomTime = -1;
	m_flRezoomTime = -1;
}

// reduce rezoome time, to account for fade in we're replacing with anim
#define REZOOM_TIME	1.2f

void CDODSniperWeapon::ZoomOutIn( void )
{
	ZoomOut();

	m_flRezoomTime = gpGlobals->curtime + 1.0;
}

void CDODSniperWeapon::SetRezoom( bool bRezoom, float flDelay )
{
	m_flUnzoomTime = gpGlobals->curtime + flDelay;

	m_bRezoomAfterShot = bRezoom;
}

bool CDODSniperWeapon::IsFullyZoomed( void )
{
	return ( IsZoomed() == true &&
		(( gpGlobals->curtime - m_flZoomChangeTime ) > DOD_SNIPER_ZOOM_CHANGE_TIME) );
}

bool CDODSniperWeapon::IsZoomingIn( void )
{
	return ( m_flZoomInTime > gpGlobals->curtime );
}

#ifdef CLIENT_DLL

	float CDODSniperWeapon::GetZoomedPercentage( void )
	{
		return m_flZoomPercent;
	}

	Vector CDODSniperWeapon::GetDesiredViewModelOffset( C_DODPlayer *pOwner )
	{
		static Vector vecLastResult = vec3_origin;

		if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		{
			return vecLastResult;
		}

		if ( m_bDoViewAnim != m_bDoViewAnimCache )
		{
			// start the anim timer 
			m_flViewAnimTimer = gpGlobals->curtime + DOD_SNIPER_ZOOM_CHANGE_TIME;
			m_bDoViewAnimCache = m_bDoViewAnim.m_Value;
		}

		Vector viewOffset = pOwner->GetViewOffset();

		float flPercent = clamp( ( viewOffset.z - VEC_PRONE_VIEW_SCALED( pOwner ).z ) / ( VEC_VIEW_SCALED( pOwner ).z - VEC_PRONE_VIEW_SCALED( pOwner ).z ), 0.0, 1.0 );

		Vector offset = ( flPercent * GetDODWpnData().m_vecViewNormalOffset +
			( 1.0 - flPercent ) * GetDODWpnData().m_vecViewProneOffset );

		
		// how long since we changed iron sight mode
		float flZoomAnimPercent = clamp( ( (m_flViewAnimTimer - gpGlobals->curtime) / ( DOD_SNIPER_ZOOM_CHANGE_TIME ) ), 0.0, 1.0 );

		m_flZoomPercent = ( IsZoomed() ? ( 1.0 - flZoomAnimPercent ) : flZoomAnimPercent );

		//Msg( "(%.1f) zoom %.2f %.2f\n", gpGlobals->curtime, m_flViewAnimTimer - gpGlobals->curtime, m_flZoomPercent );

		// use that percent to interp to iron sight position
		Vector vecResult = ( m_flZoomPercent * GetDODWpnData().m_vecIronSightOffset + 
			( 1.0 - m_flZoomPercent ) * offset );

		// store this value to use when called in prediction
		vecLastResult = vecResult;

		return vecResult;
	}

	float CDODSniperWeapon::GetViewModelSwayScale( void )
	{
		if ( IsFullyZoomed() )
			return 0;

		return BaseClass::GetViewModelSwayScale();
	}

#endif	//CLIENT_DLL
