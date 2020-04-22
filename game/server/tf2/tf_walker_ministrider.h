//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_WALKER_MINISTRIDER_H
#define TF_WALKER_MINISTRIDER_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_walker_base.h"


class CBeam;


class CWalkerMiniStrider : public CWalkerBase
{
public:
	DECLARE_CLASS( CWalkerMiniStrider, CWalkerBase );
	DECLARE_SERVERCLASS();

	CWalkerMiniStrider();
	virtual ~CWalkerMiniStrider();


// CWalkerBase.
protected:
	virtual void WalkerThink();
	virtual Vector GetWalkerLocalMovement();


// CBaseObject.
public:
	virtual bool StartBuilding( CBaseEntity *pBuilder );
	

// CBaseEntity.
public:	
	virtual void Precache();
	virtual void Spawn();
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );


// CBaseAnimating.
public:

	virtual void HandleAnimEvent( animevent_t *pEvent );


// IServerVehicle.
public:	
	virtual bool IsPassengerVisible( int nRole );
	virtual void SetPassenger( int nRole, CBasePlayer *pPassenger );


// IVehicle overrides.
public:
	virtual void SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );


private:
	void FootHit( const char *pFootName );

	void StartFiringMachineGun();
	void StopFiringMachineGun();
	void FireMachineGun();

	Vector GetLargeGunShootOrigin();
	void StartFiringLargeGun();
	void StopFiringLargeGun();
	void UpdateLargeGun();

	void Crouch();
	void UnCrouch();
	void UpdateCrouch();


private:
	
	enum
	{
		STATE_CROUCHING=0,
		STATE_CROUCHED,
		STATE_UNCROUCHING,
		STATE_UNCROUCHED,
		STATE_NORMAL
	};

	int m_State;

	float m_flCrouchTimer;

	CNetworkVar( bool, m_bFiringMachineGun );
	CNetworkVar( bool, m_bFiringLargeGun );
	CNetworkVector( m_vLargeGunTargetPos );
	float m_flLargeGunCountdown;
	Vector m_vLargeGunForward;
	CHandle<CBeam> m_pEnergyBeam;

	// Firing
	float	m_flNextShootTime;
	bool	m_bFiringLeftGun;

	// Used to keep him on the ground.
	float m_flOriginToLowestLegHeight;
	float m_flWantedZ;

	// Used to get around an anim event bug where it triggers events a bunch of times when an animation loops.
	float m_flNextFootstepSoundTime;
};	


#endif // TF_WALKER_MINISTRIDER_H
