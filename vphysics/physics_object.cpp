//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "ivp_surman_polygon.hxx"
#include "ivp_compact_ledge.hxx"
#include "ivp_compact_ledge_solver.hxx"
#include "ivp_mindist.hxx"
#include "ivp_mindist_intern.hxx"
#include "ivp_friction.hxx"
#include "ivp_phantom.hxx"
#include "ivp_listener_collision.hxx"
#include "ivp_clustering_visualizer.hxx"
#include "ivp_anomaly_manager.hxx"
#include "ivp_collision_filter.hxx"

#include "hk_mopp/ivp_surman_mopp.hxx"
#include "hk_mopp/ivp_compact_mopp.hxx"

#include "ivp_compact_surface.hxx"
#include "physics_trace.h"
#include "physics_shadow.h"
#include "physics_friction.h"
#include "physics_constraint.h"
#include "bspflags.h"
#include "vphysics/player_controller.h"
#include "vphysics/friction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IPhysicsCollision *physcollision;

// UNDONE: Make this a stack variable / member variable of some save/load object or function?
// NOTE: This keeps a list of objects who were saved while asleep, but not created asleep
// So some info will be lost unless it's regenerated after loading.
struct postrestore_objectlist_t
{
	CPhysicsObject	*pObject;
	bool			growFriction;
	bool			enableCollisions;

	void Defaults()
	{
		pObject = NULL;
		growFriction = false;
		enableCollisions = false;
	}
};

static CUtlVector<postrestore_objectlist_t> g_PostRestoreObjectList;

// This angular basis is the integral of each differential drag area's torque over the whole OBB
// For each axis, each face, the integral is (1/Iaxis) * (1/3 * w^2l^3 + 1/2 * w^4l + lw^2h^2)
// l,w, & h are half widths - where l is in the direction of the axis, w is in the plane (l/w plane) of the face, 
// and h is perpendicular to the face.  So for each axis, you sum up this integral over 2 pairs of faces 
// (this function returns the integral for one pair of opposite faces, not one face)
static float AngDragIntegral( float invInertia, float l, float w, float h )
{
	float w2 = w*w;
	float l2 = l*l;
	float h2 = h*h;

	return invInertia * ( (1.f/3.f)*w2*l*l2 + 0.5 * w2*w2*l + l*w2*h2 );
}


CPhysicsObject::CPhysicsObject( void )
{
#ifdef _WIN32
	void *pData = ((char *)this) + sizeof(void *); // offset beyond vtable
	int dataSize = sizeof(*this) - sizeof(void *);

	Assert( pData == &m_pGameData );

	memset( pData, 0, dataSize );
#elif POSIX

	//!!HACK HACK - rework this if we ever change compiler versions (from gcc 3.2!!!)
	void *pData = ((char *)this) + sizeof(void *); // offset beyond vtable
	int dataSize = sizeof(*this) - sizeof(void *);

	Assert( pData == &m_pGameData );

	memset( pData, 0, dataSize );	
#else
#error
#endif

	// HACKHACK: init this as a sphere until someone attaches a surfacemanager
	m_collideType = COLLIDE_BALL;
	m_contentsMask = CONTENTS_SOLID;
	m_hasTouchedDynamic = 0;
}

void CPhysicsObject::Init( const CPhysCollide *pCollisionModel, IVP_Real_Object *pObject, int materialIndex, float volume, float drag, float angDrag )
{
	m_pCollide = pCollisionModel;
	m_materialIndex = materialIndex;
	m_pObject = pObject;
	pObject->client_data = (void *)this;
	m_pGameData = NULL;
	m_gameFlags = 0;
	m_gameIndex = 0;
	m_sleepState = OBJ_SLEEP;		// objects start asleep
	m_callbacks = CALLBACK_GLOBAL_COLLISION|CALLBACK_GLOBAL_FRICTION|CALLBACK_FLUID_TOUCH|CALLBACK_GLOBAL_TOUCH|CALLBACK_GLOBAL_COLLIDE_STATIC|CALLBACK_DO_FLUID_SIMULATION;
	m_activeIndex = 0xFFFF;
	m_pShadow = NULL;
	m_shadowTempGravityDisable = false;
	m_forceSilentDelete = false;
	m_dragBasis = vec3_origin;
	m_angDragBasis = vec3_origin;

	if ( !IsStatic() && GetCollide() )
	{
		RecomputeDragBases();
	}
	else
	{
		drag = 0;
		angDrag = 0;
	}

	m_dragCoefficient = drag;
	m_angDragCoefficient = angDrag;

	SetVolume( volume );
}

CPhysicsObject::~CPhysicsObject( void )
{
	RemoveShadowController();

	if ( m_pObject )
	{
		// prevents callbacks to the game code / unlink from this object
		m_callbacks = 0;
		m_pGameData = 0;
		m_pObject->client_data = 0;

		IVP_Core *pCore = m_pObject->get_core();
		if ( pCore->physical_unmoveable == IVP_TRUE && pCore->controllers_of_core.n_elems )
		{
			// go ahead and notify them if this happens in the real world
			for(int i = pCore->controllers_of_core.len()-1; i >=0 ;i-- ) 
			{
				IVP_Controller *my_controller = pCore->controllers_of_core.element_at(i);
				my_controller->core_is_going_to_be_deleted_event(pCore);
				Assert(my_controller==pCore->environment->get_gravity_controller());
			}
		}

		// UNDONE: Don't free the surface manager here
		// UNDONE: Remove reference to it by calling something in physics_collide
		IVP_SurfaceManager *pSurman = GetSurfaceManager();
		CPhysicsEnvironment *pVEnv = GetVPhysicsEnvironment();

		// BUGBUG: Sometimes IVP will call a "revive" on the object we're deleting!
		MEM_ALLOC_CREDIT();
		if ( m_forceSilentDelete || (pVEnv && pVEnv->ShouldQuickDelete()) || !m_hasTouchedDynamic )
		{
			m_pObject->delete_silently();
		}
		else
		{
			m_pObject->delete_and_check_vicinity();
		}
		delete pSurman;
	}
}

void CPhysicsObject::Wake( void )
{
	m_pObject->ensure_in_simulation();
}

// supported
void CPhysicsObject::Sleep( void )
{
	m_pObject->disable_simulation();
}


bool CPhysicsObject::IsAsleep() const
{
	if ( m_sleepState == OBJ_AWAKE )
		return false;

	// double-check that we aren't pending
	if ( m_pObject->get_core()->is_in_wakeup_vec )
		return false;

	return true;
}

void CPhysicsObject::NotifySleep( void )
{
	if ( m_sleepState == OBJ_AWAKE )
	{
		m_sleepState = OBJ_STARTSLEEP;
	}
	else
	{
		// UNDONE: This fails sometimes and we get sleep calls for a sleeping object, debug?
		//Assert(m_sleepState==OBJ_STARTSLEEP);
		m_sleepState = OBJ_SLEEP;
	}
}


void CPhysicsObject::NotifyWake( void )
{
	m_asleepSinceCreation = false;
	m_sleepState = OBJ_AWAKE;
}


void CPhysicsObject::SetCallbackFlags( unsigned short callbackflags )
{
#if IVP_ENABLE_VISUALIZER
	unsigned short changedFlags = m_callbacks ^ callbackflags;
	if ( changedFlags & CALLBACK_MARKED_FOR_TEST )
	{
		if ( callbackflags & CALLBACK_MARKED_FOR_TEST )
		{
			ENABLE_SHORTRANGE_VISUALIZATION(m_pObject);
			ENABLE_LONGRANGE_VISUALIZATION(m_pObject);
		}
		else
		{
			DISABLE_SHORTRANGE_VISUALIZATION(m_pObject);
			DISABLE_LONGRANGE_VISUALIZATION(m_pObject);
		}
	}
#endif
	m_callbacks = callbackflags;

}


unsigned short CPhysicsObject::GetCallbackFlags() const
{
	return m_callbacks;
}


void CPhysicsObject::SetGameFlags( unsigned short userFlags )
{
	m_gameFlags = userFlags;
}

unsigned short CPhysicsObject::GetGameFlags() const
{
	return m_gameFlags;
}


void CPhysicsObject::SetGameIndex( unsigned short gameIndex )
{
	m_gameIndex = gameIndex;
}

unsigned short CPhysicsObject::GetGameIndex() const
{
	return m_gameIndex;
}

bool CPhysicsObject::IsStatic() const
{
	if ( m_pObject->get_core()->physical_unmoveable )
		return true;

	return false;
}


void CPhysicsObject::EnableCollisions( bool enable )
{
	if ( enable )
	{
		m_callbacks |= CALLBACK_ENABLING_COLLISION;
		BEGIN_IVP_ALLOCATION();
		m_pObject->enable_collision_detection( IVP_TRUE );
		END_IVP_ALLOCATION();
		m_callbacks &= ~CALLBACK_ENABLING_COLLISION;
	}
	else
	{
		if ( IsCollisionEnabled() )
		{
			// Delete all contact points with this physics object because it's collision is becoming disabled
			IPhysicsFrictionSnapshot *pSnapshot = CreateFrictionSnapshot();
			while ( pSnapshot->IsValid() )
			{
				pSnapshot->MarkContactForDelete();
				pSnapshot->NextFrictionData();
			}
			pSnapshot->DeleteAllMarkedContacts( true );
			DestroyFrictionSnapshot( pSnapshot );
		}

		m_pObject->enable_collision_detection( IVP_FALSE );
	}
}

void CPhysicsObject::RecheckCollisionFilter()
{
	if ( CallbackFlags() & CALLBACK_MARKED_FOR_DELETE )
		return;

	m_callbacks |= CALLBACK_ENABLING_COLLISION;
	BEGIN_IVP_ALLOCATION();
	m_pObject->recheck_collision_filter();
	// UNDONE: do a RecheckContactPoints() here?
	END_IVP_ALLOCATION();
	m_callbacks &= ~CALLBACK_ENABLING_COLLISION;
}

void CPhysicsObject::RecheckContactPoints()
{
	IVP_Environment *pEnv = m_pObject->get_environment();
	IVP_Collision_Filter *coll_filter = pEnv->get_collision_filter();
	IPhysicsFrictionSnapshot *pSnapshot = CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		CPhysicsObject *pOther = static_cast<CPhysicsObject *>(pSnapshot->GetObject(1));
		if ( !coll_filter->check_objects_for_collision_detection( m_pObject, pOther->m_pObject ) )
		{
			pSnapshot->MarkContactForDelete();
		}
		pSnapshot->NextFrictionData();
	}
	pSnapshot->DeleteAllMarkedContacts( true );
	DestroyFrictionSnapshot( pSnapshot );
}

CPhysicsEnvironment	*CPhysicsObject::GetVPhysicsEnvironment()
{
	return (CPhysicsEnvironment *) (m_pObject->get_environment()->client_data);
}

const CPhysicsEnvironment	*CPhysicsObject::GetVPhysicsEnvironment() const
{
	return (CPhysicsEnvironment *) (m_pObject->get_environment()->client_data);
}


bool CPhysicsObject::IsControlling( const IVP_Controller *pController ) const
{
	IVP_Core *pCore = m_pObject->get_core();
	for ( int i = 0; i < pCore->controllers_of_core.len(); i++ )
	{
		// already controlling this core?
		if ( pCore->controllers_of_core.element_at(i) == pController )
			return true;
	}

	return false;
}

bool CPhysicsObject::IsGravityEnabled() const
{
	if ( !IsStatic() )
	{
		return IsControlling( m_pObject->get_core()->environment->get_gravity_controller() );
	}

	return false;
}

bool CPhysicsObject::IsDragEnabled() const
{
	if ( !IsStatic() )
	{
		return IsControlling( GetVPhysicsEnvironment()->GetDragController() );
	}

	return false;
}


bool CPhysicsObject::IsMotionEnabled() const
{
	return m_pObject->get_core()->pinned ? false : true;
}


bool CPhysicsObject::IsMoveable() const
{
	if ( IsStatic() || !IsMotionEnabled() )
		return false;
	return true;
}


void CPhysicsObject::EnableGravity( bool enable )
{
	if ( IsStatic() )
		return;


	bool isEnabled = IsGravityEnabled();

	if ( enable == isEnabled )
		return;

	IVP_Controller *pGravity = m_pObject->get_core()->environment->get_gravity_controller();
	if ( enable )
	{
		m_pObject->get_core()->add_core_controller( pGravity );
	}
	else
	{
		m_pObject->get_core()->rem_core_controller( pGravity );
	}
}

void CPhysicsObject::EnableDrag( bool enable )
{
	if ( IsStatic() )
		return;

	bool isEnabled = IsDragEnabled();

	if ( enable == isEnabled )
		return;

	IVP_Controller *pDrag = GetVPhysicsEnvironment()->GetDragController();

	if ( enable )
	{
		m_pObject->get_core()->add_core_controller( pDrag );
	}
	else
	{
		m_pObject->get_core()->rem_core_controller( pDrag );
	}
}


void CPhysicsObject::SetDragCoefficient( float *pDrag, float *pAngularDrag )
{
	if ( pDrag )
	{
		m_dragCoefficient = *pDrag;
	}
	if ( pAngularDrag )
	{
		m_angDragCoefficient = *pAngularDrag;
	}

	EnableDrag( m_dragCoefficient || m_angDragCoefficient );
}


void CPhysicsObject::RecomputeDragBases()
{
	if ( IsStatic() || !GetCollide() )
		return;

	// Basically we are computing drag as an OBB.  Get OBB extents for projection
	// scale those extents by appropriate mass/inertia to compute velocity directly (not force)
	// in the controller
	// NOTE: Compute these even if drag coefficients are zero, because the drag coefficient could change later

	// Get an AABB for this object and use the area of each side as a basis for approximating cross-section area for drag
	Vector dragMins, dragMaxs;
	// NOTE: coordinates in/out of physcollision are in HL units, not IVP
	// PERFORMANCE: Cache this?  Expensive.
	physcollision->CollideGetAABB( &dragMins, &dragMaxs, GetCollide(), vec3_origin, vec3_angle );

	Vector areaFractions = physcollision->CollideGetOrthographicAreas( GetCollide() );
	Vector delta = dragMaxs - dragMins;
	ConvertPositionToIVP( delta.x, delta.y, delta.z );
	delta.x = fabsf(delta.x);
	delta.y = fabsf(delta.y);
	delta.z = fabsf(delta.z);
	// dragBasis is now the area of each side
	m_dragBasis.x = delta.y * delta.z * areaFractions.x;
	m_dragBasis.y = delta.x * delta.z * areaFractions.y;
	m_dragBasis.z = delta.x * delta.y * areaFractions.z;
	m_dragBasis *= GetInvMass();

	const IVP_U_Float_Point *pInvRI = m_pObject->get_core()->get_inv_rot_inertia();

	// This angular basis is the integral of each differential drag area's torque over the whole OBB
	// need half lengths for this integral
	delta *= 0.5;
	// rotation about the x axis
	m_angDragBasis.x = areaFractions.z * AngDragIntegral( pInvRI->k[0], delta.x, delta.y, delta.z ) + areaFractions.y * AngDragIntegral( pInvRI->k[0], delta.x, delta.z, delta.y );
	// rotation about the y axis
	m_angDragBasis.y = areaFractions.z * AngDragIntegral( pInvRI->k[1], delta.y, delta.x, delta.z ) + areaFractions.x * AngDragIntegral( pInvRI->k[1], delta.y, delta.z, delta.x );
	// rotation about the z axis
	m_angDragBasis.z = areaFractions.y * AngDragIntegral( pInvRI->k[2], delta.z, delta.x, delta.y ) + areaFractions.x * AngDragIntegral( pInvRI->k[2], delta.z, delta.y, delta.x );
}



void CPhysicsObject::EnableMotion( bool enable )
{
	if ( IsStatic() )
		return;

	bool isMoveable = IsMotionEnabled();

	// no change
	if ( isMoveable == enable )
		return;

	BEGIN_IVP_ALLOCATION();
	m_pObject->set_pinned( enable ? IVP_FALSE : IVP_TRUE );
	END_IVP_ALLOCATION();

	if ( enable && IsHinged() )
	{
		BecomeHinged( m_hingedAxis-1 );
	}
	RecheckCollisionFilter();
	RecheckContactPoints();
}

bool CPhysicsObject::IsControlledByGame() const
{
	if (m_pShadow && !m_pShadow->IsPhysicallyControlled())
		return true;

	if ( CallbackFlags() & CALLBACK_IS_PLAYER_CONTROLLER )
		return true;

	return false;
}

IPhysicsFrictionSnapshot *CPhysicsObject::CreateFrictionSnapshot()
{
	return ::CreateFrictionSnapshot( m_pObject );
}

void CPhysicsObject::DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot )
{
	::DestroyFrictionSnapshot(pSnapshot);
}

bool CPhysicsObject::IsMassCenterAtDefault() const
{
	// this is the actual mass center of the object as created
	Vector massCenterHL = GetMassCenterLocalSpace();

	// Get the default mass center to see if it has been changed
	IVP_U_Float_Point massCenterIVPDefault;
	Vector massCenterHLDefault;
	GetObject()->get_surface_manager()->get_mass_center( &massCenterIVPDefault );
	ConvertPositionToHL( massCenterIVPDefault, massCenterHLDefault );
	float delta = (massCenterHLDefault - massCenterHL).Length();

	return ( delta <= g_PhysicsUnits.collisionSweepIncrementalEpsilon ) ? true : false;
}

Vector CPhysicsObject::GetMassCenterLocalSpace() const
{
	if ( m_pObject->flags.shift_core_f_object_is_zero )
		return vec3_origin;

	Vector out;
	ConvertPositionToHL( *m_pObject->get_shift_core_f_object(), out );
	// core shift is what you add to the mass center to get the origin
	// so we want the negative core shift (origin relative position of the mass center)
	return -out;
}


void CPhysicsObject::SetGameData( void *pGameData )
{
	m_pGameData = pGameData;
}

void *CPhysicsObject::GetGameData( void ) const
{
	return m_pGameData;
}

void CPhysicsObject::SetMass( float mass )
{
	bool reset = false;

	if ( !IsMoveable() )
	{
		reset = true;
		EnableMotion(true);
	}

	Assert( mass > 0 );
	mass = clamp( mass, 0, VPHYSICS_MAX_MASS ); // NOTE: Allow zero procedurally, but not by initialization
	m_pObject->change_mass( mass );
	SetVolume( m_volume );
	RecomputeDragBases();
	if ( reset )
	{
		EnableMotion(false);
	}
}

float CPhysicsObject::GetMass( void ) const
{
	return m_pObject->get_core()->get_mass();
}

float CPhysicsObject::GetInvMass( void ) const
{
	return m_pObject->get_core()->get_inv_mass();
}

Vector CPhysicsObject::GetInertia( void ) const
{
	const IVP_U_Float_Point *pRI = m_pObject->get_core()->get_rot_inertia();

	Vector hlInertia;
	ConvertDirectionToHL( *pRI, hlInertia );
	VectorAbs( hlInertia, hlInertia );
	return hlInertia;
}

Vector CPhysicsObject::GetInvInertia( void ) const
{
	const IVP_U_Float_Point *pRI = m_pObject->get_core()->get_inv_rot_inertia();

	Vector hlInvInertia;
	ConvertDirectionToHL( *pRI, hlInvInertia );
	VectorAbs( hlInvInertia, hlInvInertia );
	return hlInvInertia;
}


void CPhysicsObject::SetInertia( const Vector &inertia )
{
	IVP_U_Float_Point ri;
	ConvertDirectionToIVP( inertia, ri );
	ri.k[0] = IVP_Inline_Math::fabsd(ri.k[0]);
	ri.k[1] = IVP_Inline_Math::fabsd(ri.k[1]);
	ri.k[2] = IVP_Inline_Math::fabsd(ri.k[2]);
	m_pObject->get_core()->set_rotation_inertia( &ri );
}


void CPhysicsObject::GetDamping( float *speed, float *rot ) const
{
	IVP_Core *pCore = m_pObject->get_core();
	if ( speed )
	{
		*speed = pCore->speed_damp_factor;
	}
	if ( rot )
	{
		*rot = pCore->rot_speed_damp_factor.k[0];
	}
}

void CPhysicsObject::SetDamping( const float *speed, const float *rot )
{
	IVP_Core *pCore = m_pObject->get_core();
	if ( speed )
	{
		pCore->speed_damp_factor = *speed;
	}
	if ( rot )
	{
		pCore->rot_speed_damp_factor.set( *rot, *rot, *rot );
	}
}

void CPhysicsObject::SetVolume( float volume )
{
	m_volume = volume;
	if ( volume != 0.f )
	{
		// minimum volume is 5 cubic inches - otherwise buoyancy can get unstable
		if ( volume < 5.0f )
		{
			volume = 5.0f;
		}
		volume *= HL2IVP_FACTOR*HL2IVP_FACTOR*HL2IVP_FACTOR;
		float density = GetMass() / volume;
		float matDensity;
		physprops->GetPhysicsProperties( GetMaterialIndexInternal(), &matDensity, NULL, NULL, NULL );
		m_buoyancyRatio = density / matDensity;
	}
	else
	{
		m_buoyancyRatio = 1.0f;
	}
}

float CPhysicsObject::GetVolume() const
{
	return m_volume;
}


void CPhysicsObject::SetBuoyancyRatio( float ratio )
{
	m_buoyancyRatio = ratio;
}

void CPhysicsObject::SetContents( unsigned int contents )
{
	m_contentsMask = contents;
}

// converts HL local units to HL world units
void CPhysicsObject::LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const
{
	matrix3x4_t matrix;
	GetPositionMatrix( &matrix );
	// copy in case the src == dest
	VectorTransform( Vector(localPosition), matrix, *worldPosition );
}

// Converts world HL units to HL local/object units
void CPhysicsObject::WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const
{
	matrix3x4_t matrix;
	GetPositionMatrix( &matrix );
	// copy in case the src == dest
	VectorITransform( Vector(worldPosition), matrix, *localPosition );
}

void CPhysicsObject::LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const
{
	matrix3x4_t matrix;
	GetPositionMatrix( &matrix );
	// copy in case the src == dest
	VectorRotate( Vector(localVector), matrix, *worldVector );
}

void CPhysicsObject::WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const
{
	matrix3x4_t matrix;
	GetPositionMatrix( &matrix );
	// copy in case the src == dest
	VectorIRotate( Vector(worldVector), matrix, *localVector );
}


// Apply force impulse (momentum) to the object
void CPhysicsObject::ApplyForceCenter( const Vector &forceVector )
{
	if ( !IsMoveable() )
		return;

	IVP_U_Float_Point tmp;

	ConvertForceImpulseToIVP( forceVector, tmp );
	IVP_Core *core = m_pObject->get_core();
	tmp.mult( core->get_inv_mass() );
	m_pObject->async_add_speed_object_ws( &tmp );
	ClampVelocity();
}

void CPhysicsObject::ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition )
{
	if ( !IsMoveable() )
		return;

	IVP_U_Point pos;
	IVP_U_Float_Point force;

	ConvertForceImpulseToIVP( forceVector, force );
	ConvertPositionToIVP( worldPosition, pos );

	IVP_Core *core = m_pObject->get_core();
	core->async_push_core_ws( &pos, &force );
	Wake();
	ClampVelocity();
}

void CPhysicsObject::CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const
{
	IVP_U_Point pos;
	IVP_U_Float_Point force;

	ConvertPositionToIVP( forceVector, force );
	ConvertPositionToIVP( worldPosition, pos );

	IVP_Core *core = m_pObject->get_core();

	const IVP_U_Matrix *m_world_f_core = core->get_m_world_f_core_PSI();

	IVP_U_Float_Point point_d_ws;
	point_d_ws.subtract(&pos, m_world_f_core->get_position());

	IVP_U_Float_Point cross_point_dir;

	cross_point_dir.calc_cross_product( &point_d_ws, &force);
	m_world_f_core->inline_vimult3( &cross_point_dir, &cross_point_dir);

	ConvertAngularImpulseToHL( cross_point_dir, *centerTorque );
	ConvertForceImpulseToHL( force, *centerForce );
}

void CPhysicsObject::CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const
{
	IVP_U_Point pos;
	IVP_U_Float_Point force;

	ConvertForceImpulseToIVP( forceVector, force );
	ConvertPositionToIVP( worldPosition, pos );

	IVP_Core *core = m_pObject->get_core();

	const IVP_U_Matrix *m_world_f_core = core->get_m_world_f_core_PSI();

	IVP_U_Float_Point point_d_ws;
	point_d_ws.subtract(&pos, m_world_f_core->get_position());

	IVP_U_Float_Point cross_point_dir;

	cross_point_dir.calc_cross_product( &point_d_ws, &force);
	m_world_f_core->inline_vimult3( &cross_point_dir, &cross_point_dir);

	cross_point_dir.set_pairwise_mult( &cross_point_dir, core->get_inv_rot_inertia());
	ConvertAngularImpulseToHL( cross_point_dir, *centerAngularVelocity );
	force.set_multiple( &force, core->get_inv_mass() );
	ConvertForceImpulseToHL( force, *centerVelocity );
}

void CPhysicsObject::ApplyTorqueCenter( const AngularImpulse &torqueImpulse )
{
	if ( !IsMoveable() )
		return;
	IVP_U_Float_Point ivpTorque;
	ConvertAngularImpulseToIVP( torqueImpulse, ivpTorque );
	IVP_Core *core = m_pObject->get_core();
	core->async_rot_push_core_multiple_ws( &ivpTorque, 1.0 );
	Wake();
	ClampVelocity();
}

void CPhysicsObject::GetPosition( Vector *worldPosition, QAngle *angles ) const
{
	IVP_U_Matrix matrix;
	m_pObject->get_m_world_f_object_AT( &matrix );
	if ( worldPosition )
	{
		ConvertPositionToHL( matrix.vv, *worldPosition );
	}

	if ( angles )
	{
		ConvertRotationToHL( matrix, *angles );
	}
}


void CPhysicsObject::GetPositionMatrix( matrix3x4_t *positionMatrix ) const
{
	IVP_U_Matrix matrix;
	m_pObject->get_m_world_f_object_AT( &matrix );
	ConvertMatrixToHL( matrix, *positionMatrix );
}


void CPhysicsObject::GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{
	if ( !velocity && !angularVelocity )
		return;

	IVP_Core *core = m_pObject->get_core();
	if ( velocity )
	{
		// just convert the cached dx
		ConvertPositionToHL( core->delta_world_f_core_psis, *velocity );
	}

	if ( angularVelocity )
	{
		// compute the relative transform that was actually integrated in the last psi
		IVP_U_Quat q_core_f_core;
		q_core_f_core.set_invert_mult( &core->q_world_f_core_last_psi, &core->q_world_f_core_next_psi);
		
		// now convert that to an axis/angle pair
		Quaternion q( q_core_f_core.x, q_core_f_core.y, q_core_f_core.z, q_core_f_core.w );
		AngularImpulse axis;
		float angle;
		QuaternionAxisAngle( q, axis, angle );

		// scale it by the timestep to get a velocity
		angle *= core->i_delta_time;

		// ConvertDirectionToHL() - convert this ipion direction (in HL type) to HL coords
		float tmpY = axis.z;
		angularVelocity->z = -axis.y;
		angularVelocity->y = tmpY;
		angularVelocity->x = axis.x;

		// now scale the axis by the angle to return the data in the correct format
		(*angularVelocity) *= angle;
	}
}

void CPhysicsObject::GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{
	if ( !velocity && !angularVelocity )
		return;

	IVP_Core *core = m_pObject->get_core();
	if ( velocity )
	{
		IVP_U_Float_Point speed;
		speed.add( &core->speed, &core->speed_change );
		ConvertPositionToHL( speed, *velocity );
	}

	if ( angularVelocity )
	{
		IVP_U_Float_Point rotSpeed;
		rotSpeed.add( &core->rot_speed, &core->rot_speed_change );
		// xform to HL space
		ConvertAngularImpulseToHL( rotSpeed, *angularVelocity );
	}
}

void CPhysicsObject::GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const
{
	IVP_Core *core = m_pObject->get_core();
	IVP_U_Point pos;
	ConvertPositionToIVP( worldPosition, pos );

	IVP_U_Float_Point rotSpeed;
	rotSpeed.add( &core->rot_speed, &core->rot_speed_change );

	IVP_U_Float_Point av_ws;
	core->get_m_world_f_core_PSI()->vmult3( &rotSpeed, &av_ws);

	IVP_U_Float_Point pos_rel; 	
	pos_rel.subtract( &pos, core->get_position_PSI());
	IVP_U_Float_Point cross;    
	cross.inline_calc_cross_product(&av_ws,&pos_rel);

	IVP_U_Float_Point speed;
	speed.add(&core->speed, &cross);
	speed.add(&core->speed_change);

	ConvertPositionToHL( speed, *pVelocity );
}


// UNDONE: Limit these?
void CPhysicsObject::AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	Assert(IsMoveable());
	if ( !IsMoveable() )
		return;
	IVP_Core *core = m_pObject->get_core();

	Wake();

	if ( velocity )
	{
		IVP_U_Float_Point ivpVelocity;
		ConvertPositionToIVP( *velocity, ivpVelocity );
		core->speed_change.add( &ivpVelocity );
	}

	if ( angularVelocity )
	{
		IVP_U_Float_Point ivpAngularVelocity;
		ConvertAngularImpulseToIVP( *angularVelocity, ivpAngularVelocity );

		core->rot_speed_change.add(&ivpAngularVelocity);
	}
	ClampVelocity();
}

void CPhysicsObject::SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport )
{
	IVP_U_Quat rot;
	IVP_U_Point pos;

	if ( m_pShadow )
	{
		UpdateShadow( worldPosition, angles, false, 0 );
	}
	ConvertPositionToIVP( worldPosition, pos );

	ConvertRotationToIVP( angles, rot );

	if ( m_pObject->is_collision_detection_enabled() && isTeleport )
	{
		EnableCollisions( false );
		m_pObject->beam_object_to_new_position( &rot, &pos, IVP_FALSE );
		EnableCollisions( true );
	}
	else
	{
		m_pObject->beam_object_to_new_position( &rot, &pos, IVP_FALSE );
	}
}

void CPhysicsObject::SetPositionMatrix( const matrix3x4_t& matrix, bool isTeleport )
{
	if ( m_pShadow )
	{
		Vector worldPosition;
		QAngle angles;
		MatrixAngles( matrix, angles );
		MatrixGetColumn( matrix, 3, worldPosition );
		UpdateShadow( worldPosition, angles, false, 0 );
	}

	IVP_U_Quat rot;
	IVP_U_Matrix mat;

	ConvertMatrixToIVP( matrix, mat );

	rot.set_quaternion( &mat );

	if ( m_pObject->is_collision_detection_enabled() && isTeleport )
	{
		EnableCollisions( false );
		m_pObject->beam_object_to_new_position( &rot, &mat.vv, IVP_FALSE );
		EnableCollisions( true );
	}
	else
	{
		m_pObject->beam_object_to_new_position( &rot, &mat.vv, IVP_FALSE );
	}
}

void CPhysicsObject::SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	Assert(IsMoveable());
	if ( !IsMoveable() )
		return;
	IVP_Core *core = m_pObject->get_core();

	Wake();

	if ( velocity )
	{
		ConvertPositionToIVP( *velocity, core->speed );
		core->speed_change.set_to_zero();
	}

	if ( angularVelocity )
	{
		ConvertAngularImpulseToIVP( *angularVelocity, core->rot_speed );
		core->rot_speed_change.set_to_zero();
	}
	ClampVelocity();
}

void CPhysicsObject::SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	if ( !IsMoveable() )
		return;
	IVP_Core *core = m_pObject->get_core();

	Wake();

	if ( velocity )
	{
		ConvertPositionToIVP( *velocity, core->speed_change );
		core->speed.set_to_zero();
	}

	if ( angularVelocity )
	{
		ConvertAngularImpulseToIVP( *angularVelocity, core->rot_speed_change );
		core->rot_speed.set_to_zero();
	}
	ClampVelocity();
}


void CPhysicsObject::ClampVelocity()
{
	if ( m_pShadow )
		return;

	m_pObject->get_core()->apply_velocity_limit();
}

void GetWorldCoordFromSynapse( IVP_Synapse_Friction *pfriction, IVP_U_Point &world )
{
	world.set(pfriction->get_contact_point()->get_contact_point_ws());
}


bool CPhysicsObject::GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const
{
	IVP_Synapse_Friction *pfriction = m_pObject->get_first_friction_synapse();
	if ( !pfriction )
		return false;

	if ( contactPoint )
	{
		IVP_U_Point world;
		GetWorldCoordFromSynapse( pfriction, world );
		ConvertPositionToHL( world, *contactPoint );
	}
	if ( contactObject )
	{
		IVP_Real_Object *pivp = GetOppositeSynapseObject( pfriction );
		*contactObject = static_cast<IPhysicsObject *>(pivp->client_data);
	}
	return true;
}

void CPhysicsObject::SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation )
{
	if ( m_pShadow )
	{
		m_pShadow->MaxSpeed( maxSpeed, maxAngularSpeed );
	}
	else
	{
		m_shadowTempGravityDisable = false;

		CPhysicsEnvironment *pVEnv = GetVPhysicsEnvironment();
		m_pShadow = pVEnv->CreateShadowController( this, allowPhysicsMovement, allowPhysicsRotation );
		m_pShadow->MaxSpeed( maxSpeed, maxAngularSpeed );
		// This really should be in the game code, but do this here because the game may (does) use 
		// shadow/AI control as a collision filter indicator.
		RecheckCollisionFilter();
	}
}

void CPhysicsObject::UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset )
{
	if ( tempDisableGravity != m_shadowTempGravityDisable )
	{
		m_shadowTempGravityDisable = tempDisableGravity;
		if ( !m_pShadow || m_pShadow->AllowsTranslation() )
		{
			EnableGravity( !m_shadowTempGravityDisable );
		}
	}
	if ( m_pShadow )
	{
		m_pShadow->Update( targetPosition, targetAngles, timeOffset );
	}
}


void CPhysicsObject::RemoveShadowController()
{
	if ( m_pShadow )
	{
		CPhysicsEnvironment *pVEnv = GetVPhysicsEnvironment();
		pVEnv->DestroyShadowController( m_pShadow );
		m_pShadow = NULL;
	}
}

// Back door to allow save/restore of backlink between shadow controller and physics object
void CPhysicsObject::RestoreShadowController( IPhysicsShadowController *pShadowController )
{
	Assert( !m_pShadow );
	m_pShadow = pShadowController;
}

int CPhysicsObject::GetShadowPosition( Vector *position, QAngle *angles ) const
{
	IVP_U_Matrix matrix;

	IVP_Environment *pEnv = m_pObject->get_environment();
	double psi = pEnv->get_next_PSI_time().get_seconds();
	m_pObject->calc_at_matrix( psi, &matrix );
	if ( angles )
	{
		ConvertRotationToHL( matrix, *angles );
	}
	if ( position )
	{
		ConvertPositionToHL( matrix.vv, *position );
	}

	return 1;
}


IPhysicsShadowController *CPhysicsObject::GetShadowController( void ) const
{
	return m_pShadow;
}

const CPhysCollide *CPhysicsObject::GetCollide( void ) const
{
	return m_pCollide;
}


IVP_SurfaceManager *CPhysicsObject::GetSurfaceManager( void ) const
{
	if ( m_collideType != COLLIDE_BALL )
	{
		return m_pObject->get_surface_manager();
	}
	return NULL;
}


float CPhysicsObject::GetDragInDirection( const IVP_U_Float_Point &velocity ) const
{
	IVP_U_Float_Point local;

	const IVP_U_Matrix *m_world_f_core = m_pObject->get_core()->get_m_world_f_core_PSI();
	m_world_f_core->vimult3( &velocity, &local );

	return m_dragCoefficient * IVP_Inline_Math::fabsd( local.k[0] * m_dragBasis.x ) + 
		IVP_Inline_Math::fabsd( local.k[1] * m_dragBasis.y ) + 
		IVP_Inline_Math::fabsd( local.k[2] * m_dragBasis.z );
}

float CPhysicsObject::GetAngularDragInDirection( const IVP_U_Float_Point &angVelocity ) const
{
	return m_angDragCoefficient * IVP_Inline_Math::fabsd( angVelocity.k[0] * m_angDragBasis.x ) + 
		IVP_Inline_Math::fabsd( angVelocity.k[1] * m_angDragBasis.y ) + 
		IVP_Inline_Math::fabsd( angVelocity.k[2] * m_angDragBasis.z );
}

const char *CPhysicsObject::GetName() const
{
	return m_pObject->get_name();
}

void CPhysicsObject::SetMaterialIndex( int materialIndex )
{
	if ( m_materialIndex == materialIndex )
		return;

	m_materialIndex = materialIndex;
	IVP_Material *pMaterial = physprops->GetIVPMaterial( materialIndex );
	Assert(pMaterial);
	m_pObject->l_default_material = pMaterial;
	m_callbacks |= CALLBACK_ENABLING_COLLISION;
	BEGIN_IVP_ALLOCATION();
	m_pObject->recompile_material_changed();
	END_IVP_ALLOCATION();
	m_callbacks &= ~CALLBACK_ENABLING_COLLISION;
	if ( GetShadowController() )
	{
		GetShadowController()->ObjectMaterialChanged( materialIndex );
	}
}

// convert square velocity magnitude from IVP to HL
float CPhysicsObject::GetEnergy() const
{
	IVP_Core *pCore = m_pObject->get_core();
	IVP_FLOAT energy = 0.0f;
	IVP_U_Float_Point tmp;

	energy = 0.5f * pCore->get_mass() * pCore->speed.dot_product(&pCore->speed); // 1/2mvv
	tmp.set_pairwise_mult(&pCore->rot_speed, pCore->get_rot_inertia()); // wI
	energy += 0.5f * tmp.dot_product(&pCore->rot_speed); // 1/2mvv + 1/2wIw

	return ConvertEnergyToHL( energy );
}

float CPhysicsObject::ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt )
{
	return ComputeShadowControllerHL( this, params, secondsToArrival, dt );
}

float CPhysicsObject::GetSphereRadius() const
{
	if ( m_collideType != COLLIDE_BALL )
		return 0;

	return ConvertDistanceToHL( m_pObject->to_ball()->get_radius() );
}

float CPhysicsObject::CalculateLinearDrag( const Vector &unitDirection ) const
{
	IVP_U_Float_Point ivpDir;
	ConvertDirectionToIVP( unitDirection, ivpDir );

	return GetDragInDirection( ivpDir );
}

float CPhysicsObject::CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const
{
	IVP_U_Float_Point ivpAxis;
	ConvertDirectionToIVP( objectSpaceRotationAxis, ivpAxis );

	// drag factor is per-radian, convert to per-degree
	return GetAngularDragInDirection( ivpAxis ) * DEG2RAD(1.0);
}


void CPhysicsObject::BecomeTrigger()
{
	if ( IsTrigger() )
		return;

	if ( GetShadowController() )
	{
		// triggers won't have the standard collisions, so the material change is no longer necessary
		// also: This will fix problems with surfaceprops if the trigger becomes a fluid.
		GetShadowController()->UseShadowMaterial( false );
	}
	EnableDrag( false );
	EnableGravity( false );

	// UNDONE: Use defaults here?  Do we want object sets by default?
	IVP_Template_Phantom trigger;
	trigger.manage_intruding_cores = IVP_TRUE; // manage a list of intruded objects
	trigger.manage_sleeping_cores = IVP_TRUE; // don't untouch/touch on sleep/wake
	trigger.dont_check_for_unmoveables = IVP_TRUE;
	trigger.exit_policy_extra_radius = 0.1f; // relatively strict exit check [m]

	bool enableCollisions = IsCollisionEnabled();
	EnableCollisions( false );
	BEGIN_IVP_ALLOCATION();
	m_pObject->convert_to_phantom( &trigger );
	END_IVP_ALLOCATION();
	// hook up events
	CPhysicsEnvironment *pVEnv = GetVPhysicsEnvironment();
	pVEnv->PhantomAdd( this );


	EnableCollisions( enableCollisions );
}


void CPhysicsObject::RemoveTrigger()
{
	IVP_Controller_Phantom *pController = m_pObject->get_controller_phantom();

	// NOTE: This will remove the back-link in the object
	delete pController;
}


bool CPhysicsObject::IsTrigger() const
{
	return m_pObject->get_controller_phantom() != NULL ? true : false;
}

bool CPhysicsObject::IsFluid() const
{
	IVP_Controller_Phantom *pController = m_pObject->get_controller_phantom();
	if ( pController )
	{
		// UNDONE: Make a base class for triggers?  IPhysicsTrigger?
		//	and derive fluids and any other triggers from that class
		//	then you can ask that class what to do here.
		if ( pController->client_data )
			return true;
	}

	return false;
}

// sets the object to be hinged.  Fixed it place, but able to rotate around one axis.
void CPhysicsObject::BecomeHinged( int localAxis )
{
	if ( IsMoveable() )
	{
		float savedMass = GetMass();

		IVP_U_Float_Hesse *iri = (IVP_U_Float_Hesse *)m_pObject->get_core()->get_inv_rot_inertia();

		float savedRI[3];
		for ( int i = 0; i < 3; i++ )
			savedRI[i] = iri->k[i];

		SetMass( VPHYSICS_MAX_MASS );
		IVP_U_Float_Hesse tmp = *iri;
#if 0
		for ( i = 0; i < 3; i++ )
			tmp.k[i] = savedRI[i];
#else
		int localAxisIVP = ConvertCoordinateAxisToIVP(localAxis);
		tmp.k[localAxisIVP] = savedRI[localAxisIVP];
#endif

		SetMass( savedMass );
		*iri = tmp;
	}
	m_hingedAxis = localAxis+1;
}

void CPhysicsObject::RemoveHinged()
{
	m_hingedAxis = 0;
	m_pObject->get_core()->calc_calc();
}

// dumps info about the object to Msg()
void CPhysicsObject::OutputDebugInfo() const
{
	Msg("-----------------\nObject: %s\n", m_pObject->get_name());
	Msg("Mass: %.1f (inv %.3f)\n", GetMass(), GetInvMass() );
	Vector inertia = GetInertia();
	Vector invInertia = GetInvInertia();
	Msg("Inertia: %.2f, %.2f, %.2f (inv %.3f, %.3f, %.3f)\n", inertia.x, inertia.y, inertia.z, invInertia.x, invInertia.y, invInertia.z );

	Vector speed, angSpeed;
	GetVelocity( &speed, &angSpeed );
	Msg("Velocity: %.2f, %.2f, %.2f \n", speed.x, speed.y, speed.z );
	Msg("Ang Velocity: %.2f, %.2f, %.2f \n", angSpeed.x, angSpeed.y, angSpeed.z );

	float damp, angDamp;
	GetDamping( &damp, &angDamp );
	Msg("Damping %.2f linear, %.2f angular\n", damp, angDamp );

	Msg("Linear Drag: %.2f, %.2f, %.2f (factor %.2f)\n", m_dragBasis.x, m_dragBasis.y, m_dragBasis.z, m_dragCoefficient );
	Msg("Angular Drag: %.2f, %.2f, %.2f (factor %.2f)\n", m_angDragBasis.x, m_angDragBasis.y, m_angDragBasis.z, m_angDragCoefficient );

	if ( IsHinged() )
	{
		const char *pAxisNames[] = {"x", "y", "z"};
		Msg("Hinged on %s axis\n", pAxisNames[m_hingedAxis-1] );
	}
	Msg("attached to %d controllers\n", m_pObject->get_core()->controllers_of_core.len() );
	for (int k = m_pObject->get_core()->controllers_of_core.len()-1; k>=0;k--)
	{
		// NOTE: Set a breakpoint here and take a look at what it's hooked to
		IVP_Controller *pController = m_pObject->get_core()->controllers_of_core.element_at(k);
		Msg("%d) %s\n", k, pController->get_controller_name() );
	}
	Msg("State: %s, Collision %s, Motion %s, %sFlags %04X (game %04x, index %d)\n", 
		IsAsleep() ? "Asleep" : "Awake",
		IsCollisionEnabled() ? "Enabled" : "Disabled",
		IsStatic() ? "Static" : (IsMotionEnabled() ? "Enabled" : "Disabled"),
		(GetCallbackFlags() & CALLBACK_MARKED_FOR_TEST) ? "Debug! " : "",
		(int)GetCallbackFlags(), (int)GetGameFlags(), (int)GetGameIndex() );

	float matDensity = 0;
	float matThickness = 0;
	float matFriction = 0;
	float matElasticity = 0;
	physprops->GetPhysicsProperties( GetMaterialIndexInternal(), &matDensity, &matThickness, &matFriction, &matElasticity );
	Msg("Material: %s : density(%.1f), thickness(%.2f), friction(%.2f), elasticity(%.2f)\n", physprops->GetPropName(GetMaterialIndexInternal()),
		matDensity, matThickness, matFriction, matElasticity );
	if ( GetCollide() )
	{
		OutputCollideDebugInfo( GetCollide() );
	}
}

bool CPhysicsObject::IsAttachedToConstraint( bool bExternalOnly ) const
{
	if ( m_pObject )
	{
		for (int k = m_pObject->get_core()->controllers_of_core.len()-1; k>=0;k--)
		{
			IVP_Controller *pController = m_pObject->get_core()->controllers_of_core.element_at(k);
			if ( pController->get_controller_priority() == IVP_CP_CONSTRAINTS )
			{
				if ( !bExternalOnly || IsExternalConstraint(pController, GetGameData()) )
					return true;
			}
		}
	}
	return false;
}

static void InitObjectTemplate( IVP_Template_Real_Object &objectTemplate, int materialIndex, objectparams_t *pParams, bool isStatic )
{
	objectTemplate.mass = pParams->mass;
	objectTemplate.mass = clamp( objectTemplate.mass, VPHYSICS_MIN_MASS, VPHYSICS_MAX_MASS );

	if ( materialIndex >= 0 )
	{
		objectTemplate.material = physprops->GetIVPMaterial( materialIndex );
	}
	else
	{
		materialIndex = physprops->GetSurfaceIndex( "default" );
		objectTemplate.material = physprops->GetIVPMaterial( materialIndex );
	}

	// HACKHACK: Do something with this name?
	BEGIN_IVP_ALLOCATION();
	if ( IsPC() )
	{
		objectTemplate.set_name(pParams->pName);
	}
	END_IVP_ALLOCATION();
#if USE_COLLISION_GROUP_STRING
	objectTemplate.set_nocoll_group_ident( NULL );
#endif

	objectTemplate.physical_unmoveable = isStatic ? IVP_TRUE : IVP_FALSE;
	objectTemplate.rot_inertia_is_factor = IVP_TRUE;

	float inertia = pParams->inertia;

	// don't allow <=0 inertia!!!!
	if ( inertia <= 0 )
		inertia = 1.0;

	if ( inertia > 1e18f )
		inertia = 1e18f;

	objectTemplate.rot_inertia.set(inertia, inertia, inertia);
	objectTemplate.rot_speed_damp_factor.set(pParams->rotdamping, pParams->rotdamping, pParams->rotdamping);
	objectTemplate.speed_damp_factor = pParams->damping;
	objectTemplate.auto_check_rot_inertia = pParams->rotInertiaLimit;
}

CPhysicsObject *CreatePhysicsObject( CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle& angles, objectparams_t *pParams, bool isStatic )
{
	if ( materialIndex < 0 )
	{
		materialIndex = physprops->GetSurfaceIndex( "default" );
	}
	AssertOnce(materialIndex>=0 && materialIndex<127);
	IVP_Template_Real_Object objectTemplate;
	IVP_U_Quat rotation;
	IVP_U_Point pos;

	Assert( position.IsValid() );
	Assert( angles.IsValid() );

#if _WIN32
	if ( !position.IsValid() || !angles.IsValid() )
	{
		DebuggerBreakIfDebugging();
		Warning("Invalid initial position on %s\n", pParams->pName );

		Vector *pPos = (Vector *)&position;
		QAngle *pRot = (QAngle *)&angles;
		if ( !pPos->IsValid() )
			pPos->Init();
		if ( !pRot->IsValid() )
			pRot->Init();
	}
#endif

	ConvertRotationToIVP( angles, rotation );
	ConvertPositionToIVP( position, pos );

	InitObjectTemplate( objectTemplate, materialIndex, pParams, isStatic );

	IVP_U_Matrix massCenterMatrix;
	massCenterMatrix.init();
	if ( pParams->massCenterOverride )
	{
		IVP_U_Point center;
		ConvertPositionToIVP( *pParams->massCenterOverride, center );
		massCenterMatrix.shift_os( &center );
		objectTemplate.mass_center_override = &massCenterMatrix;
	}

	CPhysicsObject *pObject = new CPhysicsObject();
	short collideType;
	IVP_SurfaceManager *pSurman = CreateSurfaceManager( pCollisionModel, collideType );
	if ( !pSurman )
		return NULL;
	pObject->m_collideType = collideType;
	pObject->m_asleepSinceCreation = true;

	BEGIN_IVP_ALLOCATION();

	IVP_Polygon *realObject = pEnvironment->GetIVPEnvironment()->create_polygon(pSurman, &objectTemplate, &rotation, &pos);

	pObject->Init( pCollisionModel, realObject, materialIndex, pParams->volume, pParams->dragCoefficient, pParams->dragCoefficient );
	pObject->SetGameData( pParams->pGameData );

	if ( pParams->enableCollisions )
	{
		pObject->EnableCollisions( true );
	}
	if ( !isStatic && pParams->dragCoefficient != 0.0f )
	{
		pObject->EnableDrag( true );
	}

	END_IVP_ALLOCATION();

	return pObject;
}

CPhysicsObject *CreatePhysicsSphere( CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic )
{
	IVP_U_Quat rotation;
	IVP_U_Point pos;

	ConvertRotationToIVP( angles, rotation );
	ConvertPositionToIVP( position, pos );

	IVP_Template_Real_Object objectTemplate;
	InitObjectTemplate( objectTemplate, materialIndex, pParams, isStatic );

	IVP_Template_Ball ballTemplate;
	ballTemplate.radius = ConvertDistanceToIVP( radius );

	MEM_ALLOC_CREDIT();
	IVP_Ball *realObject = pEnvironment->GetIVPEnvironment()->create_ball( &ballTemplate, &objectTemplate, &rotation, &pos );

	float volume = pParams->volume;
	if ( volume <= 0 )
	{
		volume = 4.0f * radius * radius * radius * M_PI / 3.0f;
	}
	CPhysicsObject *pObject = new CPhysicsObject();
	pObject->Init( NULL, realObject, materialIndex, volume, 0, 0 ); //, pParams->dragCoefficient, pParams->dragCoefficient
	pObject->SetGameData( pParams->pGameData );

	if ( pParams->enableCollisions )
	{
		pObject->EnableCollisions( true );
	}
	// drag is not supported on spheres
	//pObject->EnableDrag( false );

	return pObject;
}

class CMaterialIndexOps : public CDefSaveRestoreOps
{
public:
	// save data type interface
	virtual void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) 
	{
		int materialIndex = *((int *)fieldInfo.pField);
		const char *pMaterialName = physprops->GetPropName( materialIndex );
		if ( !pMaterialName )
		{
			pMaterialName = physprops->GetPropName( 0 );
		}
		int len = strlen(pMaterialName) + 1;
		pSave->WriteInt( &len );
		pSave->WriteString( pMaterialName );
	}

	virtual void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) 
	{
		char nameBuf[1024];
		int nameLen = pRestore->ReadInt();
		pRestore->ReadString( nameBuf, sizeof(nameBuf), nameLen );
		int *pMaterialIndex = (int *)fieldInfo.pField;
		*pMaterialIndex = physprops->GetSurfaceIndex( nameBuf );
		if ( *pMaterialIndex < 0 )
		{
			*pMaterialIndex = 0;
		}
	}

	virtual bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) 
	{ 
		int *pMaterialIndex = (int *)fieldInfo.pField;
		return (*pMaterialIndex == 0);
	}

	virtual void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo ) 
	{
		int *pMaterialIndex = (int *)fieldInfo.pField;
		*pMaterialIndex = 0;
	}
};

static CMaterialIndexOps g_MaterialIndexDataOps;

ISaveRestoreOps* MaterialIndexDataOps()
{
	return &g_MaterialIndexDataOps;
}

BEGIN_SIMPLE_DATADESC( vphysics_save_cphysicsobject_t )
//	DEFINE_FIELD( pCollide,			FIELD_???	),	// don't save this
//	DEFINE_FIELD( pName,				FIELD_???	),	// don't save this
DEFINE_FIELD( sphereRadius,		FIELD_FLOAT	),
DEFINE_FIELD( isStatic,			FIELD_BOOLEAN	),
DEFINE_FIELD( collisionEnabled,	FIELD_BOOLEAN	),
DEFINE_FIELD( gravityEnabled,		FIELD_BOOLEAN	),
DEFINE_FIELD( dragEnabled,		FIELD_BOOLEAN	),
DEFINE_FIELD( motionEnabled,		FIELD_BOOLEAN	),
DEFINE_FIELD( isAsleep,			FIELD_BOOLEAN	),
DEFINE_FIELD( isTrigger,			FIELD_BOOLEAN	),
DEFINE_FIELD( asleepSinceCreation,	FIELD_BOOLEAN	),
DEFINE_FIELD( hasTouchedDynamic,	FIELD_BOOLEAN	),
DEFINE_CUSTOM_FIELD( materialIndex, &g_MaterialIndexDataOps ),
DEFINE_FIELD( mass,				FIELD_FLOAT	),
DEFINE_FIELD( rotInertia,			FIELD_VECTOR ),
DEFINE_FIELD( speedDamping,		FIELD_FLOAT	),
DEFINE_FIELD( rotSpeedDamping,	FIELD_FLOAT	),
DEFINE_FIELD( massCenterOverride,	FIELD_VECTOR ),
DEFINE_FIELD( callbacks,			FIELD_INTEGER	),
DEFINE_FIELD( gameFlags,			FIELD_INTEGER	),
DEFINE_FIELD( contentsMask,		FIELD_INTEGER	),
DEFINE_FIELD( volume,				FIELD_FLOAT	),
DEFINE_FIELD( dragCoefficient,	FIELD_FLOAT	),
DEFINE_FIELD( angDragCoefficient,	FIELD_FLOAT	),
DEFINE_FIELD( hasShadowController,FIELD_BOOLEAN	),
//DEFINE_VPHYSPTR( pShadow ),
DEFINE_FIELD( origin,				FIELD_POSITION_VECTOR	),
DEFINE_FIELD( angles,				FIELD_VECTOR	),
DEFINE_FIELD( velocity,			FIELD_VECTOR	),
DEFINE_FIELD( angVelocity,		FIELD_VECTOR	),
DEFINE_FIELD( collideType,		FIELD_SHORT	),
DEFINE_FIELD( gameIndex,		FIELD_SHORT ),
DEFINE_FIELD( hingeAxis,		FIELD_INTEGER	),
END_DATADESC()

bool CPhysicsObject::IsCollisionEnabled() const
{
	return GetObject()->is_collision_detection_enabled() ? true : false;
}

void CPhysicsObject::WriteToTemplate( vphysics_save_cphysicsobject_t &objectTemplate )
{
	if ( m_collideType == COLLIDE_BALL )
	{
		objectTemplate.pCollide = NULL;
		objectTemplate.sphereRadius = GetSphereRadius();
	}
	else
	{
		objectTemplate.pCollide = GetCollide();
		objectTemplate.sphereRadius = 0;
	}
	objectTemplate.isStatic = IsStatic();
	objectTemplate.collisionEnabled = IsCollisionEnabled();
	objectTemplate.gravityEnabled = IsGravityEnabled();
	objectTemplate.dragEnabled = IsDragEnabled();
	objectTemplate.motionEnabled = IsMotionEnabled();
	objectTemplate.isAsleep = IsAsleep();
	objectTemplate.isTrigger = IsTrigger();
	objectTemplate.asleepSinceCreation = m_asleepSinceCreation;
	objectTemplate.materialIndex = m_materialIndex;
	objectTemplate.mass = GetMass();

	objectTemplate.rotInertia = GetInertia();
	GetDamping( &objectTemplate.speedDamping, &objectTemplate.rotSpeedDamping );
	objectTemplate.massCenterOverride.Init();
	if ( !IsMassCenterAtDefault() )
	{
		objectTemplate.massCenterOverride = GetMassCenterLocalSpace();
	}

	objectTemplate.callbacks = m_callbacks;
	objectTemplate.gameFlags = m_gameFlags;
	objectTemplate.volume = GetVolume();
	objectTemplate.dragCoefficient = m_dragCoefficient;
	objectTemplate.angDragCoefficient = m_angDragCoefficient;
	objectTemplate.pShadow = m_pShadow;
	objectTemplate.hasShadowController = (m_pShadow != NULL) ? true : false;
	objectTemplate.hasTouchedDynamic = HasTouchedDynamic();
	//bool			m_shadowTempGravityDisable;
	objectTemplate.collideType = m_collideType;
	objectTemplate.gameIndex = m_gameIndex;
	objectTemplate.contentsMask = m_contentsMask;
	objectTemplate.hingeAxis = m_hingedAxis;
	GetPosition( &objectTemplate.origin, &objectTemplate.angles );
	GetVelocity( &objectTemplate.velocity, &objectTemplate.angVelocity );
}

void CPhysicsObject::InitFromTemplate( CPhysicsEnvironment *pEnvironment, void *pGameData, const vphysics_save_cphysicsobject_t &objectTemplate )
{
	MEM_ALLOC_CREDIT();
	m_collideType = objectTemplate.collideType;

	IVP_Template_Real_Object ivpObjectTemplate;
	IVP_U_Quat rotation;
	IVP_U_Point pos;

	ConvertRotationToIVP( objectTemplate.angles, rotation );
	ConvertPositionToIVP( objectTemplate.origin, pos );

	ivpObjectTemplate.mass = objectTemplate.mass;

	if ( objectTemplate.materialIndex >= 0 )
	{
		ivpObjectTemplate.material = physprops->GetIVPMaterial( objectTemplate.materialIndex );
	}
	else
	{
		ivpObjectTemplate.material = physprops->GetIVPMaterial( physprops->GetSurfaceIndex( "default" ) );
	}

	Assert( ivpObjectTemplate.material );
	// HACKHACK: Pass this name in for debug
	ivpObjectTemplate.set_name(objectTemplate.pName);
#if USE_COLLISION_GROUP_STRING
	ivpObjectTemplate.set_nocoll_group_ident( NULL );
#endif

	ivpObjectTemplate.physical_unmoveable = objectTemplate.isStatic ? IVP_TRUE : IVP_FALSE;
	ivpObjectTemplate.rot_inertia_is_factor = IVP_TRUE;

	ivpObjectTemplate.rot_inertia.set( 1,1,1 );
	ivpObjectTemplate.rot_speed_damp_factor.set( objectTemplate.rotSpeedDamping, objectTemplate.rotSpeedDamping, objectTemplate.rotSpeedDamping );
	ivpObjectTemplate.speed_damp_factor = objectTemplate.speedDamping;

	IVP_U_Matrix massCenterMatrix;
	massCenterMatrix.init();
	if ( objectTemplate.massCenterOverride != vec3_origin )
	{
		IVP_U_Point center;
		ConvertPositionToIVP( objectTemplate.massCenterOverride, center );
		massCenterMatrix.shift_os( &center );
		ivpObjectTemplate.mass_center_override = &massCenterMatrix;
	}

	IVP_Real_Object *realObject = NULL;
	if ( m_collideType == COLLIDE_BALL )
	{
		IVP_Template_Ball ballTemplate;
		ballTemplate.radius = ConvertDistanceToIVP( objectTemplate.sphereRadius );

		realObject = pEnvironment->GetIVPEnvironment()->create_ball( &ballTemplate, &ivpObjectTemplate, &rotation, &pos );
	}
	else
	{
		short collideType;
		IVP_SurfaceManager *surman = CreateSurfaceManager( objectTemplate.pCollide, collideType );
		m_collideType = collideType;
		realObject = pEnvironment->GetIVPEnvironment()->create_polygon(surman, &ivpObjectTemplate, &rotation, &pos);
	}

	m_pObject = realObject;
	SetInertia( objectTemplate.rotInertia );
	Init( objectTemplate.pCollide, realObject, objectTemplate.materialIndex, objectTemplate.volume, objectTemplate.dragCoefficient, objectTemplate.dragCoefficient );

	SetCallbackFlags( (unsigned short) objectTemplate.callbacks );
	SetGameFlags( (unsigned short) objectTemplate.gameFlags );
	SetGameIndex( objectTemplate.gameIndex );
	SetGameData( pGameData );
	SetContents( objectTemplate.contentsMask );

	if ( objectTemplate.dragEnabled )
	{
		Assert( !objectTemplate.isStatic );
		EnableDrag( true );
	}

	if ( !objectTemplate.motionEnabled )
	{
		Assert( !objectTemplate.isStatic );
		EnableMotion( false );
	}

	if ( objectTemplate.isTrigger )
	{
		BecomeTrigger();
	}

	if ( !objectTemplate.gravityEnabled )
	{
		EnableGravity( false );
	}

	if ( objectTemplate.collisionEnabled )
	{
		EnableCollisions( true );
	}

	// will wake up the object
	if ( objectTemplate.velocity.LengthSqr() != 0 || objectTemplate.angVelocity.LengthSqr() != 0 )
	{
		SetVelocityInstantaneous( &objectTemplate.velocity, &objectTemplate.angVelocity );
		if ( objectTemplate.isAsleep )
		{
			Sleep();
		}
	}

	m_asleepSinceCreation = objectTemplate.asleepSinceCreation;
	if ( !objectTemplate.isAsleep )
	{
		Assert( !objectTemplate.isStatic );
		Wake();
	}

	if ( objectTemplate.hingeAxis )
	{
		BecomeHinged( objectTemplate.hingeAxis-1 );
	}
	if ( objectTemplate.hasTouchedDynamic )
	{
		SetTouchedDynamic();
	}

	m_pShadow = NULL;
}


bool SavePhysicsObject( const physsaveparams_t &params, CPhysicsObject *pObject )
{
	vphysics_save_cphysicsobject_t objectTemplate;
	memset( &objectTemplate, 0, sizeof(objectTemplate) );

	pObject->WriteToTemplate( objectTemplate );
	params.pSave->WriteAll( &objectTemplate );

	if ( objectTemplate.hasShadowController )
	{
		return SavePhysicsShadowController( params, objectTemplate.pShadow );
	}
	return true;
}

bool RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject )
{
	vphysics_save_cphysicsobject_t objectTemplate;
	memset( &objectTemplate, 0, sizeof(objectTemplate) );
	params.pRestore->ReadAll( &objectTemplate );
	Assert(objectTemplate.origin.IsValid());
	Assert(objectTemplate.angles.IsValid());
	objectTemplate.pCollide = params.pCollisionModel;
	objectTemplate.pName = params.pName;
	*ppObject = new CPhysicsObject();

	postrestore_objectlist_t entry;
	entry.Defaults();

	if ( objectTemplate.collisionEnabled )
	{
		// queue up the collision enable for these in case their entities have other dependent
		// physics handlers (like controllers) that need to be restored before callbacks are useful
		entry.pObject = *ppObject;
		entry.enableCollisions = true;
		objectTemplate.collisionEnabled = false;
	}

	(*ppObject)->InitFromTemplate( static_cast<CPhysicsEnvironment *>(params.pEnvironment), params.pGameData, objectTemplate );

	if ( (*ppObject)->IsAsleep() && !(*ppObject)->m_asleepSinceCreation && !(*ppObject)->IsStatic() )
	{
		entry.pObject = *ppObject;
		entry.growFriction = true;
	}

	if ( entry.pObject )
	{
		g_PostRestoreObjectList.AddToTail( entry );
	}

	if ( objectTemplate.hasShadowController )
	{
		bool restored = RestorePhysicsShadowControllerInternal( params, &objectTemplate.pShadow, *ppObject );
		(*ppObject)->RestoreShadowController( objectTemplate.pShadow );
		return restored;
	}

	return true;
}

IPhysicsObject *CreateObjectFromBuffer( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions )
{
	CPhysicsObject *pObject = new CPhysicsObject();
	if ( bufferSize >= sizeof(vphysics_save_cphysicsobject_t))
	{
		vphysics_save_cphysicsobject_t *pTemplate = reinterpret_cast<vphysics_save_cphysicsobject_t *>(pBuffer);
		pTemplate->hasShadowController = false; // this hasn't been saved separately so cannot be supported via this path
		pObject->InitFromTemplate( pEnvironment, pGameData, *pTemplate );
		if ( pTemplate->collisionEnabled && enableCollisions )
		{
			pObject->EnableCollisions(true);
		}
		return pObject;
	}
	return NULL;
}

IPhysicsObject *CreateObjectFromBuffer_UseExistingMemory( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, CPhysicsObject *pExistingMemory )
{
	if ( bufferSize >= sizeof(vphysics_save_cphysicsobject_t))
	{
		vphysics_save_cphysicsobject_t *pTemplate = reinterpret_cast<vphysics_save_cphysicsobject_t *>(pBuffer);
		// Allow the placement new. If we don't do this, then it'll get a compile error because new
		// might be defined as the special form in MEMALL_DEBUG_NEW.
		#include "tier0/memdbgoff.h"
		pExistingMemory = new ( pExistingMemory ) CPhysicsObject();
		#include "tier0/memdbgon.h"
		pExistingMemory->InitFromTemplate( pEnvironment, pGameData, *pTemplate );
		if ( pTemplate->collisionEnabled )
		{
			pExistingMemory->EnableCollisions(true);
		}
		return pExistingMemory;
	}
	return NULL;
}

// regenerate the friction systems for these objects.  Because when it was saved it had them (came to rest with the contact points).
// So now we need to recreate them or some objects may not wake up when this object (or its neighbors) are deleted.
void PostRestorePhysicsObject()
{
	for ( int i = g_PostRestoreObjectList.Count()-1; i >= 0; --i )
	{
		if ( g_PostRestoreObjectList[i].pObject )
		{
			if ( g_PostRestoreObjectList[i].growFriction )
			{
				g_PostRestoreObjectList[i].pObject->GetObject()->force_grow_friction_system();
			}
			if ( g_PostRestoreObjectList[i].enableCollisions )
			{
				g_PostRestoreObjectList[i].pObject->EnableCollisions( true );
			}
		}
	}
	g_PostRestoreObjectList.Purge();
}
