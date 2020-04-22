//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponAWP C_WeaponAWP
	#include "c_cs_player.h"

#else

	#include "cs_player.h"
	#include "KeyValues.h"

#endif

#define SNIPER_ZOOM_CONTEXT		"SniperRifleThink"

const int cAWPMidZoomFOV = 40;
const int cAWPMaxZoomFOV = 10;

#ifdef AWP_UNZOOM
	ConVar sv_awpunzoomdelay( 
			"sv_awpunzoomdelay",
			"1.0",
			0,
			"how many seconds to zoom the zoom up after firing",
			true, 0,	// min value
			false, 0	// max value
			);
#endif


class CWeaponAWP : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponAWP, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
	
	CWeaponAWP();

	virtual void Spawn();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();

 	virtual float GetInaccuracy() const;
	virtual float GetMaxSpeed() const;
	virtual bool IsAwp() const;
	virtual bool Reload();
	virtual bool Deploy();

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_AWP; }

private:

#ifdef AWP_UNZOOM
	void				UnzoomThink( void );
#endif

	CWeaponAWP( const CWeaponAWP & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponAWP, DT_WeaponAWP )

BEGIN_NETWORK_TABLE( CWeaponAWP, DT_WeaponAWP )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponAWP )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_awp, CWeaponAWP );
PRECACHE_WEAPON_REGISTER( weapon_awp );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CWeaponAWP )
#ifdef AWP_UNZOOM
		DEFINE_THINKFUNC( UnzoomThink ),
#endif
	END_DATADESC()

#endif

CWeaponAWP::CWeaponAWP()
{
}

void CWeaponAWP::Spawn()
{
	Precache();

	BaseClass::Spawn();
}


void CWeaponAWP::SecondaryAttack()
{
	const float kZoomTime = 0.10f;

	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer == NULL )
	{
		Assert( pPlayer != NULL );
		return;
	}

	if ( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() )
	{
			pPlayer->SetFOV( pPlayer, cAWPMidZoomFOV, kZoomTime );
			m_weaponMode = Secondary_Mode;
			m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyAltSwitch;
	}
	else if ( pPlayer->GetFOV() == cAWPMidZoomFOV )
	{
			pPlayer->SetFOV( pPlayer, cAWPMaxZoomFOV, kZoomTime );
			m_weaponMode = Secondary_Mode;
	}
	else
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), kZoomTime );
		m_weaponMode = Primary_Mode;
	}


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
		GetPlayerOwner()->EmitSound( "Default.Zoom" );
	}
	//=============================================================================
	// HPE_END
	//=============================================================================
	// let the bots hear the rifle zoom
	IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}
#endif

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
	m_zoomFullyActiveTime = gpGlobals->curtime + 0.15; // The worst zoom time from above.  

}

float CWeaponAWP::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		float fSpread = 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			fSpread = 0.85f;
	
		else if ( pPlayer->GetAbsVelocity().Length2D() > 140 )
			fSpread = 0.25f;
	
		else if ( pPlayer->GetAbsVelocity().Length2D() > 10 )
			fSpread = 0.10f;
	
		else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
			fSpread = 0.0f;
	
		else
			fSpread = 0.001f;
	
		// If we are not zoomed in, or we have very recently zoomed and are still transitioning, the bullet diverts more.
		if (pPlayer->GetFOV() == pPlayer->GetDefaultFOV() || (gpGlobals->curtime < m_zoomFullyActiveTime))
		{
			fSpread += 0.08f;
		}
	
		return fSpread;
	}
	else
	{
		return BaseClass::GetInaccuracy();
	}
}

void CWeaponAWP::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, m_weaponMode ) )
		return;

	if ( m_weaponMode == Secondary_Mode )
	{	
		float	midFOVdistance = fabs( pPlayer->GetFOV() - (float)cAWPMidZoomFOV );
		float	farFOVdistance = fabs( pPlayer->GetFOV() - (float)cAWPMaxZoomFOV );
		if ( midFOVdistance < farFOVdistance )
		{
			pPlayer->m_iLastZoom = cAWPMidZoomFOV;
		}
		else
		{
			pPlayer->m_iLastZoom = cAWPMaxZoomFOV;
		}
		
		#ifdef AWP_UNZOOM
			SetContextThink( &CWeaponAWP::UnzoomThink, gpGlobals->curtime + sv_awpunzoomdelay.GetFloat(), SNIPER_ZOOM_CONTEXT );
		#else
			pPlayer->m_bResumeZoom = true;
			pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.1f );
			m_weaponMode = Primary_Mode;
		#endif
	}

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= 2;
	pPlayer->SetPunchAngle( angle );
}

#ifdef AWP_UNZOOM
void CWeaponAWP::UnzoomThink( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if (pPlayer == NULL)
	{
		Assert(pPlayer != NULL);
		return;
	}

	pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.1f );
}
#endif


float CWeaponAWP::GetMaxSpeed() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if (pPlayer == NULL)
	{
		Assert(pPlayer != NULL);
		return BaseClass::GetMaxSpeed();
	}

	if ( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() )
	{
		return BaseClass::GetMaxSpeed();
	}
	else
	{
		// Slower speed when zoomed in.
		return 150;
	}
}


bool CWeaponAWP::IsAwp() const
{
	return true;
}


bool CWeaponAWP::Reload()
{
	m_weaponMode = Primary_Mode;
	return BaseClass::Reload();
}

bool CWeaponAWP::Deploy()
{
	// don't allow weapon switching to shortcut cycle time (quickswitch exploit)
	float fOldNextPrimaryAttack	= m_flNextPrimaryAttack;
	float fOldNextSecondaryAttack = m_flNextSecondaryAttack;

	if ( !BaseClass::Deploy() )
		return false;

	m_weaponMode = Primary_Mode;
	m_flNextPrimaryAttack	= MAX( m_flNextPrimaryAttack, fOldNextPrimaryAttack );
	m_flNextSecondaryAttack	= MAX( m_flNextSecondaryAttack, fOldNextSecondaryAttack );
	return true;
}
