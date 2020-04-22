//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "fx_cs_shared.h"

#if defined( CLIENT_DLL )

	#define CWeaponFiveSeven C_WeaponFiveSeven
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponFiveSeven : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponFiveSeven, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponFiveSeven();

	virtual void Spawn();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Deploy();

	virtual bool Reload();

	virtual void WeaponIdle();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_FIVESEVEN; }

private:
	
	CWeaponFiveSeven( const CWeaponFiveSeven & );
	
	float m_flLastFire;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFiveSeven, DT_WeaponFiveSeven )

BEGIN_NETWORK_TABLE( CWeaponFiveSeven, DT_WeaponFiveSeven )
END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponFiveSeven )
	DEFINE_FIELD( m_flLastFire, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_fiveseven, CWeaponFiveSeven );
PRECACHE_WEAPON_REGISTER( weapon_fiveseven );



CWeaponFiveSeven::CWeaponFiveSeven()
{
	m_flLastFire = gpGlobals->curtime;
}


void CWeaponFiveSeven::Spawn( )
{
	BaseClass::Spawn();

	m_flAccuracy = 0.92;
}

bool CWeaponFiveSeven::Deploy()
{
	m_flAccuracy = 0.92;
	return BaseClass::Deploy();
}


float CWeaponFiveSeven::GetInaccuracy() const
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

void CWeaponFiveSeven::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	m_flAccuracy -= (0.25)*(0.275 - (gpGlobals->curtime - m_flLastFire));

	if (m_flAccuracy > 0.92)
		m_flAccuracy = 0.92;
	else if (m_flAccuracy < 0.725)
		m_flAccuracy = 0.725;

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

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	
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

	if (!m_iClip1 && pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}

	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	// update accuracy
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyImpulseFire[Primary_Mode];

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= 2;
	pPlayer->SetPunchAngle( angle );
}


void CWeaponFiveSeven::SecondaryAttack() 
{
}


bool CWeaponFiveSeven::Reload()
{
	if ( !DefaultPistolReload() )
		return false;

	m_flAccuracy = 0.92;
	return true;
}

void CWeaponFiveSeven::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if (m_iClip1 != 0)
	{	
		SetWeaponIdleTime( gpGlobals->curtime + 4 );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}
