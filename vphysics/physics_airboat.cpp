//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The airboat, a sporty nimble water craft.
//
//=============================================================================//

#include "cbase.h"
#include "physics_airboat.h"
#include "cmodel.h"
#include <ivp_ray_solver.hxx>


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _X360 
	#define AIRBOAT_STEERING_RATE_MIN			0.000225f
	#define AIRBOAT_STEERING_RATE_MAX			(10.0f * AIRBOAT_STEERING_RATE_MIN)
	#define AIRBOAT_STEERING_INTERVAL			1.5f
#else
	#define AIRBOAT_STEERING_RATE_MIN			0.00045f
	#define AIRBOAT_STEERING_RATE_MAX			(5.0f * AIRBOAT_STEERING_RATE_MIN)
	#define AIRBOAT_STEERING_INTERVAL			0.5f
#endif //_X360

#define AIRBOAT_ROT_DRAG					0.00004f
#define AIRBOAT_ROT_DAMPING					0.001f

// Mass-independent thrust values
#define AIRBOAT_THRUST_MAX					11.0f		// N / kg
#define AIRBOAT_THRUST_MAX_REVERSE			7.5f		// N / kg

// Mass-independent drag values
#define AIRBOAT_WATER_DRAG_LEFT_RIGHT		0.6f
#define AIRBOAT_WATER_DRAG_FORWARD_BACK		0.005f
#define AIRBOAT_WATER_DRAG_UP_DOWN			0.0025f

#define AIRBOAT_GROUND_DRAG_LEFT_RIGHT		2.0
#define AIRBOAT_GROUND_DRAG_FORWARD_BACK	1.0
#define AIRBOAT_GROUND_DRAG_UP_DOWN			0.8

#define AIRBOAT_DRY_FRICTION_SCALE		0.6f		// unitless, reduces our friction on all surfaces other than water

#define AIRBOAT_RAYCAST_DIST			0.35f		// m (~14in)
#define AIRBOAT_RAYCAST_DIST_WATER_LOW	0.1f		// m (~4in)
#define AIRBOAT_RAYCAST_DIST_WATER_HIGH	0.35f		// m (~16in)

// Amplitude of wave noise. Blend from max to min as speed increases.
#define AIRBOAT_WATER_NOISE_MIN			0.01		// m (~0.4in)
#define AIRBOAT_WATER_NOISE_MAX			0.03		// m (~1.2in)

// Frequency of wave noise. Blend from min to max as speed increases.
#define AIRBOAT_WATER_FREQ_MIN			1.5
#define AIRBOAT_WATER_FREQ_MAX			1.5

// Phase difference in wave noise between left and right pontoons
// Blend from max to min as speed increases.
#define AIRBOAT_WATER_PHASE_MIN			0.0			// s
#define AIRBOAT_WATER_PHASE_MAX			1.5			// s


#define AIRBOAT_GRAVITY					9.81f		// m/s2

// Pontoon indices
enum
{
	AIRBOAT_PONTOON_FRONT_LEFT = 0,
	AIRBOAT_PONTOON_FRONT_RIGHT,
	AIRBOAT_PONTOON_REAR_LEFT,
	AIRBOAT_PONTOON_REAR_RIGHT,
};


class IVP_Ray_Solver_Template;
class IVP_Ray_Hit;
class IVP_Event_Sim;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
class CAirboatFrictionData : public IPhysicsCollisionData
{
public:
	CAirboatFrictionData()
	{
		m_vecPoint.Init( 0, 0, 0 );
		m_vecNormal.Init( 0, 0, 0 );
		m_vecVelocity.Init( 0, 0, 0 );
	}

	virtual void GetSurfaceNormal( Vector &out )
	{
		out = m_vecPoint;
	}

	virtual void GetContactPoint( Vector &out )
	{
		out = m_vecNormal;
	}

	virtual void GetContactSpeed( Vector &out )
	{
		out = m_vecVelocity;
	}

public:
	Vector m_vecPoint;
	Vector m_vecNormal;
	Vector m_vecVelocity;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPhysics_Airboat::CPhysics_Airboat( IVP_Environment *pEnv, const IVP_Template_Car_System *pCarSystem,
								    IPhysicsGameTrace *pGameTrace )
{
	InitRaycastCarBody( pCarSystem );
	InitRaycastCarEnvironment( pEnv, pCarSystem );
	InitRaycastCarWheels( pCarSystem );
	InitRaycastCarAxes( pCarSystem );	

	InitAirboat( pCarSystem );
	m_pGameTrace = pGameTrace;

    m_SteeringAngle = 0;
	m_bSteeringReversed = false;

	m_flThrust = 0;

	m_bAirborne = false;
	m_flAirTime = 0;
	m_bWeakJump = false;

	m_flPitchErrorPrev = 0;
	m_flRollErrorPrev = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CPhysics_Airboat::~CPhysics_Airboat()
{
    m_pAirboatBody->get_environment()->get_controller_manager()->remove_controller_from_environment( this, IVP_TRUE );
}


//-----------------------------------------------------------------------------
// Purpose: Setup the car system wheels.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::InitAirboat( const IVP_Template_Car_System *pCarSystem )
{
	for ( int iWheel = 0; iWheel < pCarSystem->n_wheels; ++iWheel )
	{
		m_pWheels[iWheel] = pCarSystem->car_wheel[iWheel];
		m_pWheels[iWheel]->enable_collision_detection( IVP_FALSE );
	}

	CPhysicsObject* pBodyObject = static_cast<CPhysicsObject*>(pCarSystem->car_body->client_data);

	pBodyObject->EnableGravity( false );

	// We do our own buoyancy simulation.
	pBodyObject->SetCallbackFlags( pBodyObject->GetCallbackFlags() & ~CALLBACK_DO_FLUID_SIMULATION );
}


//-----------------------------------------------------------------------------
// Purpose: Get the raycast wheel.
//-----------------------------------------------------------------------------
IPhysicsObject *CPhysics_Airboat::GetWheel( int index )
{
	Assert( index >= 0 );
	Assert( index < n_wheels );

	return ( IPhysicsObject* )m_pWheels[index]->client_data;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::SetWheelFriction( int iWheel, float flFriction )
{
	change_friction_of_wheel( IVP_POS_WHEEL( iWheel ), flFriction );
}


//-----------------------------------------------------------------------------
// Purpose: Returns an amount to add to the front pontoon raycasts to simulate wave noise.
// Input  : nPontoonIndex - Which pontoon we're dealing with (0 or 1).
//			flSpeedRatio - Speed as a ratio of max speed [0..1]
//-----------------------------------------------------------------------------
float CPhysics_Airboat::ComputeFrontPontoonWaveNoise( int nPontoonIndex, float flSpeedRatio )
{
	// Add in sinusoidal noise cause by undulating water. Reduce the amplitude of the noise at higher speeds.
	IVP_FLOAT flNoiseScale = RemapValClamped( 1.0 - flSpeedRatio, 0, 1, AIRBOAT_WATER_NOISE_MIN, AIRBOAT_WATER_NOISE_MAX );

	// Apply a phase shift between left and right pontoons to simulate waves passing under the boat.
	IVP_FLOAT flPhaseShift = 0;
	if ( flSpeedRatio < 0.3 )
	{
		// BUG: this allows a discontinuity in the waveform - use two superimposed sine waves instead?
		flPhaseShift = nPontoonIndex * AIRBOAT_WATER_PHASE_MAX;
	}

	// Increase the wave frequency as speed increases.
	IVP_FLOAT flFrequency = RemapValClamped( flSpeedRatio, 0, 1, AIRBOAT_WATER_FREQ_MIN, AIRBOAT_WATER_FREQ_MAX );

	//Msg( "Wave amp=%f, freq=%f, phase=%f\n", flNoiseScale, flFrequency, flPhaseShift );
	return flNoiseScale * sin( flFrequency * ( m_pCore->environment->get_current_time().get_seconds() + flPhaseShift ) );
}


//-----------------------------------------------------------------------------
// Purpose:: Convert data to HL2 measurements, and test direction of raycast.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::pre_raycasts_gameside( int nRaycastCount, IVP_Ray_Solver_Template *pRays,
											  Ray_t *pGameRays, IVP_Raycast_Airboat_Impact *pImpacts )
{
	IVP_FLOAT flForwardSpeedRatio = clamp( m_vecLocalVelocity.k[2] / 10.0f, 0.f, 1.0f );
	//Msg( "flForwardSpeedRatio = %f\n", flForwardSpeedRatio );

	IVP_FLOAT flSpeed = ( IVP_FLOAT )m_pCore->speed.real_length();
	IVP_FLOAT flSpeedRatio = clamp( flSpeed / 15.0f, 0.f, 1.0f );
	if ( !m_flThrust )
	{
		flForwardSpeedRatio *= 0.5;
	}

	// This is a little weird. We adjust the front pontoon ray lengths based on forward velocity,
	// but ONLY if both pontoons are in the water, which we won't know until we do the raycast.
	// So we do most of the work here, and cache some of the results to use them later.
	Vector vecStart[4];
	Vector vecDirection[4];
	Vector vecZero( 0.0f, 0.0f, 0.0f );

	int nFrontPontoonsInWater = 0;

	int iRaycast;
	for ( iRaycast = 0; iRaycast < nRaycastCount; ++iRaycast )
	{
		// Setup the ray.
		ConvertPositionToHL( pRays[iRaycast].ray_start_point, vecStart[iRaycast] );
		ConvertDirectionToHL( pRays[iRaycast].ray_normized_direction, vecDirection[iRaycast] );

		float flRayLength = IVP2HL( pRays[iRaycast].ray_length );

		// Check to see if that point is in water.
		pImpacts[iRaycast].bInWater = IVP_FALSE;
		if ( m_pGameTrace->VehiclePointInWater( vecStart[iRaycast] ) )
		{
			vecDirection[iRaycast].Negate();
			pImpacts[iRaycast].bInWater = IVP_TRUE;
		}

		Vector vecEnd = vecStart[iRaycast] + ( vecDirection[iRaycast] * flRayLength );

		// Adjust the trace if the pontoon is in the water.
		if ( m_pGameTrace->VehiclePointInWater( vecEnd ) )
		{
			// Reduce the ray length in the water.
			pRays[iRaycast].ray_length = AIRBOAT_RAYCAST_DIST_WATER_LOW;

			if ( iRaycast < 2 )
			{
				nFrontPontoonsInWater++;

				// Front pontoons.
				// Add a little sinusoidal noise to simulate waves.
				IVP_FLOAT flNoise = ComputeFrontPontoonWaveNoise( iRaycast, flSpeedRatio );
				pRays[iRaycast].ray_length += flNoise;
			}
			else
			{
				// Recalculate the end position in HL coordinates.
				flRayLength = IVP2HL( pRays[iRaycast].ray_length );
				vecEnd = vecStart[iRaycast] + ( vecDirection[iRaycast] * flRayLength );
			}
		}

		pGameRays[iRaycast].Init( vecStart[iRaycast], vecEnd, vecZero, vecZero );
	}

	// If both front pontoons are in the water, add in a bit of lift proportional to our
	// forward speed. We can't do this to only one of the front pontoons because it causes
	// some twist if we do.
	// FIXME: this does some redundant work (computes the wave noise again)
	if ( nFrontPontoonsInWater == 2 )
	{
		for ( int i = 0; i < 2; i++ )
		{
			// Front pontoons.
			// Raise it higher out of the water as we go faster forward.
			pRays[i].ray_length = RemapValClamped( flForwardSpeedRatio, 0, 1, AIRBOAT_RAYCAST_DIST_WATER_LOW, AIRBOAT_RAYCAST_DIST_WATER_HIGH );

			// Add a little sinusoidal noise to simulate waves.
			IVP_FLOAT flNoise = ComputeFrontPontoonWaveNoise( i, flSpeedRatio );
			pRays[i].ray_length += flNoise;

			// Recalculate the end position in HL coordinates.
			float flRayLength = IVP2HL( pRays[i].ray_length );
			Vector vecEnd = vecStart[i] + ( vecDirection[i] * flRayLength );

			pGameRays[i].Init( vecStart[i], vecEnd, vecZero, vecZero );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CPhysics_Airboat::GetWaterDepth( Ray_t *pGameRay, IPhysicsObject *pPhysAirboat )
{
	float flDepth = 0.0f;

	trace_t trace;

	Ray_t waterRay;
	Vector vecStart = pGameRay->m_Start;
	Vector vecEnd( vecStart.x, vecStart.y, vecStart.z + 1000.0f );
	Vector vecZero( 0.0f, 0.0f, 0.0f );
	waterRay.Init( vecStart, vecEnd, vecZero, vecZero );
	m_pGameTrace->VehicleTraceRayWithWater( waterRay, pPhysAirboat->GetGameData(), &trace ); 

	flDepth = 1000.0f * trace.fractionleftsolid;

	return flDepth;
}


//-----------------------------------------------------------------------------
// Purpose: Performs traces to figure out what is at each of the raycast points
//			and fills out the pImpacts array with that information.
// Input  : nRaycastCount - Number of elements in the arrays pointed to by pRays
//				and pImpacts. 
//			pRays - Holds the rays to trace with.
//			pImpacts - Receives the trace results.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::do_raycasts_gameside( int nRaycastCount, IVP_Ray_Solver_Template *pRays,
										     IVP_Raycast_Airboat_Impact *pImpacts )
{
	Assert( nRaycastCount >= 0 );
	Assert( nRaycastCount <= IVP_RAYCAST_AIRBOAT_MAX_WHEELS );

	Ray_t gameRays[IVP_RAYCAST_AIRBOAT_MAX_WHEELS];
	pre_raycasts_gameside( nRaycastCount, pRays, gameRays, pImpacts );

	// Do the raycasts and set impact data.
	trace_t trace;
	for ( int iRaycast = 0; iRaycast < nRaycastCount; ++iRaycast )
	{
		// Trace.
		if ( pImpacts[iRaycast].bInWater )
		{
			// The start position is underwater. Trace up to find the water surface.
			IPhysicsObject *pPhysAirboat = static_cast<IPhysicsObject*>( m_pAirboatBody->client_data );
			m_pGameTrace->VehicleTraceRay( gameRays[iRaycast], pPhysAirboat->GetGameData(), &trace ); 
			pImpacts[iRaycast].flDepth = GetWaterDepth( &gameRays[iRaycast], pPhysAirboat );
		}
		else
		{
			// Trace down to find the ground or water.
			IPhysicsObject *pPhysAirboat = static_cast<IPhysicsObject*>( m_pAirboatBody->client_data );
			m_pGameTrace->VehicleTraceRayWithWater( gameRays[iRaycast], pPhysAirboat->GetGameData(), &trace ); 
		}
		
		ConvertPositionToIVP( gameRays[iRaycast].m_Start + gameRays[iRaycast].m_StartOffset, m_CarSystemDebugData.wheelRaycasts[iRaycast][0] );
		ConvertPositionToIVP( gameRays[iRaycast].m_Start + gameRays[iRaycast].m_StartOffset + gameRays[iRaycast].m_Delta, m_CarSystemDebugData.wheelRaycasts[iRaycast][1] );
		m_CarSystemDebugData.wheelRaycastImpacts[iRaycast] = trace.fraction * gameRays[iRaycast].m_Delta.Length();

		// Set impact data.
		pImpacts[iRaycast].bImpactWater = IVP_FALSE;
		pImpacts[iRaycast].bImpact = IVP_FALSE;
		if ( trace.fraction != 1.0f )
		{
			pImpacts[iRaycast].bImpact = IVP_TRUE;

			// Set water surface flag.
			pImpacts[iRaycast].flDepth = 0.0f;
			if ( trace.contents & MASK_WATER )
			{
				pImpacts[iRaycast].bImpactWater = IVP_TRUE;
			}

			// Save impact surface data.
			ConvertPositionToIVP( trace.endpos, pImpacts[iRaycast].vecImpactPointWS );
			ConvertDirectionToIVP( trace.plane.normal, pImpacts[iRaycast].vecImpactNormalWS );

			// Save surface properties.
			const surfacedata_t *pSurfaceData = physprops->GetSurfaceData( trace.surface.surfaceProps );

			pImpacts[iRaycast].nSurfaceProps = trace.surface.surfaceProps;

			if (pImpacts[iRaycast].vecImpactNormalWS.k[1] < -0.707)
			{
				// dampening is 1/t, where t is how long it takes to come to a complete stop
				pImpacts[iRaycast].flDampening = pSurfaceData->physics.dampening;
				pImpacts[iRaycast].flFriction = pSurfaceData->physics.friction;
			}
			else
			{
				// This surface is too vertical -- no friction or damping from it.
				pImpacts[iRaycast].flDampening = pSurfaceData->physics.dampening;
				pImpacts[iRaycast].flFriction = pSurfaceData->physics.friction;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Entry point for airboat simulation.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::do_simulation_controller( IVP_Event_Sim *pEventSim, IVP_U_Vector<IVP_Core> * )
{
    IVP_Ray_Solver_Template				raySolverTemplates[IVP_RAYCAST_AIRBOAT_MAX_WHEELS];
	IVP_Raycast_Airboat_Impact			impacts[IVP_RAYCAST_AIRBOAT_MAX_WHEELS];

	// Cache some data into members here so we only do the work once.
	m_pCore = m_pAirboatBody->get_core();
    const IVP_U_Matrix *matWorldFromCore = m_pCore->get_m_world_f_core_PSI();

	// Cache the speed.
	m_flSpeed = ( IVP_FLOAT )m_pCore->speed.real_length();

	// Cache the local velocity vector.
	matWorldFromCore->vimult3(&m_pCore->speed, &m_vecLocalVelocity);
    
	// Raycasts.
	PreRaycasts( raySolverTemplates, matWorldFromCore, impacts );
	do_raycasts_gameside( n_wheels, raySolverTemplates, impacts );
	if ( !PostRaycasts( raySolverTemplates, matWorldFromCore, impacts ) )
		return;

	UpdateAirborneState( impacts, pEventSim );

	// Enumerate the controllers attached to us.
	//for (int i = m_pCore->controllers_of_core.len() - 1; i >= 0; i--)
	//{
	//	IVP_Controller *pController = m_pCore->controllers_of_core.element_at(i);
	//}

    // Pontoons. Buoyancy or ground impacts.
	DoSimulationPontoons( impacts, pEventSim );

	// Drag due to water and ground friction.
	DoSimulationDrag( impacts, pEventSim );

	// Turbine (fan).
	DoSimulationTurbine( pEventSim );

	// Steering.
	DoSimulationSteering( pEventSim );	

	// Anti-pitch.
	DoSimulationKeepUprightPitch( impacts, pEventSim );

	// Anti-roll.
	DoSimulationKeepUprightRoll( impacts, pEventSim );

	// Additional gravity based on speed.
	DoSimulationGravity( pEventSim );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the rays to be cast from the vehicle wheel positions to
//          the "ground."
// Input  : pRaySolverTemplates - 
//			matWorldFromCore - 
//			pImpacts - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::PreRaycasts( IVP_Ray_Solver_Template *pRaySolverTemplates, 
									  const IVP_U_Matrix *matWorldFromCore,
									  IVP_Raycast_Airboat_Impact *pImpacts )
{
	int nPontoonPoints = n_wheels;
	for ( int iPoint = 0; iPoint < nPontoonPoints; ++iPoint )
	{
		IVP_Raycast_Airboat_Wheel *pPontoonPoint = get_wheel( IVP_POS_WHEEL( iPoint ) );
		if ( pPontoonPoint )
		{
			// Fill the in the ray solver template for the current wheel.
			IVP_Ray_Solver_Template &raySolverTemplate = pRaySolverTemplates[iPoint];

			// Transform the wheel "start" position from vehicle core-space to world-space.  This is
			// the raycast starting position.
			matWorldFromCore->vmult4( &pPontoonPoint->raycast_start_cs, &raySolverTemplate.ray_start_point );

			// Transform the shock (spring) direction from vehicle core-space to world-space.  This is
			// the raycast direction.
			matWorldFromCore->vmult3( &pPontoonPoint->raycast_dir_cs, &pImpacts[iPoint].raycast_dir_ws );
			raySolverTemplate.ray_normized_direction.set( &pImpacts[iPoint].raycast_dir_ws );

			// Set the length of the ray cast.
			raySolverTemplate.ray_length = AIRBOAT_RAYCAST_DIST;	

			// Set the ray solver template flags.  This defines which objects you wish to
			// collide against in the physics environment.
			raySolverTemplate.ray_flags = IVP_RAY_SOLVER_ALL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether we are airborne and whether we just performed a
//			weak or strong jump. Weak jumps are jumps at below a threshold speed,
//			and disable the turbine and pitch controller.
// Input  : pImpacts - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::UpdateAirborneState( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim )
{
	int nCount = CountSurfaceContactPoints(pImpacts);
	if (!nCount)
	{
		if (!m_bAirborne)
		{
			m_bAirborne = true;
			m_flAirTime = 0;

			IVP_FLOAT flSpeed = ( IVP_FLOAT )m_pCore->speed.real_length();
			if (flSpeed < 11.0f)
			{
				//Msg("*** WEAK JUMP at %f!!!\n", flSpeed);
				m_bWeakJump = true;
			}
			else
			{
				//Msg("Strong JUMP at %f\n", flSpeed);
			}
		}
		else
		{
			m_flAirTime += pEventSim->delta_time;
		}
	}
	else
	{
		m_bAirborne = false;
		m_bWeakJump = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPhysics_Airboat::PostRaycasts( IVP_Ray_Solver_Template *pRaySolverTemplates, const IVP_U_Matrix *matWorldFromCore,
										IVP_Raycast_Airboat_Impact *pImpacts )
{
	bool bReturn = true;

	int nPontoonPoints = n_wheels;
	for( int iPoint = 0; iPoint < nPontoonPoints; ++iPoint )
	{
		// Get data at raycast position.
		IVP_Raycast_Airboat_Wheel *pPontoonPoint = get_wheel( IVP_POS_WHEEL( iPoint ) );
		IVP_Raycast_Airboat_Impact *pImpact = &pImpacts[iPoint];
		IVP_Ray_Solver_Template *pRaySolver = &pRaySolverTemplates[iPoint];
		if ( !pPontoonPoint || !pImpact || !pRaySolver )
			continue;

		// Copy the ray length back, it may have changed.
		pPontoonPoint->raycast_length = pRaySolver->ray_length;

		// Test for inverted raycast direction.
		if ( pImpact->bInWater )
		{
			pImpact->raycast_dir_ws.set_multiple( &pImpact->raycast_dir_ws, -1 );
		}

		// Impact.
		if ( pImpact->bImpact )
		{
			// Save impact distance.
			IVP_U_Point vecDelta;
			vecDelta.subtract( &pImpact->vecImpactPointWS, &pRaySolver->ray_start_point );
			pPontoonPoint->raycast_dist = vecDelta.real_length();

			// Get the inverse portion of the surface normal in the direction of the ray cast (shock - used in the shock simulation code for the sign
			// and percentage of force applied to the shock).
			pImpact->inv_normal_dot_dir = 1.1f / ( IVP_Inline_Math::fabsd( pImpact->raycast_dir_ws.dot_product( &pImpact->vecImpactNormalWS ) ) + 0.1f );

			// Set the wheel friction - ground friction (if any) + wheel friction.
			pImpact->friction_value = pImpact->flFriction * pPontoonPoint->friction_of_wheel;
		}
		// No impact.
		else
		{
			pPontoonPoint->raycast_dist = pPontoonPoint->raycast_length;

			pImpact->inv_normal_dot_dir = 1.0f;	    
			pImpact->moveable_object_hit_by_ray = NULL;
			pImpact->vecImpactNormalWS.set_multiple( &pImpact->raycast_dir_ws, -1 );
			pImpact->friction_value = 1.0f;
		}

		// Set the new wheel position (the impact point or the full ray distance).  Make this from the wheel not the ray trace position.
		pImpact->vecImpactPointWS.add_multiple( &pRaySolver->ray_start_point, &pImpact->raycast_dir_ws, pPontoonPoint->raycast_dist );

		// Get the speed (velocity) at the impact point.
		m_pCore->get_surface_speed_ws( &pImpact->vecImpactPointWS, &pImpact->surface_speed_wheel_ws );
		pImpact->projected_surface_speed_wheel_ws.set_orthogonal_part( &pImpact->surface_speed_wheel_ws, &pImpact->vecImpactNormalWS );

		matWorldFromCore->vmult3( &pPontoonPoint->axis_direction_cs, &pImpact->axis_direction_ws );
		pImpact->projected_axis_direction_ws.set_orthogonal_part( &pImpact->axis_direction_ws, &pImpact->vecImpactNormalWS );
		if ( pImpact->projected_axis_direction_ws.normize() == IVP_FAULT )
		{
			DevMsg( "CPhysics_Airboat::do_simulation_controller projected_axis_direction_ws.normize failed\n" );
			bReturn = false;
		}
	}

	return bReturn;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationPontoons( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim )
{
	int nPontoonPoints = n_wheels;
	for ( int iPoint = 0; iPoint < nPontoonPoints; ++iPoint )
	{
		IVP_Raycast_Airboat_Wheel *pPontoonPoint = get_wheel( IVP_POS_WHEEL( iPoint ) );
		if ( !pPontoonPoint )
			continue;

		if ( pImpacts[iPoint].bImpact )
		{
			DoSimulationPontoonsGround( pPontoonPoint, &pImpacts[iPoint], pEventSim );
		}
		else if ( pImpacts[iPoint].bInWater )
		{
			DoSimulationPontoonsWater( pPontoonPoint, &pImpacts[iPoint], pEventSim );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle pontoons on ground.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationPontoonsGround( IVP_Raycast_Airboat_Wheel *pPontoonPoint, 
										             IVP_Raycast_Airboat_Impact *pImpact, IVP_Event_Sim *pEventSim )
{
	// Check to see if we hit anything, otherwise the no force on this point.
	IVP_DOUBLE flDiff = pPontoonPoint->raycast_dist - pPontoonPoint->raycast_length;
	if ( flDiff >= 0 ) 
		return;
	
	IVP_FLOAT flSpringConstant, flSpringRelax, flSpringCompress;
	flSpringConstant = pPontoonPoint->spring_constant;
	flSpringRelax = pPontoonPoint->spring_damp_relax;
	flSpringCompress = pPontoonPoint->spring_damp_compress;
	
	IVP_DOUBLE flForce = -flDiff * flSpringConstant;
	IVP_FLOAT flInvNormalDotDir = clamp(pImpact->inv_normal_dot_dir, 0.0f, 3.0f);
	flForce *= flInvNormalDotDir;
	
	IVP_U_Float_Point vecSpeedDelta; 
	vecSpeedDelta.subtract( &pImpact->projected_surface_speed_wheel_ws, &pImpact->surface_speed_wheel_ws );
	
	IVP_DOUBLE flSpeed = vecSpeedDelta.dot_product( &pImpact->raycast_dir_ws );
	if ( flSpeed > 0 )
	{
		flForce -= flSpringRelax * flSpeed;
	}
	else
	{
		flForce -= flSpringCompress * flSpeed;
	}
	
	if ( flForce < 0 )
	{
		flForce = 0.0f;
	}

	// NOTE: Spring constants are all mass-independent, so no need to multiply by mass here.	
	IVP_DOUBLE flImpulse = flForce * pEventSim->delta_time;
	
	IVP_U_Float_Point vecImpulseWS; 
	vecImpulseWS.set_multiple( &pImpact->vecImpactNormalWS, flImpulse );
	m_pCore->push_core_ws( &pImpact->vecImpactPointWS, &vecImpulseWS );
}


//-----------------------------------------------------------------------------
// Purpose: Handle pontoons on water.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationPontoonsWater( IVP_Raycast_Airboat_Wheel *pPontoonPoint, 
										            IVP_Raycast_Airboat_Impact *pImpact, IVP_Event_Sim *pEventSim )
{
	#define AIRBOAT_BUOYANCY_SCALAR	1.6f
	#define PONTOON_AREA_2D 2.8f			// 2 pontoons x 16 in x 136 in = 4352 sq inches = 2.8 sq meters
	#define PONTOON_HEIGHT 0.41f			// 16 inches high = 0.41 meters

	float flDepth = clamp( pImpact->flDepth, 0.f, PONTOON_HEIGHT );
	//Msg("depth: %f\n", pImpact->flDepth);

	// Depth is in inches, so multiply by 0.0254 meters/inch
	IVP_FLOAT flSubmergedVolume = PONTOON_AREA_2D * flDepth * 0.0254;

	// Buoyancy forces are equal to the mass of the water displaced, which is 1000 kg/m^3
	// There are 4 pontoon points, so each one can exert 1/4th of the total buoyancy force.
	IVP_FLOAT flForce = AIRBOAT_BUOYANCY_SCALAR * 0.25f * m_pCore->get_mass() * flSubmergedVolume * 1000.0f;
	IVP_DOUBLE flImpulse = flForce * pEventSim->delta_time;
	
	IVP_U_Float_Point vecImpulseWS; 
	vecImpulseWS.set( 0, -1, 0 );
	vecImpulseWS.mult( flImpulse );
	m_pCore->push_core_ws( &pImpact->vecImpactPointWS, &vecImpulseWS );

//	Vector vecPoint;
//	Vector vecDir(0, 0, 1);
//
//	ConvertPositionToHL( pImpact->vecImpactPointWS, vecPoint );
//	CPhysicsEnvironment *pEnv = (CPhysicsEnvironment *)m_pAirboatBody->get_core()->environment->client_data;
//	IVPhysicsDebugOverlay *debugoverlay = pEnv->GetDebugOverlay();
//	debugoverlay->AddLineOverlay(vecPoint, vecPoint + vecDir * 128, 255, 0, 255, false, 10.0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::PerformFrictionNotification( float flEliminatedEnergy, float dt, int nSurfaceProp, IPhysicsCollisionData *pCollisionData )
{
	CPhysicsObject *pPhysAirboat = static_cast<CPhysicsObject*>( m_pAirboatBody->client_data );
	if ( ( pPhysAirboat->CallbackFlags() & CALLBACK_GLOBAL_FRICTION ) == 0 )
		return;

	IPhysicsCollisionEvent *pEventHandler = pPhysAirboat->GetVPhysicsEnvironment()->GetCollisionEventHandler();
	if ( !pEventHandler )
		return;

	// scrape with an estimate for the energy per unit mass
	// This assumes that the game is interested in some measure of vibration
	// for sound effects.  This also assumes that more massive objects require
	// more energy to vibrate.
	flEliminatedEnergy *= dt / pPhysAirboat->GetMass();
	if ( flEliminatedEnergy > 0.05f )
	{
		pEventHandler->Friction( pPhysAirboat, flEliminatedEnergy, pPhysAirboat->GetMaterialIndexInternal(), nSurfaceProp, pCollisionData );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Drag due to water and ground friction.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationDrag( IVP_Raycast_Airboat_Impact *pImpacts, 
										   IVP_Event_Sim *pEventSim )
{
    const IVP_U_Matrix *matWorldFromCore = m_pCore->get_m_world_f_core_PSI();
	IVP_FLOAT flSpeed = ( IVP_FLOAT )m_pCore->speed.real_length();

	// Used to make airboat sliding sounds
	CAirboatFrictionData frictionData;
	ConvertDirectionToHL( m_pCore->speed, frictionData.m_vecVelocity ); 

	// Count the pontoons in the water.
	int nPontoonPoints = n_wheels;
	int nPointsInWater = 0;
	int nPointsOnGround = 0;
	float flGroundFriction = 0;
	float flAverageDampening = 0.0f;
	int *pSurfacePropCount = (int *)stackalloc( n_wheels * sizeof(int) );
	int *pSurfaceProp  = (int *)stackalloc( n_wheels * sizeof(int) );
	memset( pSurfacePropCount, 0, n_wheels * sizeof(int) );
	memset( pSurfaceProp, 0xFF, n_wheels * sizeof(int) );
	int nSurfacePropCount = 0;
	int nMaxSurfacePropIdx = 0;
	for( int iPoint = 0; iPoint < nPontoonPoints; ++iPoint )
	{
		// Get data at raycast position.
		IVP_Raycast_Airboat_Impact *pImpact = &pImpacts[iPoint];
		if ( !pImpact || !pImpact->bImpact )
			continue;

		if ( pImpact->bImpactWater )
		{
			flAverageDampening += pImpact->flDampening;
			nPointsInWater++;
		}
		else
		{
			flGroundFriction += pImpact->flFriction;
			nPointsOnGround++;

			// This logic is used to determine which surface prop we hit the most.
			int i;
			for ( i = 0; i < nSurfacePropCount; ++i )
			{
				if ( pSurfaceProp[i] == pImpact->nSurfaceProps )
					break;
			}

			if ( i == nSurfacePropCount )
			{
				++nSurfacePropCount;
			}
			pSurfaceProp[i] = pImpact->nSurfaceProps;
			if ( ++pSurfacePropCount[i] > pSurfacePropCount[nMaxSurfacePropIdx] )
			{
				nMaxSurfacePropIdx = i;
			}

			Vector frictionPoint, frictionNormal;
			ConvertPositionToHL( pImpact->vecImpactPointWS, frictionPoint );
			ConvertDirectionToHL( pImpact->vecImpactNormalWS, frictionNormal );
			frictionData.m_vecPoint += frictionPoint;
			frictionData.m_vecNormal += frictionNormal;
		}
	}

	int nSurfaceProp = pSurfaceProp[nMaxSurfacePropIdx];
	if ( nPointsOnGround > 0 )
	{
		frictionData.m_vecPoint /= nPointsOnGround;
		frictionData.m_vecNormal /= nPointsOnGround;
		VectorNormalize( frictionData.m_vecNormal );
	}

	if ( nPointsInWater > 0 )
	{
		flAverageDampening /= nPointsInWater;
	}

	//IVP_FLOAT flDebugSpeed = ( IVP_FLOAT )m_pCore->speed.real_length();
	//Msg("(water=%d/land=%d) speed=%f (%f %f %f)\n", nPointsInWater, nPointsOnGround, flDebugSpeed, vecAirboatDirLS.k[0], vecAirboatDirLS.k[1], vecAirboatDirLS.k[2]);

	if ( nPointsInWater )
	{
		// Apply the drag force opposite to the direction of motion in local space.
		IVP_U_Float_Point vecAirboatNegDirLS;
		vecAirboatNegDirLS.set_negative( &m_vecLocalVelocity );

		// Water drag is directional -- the pontoons resist left/right motion much more than forward/back.
		IVP_U_Float_Point vecDragLS;
		vecDragLS.set( AIRBOAT_WATER_DRAG_LEFT_RIGHT * vecAirboatNegDirLS.k[0],
					   AIRBOAT_WATER_DRAG_UP_DOWN * vecAirboatNegDirLS.k[1],
					   AIRBOAT_WATER_DRAG_FORWARD_BACK * vecAirboatNegDirLS.k[2] );

		vecDragLS.mult( flSpeed * m_pCore->get_mass() * pEventSim->delta_time );
		// dvs TODO: apply flAverageDampening here

		// Convert the drag force to world space and apply the drag.
		IVP_U_Float_Point vecDragWS;
		matWorldFromCore->vmult3(&vecDragLS, &vecDragWS);
		m_pCore->center_push_core_multiple_ws( &vecDragWS );
	}

	//
	// Calculate ground friction drag:
	//
	if ( nPointsOnGround && ( flSpeed > 0 ))
	{
		// Calculate the average friction across all contact points.
		flGroundFriction /= (float)nPointsOnGround;

		// Apply the drag force opposite to the direction of motion.
		IVP_U_Float_Point vecAirboatNegDir;
		vecAirboatNegDir.set_negative( &m_pCore->speed );

		IVP_FLOAT flFrictionDrag = m_pCore->get_mass() * AIRBOAT_GRAVITY * AIRBOAT_DRY_FRICTION_SCALE * flGroundFriction;
		flFrictionDrag /= flSpeed;

		IPhysicsObject *pPhysAirboat = static_cast<IPhysicsObject*>( m_pAirboatBody->client_data );
		float flEliminatedEnergy = pPhysAirboat->GetEnergy();

		// Apply the drag force opposite to the direction of motion in local space.
		IVP_U_Float_Point vecAirboatNegDirLS;
		vecAirboatNegDirLS.set_negative( &m_vecLocalVelocity );

		// Ground drag is directional -- the pontoons resist left/right motion much more than forward/back.
		IVP_U_Float_Point vecDragLS;
		vecDragLS.set( AIRBOAT_GROUND_DRAG_LEFT_RIGHT * vecAirboatNegDirLS.k[0],
					   AIRBOAT_GROUND_DRAG_UP_DOWN * vecAirboatNegDirLS.k[1],
					   AIRBOAT_GROUND_DRAG_FORWARD_BACK * vecAirboatNegDirLS.k[2] );

		vecDragLS.mult( flFrictionDrag * pEventSim->delta_time );
		// dvs TODO: apply flAverageDampening here

		// Convert the drag force to world space and apply the drag.
		IVP_U_Float_Point vecDragWS;
		matWorldFromCore->vmult3(&vecDragLS, &vecDragWS);
		m_pCore->center_push_core_multiple_ws( &vecDragWS );

		// Figure out how much energy was eliminated by friction.
		flEliminatedEnergy -= pPhysAirboat->GetEnergy();
		PerformFrictionNotification( flEliminatedEnergy, pEventSim->delta_time, nSurfaceProp, &frictionData );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationTurbine( IVP_Event_Sim *pEventSim )
{
	// Reduce the turbine power during weak jumps to avoid unrealistic air control.
	// Also, reduce reverse thrust while airborne.
	float flThrust = m_flThrust;
	if ((m_bWeakJump) || (m_bAirborne && (flThrust < 0)))
	{
		flThrust *= 0.5;
	}

	// Get the forward vector in world-space.
	IVP_U_Float_Point vecForwardWS; 
    const IVP_U_Matrix *matWorldFromCore = m_pCore->get_m_world_f_core_PSI();
	matWorldFromCore->get_col( IVP_COORDINATE_INDEX( index_z ), &vecForwardWS );

	//Msg("thrust: %f\n", m_flThrust);
	if ( ( vecForwardWS.k[1] < -0.5 ) && ( flThrust > 0 ) )
	{
		// Driving up a slope. Reduce upward thrust to prevent ludicrous climbing of steep surfaces.
		float flFactor = 1 + vecForwardWS.k[1];
		//Msg("FWD: y=%f, factor=%f\n", vecForwardWS.k[1], flFactor);
		flThrust *= flFactor;
	}
	else if ( ( vecForwardWS.k[1] > 0.5 ) && ( flThrust < 0 ) )
	{
		// Reversing up a slope. Reduce upward thrust to prevent ludicrous climbing of steep surfaces.
		float flFactor = 1 - vecForwardWS.k[1];
		//Msg("REV: y=%f, factor=%f\n", vecForwardWS.k[1], flFactor);
		flThrust *= flFactor;
	}

	// Forward (Front/Back) force
	IVP_U_Float_Point vecImpulse;
	vecImpulse.set_multiple( &vecForwardWS, flThrust * m_pCore->get_mass() * pEventSim->delta_time );

	m_pCore->center_push_core_multiple_ws( &vecImpulse );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationSteering( IVP_Event_Sim *pEventSim )
{
	// Calculate the steering direction: forward or reverse.
	// Don't mess with the steering direction while we're steering, unless thrust is applied.
	// This prevents the steering from reversing because we started drifting backwards.
	if ( ( m_SteeringAngle == 0 ) || ( m_flThrust != 0 ) )
	{
		if ( !m_bAnalogSteering )
		{
			// If we're applying reverse thrust, steering is always reversed.
			if ( m_flThrust < 0 )
			{
				m_bSteeringReversed = true;
			}
			// Else if we are applying forward thrust or moving forward, use forward steering.
			else if ( ( m_flThrust > 0 ) || ( m_vecLocalVelocity.k[2] > 0 ) )
			{
				m_bSteeringReversed = false;
			}
		}
		else
		{
			// Create a dead zone through the middle of the joystick where we don't reverse thrust.
			// If we're applying reverse thrust, steering is always reversed.
			if ( m_flThrust < -2.0f )
			{
				m_bSteeringReversed = true;
			}
			// Else if we are applying forward thrust or moving forward, use forward steering.
			else if ( ( m_flThrust > 2.0f ) || ( m_vecLocalVelocity.k[2] > 0 ) )
			{
				m_bSteeringReversed = false;
			}
		}
	}

	// Calculate the steering force.
	IVP_FLOAT flForceSteering = 0.0f;
	if ( fabsf( m_SteeringAngle ) > 0.01 )
	{
		// Get the sign of the steering force.
		IVP_FLOAT flSteeringSign = m_SteeringAngle < 0.0f ? -1.0f : 1.0f;
		if ( m_bSteeringReversed )
		{
			flSteeringSign *= -1.0f;
		}

		// If we changed steering sign or went from not steering to steering, reset the steer time
		// to blend the new steering force in over time.
		IVP_FLOAT flPrevSteeringSign = m_flPrevSteeringAngle < 0.0f ? -1.0f : 1.0f;
		if ( ( fabs( m_flPrevSteeringAngle ) < 0.01 ) || ( flSteeringSign != flPrevSteeringSign ) )
		{
			m_flSteerTime = 0;
		}

		float flSteerScale = 0.f;
		if ( !m_bAnalogSteering )
		{
			// Ramp the steering force up over two seconds.
			flSteerScale = RemapValClamped( m_flSteerTime, 0, AIRBOAT_STEERING_INTERVAL, AIRBOAT_STEERING_RATE_MIN, AIRBOAT_STEERING_RATE_MAX );
		}
		else	// consoles
		{
			// Analog steering
			flSteerScale = RemapValClamped( fabs(m_SteeringAngle), 0, AIRBOAT_STEERING_INTERVAL, AIRBOAT_STEERING_RATE_MIN, AIRBOAT_STEERING_RATE_MAX );
		}

		flForceSteering = flSteerScale * m_pCore->get_mass() * pEventSim->i_delta_time;
		flForceSteering *= -flSteeringSign;

		m_flSteerTime += pEventSim->delta_time;
	}

	//Msg("steer force=%f\n", flForceSteering);

	m_flPrevSteeringAngle = m_SteeringAngle * ( m_bSteeringReversed ? -1.0 : 1.0 );

	// Get the sign of the drag forces.
	IVP_FLOAT flRotSpeedSign = m_pCore->rot_speed.k[1] < 0.0f ? -1.0f : 1.0f;

	// Apply drag proportional to the square of the angular velocity.
	IVP_FLOAT flRotationalDrag = AIRBOAT_ROT_DRAG * m_pCore->rot_speed.k[1] * m_pCore->rot_speed.k[1] * m_pCore->get_mass() * pEventSim->i_delta_time;
	flRotationalDrag *= flRotSpeedSign;

	// Apply dampening proportional to angular velocity.
	IVP_FLOAT flRotationalDamping = AIRBOAT_ROT_DAMPING * fabs(m_pCore->rot_speed.k[1]) * m_pCore->get_mass() * pEventSim->i_delta_time;
	flRotationalDamping *= flRotSpeedSign;

	// Calculate the net rotational force.
	IVP_FLOAT flForceRotational = flForceSteering + flRotationalDrag + flRotationalDamping;

	// Apply it.
	IVP_U_Float_Point vecRotImpulse;
	vecRotImpulse.set( 0, -1, 0 );
	vecRotImpulse.mult( flForceRotational );
	m_pCore->rot_push_core_cs( &vecRotImpulse );
}


//-----------------------------------------------------------------------------
// Purpose: Adds extra gravity unless we are performing a strong jump.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationGravity( IVP_Event_Sim *pEventSim )
{
	return;

	if ( !m_bAirborne || m_bWeakJump )
	{
		IVP_U_Float_Point vecGravity;
		vecGravity.set( 0, AIRBOAT_GRAVITY / 2.0f, 0 );
		vecGravity.mult( m_pCore->get_mass() * pEventSim->delta_time );
		m_pCore->center_push_core_multiple_ws( &vecGravity );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of pontoon raycast points that were found to contact
//			the ground or water.
//-----------------------------------------------------------------------------
int CPhysics_Airboat::CountSurfaceContactPoints( IVP_Raycast_Airboat_Impact *pImpacts )
{
	int nContacts = 0;
	int nPontoonPoints = n_wheels;
	for ( int iPoint = 0; iPoint < nPontoonPoints; iPoint++ )
	{
		// Get data at raycast position.
		IVP_Raycast_Airboat_Impact *pImpact = &pImpacts[iPoint];
		if ( !pImpact )
			continue;

		if ( pImpact->bImpact )
		{
			nContacts++;
		}
	}

	return nContacts;
}


//-----------------------------------------------------------------------------
// Purpose: Prevents us from nosing down dramatically during jumps, which
//			increases our maximum jump distance.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationKeepUprightPitch( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim )
{
	// Disable pitch control during weak jumps. This reduces the unreal 'floaty' sensation.
	if (m_bWeakJump)
	{
		return;
	}

	// Reference vector in core space.
	// Pitch back by 10 degrees while airborne.
	IVP_U_Float_Point vecUpCS; 
	vecUpCS.set( 0, -cos(DEG2RAD(10)), sin(DEG2RAD(10)));

	// Calculate the goal vector in core space. We will try to align the reference
	// vector with the goal vector.
	IVP_U_Float_Point vecGoalAxisWS;
	vecGoalAxisWS.set( 0, -1, 0 );
    const IVP_U_Matrix *matWorldFromCore = m_pCore->get_m_world_f_core_PSI();
	IVP_U_Float_Point vecGoalAxisCS;
	matWorldFromCore->vimult3( &vecGoalAxisWS, &vecGoalAxisCS );

	// Eliminate roll control
	vecGoalAxisCS.k[0] = vecUpCS.k[0];
	vecGoalAxisCS.normize();

	// Get an axis to rotate around.
	IVP_U_Float_Point vecRotAxisCS;
	vecRotAxisCS.calc_cross_product( &vecUpCS, &vecGoalAxisCS );

	// Get the amount that we need to rotate.
	// atan2() is well defined, so do a Dot & Cross instead of asin(Cross)
	IVP_FLOAT cosine = vecUpCS.dot_product( &vecGoalAxisCS );
	IVP_FLOAT sine = vecRotAxisCS.real_length_plus_normize();
	IVP_FLOAT angle = atan2( sine, cosine );

	//Msg("angle: %.2f, axis: (%.2f %.2f %.2f)\n", RAD2DEG(angle), vecRotAxisCS.k[0], vecRotAxisCS.k[1], vecRotAxisCS.k[2]);

	// Don't keep upright if any pontoons are contacting a surface.
	if ( CountSurfaceContactPoints( pImpacts ) > 0 )
	{
		m_flPitchErrorPrev = angle;
		return;
	}
	
	// Don't do any correction if we're within 15 degrees of the goal orientation.
	//if ( fabs( angle ) < DEG2RAD( 15 ) )
	//{
	//	m_flPitchErrorPrev = angle;
	//	return;
	//}

	//Msg("CORRECTING\n");

	// Generate an angular impulse describing the rotation.
	IVP_U_Float_Point vecAngularImpulse;
	vecAngularImpulse.set_multiple( &vecRotAxisCS, m_pCore->get_mass() * ( 0.1f * angle + 0.04f * pEventSim->i_delta_time * ( angle - m_flPitchErrorPrev ) ) );

	// Save the last error value for calculating the derivative.
	m_flPitchErrorPrev = angle;

	// Clamp the impulse at a maximum length.
	IVP_FLOAT len = vecAngularImpulse.real_length_plus_normize();
	if ( len > ( DEG2RAD( 1.5 ) * m_pCore->get_mass() ) )
	{
		len = DEG2RAD( 1.5 ) * m_pCore->get_mass();
	}
	vecAngularImpulse.mult( len );

	// Apply the rotation.
	m_pCore->rot_push_core_cs( &vecAngularImpulse );

#if DRAW_AIRBOAT_KEEP_UPRIGHT_PITCH_VECTORS
	CPhysicsEnvironment *pEnv = (CPhysicsEnvironment *)m_pAirboatBody->get_core()->environment->client_data;
	IVPhysicsDebugOverlay *debugoverlay = pEnv->GetDebugOverlay();

	IVP_U_Float_Point vecPosIVP = m_pCore->get_position_PSI();
	Vector vecPosHL;
	ConvertPositionToHL(vecPosIVP, vecPosHL);

	Vector vecGoalAxisHL;
	ConvertDirectionToHL(vecGoalAxisWS, vecGoalAxisHL);

	IVP_U_Float_Point vecUpWS;
	matWorldFromCore->vmult3( &vecUpCS, &vecUpWS );
	Vector vecCurHL;
	ConvertDirectionToHL(vecUpWS, vecCurHL);

	static IVP_FLOAT flLastLen = 0;
	IVP_FLOAT flDebugLen = vecAngularImpulse.real_length();
	if ( flLastLen && ( fabs( flDebugLen - flLastLen ) > DEG2RAD( 1 )  * m_pCore->get_mass() ) )
	{
		debugoverlay->AddLineOverlay(vecPosHL, vecPosHL + Vector(0, 0, 10) * flDebugLen, 255, 0, 255, false, 100.0 );
	}
	else
	{
		debugoverlay->AddLineOverlay(vecPosHL, vecPosHL + Vector(0, 0, 10) * flDebugLen, 255, 255, 255, false, 100.0 );
	}
	debugoverlay->AddLineOverlay(vecPosHL + Vector(0, 0, 10) * flDebugLen, vecPosHL + Vector(0, 0, 10) * flDebugLen + vecGoalAxisHL * 10, 0, 255, 0, false, 100.0 );
	debugoverlay->AddLineOverlay(vecPosHL + Vector(0, 0, 10) * flDebugLen, vecPosHL + Vector(0, 0, 10) * flDebugLen + vecCurHL * 10, 255, 0, 0, false, 100.0 );
	flLastLen = flDebugLen;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Roll stabilizer when airborne.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::DoSimulationKeepUprightRoll( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim )
{
	// Reference vector in core space.
	// Pitch back by 10 degrees while airborne.
	IVP_U_Float_Point vecUpCS; 
	vecUpCS.set( 0, -cos(DEG2RAD(10)), sin(DEG2RAD(10)));

	// Calculate the goal vector in core space. We will try to align the reference
	// vector with the goal vector.
	IVP_U_Float_Point vecGoalAxisWS;
	vecGoalAxisWS.set( 0, -1, 0 );
    const IVP_U_Matrix *matWorldFromCore = m_pCore->get_m_world_f_core_PSI();
	IVP_U_Float_Point vecGoalAxisCS;
	matWorldFromCore->vimult3( &vecGoalAxisWS, &vecGoalAxisCS );

	// Eliminate pitch control
	vecGoalAxisCS.k[1] = vecUpCS.k[1];
	vecGoalAxisCS.normize();

	// Get an axis to rotate around.
	IVP_U_Float_Point vecRotAxisCS;
	vecRotAxisCS.calc_cross_product( &vecUpCS, &vecGoalAxisCS );

	// Get the amount that we need to rotate.
	// atan2() is well defined, so do a Dot & Cross instead of asin(Cross)
	IVP_FLOAT cosine = vecUpCS.dot_product( &vecGoalAxisCS );
	IVP_FLOAT sine = vecRotAxisCS.real_length_plus_normize();
	IVP_FLOAT angle = atan2( sine, cosine );

	//Msg("angle: %.2f, axis: (%.2f %.2f %.2f)\n", RAD2DEG(angle), vecRotAxisCS.k[0], vecRotAxisCS.k[1], vecRotAxisCS.k[2]);

	// Don't keep upright if any pontoons are contacting a surface.
	if ( CountSurfaceContactPoints( pImpacts ) > 0 )
	{
		m_flRollErrorPrev = angle;
		return;
	}
	
	// Don't do any correction if we're within 10 degrees of the goal orientation.
	if ( fabs( angle ) < DEG2RAD( 10 ) )
	{
		m_flRollErrorPrev = angle;
		return;
	}

	//Msg("CORRECTING\n");

	// Generate an angular impulse describing the rotation.
	IVP_U_Float_Point vecAngularImpulse;
	vecAngularImpulse.set_multiple( &vecRotAxisCS, m_pCore->get_mass() * ( 0.2f * angle + 0.3f * pEventSim->i_delta_time * ( angle - m_flRollErrorPrev ) ) );

	// Save the last error value for calculating the derivative.
	m_flRollErrorPrev = angle;

	// Clamp the impulse at a maximum length.
	IVP_FLOAT len = vecAngularImpulse.real_length_plus_normize();
	if ( len > ( DEG2RAD( 2 ) * m_pCore->get_mass() ) )
	{
		len = DEG2RAD( 2 ) * m_pCore->get_mass();
	}
	vecAngularImpulse.mult( len );
	m_pCore->rot_push_core_cs( &vecAngularImpulse );

	// Debugging visualization.
#if DRAW_AIRBOAT_KEEP_UPRIGHT_ROLL_VECTORS
	CPhysicsEnvironment *pEnv = (CPhysicsEnvironment *)m_pAirboatBody->get_core()->environment->client_data;
	IVPhysicsDebugOverlay *debugoverlay = pEnv->GetDebugOverlay();

	IVP_U_Float_Point vecPosIVP = m_pCore->get_position_PSI();
	Vector vecPosHL;
	ConvertPositionToHL(vecPosIVP, vecPosHL);

	Vector vecGoalAxisHL;
	ConvertDirectionToHL(vecGoalAxisWS, vecGoalAxisHL);

	IVP_U_Float_Point vecUpWS;
	matWorldFromCore->vmult3( &vecUpCS, &vecUpWS );
	Vector vecCurHL;
	ConvertDirectionToHL(vecUpWS, vecCurHL);

	static IVP_FLOAT flLastLen = 0;
	IVP_FLOAT flDebugLen = vecAngularImpulse.real_length();
	if ( flLastLen && ( fabs( flDebugLen - flLastLen ) > ( DEG2RAD( 0.25 ) * m_pCore->get_mass() ) )
	{
		debugoverlay->AddLineOverlay(vecPosHL, vecPosHL + Vector(0, 0, 10) * flDebugLen, 255, 0, 255, false, 100.0 );
	}
	else
	{
		debugoverlay->AddLineOverlay(vecPosHL, vecPosHL + Vector(0, 0, 10) * flDebugLen, 255, 255, 255, false, 100.0 );
	}
	debugoverlay->AddLineOverlay(vecPosHL + Vector(0, 0, 10) * flDebugLen, vecPosHL + Vector(0, 0, 10) * flDebugLen + vecGoalAxisHL * 10, 0, 255, 0, false, 100.0 );
	debugoverlay->AddLineOverlay(vecPosHL + Vector(0, 0, 10) * flDebugLen, vecPosHL + Vector(0, 0, 10) * flDebugLen + vecCurHL * 10, 255, 0, 0, false, 100.0 );
	flLastLen = flDebugLen;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wheel_nr - 
//			s_angle - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::do_steering_wheel(IVP_POS_WHEEL wheel_nr, IVP_FLOAT s_angle)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(wheel_nr);
	
    wheel->axis_direction_cs.set_to_zero();
    wheel->axis_direction_cs.k[ index_x ] = 1.0f;
    wheel->axis_direction_cs.rotate( IVP_COORDINATE_INDEX(index_y), s_angle);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			spring_constant - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_spring_constant(IVP_POS_WHEEL pos, IVP_FLOAT spring_constant)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->spring_constant = spring_constant;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			spring_dampening - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_spring_dampening(IVP_POS_WHEEL pos, IVP_FLOAT spring_dampening)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->spring_damp_relax = spring_dampening;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			spring_dampening - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_spring_dampening_compression(IVP_POS_WHEEL pos, IVP_FLOAT spring_dampening)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->spring_damp_compress = spring_dampening;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			pre_tension_length - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_spring_pre_tension(IVP_POS_WHEEL pos, IVP_FLOAT pre_tension_length)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->spring_len = gravity_y_direction * (wheel->distance_orig_hp_to_hp - pre_tension_length);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			spring_length - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_spring_length(IVP_POS_WHEEL pos, IVP_FLOAT spring_length)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->spring_len = spring_length;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			torque - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_wheel_torque(IVP_POS_WHEEL pos, IVP_FLOAT torque)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->torque = torque;

	// Wake the physics object if need be!
	m_pAirboatBody->get_environment()->get_controller_manager()->ensure_controller_in_simulation( this );
}

IVP_FLOAT CPhysics_Airboat::get_wheel_torque(IVP_POS_WHEEL pos)
{
	return get_wheel(pos)->torque;
}


//-----------------------------------------------------------------------------
// Purpose:
// Throttle input is -1 to 1.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::update_throttle( IVP_FLOAT flThrottle )
{
	// Forward
	if ( fabs( flThrottle ) < 0.01f )
	{
		m_flThrust = 0.0f;
	}
	else if ( flThrottle > 0.0f )
	{
		m_flThrust = AIRBOAT_THRUST_MAX * flThrottle;
	}
	else if ( flThrottle < 0.0f )
	{
		m_flThrust = AIRBOAT_THRUST_MAX_REVERSE * flThrottle;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			stop_wheel - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::fix_wheel(IVP_POS_WHEEL pos, IVP_BOOL stop_wheel)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    wheel->wheel_is_fixed = stop_wheel;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			friction - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_friction_of_wheel( IVP_POS_WHEEL pos, IVP_FLOAT friction )
{
	IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
	wheel->friction_of_wheel = friction;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
//			stabi_constant - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_stabilizer_constant(IVP_POS_AXIS pos, IVP_FLOAT stabi_constant)
{
   IVP_Raycast_Airboat_Axle *pAxle = get_axle( pos );
   pAxle->stabilizer_constant = stabi_constant;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fast_turn_factor_ - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_fast_turn_factor( IVP_FLOAT fast_turn_factor_ )
{
	//fast_turn_factor = fast_turn_factor_;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : force - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::change_body_downforce(IVP_FLOAT force)
{
    down_force = force;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : IVP_CONTROLLER_PRIORITY
//-----------------------------------------------------------------------------
IVP_CONTROLLER_PRIORITY CPhysics_Airboat::get_controller_priority()
{
    return IVP_CP_CONSTRAINTS_MAX;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : steering_angle_in - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::do_steering( IVP_FLOAT steering_angle_in, bool bAnalog )
{
	// Check for a change.
    if (  m_SteeringAngle == steering_angle_in) 
		return;

	MEM_ALLOC_CREDIT();

	// Set the new steering angle.
	m_bAnalogSteering = bAnalog;
    m_SteeringAngle = steering_angle_in;

	// Make sure the simulation is awake - we just go input.
    m_pAirboatBody->get_environment()->get_controller_manager()->ensure_controller_in_simulation( this );

	// Steer each wheel.
    for ( int iWheel = 0; iWheel < wheels_per_axis; ++iWheel )
	{
		do_steering_wheel( IVP_POS_WHEEL( iWheel ), m_SteeringAngle );
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pos - 
// Output : IVP_DOUBLE
//-----------------------------------------------------------------------------
IVP_DOUBLE CPhysics_Airboat::get_wheel_angular_velocity(IVP_POS_WHEEL pos)
{
    IVP_Raycast_Airboat_Wheel *wheel = get_wheel(pos);
    return wheel->wheel_angular_velocity;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : IVP_DOUBLE
//-----------------------------------------------------------------------------
IVP_DOUBLE CPhysics_Airboat::get_body_speed(IVP_COORDINATE_INDEX index)
{
    // return (IVP_FLOAT)car_body->get_geom_center_speed();
    IVP_U_Float_Point *vec_ws = &m_pAirboatBody->get_core()->speed;
    // works well as we do not use merged cores
    const IVP_U_Matrix *mat_ws = m_pAirboatBody->get_core()->get_m_world_f_core_PSI();
    IVP_U_Point orientation;
    mat_ws->get_col(index, &orientation);

    return orientation.dot_product(vec_ws);
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IVP_DOUBLE CPhysics_Airboat::get_orig_front_wheel_distance()
{
    IVP_U_Float_Point *left_wheel_cs = &this->get_wheel(IVP_FRONT_LEFT)->hp_cs;
    IVP_U_Float_Point *right_wheel_cs = &this->get_wheel(IVP_FRONT_RIGHT)->hp_cs;

    IVP_DOUBLE dist = left_wheel_cs->k[this->index_x] - right_wheel_cs->k[this->index_x];

    return IVP_Inline_Math::fabsd(dist); // was fabs, which was a sml call
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IVP_DOUBLE CPhysics_Airboat::get_orig_axles_distance()
{
    IVP_U_Float_Point *front_wheel_cs = &this->get_wheel(IVP_FRONT_LEFT)->hp_cs;
    IVP_U_Float_Point *rear_wheel_cs = &this->get_wheel(IVP_REAR_LEFT)->hp_cs;

    IVP_DOUBLE dist = front_wheel_cs->k[this->index_z] - rear_wheel_cs->k[this->index_z];

    return IVP_Inline_Math::fabsd(dist); // was fabs, which was a sml call
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *array_of_skid_info_out - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::get_skid_info( IVP_Wheel_Skid_Info *array_of_skid_info_out)
{
	for ( int w = 0; w < n_wheels; w++)
	{
		IVP_Wheel_Skid_Info &info = array_of_skid_info_out[w];
		//IVP_Constraint_Car_Object *wheel = car_constraint_solver->wheel_objects.element_at(w);
		info.last_contact_position_ws.set_to_zero(); // = wheel->last_contact_position_ws;
		info.last_skid_value = 0.0f; // wheel->last_skid_value;
		info.last_skid_time = 0.0f; //wheel->last_skid_time;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::InitRaycastCarEnvironment( IVP_Environment *pEnvironment, 
													        const IVP_Template_Car_System *pCarSystemTemplate )
{
	// Copies of the car system template component indices and handedness.
    index_x = pCarSystemTemplate->index_x;
    index_y = pCarSystemTemplate->index_y;
    index_z = pCarSystemTemplate->index_z;
    is_left_handed = pCarSystemTemplate->is_left_handed;

	IVP_Standard_Gravity_Controller *pGravityController = new IVP_Standard_Gravity_Controller();
	IVP_U_Point vecGravity( 0.0f, AIRBOAT_GRAVITY, 0.0f );
	pGravityController->grav_vec.set( &vecGravity );

	BEGIN_IVP_ALLOCATION();

	m_pAirboatBody->get_core()->add_core_controller( pGravityController );
	
	// Add this controller to the physics environment and setup the objects gravity.
    pEnvironment->get_controller_manager()->announce_controller_to_environment( this );

	END_IVP_ALLOCATION();

	extra_gravity = pCarSystemTemplate->extra_gravity_force_value;    

	// This works because gravity is still int the same direction, just smaller.
    if ( pEnvironment->get_gravity()->k[index_y] > 0 )
	{
		gravity_y_direction = 1.0f;
    }
	else
	{
		gravity_y_direction = -1.0f;
    }
    normized_gravity_ws.set( pEnvironment->get_gravity() );
    normized_gravity_ws.normize();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::InitRaycastCarBody( const IVP_Template_Car_System *pCarSystemTemplate )
{
	// Car body attributes.
    n_wheels = pCarSystemTemplate->n_wheels;
    n_axis = pCarSystemTemplate->n_axis;
    wheels_per_axis = n_wheels / n_axis;

	// Add the car body "core" to the list of raycast car controller "cores."
    m_pAirboatBody = pCarSystemTemplate->car_body;
    this->vector_of_cores.add( m_pAirboatBody->get_core() );

	// Init extra downward force applied to car.    
    down_force_vertical_offset = pCarSystemTemplate->body_down_force_vertical_offset;
    down_force = 0.0f;

	// Initialize.
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		m_pAirboatBody->get_core()->rot_speed.k[iAxis] = 0.0f;
		m_pAirboatBody->get_core()->speed.k[iAxis] = 0.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::InitRaycastCarWheels( const IVP_Template_Car_System *pCarSystemTemplate )
{
    IVP_U_Matrix m_core_f_object;
    m_pAirboatBody->calc_m_core_f_object( &m_core_f_object );

	// Initialize the car wheel system.
    for ( int iWheel = 0; iWheel < n_wheels; iWheel++ )
	{
		// Get and clear out memory for the current raycast wheel.
		IVP_Raycast_Airboat_Wheel *pRaycastWheel = get_wheel( IVP_POS_WHEEL( iWheel ) );
		P_MEM_CLEAR( pRaycastWheel );

		// Put the wheel in car space.
		m_core_f_object.vmult4( &pCarSystemTemplate->wheel_pos_Bos[iWheel], &pRaycastWheel->hp_cs );
		m_core_f_object.vmult4( &pCarSystemTemplate->trace_pos_Bos[iWheel], &pRaycastWheel->raycast_start_cs );
		
		// Add in the raycast start offset.
		pRaycastWheel->raycast_length = AIRBOAT_RAYCAST_DIST;
		pRaycastWheel->raycast_dir_cs.set_to_zero();
		pRaycastWheel->raycast_dir_cs.k[index_y] = gravity_y_direction;

		// Spring (Shocks) data.
		pRaycastWheel->spring_len = -pCarSystemTemplate->spring_pre_tension[iWheel];

		pRaycastWheel->spring_direction_cs.set_to_zero();
		pRaycastWheel->spring_direction_cs.k[index_y] = gravity_y_direction;
		
		pRaycastWheel->spring_constant = pCarSystemTemplate->spring_constant[iWheel];
		pRaycastWheel->spring_damp_relax = pCarSystemTemplate->spring_dampening[iWheel];
		pRaycastWheel->spring_damp_compress = pCarSystemTemplate->spring_dampening_compression[iWheel];

		// Wheel data.
		pRaycastWheel->friction_of_wheel = 1.0f;//pCarSystemTemplate->friction_of_wheel[iWheel];
		pRaycastWheel->wheel_radius = pCarSystemTemplate->wheel_radius[iWheel];
		pRaycastWheel->inv_wheel_radius = 1.0f / pCarSystemTemplate->wheel_radius[iWheel];

		do_steering_wheel( IVP_POS_WHEEL( iWheel ), 0.0f );
		
		pRaycastWheel->wheel_is_fixed = IVP_FALSE;	
		pRaycastWheel->max_rotation_speed = pCarSystemTemplate->wheel_max_rotation_speed[iWheel>>1];

		pRaycastWheel->wheel_is_fixed = IVP_TRUE;
    }
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysics_Airboat::InitRaycastCarAxes( const IVP_Template_Car_System *pCarSystemTemplate )
{
    m_SteeringAngle = -1.0f;			// make sure next call is not optimized
    this->do_steering( 0.0f, false );	// make sure next call gets through

    for ( int iAxis = 0; iAxis < n_axis; iAxis++ )
	{
		IVP_Raycast_Airboat_Axle *pAxle = get_axle( IVP_POS_AXIS( iAxis ) );
		pAxle->stabilizer_constant = pCarSystemTemplate->stabilizer_constant[iAxis];
    }    
}


//-----------------------------------------------------------------------------
// Purpose: Debug data for use in vphysics and the engine to visualize car data.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::SetCarSystemDebugData( const IVP_CarSystemDebugData_t &carSystemDebugData )
{
	// Wheels (raycast data only!)
	for ( int iWheel = 0; iWheel < IVP_RAYCAST_AIRBOAT_MAX_WHEELS; ++iWheel )
	{
		m_CarSystemDebugData.wheelRaycasts[iWheel][0] = carSystemDebugData.wheelRaycasts[iWheel][0];
		m_CarSystemDebugData.wheelRaycasts[iWheel][1] = carSystemDebugData.wheelRaycasts[iWheel][1];
		m_CarSystemDebugData.wheelRaycastImpacts[iWheel] = carSystemDebugData.wheelRaycastImpacts[iWheel];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Debug data for use in vphysics and the engine to visualize car data.
//-----------------------------------------------------------------------------
void CPhysics_Airboat::GetCarSystemDebugData( IVP_CarSystemDebugData_t &carSystemDebugData )
{
	// Wheels (raycast data only!)
	for ( int iWheel = 0; iWheel < IVP_RAYCAST_AIRBOAT_MAX_WHEELS; ++iWheel )
	{
		carSystemDebugData.wheelRaycasts[iWheel][0] = m_CarSystemDebugData.wheelRaycasts[iWheel][0];
		carSystemDebugData.wheelRaycasts[iWheel][1] = m_CarSystemDebugData.wheelRaycasts[iWheel][1];
		carSystemDebugData.wheelRaycastImpacts[iWheel] = m_CarSystemDebugData.wheelRaycastImpacts[iWheel];
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : IVP_U_Vector<IVP_Core>
//-----------------------------------------------------------------------------
IVP_U_Vector<IVP_Core> *CPhysics_Airboat::get_associated_controlled_cores( void )
{ 
	return &vector_of_cores; 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *core - 
//-----------------------------------------------------------------------------
void CPhysics_Airboat::core_is_going_to_be_deleted_event( IVP_Core *core )
{ 
	P_DELETE_THIS(this); 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : IVP_Raycast_Airboat_Axle
//-----------------------------------------------------------------------------
IVP_Raycast_Airboat_Axle *CPhysics_Airboat::get_axle( IVP_POS_AXIS i )
{ 
	return &m_aAirboatAxles[i]; 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : IVP_Raycast_Airboat_Wheel
//-----------------------------------------------------------------------------
IVP_Raycast_Airboat_Wheel *CPhysics_Airboat::get_wheel( IVP_POS_WHEEL i )
{ 
	return &m_aAirboatWheels[i]; 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IVP_Controller_Raycast_Airboat_Vector_of_Cores_1::IVP_Controller_Raycast_Airboat_Vector_of_Cores_1():
							IVP_U_Vector<IVP_Core>( &elem_buffer[0],1 ) 
{
}

