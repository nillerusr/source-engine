//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponSG550 C_WeaponSG550
	#include "c_cs_player.h"

#else

	#include "cs_player.h"
	#include "KeyValues.h"

#endif


class CWeaponSG550 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponSG550, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSG550();

	virtual void Spawn();
	virtual void SecondaryAttack();
	virtual void PrimaryAttack();
	virtual bool Reload();
	virtual bool Deploy();

 	virtual float GetInaccuracy() const;
	virtual float GetMaxSpeed() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_SG550; }


private:
	CWeaponSG550( const CWeaponSG550 & );

	float m_flLastFire;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSG550, DT_WeaponSG550 )

BEGIN_NETWORK_TABLE( CWeaponSG550, DT_WeaponSG550 )
END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponSG550 )
	DEFINE_FIELD( m_flLastFire, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_sg550, CWeaponSG550 );
PRECACHE_WEAPON_REGISTER( weapon_sg550 );



CWeaponSG550::CWeaponSG550()
{
	m_flLastFire = gpGlobals->curtime;
}

void CWeaponSG550::Spawn()
{
	BaseClass::Spawn();
	m_flAccuracy = 0.98;
}


void CWeaponSG550::SecondaryAttack()
{
	const float kZoomTime = 0.10f;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if (pPlayer->GetFOV() == pPlayer->GetDefaultFOV())
	{
		pPlayer->SetFOV( pPlayer, 40, kZoomTime );
		m_weaponMode = Secondary_Mode;
		m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyAltSwitch;
	}
	else if (pPlayer->GetFOV() == 40)
	{
		pPlayer->SetFOV( pPlayer, 15, kZoomTime );
		m_weaponMode = Secondary_Mode;
	}
	else if (pPlayer->GetFOV() == 15)
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
		GetPlayerOwner()->EmitSound( "Default.Zoom" ); // zoom sound.
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

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
	m_zoomFullyActiveTime = gpGlobals->curtime + 0.3; // The worst zoom time from above.  
}

float CWeaponSG550::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		float fSpread = 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			fSpread = 0.45f * (1 - m_flAccuracy);
		else if (pPlayer->GetAbsVelocity().Length2D() > 5)
			fSpread = 0.15f;
		else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
			fSpread = 0.04f * (1 - m_flAccuracy);
		else
			fSpread = 0.05f * (1 - m_flAccuracy);
	
		// If we are not zoomed in, or we have very recently zoomed and are still transitioning, the bullet diverts more.
		if (pPlayer->GetFOV() == pPlayer->GetDefaultFOV() || (gpGlobals->curtime < m_zoomFullyActiveTime))
			fSpread += 0.025;
	
		return fSpread;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponSG550::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	m_flAccuracy = 0.65 + (0.35) * (gpGlobals->curtime - m_flLastFire);	

	if (m_flAccuracy > 0.98)
		m_flAccuracy = 0.98;

	m_flLastFire = gpGlobals->curtime;

	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, m_weaponMode ) )
		return;

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= SharedRandomFloat("SG550PunchAngleX", 0.75, 1.25 ) + ( angle.x / 4 );
	angle.y += SharedRandomFloat("SG550PunchAngleY", -0.75, 0.75 );
	pPlayer->SetPunchAngle( angle );
}

bool CWeaponSG550::Reload()
{
	bool ret = BaseClass::Reload();
	
	m_flAccuracy = 0.98;
	m_weaponMode = Primary_Mode;
	
	return ret;
}

bool CWeaponSG550::Deploy()
{
	bool ret = BaseClass::Deploy();
	
	m_flAccuracy = 0.98;
	m_weaponMode = Primary_Mode;
	
	return ret;
}

float CWeaponSG550::GetMaxSpeed() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer || pPlayer->GetFOV() == 90 )
		return BaseClass::GetMaxSpeed();
	else
		return 150; // zoomed in
}
