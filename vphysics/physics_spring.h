//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_SPRING_H
#define PHYSICS_SPRING_H
#pragma once


class IPhysicsSpring;
class IVP_Environment;
class IVP_Real_Object;

struct springparams_t;

IPhysicsSpring *CreateSpring( IVP_Environment *pEnvironment, CPhysicsObject *pObjectStart, CPhysicsObject *pObjectEnd, springparams_t *pParams );


#endif // PHYSICS_SPRING_H
