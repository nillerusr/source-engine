//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_CONSTRAINT_H
#define PHYSICS_CONSTRAINT_H
#pragma once

class IVP_Environment;

class CPhysicsObject;
class IPhysicsConstraint;
class IPhysicsConstraintGroup;

extern IPhysicsConstraint *CreateRagdollConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );
extern IPhysicsConstraint *CreateHingeConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &ragdoll );
extern IPhysicsConstraint *CreateFixedConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
extern IPhysicsConstraint *CreateBallsocketConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
extern IPhysicsConstraint *CreateSlidingConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &slide );
extern IPhysicsConstraint *CreatePulleyConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley );
extern IPhysicsConstraint *CreateLengthConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

extern IPhysicsConstraintGroup *CreatePhysicsConstraintGroup( IVP_Environment *pEnvironment, const constraint_groupparams_t &group );

extern IPhysicsConstraint *GetClientDataForHkConstraint( class hk_Breakable_Constraint *pHkConstraint );

extern bool IsExternalConstraint( IVP_Controller *pLCS, void *pGameData );

#endif // PHYSICS_CONSTRAINT_H
