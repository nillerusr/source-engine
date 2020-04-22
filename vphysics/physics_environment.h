//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_WORLD_H
#define PHYSICS_WORLD_H
#pragma once

#include "vphysics_interface.h"
#include "ivu_types.hxx"
#include "utlvector.h"

class IVP_Environment;
class CSleepObjects;
class CPhysicsListenerCollision;
class CPhysicsListenerConstraint;
class IVP_Listener_Collision;
class IVP_Listener_Constraint;
class IVP_Listener_Object;
class IVP_Controller;
class CPhysicsFluidController;
class CCollisionSolver;
class CPhysicsObject;
class CDeleteQueue;
class IVPhysicsDebugOverlay;
struct constraint_limitedhingeparams_t;
struct vphysics_save_iphysicsobject_t;

class CPhysicsEnvironment : public IPhysicsEnvironment
{
public:
	CPhysicsEnvironment( void );
	~CPhysicsEnvironment( void );

	virtual void SetDebugOverlay( CreateInterfaceFn debugOverlayFactory );
	virtual IVPhysicsDebugOverlay *GetDebugOverlay( void );

	void			SetGravity( const Vector& gravityVector );
	IPhysicsObject	*CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams );
	IPhysicsObject	*CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams );
	virtual unsigned int	GetObjectSerializeSize( IPhysicsObject *pObject ) const;
	virtual void			SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize );
	virtual IPhysicsObject *UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions );
	


	IPhysicsSpring	*CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams );
	IPhysicsFluidController	*CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams );
	IPhysicsConstraint *CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );

	virtual IPhysicsConstraint *CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge );
	virtual IPhysicsConstraint *CreateLimitedHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &hinge );
	virtual IPhysicsConstraint *CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
	virtual IPhysicsConstraint *CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding );
	virtual IPhysicsConstraint *CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
	virtual IPhysicsConstraint *CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley );
	virtual IPhysicsConstraint *CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

	virtual IPhysicsConstraintGroup *CreateConstraintGroup( const constraint_groupparams_t &group );
	virtual void DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup );

	void			Simulate( float deltaTime );
	float			GetSimulationTimestep() const;
	void			SetSimulationTimestep( float timestep );
	float			GetSimulationTime() const;
	float			GetNextFrameTime() const;
	bool			IsInSimulation() const;

	virtual void DestroyObject( IPhysicsObject * );
	virtual void DestroySpring( IPhysicsSpring * );
	virtual void DestroyFluidController( IPhysicsFluidController * );
	virtual void DestroyConstraint( IPhysicsConstraint * );

	virtual void SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents );
	virtual void SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents );
	virtual void SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents );

	virtual IPhysicsShadowController *CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation );
	virtual void DestroyShadowController( IPhysicsShadowController * );
	virtual IPhysicsMotionController *CreateMotionController( IMotionEvent *pHandler );
	virtual void DestroyMotionController( IPhysicsMotionController *pController );
	virtual IPhysicsPlayerController *CreatePlayerController( IPhysicsObject *pObject );
	virtual void DestroyPlayerController( IPhysicsPlayerController *pController );
	virtual IPhysicsVehicleController *CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace );
	virtual void DestroyVehicleController( IPhysicsVehicleController *pController );

	virtual void SetQuickDelete( bool bQuick )
	{
		m_deleteQuick = bQuick;
	}
	virtual bool ShouldQuickDelete() const { return m_deleteQuick; }
	virtual void TraceBox( trace_t *ptr, const Vector &mins, const Vector &maxs, const Vector &start, const Vector &end );
	virtual void SetCollisionSolver( IPhysicsCollisionSolver *pCollisionSolver );
	virtual void GetGravity( Vector *pGravityVector ) const;
	virtual int	 GetActiveObjectCount() const;
	virtual void GetActiveObjects( IPhysicsObject **pOutputObjectList ) const;
	virtual const IPhysicsObject **GetObjectList( int *pOutputObjectCount ) const;
	virtual bool TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment );

	IVP_Environment	*GetIVPEnvironment( void ) { return m_pPhysEnv; }
	void		ClearDeadObjects( void );
	IVP_Controller *GetDragController() { return m_pDragController; }
	const IVP_Controller *GetDragController() const { return m_pDragController; }
	virtual void SetAirDensity( float density );
	virtual float GetAirDensity( void ) const;
	virtual void ResetSimulationClock( void );
	virtual IPhysicsObject *CreateSphereObject( float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
	virtual void CleanupDeleteList();
	virtual void EnableDeleteQueue( bool enable ) { m_queueDeleteObject = enable; }
	// debug
	virtual bool IsCollisionModelUsed( CPhysCollide *pCollide ) const;

	// trace against the physics world
	virtual void TraceRay( const Ray_t &ray, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace );
	virtual void SweepCollideable( const CPhysCollide *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace );

	// performance tuning
	virtual void GetPerformanceSettings( physics_performanceparams_t *pOutput ) const;
	virtual void SetPerformanceSettings( const physics_performanceparams_t *pSettings );

	// perf/cost statistics
	virtual void ReadStats( physics_stats_t *pOutput );
	virtual void ClearStats();
	virtual void EnableConstraintNotify( bool bEnable );
	// debug
	virtual void DebugCheckContacts(void);

	// Save/restore
	bool Save( const physsaveparams_t &params  );
	void PreRestore( const physprerestoreparams_t &params );
	bool Restore( const physrestoreparams_t &params );
	void PostRestore();
	void PhantomAdd( CPhysicsObject *pObject );
	void PhantomRemove( CPhysicsObject *pObject );

	void AddPlayerController( IPhysicsPlayerController *pController );
	void RemovePlayerController( IPhysicsPlayerController *pController );
	IPhysicsPlayerController *FindPlayerController( IPhysicsObject *pObject );

	IPhysicsCollisionEvent *GetCollisionEventHandler();
	// a constraint is being disabled - report the game DLL as "broken"
	void NotifyConstraintDisabled( IPhysicsConstraint *pConstraint );

private:
	IVP_Environment					*m_pPhysEnv;
	IVP_Controller					*m_pDragController;
	IVPhysicsDebugOverlay			*m_pDebugOverlay;			// Interface to use for drawing debug overlays.
	CUtlVector<IPhysicsObject *>	m_objects;
	CUtlVector<IPhysicsObject *>	m_deadObjects;
	CUtlVector<CPhysicsFluidController *> m_fluids;
	CUtlVector<IPhysicsPlayerController *> m_playerControllers;
	CSleepObjects					*m_pSleepEvents;
	CPhysicsListenerCollision		*m_pCollisionListener;
	CCollisionSolver				*m_pCollisionSolver;
	CPhysicsListenerConstraint		*m_pConstraintListener;
	CDeleteQueue					*m_pDeleteQueue;
	int								m_lastObjectThisTick;
	bool							m_deleteQuick;
	bool							m_inSimulation;
	bool							m_queueDeleteObject;
	bool							m_fixedTimestep;
	bool							m_enableConstraintNotify;
};

extern IPhysicsEnvironment *CreatePhysicsEnvironment( void );

class IVP_Synapse_Friction;
class IVP_Real_Object;
extern IVP_Real_Object *GetOppositeSynapseObject( IVP_Synapse_Friction *pfriction );
extern IPhysicsObjectPairHash *CreateObjectPairHash();

#endif // PHYSICS_WORLD_H
