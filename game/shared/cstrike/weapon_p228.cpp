//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "fx_cs_shared.h"


#if defined( CLIENT_DLL )

	#define CWeaponP228 C_WeaponP228
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponP228 : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponP228, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponP228();

	virtual void Spawn();

	virtual void PrimaryAttack();
	virtual bool Deploy();

	virtual bool Reload();
	virtual void WeaponIdle();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_P228; }

private:
	
	CWeaponP228( const CWeaponP228 & );

	float m_flLastFire;
};

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponP228 )
	DEFINE_FIELD( m_flLastFire, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_p228, CWeaponP228 );
PRECACHE_WEAPON_REGISTER( weapon_p228 );



CWeaponP228::CWeaponP228()
{
	m_flLastFire = gpGlobals->curtime;
}


void CWeaponP228::Spawn( )
{
	m_flAccuracy = 0.9;
	
	BaseClass::Spawn();
}


bool CWeaponP228::Deploy( )
{
	m_flAccuracy = 0.9;

	return BaseClass::Deploy();
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponP228, DT_WeaponP228 )

BEGIN_NETWORK_TABLE( CWeaponP228, DT_WeaponP228 )
END_NETWORK_TABLE()


float CWeaponP228::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;

		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 1.5f * (1 - m_flAccuracy);
		else if (pPlayer->GetAbsVelocity().Length2D() > 5)
			return 0.255f * (1 - m_flAccuracy);
		else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
			return 0.075f * (1 - m_flAccuracy);
		else
			return 0.15f * (1 - m_flAccuracy);
	}
	else
		return BaseClass::GetInaccuracy();
}


void CWeaponP228::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	m_flAccuracy -= (0.3)*(0.325 - (gpGlobals->curtime - m_flLastFire));

	if (m_flAccuracy > 0.9)
		m_flAccuracy = 0.9;
	else if (m_flAccuracy < 0.6)
		m_flAccuracy = 0.6;

	m_flLastFire = gpGlobals->curtime;
	
	if (m_iClip1 <= 0)
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.1f;
			m_bFireOnEmpty = false;
		}

		return;
	}

	pPlayer->m_iShotsFired++;

	m_iClip1--;
	
	 pPlayer->DoMuzzleFlash();
	//SetPlayerShieldAnim();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
	// Aiming
	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetInaccuracy(),
		GetSpread());
	
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetCSWpnData().m_flCycleTime;

	if (!m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}

	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	//ResetPlayerShieldAnim();

	// update accuracy
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyImpulseFire[Primary_Mode];

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= 2;
	pPlayer->SetPunchAngle( angle );
}


bool CWeaponP228::Reload()
{
	if ( !DefaultPistolReload() )
		return false;

	m_flAccuracy = 0.9;
	return true;
}

void CWeaponP228::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if (m_iClip1 != 0)
	{	
		SetWeaponIdleTime( gpGlobals->curtime + 3.0 ) ;
		SendWeaponAnim( ACT_VM_IDLE );
	}
}
