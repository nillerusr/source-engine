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
//	pObject->client_data = (void *)this;
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
	//m_pObject->ensure_in_simulation();
}

void CPhysicsObject::WakeNow( void )
{
	//m_pObject->ensure_in_simulation_now();
}

// supported
void CPhysicsObject::Sleep( void )
{
	//m_pObject->disable_simulation();
}


bool CPhysicsObject::IsAsleep() const
{
	if ( m_sleepState == OBJ_AWAKE )
		return false;

	// double-check that we aren't pending
	//if ( m_pObject->get_core()->is_in_wakeup_vec )
	//	return false;

	return true;
}

void CPhysicsObject::NotifySleep( void )
{
//	if ( m_sleepState == OBJ_AWAKE )
//	{
//		m_sleepState = OBJ_STARTSLEEP;
//	}
//	else
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
	return true;
}


void CPhysicsObject::EnableCollisions( bool enable )
{
}

void CPhysicsObject::RecheckCollisionFilter()
{
	if ( CallbackFlags() & CALLBACK_MARKED_FOR_DELETE )
		return;

}

void CPhysicsObject::RecheckContactPoints()
{
}

CPhysicsEnvironment	*CPhysicsObject::GetVPhysicsEnvironment()
{
	return NULL; //(CPhysicsEnvironment *) (m_pObject->get_environment()->client_data);
}

const CPhysicsEnvironment	*CPhysicsObject::GetVPhysicsEnvironment() const
{
	return NULL; // (CPhysicsEnvironment *) (m_pObject->get_environment()->client_data);
}


bool CPhysicsObject::IsControlling( const IVP_Controller *pController ) const
{
	return false;
}

bool CPhysicsObject::IsGravityEnabled() const
{
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
	return false;
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
}

void CPhysicsObject::EnableDrag( bool enable )
{
	if ( IsStatic() )
		return;

	bool isEnabled = IsDragEnabled();

	if ( enable == isEnabled )
		return;
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
}



void CPhysicsObject::EnableMotion( bool enable )
{
	if ( IsStatic() )
		return;

	bool isMoveable = IsMotionEnabled();

	// no change
	if ( isMoveable == enable )
		return;

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
	return false;
}

Vector CPhysicsObject::GetMassCenterLocalSpace() const
{
	return Vector( 0, 0, 0 );
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

	mass = clamp( mass, 1.f, VPHYSICS_MAX_MASS );
	//m_pObject->change_mass( mass );
	SetVolume( m_volume );
	RecomputeDragBases();
	if ( reset )
	{
		EnableMotion(false);
	}
}

float CPhysicsObject::GetMass( void ) const
{
	return 0.f;
}

float CPhysicsObject::GetInvMass( void ) const
{
	return 1.f;
}

Vector CPhysicsObject::GetInertia( void ) const
{
	return Vector(0, 0, 0);
}

Vector CPhysicsObject::GetInvInertia( void ) const
{
	return Vector(0, 0, 0);
}



void CPhysicsObject::SetInertia( const Vector &inertia )
{
}


void CPhysicsObject::GetDamping( float *speed, float *rot ) const
{
}

void CPhysicsObject::SetDamping( const float *speed, const float *rot )
{
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

}

void CPhysicsObject::ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition )
{
	if ( !IsMoveable() )
		return;

	IVP_U_Point pos;
	IVP_U_Float_Point force;

	ConvertForceImpulseToIVP( forceVector, force );
	ConvertPositionToIVP( worldPosition, pos );

	Wake();
	ClampVelocity();
}

void CPhysicsObject::CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const
{
}

void CPhysicsObject::CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const
{
}

void CPhysicsObject::ApplyTorqueCenter( const AngularImpulse &torqueImpulse )
{
	if ( !IsMoveable() )
		return;

	Wake();
	ClampVelocity();
}

void CPhysicsObject::GetPosition( Vector *worldPosition, QAngle *angles ) const
{
}


void CPhysicsObject::GetPositionMatrix( matrix3x4_t *positionMatrix ) const
{
}


void CPhysicsObject::GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{
	if ( !velocity && !angularVelocity )
		return;
}

void CPhysicsObject::GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const
{
	if ( !velocity && !angularVelocity )
		return;

}

void CPhysicsObject::GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const
{
}


// UNDONE: Limit these?
void CPhysicsObject::AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	Assert(IsMoveable());
	if ( !IsMoveable() )
		return;

	Wake();
	ClampVelocity();
}

void CPhysicsObject::SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport )
{

}

void CPhysicsObject::SetPositionMatrix( const matrix3x4_t& matrix, bool isTeleport )
{
}

void CPhysicsObject::SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	Assert(IsMoveable());
	if ( !IsMoveable() )
		return;

}

void CPhysicsObject::SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity )
{
	if ( !IsMoveable() )
		return;
}


void CPhysicsObject::ClampVelocity()
{
	if ( m_pShadow )
		return;

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
	return;
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

}


void CPhysicsObject::RemoveTrigger()
{
}


bool CPhysicsObject::IsTrigger() const
{
	return false;
}

bool CPhysicsObject::IsFluid() const
{
	return false;
}

// sets the object to be hinged.  Fixed it place, but able to rotate around one axis.
void CPhysicsObject::BecomeHinged( int localAxis )
{
}

void CPhysicsObject::RemoveHinged()
{
	m_hingedAxis = 0;
}

// dumps info about the object to Msg()
void CPhysicsObject::OutputDebugInfo() const
{
	Msg("-----------------\nObject: %s\n", m_pObject->get_name());
	Msg("Mass: %.3e (inv %.3e)\n", GetMass(), GetInvMass() );
	Vector inertia = GetInertia();
	Vector invInertia = GetInvInertia();
	Msg("Inertia: %.3e, %.3e, %.3e (inv %.3e, %.3e, %.3e)\n", inertia.x, inertia.y, inertia.z, invInertia.x, invInertia.y, invInertia.z );

	Vector speed, angSpeed;
	GetVelocity( &speed, &angSpeed );
	Msg("Velocity: %.2f, %.2f, %.2f \n", speed.x, speed.y, speed.z );
	Msg("Ang Velocity: %.2f, %.2f, %.2f \n", angSpeed.x, angSpeed.y, angSpeed.z );

	float damp, angDamp;
	GetDamping( &damp, &angDamp );
	Msg("Damping %.3e linear, %.3e angular\n", damp, angDamp );

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
	return false;
}

static void InitObjectTemplate( IVP_Template_Real_Object &objectTemplate, int materialIndex, objectparams_t *pParams, bool isStatic )
{
	objectTemplate.mass = clamp( pParams->mass, VPHYSICS_MIN_MASS, VPHYSICS_MAX_MASS );

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

	if ( inertia > 1e14f )
		inertia = 1e14f;

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

//	IVP_SurfaceManager *pSurman = CreateSurfaceManager( pCollisionModel, collideType );

//	if ( !pSurman )
//		return NULL;

	pObject->m_collideType = collideType;
	pObject->m_asleepSinceCreation = true;

	BEGIN_IVP_ALLOCATION();

//	IVP_Polygon *realObject = pEnvironment->GetIVPEnvironment()->create_polygon(pSurman, &objectTemplate, &rotation, &pos);

	if( !pCollisionModel )
		return NULL;

	PxScene *scene = pEnvironment->GetPxScene();
	PxShape *shape = pCollisionModel->GetPxShape();

	if( shape )
	{
		RadianEuler radian(angles);
		Quaternion qw(radian);

		PxQuat q( qw.x, qw.y, qw.z, qw.w );

		PxTransform t(PxVec3(position.x, position.y, position.z), q);
		PxRigidStatic* body = gPxPhysics->createRigidStatic(t);

		while( shape )
		{
			body->attachShape( *shape );
			shape = (PxShape*)shape->userData;
		}

		scene->addActor(*body);
	}
#if 0
	if( pCollisionModel && mesh->getNbVertices() != 0 )
	{
		RadianEuler radian(angles);
		Quaternion qw(radian);

		PxQuat q( qw.x, qw.y, qw.z, qw.w );

		PxTransform t(PxVec3(position.x, position.y, position.z), q);
		PxMaterial *mat = gPxPhysics->createMaterial(0.5f, 0.5f, 0.6f);

		PxRigidStatic* body = gPxPhysics->createRigidStatic(t);

		if( body )
		{
			PxShape* aConvexShape = PxRigidActorExt::createExclusiveShape(*body,
				PxConvexMeshGeometry(mesh), *mat);

			scene->addActor(*body);
		}
	}
#endif

	pObject->Init( pCollisionModel, NULL, materialIndex, pParams->volume, pParams->dragCoefficient, pParams->dragCoefficient );
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
	return false;
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

	m_asleepSinceCreation = objectTemplate.asleepSinceCreation;

	if ( objectTemplate.velocity.LengthSqr() != 0 || objectTemplate.angVelocity.LengthSqr() != 0 )
	{
		// will wake up the object
		SetVelocityInstantaneous( &objectTemplate.velocity, &objectTemplate.angVelocity );
	}
	else if( !objectTemplate.isAsleep )
	{
		Assert( !objectTemplate.isStatic );
		WakeNow();
	}

	if( objectTemplate.isAsleep )
	{
		Sleep();
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
