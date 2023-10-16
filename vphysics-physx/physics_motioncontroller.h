//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_MOTIONCONTROLLER_H
#define PHYSICS_MOTIONCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

class IPhysicsMotionController;
class CPhysicsEnvironment;
class IMotionEvent;

IPhysicsMotionController *CreateMotionController( CPhysicsEnvironment *pEnv, IMotionEvent *pHandler );

#endif // PHYSICS_MOTIONCONTROLLER_H
