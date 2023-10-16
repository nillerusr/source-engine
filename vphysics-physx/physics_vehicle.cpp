//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifdef _WIN32
#pragma warning (disable:4127)
#pragma warning (disable:4244)
#endif

#include "cbase.h"
#include "ivp_controller.hxx"
#include "ivp_cache_object.hxx"
#include "ivp_car_system.hxx"
#include "ivp_constraint_car.hxx"
#include "ivp_material.hxx"

#include "vphysics/vehicles.h"
#include "vphysics/friction.h"
#include "physics_vehicle.h"
#include "physics_controller_raycast_vehicle.h"
#include "physics_airboat.h"
#include "ivp_car_system.hxx"
#include "ivp_listener_object.hxx"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define THROTTLE_OPPOSING_FORCE_EPSILON		5.0f
#define VEHICLE_SKID_EPSILON				0.1f

// y in/s = x miles/hour * (5280 * 12 (in / mile)) * (1 / 3600 (hour / sec) )
//#define MPH2INS(x)		( (x) * 5280.0f * 12.0f / 3600.0f )
//#define INS2MPH(x)	    ( (x) * 3600 * (1/5280.0f) * (1/12.0f) )

#define MPH_TO_METERSPERSECOND	0.44707f
#define METERSPERSECOND_TO_MPH	(1.0f / MPH_TO_METERSPERSECOND)

#define MPH_TO_GAMEVEL(x)	(ConvertDistanceToHL( (x) * MPH_TO_METERSPERSECOND ))
#define GAMEVEL_TO_MPH(x)	(ConvertDistanceToIVP(x) * METERSPERSECOND_TO_MPH)


#define FVEHICLE_THROTTLE_STOPPED		0x00000001
#define FVEHICLE_HANDBRAKE_ON			0x00000002

struct vphysics_save_cvehiclecontroller_t
{
	CPhysicsObject				*m_pCarBody;
	int							m_wheelCount;
	vehicleparams_t				m_vehicleData;
	vehicle_operatingparams_t	m_currentState;
	float						m_wheelRadius;
	float						m_bodyMass;
	float						m_totalWheelMass;
	float						m_gravityLength;
	float						m_torqueScale;
	CPhysicsObject				*m_pWheels[VEHICLE_MAX_WHEEL_COUNT];
	Vector						m_wheelPosition_Bs[VEHICLE_MAX_WHEEL_COUNT];
	Vector						m_tracePosition_Bs[VEHICLE_MAX_WHEEL_COUNT];
	int							m_vehicleFlags;
	unsigned int				m_nTireType;
	unsigned int				m_nVehicleType;
	bool						m_bTraceData;
	bool						m_bOccupied;
	bool						m_bEngineDisable;
	float						m_flVelocity[3];

	DECLARE_SIMPLE_DATADESC();
};


BEGIN_SIMPLE_DATADESC( vphysics_save_cvehiclecontroller_t )
	DEFINE_VPHYSPTR( m_pCarBody ),
	DEFINE_FIELD( m_wheelCount,		FIELD_INTEGER ),
	DEFINE_EMBEDDED( m_vehicleData ),
	DEFINE_EMBEDDED( m_currentState ),
	DEFINE_FIELD( m_wheelCount,		FIELD_INTEGER ),
	DEFINE_FIELD( m_bodyMass,			FIELD_FLOAT	),
	DEFINE_FIELD( m_totalWheelMass,	FIELD_FLOAT	),
	DEFINE_FIELD( m_gravityLength,	FIELD_FLOAT	),
	DEFINE_FIELD( m_torqueScale,	FIELD_FLOAT	),

	DEFINE_VPHYSPTR_ARRAY( m_pWheels, VEHICLE_MAX_WHEEL_COUNT ),
	DEFINE_ARRAY( m_wheelPosition_Bs,	FIELD_VECTOR, VEHICLE_MAX_WHEEL_COUNT ),
	DEFINE_ARRAY( m_tracePosition_Bs,	FIELD_VECTOR, VEHICLE_MAX_WHEEL_COUNT ),
	DEFINE_FIELD( m_vehicleFlags,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nTireType,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nVehicleType,		FIELD_INTEGER ),
	DEFINE_FIELD( m_bTraceData,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOccupied,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bEngineDisable,	FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_flVelocity,		FIELD_FLOAT, 3 ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_operatingparams_t )
	DEFINE_FIELD( speed,			FIELD_FLOAT ),
	DEFINE_FIELD( engineRPM,		FIELD_FLOAT ),
	DEFINE_FIELD( gear,			FIELD_INTEGER ),
	DEFINE_FIELD( boostDelay,		FIELD_FLOAT ),
	DEFINE_FIELD( boostTimeLeft,	FIELD_INTEGER ),
	DEFINE_FIELD( skidSpeed,		FIELD_FLOAT ),
	DEFINE_CUSTOM_FIELD( skidMaterial,	MaterialIndexDataOps() ),
	DEFINE_FIELD( steeringAngle,	FIELD_FLOAT ),
	DEFINE_FIELD( wheelsInContact,	FIELD_INTEGER ),
	DEFINE_FIELD( wheelsNotInContact,FIELD_INTEGER ),
	DEFINE_FIELD( isTorqueBoosting,	FIELD_BOOLEAN ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_bodyparams_t )
	DEFINE_FIELD( massCenterOverride,	FIELD_VECTOR ),
	DEFINE_FIELD( massOverride,			FIELD_FLOAT ),
	DEFINE_FIELD( addGravity,			FIELD_FLOAT ),
	DEFINE_FIELD( maxAngularVelocity,	FIELD_FLOAT ),
	DEFINE_FIELD( tiltForce,			FIELD_FLOAT ),
	DEFINE_FIELD( tiltForceHeight,		FIELD_FLOAT ),
	DEFINE_FIELD( counterTorqueFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( keepUprightTorque,	FIELD_FLOAT ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_wheelparams_t )
	DEFINE_FIELD( radius,				FIELD_FLOAT ),
	DEFINE_FIELD( mass,				FIELD_FLOAT ),
	DEFINE_FIELD( inertia,			FIELD_FLOAT ),
	DEFINE_FIELD( damping,			FIELD_FLOAT ),
	DEFINE_FIELD( rotdamping,			FIELD_FLOAT ),
	DEFINE_FIELD( frictionScale,		FIELD_FLOAT ),
	DEFINE_CUSTOM_FIELD( materialIndex,		MaterialIndexDataOps() ),
	DEFINE_CUSTOM_FIELD( brakeMaterialIndex,	MaterialIndexDataOps() ),
	DEFINE_CUSTOM_FIELD( skidMaterialIndex,	MaterialIndexDataOps() ),
	DEFINE_FIELD( springAdditionalLength,	FIELD_FLOAT ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_suspensionparams_t )
	DEFINE_FIELD( springConstant,		FIELD_FLOAT ),
	DEFINE_FIELD( springDamping,		FIELD_FLOAT ),
	DEFINE_FIELD( stabilizerConstant,	FIELD_FLOAT ),
	DEFINE_FIELD( springDampingCompression,	FIELD_FLOAT ),
	DEFINE_FIELD( maxBodyForce,		FIELD_FLOAT ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_axleparams_t )
	DEFINE_FIELD( offset,					FIELD_VECTOR ),
	DEFINE_FIELD( wheelOffset,			FIELD_VECTOR ),
	DEFINE_FIELD( raytraceCenterOffset,	FIELD_VECTOR ),
	DEFINE_FIELD( raytraceOffset,			FIELD_VECTOR ),
	DEFINE_EMBEDDED( wheels ),
	DEFINE_EMBEDDED( suspension ),
	DEFINE_FIELD( torqueFactor,			FIELD_FLOAT ),
	DEFINE_FIELD( brakeFactor,			FIELD_FLOAT ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_steeringparams_t )
	DEFINE_FIELD( degreesSlow,		FIELD_FLOAT ),
	DEFINE_FIELD( degreesFast,		FIELD_FLOAT ),
	DEFINE_FIELD( degreesBoost,		FIELD_FLOAT ),
	DEFINE_FIELD( steeringRateSlow,	FIELD_FLOAT ),
	DEFINE_FIELD( steeringRateFast,	FIELD_FLOAT ),
	DEFINE_FIELD( steeringRestRateSlow,	FIELD_FLOAT ),
	DEFINE_FIELD( steeringRestRateFast,	FIELD_FLOAT ),
	DEFINE_FIELD( throttleSteeringRestRateFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( boostSteeringRestRateFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( boostSteeringRateFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( steeringExponent,	FIELD_FLOAT ),
	DEFINE_FIELD( speedSlow,			FIELD_FLOAT ),			
	DEFINE_FIELD( speedFast,			FIELD_FLOAT ),
	DEFINE_FIELD( turnThrottleReduceSlow,	FIELD_FLOAT ),
	DEFINE_FIELD( turnThrottleReduceFast,	FIELD_FLOAT ),
	DEFINE_FIELD( powerSlideAccel,	FIELD_FLOAT ),
	DEFINE_FIELD( brakeSteeringRateFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( isSkidAllowed,		FIELD_BOOLEAN ),
	DEFINE_FIELD( dustCloud,			FIELD_BOOLEAN ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicle_engineparams_t )
	DEFINE_FIELD( horsepower,			FIELD_FLOAT ),
	DEFINE_FIELD( maxSpeed,			FIELD_FLOAT ),
	DEFINE_FIELD( maxRevSpeed,		FIELD_FLOAT ),
	DEFINE_FIELD( maxRPM,				FIELD_FLOAT ),
	DEFINE_FIELD( axleRatio,			FIELD_FLOAT ),
	DEFINE_FIELD( throttleTime,		FIELD_FLOAT ),
	DEFINE_FIELD( maxRPM,				FIELD_FLOAT ),
	DEFINE_FIELD( isAutoTransmission,	FIELD_BOOLEAN ),
	DEFINE_FIELD( gearCount,			FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( gearRatio,		FIELD_FLOAT ),
	DEFINE_FIELD( shiftUpRPM,			FIELD_FLOAT ),
	DEFINE_FIELD( shiftDownRPM,		FIELD_FLOAT ),
	DEFINE_FIELD( boostForce,			FIELD_FLOAT ),
	DEFINE_FIELD( boostDuration,		FIELD_FLOAT ),
	DEFINE_FIELD( boostDelay,			FIELD_FLOAT ),
	DEFINE_FIELD( boostMaxSpeed,		FIELD_FLOAT ),
	DEFINE_FIELD( autobrakeSpeedGain,	FIELD_FLOAT ),
	DEFINE_FIELD( autobrakeSpeedFactor,	FIELD_FLOAT ),
	DEFINE_FIELD( torqueBoost,		FIELD_BOOLEAN ),
END_DATADESC()

BEGIN_SIMPLE_DATADESC( vehicleparams_t )
	DEFINE_FIELD( axleCount,					FIELD_INTEGER ),
	DEFINE_FIELD( wheelsPerAxle,				FIELD_INTEGER ),
	DEFINE_EMBEDDED( body ),
	DEFINE_EMBEDDED_AUTO_ARRAY( axles ),
	DEFINE_EMBEDDED( engine ),
	DEFINE_EMBEDDED( steering ),
END_DATADESC()


bool IsVehicleWheel( IVP_Real_Object *pivp )
{
	CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pivp->client_data);

	// FIXME: Check why this is null! It occurs when jumping the ravine in seafloor
	if (!pObject)
		return false;

	return (pObject->CallbackFlags() & CALLBACK_IS_VEHICLE_WHEEL) ? true : false;
}

inline bool IsMoveable( IVP_Real_Object *pObject )
{
	IVP_Core *pCore = pObject->get_core();
	if ( pCore->pinned || pCore->physical_unmoveable )
		return false;
	return true;
}

inline bool IVPFloatPointIsZero( const IVP_U_Float_Point &test )
{
	const float eps = 1e-4f;
	return test.quad_length() < eps ? true : false;
}

bool ShouldOverrideWheelContactFriction( float *pFrictionOut, IVP_Real_Object *pivp0, IVP_Real_Object *pivp1, IVP_U_Float_Point *pNormal )
{
	if ( !pivp0->get_core()->car_wheel && !pivp1->get_core()->car_wheel )
		return false;

	if ( !IsVehicleWheel(pivp0) )
	{
		if ( !IsVehicleWheel(pivp1) )
			return false;
		// swap so pivp0 is a wheel
		IVP_Real_Object *pTmp = pivp0;
		pivp0 = pivp1;
		pivp1 = pTmp;
	}

	// if we got here then pivp0 is a car wheel object
	// BUGBUG: IVP sometimes sends us a bogus normal
	// when doing a material realc on existing contacts!
	if ( !IVPFloatPointIsZero(pNormal) )
	{
		IVP_U_Float_Point normalWheelSpace;
		pivp0->get_core()->get_m_world_f_core_PSI()->vimult3( pNormal, &normalWheelSpace );
		if ( fabs(normalWheelSpace.k[0]) > 0.2588f )	// 15 degree wheel cone
		{
			// adjust friction here, this isn't a valid part of the wheel for contact, set friction to zero
			//Vector tmp;ConvertDirectionToHL( normalWheelSpace, tmp );Msg("Wheel sliding on surface %.2f %.2f %.2f\n", tmp.x, tmp.y, tmp.z );
			*pFrictionOut = 0;
			return true;
		}
	}
	// was car wheel, but didn't adjust - use default friction
	return false;
}


class CVehicleController : public IPhysicsVehicleController, public IVP_Listener_Object
{
public:
	CVehicleController( );
	CVehicleController( const vehicleparams_t &params, CPhysicsEnvironment *pEnv, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace );
	~CVehicleController();

	// CVehicleController
	void InitCarSystem( CPhysicsObject *pBodyObject );

	// IPhysicsVehicleController
	void Update( float dt, vehicle_controlparams_t &controls );
	float UpdateBooster( float dt );
	void SetSpringLength(int wheelIndex, float length);
	const vehicle_operatingparams_t &GetOperatingParams()	{ return m_currentState; }
	const vehicleparams_t &GetVehicleParams()				{ return m_vehicleData; }
	vehicleparams_t &GetVehicleParamsForChange()			{ return m_vehicleData; }
	int GetWheelCount(void)									{ return m_wheelCount; };
	IPhysicsObject* GetWheel(int index);
	virtual bool GetWheelContactPoint( int index, Vector *pContactPoint, int *pSurfaceProps );
	void SetWheelFriction(int wheelIndex, float friction);

	void SetEngineDisabled( bool bDisable )					{ m_bEngineDisable = bDisable; }
	bool IsEngineDisabled( void )							{ return m_bEngineDisable; }

	// Save/load
	void WriteToTemplate( vphysics_save_cvehiclecontroller_t &controllerTemplate );
	void InitFromTemplate( CPhysicsEnvironment *pEnv, void *pGameData, IPhysicsGameTrace *pGameTrace, const vphysics_save_cvehiclecontroller_t &controllerTemplate );
	void VehicleDataReload();

	// Debug
	void GetCarSystemDebugData( vehicle_debugcarsystem_t &debugCarSystem );

	// IVP_Listener_Object
	// Object listener, only hook delete
    virtual void event_object_deleted( IVP_Event_Object *);
    virtual void event_object_created( IVP_Event_Object *) {}
    virtual void event_object_revived( IVP_Event_Object *) {}
    virtual void event_object_frozen ( IVP_Event_Object *) {}

	// Entry/Exit 
	void OnVehicleEnter( void );
	void OnVehicleExit( void );

protected:
	void CreateIVPObjects( );
	void ShutdownCarSystem();

	void InitVehicleData( const vehicleparams_t &params );
	void InitCarSystemBody( IVP_Template_Car_System &ivpVehicleData );
	void InitCarSystemWheels( IVP_Template_Car_System &ivpVehicleData );

	void AttachListener();

	IVP_Real_Object *CreateWheel( int wheelIndex, vehicle_axleparams_t &axle );
	void CreateTraceData( int wheelIndex, vehicle_axleparams_t &axle );

	// Update.
	void UpdateSteering( const vehicle_controlparams_t &controls, float flDeltaTime, float flSpeed );
	void UpdatePowerslide( const vehicle_controlparams_t &controls, bool bPowerslide, float flSpeed );
	void UpdateEngine( const vehicle_controlparams_t &controls, float flDeltaTime, float flThrottle, float flBrake, bool bHandbrake, bool bPowerslide );
	bool UpdateEngineTurboStart( const vehicle_controlparams_t &controls, float flDeltaTime );
	void UpdateEngineTurboFinish( void );
	void UpdateHandbrake( const vehicle_controlparams_t &controls, float flThrottle, bool bHandbrake, bool bPowerslide );
	void UpdateSkidding( bool bHandbrake );
	void UpdateExtraForces( void );
	void UpdateWheelPositions( void );
	float CalcSteering( float dt, float speed, float steering, bool bAnalog );
	void CalcEngine( float throttle, float brake_val, bool handbrake, float steeringVal, bool torqueBoost );
	void CalcEngineTransmission( float flThrottle );

	virtual bool IsBoosting( void );

private:
	void ResetState();
	IVP_Car_System				*m_pCarSystem;
	CPhysicsObject				*m_pCarBody;
	CPhysicsEnvironment			*m_pEnv;
	IPhysicsGameTrace			*m_pGameTrace;
	int							m_wheelCount;
	vehicleparams_t				m_vehicleData;
	vehicle_operatingparams_t	m_currentState;
	float						m_wheelRadius;
	float						m_bodyMass;
	float						m_totalWheelMass;
	float						m_gravityLength;
	float						m_torqueScale;
	CPhysicsObject				*m_pWheels[VEHICLE_MAX_WHEEL_COUNT];
	IVP_U_Float_Point			m_wheelPosition_Bs[VEHICLE_MAX_WHEEL_COUNT];
	IVP_U_Float_Point			m_tracePosition_Bs[VEHICLE_MAX_WHEEL_COUNT];
	int							m_vehicleFlags;
	unsigned int				m_nTireType;
	unsigned int				m_nVehicleType;
	bool						m_bTraceData;
	bool						m_bOccupied;
	bool						m_bEngineDisable;
	float						m_flVelocity[3];
};

CVehicleController::CVehicleController( const vehicleparams_t &params, CPhysicsEnvironment *pEnv, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace )
{
	m_pEnv = pEnv;
	m_pGameTrace = pGameTrace;
	m_nVehicleType = nVehicleType;
	InitVehicleData( params );
	ResetState();
}


CVehicleController::CVehicleController()
{
	ResetState();
}


void CVehicleController::ResetState()
{
	m_pCarSystem = NULL;
	m_flVelocity[0] = m_flVelocity[1]= m_flVelocity[2] = 0.0f;
	for ( int i = 0; i < VEHICLE_MAX_WHEEL_COUNT; i++ )
	{
		m_pWheels[i] = NULL;
	}
	m_pCarBody = NULL;
	m_torqueScale = 1;
	m_wheelCount = 0;
	m_wheelRadius = 0;
	memset( &m_currentState, 0, sizeof(m_currentState) );
	m_bodyMass = 0;
	m_vehicleFlags = 0;
	memset( m_wheelPosition_Bs, 0, sizeof(m_wheelPosition_Bs) );
	memset( m_tracePosition_Bs, 0, sizeof(m_tracePosition_Bs) );

	m_bTraceData = false;
	if ( m_nVehicleType == VEHICLE_TYPE_AIRBOAT_RAYCAST )
	{
		m_bTraceData = true;
	}

	m_nTireType = VEHICLE_TIRE_NORMAL;

	m_bOccupied = false;
	m_bEngineDisable = false;
}

CVehicleController::~CVehicleController()
{
	ShutdownCarSystem();
}

IPhysicsObject* CVehicleController::GetWheel( int index ) 
{ 
	// TODO: This is getting messy.
	if ( m_nVehicleType == VEHICLE_TYPE_CAR_WHEELS )
	{
		return m_pWheels[index]; 
	}
	else if ( m_nVehicleType == VEHICLE_TYPE_CAR_RAYCAST && m_pCarSystem )
	{
		return static_cast<CPhysics_Car_System_Raycast_Wheels*>( m_pCarSystem )->GetWheel( index );
	}
	else if ( m_nVehicleType == VEHICLE_TYPE_AIRBOAT_RAYCAST && m_pCarSystem )
	{
		return static_cast<CPhysics_Airboat*>( m_pCarSystem )->GetWheel( index );
	}

	return NULL;
}

void CVehicleController::SetWheelFriction(int wheelIndex, float friction)
{
	CPhysics_Airboat *pAirboat = static_cast<CPhysics_Airboat*>( m_pCarSystem );
	if ( !pAirboat )
		return;

	pAirboat->SetWheelFriction( wheelIndex, friction );
}

bool CVehicleController::GetWheelContactPoint( int index, Vector *pContactPoint, int *pSurfaceProps )
{
	bool bSet = false;
	if ( index < m_wheelCount )
	{
		IPhysicsFrictionSnapshot *pSnapshot = m_pWheels[index]->CreateFrictionSnapshot();
		float forceMax = -1.0f;
		m_pWheels[index]->GetPosition( pContactPoint, NULL );
		while ( pSnapshot->IsValid() )
		{
			float thisForce = pSnapshot->GetNormalForce();
			if ( thisForce > forceMax )
			{
				forceMax = thisForce;
				if ( pContactPoint )
				{
					pSnapshot->GetContactPoint( *pContactPoint );
				}
				if ( pSurfaceProps )
				{
					*pSurfaceProps = pSnapshot->GetMaterial(1);
				}
				bSet = true;
			}
			pSnapshot->NextFrictionData();
		}
		m_pWheels[index]->DestroyFrictionSnapshot(pSnapshot);
	}
	else
	{
		if ( pContactPoint )
		{
			pContactPoint->Init();
		}
		if ( pSurfaceProps )
		{
			*pSurfaceProps = 0;
		}
		
	}
	return bSet;
}

void CVehicleController::AttachListener()
{
	m_pCarBody->GetObject()->add_listener_object( this );
}

void CVehicleController::event_object_deleted( IVP_Event_Object *pEvent )
{
	// the car system's constraint solver is going to delete itself now, so NULL the car system.

	m_pCarSystem->event_object_deleted( pEvent );	
	m_pCarSystem = NULL;
	ShutdownCarSystem();
}

IVP_Real_Object *CVehicleController::CreateWheel( int wheelIndex, vehicle_axleparams_t &axle )
{
	if ( wheelIndex >= VEHICLE_MAX_WHEEL_COUNT )
		return NULL;

	// HACKHACK: In Save/load, the wheel was reloaded, so pretend to create it
	// ALSO NOTE: Save/load puts the results into m_pWheels	regardless of vehicle type!!!
	// That's why I'm not calling GetWheel().
	if ( m_pWheels[wheelIndex] )
	{
		CPhysicsObject *pWheelObject = static_cast<CPhysicsObject *>(m_pWheels[wheelIndex]);
		return pWheelObject->GetObject();
	}

	objectparams_t params;
	memset( &params, 0, sizeof(params) );

	Vector bodyPosition;
	QAngle bodyAngles;
	m_pCarBody->GetPosition( &bodyPosition, &bodyAngles );
	matrix3x4_t matrix;
	AngleMatrix( bodyAngles, bodyPosition, matrix );

	Vector position = axle.offset;

	// BUGBUG: This only works with 2 wheels per axle
	if ( wheelIndex & 1 )
	{
		position += axle.wheelOffset;
	}
	else
	{
		position -= axle.wheelOffset;
	}

	Vector wheelPositionHL;
	VectorTransform( position, matrix, wheelPositionHL );

	params.damping = axle.wheels.damping;
	params.dragCoefficient = 0;
	params.enableCollisions = false;
	params.inertia = axle.wheels.inertia;
	params.mass = axle.wheels.mass;
	params.pGameData = m_pCarBody->GetGameData();
	params.pName = "VehicleWheel";
	params.rotdamping = axle.wheels.rotdamping;
	params.rotInertiaLimit = 0;
	params.massCenterOverride = NULL;
	// needs to be in HL units because we're calling through the "outer" interface to create
	// the wheels
	float radius = axle.wheels.radius;
	float r3 = radius * radius * radius;
	params.volume = (4 / 3) * M_PI * r3;

	CPhysicsObject *pWheel = (CPhysicsObject *)m_pEnv->CreateSphereObject( radius, axle.wheels.materialIndex, wheelPositionHL, bodyAngles, &params, false );
    pWheel->Wake();

	// UNDONE: only mask off some of these flags?
	unsigned int flags = pWheel->CallbackFlags();
	flags = 0;
	pWheel->SetCallbackFlags( flags );
	// copy the body's game flags
	pWheel->SetGameFlags( m_pCarBody->GetGameFlags() );
	// cache the wheel object pointer
	m_pWheels[wheelIndex] = pWheel;

	IVP_U_Point wheelPositionIVP, wheelPositionBs;
	ConvertPositionToIVP( wheelPositionHL, wheelPositionIVP );
	TransformIVPToLocal( wheelPositionIVP, wheelPositionBs, m_pCarBody->GetObject(), true );
	m_wheelPosition_Bs[wheelIndex].set_to_zero();
	m_wheelPosition_Bs[wheelIndex].set( &wheelPositionBs );

	pWheel->AddCallbackFlags( CALLBACK_IS_VEHICLE_WHEEL );

	return pWheel->GetObject();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::CreateTraceData( int wheelIndex, vehicle_axleparams_t &axle )
{
	if ( wheelIndex >= VEHICLE_MAX_WHEEL_COUNT )
		return;

	objectparams_t params;
	memset( &params, 0, sizeof( params ) );

	Vector bodyPosition;
	QAngle bodyAngles;
	matrix3x4_t matrix;
	m_pCarBody->GetPosition( &bodyPosition, &bodyAngles );
	AngleMatrix( bodyAngles, bodyPosition, matrix );

	Vector tracePosition = axle.raytraceCenterOffset;
	// BUGBUG: This only works with 2 wheels per axle
	if ( wheelIndex & 1 )
	{
		tracePosition += axle.raytraceOffset;
	}
	else
	{
		tracePosition -= axle.raytraceOffset;
	}

	Vector tracePositionHL;
	VectorTransform( tracePosition, matrix, tracePositionHL );

	IVP_U_Point tracePositionIVP, tracePositionBs;
	ConvertPositionToIVP( tracePositionHL, tracePositionIVP );
	TransformIVPToLocal( tracePositionIVP, tracePositionBs, m_pCarBody->GetObject(), true );
	m_tracePosition_Bs[wheelIndex].set_to_zero();
	m_tracePosition_Bs[wheelIndex].set( &tracePositionBs );
}

void CVehicleController::CreateIVPObjects( )
{
	// Initialize the car system (body and wheels).
	IVP_Template_Car_System ivpVehicleData( m_wheelCount, m_vehicleData.axleCount );
	InitCarSystemBody( ivpVehicleData );

	InitCarSystemWheels( ivpVehicleData );

	BEGIN_IVP_ALLOCATION();

	// Raycast Car
	switch ( m_nVehicleType )
	{
	case VEHICLE_TYPE_CAR_WHEELS:
		m_pCarSystem = new IVP_Car_System_Real_Wheels( m_pEnv->GetIVPEnvironment(), &ivpVehicleData ); 
		break
			;
	case VEHICLE_TYPE_CAR_RAYCAST: 
		m_pCarSystem = new CPhysics_Car_System_Raycast_Wheels( m_pEnv->GetIVPEnvironment(), &ivpVehicleData ); 
		break;

	case VEHICLE_TYPE_AIRBOAT_RAYCAST: 
		m_pCarSystem = new CPhysics_Airboat( m_pEnv->GetIVPEnvironment(), &ivpVehicleData, m_pGameTrace );
		break; 
	}

	AttachListener();

	END_IVP_ALLOCATION();
}


void CVehicleController::InitCarSystem( CPhysicsObject *pBodyObject )
{
	if ( m_pCarSystem )
	{
		ShutdownCarSystem();
	}

	// Car body.
	m_pCarBody = pBodyObject;
	m_bodyMass = m_pCarBody->GetMass();
	m_gravityLength = m_pEnv->GetIVPEnvironment()->get_gravity()->real_length();
	// Setup axle/wheel counts.
	m_wheelCount = m_vehicleData.axleCount * m_vehicleData.wheelsPerAxle;
	CreateIVPObjects();

	if ( m_nVehicleType == VEHICLE_TYPE_AIRBOAT_RAYCAST )
	{
		float flDampSpeed = 1.0f;
		float flDampRotSpeed = 1.0f;
		m_pCarBody->SetDamping( &flDampSpeed, &flDampRotSpeed );
	}
}

void CVehicleController::VehicleDataReload()
{
	// compute torque normalization factor
	m_torqueScale = 1;
	// Clear accumulation.
	float totalTorqueDistribution = 0.0f;
	for ( int i = 0; i < m_vehicleData.axleCount; i++ )
	{
		totalTorqueDistribution += m_vehicleData.axles[i].torqueFactor;
	}

	if ( totalTorqueDistribution > 0 )
	{
		m_torqueScale /= totalTorqueDistribution;
	}
	// input speed is in miles/hour.  Convert to in/s
	m_vehicleData.engine.maxSpeed = MPH_TO_GAMEVEL(m_vehicleData.engine.maxSpeed);
	m_vehicleData.engine.maxRevSpeed = MPH_TO_GAMEVEL(m_vehicleData.engine.maxRevSpeed);
	m_vehicleData.engine.boostMaxSpeed = MPH_TO_GAMEVEL(m_vehicleData.engine.boostMaxSpeed);
}


//-----------------------------------------------------------------------------
// Purpose: Setup the body parameters.
//-----------------------------------------------------------------------------
void CVehicleController::InitCarSystemBody( IVP_Template_Car_System &ivpVehicleData )
{
	ivpVehicleData.car_body = m_pCarBody->GetObject();

	ivpVehicleData.index_x = IVP_INDEX_X;
	ivpVehicleData.index_y = IVP_INDEX_Y;
	ivpVehicleData.index_z = IVP_INDEX_Z;

	ivpVehicleData.body_counter_torque_factor = m_vehicleData.body.counterTorqueFactor;
	ivpVehicleData.body_down_force_vertical_offset = ConvertDistanceToIVP( m_vehicleData.body.tiltForceHeight );
	ivpVehicleData.extra_gravity_force_value = m_vehicleData.body.addGravity * m_gravityLength * m_bodyMass;
	ivpVehicleData.extra_gravity_height_offset = 0;

#if 0
	// HACKHACK: match example
	ivpVehicleData.extra_gravity_force_value = 1.2;
	ivpVehicleData.body_down_force_vertical_offset = 2;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Setup the wheel paramters.
//-----------------------------------------------------------------------------
void CVehicleController::InitCarSystemWheels( IVP_Template_Car_System &ivpVehicleData )
{
	int	  wheelIndex = 0;

	m_wheelRadius = 0;
	m_totalWheelMass = 0;

	int i;
	for ( i = 0; i < m_vehicleData.axleCount; i++ )
	{
		for ( int w = 0; w < m_vehicleData.wheelsPerAxle; w++, wheelIndex++ )
		{
			IVP_Real_Object *pWheel = CreateWheel( wheelIndex, m_vehicleData.axles[i] );
			if ( pWheel )
			{
				// Create ray trace data for wheel.
				if ( m_bTraceData )
				{
					CreateTraceData( wheelIndex, m_vehicleData.axles[i] );
				}

				ivpVehicleData.car_wheel[wheelIndex] = pWheel;
				ivpVehicleData.wheel_radius[wheelIndex] = pWheel->get_core()->upper_limit_radius;
				ivpVehicleData.wheel_reversed_sign[wheelIndex] = 1.0;
				// only for raycast car

				ivpVehicleData.friction_of_wheel[wheelIndex] = m_vehicleData.axles[i].wheels.frictionScale;
				ivpVehicleData.spring_constant[wheelIndex] = m_vehicleData.axles[i].suspension.springConstant * m_bodyMass;
				ivpVehicleData.spring_dampening[wheelIndex] = m_vehicleData.axles[i].suspension.springDamping * m_bodyMass;
				ivpVehicleData.spring_dampening_compression[wheelIndex] = m_vehicleData.axles[i].suspension.springDampingCompression * m_bodyMass;
				ivpVehicleData.max_body_force[wheelIndex] = m_vehicleData.axles[i].suspension.maxBodyForce * m_bodyMass;
				ivpVehicleData.spring_pre_tension[wheelIndex] = -ConvertDistanceToIVP( m_vehicleData.axles[i].wheels.springAdditionalLength );
								
				ivpVehicleData.wheel_pos_Bos[wheelIndex] = m_wheelPosition_Bs[wheelIndex];
				if ( m_bTraceData )
				{
					ivpVehicleData.trace_pos_Bos[wheelIndex] = m_tracePosition_Bs[wheelIndex];
				}

				m_totalWheelMass += m_vehicleData.axles[i].wheels.mass;
			}
		}

		ivpVehicleData.stabilizer_constant[i] = m_vehicleData.axles[i].suspension.stabilizerConstant * m_bodyMass;
		// this should output in radians per second
		float radius = ConvertDistanceToIVP( m_vehicleData.axles[i].wheels.radius );
		float totalMaxSpeed = max( m_vehicleData.engine.boostMaxSpeed, m_vehicleData.engine.maxSpeed );
		ivpVehicleData.wheel_max_rotation_speed[i] = totalMaxSpeed / radius;
		if ( radius > m_wheelRadius )
		{
			m_wheelRadius = radius;
		}
	}

	for ( i = 0; i < m_wheelCount; i++ )
	{
		m_pWheels[i]->EnableCollisions( true );
	}
}

void CVehicleController::ShutdownCarSystem()
{
	delete m_pCarSystem;
	m_pCarSystem = NULL;
	for ( int i = 0; i < m_wheelCount; i++ )
	{
		if ( m_pWheels[i] )
		{
			m_pEnv->DestroyObject( m_pWheels[i] );
		}
		m_pWheels[i] = NULL;
	}
}


void CVehicleController::InitVehicleData( const vehicleparams_t &params )
{
	m_vehicleData = params;
	VehicleDataReload();
}

void CVehicleController::SetSpringLength(int wheelIndex, float length)
{
	m_pCarSystem->change_spring_length((IVP_POS_WHEEL)wheelIndex, length);
}

//-----------------------------------------------------------------------------
// Purpose: Allows booster timer to run, 
// Returns: true if time still exists
//			false if timer has run out (i.e. can use boost again)
//-----------------------------------------------------------------------------
float CVehicleController::UpdateBooster( float dt )
{
	m_pCarSystem->update_booster( dt );
	m_currentState.boostDelay = m_pCarSystem->get_booster_delay();
	return m_currentState.boostDelay;
}

//-----------------------------------------------------------------------------
// Purpose: Are whe boosting?
//-----------------------------------------------------------------------------
bool CVehicleController::IsBoosting( void )
{
	return ( m_pCarSystem->get_booster_time_to_go() > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Update the vehicle controller.
//-----------------------------------------------------------------------------
void CVehicleController::Update( float dt, vehicle_controlparams_t &controlsIn )
{
	vehicle_controlparams_t controls = controlsIn;
	// Speed.
    m_currentState.speed = ConvertDistanceToHL( m_pCarSystem->get_body_speed() );
	float flSpeed = GAMEVEL_TO_MPH( m_currentState.speed );
	float flAbsSpeed = fabsf( flSpeed );

	// Calculate the throttle and brake values.
	float flThrottle = controls.throttle;
	bool bHandbrake = controls.handbrake;
	float flBrake = controls.brake;
	bool bPowerslide = bHandbrake && ( flAbsSpeed > 18.0f );

	if ( bHandbrake )
	{
		flThrottle = 0.0f;
	}

	if ( IsBoosting() )
	{
		controls.boost = true;
		flThrottle = flThrottle < 0.0f ? -1.0f : 1.0f;
	}

	if ( flThrottle == 0.0f && flBrake == 0.0f && !bHandbrake )
	{
		flBrake = 0.1f;
	}

	// Update steering.
	UpdateSteering( controls, dt, flAbsSpeed );

	// Update powerslide.
	UpdatePowerslide( controls, bPowerslide, flSpeed );

	// Update engine.
	UpdateEngine( controls, dt, flThrottle, flBrake, bHandbrake, bPowerslide );

	// Update handbrake.
	UpdateHandbrake( controls, flThrottle, bHandbrake, bPowerslide );

	// Update skidding.
	UpdateSkidding( bHandbrake );

	// Apply the extra forces to the car (downward, counter-torque, etc.)
	UpdateExtraForces();

	// Update the physical position of the wheels for raycast vehicles.
	UpdateWheelPositions();
}

//-----------------------------------------------------------------------------
// Purpose: Update the steering on the vehicle.
//-----------------------------------------------------------------------------
void CVehicleController::UpdateSteering( const vehicle_controlparams_t &controls, float flDeltaTime, float flSpeed )
{
   // Steering - IVP steering is in radians.
    float flSteeringAngle = CalcSteering( flDeltaTime, flSpeed, controls.steering, controls.bAnalogSteering );
    m_pCarSystem->do_steering( DEG2RAD( flSteeringAngle ), controls.bAnalogSteering );
	m_currentState.steeringAngle = flSteeringAngle;
}

//-----------------------------------------------------------------------------
// Purpose: Update the powerslide state (wheel materials).
//-----------------------------------------------------------------------------
void CVehicleController::UpdatePowerslide( const vehicle_controlparams_t &controls, bool bPowerslide, float flSpeed )
{
	// Only allow skidding if it is allowed by the vehicle type.
	if ( !m_vehicleData.steering.isSkidAllowed )
		return;

	// Check to see if the vehicle is occupied.
	if ( !m_bOccupied )
		return;

	// Set the powerslide left/right.
	bool bPowerslideLeft = bPowerslide && controls.handbrakeLeft;
	bool bPowerslideRight = bPowerslide && controls.handbrakeRight;

	int iWheel = 0;
	unsigned int newTireType = VEHICLE_TIRE_NORMAL;
	if ( bPowerslideLeft || bPowerslideRight )
	{
		newTireType = VEHICLE_TIRE_POWERSLIDE;
	}
	else if ( bPowerslide )
	{
		newTireType = VEHICLE_TIRE_BRAKING;
	}

	if ( newTireType != m_nTireType )
	{
		for ( int iAxle = 0; iAxle < m_vehicleData.axleCount; ++iAxle )
		{
			int materialIndex = m_vehicleData.axles[iAxle].wheels.materialIndex;
			if ( newTireType == VEHICLE_TIRE_POWERSLIDE && ( m_vehicleData.axles[iAxle].wheels.skidMaterialIndex != - 1 ) )
			{
				materialIndex = m_vehicleData.axles[iAxle].wheels.skidMaterialIndex;
			}
			else if ( newTireType == VEHICLE_TIRE_BRAKING && ( m_vehicleData.axles[iAxle].wheels.brakeMaterialIndex != -1 ) )
			{
				materialIndex = m_vehicleData.axles[iAxle].wheels.brakeMaterialIndex;
			}

			for ( int iAxleWheel = 0; iAxleWheel < m_vehicleData.wheelsPerAxle; ++iAxleWheel, ++iWheel )
			{
				m_pWheels[iWheel]->SetMaterialIndex( materialIndex );
			}
						
			m_nTireType = newTireType;
		}
	}

	// Push the car a little.
	float flFrontAccel = 0.0f;
	float flRearAccel = 0.0f;
	if ( flSpeed > 0 && (bPowerslideLeft != bPowerslideRight) )
	{
		// NOTE: positive acceleration is to the left
		float powerSlide = RemapValClamped( flSpeed, m_vehicleData.steering.speedSlow, m_vehicleData.steering.speedFast, 0, 1 );
		float powerSlideAccel = ConvertDistanceToIVP( m_vehicleData.steering.powerSlideAccel);
		if ( bPowerslideLeft )
		{
			flFrontAccel = powerSlideAccel * powerSlide;
			flRearAccel = -powerSlideAccel * powerSlide;
		}
		else
		{
			flFrontAccel = -powerSlideAccel * powerSlide;
			flRearAccel = powerSlideAccel * powerSlide;
		}
	}
	m_pCarSystem->set_powerslide( flFrontAccel, flRearAccel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::UpdateEngine( const vehicle_controlparams_t &controls, float flDeltaTime, 
									   float flThrottle, float flBrake, bool bHandbrake, bool bPowerslide )
{
	bool bTorqueBoost = UpdateEngineTurboStart( controls, flDeltaTime );

	CalcEngine( flThrottle, flBrake, bHandbrake, controls.steering, bTorqueBoost );

	UpdateEngineTurboFinish();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVehicleController::UpdateEngineTurboStart( const vehicle_controlparams_t &controls, float flDeltaTime )
{
	bool bTorqueBoost = false;
	if ( controls.boost > 0 )
	{
		if ( m_vehicleData.engine.torqueBoost )
		{
			// Turbo will be applied at the engine level.
			bTorqueBoost = true;
			m_pCarSystem->activate_booster( 0.0f, m_vehicleData.engine.boostDuration, m_vehicleData.engine.boostDelay );
		}
		else
		{
			// Activate the turbo force booster - applied to vehicle body.
			m_pCarSystem->activate_booster( m_vehicleData.engine.boostForce * controls.boost, m_vehicleData.engine.boostDuration, m_vehicleData.engine.boostDelay );
		}
	}

	m_pCarSystem->update_booster( flDeltaTime );
	m_currentState.boostDelay = m_pCarSystem->get_booster_delay();
	m_currentState.isTorqueBoosting = bTorqueBoost;

	return bTorqueBoost;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::UpdateEngineTurboFinish( void )
{
	if ( m_vehicleData.engine.boostDuration + m_vehicleData.engine.boostDelay > 0 )	// watch out for div by zero
	{
		if ( m_currentState.boostDelay > 0 )
		{
			m_currentState.boostTimeLeft = 100 - 100 * ( m_currentState.boostDelay / ( m_vehicleData.engine.boostDuration +  m_vehicleData.engine.boostDelay ) );
		}
		else
		{
			m_currentState.boostTimeLeft = 100;		// ready to go any time
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the handbrake.
//-----------------------------------------------------------------------------
void CVehicleController::UpdateHandbrake( const vehicle_controlparams_t &controls, float flThrottle, bool bHandbrake, bool bPowerslide )
{
	// Get the current vehicle speed.
    m_currentState.speed = ConvertDistanceToHL( m_pCarSystem->get_body_speed() );
	if ( !bPowerslide )
	{
		// HACK! Allowing you to overcome gravity at low throttle.
		if ( ( flThrottle < 0.0f && m_currentState.speed > THROTTLE_OPPOSING_FORCE_EPSILON ) ||
			 ( flThrottle > 0.0f && m_currentState.speed < -THROTTLE_OPPOSING_FORCE_EPSILON ) )
		{	
			bHandbrake = true;
		}
	}

	if ( bHandbrake )
	{
		// HACKHACK: only allow the handbrake when the wheels have contact with something
		// otherwise they will affect the car in an undesirable way
 		bHandbrake = false;
		for ( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
		{
			if ( m_pWheels[iWheel]->GetContactPoint(NULL, NULL) )
			{
				bHandbrake = true;
				break;
			}
		}
	}

	bool currentHandbrake = (m_vehicleFlags & FVEHICLE_HANDBRAKE_ON) ? true : false;
	if ( bHandbrake != currentHandbrake )
	{
		if ( bHandbrake )
		{
			m_vehicleFlags |= FVEHICLE_HANDBRAKE_ON;
		}
		else
		{
			m_vehicleFlags &= ~FVEHICLE_HANDBRAKE_ON;
		}

		for ( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
		{
			m_pCarSystem->fix_wheel( ( IVP_POS_WHEEL )iWheel, bHandbrake ? IVP_TRUE : IVP_FALSE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::UpdateSkidding( bool bHandbrake )
{
	m_currentState.skidSpeed = 0.0f;
	m_currentState.skidMaterial = 0;
	m_currentState.wheelsInContact = m_wheelCount;
	m_currentState.wheelsNotInContact = 0;
	if ( m_vehicleData.steering.isSkidAllowed )
	{
		// Estimate rot speed based on current speed and the front wheels radius
		float flAbsSpeed = fabs( m_currentState.speed );

		Vector contact;
		Vector velocity;
		int surfaceProps;
		m_currentState.wheelsInContact = 0;
		m_currentState.wheelsNotInContact = 0;

		for( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
		{
			if ( GetWheelContactPoint( iWheel, &contact, &surfaceProps ) )
			{
				// NOTE: The wheel should be translating by the negative of the speed a point in contact with the surface
				// is moving.  So the net velocity on the surface is zero if that wheel is 100% engaged in driving the car
				// any velocity in excess of this gets compared against the threshold for skidding
				m_pWheels[iWheel]->GetVelocityAtPoint( contact, &velocity );
				float speed = velocity.Length();
				if ( speed > m_currentState.skidSpeed || m_currentState.skidSpeed <= 0.0f )
				{
					m_currentState.skidSpeed = speed;
					m_currentState.skidMaterial = surfaceProps;
				}
				m_currentState.wheelsInContact++;
			}
			else
			{
				m_currentState.wheelsNotInContact++;
			}
		}
		// Check for locked wheels.
		if ( bHandbrake && ( flAbsSpeed > 30 ) )
		{
			m_currentState.skidSpeed = flAbsSpeed;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Apply extra forces to the vehicle.  The downward force, counter-
//          torque etc.
//-----------------------------------------------------------------------------
void CVehicleController::UpdateExtraForces( void )
{
	// Extra downward force.
	IVP_Cache_Object *co = m_pCarBody->GetObject()->get_cache_object();
	float y_val = co->m_world_f_object.get_elem( IVP_INDEX_Y, IVP_INDEX_Y );
	if ( fabs( y_val ) < 0.05 )
	{
		m_pCarSystem->change_body_downforce( m_vehicleData.body.tiltForce * m_gravityLength * m_bodyMass );
	}
	else
	{
		m_pCarSystem->change_body_downforce( 0.0 );
	}
	co->remove_reference();

	// Counter-torque.
	if ( m_nVehicleType == VEHICLE_TYPE_CAR_WHEELS )
	{
	    m_pCarSystem->update_body_countertorque();
	}

	// if the car has a global angular velocity limit, apply that constraint
	AngularImpulse angVel;
	m_pCarBody->GetVelocity( NULL, &angVel );
	if ( m_vehicleData.body.maxAngularVelocity > 0 && angVel.Length() > m_vehicleData.body.maxAngularVelocity )
	{
		VectorNormalize(angVel);
		angVel *= m_vehicleData.body.maxAngularVelocity;
		m_pCarBody->SetVelocityInstantaneous( NULL, &angVel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the physical position of the wheels for raycast vehicles.
//	  NOTE: Raycast boat doesn't have wheels.
//-----------------------------------------------------------------------------
void CVehicleController::UpdateWheelPositions( void )
{
	if ( m_nVehicleType == VEHICLE_TYPE_CAR_RAYCAST )
	{
		m_pCarSystem->update_wheel_positions();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CVehicleController::CalcSteering( float dt, float speed, float steering, bool bAnalog )
{
	float degrees = RemapValClamped( speed, m_vehicleData.steering.speedSlow, m_vehicleData.steering.speedFast, m_vehicleData.steering.degreesSlow, m_vehicleData.steering.degreesFast );
	float speedGame = MPH_TO_GAMEVEL(speed);
	if ( speedGame > m_vehicleData.engine.maxSpeed )
	{
		degrees = RemapValClamped( speedGame, m_vehicleData.engine.maxSpeed, m_vehicleData.engine.boostMaxSpeed, m_vehicleData.steering.degreesFast, m_vehicleData.steering.degreesBoost );
	}
	if ( m_vehicleData.steering.steeringExponent != 0 )
	{
		float sign = steering < 0 ? -1 : 1;
		float absSteering = fabs(steering);
		if ( bAnalog )
		{
			// analog steering is directly mapped, not integrated, so go ahead and map the full range using the exponent
			// then clamp to the output cone - keeps stick position:turn rate constant
			// NOTE: Also hardcode exponent to 2 because we can't add a script entry at this point
			float output = pow(absSteering, 2.0f) * sign * m_vehicleData.steering.degreesSlow;
			return clamp(output, -degrees, degrees );
		}
		// digital steering is integrated, keep time to full turn rate constant
		return pow(absSteering, m_vehicleData.steering.steeringExponent) * sign * degrees;
	}
	return steering * degrees;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::CalcEngineTransmission( float flThrottle )
{
	// Automatic Transmission?
	if ( !m_vehicleData.engine.isAutoTransmission )
		return;

	// Calculate the average rotational speed of the vehicle's wheels.
	float flAvgRotSpeed = 0.0;
	for( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
	{
		float flRotSpeed = fabs( m_pCarSystem->get_wheel_angular_velocity( IVP_POS_WHEEL( iWheel ) ) );
		flAvgRotSpeed += flRotSpeed;
	}
	flAvgRotSpeed *= 0.5f / ( float )IVP_PI / m_wheelCount;
	
	float flEstEngineRPM = flAvgRotSpeed * m_vehicleData.engine.axleRatio * m_vehicleData.engine.gearRatio[m_currentState.gear] * 60;
	
	// Only shift up when going forward (throttling).
	if ( flThrottle > 0.0f )
	{
		// Shift up?, top gear is gearcount-1 (0 based)
		while ( ( flEstEngineRPM > m_vehicleData.engine.shiftUpRPM ) && ( m_currentState.gear < m_vehicleData.engine.gearCount-1 ) )
		{
			m_currentState.gear++;
			flEstEngineRPM = flAvgRotSpeed * m_vehicleData.engine.axleRatio * m_vehicleData.engine.gearRatio[m_currentState.gear] * 60;
		}
	}
	
	// Downshift?
	while ( ( flEstEngineRPM < m_vehicleData.engine.shiftDownRPM ) && ( m_currentState.gear > 0 ) )
	{
		m_currentState.gear--;
		flEstEngineRPM = flAvgRotSpeed * m_vehicleData.engine.axleRatio * m_vehicleData.engine.gearRatio[m_currentState.gear] * 60;
	}

	m_currentState.engineRPM = flEstEngineRPM;
}

//-----------------------------------------------------------------------------
// Purpose:
// throttle goes forward and backward, [-1, 1]
// brake_val [0..1]
//-----------------------------------------------------------------------------
void CVehicleController::CalcEngine( float throttle, float brake_val, bool handbrake, float steeringVal, bool torqueBoost )
{
	// Update the engine transmission.
	CalcEngineTransmission( throttle );
	
	// Get the speed of the vehicle.
    float flAbsSpeed = fabs( m_currentState.speed );

	// Speed governor
	if ( IsPC() )
	{
		float maxSpeed = torqueBoost ? m_vehicleData.engine.boostMaxSpeed : m_vehicleData.engine.maxSpeed;
		maxSpeed = max(1.f,maxSpeed);	// make sure this is non-zero before the divide
		if ( (throttle > 0) && (flAbsSpeed > maxSpeed) )
		{
			float frac = flAbsSpeed / maxSpeed;
			if ( frac > m_vehicleData.engine.autobrakeSpeedGain )
			{
				throttle = 0;
				brake_val = (frac - 1.0f) * m_vehicleData.engine.autobrakeSpeedFactor;
				if ( m_currentState.wheelsInContact == 0 )
				{
					brake_val = 0;
				}
			}
			throttle *= 0.1f;
		}
	}
	else	// consoles
	{
		if ( ( throttle > 0 ) && ( ( !torqueBoost && flAbsSpeed > (m_vehicleData.engine.maxSpeed * throttle) ) || 
			( torqueBoost && flAbsSpeed > m_vehicleData.engine.boostMaxSpeed) ) )
		{
			throttle *= 0.1f;
		}
	}

	// Check for reverse - both of these "governors" or horrible and need to be redone before we ship!
	if ( ( throttle < 0 ) && ( !torqueBoost && ( flAbsSpeed > m_vehicleData.engine.maxRevSpeed ) ) )
	{
		throttle *= 0.1f;
	}

    if ( throttle != 0.0 )
	{
		m_vehicleFlags &= ~FVEHICLE_THROTTLE_STOPPED;
		// calculate the force that propels the car
		const float watt_per_hp = 745.0f;
		const float seconds_per_minute = 60.0f;

		float wheel_force_by_throttle = throttle * 
			m_vehicleData.engine.horsepower * (watt_per_hp * seconds_per_minute) * 
			m_vehicleData.engine.gearRatio[m_currentState.gear]  * m_vehicleData.engine.axleRatio / 
			(m_vehicleData.engine.maxRPM * m_wheelRadius * (2 * IVP_PI));

		if ( m_currentState.engineRPM >= m_vehicleData.engine.maxRPM )
		{
			wheel_force_by_throttle = 0;
		}

		int wheelIndex = 0;
		for ( int i = 0; i < m_vehicleData.axleCount; i++ )
		{
			float axleFactor = m_vehicleData.axles[i].torqueFactor * m_torqueScale;

			float boostFactor = 0.5f;
			if ( torqueBoost && IsBoosting() )
			{
				// reduce the boost at low speeds and high turns since this usually just makes the tires spin
				// this means you only get the full boost when travelling in a straight line at high speed
				float speedFactor = RemapValClamped( flAbsSpeed, 0, m_vehicleData.engine.maxSpeed, 0.1f, 1.0f );
				float turnFactor = 1.0f - (fabs(steeringVal) * 0.95f);
				float dampedBoost = m_vehicleData.engine.boostForce * speedFactor * turnFactor;
				if ( dampedBoost > boostFactor )
				{
					boostFactor = dampedBoost;
				}
				//Msg("Boost applied %.2f, speed %.2f, turn %.2f\n", boostFactor, speedFactor, turnFactor );
			}
			float axleTorque = boostFactor * wheel_force_by_throttle * axleFactor * ConvertDistanceToIVP( m_vehicleData.axles[i].wheels.radius );

			for ( int w = 0; w < m_vehicleData.wheelsPerAxle; w++, wheelIndex++ )
			{
				float torqueVal = axleTorque;
				m_pCarSystem->change_wheel_torque((IVP_POS_WHEEL)wheelIndex, torqueVal);
			}
		}
	}
	else if ( brake_val != 0 )
	{
		m_vehicleFlags &= ~FVEHICLE_THROTTLE_STOPPED;

		// Brake to slow down the wheel.
		float wheel_force_by_brake = brake_val * m_gravityLength * ( m_bodyMass + m_totalWheelMass );
		
		float sign = m_currentState.speed >= 0.0f ? -1.0f : 1.0f;
		int wheelIndex = 0;
		for ( int i = 0; i < m_vehicleData.axleCount; i++ )
		{
			float torque_val = 0.5 * sign * wheel_force_by_brake * m_vehicleData.axles[i].brakeFactor * ConvertDistanceToIVP( m_vehicleData.axles[i].wheels.radius );
			for ( int w = 0; w < m_vehicleData.wheelsPerAxle; w++, wheelIndex++ )
			{
				m_pCarSystem->change_wheel_torque( ( IVP_POS_WHEEL )wheelIndex, torque_val );
			}
		}
	}
	else if ( !(m_vehicleFlags & FVEHICLE_THROTTLE_STOPPED) )
	{
		m_vehicleFlags |= FVEHICLE_THROTTLE_STOPPED;

		for ( int w = 0; w < m_wheelCount; w++ )
		{
			m_pCarSystem->change_wheel_torque((IVP_POS_WHEEL)w, 0);
		}
	}

	// Update the throttle - primarily for the airboat!
	m_pCarSystem->update_throttle( throttle );
}

//-----------------------------------------------------------------------------
// Purpose: Get debug rendering data from the ipion physics system.
//-----------------------------------------------------------------------------
void CVehicleController::GetCarSystemDebugData( vehicle_debugcarsystem_t &debugCarSystem )
{
	IVP_CarSystemDebugData_t carSystemDebugData;
	memset(&carSystemDebugData,0,sizeof(carSystemDebugData));
	m_pCarSystem->GetCarSystemDebugData( carSystemDebugData );

	// Raycast car wheel trace data.
	for ( int iWheel = 0; iWheel < VEHICLE_DEBUGRENDERDATA_MAX_WHEELS; ++iWheel )
	{
		debugCarSystem.vecWheelRaycasts[iWheel][0].x = carSystemDebugData.wheelRaycasts[iWheel][0].k[0];
		debugCarSystem.vecWheelRaycasts[iWheel][0].y = carSystemDebugData.wheelRaycasts[iWheel][0].k[1];
		debugCarSystem.vecWheelRaycasts[iWheel][0].z = carSystemDebugData.wheelRaycasts[iWheel][0].k[2];

		debugCarSystem.vecWheelRaycasts[iWheel][1].x = carSystemDebugData.wheelRaycasts[iWheel][1].k[0];
		debugCarSystem.vecWheelRaycasts[iWheel][1].y = carSystemDebugData.wheelRaycasts[iWheel][1].k[1];
		debugCarSystem.vecWheelRaycasts[iWheel][1].z = carSystemDebugData.wheelRaycasts[iWheel][1].k[2];

		debugCarSystem.vecWheelRaycastImpacts[iWheel] = debugCarSystem.vecWheelRaycasts[iWheel][0] + ( carSystemDebugData.wheelRaycastImpacts[iWheel] *
			                                            ( debugCarSystem.vecWheelRaycasts[iWheel][1] - debugCarSystem.vecWheelRaycasts[iWheel][0] ) );
	}

	ConvertPositionToHL( carSystemDebugData.backActuatorLeft, debugCarSystem.vecAxlePos[0] );
	ConvertPositionToHL( carSystemDebugData.backActuatorRight, debugCarSystem.vecAxlePos[1] );
	ConvertPositionToHL( carSystemDebugData.frontActuatorLeft, debugCarSystem.vecAxlePos[2] );
	// vecAxlePos only has three elements so this line is illegal. The mapping of actuators
	// to axles seems dodgy anyway.
	//ConvertPositionToHL( carSystemDebugData.frontActuatorRight, debugCarSystem.vecAxlePos[3] );
}


//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
void CVehicleController::WriteToTemplate( vphysics_save_cvehiclecontroller_t &controllerTemplate )
{
	// Get rid of the handbrake flag.  The car keeps the flag and will reset it fixing wheels, 
	// else the system thinks it already fixed the wheels on load and the car roles.
	m_vehicleFlags &= ~FVEHICLE_HANDBRAKE_ON;

	controllerTemplate.m_pCarBody = m_pCarBody;
	controllerTemplate.m_wheelCount = m_wheelCount;
	controllerTemplate.m_wheelRadius = m_wheelRadius;
	controllerTemplate.m_bodyMass = m_bodyMass;
	controllerTemplate.m_totalWheelMass = m_totalWheelMass;
	controllerTemplate.m_gravityLength = m_gravityLength;
	controllerTemplate.m_torqueScale = m_torqueScale;
	controllerTemplate.m_vehicleFlags = m_vehicleFlags;
	controllerTemplate.m_nTireType = m_nTireType;
	controllerTemplate.m_nVehicleType = m_nVehicleType;
	controllerTemplate.m_bTraceData = m_bTraceData;
	controllerTemplate.m_bOccupied = m_bOccupied;
	controllerTemplate.m_bEngineDisable = m_bEngineDisable;
	memcpy( &controllerTemplate.m_currentState, &m_currentState, sizeof(m_currentState) );
	memcpy( &controllerTemplate.m_vehicleData, &m_vehicleData, sizeof(m_vehicleData) );
	for (int i = 0; i < VEHICLE_MAX_WHEEL_COUNT; ++i )
	{
		controllerTemplate.m_pWheels[i] = m_pWheels[i];
		ConvertPositionToHL( m_wheelPosition_Bs[i], controllerTemplate.m_wheelPosition_Bs[i] );
		ConvertPositionToHL( m_tracePosition_Bs[i], controllerTemplate.m_tracePosition_Bs[i] );
	}
	m_flVelocity[0] = m_flVelocity[1] = m_flVelocity[2] = 0.0f;
	if ( m_pCarBody )
	{
		IVP_U_Float_Point &speed = m_pCarBody->GetObject()->get_core()->speed; 
		controllerTemplate.m_flVelocity[0] = speed.k[0];
		controllerTemplate.m_flVelocity[1] = speed.k[1];
		controllerTemplate.m_flVelocity[2] = speed.k[2];
	}

}

// JAY: Keep this around for now while we still have a bunch of games saved with the old 
// vehicle controls.  We won't ship this, but it lets us debug
#define OLD_SAVED_GAME 1

#if OLD_SAVED_GAME
#define SET_DEFAULT(x,y) { if ( x == 0 ) x = y; }
#endif

void CVehicleController::InitFromTemplate( CPhysicsEnvironment *pEnv, void *pGameData,
	IPhysicsGameTrace *pGameTrace, const vphysics_save_cvehiclecontroller_t &controllerTemplate )
{
	m_pEnv = pEnv;
	m_pGameTrace = pGameTrace;
	m_pCarBody = controllerTemplate.m_pCarBody;
	m_wheelCount = controllerTemplate.m_wheelCount;
	m_wheelRadius = controllerTemplate.m_wheelRadius;
	m_bodyMass = controllerTemplate.m_bodyMass;
	m_totalWheelMass = controllerTemplate.m_totalWheelMass;
	m_gravityLength = controllerTemplate.m_gravityLength;
	m_torqueScale = controllerTemplate.m_torqueScale;
	m_vehicleFlags = controllerTemplate.m_vehicleFlags;
	m_nTireType = controllerTemplate.m_nTireType;
	m_nVehicleType = controllerTemplate.m_nVehicleType;
	m_bTraceData = controllerTemplate.m_bTraceData;
	m_bOccupied = controllerTemplate.m_bOccupied;
	m_bEngineDisable = controllerTemplate.m_bEngineDisable;
	m_pCarSystem = NULL;
	memcpy( &m_currentState, &controllerTemplate.m_currentState, sizeof(m_currentState) );
	memcpy( &m_vehicleData, &controllerTemplate.m_vehicleData, sizeof(m_vehicleData) );
	memcpy( &m_flVelocity, controllerTemplate.m_flVelocity, sizeof(m_flVelocity) );

#if OLD_SAVED_GAME
	SET_DEFAULT( m_torqueScale, 1.0 );
	SET_DEFAULT( m_vehicleData.steering.steeringRateSlow, 4.5 );
	SET_DEFAULT( m_vehicleData.steering.steeringRateFast, 0.5 );
	SET_DEFAULT( m_vehicleData.steering.steeringRestRateSlow, 3.0 );
	SET_DEFAULT( m_vehicleData.steering.steeringRestRateFast, 1.8 );
	SET_DEFAULT( m_vehicleData.steering.speedSlow, m_vehicleData.engine.maxSpeed*0.25 );
	SET_DEFAULT( m_vehicleData.steering.speedFast, m_vehicleData.engine.maxSpeed*0.75 );
	SET_DEFAULT( m_vehicleData.steering.degreesSlow, 50 );
	SET_DEFAULT( m_vehicleData.steering.degreesFast, 18 );
	SET_DEFAULT( m_vehicleData.steering.degreesBoost, 10 );
	

	SET_DEFAULT( m_vehicleData.steering.turnThrottleReduceSlow, 0.3 );
	SET_DEFAULT( m_vehicleData.steering.turnThrottleReduceFast, 3 );
	SET_DEFAULT( m_vehicleData.steering.brakeSteeringRateFactor, 6 );
	SET_DEFAULT( m_vehicleData.steering.throttleSteeringRestRateFactor, 2 );
	SET_DEFAULT( m_vehicleData.steering.boostSteeringRestRateFactor, 1 );
	SET_DEFAULT( m_vehicleData.steering.boostSteeringRateFactor, 1 );
	SET_DEFAULT( m_vehicleData.steering.powerSlideAccel, 200 );

	SET_DEFAULT( m_vehicleData.engine.autobrakeSpeedGain, 1.0 );
	SET_DEFAULT( m_vehicleData.engine.autobrakeSpeedFactor, 2.0 );
#endif

	for (int i = 0; i < VEHICLE_MAX_WHEEL_COUNT; ++i )
	{
		m_pWheels[i] = controllerTemplate.m_pWheels[i];
		ConvertPositionToIVP( controllerTemplate.m_wheelPosition_Bs[i], m_wheelPosition_Bs[i] );
		ConvertPositionToIVP( controllerTemplate.m_tracePosition_Bs[i], m_tracePosition_Bs[i] );
	}

	CreateIVPObjects( );

	// HACKHACK: vehicle wheels don't have valid friction at startup, clearing the body's angular velocity keeps 
	// this fact from affecting the vehicle dynamics in any noticeable way
	// using growFriction will re-establish the contact point with moveable objects, but the friction that 
	// occurs afterward is not the same across the save even when that is extended to include static objects
	if ( m_pCarBody )
	{
		// clear angVel
		m_pCarBody->SetVelocity( NULL, &vec3_origin );
		m_pCarBody->GetObject()->get_core()->speed_change.set( m_flVelocity[0], m_flVelocity[1], m_flVelocity[2] );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::OnVehicleEnter( void )
{ 
	m_bOccupied = true; 

	if ( m_nVehicleType == VEHICLE_TYPE_AIRBOAT_RAYCAST )
	{
		float flDampSpeed = 0.0f;
		float flDampRotSpeed = 0.0f;
		m_pCarBody->SetDamping( &flDampSpeed, &flDampRotSpeed );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleController::OnVehicleExit( void )
{ 
	m_bOccupied = false; 

	// Reset the vehicle tires when exiting the vehicle.
	if ( m_vehicleData.steering.isSkidAllowed )
	{
		int iWheel = 0;
		for ( int iAxle = 0; iAxle < m_vehicleData.axleCount; ++iAxle )
		{
			for ( int iAxleWheel = 0; iAxleWheel < m_vehicleData.wheelsPerAxle; ++iAxleWheel, ++iWheel )
			{
				// Change back to normal tires.
				if ( m_nTireType != VEHICLE_TIRE_NORMAL )
				{
					m_pWheels[iWheel]->SetMaterialIndex( m_vehicleData.axles[iAxle].wheels.materialIndex );
				}

				m_pCarSystem->fix_wheel( ( IVP_POS_WHEEL )iWheel, IVP_TRUE );
			}
		}
		m_nTireType = VEHICLE_TIRE_NORMAL;
		m_currentState.skidSpeed = 0.0f;
	}

	if ( m_nVehicleType == VEHICLE_TYPE_AIRBOAT_RAYCAST ) 
	{
		float flDampSpeed = 1.0f;
		float flDampRotSpeed = 1.0f;
		m_pCarBody->SetDamping( &flDampSpeed, &flDampRotSpeed );
	}

	SetEngineDisabled( false );
}

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IPhysicsVehicleController *CreateVehicleController( CPhysicsEnvironment *pEnv, CPhysicsObject *pBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace )
{
	CVehicleController *pController = new CVehicleController( params, pEnv, nVehicleType, pGameTrace );
	pController->InitCarSystem( pBodyObject );
	return pController;
}

bool SavePhysicsVehicleController( const physsaveparams_t &params, CVehicleController *pVehicleController )
{
	vphysics_save_cvehiclecontroller_t controllerTemplate;
	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );

	pVehicleController->WriteToTemplate( controllerTemplate );
	params.pSave->WriteAll( &controllerTemplate );

	return true;
}

bool RestorePhysicsVehicleController( const physrestoreparams_t &params, CVehicleController **ppVehicleController )
{
	*ppVehicleController = new CVehicleController;
	
	vphysics_save_cvehiclecontroller_t controllerTemplate;
	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );
	params.pRestore->ReadAll( &controllerTemplate );

	(*ppVehicleController)->InitFromTemplate( static_cast<CPhysicsEnvironment *>(params.pEnvironment),
		params.pGameData, params.pGameTrace, controllerTemplate );

	return true;
}


