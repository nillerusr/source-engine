//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponScout C_WeaponScout
	#include "c_cs_player.h"

#else

	#include "cs_player.h"
	#include "KeyValues.h"

#endif

const int cScoutMidZoomFOV = 40;
const int cScoutMaxZoomFOV = 15;


class CWeaponScout : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponScout, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponScout();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();

 	virtual float GetInaccuracy() const;
	virtual float GetMaxSpeed() const;
	virtual bool Reload();
	virtual bool Deploy();

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_SCOUT; }


private:
	
	CWeaponScout( const CWeaponScout & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponScout, DT_WeaponScout )

BEGIN_NETWORK_TABLE( CWeaponScout, DT_WeaponScout )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponScout )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_scout, CWeaponScout );
PRECACHE_WEAPON_REGISTER( weapon_scout );



CWeaponScout::CWeaponScout()
{
}

void CWeaponScout::SecondaryAttack()
{
	const float kZoomTime = 0.10f;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if (pPlayer == NULL)
	{
		Assert(pPlayer != NULL);
		return;
	}

	if (pPlayer->GetFOV() == pPlayer->GetDefaultFOV())
	{
		pPlayer->SetFOV( pPlayer, cScoutMidZoomFOV, kZoomTime );
		m_weaponMode = Secondary_Mode;
		m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyAltSwitch;
	}
	else if (pPlayer->GetFOV() == cScoutMidZoomFOV)
	{
		pPlayer->SetFOV( pPlayer, cScoutMaxZoomFOV, kZoomTime );
		m_weaponMode = Secondary_Mode;
	}
	else if (pPlayer->GetFOV() == cScoutMaxZoomFOV)
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), kZoomTime );
		m_weaponMode = Primary_Mode;
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;   
	m_zoomFullyActiveTime = gpGlobals->curtime + 0.15; // The worst zoom time from above.  

#ifndef CLIENT_DLL
	// If this isn't guarded, the sound will be emitted twice, once by the server and once by the client.
	// Let the server play it since if only the client plays it, it's liable to get played twice cause of
	// a prediction error. joy.	
	
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Playing this from the player so that we don't try to play the sound outside the level.
	//=============================================================================
	if ( GetPlayerOwner() )
	{
		GetPlayerOwner()->EmitSound( "Default.Zoom" ); // zoom sound
	}
	//=============================================================================
	// HPE_END
	//=============================================================================
	// let the bots hear the rifle zoom
	IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom" );
	if( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}
#endif
}

float CWeaponScout::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if (pPlayer == NULL)
			return 0.0f;
	
		float fSpread = 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			fSpread = 0.2f;
		else if (pPlayer->GetAbsVelocity().Length2D() > 170)
			fSpread = 0.075f;
		else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
			fSpread = 0.0f;
		else
			fSpread = 0.007f;
	
		// If we are not zoomed in, or we have very recently zoomed and are still transitioning, the bullet diverts more.
		if (pPlayer->GetFOV() == pPlayer->GetDefaultFOV() || (gpGlobals->curtime < m_zoomFullyActiveTime))
		{
			fSpread += 0.025;
		}
	
		return fSpread;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponScout::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if (pPlayer == NULL)
		return;

	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, m_weaponMode ) )
		return;

	if ( m_weaponMode == Secondary_Mode )
	{	
		float	midFOVdistance = fabs( pPlayer->GetFOV() - (float)cScoutMidZoomFOV );
		float	farFOVdistance = fabs( pPlayer->GetFOV() - (float)cScoutMaxZoomFOV );

		if ( midFOVdistance < farFOVdistance )
		{
			pPlayer->m_iLastZoom = cScoutMidZoomFOV;
		}
		else
		{
			pPlayer->m_iLastZoom = cScoutMaxZoomFOV;
		}
		
// 		#ifndef CLIENT_DLL
			pPlayer->m_bResumeZoom = true;
			pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.05f );
			m_weaponMode = Primary_Mode;
// 		#endif
	}

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= 2;
	pPlayer->SetPunchAngle( angle );
}


float CWeaponScout::GetMaxSpeed() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if (pPlayer == NULL)
	{
		Assert(pPlayer != NULL);
		return BaseClass::GetMaxSpeed();
	}

	if ( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() )
		return BaseClass::GetMaxSpeed();
	else
		return 220;	// zoomed in.
}


bool CWeaponScout::Reload()
{
	m_weaponMode = Primary_Mode;
	return BaseClass::Reload();

}

bool CWeaponScout::Deploy()
{
	m_weaponMode = Primary_Mode;
	return BaseClass::Deploy();
}
