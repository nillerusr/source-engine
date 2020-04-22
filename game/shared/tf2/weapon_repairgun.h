//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_REPAIRGUN_H
#define WEAPON_REPAIRGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"

#if defined( CLIENT_DLL )
#define CWeaponRepairGun C_WeaponRepairGun
#endif

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponRepairGun : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponRepairGun, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponRepairGun( void );

	virtual void	Precache();

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	WeaponIdle( void );
	virtual void	PlayAttackAnimation( int activity );
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual bool	ComputeEMPFireState( void );

	virtual float	GetTargetRange( void );
	virtual float	GetStickRange( void );
	virtual float	GetHealRate( void );
	virtual bool	AppliesModifier( void ) { return true; }
	virtual bool	TargetsPlayers( void ) { return true; }

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	// Stop all sounds being output.
	void			StopRepairSound( bool bStopHealingSound = true, bool bStopNoTargetSound = true );

	// This is used to stop making particles when the entity goes dormant.
	virtual void	NotifyShouldTransmit( ShouldTransmitState_t state );
// IClientNetworkable.
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );
// IClientThinkable.
public:

	virtual void	ClientThink();

#endif

protected:
	virtual CBaseEntity		*GetTargetToHeal( CBaseEntity *pCurHealing );
	virtual CBaseEntity		*CheckVehicleTargets( CBaseEntity *pCurHealing );

private:
	CBaseEntity*			GetCurHealingTarget()	{ return m_hHealingTarget; }
	void					RemoveHealingTarget();

	double					m_flNextBuzzTime;

	float					m_flHealEffectLifetime;	// Count down until the healing effect goes off.
#if !defined( CLIENT_DLL )
	CDamageModifier			m_DamageModifier;		// This attaches to whoever we're healing.
#endif
	CNetworkVar( bool, m_bAttacking );

protected:
// Networked data.
	CNetworkVar( bool, m_bHealing );
	CNetworkHandle( CBaseEntity, m_hHealingTarget );

#if defined( CLIENT_DLL )
	CSmartPtr<CSimpleEmitter> m_pEmitter;
	PMaterialHandle			m_hParticleMaterial;

	TimedEvent				m_PathParticleEvent;

	bool					m_bPlayingSound;

#endif
private:														
	CWeaponRepairGun( const CWeaponRepairGun & );
};

#endif // WEAPON_REPAIRGUN_H
