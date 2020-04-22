//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_FLAME_THROWER_H
#define WEAPON_FLAME_THROWER_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"

#if defined( CLIENT_DLL )
	#define CWeaponFlameThrower C_WeaponFlameThrower

	#include "particlemgr.h"
	#include "particle_util.h"
	#include "particles_simple.h"

#else

	class CGasolineBlob;

#endif

//=========================================================
// Medikit Weapon
//=========================================================
class CWeaponFlameThrower : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponFlameThrower, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponFlameThrower();
	CWeaponFlameThrower( bool bCanister );
	~CWeaponFlameThrower();

	virtual void			Precache();

	// The gas canister says yes so we can do different things.
	bool					IsGasCanister() const;

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const;
	virtual void			ItemPostFrame();


#if defined( CLIENT_DLL )

	virtual bool	ShouldPredict( void );

	virtual void			NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void			ClientThink();

	// Start/stop the fire sound.
	void					StartSound();
	void					StopFlameSound();
	bool					m_bSoundOn;	// Is the sound on?

	CSmartPtr<CSimpleEmitter>	m_hFlameEmitter;
	PMaterialHandle				m_hFireMaterial;
	TimedEvent					m_FlameEvent;

#else

	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo );
	void					IgniteNearbyGasolineBlobs();

private:

	// Used to link the blobs together.
	CHandle<CGasolineBlob>	m_hPrevBlob;


#endif


private:

	void		PrimaryAttack();
	void		InternalConstructor( bool bCanister );

	CNetworkVar( bool, m_bFiring );
	bool		m_bCanister;	// Tells if we're a gas canister or a flamethrower (both act in similar ways).
	float		m_flNextPrimaryAttack;

	CWeaponFlameThrower( const CWeaponFlameThrower & );
};

#endif // WEAPON_FLAME_THROWER_H
