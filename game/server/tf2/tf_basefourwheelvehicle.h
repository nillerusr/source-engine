//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A base class that deals with four-wheel vehicles
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_BASE_FOUR_WHEEL_VEHICLE_H
#define TF_BASE_FOUR_WHEEL_VEHICLE_H

#ifdef _WIN32
#pragma once
#endif

#include "basetfvehicle.h"
#include "vphysics/vehicles.h"
#include "fourwheelvehiclephysics.h"
#include "tf_vehicleshared.h"

class CMoveData;

class CBaseTFFourWheelVehicle : public CBaseTFVehicle
{
public:
	DECLARE_CLASS( CBaseTFFourWheelVehicle, CBaseTFVehicle );
	DECLARE_SERVERCLASS();

public:
	CBaseTFFourWheelVehicle();
	~CBaseTFFourWheelVehicle ();
			 
	// CBaseEntity
	void Spawn();
	void Precache();
	void VPhysicsUpdate( IPhysicsObject *pPhysics );
	void DrawDebugGeometryOverlays();
	int DrawDebugTextOverlays();
	void Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity );
	void BaseFourWheeledVehicleThink();
	void BaseFourWheeledVehicleDeployThink( void );

	// HACK HACK:  This is a hack to avoid physics spazzing out on a newly created vehicle with the handbrake
	//  set.  We create and activate it, but then release the handbrake for a single Think function call and 
	//  then zero out the controls right then.  This seems to stabilize something in the physics simulator.
	void BaseFourWheeledVehicleStopTheRodeoMadnessThink( void );

	virtual bool StartBuilding( CBaseEntity *pPlayer );
	virtual void FinishedBuilding( void );

	virtual void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData );
	virtual void SetPassenger( int nRole, CBasePlayer *pEnt );

	// Powerup handling
	virtual void PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual	void PowerupEnd( int iPowerup );

	// Collision
	virtual void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

	// Inputs
	void InputThrottle( inputdata_t &inputdata );
	void InputSteering( inputdata_t &inputdata );
	void InputAction( inputdata_t &inputdata );
	void InputTurnOn( inputdata_t &inputdata );
	void InputTurnOff( inputdata_t &inputdata );

	// Boost
	void			SetBoostUpgrade( bool bBoostUpgrade ); 
	bool			IsBoostable( void );
	bool			IsBoosting( void );
	void			StartBoost( void );

	bool			IsDeployed( void ) { return ( m_eDeployMode == VEHICLE_MODE_DEPLOYED ); }
	bool			IsDeploying( void ) { return ( m_eDeployMode == VEHICLE_MODE_DEPLOYING ); }
	bool			IsUndeploying( void ) { return ( m_eDeployMode == VEHICLE_MODE_UNDEPLOYING ); }
	bool			InDeployMode( void ) { return ( m_eDeployMode != VEHICLE_MODE_NORMAL ); }

	DECLARE_DATADESC();

	// locals
protected:
	// engine sounds
	void SoundInit();
	void SoundShutdown();
	void SoundUpdate( const vehicle_operatingparams_t &params, const vehicleparams_t &vehicle );
	void CalcWheelData( vehicleparams_t &vehicle );
	void ResetControls();

	// Deploy
	bool			Deploy( void );
	void			UnDeploy( void );
	void			CancelDeploy( void );
	virtual void	OnFinishedDeploy( void );
	virtual void	OnFinishedUnDeploy( void );

private:
	void DriveVehicle( CBasePlayer *pPlayer, CUserCmd *ucmd );
	void PlayerControlInit( CBasePlayer *pPlayer );
	void PlayerControlShutdown();
	void ResetUseKey( CBasePlayer *pPlayer );
	void InitializePoseParameters();
	bool ParseVehicleScript( solid_t &solid, vehicleparams_t &vehicle );

	void EnableMotion( void );
	void DisableMotion( void );

private:
	CFourWheelVehiclePhysics	m_VehiclePhysics;
	COutputEvent				m_playerOn;
	COutputEvent				m_playerOff;

	COutputEvent				m_pressedAttack;
	COutputEvent				m_pressedAttack2;

	COutputFloat				m_attackaxis;
	COutputFloat				m_attack2axis;

	int							m_nMovementRole;
	Vector						m_savedViewOffset; //[MAX_PASSENGERS];
	
	float						m_flNextEmpSound;

	// Deploy
	CNetworkVar( VehicleModeDeploy_e, m_eDeployMode );
	Activity					m_PreDeployActivity;

	// Used for vgui screens on the client.
	CNetworkVar( float, m_flDeployFinishTime );
	CNetworkVar( bool, m_bBoostUpgrade );
	CNetworkVar( int, m_nBoostTimeLeft );

	float						m_flNextHitTime;
};



#endif // TF_BASE_FOUR_WHEEL_VEHICLE_H
