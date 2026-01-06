//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VEHICLE_DRIVABLE_APC_H
#define VEHICLE_DRIVABLE_APC_H
#ifdef _WIN32
#pragma once
#endif

#include "vehicle_base.h"
#include "weapon_rpg.h"
#include "Sprite.h"
#include "npcevent.h"
#include "beam_shared.h"

#define APC_WHEEL_COUNT	4

//-----------------------------------------------------------------------------
// Purpose: Four wheel physics vehicle server vehicle with weaponry
//-----------------------------------------------------------------------------
class CDrivableAPCFourWheelServerVehicle : public CFourWheelServerVehicle
{
	typedef CFourWheelServerVehicle BaseClass;
// IServerVehicle
public:
	bool		NPC_HasPrimaryWeapon( void ) { return true; }
	void		NPC_AimPrimaryWeapon( Vector vecTarget );
	int			GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPropDrivableAPC : public CPropVehicleDriveable
{
	DECLARE_CLASS( CPropDrivableAPC, CPropVehicleDriveable );

public:

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPropDrivableAPC( void );

	// CPropVehicle
	virtual void	ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData );
	virtual void	DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased );
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );
	virtual bool	AllowBlockedExit( CBasePlayer *pPlayer, int nRole ) { return false; }
	virtual bool	CanExitVehicle( CBaseEntity *pEntity );

	//virtual Class_T Classify(); 

	// CBaseEntity
	void			Think(void);
	void			Precache( void );
	void			Spawn( void ); 
	void			Activate( void );
	void			OnRestore( void );
	void			UpdateOnRemove( void );

#ifdef EZ2
	// Required for NPC sensing. Hopefully causes no side effects
	bool			IsAlive() { return true; }
#endif

	virtual void	CreateServerVehicle( void );
	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	EnterVehicle( CBasePlayer *pPlayer );
	virtual void	ExitVehicle( int nRole );

	void			AimGunAt( Vector *endPos, float flInterval );

	const Vector	&GetPrimaryGunOrigin( void );
	void			AimPrimaryWeapon( const Vector &vecForward );
	void			AimSecondaryWeaponAt( CBaseEntity *pTarget );
	float			PrimaryWeaponFireTime( void ) { return m_flMachineGunTime; }
	float			SecondaryWeaponFireTime( void ) { return m_flRocketTime; }

	void			InputForcePlayerOut( inputdata_t &inputdata );

	void			InputDisableMove( inputdata_t &inputdata );
	void			InputEnableMove( inputdata_t &inputdata ); 

#ifdef EZ
	void			InputHeadlightOn( inputdata_t &inputdata );
	void			InputHeadlightOff( inputdata_t &inputdata );
	void			InputConstrainEntity( inputdata_t &inputdata );
	void			InputUnconstrainEntity( inputdata_t &inputdata );
	void			InputConstraintBroken( inputdata_t &inputdata );
	void			InputResetConstraintToDefault( inputdata_t &inputdata );
	void			ConstrainEntity( CBaseEntity *pEntity, CBasePlayer *pPlayer = NULL );
	void			UnconstrainEntity( bool bBroken = false, CBasePlayer *pPlayer = NULL );
#endif

	// Muzzle flashes
	const char		*GetTracerType( void ) ;
	void			DoImpactEffect( trace_t &tr, int nDamageType );
	void			DoMuzzleFlash( void );


	//const char		*GetTracerType( void ) { return "AR2Tracer"; }

	bool			HeadlightIsOn( void ) { return m_bHeadlightIsOn; }
	void			HeadlightTurnOn( void );
	void			HeadlightTurnOff( void );

	virtual int		ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_NOTIFY_ON_TRANSITION); }

protected:
	virtual float			GetUprightTime( void ) { return 1.0f; }
	virtual float			GetUprightStrength( void );

private:

	void			GetCannonAim( Vector *resultDir );

	void			UpdateSteeringAngle( void );
	void			CreateDangerSounds( void );

	void			ComputePDControllerCoefficients( float *pCoefficientsOut, float flFrequency, float flDampening, float flDeltaTime );
	void			DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void			DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );

//APC:
	
	void			FireMachineGun( void );
	void			FireRocket( void );

	void			GetRocketShootPosition( Vector *pPosition );

	void			CreateAPCLaserDot( void );

private:

	float			m_flDangerSoundTime;
	Vector			m_vecGunOrigin;
	CSoundPatch		*m_sndCannonCharge;
	float			m_aimYaw;
	float			m_aimPitch;
	float			m_throttleDisableTime;

	// handbrake after the fact to keep vehicles from rolling
	float			m_flHandbrakeTime;
	bool			m_bInitialHandbrake;

	float			m_flOverturnedTime;

	Vector			m_vecLastEyePos;
	Vector			m_vecLastEyeTarget;
	Vector			m_vecEyeSpeed;
	Vector			m_vecTargetSpeed;

	int				m_iNumberOfEntries;

	// Seagull perching
	float			m_flPlayerExitedTime;	// Time at which the player last left this vehicle
	float			m_flLastSawPlayerAt;	// Time at which we last saw the player
	EHANDLE			m_hLastPlayerInVehicle;

	CNetworkVar( bool, m_bHeadlightIsOn );

#ifdef EZ
	// Blixibon - APC spotlight
	EHANDLE			m_Spotlight;
	int				m_nSpotlightAttachment;

	// Blixibon - Top attach
	int				m_nTopAttachAttachment;
	IPhysicsConstraint		*m_pConstraint;
	EHANDLE			m_hConstrainedEntity;
	float			m_flConstrainCooldown;
#endif


//APC:

	// Machine gun attacks
	int		m_nMachineGunMuzzleAttachment;
	int		m_nMachineGunBaseAttachment;
	float	m_flMachineGunTime;
	float	m_flMachineGunReloadTime;

	CNetworkVar( bool, m_bIsMounted );
	CNetworkHandle( CBaseEntity,	m_hTarget );
	CNetworkVar( int, m_iTargetType );
	float m_flTargetSelectTime;

	CNetworkVar( int, m_iMachineGunBurstLeft );

	// Rocket attacks
	EHANDLE				m_hLaserDot;
	EHANDLE				m_hRocketTarget;
	float m_flLaserTargetTime;

	CNetworkVar( int,m_iRocketSalvoLeft );

	float	m_flRocketTime;
	float	m_flRocketReloadTime;
	int		m_nRocketAttachment;
	int		m_nRocketSide;

	float	m_flViewLowered;

	float	m_flNextPropAttackTime;
	float	m_flStopBreakTime;
	bool	m_bShouldAttackProps;

	bool	m_bCannotMove;

//	bool	m_bForcePlayerOut;

#ifdef EZ
	COutputEvent		m_onOverturned;
	COutputEvent		m_onObjectAttached;
#endif
};



#endif // VEHICLE_DRIVABLE_APC_H
