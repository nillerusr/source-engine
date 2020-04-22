//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A base vehicle class
//
//=============================================================================//

#ifndef BASETFVEHICLE_H
#define BASETFVEHICLE_H

#ifdef _WIN32
#pragma once
#endif

#include "basetfplayer_shared.h"
#include "baseobject_shared.h"
#include "tf_obj_basedrivergun_shared.h"

#if defined( CLIENT_DLL )
#include "iclientvehicle.h"
#else
#include "IServerVehicle.h"
#endif


class CMoveData;
class CUserCmd;
class CBasePlayer;

#if defined( CLIENT_DLL )
#define CBaseTFVehicle C_BaseTFVehicle
#endif

class CBaseTFVehicle;
class CBaseObjectDriverGun;

struct VehicleBaseMoveData_t
{
	CBaseTFVehicle	*m_pVehicle;
};

// ------------------------------------------------------------------------ //
// The base class that all vehicles in tf2 will derive from
// ------------------------------------------------------------------------ //
#if defined( CLIENT_DLL )
class CBaseTFVehicle : public CBaseObject, public IClientVehicle
#else
class CBaseTFVehicle : public CBaseObject, public IServerVehicle
#endif
{
	DECLARE_CLASS( CBaseTFVehicle, CBaseObject );

public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseTFVehicle();

#if !defined (CLIENT_DLL)
	// CBaseEntity overrides
	virtual void	FinishedBuilding( void );
	virtual void	DestroyObject( );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual bool	UseAttachedItem( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const;

	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );

	// IVehicle overrides
	virtual IServerVehicle*	GetServerVehicle() { return this; }

	virtual CBaseEntity* GetVehicleEnt();

	// Get and set the current driver.
	virtual void SetPassenger( int nRole, CBasePlayer *pEnt );

	// Where do we get out of the vehicle?
	virtual bool GetPassengerExitPoint( int nRole, Vector *pExitPoint, QAngle *pAngles );

	virtual Class_T			ClassifyPassenger( CBasePlayer *pPassenger, Class_T defaultClassification ) { return defaultClassification; }
	virtual float			DamageModifier ( CTakeDamageInfo &info ) { return 1.0; }
	virtual const vehicleparams_t	*GetVehicleParams( void ) { return NULL; }

	virtual bool			IsVehicleUpright( void ) { return true; }
	virtual bool			IsPassengerEntering( void ) { Assert( 0 ); return true; }
	virtual bool			IsPassengerExiting( void ) { Assert( 0 ); return true; }

	// NPC Driving
	virtual bool			NPC_CanDrive( void ) { return true; }
	virtual void			NPC_SetDriver( CNPC_VehicleDriver *pDriver ) { return; }
	virtual void			NPC_DriveVehicle( void ) { return; }
	virtual void			NPC_ThrottleCenter( void ) { return; }
	virtual void			NPC_ThrottleReverse( void ) { return; }
	virtual void			NPC_ThrottleForward( void ) { return; }
	virtual void			NPC_Brake( void ) { return; }
	virtual void			NPC_TurnLeft( float flDegrees ) { return; }
	virtual void			NPC_TurnRight( float flDegrees ) { return; }
	virtual void			NPC_TurnCenter( void ) { return; }
	virtual void			NPC_PrimaryFire( void ) { return; }
	virtual void			NPC_SecondaryFire( void ) { return; }
	virtual bool			NPC_HasPrimaryWeapon( void ) { return false; }
	virtual bool			NPC_HasSecondaryWeapon( void ) { return false; }
	virtual void			NPC_AimPrimaryWeapon( Vector vecTarget ) { return; }
	virtual void			NPC_AimSecondaryWeapon( Vector vecTarget ) { return; }

	// Weapon handling
	virtual void			Weapon_PrimaryRanges( float *flMinRange, float *flMaxRange ) { *flMinRange = 0; *flMaxRange = 0; }
	virtual void			Weapon_SecondaryRanges( float *flMinRange, float *flMaxRange ) { *flMinRange = 0; *flMaxRange = 0; }
	virtual float			Weapon_PrimaryCanFireAt( void ) { return gpGlobals->curtime; }		// Return the time at which this vehicle's primary weapon can fire again
	virtual float			Weapon_SecondaryCanFireAt( void ) { return gpGlobals->curtime; }	// Return the time at which this vehicle's secondary weapon can fire again

	// Vehicles dont want to attach to anything they're built upon
	virtual bool ShouldAttachToParent( void ) { return false; }

	virtual bool	MustNotBeBuiltInConstructionYard( void ) const { return false; }

	// Purpose: Try to board the vehicle
	void AttemptToBoardVehicle( CBaseTFPlayer *pPlayer );

	// Figure out which role of a parent vehicle this vehicle is sitting in..
	int GetParentVehicleRole();

	// Purpose: 
	void GetPassengerExitPoint( CBasePlayer *pPlayer, int nRole, Vector *pAbsPosition, QAngle *pAbsAngles );
	int	 GetEntryAnimForPoint( const Vector &vecPoint );
	int  GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked );
	void HandleEntryExitFinish( bool bExitAnimOn, bool bResetAnim );
	void HandlePassengerEntry( CBasePlayer *pPlayer, bool bAllowEntryOutsideZone = false );
	bool HandlePassengerExit( CBasePlayer *pPlayer );

	// Deterioration
	void	VehicleDeteriorationThink( void );
	void	VehiclePassengerThink( void );

#endif

	bool	IsReadyToDrive( void );

	virtual bool	IsAVehicle( void ) { return true; }

	// Get a position in *local space* inside the vehicle for the player to start at
	virtual void GetPassengerStartPoint( int nRole, Vector *pPoint, QAngle *pAngles );

#if defined( CLIENT_DLL )
	// C_BaseEntity overrides
	virtual IClientVehicle*	GetClientVehicle() { return this; }

	virtual C_BaseEntity	*GetVehicleEnt();

	virtual void ClientThink();

	// Fills in the unperterbed view position for a particular role.

	// Prediction
	virtual bool	ShouldPredict( void );
	virtual bool	IsPredicted( void ) const { return true; }

	// IClientVehicle

	// Called at time player enters vehicle
	virtual void	GetVehicleFOV( float &flFOV ) { return; }
	virtual void	DrawHudElements( void );

	// Get the angles that a player in the specified role should be using for visuals
	virtual QAngle	GetPassengerAngles( QAngle angCurrent, int nRole );

	// Allows the vehicle to restrict view angles
	virtual void	UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd ) {}
	virtual void	GetVehicleClipPlanes( float &flZNear, float &flZFar ) const {}

	bool			IsBoostable( void )		{ return m_bBoostUpgrade; }

	// Hud
	virtual void	DrawHudBoostData( void );
	virtual void	SetupCrosshair( void );

#endif

	int		LocateEntryPoint( CBaseTFPlayer *pPlayer, float* fBest2dDistanceSqr= NULL );

	// This lets the object decide whether or not it wants to use the ThirdPersonCameraOrigin attachment for its view.
	// The manned guns use first-person when they're on the ground and third-person when they're in a vehicle.
	virtual bool	ShouldUseThirdPersonVehicleView();
	virtual void	GetVehicleViewPosition( int nRole, Vector *pOrigin, QAngle *pAngles, float *pFOV = NULL );
	virtual bool	GetRoleViewPosition( int nRole, Vector *pVehicleEyeOrigin, QAngle *pVehicleEyeAngles );
	virtual bool	GetRoleAbsViewPosition( int nRole, Vector *pAbsVehicleEyeOrigin, QAngle *pAbsVehicleEyeAngles );

	// Can a given passenger take damage?
	virtual bool	IsPassengerDamagable( int nRole ) { return (nRole != VEHICLE_DRIVER); }


	virtual void	Spawn();
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove ) {}
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );
	virtual void	ItemPostFrame( CBasePlayer *pPassenger );

	virtual CBasePlayer* GetPassenger( int nRole = VEHICLE_DRIVER );
	virtual int		GetPassengerRole( CBasePlayer *pEnt );

	// Does the player use his normal weapons while in this mode?
	virtual bool	IsPassengerUsingStandardWeapons( int nRole = VEHICLE_DRIVER ) { return false; }

	virtual Vector GetSoundEmissionOrigin() const;

	// Returns the driver as a tfplayer pointer
	CBaseTFPlayer *GetDriverPlayer();
	
	int	GetMaxPassengerCount() const;
	int GetPassengerCount() const;

	// Is a particular player in the vehicle?
	bool IsPlayerInVehicle( CBaseTFPlayer *pPlayer );

	void	ResetDeteriorationTime( void );

	// Driver controlled guns
	void	SetDriverGun( CBaseObjectDriverGun *pGun );
	void	VehicleDriverGunThink( void );

protected:
	enum
	{
		MAX_PASSENGERS = 4,
		MAX_PASSENGER_BITS = 3
	};

	// Can we get into the vehicle?
	virtual bool CanGetInVehicle( CBaseTFPlayer *pPlayer );

	// Here's where we deal with weapons
	virtual void OnItemPostFrame( CBaseTFPlayer *pDriver );

	// Specify the number of roles we can have
	void SetMaxPassengerCount( int nMaxPassengers );

	bool	IsValidExitPoint( int nRole, Vector *pExitPoint, QAngle *pAngles );
	int 	GetEmptyRole( void );

private:
#if !defined (CLIENT_DLL)
	// Get the parent vehicle of this vehicle..
	CBaseTFVehicle *GetParentVehicle();

	// Get a position in *world space* inside the vehicle for the player to exit at
	void GetInitialPassengerExitPoint( int nRole, Vector *pAbsPoint, QAngle *pAbsAngles );

	// Figure out which role of a vehicle a child vehicle is sitting in..
	int GetChildVehicleRole( CBaseTFVehicle *pChild );
#endif

private:
	typedef CHandle<CBaseTFPlayer> CPlayerHandle;
	CNetworkArray( CPlayerHandle, m_hPassengers, MAX_PASSENGERS );
	CNetworkVar( int, m_nMaxPassengers );

	// Driver controlled gun
	CNetworkHandle( CBaseObjectDriverGun, m_hDriverGun );

#if defined( CLIENT_DLL )

	CHudTexture			*m_pIconDefaultCrosshair;

	bool			m_bBoostUpgrade;
	int				m_nBoostTimeLeft;

private:
	CBaseTFVehicle( const CBaseTFVehicle & ); // not defined, not accessible
#endif
};

#endif // BASETFVEHICLE_H
