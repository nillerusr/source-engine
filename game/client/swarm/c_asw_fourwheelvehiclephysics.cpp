// Clientside vehicle physics

// asw, main changes over the serverside version:
//   all calls to m_pOuterServerVehicle removed (they were all sound calls)
//   wheel dust spawning removed
//   ndebug functions changed to debugoverlay

#include "cbase.h"
#include "c_asw_fourwheelvehiclephysics.h"
#include "engine/IEngineSound.h"
#include "soundenvelope.h"
#include "vcollide_parse.h"
#include "in_buttons.h"
#include "c_baseplayer.h"
#include "IEffects.h"
#include "physics_saverestore.h"
//#include "vehicle_base.h"
#include "isaverestore.h"
#include "movevars_shared.h"
//#include "te_effect_dispatch.h"
#include "engine/ivdebugoverlay.h"
//#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// the tires are considered to be skidding if they have sliding velocity of 10 in/s or more
const float DEFAULT_SKID_THRESHOLD = 10.0f;

#define DUST_SPEED			5		// speed at which dust starts
#define REAR_AXLE			1		// indexes of axlex
#define	FRONT_AXLE			0
#define MAX_GUAGE_SPEED		100.0	// 100 mph is max speed shown on guage

#define BRAKE_MAX_VALUE				1.0f
#define BRAKE_BACK_FORWARD_SCALAR	2.0f
ConVar r_asw_vehicleDrawDebug( "r_asw_vehicleDrawDebug", "0", FCVAR_CHEAT );
ConVar r_asw_vehicleBrakeRate( "r_asw_vehicleBrakeRate", "1.5", FCVAR_CHEAT );

// remaps an angular variable to a 3 band function:
// 0 <= t < start :		f(t) = 0
// start <= t <= end :	f(t) = end * spline(( t-start) / (end-start) )  // s curve between clamped and linear
// end < t :			f(t) = t
float RemapAngleRange( float startInterval, float endInterval, float value )
{
	// Fixup the roll
	value = AngleNormalize( value );
	float absAngle = fabs(value);

	// beneath cutoff?
	if ( absAngle < startInterval )
	{
		value = 0;
	}
	// in spline range?
	else if ( absAngle <= endInterval )
	{
		float newAngle = SimpleSpline( (absAngle - startInterval) / (endInterval-startInterval) ) * endInterval;
		// grab the sign from the initial value
		if ( value < 0 )
		{
			newAngle *= -1;
		}
		value = newAngle;
	}
	// else leave it alone, in linear range
	
	return value;
}

enum vehicle_pose_params
{
	VEH_FL_WHEEL_HEIGHT=0,
	VEH_FR_WHEEL_HEIGHT,
	VEH_RL_WHEEL_HEIGHT,
	VEH_RR_WHEEL_HEIGHT,
	VEH_FL_WHEEL_SPIN,
	VEH_FR_WHEEL_SPIN,
	VEH_RL_WHEEL_SPIN,
	VEH_RR_WHEEL_SPIN,
	VEH_STEER,
	VEH_ACTION,
	VEH_SPEEDO,

};


BEGIN_DATADESC_NO_BASE( C_ASW_FourWheelVehiclePhysics )

// These two are reset every time 
//	DEFINE_FIELD( m_pOuter, FIELD_EHANDLE ),
//											m_pOuterServerVehicle;

	// Quiet down classcheck
	// DEFINE_FIELD( m_controls, vehicle_controlparams_t ),	


	// Controls
	DEFINE_FIELD( m_controls.throttle, FIELD_FLOAT ),
	DEFINE_FIELD( m_controls.steering, FIELD_FLOAT ),
	DEFINE_FIELD( m_controls.brake, FIELD_FLOAT ),
	DEFINE_FIELD( m_controls.boost, FIELD_FLOAT ),
	DEFINE_FIELD( m_controls.handbrake, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_controls.handbrakeLeft, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_controls.handbrakeRight, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_controls.brakepedal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_controls.bHasBrakePedal, FIELD_BOOLEAN ),

	// This has to be handled by the containing class owing to 'owner' issues
//	DEFINE_PHYSPTR( m_pVehicle ),

	DEFINE_FIELD( m_nSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( m_nLastSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( m_nRPM, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastBoost, FIELD_FLOAT ),
	DEFINE_FIELD( m_nBoostTimeLeft, FIELD_INTEGER ),
	DEFINE_FIELD( m_nHasBoost, FIELD_INTEGER ),

	DEFINE_FIELD( m_maxThrottle, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxRevThrottle, FIELD_FLOAT ),
	DEFINE_FIELD( m_flThrottleReduction, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_actionSpeed, FIELD_FLOAT ),

	// This has to be handled by the containing class owing to 'owner' issues
//	DEFINE_PHYSPTR_ARRAY( m_pWheels ),

	DEFINE_FIELD( m_wheelCount, FIELD_INTEGER ),

	DEFINE_ARRAY( m_wheelPosition, FIELD_VECTOR, 4 ),
	DEFINE_ARRAY( m_wheelRotation, FIELD_VECTOR, 4 ),
	DEFINE_ARRAY( m_wheelBaseHeight, FIELD_FLOAT, 4 ),
	DEFINE_ARRAY( m_wheelTotalHeight, FIELD_FLOAT, 4 ),
	DEFINE_ARRAY( m_poseParameters, FIELD_INTEGER, 12 ),
	DEFINE_FIELD( m_actionValue, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_actionScale, FIELD_FLOAT, "actionScale" ),
	DEFINE_FIELD( m_debugRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_throttleRate, FIELD_FLOAT ),
	DEFINE_FIELD( m_throttleStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_throttleActiveTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_turboTimer, FIELD_FLOAT ),

	DEFINE_FIELD( m_flVehicleVolume, FIELD_FLOAT ),
	DEFINE_FIELD( m_bIsOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLastThrottle, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLastBoost, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLastSkid, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_nTurnLeftCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_nTurnRightCount, FIELD_INTEGER ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_ASW_FourWheelVehiclePhysics::C_ASW_FourWheelVehiclePhysics( C_BaseAnimating *pOuter )
{
	m_flVehicleVolume = 0.5;
	m_pOuter = NULL;
	//m_pOuterServerVehicle = NULL;	// asw comment
	m_flMaxSpeed = 30;
	m_flThrottleReduction = 0;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
C_ASW_FourWheelVehiclePhysics::~C_ASW_FourWheelVehiclePhysics ()
{
	physenv->DestroyVehicleController( m_pVehicle );
}

//-----------------------------------------------------------------------------
// A couple wrapper methods to perform common operations
//-----------------------------------------------------------------------------
inline int C_ASW_FourWheelVehiclePhysics::LookupPoseParameter( const char *szName )
{
	return m_pOuter->LookupPoseParameter( szName );
}

inline float C_ASW_FourWheelVehiclePhysics::GetPoseParameter( int iParameter )
{
	return m_pOuter->GetPoseParameter( iParameter );
}

inline float C_ASW_FourWheelVehiclePhysics::SetPoseParameter( int iParameter, float flValue )
{
	return m_pOuter->SetPoseParameter( iParameter, flValue );
}

inline bool C_ASW_FourWheelVehiclePhysics::GetAttachment( const char *szName, Vector &origin, QAngle &angles )
{
	return m_pOuter->GetAttachment( szName, origin, angles );
}

//-----------------------------------------------------------------------------
// Methods related to spawn
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::InitializePoseParameters()
{
	m_poseParameters[VEH_FL_WHEEL_HEIGHT] = LookupPoseParameter( "vehicle_wheel_fl_height" );
	m_poseParameters[VEH_FR_WHEEL_HEIGHT] = LookupPoseParameter( "vehicle_wheel_fr_height" );
	m_poseParameters[VEH_RL_WHEEL_HEIGHT] = LookupPoseParameter( "vehicle_wheel_rl_height" );
	m_poseParameters[VEH_RR_WHEEL_HEIGHT] = LookupPoseParameter( "vehicle_wheel_rr_height" );
	m_poseParameters[VEH_FL_WHEEL_SPIN] = LookupPoseParameter( "vehicle_wheel_fl_spin" );
	m_poseParameters[VEH_FR_WHEEL_SPIN] = LookupPoseParameter( "vehicle_wheel_fr_spin" );
	m_poseParameters[VEH_RL_WHEEL_SPIN] = LookupPoseParameter( "vehicle_wheel_rl_spin" );
	m_poseParameters[VEH_RR_WHEEL_SPIN] = LookupPoseParameter( "vehicle_wheel_rr_spin" );
	m_poseParameters[VEH_STEER] = LookupPoseParameter( "vehicle_steer" );
	m_poseParameters[VEH_ACTION] = LookupPoseParameter( "vehicle_action" );
	m_poseParameters[VEH_SPEEDO] = LookupPoseParameter( "vehicle_guage" );


	// move the wheels to a neutral position
	SetPoseParameter( m_poseParameters[VEH_SPEEDO], 0 );
	SetPoseParameter( m_poseParameters[VEH_STEER], 0 );
	SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_FR_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RR_WHEEL_HEIGHT], 0 );
	m_pOuter->InvalidateBoneCache();
}

//-----------------------------------------------------------------------------
// Purpose: Parses the vehicle's script
//-----------------------------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::ParseVehicleScript( const char *pScriptName, solid_t &solid, vehicleparams_t &vehicle)
{
	byte *pFile = UTIL_LoadFileForMe( pScriptName, NULL );
	if ( !pFile )
		return false;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( (char *)pFile );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "vehicle" ) )
		{
			pParse->ParseVehicle( &vehicle, NULL );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );

	UTIL_FreeFile( pFile );

	m_debugRadius = vehicle.axles[0].wheels.radius;
	CalcWheelData( vehicle );

	PhysModelParseSolid( solid, m_pOuter, m_pOuter->GetModelIndex() );
	
	// Allow the script to shift the center of mass
	if ( vehicle.body.massCenterOverride != vec3_origin )
	{
		solid.massCenterOverride = vehicle.body.massCenterOverride;
		solid.params.massCenterOverride = &solid.massCenterOverride;
	}

	// allow script to change the mass of the vehicle body
	if ( vehicle.body.massOverride > 0 )
	{
		solid.params.mass = vehicle.body.massOverride;
	}

	return true;
}

void C_ASW_FourWheelVehiclePhysics::CalcWheelData( vehicleparams_t &vehicle )
{
	Vector left, right;
	QAngle dummy;
	SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_FR_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RR_WHEEL_HEIGHT], 0 );
	m_pOuter->InvalidateBoneCache();
	if ( GetAttachment( "wheel_fl", left, dummy ) && GetAttachment( "wheel_fr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		Vector center = (left + right) * 0.5;
		vehicle.axles[0].offset = center;
		vehicle.axles[0].wheelOffset = right - center;
		// Cache the base height of the wheels in body space
		m_wheelBaseHeight[0] = left.z;
		m_wheelBaseHeight[1] = right.z;
	}

	if ( GetAttachment( "wheel_rl", left, dummy ) && GetAttachment( "wheel_rr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		Vector center = (left + right) * 0.5;
		vehicle.axles[1].offset = center;
		vehicle.axles[1].wheelOffset = right - center;
		// Cache the base height of the wheels in body space
		m_wheelBaseHeight[2] = left.z;
		m_wheelBaseHeight[3] = right.z;
	}
	SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_HEIGHT], 1 );
	SetPoseParameter( m_poseParameters[VEH_FR_WHEEL_HEIGHT], 1 );
	SetPoseParameter( m_poseParameters[VEH_RL_WHEEL_HEIGHT], 1 );
	SetPoseParameter( m_poseParameters[VEH_RR_WHEEL_HEIGHT], 1 );
	m_pOuter->InvalidateBoneCache();
	if ( GetAttachment( "wheel_fl", left, dummy ) && GetAttachment( "wheel_fr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		// Cache the height range of the wheels in body space
		m_wheelTotalHeight[0] = m_wheelBaseHeight[0] - left.z;
		m_wheelTotalHeight[1] = m_wheelBaseHeight[1] - right.z;
		vehicle.axles[0].wheels.springAdditionalLength = m_wheelTotalHeight[0];
	}

	if ( GetAttachment( "wheel_rl", left, dummy ) && GetAttachment( "wheel_rr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		// Cache the height range of the wheels in body space
		m_wheelTotalHeight[2] = m_wheelBaseHeight[0] - left.z;
		m_wheelTotalHeight[3] = m_wheelBaseHeight[1] - right.z;
		vehicle.axles[1].wheels.springAdditionalLength = m_wheelTotalHeight[2];
	}
	SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_FR_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RL_WHEEL_HEIGHT], 0 );
	SetPoseParameter( m_poseParameters[VEH_RR_WHEEL_HEIGHT], 0 );
	m_pOuter->InvalidateBoneCache();

	// Get raytrace offsets if they exist.
	if ( GetAttachment( "raytrace_fl", left, dummy ) && GetAttachment( "raytrace_fr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		Vector center = ( left + right ) * 0.5;
		vehicle.axles[0].raytraceCenterOffset = center;
		vehicle.axles[0].raytraceOffset = right - center;
	}

	if ( GetAttachment( "raytrace_rl", left, dummy ) && GetAttachment( "raytrace_rr", right, dummy ) )
	{
		VectorITransform( left, m_pOuter->EntityToWorldTransform(), left );
		VectorITransform( right, m_pOuter->EntityToWorldTransform(), right );
		Vector center = ( left + right ) * 0.5;
		vehicle.axles[1].raytraceCenterOffset = center;
		vehicle.axles[1].raytraceOffset = right - center;
	}
}


//-----------------------------------------------------------------------------
// Spawns the vehicle
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::Spawn( )
{
	Assert( m_pOuter );

	m_actionValue = 0;
	m_actionSpeed = 0;

	m_bIsOn = true;	// asw start with it on for now
	m_controls.handbrake = false;
	m_controls.handbrakeLeft = false;
	m_controls.handbrakeRight = false;
	m_controls.bHasBrakePedal = true;
	SetMaxThrottle( 1.0 );
	SetMaxReverseThrottle( -1.0f );

	InitializePoseParameters();	
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the vehicle physics
//			Called by our outer vehicle in it's Spawn()
//-----------------------------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::Initialize( const char *pVehicleScript, unsigned int nVehicleType )
{
	// Ok, turn on the simulation now
	// FIXME: Disabling collisions here is necessary because we seem to be
	// getting a one-frame collision between the old + new collision models
	if ( m_pOuter->VPhysicsGetObject() )
	{
		m_pOuter->VPhysicsGetObject()->EnableCollisions(false);
	}
	m_pOuter->VPhysicsDestroyObject();

	// Create the vphysics model + teleport it into position
	solid_t solid;
	vehicleparams_t vehicle;
	if (!ParseVehicleScript( pVehicleScript, solid, vehicle ))
	{
		Assert(0);
		//UTIL_Remove(m_pOuter);	// asw comment
		//m_pOuter->Remove();
		return false;
	}

	// NOTE: this needs to be greater than your max framerate (so zero is still instant)
	m_throttleRate = 10000.0;
	if ( vehicle.engine.throttleTime > 0 )
	{
		m_throttleRate = 1.0 / vehicle.engine.throttleTime;
	}

	m_flMaxSpeed = vehicle.engine.maxSpeed;

	IPhysicsObject *pBody = m_pOuter->VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &solid );
	PhysSetGameFlags( pBody, FVPHYSICS_NO_SELF_COLLISIONS | FVPHYSICS_MULTIOBJECT_ENTITY );
	m_pVehicle = physenv->CreateVehicleController( pBody, vehicle, nVehicleType, physgametrace );
	m_wheelCount = m_pVehicle->GetWheelCount();
	for ( int i = 0; i < m_wheelCount; i++ )
	{
		m_pWheels[i] = m_pVehicle->GetWheel( i );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Various steering parameters
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetThrottle( float flThrottle )
{
	m_controls.throttle = flThrottle;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetMaxThrottle( float flMaxThrottle )
{
	m_maxThrottle = flMaxThrottle;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetMaxReverseThrottle( float flMaxThrottle )
{
	m_flMaxRevThrottle = flMaxThrottle;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetSteering( float flSteering, float flSteeringRate )
{
	if ( !flSteeringRate )
	{
		m_controls.steering = flSteering;
	}
	else
	{
		m_controls.steering = Approach( flSteering, m_controls.steering, flSteeringRate );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetSteeringDegrees( float flDegrees )
{
	vehicleparams_t &vehicleParams = m_pVehicle->GetVehicleParamsForChange();
	vehicleParams.steering.degreesSlow = flDegrees;
	vehicleParams.steering.degreesFast = flDegrees;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetAction( float flAction )
{
	m_actionSpeed = flAction;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::TurnOn( )
{
	if ( IsEngineDisabled() )
		return;

	if ( !m_bIsOn )
	{
		//m_pOuterServerVehicle->SoundStart();	// asw comment
		m_bIsOn = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::TurnOff( )
{
	ResetControls();

	if ( m_bIsOn )
	{
		//m_pOuterServerVehicle->SoundShutdown();	// asw comment
		m_bIsOn = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetBoost( float flBoost )
{
	if ( !IsEngineDisabled() )
	{
		m_controls.boost = flBoost;
	}
}

//------------------------------------------------------
// UpdateBooster - Calls UpdateBooster() in the vphysics
// code to allow the timer to be updated
//
// Returns: false if timer has expired (can use again and
//			can stop think
//			true if timer still running
//------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::UpdateBooster( float flFrameTime )
{
	float retval = m_pVehicle->UpdateBooster(flFrameTime );
	return ( retval > 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetHasBrakePedal( bool bHasBrakePedal )
{
	m_controls.bHasBrakePedal = bHasBrakePedal;
}

//-----------------------------------------------------------------------------
// Teleport
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::Teleport( matrix3x4_t& relativeTransform )
{
	// We basically just have to make sure the wheels are in the right place
	// after teleportation occurs

	for ( int i = 0; i < m_wheelCount; i++ )
	{
		matrix3x4_t matrix, newMatrix;
		m_pWheels[i]->GetPositionMatrix( &matrix );
		ConcatTransforms( relativeTransform, matrix, newMatrix );
		m_pWheels[i]->SetPositionMatrix( newMatrix, true );
	}

}

#if 1
// For the #if 0 debug code below!
#define HL2IVP_FACTOR	METERS_PER_INCH
#define IVP2HL(x)		(float)(x * (1.0f/HL2IVP_FACTOR))
#define HL2IVP(x)		(double)(x * HL2IVP_FACTOR)		
#endif

//-----------------------------------------------------------------------------
// Debugging methods
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::DrawDebugGeometryOverlays()
{
	Vector vecRad(m_debugRadius,m_debugRadius,m_debugRadius);
	for ( int i = 0; i < m_wheelCount; i++ )
	{
		debugoverlay->AddBoxOverlay(m_wheelPosition[i], -vecRad, vecRad, m_wheelRotation[i], 0, 255, 45, 0 ,0);	// asw comment
	}

	for ( int iWheel = 0; iWheel < m_wheelCount; iWheel++ )
	{
		IPhysicsObject *pWheel = m_pVehicle->GetWheel( iWheel );
		
		Vector vecPos;
		QAngle vecRot;
		pWheel->GetPosition( &vecPos, &vecRot );

		debugoverlay->AddBoxOverlay( vecPos, -vecRad, vecRad, vecRot, 0, 255, 45, 0 ,0 );		// asw comment
	}

#if 1
	// Render vehicle data.
	IPhysicsObject *pBody = m_pOuter->VPhysicsGetObject();
	if ( pBody )
	{
		const vehicleparams_t vehicleParams = m_pVehicle->GetVehicleParams();

		// Draw a red cube as the "center" of the vehicle.
		Vector vecBodyPosition; 
		QAngle angBodyDirection;
		pBody->GetPosition( &vecBodyPosition, &angBodyDirection );
		debugoverlay->AddBoxOverlay( vecBodyPosition, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), angBodyDirection, 255, 0, 0, 0 ,0 );	// asw comment

		matrix3x4_t matrix;
		AngleMatrix( angBodyDirection, vecBodyPosition, matrix );

		// Draw green cubes at axle centers.
		Vector vecAxlePositions[2], vecAxlePositionsHL[2];
		vecAxlePositions[0] = vehicleParams.axles[0].offset;
		vecAxlePositions[1] = vehicleParams.axles[1].offset;

		VectorTransform( vecAxlePositions[0], matrix, vecAxlePositionsHL[0] );		
		VectorTransform( vecAxlePositions[1], matrix, vecAxlePositionsHL[1] );

		debugoverlay->AddBoxOverlay( vecAxlePositionsHL[0], Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), angBodyDirection, 0, 255, 0, 0 ,0 );	// asw comment
		debugoverlay->AddBoxOverlay( vecAxlePositionsHL[1], Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), angBodyDirection, 0, 255, 0, 0 ,0 );	// asw comment

		// Draw blue cubes at wheel centers.
		Vector vecWheelPositions[4], vecWheelPositionsHL[4];
		vecWheelPositions[0] = vehicleParams.axles[0].offset;
		vecWheelPositions[0] += vehicleParams.axles[0].wheelOffset;
		vecWheelPositions[1] = vehicleParams.axles[0].offset;
		vecWheelPositions[1] -= vehicleParams.axles[0].wheelOffset;
		vecWheelPositions[2] = vehicleParams.axles[1].offset;
		vecWheelPositions[2] += vehicleParams.axles[1].wheelOffset;
		vecWheelPositions[3] = vehicleParams.axles[1].offset;
		vecWheelPositions[3] -= vehicleParams.axles[1].wheelOffset;

		VectorTransform( vecWheelPositions[0], matrix, vecWheelPositionsHL[0] );
		VectorTransform( vecWheelPositions[1], matrix, vecWheelPositionsHL[1] );
		VectorTransform( vecWheelPositions[2], matrix, vecWheelPositionsHL[2] );
		VectorTransform( vecWheelPositions[3], matrix, vecWheelPositionsHL[3] );

		float flWheelRadius = vehicleParams.axles[0].wheels.radius;
		flWheelRadius = IVP2HL( flWheelRadius );
		Vector vecWheelRadius( flWheelRadius, flWheelRadius, flWheelRadius );

		debugoverlay->AddBoxOverlay( vecWheelPositionsHL[0], -vecWheelRadius, vecWheelRadius, angBodyDirection, 0, 0, 255, 0 ,0 );	// asw comment
		debugoverlay->AddBoxOverlay( vecWheelPositionsHL[1], -vecWheelRadius, vecWheelRadius, angBodyDirection, 0, 0, 255, 0 ,0 );	// asw comment
		debugoverlay->AddBoxOverlay( vecWheelPositionsHL[2], -vecWheelRadius, vecWheelRadius, angBodyDirection, 0, 0, 255, 0 ,0 );	// asw comment
		debugoverlay->AddBoxOverlay( vecWheelPositionsHL[3], -vecWheelRadius, vecWheelRadius, angBodyDirection, 0, 0, 255, 0 ,0 );	// asw comment

		// Draw wheel raycasts in yellow
		vehicle_debugcarsystem_t debugCarSystem;
		m_pVehicle->GetCarSystemDebugData( debugCarSystem );
		for ( int iWheel = 0; iWheel < 4; ++iWheel )
		{
			Vector vecStart, vecEnd, vecImpact;

			// Hack for now.
			float tmpY = IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][0].z );
			vecStart.z = -IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][0].y );
			vecStart.y = tmpY;
			vecStart.x = IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][0].x );

			tmpY = IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][1].z );
			vecEnd.z = -IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][1].y );
			vecEnd.y = tmpY;
			vecEnd.x = IVP2HL( debugCarSystem.vecWheelRaycasts[iWheel][1].x );

			tmpY = IVP2HL( debugCarSystem.vecWheelRaycastImpacts[iWheel].z );
			vecImpact.z = -IVP2HL( debugCarSystem.vecWheelRaycastImpacts[iWheel].y );
			vecImpact.y = tmpY;
			vecImpact.x = IVP2HL( debugCarSystem.vecWheelRaycastImpacts[iWheel].x );
			
			debugoverlay->AddBoxOverlay( vecStart, Vector( -1 , -1, -1 ), Vector( 1, 1, 1 ), angBodyDirection, 0, 255, 0, 0, 0  );	// asw comment			
			debugoverlay->AddLineOverlay( vecStart, vecEnd, 255, 255, 0, true, 0 );	// asw comment
			debugoverlay->AddBoxOverlay( vecEnd, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), angBodyDirection, 255, 0, 0, 0, 0 );	// asw comment
			debugoverlay->AddBoxOverlay( vecImpact, Vector( -0.5f , -0.5f, -0.5f ), Vector( 0.5f, 0.5f, 0.5f ), angBodyDirection, 0, 0, 255, 0, 0  );	// asw comment
		}
	}
#endif
}

int C_ASW_FourWheelVehiclePhysics::DrawDebugTextOverlays( int nOffset )
{
	const vehicle_operatingparams_t &params = m_pVehicle->GetOperatingParams();
	char tempstr[512];
	Q_snprintf( tempstr,sizeof(tempstr), "Speed %.1f  T/S/B (%.0f/%.0f/%.1f)", params.speed, m_controls.throttle, m_controls.steering, m_controls.brake );	
	debugoverlay->AddEntityTextOverlay(m_pOuter->entindex(), nOffset, 0, 255, 255, 255, 255, tempstr);	
	nOffset++;
	Msg( "%s", tempstr );

	Q_snprintf( tempstr,sizeof(tempstr), "Gear: %d, RPM %4d", params.gear, (int)params.engineRPM );
	debugoverlay->AddEntityTextOverlay(m_pOuter->entindex(), nOffset, 0, 255, 255, 255, 255, tempstr);
	nOffset++;
	Msg( " %s\n", tempstr );

	return nOffset;
}

//----------------------------------------------------
// Place dust at vector passed in
//----------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::PlaceWheelDust( int wheelIndex, bool ignoreSpeed )
{
	Vector	vecPos, vecVel;
	m_pVehicle->GetWheelContactPoint( wheelIndex, &vecPos, NULL );

	vecVel.Random( -1.0f, 1.0f );
	vecVel.z = random->RandomFloat( 0.3f, 1.0f );
	
	VectorNormalize( vecVel );

	// Higher speeds make larger dust clouds
	float flSize;
	if ( ignoreSpeed )
	{
		flSize = 1.0f;
	}
	else
	{
		flSize = RemapValClamped( m_nSpeed, DUST_SPEED, m_flMaxSpeed, 0.0f, 1.0f );
	}

	if ( flSize )
	{
		// asw comment
		/*
		CEffectData	data;

		data.m_vOrigin = vecPos;
		data.m_vNormal = vecVel;
		data.m_flScale = flSize;

		DispatchEffect( "WheelDust", data );
		*/
	}
}

//-----------------------------------------------------------------------------
// Frame-based updating 
//-----------------------------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::Think(float flTime)
{
	if (!m_pVehicle)
		return false;

	//Msg("[C] physics thinking for %f tickcount %d\n", flTime, gpGlobals->tickcount);

	// Update sound + physics state
	const vehicle_operatingparams_t &carState = m_pVehicle->GetOperatingParams();
	const vehicleparams_t &vehicleData = m_pVehicle->GetVehicleParams();

	// Set save data.
	float carSpeed = fabs( INS2MPH( carState.speed ) );
	m_nLastSpeed = m_nSpeed;
	m_nSpeed = ( int )carSpeed;
	m_nRPM = ( int )carState.engineRPM;
	m_nHasBoost = vehicleData.engine.boostDelay;	// if we have any boost delay, vehicle has boost ability

	m_pVehicle->Update( flTime, m_controls);

	// boost sounds
	if( IsBoosting() && !m_bLastBoost )
	{
		m_bLastBoost = true;
		m_turboTimer = gpGlobals->curtime + 2.75f;		// min duration for turbo sound
	}
	else if( !IsBoosting() && m_bLastBoost )
	{
		if ( gpGlobals->curtime >= m_turboTimer )
		{
			m_bLastBoost = false;
		}
	}

	m_fLastBoost = carState.boostDelay;
	m_nBoostTimeLeft =  carState.boostTimeLeft;

	// UNDONE: Use skid info from the physics system?
	// Only check wheels if we're not being carried by a dropship
	if ( m_pOuter->VPhysicsGetObject() && !m_pOuter->VPhysicsGetObject()->GetShadowController() )
	{
		const float skidFactor = 0.15f;
		const float minSpeed = DEFAULT_SKID_THRESHOLD / skidFactor;
		// we have to slide at least 15% of our speed at higher speeds to make the skid sound (otherwise it can be too frequent)
		float skidThreshold = m_bLastSkid ? DEFAULT_SKID_THRESHOLD : (carState.speed * 0.15f);
		if ( skidThreshold < DEFAULT_SKID_THRESHOLD )
		{
			// otherwise, ramp in the skid threshold to avoid the sound at really low speeds unless really skidding
			skidThreshold = RemapValClamped( fabs(carState.speed), 0, minSpeed, DEFAULT_SKID_THRESHOLD*8, DEFAULT_SKID_THRESHOLD );
		}
		// check for skidding, if we're skidding, need to play the sound
		if ( carState.skidSpeed > skidThreshold && m_bIsOn )
		{
			if ( !m_bLastSkid )	// only play sound once
			{
				m_bLastSkid = true;
				CPASAttenuationFilter filter( m_pOuter );
				//m_pOuterServerVehicle->PlaySound( VS_SKID_FRICTION_NORMAL );	// asw comment
			}

			// kick up dust from the wheels while skidding
			for ( int i = 0; i < 4; i++ )
			{
				PlaceWheelDust( i, true );
			}
		}
		else if ( m_bLastSkid == true )
		{
			m_bLastSkid = false;
			//m_pOuterServerVehicle->StopSound( VS_SKID_FRICTION_NORMAL );	// asw comment
		}

		// toss dust up from the wheels of the vehicle if we're moving fast enough
		if ( m_nSpeed >= DUST_SPEED && vehicleData.steering.dustCloud && m_bIsOn )
		{
			for ( int i = 0; i < 4; i++ )
			{
				PlaceWheelDust( i );
			}
		}
	}

	// Make the steering wheel match the input, with a little dampening.
	#define STEER_DAMPING	0.8
	//float flSteer = 
		GetPoseParameter( m_poseParameters[VEH_STEER] );
	//SetPoseParameter( m_poseParameters[VEH_STEER], (STEER_DAMPING * flSteer) + ((1 - STEER_DAMPING) * m_controls.steering) );
	SetPoseParameter( m_poseParameters[VEH_STEER], m_controls.steering);

	m_actionValue += m_actionSpeed * m_actionScale * flTime;
	SetPoseParameter( m_poseParameters[VEH_ACTION], m_actionValue );

	// setup speedometer
	if ( m_bIsOn == true )
	{
		float displaySpeed = m_nSpeed / MAX_GUAGE_SPEED;
		SetPoseParameter( m_poseParameters[VEH_SPEEDO], displaySpeed );
	}

	return m_bIsOn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	if ( r_asw_vehicleDrawDebug.GetInt() )
	{
		DrawDebugGeometryOverlays();
	}

	// must be a wheel
	if ( pPhysics == m_pOuter->VPhysicsGetObject() )
		return true;

	// This is here so we can make the pose parameters of the wheels
	// reflect their current physics state
	for ( int i = 0; i < m_wheelCount; i++ )
	{
		if ( pPhysics == m_pWheels[i] )
		{
			Vector tmp;
			pPhysics->GetPosition( &m_wheelPosition[i], &m_wheelRotation[i] );

			// transform the wheel into body space
			VectorITransform( m_wheelPosition[i], m_pOuter->EntityToWorldTransform(), tmp );
			SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_HEIGHT + i], (m_wheelBaseHeight[i] - tmp.z) / m_wheelTotalHeight[i] );
			SetPoseParameter( m_poseParameters[VEH_FL_WHEEL_SPIN + i], -m_wheelRotation[i].z );
			return false;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Shared code to compute the vehicle view position
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::GetVehicleViewPosition( const char *pViewAttachment, float flPitchFactor, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	matrix3x4_t vehicleEyePosToWorld;
	Vector vehicleEyeOrigin;
	QAngle vehicleEyeAngles;
	GetAttachment( pViewAttachment, vehicleEyeOrigin, vehicleEyeAngles );
	AngleMatrix( vehicleEyeAngles, vehicleEyePosToWorld );

#ifdef HL2_DLL
	// View dampening.
	if ( r_VehicleViewDampen.GetInt() )
	{
		//m_pOuterServerVehicle->GetFourWheelVehicle()->DampenEyePosition( vehicleEyeOrigin, vehicleEyeAngles );	// asw comment
	}
#endif

	// Compute the relative rotation between the unperterbed eye attachment + the eye angles
	matrix3x4_t cameraToWorld;
	AngleMatrix( *pAbsAngles, cameraToWorld );

	matrix3x4_t worldToEyePos;
	MatrixInvert( vehicleEyePosToWorld, worldToEyePos );

	matrix3x4_t vehicleCameraToEyePos;
	ConcatTransforms( worldToEyePos, cameraToWorld, vehicleCameraToEyePos );

	// Now perterb the attachment point
	vehicleEyeAngles.x = RemapAngleRange( PITCH_CURVE_ZERO * flPitchFactor, PITCH_CURVE_LINEAR, vehicleEyeAngles.x );
	vehicleEyeAngles.z = RemapAngleRange( ROLL_CURVE_ZERO * flPitchFactor, ROLL_CURVE_LINEAR, vehicleEyeAngles.z );
	AngleMatrix( vehicleEyeAngles, vehicleEyeOrigin, vehicleEyePosToWorld );

	// Now treat the relative eye angles as being relative to this new, perterbed view position...
	matrix3x4_t newCameraToWorld;
	ConcatTransforms( vehicleEyePosToWorld, vehicleCameraToEyePos, newCameraToWorld );

	// output new view abs angles
	MatrixAngles( newCameraToWorld, *pAbsAngles );

	// UNDONE: *pOrigin would already be correct in single player if the HandleView() on the server ran after vphysics
	MatrixGetColumn( newCameraToWorld, 3, *pAbsOrigin );
}


//-----------------------------------------------------------------------------
// Control initialization
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::ResetControls()
{
	m_controls.handbrake = true;
	m_controls.handbrakeLeft = false;
	m_controls.handbrakeRight = false;
	m_controls.boost = 0;
	m_controls.brake = 0.0f;
	m_controls.throttle = 0;
	m_controls.steering = 0;
}

void C_ASW_FourWheelVehiclePhysics::ReleaseHandbrake()
{
	m_controls.handbrake = false;
}

void C_ASW_FourWheelVehiclePhysics::SetHandbrake( bool bBrake )
{
	m_controls.handbrake = bBrake;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::EnableMotion( void )
{
	for( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
	{
		m_pWheels[iWheel]->EnableMotion( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::DisableMotion( void )
{
	Vector vecZero( 0.0f, 0.0f, 0.0f );
	AngularImpulse angNone( 0.0f, 0.0f, 0.0f );

	for( int iWheel = 0; iWheel < m_wheelCount; ++iWheel )
	{
		m_pWheels[iWheel]->SetVelocity( &vecZero, &angNone );
		m_pWheels[iWheel]->EnableMotion( false );
	}
}

float C_ASW_FourWheelVehiclePhysics::GetHLSpeed() const
{
	const vehicle_operatingparams_t &carState = m_pVehicle->GetOperatingParams();
	return carState.speed;
}

float C_ASW_FourWheelVehiclePhysics::GetSteering() const
{
	return m_controls.steering;
}

float C_ASW_FourWheelVehiclePhysics::GetSteeringDegrees() const
{
	const vehicleparams_t vehicleParams = m_pVehicle->GetVehicleParams();
	return vehicleParams.steering.degreesSlow;
}

#define STEERING_BASE_RATE	2.0f
#define STEERING_REST_DECAY	0.5f
#define STEERING_REST_EPS	0.001f	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SteeringRest( float carSpeed, const vehicleparams_t &vehicleData, float flFrameTime )
{
	float flSteeringRate = RemapValClamped( carSpeed, vehicleData.steering.speedSlow, vehicleData.steering.speedFast, 
		vehicleData.steering.steeringRestRateSlow, vehicleData.steering.steeringRestRateFast );
	m_controls.steering = Approach(0, m_controls.steering, flSteeringRate * gpGlobals->frametime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SteeringTurn( float carSpeed, const vehicleparams_t &vehicleData, bool bTurnLeft, float flFrameTime )
{
	float flSteeringRate = STEERING_BASE_RATE;

	if ( bTurnLeft )
	{		
		// TODO: change the log function to an approx. 
		m_nTurnLeftCount = clamp( m_nTurnLeftCount, 2, 30 );
		flSteeringRate *= log( (float) m_nTurnLeftCount );
		flSteeringRate *= flFrameTime;

		SetSteering( -1, flSteeringRate );

		m_nTurnLeftCount++;
		m_nTurnRightCount = 2;
	}
	else
	{
		// TODO: change the log function to an approx. 
		m_nTurnRightCount = clamp( m_nTurnRightCount, 2, 30 );
		flSteeringRate *= log( (float) m_nTurnRightCount );
		flSteeringRate *= flFrameTime;

		SetSteering( 1, flSteeringRate );

		m_nTurnLeftCount = 2;
		m_nTurnRightCount++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SteeringTurnAnalog( float carSpeed, const vehicleparams_t &vehicleData, float sidemove, float flFrameTime )
{

	// OLD Code
#if 0
	float flSteeringRate = STEERING_BASE_RATE;

	float factor = clamp( fabs( sidemove ) / 400.0f, 0.0f, 1.0f );

	factor *= 30;
	flSteeringRate *= log( factor );
	flSteeringRate *= flFrameTime;

	SetSteering( sidemove < 0.0f ? -1 : 1, flSteeringRate );
#else
	// This is tested with gamepads with analog sticks.  It gives full analog control
	// allowing the player to hold shallow turns.
	float steering = sidemove / 400.0f;
	steering = clamp( steering, -1.0f, 1.0f );
	SetSteering( steering, 0 );
#endif

	// Neutralize
	m_nTurnLeftCount = 2;
	m_nTurnRightCount = 2;
}

//-----------------------------------------------------------------------------
// Methods related to actually driving the vehicle
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::UpdateDriverControls( CUserCmd *cmd, float flFrameTime )
{
	int nButtons = cmd->buttons;

	//Msg("[C] UpdateDriverControls frametime %f command number %d\n", flFrameTime, cmd->command_number);

	// asw simple controls test
	/*
	m_controls.steering = cmd->sidemove / 400.0f;
	m_controls.throttle = cmd->forwardmove / 400.0f;
	m_controls.brake = 0; //(cmd->forwardmove < 0) ? -cmd->forwardmove / 400.0f : 0;
	m_controls.boost = 0;
	m_controls.handbrake = false;
	m_controls.handbrakeLeft = false;
	m_controls.handbrakeRight = false;
	m_controls.brakepedal = false;

	//Msg("   [C]\tt=%f\t\ts=%f\t\tb=%f\n", m_controls.throttle, m_controls.steering, m_controls.brake);
	return;
	*/

	// Get vehicle data.
	const vehicle_operatingparams_t &carState = m_pVehicle->GetOperatingParams();
	const vehicleparams_t &vehicleData = m_pVehicle->GetVehicleParams();

	// Get current speed in miles/hour.
	float flCarSign = carState.speed >= 0.0f ? 1.0f : -1.0f;
	float carSpeed = fabs(INS2MPH(carState.speed));

#if 0
	// Set save data.
	m_nLastSpeed = m_nSpeed;
	m_nSpeed = (int)carSpeed;
	m_nRPM = (int)carState.engineRPM;
	m_nHasBoost = vehicleData.engine.boostDelay;	// if we have any boost delay, vehicle has boost ability
#endif

	// If changing direction, use default "return to zero" speed to more quickly transition.
	if ( ( nButtons & IN_MOVELEFT ) || ( nButtons & IN_MOVERIGHT ) )
	{
		SteeringTurn( carSpeed, vehicleData, ( ( nButtons & IN_MOVELEFT ) != 0 ), flFrameTime );
	}
	else if ( cmd->sidemove != 0.0f )
	{
		SteeringTurnAnalog( carSpeed, vehicleData, cmd->sidemove, flFrameTime );
	}
	else
	{
		SteeringRest( carSpeed, vehicleData, flFrameTime );
	}

	// Set vehicle control inputs.
	m_controls.boost = 0;
	m_controls.handbrake = false;
	m_controls.handbrakeLeft = false;
	m_controls.handbrakeRight = false;
	m_controls.brakepedal = false;	
	bool bThrottle;

	if ( nButtons & IN_FORWARD )
	{
		bThrottle = true;
		if ( m_controls.throttle < 0 )
		{
			m_controls.throttle = 0;
		}

		float flMaxThrottle = MAX( 0.1, m_maxThrottle - ( m_maxThrottle * m_flThrottleReduction ) );
		m_controls.throttle = Approach( flMaxThrottle, m_controls.throttle, flFrameTime * m_throttleRate );

		// Apply the brake.
		if ( ( flCarSign < 0.0f ) && m_controls.bHasBrakePedal )
		{
			m_controls.brake = Approach( BRAKE_MAX_VALUE, m_controls.brake, flFrameTime * r_asw_vehicleBrakeRate.GetFloat() * BRAKE_BACK_FORWARD_SCALAR );
			m_controls.brakepedal = true;	
			m_controls.throttle = 0.0f;
			bThrottle = false;
		}
		else
		{
			m_controls.brake = 0.0f;
		}
	}
	else if ( nButtons & IN_BACK )
	{
		bThrottle = true;
		if ( m_controls.throttle > 0 )
		{
			m_controls.throttle = 0;
		}

		float flMaxThrottle = MIN( -0.1, m_flMaxRevThrottle - ( m_flMaxRevThrottle * m_flThrottleReduction ) );
		m_controls.throttle = Approach( flMaxThrottle, m_controls.throttle, flFrameTime * m_throttleRate );

		// Apply the brake.
		if ( ( flCarSign > 0.0f ) && m_controls.bHasBrakePedal )
		{
			m_controls.brake = Approach( BRAKE_MAX_VALUE, m_controls.brake, flFrameTime * r_asw_vehicleBrakeRate.GetFloat() );
			m_controls.brakepedal = true;
			m_controls.throttle = 0.0f;
			bThrottle = false;
		}
		else
		{
			m_controls.brake = 0.0f;
		}
	}
	else
	{
		bThrottle = false;
		m_controls.throttle = 0;
		m_controls.brake = 0.0f;
	}

	if ( ( nButtons & IN_SPEED ) && !IsEngineDisabled() )
	{
		m_controls.boost = 1.0f;
	}

	// Using has brakepedal for handbrake as well.
	if ( ( nButtons & IN_JUMP ) && m_controls.bHasBrakePedal )
	{
		m_controls.handbrake = true;	

		if ( nButtons & IN_MOVELEFT )
		{
			m_controls.handbrakeLeft = true;
		}
		else if ( nButtons & IN_MOVERIGHT )
		{
			m_controls.handbrakeRight = true;
		}

		// Prevent playing of the engine revup when we're braking
		bThrottle = false;
	}

	if ( IsEngineDisabled() )
	{
		m_controls.throttle = 0.0f;
		m_controls.handbrake = true;
		bThrottle = false;
	}

	// throttle sounds
	// If we dropped a bunch of speed, restart the throttle
	if ( bThrottle && (m_nLastSpeed > m_nSpeed && (m_nLastSpeed - m_nSpeed > 10)) )
	{
		m_bLastThrottle = false;
	}

	// throttle down now but not before??? (or we're braking)
	if ( !m_controls.handbrake && !m_controls.brakepedal && bThrottle && !m_bLastThrottle )
	{
		m_throttleStartTime = gpGlobals->curtime;		// need to track how long throttle is down
		m_bLastThrottle = true;
	}
	// throttle up now but not before??
	else if ( !bThrottle && m_bLastThrottle && IsEngineDisabled() == false )
	{
		m_throttleActiveTime = gpGlobals->curtime - m_throttleStartTime;
		m_bLastThrottle = false;
	}

	// asw comment
	/*
	float flSpeedPercentage = clamp( m_nSpeed / m_flMaxSpeed, 0, 1 );
	vbs_sound_update_t params;
	params.Defaults();
	params.bReverse = (m_controls.throttle < 0);
	params.bThrottleDown = bThrottle;
	params.bTurbo = IsBoosting();
	params.bVehicleInWater = m_pOuterServerVehicle->IsVehicleBodyInWater();
	params.flCurrentSpeedFraction = flSpeedPercentage;
	params.flFrameTime = flFrameTime;
	params.flWorldSpaceSpeed = carState.speed;
	m_pOuterServerVehicle->SoundUpdate( params );
	*/
	//Msg(" throttle=%f steering=%f brake=%f\n", m_controls.throttle, m_controls.steering, m_controls.brake);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::AddThrottleReduction( float flPercentage )
{
	// We allow the speed reduction to go over 1, but extras have no effect.
	m_flThrottleReduction = m_flThrottleReduction + flPercentage;

	//Msg("Added speed reduction. Now %.2f\n", m_flThrottleReduction );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::RemoveThrottleReduction( float flPercentage )
{
	m_flThrottleReduction = MAX( 0, m_flThrottleReduction - flPercentage );

	//Msg("Removed speed reduction. Now %.2f\n", m_flThrottleReduction );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool C_ASW_FourWheelVehiclePhysics::IsBoosting( void )
{
	const vehicleparams_t *pVehicleParams = &m_pVehicle->GetVehicleParams();
	const vehicle_operatingparams_t *pVehicleOperating = &m_pVehicle->GetOperatingParams();
	if ( pVehicleParams && pVehicleOperating )
	{	
		if ( ( pVehicleOperating->boostDelay - pVehicleParams->engine.boostDelay ) > 0.0f )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_FourWheelVehiclePhysics::SetDisableEngine( bool bDisable )
{
	// Set the engine state.
	m_pVehicle->SetEngineDisabled( bDisable );
}

static int AddPhysToList( IPhysicsObject **pList, int listMax, int count, IPhysicsObject *pPhys )
{
	if ( pPhys )
	{
		if ( count < listMax )
		{
			pList[count] = pPhys;
			count++;
		}
	}
	return count;
}

int C_ASW_FourWheelVehiclePhysics::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	int count = 0;
	// add the body
	count = AddPhysToList( pList, listMax, count, m_pOuter->VPhysicsGetObject() );
	for ( int i = 0; i < 4; i++ )
	{
		count = AddPhysToList( pList, listMax, count, m_pWheels[i] );
	}
	return count;
}
