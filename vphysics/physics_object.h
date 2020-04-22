//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_OBJECT_H
#define PHYSICS_OBJECT_H

#ifdef _WIN32
#pragma once
#endif

#include "vphysics_interface.h"

class IVP_Real_Object;
class IVP_Environment;
class IVP_U_Float_Point;
class IVP_SurfaceManager;
class IVP_Controller;
class CPhysicsEnvironment;
struct vphysics_save_cphysicsobject_t
{
	const CPhysCollide *pCollide;
	const char *pName;
	float	sphereRadius;

	bool	isStatic;
	bool	collisionEnabled;
	bool	gravityEnabled;
	bool	dragEnabled;
	bool	motionEnabled;
	bool	isAsleep;
	bool	isTrigger;
	bool	asleepSinceCreation;		// has this been asleep since creation?
	bool	hasTouchedDynamic;
	bool	hasShadowController;
	short	collideType;
	unsigned short	gameIndex;
	int		hingeAxis;
	int		materialIndex;
	float	mass;
	Vector	rotInertia;
	float	speedDamping;
	float	rotSpeedDamping;
	Vector	massCenterOverride;

	unsigned int	callbacks;
	unsigned int	gameFlags;

	unsigned int	contentsMask;

	float			volume;
	float			dragCoefficient;
	float			angDragCoefficient;
	IPhysicsShadowController	*pShadow;
	//bool			m_shadowTempGravityDisable;

	Vector			origin;
	QAngle			angles;
	Vector			velocity;
	AngularImpulse	angVelocity;

	DECLARE_SIMPLE_DATADESC();
};

enum
{
	OBJ_AWAKE = 0,			// awake, simulating
	OBJ_STARTSLEEP = 1,		// going to sleep, but not queried yet
	OBJ_SLEEP = 2,			// sleeping, no state changes since last query
};


class CPhysicsObject : public IPhysicsObject
{
public:
	CPhysicsObject( void );
	virtual ~CPhysicsObject( void );

	void			Init( const CPhysCollide *pCollisionModel, IVP_Real_Object *pObject, int materialIndex, float volume, float drag, float angDrag );

	// IPhysicsObject functions
	bool			IsStatic() const;
	bool			IsAsleep() const;
	bool			IsTrigger() const;
	bool			IsFluid() const;
	bool			IsHinged() const { return (m_hingedAxis != 0) ? true : false; }
	bool			IsCollisionEnabled() const;
	bool			IsGravityEnabled() const;
	bool			IsDragEnabled() const;
	bool			IsMotionEnabled() const;
	bool			IsMoveable() const;
	bool			IsAttachedToConstraint( bool bExternalOnly ) const;


	void			EnableCollisions( bool enable );
	// Enable / disable gravity for this object
	void			EnableGravity( bool enable );
	// Enable / disable air friction / drag for this object
	void			EnableDrag( bool enable );
	void			EnableMotion( bool enable );

	void			SetGameData( void *pAppData );
	void			*GetGameData( void ) const;
	void			SetCallbackFlags( unsigned short callbackflags );
	unsigned short	GetCallbackFlags( void ) const;
	void			SetGameFlags( unsigned short userFlags );
	unsigned short	GetGameFlags( void ) const;
	void			SetGameIndex( unsigned short gameIndex );
	unsigned short	GetGameIndex( void ) const;

	void			Wake();
	void			Sleep();
	void			RecheckCollisionFilter();
	void			RecheckContactPoints();

	void			SetMass( float mass );
	float			GetMass( void ) const;
	float			GetInvMass( void ) const;
	void			SetInertia( const Vector &inertia );
	Vector			GetInertia( void ) const;
	Vector			GetInvInertia( void ) const;

	void			GetDamping( float *speed, float *rot ) const;
	void			SetDamping( const float *speed, const float *rot );
	void			SetDragCoefficient( float *pDrag, float *pAngularDrag );
	void			SetBuoyancyRatio( float ratio );
	int				GetMaterialIndex() const { return GetMaterialIndexInternal(); }
	void			SetMaterialIndex( int materialIndex );
	inline int		GetMaterialIndexInternal( void ) const { return m_materialIndex; }

	unsigned int	GetContents() const { return m_contentsMask; }
	void			SetContents( unsigned int contents );

	float			GetSphereRadius() const;
	Vector			GetMassCenterLocalSpace() const;
	float			GetEnergy() const;

	void			SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport = false );
	void			SetPositionMatrix( const matrix3x4_t& matrix, bool isTeleport = false  );
	void			GetPosition( Vector *worldPosition, QAngle *angles ) const;
	void			GetPositionMatrix( matrix3x4_t *positionMatrix ) const;

	void			SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );
	void			SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity );
	void			AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );
	void			GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;
	void			GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;
	void			GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const;

	void			LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const;
	void			WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const;
	void			LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const;
	void			WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const;

	void			ApplyForceCenter( const Vector &forceVector );
	void			ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition );
	void			ApplyTorqueCenter( const AngularImpulse & );
	void			CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const;
	void			CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const;
	float			CalculateLinearDrag( const Vector &unitDirection ) const;
	float			CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const;

	bool			GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const;
	void			SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation );
	void			UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset );
	void			RemoveShadowController();
	int				GetShadowPosition( Vector *position, QAngle *angles ) const;
	IPhysicsShadowController *GetShadowController( void ) const;
	float			ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt );

	const CPhysCollide	*GetCollide( void ) const;
	char const		*GetName() const;

	float			GetDragInDirection( const IVP_U_Float_Point &dir ) const;
	float			GetAngularDragInDirection( const IVP_U_Float_Point &angVelocity ) const;
	void			BecomeTrigger();
	void			RemoveTrigger();
	void			BecomeHinged( int localAxis );
	void			RemoveHinged();

	IPhysicsFrictionSnapshot *CreateFrictionSnapshot();
	void			DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot );

	void			OutputDebugInfo() const;

	// local functions
	inline	IVP_Real_Object *GetObject( void ) const { return m_pObject; }
	inline int		CallbackFlags( void ) const { return m_callbacks; }
	inline void		AddCallbackFlags( unsigned short flags ) { m_callbacks |= flags; }
	inline void		RemoveCallbackFlags( unsigned short flags ) { m_callbacks &= ~flags; }
	inline bool		HasTouchedDynamic();
	inline void		SetTouchedDynamic();
	void			NotifySleep( void );
	void			NotifyWake( void );
	int				GetSleepState( void ) const { return m_sleepState; }
	inline void		ForceSilentDelete() { m_forceSilentDelete = true; }

	inline int		GetActiveIndex( void ) const { return m_activeIndex; }
	inline void		SetActiveIndex( int index ) { m_activeIndex = index; }
	inline float	GetBuoyancyRatio( void ) const { return m_buoyancyRatio; }
	// returns true if the mass center is set to the default for the collision model
	bool			IsMassCenterAtDefault() const;

	// is this object simulated, or controlled by game logic?
	bool			IsControlledByGame() const;

	IVP_SurfaceManager *GetSurfaceManager( void ) const;

	void			WriteToTemplate( vphysics_save_cphysicsobject_t &objectTemplate );
	void			InitFromTemplate( CPhysicsEnvironment *pEnvironment, void *pGameData, const vphysics_save_cphysicsobject_t &objectTemplate );

	CPhysicsEnvironment	*GetVPhysicsEnvironment();
	const CPhysicsEnvironment	*GetVPhysicsEnvironment() const;

private:
	// NOTE: Local to vphysics, used to save/restore shadow controller
	void			RestoreShadowController( IPhysicsShadowController *pShadowController );
	friend bool		RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject );

	bool			IsControlling( const IVP_Controller *pController ) const;
	float			GetVolume() const;
	void			SetVolume( float volume );
	
	// the mass has changed, recompute the drag information
	void			RecomputeDragBases();

	void			ClampVelocity();

	// NOTE: If m_pGameData is not the first member, the constructor debug code must be modified
	void			*m_pGameData;
	IVP_Real_Object	*m_pObject;
	const CPhysCollide *m_pCollide;
	IPhysicsShadowController	*m_pShadow;

	Vector			m_dragBasis;
	Vector			m_angDragBasis;

	// these 5 should pack into a short
	// pack new bools here
	bool			m_shadowTempGravityDisable : 5;
	bool			m_hasTouchedDynamic : 1;
	bool			m_asleepSinceCreation : 1;
	bool			m_forceSilentDelete : 1;
	unsigned char	m_sleepState : 2;
	unsigned char	m_hingedAxis : 3;
	unsigned char	m_collideType : 3;
	unsigned short	m_gameIndex;

private:
	unsigned short	m_materialIndex;
	unsigned short	m_activeIndex;

	unsigned short	m_callbacks;
	unsigned short	m_gameFlags;
	unsigned int	m_contentsMask;
	
	float			m_volume;
	float			m_buoyancyRatio;
	float			m_dragCoefficient;
	float			m_angDragCoefficient;

	friend CPhysicsObject *CreatePhysicsObject( CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle& angles, objectparams_t *pParams, bool isStatic );
	friend bool CPhysicsEnvironment::TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment ); //need direct access to m_pShadow for Portal mod's physics object transfer system
};

// If you haven't ever touched a dynamic object, there's no need to search for contacting objects to 
// wakeup when you are deleted.  So cache a bit here when contacts are generated
inline bool CPhysicsObject::HasTouchedDynamic()
{
	return m_hasTouchedDynamic;
}

inline void CPhysicsObject::SetTouchedDynamic()
{
	m_hasTouchedDynamic = true;
}

extern CPhysicsObject *CreatePhysicsObject( CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
extern CPhysicsObject *CreatePhysicsSphere( CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
extern void PostRestorePhysicsObject();
extern IPhysicsObject *CreateObjectFromBuffer( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions );
extern IPhysicsObject *CreateObjectFromBuffer_UseExistingMemory( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, CPhysicsObject *pExistingMemory );

#endif // PHYSICS_OBJECT_H
