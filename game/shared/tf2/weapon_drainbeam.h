//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_DRAINBEAM_H
#define WEAPON_DRAINBEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"

#if defined( CLIENT_DLL )
#define CWeaponDrainBeam C_WeaponDrainBeam
#endif

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponDrainBeam : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponDrainBeam, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponDrainBeam( void );

	virtual void	Precache();

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	WeaponIdle( void );

	virtual void	PlayAttackAnimation( int activity );
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	// Draining
	CBaseEntity		*GetTargetToDrain( CBaseEntity *pCurDrainTarget );
	CBaseEntity		*GetCurDrainTarget( void )	{ return m_hDrainTarget; }
	void			RemoveDrainTarget( void );

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
	void			StopDrainSound( bool bStopHealingSound = true, bool bStopNoTargetSound = true );
// IClientNetworkable.
public:
	virtual void	OnDataChanged( DataUpdateType_t updateType );
// IClientThinkable.
public:

	virtual void	ClientThink();

#endif

public:
	CNetworkHandle( CBaseEntity, m_hDrainTarget );

// Networked data.
public:
	CNetworkVar( bool, m_bDraining );
	CNetworkVar( bool, m_bAttacking );
	CNetworkVector( m_vFireTarget );

#if defined( CLIENT_DLL )
	CSmartPtr<CSimpleEmitter> m_pEmitter;
	PMaterialHandle			m_hParticleMaterial;

	TimedEvent				m_PathParticleEvent;
	TimedEvent				m_DribbleParticleEvent;

	bool					m_bPlayingSound;
#else
	float					m_flDrainStartedAt;
#endif
private:
	double					m_flNextBuzzTime;
	
	CWeaponDrainBeam( const CWeaponDrainBeam & );
};

#endif // WEAPON_DRAINBEAM_H
