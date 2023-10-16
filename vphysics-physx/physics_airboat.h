//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYSICS_AIRBOAT_H
#define PHYSICS_AIRBOAT_H
#ifdef _WIN32
#pragma once
#endif

#include "ivp_controller.hxx"
#include "ivp_car_system.hxx"


class IPhysicsObject;
class IVP_Ray_Solver_Template;
class IVP_Ray_Hit;
class IVP_Event_Sim;


#define IVP_RAYCAST_AIRBOAT_MAX_WHEELS			4


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class IVP_Raycast_Airboat_Wheel
{
public:

    // static section
    IVP_U_Float_Point	hp_cs;										// hard point core system projected on y plane
	IVP_U_Float_Point	raycast_start_cs;							// ray cast start position
	IVP_U_Float_Point	raycast_dir_cs;
	IVP_FLOAT			raycast_length;

    IVP_U_Float_Point	spring_direction_cs;						// spring direction in core-space
    IVP_FLOAT			distance_orig_hp_to_hp;						// distance hp is moved by projecting it onto the y - plane
    IVP_FLOAT			spring_len;  								// == pretension + distance_orig_hp_to_hp
    IVP_FLOAT			spring_constant;							// shock at wheel spring constant
    IVP_FLOAT			spring_damp_relax;							// shock at wheel spring dampening during relaxation
    IVP_FLOAT			spring_damp_compress;						// shock at wheel spring dampening during compression

    IVP_FLOAT			max_rotation_speed;							// max rotational speed of the wheel
    
    IVP_FLOAT			wheel_radius;								// wheel radius
    IVP_FLOAT			inv_wheel_radius;							// inverse wheel radius
    IVP_FLOAT			friction_of_wheel;							// wheel friction
    
    // dynamic section
    IVP_FLOAT			torque;										// torque applied to wheel
    IVP_BOOL			wheel_is_fixed;								// eg. handbrake (fixed = stationary)
    IVP_U_Float_Point	axis_direction_cs;							// axle direction in core-space
    IVP_FLOAT			angle_wheel;								// wheel angle
    IVP_FLOAT			wheel_angular_velocity;						// angular velocity of wheel
    
    // out
    IVP_U_Float_Point	surface_speed_of_wheel_on_ground_ws;		// actual speed in world-space
    IVP_FLOAT			pressure;									// force from gravity, mass of car, stabilizers, etc. on wheel
    IVP_FLOAT			raycast_dist;								// raycast distance to impact for wheel
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class IVP_Raycast_Airboat_Impact
{
public:

    IVP_FLOAT			friction_value;								// combined (multiply) frictional value of impact surface and wheel
    IVP_FLOAT			stabilizer_force;							// force on wheel due to axle stabilization
    IVP_Real_Object		*moveable_object_hit_by_ray;				// moveable physics object hit by raycast

	IVP_U_Float_Point	raycast_dir_ws;								// raycast direction in world-space
    IVP_U_Float_Point	spring_direction_ws;						// spring direction (raycast for impact direction) in world-space
    IVP_U_Float_Point	surface_speed_wheel_ws;						// wheel speed in world-space
    IVP_U_Float_Point	projected_surface_speed_wheel_ws;			// ???
    IVP_U_Float_Point	axis_direction_ws;							// axle direction in world-space
    IVP_U_Float_Point	projected_axis_direction_ws;				// ???

    IVP_FLOAT			forces_needed_to_drive_straight;			// forces need to keep the vehicle driving straight (attempt and directional wheel friction)
    IVP_FLOAT			inv_normal_dot_dir;							// ???

	// Impact information.
	IVP_BOOL			bImpact;							// Had an impact?
	IVP_BOOL			bImpactWater;						// Impact with water?
	IVP_BOOL			bInWater;							// Point in water?
	IVP_U_Point			vecImpactPointWS;					// Impact point in world-space.
	IVP_U_Float_Point	vecImpactNormalWS;					// Impact normal in world-space.
	IVP_FLOAT			flDepth;							// Distance to water surface.
	IVP_FLOAT			flFriction;							// Friction at impact point.
	IVP_FLOAT			flDampening;						// Dampening at surface.
	int					nSurfaceProps;						// Surface property!
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class IVP_Raycast_Airboat_Axle 
{
public:

    IVP_FLOAT			stabilizer_constant;						// axle (for wheels) stabilizer constant
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class IVP_Controller_Raycast_Airboat_Vector_of_Cores_1: public IVP_U_Vector<IVP_Core> 
{
    void *elem_buffer[1];

public:

    IVP_Controller_Raycast_Airboat_Vector_of_Cores_1();
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CPhysics_Airboat : public IVP_Car_System, protected IVP_Controller_Dependent  
{

public:

    CPhysics_Airboat( IVP_Environment *env, const IVP_Template_Car_System *t, IPhysicsGameTrace *pGameTrace );
    virtual ~CPhysics_Airboat();

    void update_wheel_positions( void ) {}
	void SetWheelFriction( int iWheel, float flFriction );

	IPhysicsObject *GetWheel( int index );

	virtual const char *get_controller_name() { return "sys:airboat"; }

protected:

	void InitAirboat( const IVP_Template_Car_System *pCarSystem );
	float GetWaterDepth( Ray_t *pGameRay, IPhysicsObject *pPhysAirboat );

	// Purpose: Deconstructor
	void PerformFrictionNotification( float flEliminatedEnergy, float dt, int nSurfaceProp, IPhysicsCollisionData *pCollisionData );

	void do_raycasts_gameside( int nRaycastCount, IVP_Ray_Solver_Template *pRays, IVP_Raycast_Airboat_Impact *pImpacts );
	void pre_raycasts_gameside( int nRaycastCount, IVP_Ray_Solver_Template *pRays, Ray_t *pGameRays, IVP_Raycast_Airboat_Impact *pImpacts );

    IVP_Real_Object		*m_pWheels[IVP_RAYCAST_AIRBOAT_MAX_WHEELS];
	IPhysicsGameTrace	*m_pGameTrace;

public:

	// Steering
    void							do_steering_wheel(IVP_POS_WHEEL wheel_pos, IVP_FLOAT s_angle);						// called by do_steering()

    // Car Adjustment
    void							change_spring_constant(IVP_POS_WHEEL pos, IVP_FLOAT spring_constant);				// [Newton/meter]
    void							change_spring_dampening(IVP_POS_WHEEL pos, IVP_FLOAT spring_dampening);				// when spring is relaxing spring
    void							change_spring_dampening_compression(IVP_POS_WHEEL pos, IVP_FLOAT spring_dampening); // [Newton/meter] for compressing spring
    void							change_max_body_force(IVP_POS_WHEEL , IVP_FLOAT mforce) {}
    void							change_spring_pre_tension(IVP_POS_WHEEL pos, IVP_FLOAT pre_tension_length);
	void							change_spring_length(IVP_POS_WHEEL pos, IVP_FLOAT spring_length);

    void							change_stabilizer_constant(IVP_POS_AXIS pos, IVP_FLOAT stabi_constant);				// [Newton/meter]
	void							change_fast_turn_factor( IVP_FLOAT fast_turn_factor_ );								// not implemented for raycasts
    void							change_wheel_torque(IVP_POS_WHEEL pos, IVP_FLOAT torque);
	IVP_FLOAT						get_wheel_torque(IVP_POS_WHEEL wheel_nr);
    
	void							update_throttle( IVP_FLOAT flThrottle );

    void							update_body_countertorque() {}
    
    void							change_body_downforce(IVP_FLOAT force);												// extra force to keep flipped objects flipped over

    void							fix_wheel( IVP_POS_WHEEL, IVP_BOOL stop_wheel );									// stop wheel completely (e.g. handbrake )
	void							change_friction_of_wheel( IVP_POS_WHEEL pos, IVP_FLOAT friction );
	void							set_powerslide( float frontAccel, float rearAccel )	{}

    // Car Info
    IVP_DOUBLE						get_body_speed(IVP_COORDINATE_INDEX idx_z = IVP_INDEX_Z);							// km/h in 'z' direction
    IVP_DOUBLE						get_wheel_angular_velocity(IVP_POS_WHEEL);
    IVP_DOUBLE						get_orig_front_wheel_distance();
    IVP_DOUBLE						get_orig_axles_distance();
	void							get_skid_info( IVP_Wheel_Skid_Info *array_of_skid_info_out);
    
    void							get_wheel_position(IVP_U_Point *position_ws_out, IVP_U_Quat *direction_ws_out);
  
    // Methods: 2nd Level, based on primitives
    virtual void					do_steering(IVP_FLOAT steering_angle_in, bool bAnalog);											// default implementation updates this->steering_angle

	//
	// Booster (the airboat has no booster). 
	//
	virtual bool IsBoosting(void) { return false; }
    virtual void					set_booster_acceleration( IVP_FLOAT acceleration) {}
    virtual void					activate_booster(IVP_FLOAT thrust, IVP_FLOAT duration, IVP_FLOAT delay) {}
    virtual void					update_booster(IVP_FLOAT delta_time) {}
	virtual IVP_FLOAT				get_booster_delay() { return 0; }
	virtual IVP_FLOAT				get_booster_time_to_go() { return 0; }
    
	// Debug
	void							SetCarSystemDebugData( const IVP_CarSystemDebugData_t &carSystemDebugData );
	void							GetCarSystemDebugData( IVP_CarSystemDebugData_t &carSystemDebugData );

protected:

	IVP_Core						*m_pCore;
	IVP_U_Float_Point				m_vecLocalVelocity;
	float							m_flSpeed;
    IVP_Real_Object					*m_pAirboatBody;			// *car_body

	// Wheels/Axles.
    short							n_wheels;
    short							n_axis;
    short							wheels_per_axis;
    IVP_Raycast_Airboat_Wheel		m_aAirboatWheels[IVP_RAYCAST_AIRBOAT_MAX_WHEELS];		// wheel_of_car
    IVP_Raycast_Airboat_Axle		m_aAirboatAxles[IVP_RAYCAST_AIRBOAT_MAX_WHEELS/2];		// axis_of_car

	// Gravity.
    IVP_FLOAT						gravity_y_direction; //  +/-1
    IVP_U_Float_Point				normized_gravity_ws;
    IVP_FLOAT						extra_gravity;
    
	// Orientation.
    IVP_COORDINATE_INDEX			index_x;
    IVP_COORDINATE_INDEX			index_y;
    IVP_COORDINATE_INDEX			index_z;
    IVP_BOOL						is_left_handed;

	// Speed.
    IVP_FLOAT						max_speed;

	//
    IVP_FLOAT						down_force;
    IVP_FLOAT						down_force_vertical_offset;

    // Steering
    IVP_FLOAT						m_SteeringAngle;
	bool							m_bSteeringReversed;
	bool							m_bAnalogSteering;
	IVP_FLOAT						m_flPrevSteeringAngle;
	IVP_FLOAT						m_flSteerTime;			// Number of seconds we've steered in this direction.

	// Thrust.
	IVP_FLOAT						m_flThrust;

	bool							m_bAirborne;		// Whether we are airborne or not.
	IVP_FLOAT						m_flAirTime;		// How long we've been airborne (if we are).
	bool							m_bWeakJump;		// Set when we become airborne while going slow.

	// Pitch and roll stabilizers.
	IVP_FLOAT						m_flPitchErrorPrev;
	IVP_FLOAT						m_flRollErrorPrev;

	// Debugging!
	IVP_CarSystemDebugData_t		m_CarSystemDebugData;
    
protected:

    IVP_Raycast_Airboat_Wheel		*get_wheel( IVP_POS_WHEEL i );
    IVP_Raycast_Airboat_Axle		*get_axle( IVP_POS_AXIS i );
							
    virtual void			core_is_going_to_be_deleted_event( IVP_Core * );
    virtual IVP_U_Vector<IVP_Core>  *get_associated_controlled_cores( void );
    
    virtual void                    do_simulation_controller(IVP_Event_Sim *,IVP_U_Vector<IVP_Core> *core_list);
    virtual IVP_CONTROLLER_PRIORITY get_controller_priority();

private:

	// Initialization. 
	void							InitRaycastCarEnvironment( IVP_Environment *pEnvironment, const IVP_Template_Car_System *pCarSystemTemplate );
	void							InitRaycastCarBody( const IVP_Template_Car_System *pCarSystemTemplate );
	void							InitRaycastCarWheels( const IVP_Template_Car_System *pCarSystemTemplate );
	void							InitRaycastCarAxes( const IVP_Template_Car_System *pCarSystemTemplate );

	// Raycasts for simulation.
	void							PreRaycasts( IVP_Ray_Solver_Template *pRaySolverTemplates, const IVP_U_Matrix *m_world_f_core, IVP_Raycast_Airboat_Impact *pImpacts );	
	bool							PostRaycasts( IVP_Ray_Solver_Template *pRaySolverTemplates, const IVP_U_Matrix *matWorldFromCore, IVP_Raycast_Airboat_Impact *pImpacts );

	// Simulation.
	void							DoSimulationPontoons( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim );
	void							DoSimulationPontoonsGround( IVP_Raycast_Airboat_Wheel *pPontoonPoint, IVP_Raycast_Airboat_Impact *pImpact, IVP_Event_Sim *pEventSim );
	void							DoSimulationPontoonsWater( IVP_Raycast_Airboat_Wheel *pPontoonPoint, IVP_Raycast_Airboat_Impact *pImpact, IVP_Event_Sim *pEventSim );
	void							DoSimulationDrag( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim );
	void							DoSimulationTurbine( IVP_Event_Sim *pEventSim );
	void							DoSimulationSteering( IVP_Event_Sim *pEventSim );
	void							DoSimulationKeepUprightPitch( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim );
	void							DoSimulationKeepUprightRoll( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim );
	void							DoSimulationGravity( IVP_Event_Sim *pEventSim );

	int								CountSurfaceContactPoints( IVP_Raycast_Airboat_Impact *pImpacts );
	void							UpdateAirborneState( IVP_Raycast_Airboat_Impact *pImpacts, IVP_Event_Sim *pEventSim );

	float							ComputeFrontPontoonWaveNoise( int nPontoonIndex, float flSpeedRatio );

	void							CalcImpactPosition( IVP_Ray_Solver_Template *pRaySolver, IVP_Raycast_Airboat_Wheel *pPontoonPoint,
													    IVP_Raycast_Airboat_Impact *pImpacts );

    IVP_Controller_Raycast_Airboat_Vector_of_Cores_1 vector_of_cores;
};

#endif // PHYSICS_AIRBOAT_H
