//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Chargeable Plasma & Shield combo
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_combatshield.h"
#include "tf_guidedplasma.h"
#include "in_buttons.h"
#include "tf_gamerules.h"

#define BURST_FIRE_RATE			0.15

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombat_ChargeablePlasma : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombat_ChargeablePlasma, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_SERVERCLASS();

	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual void	Spawn();
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	Precache( void );
	virtual	void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual CBaseEntity *GetLockTarget( void );

private:
	CNetworkVar( bool, m_bCharging );
	float	m_flChargeStartTime;
	float	m_flPower;
	float	m_flNextBurstShotTime;
	int		m_iBurstShotsRemaining;
	bool	m_bHasBurstShot;
	bool	m_bHasCharge;

	// Guidance
	EHANDLE	m_hLockTarget;
	Vector	m_vecTargetOffset;
	float	m_flLockedAt;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponCombat_ChargeablePlasma, DT_WeaponCombat_ChargeablePlasma )
	SendPropInt( SENDINFO( m_bCharging ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_combat_chargeableplasma, CWeaponCombat_ChargeablePlasma );
PRECACHE_WEAPON_REGISTER(weapon_combat_chargeableplasma);


//-----------------------------------------------------------------------------
// Spawn weapon
//-----------------------------------------------------------------------------
void CWeaponCombat_ChargeablePlasma::Spawn()
{
	BaseClass::Spawn();
	m_bHasBurstShot = false;
	m_bHasCharge = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombat_ChargeablePlasma::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// New technologies: 
//-----------------------------------------------------------------------------
void CWeaponCombat_ChargeablePlasma::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	BaseClass::GainedNewTechnology( pTechnology );

	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( (CBaseEntity*)GetOwner() );
	if ( pPlayer )
	{
		// Charge-up mode?
		if ( pPlayer->HasNamedTechnology( "com_comboshield_charge" ) )
		{
			m_bHasCharge = true;
		}
		else
		{
			m_bHasCharge = false;
		}

		// Burst shot mode?
		if ( pPlayer->HasNamedTechnology( "com_comboshield_tripleshot" ) )
		{
			m_bHasBurstShot = true;
		}
		else
		{
			m_bHasBurstShot = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombat_ChargeablePlasma::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	// If burst shots are firing, ignore input
	if ( m_iBurstShotsRemaining > 0 )
	{
		if ( gpGlobals->curtime < m_flNextBurstShotTime )
			return;

		if ( m_iClip1 > 0 )
		{
			PrimaryAttack();
		}

		m_iBurstShotsRemaining--;
		m_flNextBurstShotTime = gpGlobals->curtime + BURST_FIRE_RATE;
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
		return;
	}

	// Handle charge firing
	if ( m_iClip1 > 0 && GetShieldState() == SS_DOWN && !m_bInReload )
	{
		if ( (pOwner->m_nButtons & IN_ATTACK ) )
		{
			if (m_bHasCharge)
			{
				if ( !m_bCharging && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
				{
					m_bCharging = true;
					m_flChargeStartTime = gpGlobals->curtime;

					// Get a lock target right now
					m_hLockTarget = GetLockTarget();
				}
			}
			else
			{
				// Fire the plasma shot
				if (m_flNextPrimaryAttack <= gpGlobals->curtime)
					PrimaryAttack();
			}
		}
		else if ( m_bCharging )
		{
			m_bCharging = false;

			// Fire the plasma shot
			PrimaryAttack();

			// We might be firing a burst shot
			if (m_bHasBurstShot)
			{
				if ( m_flPower >= (MAX_CHARGED_TIME * 0.5) )
				{
					if ( m_flPower >= MAX_CHARGED_TIME )
					{
						m_iBurstShotsRemaining = 2;
					}
					else
					{
						m_iBurstShotsRemaining = 1;
					}

					m_flNextBurstShotTime = gpGlobals->curtime + BURST_FIRE_RATE;
				}
			}
		}
	}

	// Reload button
	if ( m_iBurstShotsRemaining == 0 && !m_bCharging )
	{
		if ( pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload )
		{
			Reload();
		}
	}

	// Prevent shield post frame if we're not ready to attack, or we're charging
	AllowShieldPostFrame( !m_bCharging && ((m_flNextPrimaryAttack <= gpGlobals->curtime) || m_bInReload) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombat_ChargeablePlasma::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	WeaponSound(SINGLE);

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	// If we already have a lock target from button down, see if we shouldn't try and get a new one
	// Only do this is the button was released immediately
	if ( !m_hLockTarget || ( m_flLockedAt < gpGlobals->curtime ) )
	{
		m_hLockTarget = GetLockTarget();
	}

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Shift it down a bit so the firer can see it
	Vector right;
	AngleVectors( pPlayer->EyeAngles() + pPlayer->m_Local.m_vecPunchAngle, NULL, &right, NULL );
	Vector vecStartSpot = vecSrc + Vector(0,0,-8) + right * 12;

	CGuidedPlasma *pShot = CGuidedPlasma::Create(vecStartSpot, vecAiming, m_hLockTarget, m_vecTargetOffset, pPlayer);

	// Set it's charged power level
	if (m_bHasCharge)
		m_flPower = MIN( MAX_CHARGED_TIME, gpGlobals->curtime - m_flChargeStartTime );
	else
		m_flPower = 0.0f;

	float flDamageMult = RemapVal( m_flPower, 0, MAX_CHARGED_TIME, 1.0, MAX_CHARGED_POWER );
	pShot->SetPowerLevel( flDamageMult );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
	m_hLockTarget = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombat_ChargeablePlasma::GetFireRate( void )
{	
	return SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCombat_ChargeablePlasma::Deploy( void )
{
	if ( BaseClass::Deploy() )
	{
		GainedNewTechnology(NULL);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Our player just died
//-----------------------------------------------------------------------------
bool CWeaponCombat_ChargeablePlasma::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	bool bReturn = BaseClass::Holster(pSwitchingTo);

	// Stop the charging sound
	if ( m_bCharging )
	{
		m_bCharging = false;
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: Try and find an entity to lock onto
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponCombat_ChargeablePlasma::GetLockTarget( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if ( !pPlayer )
		return NULL;

	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );
	Vector vecEnd = vecSrc + vecAiming * MAX_TRACE_LENGTH;

	trace_t tr;
	TFGameRules()->WeaponTraceLine( vecSrc, vecEnd, MASK_SHOT, pPlayer, GetDamageType(), &tr );

	if ( (tr.fraction < 1.0f) && tr.m_pEnt )
	{
		CBaseEntity *pTargetEntity = tr.m_pEnt;

		// Don't guide on same team or on anything other than players, objects, and NPCs
		if ( pTargetEntity->InSameTeam(pPlayer) || (!pTargetEntity->IsPlayer() 
			&& (pTargetEntity->MyNPCPointer() == NULL)) )
			return NULL;

		// Compute the target offset relative to the target
		Vector vecWorldOffset;
		VectorSubtract( tr.endpos, pTargetEntity->GetAbsOrigin(), vecWorldOffset );
		VectorIRotate( vecWorldOffset, pTargetEntity->EntityToWorldTransform(), m_vecTargetOffset ); 
		m_flLockedAt = gpGlobals->curtime + 0.2;
		return pTargetEntity;
	}

	return NULL;
}
