//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYSICS_CONTROLLER_RAYCAST_VEHICLE_H
#define PHYSICS_CONTROLLER_RAYCAST_VEHICLE_H
#ifdef _WIN32
#pragma once
#endif

#include "ivp_controller.hxx"
#include "ivp_car_system.hxx"
#include "ivp_controller_raycast_car.hxx"

class IPhysicsObject;

//=============================================================================
//
// Raycast Car System
//
class CPhysics_Car_System_Raycast_Wheels : public IVP_Controller_Raycast_Car 
{

public:

    CPhysics_Car_System_Raycast_Wheels( IVP_Environment *env, const IVP_Template_Car_System *t );
    virtual ~CPhysics_Car_System_Raycast_Wheels();

    virtual void do_raycasts( IVP_Event_Sim *, int n_wheels, IVP_Ray_Solver_Template *t_in,
			                  IVP_Ray_Hit *hits_out, IVP_FLOAT *friction_of_object_out );

    void update_wheel_positions( void );

	IPhysicsObject *GetWheel( int index );

	virtual const char *get_controller_name() { return "sys:vehicle"; }
protected:

	void InitCarSystemWheels( const IVP_Template_Car_System *pCarSystem );

    IVP_Real_Object *m_pWheels[IVP_RAYCAST_CAR_MAX_WHEELS];
};

#endif // PHYSICS_CONTROLLER_RAYCAST_VEHICLE_H
