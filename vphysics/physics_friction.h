//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYSICS_FRICTION_H
#define PHYSICS_FRICTION_H
#ifdef _WIN32
#pragma once
#endif


class IVP_Real_Object;
class IPhysicsFrictionSnapshot;

IPhysicsFrictionSnapshot *CreateFrictionSnapshot( IVP_Real_Object *pObject );
void DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot );
void DeleteAllFrictionPairs( IVP_Real_Object *pObject0, IVP_Real_Object *pObject1 );

#endif // PHYSICS_FRICTION_H
