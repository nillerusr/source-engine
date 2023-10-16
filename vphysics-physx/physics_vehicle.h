//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_VEHICLE_H
#define PHYSICS_VEHICLE_H
#ifdef _WIN32
#pragma once
#endif


struct vehicleparams_t;
class IPhysicsVehicleController;
class CPhysicsObject;
class CPhysicsEnvironment;
class IVP_Real_Object;

bool ShouldOverrideWheelContactFriction( float *pFrictionOut, IVP_Real_Object *pivp0, IVP_Real_Object *pivp1, IVP_U_Float_Point *pNormal );

IPhysicsVehicleController *CreateVehicleController( CPhysicsEnvironment *pEnv, CPhysicsObject *pBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace );

#endif // PHYSICS_VEHICLE_H
