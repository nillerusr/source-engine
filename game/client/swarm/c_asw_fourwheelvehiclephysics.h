// Clientside version of the vehicle physics

#ifndef C_ASW_FOUR_WHEEL_VEHICLE_PHYSICS_H
#define C_ASW_FOUR_WHEEL_VEHICLE_PHYSICS_H

#ifdef _WIN32
#pragma once
#endif

#include "vphysics/vehicles.h"
#include "vcollide_parse.h"
#include "datamap.h"
//#include "vehicle_sounds.h"

// in/sec to miles/hour
#define INS2MPH_SCALE	( 3600 * (1/5280.0f) * (1/12.0f) )
#define INS2MPH(x)		( (x) * INS2MPH_SCALE )
#define MPH2INS(x)		( (x) * (1/INS2MPH_SCALE) )

class C_BaseAnimating;
//class CFourWheelServerVehicle;	// asw comment


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ASW_FourWheelVehiclePhysics
{
public:
	DECLARE_DATADESC();

	C_ASW_FourWheelVehiclePhysics( C_BaseAnimating *pOuter );
	virtual ~C_ASW_FourWheelVehiclePhysics ();

	// Call Precache + Spawn from the containing entity's Precache + Spawn methods
	void Spawn();
	//void SetOuter( C_BaseAnimating *pOuter, CFourWheelServerVehicle *pServerVehicle ); // asw comment
	void SetOuter( C_BaseAnimating *pOuter ); 
	
	// Initializes the vehicle physics so we can drive it
	bool Initialize( const char *pScriptName, unsigned int nVehicleType );

	void Teleport( matrix3x4_t& relativeTransform );
	bool VPhysicsUpdate( IPhysicsObject *pPhysics );
	bool Think(float fTime);
	void PlaceWheelDust( int wheelIndex, bool ignoreSpeed = false );

	void DrawDebugGeometryOverlays();
	int DrawDebugTextOverlays( int nOffset );

	// Updates the controls based on user input
	void UpdateDriverControls( CUserCmd *cmd, float flFrameTime );

	// Various steering parameters
	void SetThrottle( float flThrottle );
	void SetMaxThrottle( float flMaxThrottle );
	void SetMaxReverseThrottle( float flMaxThrottle );
	void SetSteering( float flSteering, float flSteeringRate );
	void SetSteeringDegrees( float flDegrees );
	void SetAction( float flAction );
	void TurnOn( );
	void TurnOff();
	void ReleaseHandbrake();
	void SetHandbrake( bool bBrake );
	bool IsOn() const { return m_bIsOn; }
	void ResetControls();
	void SetBoost( float flBoost );
	bool UpdateBooster( float flFrameTime );
	void SetHasBrakePedal( bool bHasBrakePedal );

	// Engine
	void SetDisableEngine( bool bDisable );
	bool IsEngineDisabled( void )							{ return m_pVehicle->IsEngineDisabled(); }

	// Enable/Disable Motion
	void EnableMotion( void );
	void DisableMotion( void );

	// Shared code to compute the vehicle view position
	void GetVehicleViewPosition( const char *pViewAttachment, float flPitchFactor, Vector *pAbsPosition, QAngle *pAbsAngles );

	IPhysicsObject *GetWheel( int iWheel )				{ return m_pWheels[iWheel]; }

	int	GetSpeed() const;
	int GetMaxSpeed() const;
	int GetRPM() const;
	float GetThrottle() const;
	bool HasBoost() const;
	int BoostTimeLeft() const;
	bool IsBoosting( void );
	float GetHLSpeed() const;
	float GetSteering() const;
	float GetSteeringDegrees() const;
	IPhysicsVehicleController* GetVehicle(void) { return m_pVehicle; } 
	float GetWheelBaseHeight(int wheelIndex) { return m_wheelBaseHeight[wheelIndex]; }
	float GetWheelTotalHeight(int wheelIndex) { return m_wheelTotalHeight[wheelIndex]; }

	const vehicleparams_t &GetVehicleParams( void ) { return m_pVehicle->GetVehicleParams(); }
	const vehicle_controlparams_t &GetVehicleControls( void ) { return m_controls; }

	int VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );

	void	AddThrottleReduction( float flPercentage );
	void	RemoveThrottleReduction( float flPercentage );

private:
	// engine sounds
	void CalcWheelData( vehicleparams_t &vehicle );

	void SteeringRest( float carSpeed, const vehicleparams_t &vehicleData, float flFrameTime );
	void SteeringTurn( float carSpeed, const vehicleparams_t &vehicleData, bool bTurnLeft, float flFrameTime );
	void SteeringTurnAnalog( float carSpeed, const vehicleparams_t &vehicleData, float sidemove, float flFrameTime );

	// A couple wrapper methods to perform common operations
	int		LookupPoseParameter( const char *szName );
	float	GetPoseParameter( int iParameter );
	float	SetPoseParameter( int iParameter, float flValue );
	bool	GetAttachment ( const char *szName, Vector &origin, QAngle &angles );

	void InitializePoseParameters();
	bool ParseVehicleScript( const char *pScriptName, solid_t &solid, vehicleparams_t &vehicle );

private:
	// This is the entity that contains this class
	CHandle<C_BaseAnimating>		m_pOuter;
	//CFourWheelServerVehicle		*m_pOuterServerVehicle;	// asw comment

	vehicle_controlparams_t		m_controls;
	IPhysicsVehicleController	*m_pVehicle;

	// Vehicle state info
	int					m_nSpeed;
	int					m_nLastSpeed;
	int					m_nRPM;
	float				m_fLastBoost;
	int					m_nBoostTimeLeft;
	int					m_nHasBoost;

	float				m_maxThrottle;
	float				m_flMaxRevThrottle;
	float				m_flThrottleReduction;
	float				m_flMaxSpeed;
	float				m_actionSpeed;
	IPhysicsObject		*m_pWheels[4];

	int					m_wheelCount;

	Vector				m_wheelPosition[4];
	QAngle				m_wheelRotation[4];
	float				m_wheelBaseHeight[4];
	float				m_wheelTotalHeight[4];
	int					m_poseParameters[12];
	float				m_actionValue;
	float				m_actionScale;
	float				m_debugRadius;
	float				m_throttleRate;
	float				m_throttleStartTime;
	float				m_throttleActiveTime;
	float				m_turboTimer;

	float				m_flVehicleVolume;		// NPC driven vehicles used louder sounds
	bool				m_bIsOn;
	bool				m_bLastThrottle;
	bool				m_bLastBoost;
	bool				m_bLastSkid;

	int					m_nTurnLeftCount;
	int					m_nTurnRightCount;
};


//-----------------------------------------------------------------------------
// Physics state..
//-----------------------------------------------------------------------------
inline int C_ASW_FourWheelVehiclePhysics::GetSpeed() const
{
	return m_nSpeed;
}

inline int C_ASW_FourWheelVehiclePhysics::GetMaxSpeed() const
{
	return INS2MPH(m_pVehicle->GetVehicleParams().engine.maxSpeed);
}

inline int C_ASW_FourWheelVehiclePhysics::GetRPM() const
{
	return m_nRPM;
}

inline float C_ASW_FourWheelVehiclePhysics::GetThrottle() const
{
	return m_controls.throttle;
}

inline bool C_ASW_FourWheelVehiclePhysics::HasBoost() const
{
	return m_nHasBoost != 0;
}

inline int C_ASW_FourWheelVehiclePhysics::BoostTimeLeft() const
{
	return m_nBoostTimeLeft;
}

//inline void C_ASW_FourWheelVehiclePhysics::SetOuter( C_BaseAnimating *pOuter, CFourWheelServerVehicle *pServerVehicle )	// asw comment
inline void C_ASW_FourWheelVehiclePhysics::SetOuter( C_BaseAnimating *pOuter ) 
{ 
	m_pOuter = pOuter; 
	//	m_pOuterServerVehicle = pServerVehicle; // asw comment
}

float RemapAngleRange( float startInterval, float endInterval, float value );

#define ROLL_CURVE_ZERO		5		// roll less than this is clamped to zero
#define ROLL_CURVE_LINEAR	45		// roll greater than this is copied out

#define PITCH_CURVE_ZERO		10	// pitch less than this is clamped to zero
#define PITCH_CURVE_LINEAR		45	// pitch greater than this is copied out

#endif // C_ASW_FOUR_WHEEL_VEHICLE_PHYSICS_H
