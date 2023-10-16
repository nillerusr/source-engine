//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#include "physics_shadow.h"
#include "vphysics/player_controller.h"
#include "physics_friction.h"
#include "vphysics/friction.h"

// IsInContact
#include "ivp_mindist.hxx"
#include "ivp_mindist_intern.hxx"
#include "ivp_core.hxx"
#include "ivp_friction.hxx"
#include "ivp_listener_object.hxx"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct vphysics_save_cshadowcontroller_t;
struct vphysics_save_shadowcontrolparams_t;


// UNDONE: Try this controller!
//damping is usually 1.0
//frequency is usually in the range 1..16
void ComputePDControllerCoefficients( float *coefficientsOut, const float frequency, const float damping, const float dt )
{
	const float ks = 9.0f * frequency * frequency;
	const float kd = 4.5f * frequency * damping;

	const float scale = 1.0f / ( 1.0f  + kd * dt + ks * dt * dt );

	coefficientsOut[0] = ks  *  scale;
	coefficientsOut[1] = ( kd + ks * dt ) * scale;

	// Use this controller like:
	// speed += (coefficientsOut[0] * (targetPos - currentPos) + coefficientsOut[1] * (targetSpeed - currentSpeed)) * dt
}

void ComputeController( IVP_U_Float_Point &currentSpeed, const IVP_U_Float_Point &delta, float maxSpeed, float maxDampSpeed, float scaleDelta, float damping, IVP_U_Float_Point *pOutImpulse = NULL )
{
	if ( currentSpeed.quad_length() < 1e-6 )
	{
		currentSpeed.set_to_zero();
	}

	// scale by timestep
	IVP_U_Float_Point acceleration;
	if ( maxSpeed > 0 )
	{
		acceleration.set_multiple( &delta, scaleDelta );
		float speed = acceleration.real_length();
		if ( speed > maxSpeed )
		{
			speed = maxSpeed / speed;
			acceleration.mult( speed );
		}
	}
	else
	{
		acceleration.set_to_zero();
	}

	IVP_U_Float_Point dampAccel;
	if ( maxDampSpeed > 0 )
	{
		dampAccel.set_multiple( &currentSpeed, -damping );
		float speed = dampAccel.real_length();
		if ( speed > maxDampSpeed )
		{
			speed = maxDampSpeed / speed;
			dampAccel.mult( speed );
		}
	}
	else
	{
		dampAccel.set_to_zero();
	}
	currentSpeed.add( &dampAccel );
	currentSpeed.add( &acceleration );
	if ( pOutImpulse )
	{
		*pOutImpulse = acceleration;
	}
}


void ComputeController( IVP_U_Float_Point &currentSpeed, const IVP_U_Float_Point &delta, const IVP_U_Float_Point &maxSpeed, float scaleDelta, float damping, IVP_U_Float_Point *pOutImpulse )
{
	// scale by timestep
	IVP_U_Float_Point acceleration;
	acceleration.set_multiple( &delta, scaleDelta );
	
	if ( currentSpeed.quad_length() < 1e-6 )
	{
		currentSpeed.set_to_zero();
	}

	acceleration.add_multiple( &currentSpeed, -damping );

	for(int i=2; i>=0; i--)
	{
		if(IVP_Inline_Math::fabsd(acceleration.k[i]) < maxSpeed.k[i]) 
			continue;
		
		// clip force
		acceleration.k[i] = (acceleration.k[i] < 0) ? -maxSpeed.k[i] : maxSpeed.k[i];
	}

	currentSpeed.add( &acceleration );
	if ( pOutImpulse )
	{
		*pOutImpulse = acceleration;
	}
}


static bool IsOnGround( IVP_Real_Object *pivp )
{
	IPhysicsFrictionSnapshot *pSnapshot = CreateFrictionSnapshot( pivp );
	bool bGround = false;
	while (pSnapshot->IsValid())
	{
		Vector normal;
		pSnapshot->GetSurfaceNormal( normal );
		if ( normal.z < -0.7f )
		{
			bGround = true;
			break;
		}
		
		pSnapshot->NextFrictionData();
	}
	DestroyFrictionSnapshot( pSnapshot );
	return bGround;
}

class CPlayerController : public IVP_Controller_Independent, public IPhysicsPlayerController, public IVP_Listener_Object
{
public:
	CPlayerController( CPhysicsObject *pObject );
	~CPlayerController( void );

	// ipion interfaces
    void do_simulation_controller( IVP_Event_Sim *es,IVP_U_Vector<IVP_Core> *cores);
    virtual IVP_CONTROLLER_PRIORITY get_controller_priority() { return (IVP_CONTROLLER_PRIORITY) (IVP_CP_MOTION+1); }
	virtual const char *get_controller_name() { return "vphysics:player"; }

	void SetObject( IPhysicsObject *pObject );
	void SetEventHandler( IPhysicsPlayerControllerEvent *handler );
	void Update( const Vector& position, const Vector& velocity, float secondsToArrival, bool onground, IPhysicsObject *ground );
	void MaxSpeed( const Vector &velocity );
	bool IsInContact( void );
	virtual bool WasFrozen() 
	{ 	
		IVP_Real_Object *pivp = m_pObject->GetObject();
		IVP_Core *pCore = pivp->get_core();
		return pCore->temporarily_unmovable ? true : false;
	}

	void ForceTeleportToCurrentPosition()
	{
		m_forceTeleport = true;
	}

	int GetShadowPosition( Vector *position, QAngle *angles )
	{
		IVP_U_Matrix matrix;
		
		IVP_Environment *pEnv = m_pObject->GetObject()->get_environment();

		double psi = pEnv->get_next_PSI_time().get_seconds();
		m_pObject->GetObject()->calc_at_matrix( psi, &matrix );
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
	void GetShadowVelocity( Vector *velocity );
	virtual void GetLastImpulse( Vector *pOut )
	{
		ConvertPositionToHL( m_lastImpulse, *pOut );
	}

	virtual void StepUp( float height );
	virtual void Jump();
	virtual IPhysicsObject *GetObject() { return m_pObject; }

	virtual void SetPushMassLimit( float maxPushMass )
	{
		m_pushableMassLimit = maxPushMass;
	}

	virtual void SetPushSpeedLimit( float maxPushSpeed )
	{
		m_pushableSpeedLimit = maxPushSpeed;
	}

	virtual float GetPushMassLimit() { return m_pushableMassLimit; }
	virtual float GetPushSpeedLimit() { return m_pushableSpeedLimit; }

	// Object listener
	virtual void event_object_deleted( IVP_Event_Object *pEvent)
	{
		Assert( pEvent->real_object == m_pGround->GetObject() );
		m_pGround = NULL;
	}
	virtual void event_object_created( IVP_Event_Object *) {}
	virtual void event_object_revived( IVP_Event_Object *) {}
	virtual void event_object_frozen ( IVP_Event_Object *) {}

private:
	void AttachObject( void );
	void DetachObject( void );
	int TryTeleportObject( void );
	void SetGround( CPhysicsObject *pGroundObject );

	CPhysicsObject		*m_pObject;
	IVP_U_Float_Point	m_saveRot;
	CPhysicsObject		*m_pGround;	// Uses object listener to clear - so ok to hold over frames

	IPhysicsPlayerControllerEvent	*m_handler;
	float				m_maxDeltaPosition;
	float				m_dampFactor;
	float				m_secondsToArrival;
	float				m_pushableMassLimit;
	float				m_pushableSpeedLimit;
	IVP_U_Point			m_targetPosition;
	IVP_U_Float_Point	m_groundPosition;
	IVP_U_Float_Point	m_maxSpeed;
	IVP_U_Float_Point	m_currentSpeed;
	IVP_U_Float_Point	m_lastImpulse;
	bool				m_enable : 1;
	bool				m_onground : 1;
	bool				m_forceTeleport : 1;
	bool				m_updatedSinceLast : 5;
};


CPlayerController::CPlayerController( CPhysicsObject *pObject )
{
	m_pGround = NULL;
	m_pObject = pObject;
	m_handler = NULL;
	m_maxDeltaPosition = ConvertDistanceToIVP( 24 );
	m_dampFactor = 1.0f;
	m_targetPosition.k[0] = m_targetPosition.k[1] = m_targetPosition.k[2] = 0;
	m_pushableMassLimit = VPHYSICS_MAX_MASS;
	m_pushableSpeedLimit = 1e4f;
	m_forceTeleport = false;
	AttachObject();
}

CPlayerController::~CPlayerController( void ) 
{
	DetachObject();
}

void CPlayerController::SetGround( CPhysicsObject *pGroundObject )
{
	if ( m_pGround != pGroundObject )
	{
		if ( m_pGround && m_pGround->GetObject() )
		{
			m_pGround->GetObject()->remove_listener_object(this);
		}
		m_pGround = pGroundObject;
		if ( m_pGround && m_pGround->GetObject() )
		{
			m_pGround->GetObject()->add_listener_object(this);
		}
	}
}

void CPlayerController::AttachObject( void )
{
	m_pObject->EnableDrag( false );
	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_Core *pCore = pivp->get_core();
	m_saveRot = pCore->rot_speed_damp_factor;
	pCore->rot_speed_damp_factor = IVP_U_Float_Point( 100, 100, 100 );
	pCore->calc_calc();
	BEGIN_IVP_ALLOCATION();
	pivp->get_environment()->get_controller_manager()->add_controller_to_core( this, pCore );
	END_IVP_ALLOCATION();
	m_pObject->AddCallbackFlags( CALLBACK_IS_PLAYER_CONTROLLER );
}

void CPlayerController::DetachObject( void )
{
	if ( !m_pObject )
		return;

	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_Core *pCore = pivp->get_core();
	pCore->rot_speed_damp_factor = m_saveRot;
	pCore->calc_calc();
	m_pObject->RemoveCallbackFlags( CALLBACK_IS_PLAYER_CONTROLLER );
	m_pObject = NULL;
	pivp->get_environment()->get_controller_manager()->remove_controller_from_core( this, pCore );
	SetGround(NULL);
}

void CPlayerController::SetObject( IPhysicsObject *pObject )
{
	CPhysicsObject *obj = (CPhysicsObject *)pObject;
	if ( obj == m_pObject )
		return;

	DetachObject();
	m_pObject = obj;
	AttachObject();
}

int CPlayerController::TryTeleportObject( void )
{
	if ( m_handler && !m_forceTeleport )
	{
		Vector hlPosition;
		ConvertPositionToHL( m_targetPosition, hlPosition );
		if ( !m_handler->ShouldMoveTo( m_pObject, hlPosition ) )
			return 0;
	}

	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_U_Quat targetOrientation;
	IVP_U_Point outPosition;

	pivp->get_quat_world_f_object_AT( &targetOrientation, &outPosition );

	if ( pivp->is_collision_detection_enabled() )
	{
		m_pObject->EnableCollisions( false );
		pivp->beam_object_to_new_position( &targetOrientation, &m_targetPosition, IVP_TRUE );
		m_pObject->EnableCollisions( true );
	}
	else
	{
		pivp->beam_object_to_new_position( &targetOrientation, &m_targetPosition, IVP_TRUE );
	}
	m_forceTeleport = false;
	return 1;
}

void CPlayerController::StepUp( float height )
{
	if ( height == 0.0f )
		return;

	Vector step( 0, 0, height );

	IVP_Real_Object *pIVP = m_pObject->GetObject();
	IVP_U_Quat world_f_object;
	IVP_U_Point positionIVP, deltaIVP;
	ConvertPositionToIVP( step, deltaIVP );
	pIVP->get_quat_world_f_object_AT( &world_f_object, &positionIVP );
	positionIVP.add( &deltaIVP );
	pIVP->beam_object_to_new_position( &world_f_object, &positionIVP, IVP_TRUE );
}

void CPlayerController::Jump()
{
#if 0
	// float for one tick to allow stepping and jumping to work properly
	IVP_Real_Object *pIVP = m_pObject->GetObject();
	const IVP_U_Point *pgrav = pIVP->get_environment()->get_gravity();
	IVP_U_Float_Point gravSpeed;
	gravSpeed.set_multiple( pgrav, pIVP->get_environment()->get_delta_PSI_time() );
	pIVP->get_core()->speed.subtract( &gravSpeed );
#endif
}

const int MAX_LIST_NORMALS = 8;
class CNormalList
{
public:
	bool IsFull() { return m_Normals.Count() == MAX_LIST_NORMALS; }
	void AddNormal( const Vector &normal )
	{
		if ( IsFull() )
			return;

		for ( int i = m_Normals.Count(); --i >= 0; )
		{
			if ( DotProduct( m_Normals[i], normal ) > 0.99f )
				return;
		}
		m_Normals.AddToTail( normal );
	}

	bool HasPositiveProjection( const Vector &vec )
	{
		for ( int i = m_Normals.Count(); --i >= 0; )
		{
			if ( DotProduct( m_Normals[i], vec ) > 0 )
				return true;
		}
		return false;
	}

	// UNDONE: Handle the case better where we clamp to multiple planes
	// and still have a projection, but don't exceed limitVel.  Currently that will stop.
	// when this is done, remove the ground exception below.
	Vector ClampVector( const Vector &inVector, float limitVel )
	{
		if ( m_Normals.Count() > 2 )
		{
			for ( int i = 0; i < m_Normals.Count(); i++ )
			{
				if ( DotProduct(inVector, m_Normals[i]) > 0 )
				{
					return vec3_origin;
				}
			}
		}
		else
		{
			if ( m_Normals.Count() == 2 )
			{
				Vector crease;
				CrossProduct( m_Normals[0], m_Normals[1], crease );
				float dot = DotProduct( inVector, crease );
				return crease * dot;
			}
			else if (m_Normals.Count() == 1)
			{
				float dot = DotProduct( inVector, m_Normals[0] );
				if ( dot > limitVel )
				{
					return inVector + m_Normals[0]*(limitVel - dot);
				}
			}
		}
		return inVector;
	}
private:
	CUtlVectorFixed<Vector, MAX_LIST_NORMALS>	m_Normals;
};

void CPlayerController::do_simulation_controller( IVP_Event_Sim *es,IVP_U_Vector<IVP_Core> *)
{
	if ( !m_enable )
		return;
	IVP_Real_Object *pivp = m_pObject->GetObject();

	IVP_Core *pCore = pivp->get_core();
	Assert(!pCore->pinned && !pCore->physical_unmoveable);
	// current situation
	const IVP_U_Matrix *m_world_f_core = pCore->get_m_world_f_core_PSI();
	const IVP_U_Point *cur_pos_ws = m_world_f_core->get_position();

	IVP_U_Float_Point baseVelocity;
	baseVelocity.set_to_zero();
	// ---------------------------------------------------------
	// Translation
	// ---------------------------------------------------------

	if ( m_pGround )
	{
		const IVP_U_Matrix *pMatrix = m_pGround->GetObject()->get_core()->get_m_world_f_core_PSI();
		pMatrix->vmult4( &m_groundPosition, &m_targetPosition );
		m_pGround->GetObject()->get_core()->get_surface_speed( &m_groundPosition, &baseVelocity );
		pCore->speed.subtract( &baseVelocity );
	}

	IVP_U_Float_Point delta_position;  delta_position.subtract( &m_targetPosition, cur_pos_ws);

	if (!pivp->flags.shift_core_f_object_is_zero)
	{
		IVP_U_Float_Point shift_cs_os_ws;
		m_world_f_core->vmult3( pivp->get_shift_core_f_object(), &shift_cs_os_ws);
		delta_position.subtract( &shift_cs_os_ws );
	}


	IVP_DOUBLE qdist = delta_position.quad_length();

	// UNDONE: This is totally bogus!  Measure error using last known estimate
	// not current position!
	if ( m_forceTeleport || qdist > m_maxDeltaPosition * m_maxDeltaPosition )
	{
		if ( TryTeleportObject() )
			return;
	}

	// float to allow stepping
	const IVP_U_Point *pgrav = es->environment->get_gravity();
	IVP_U_Float_Point gravSpeed;
	gravSpeed.set_multiple( pgrav, es->delta_time );
	if ( m_onground )
	{
		pCore->speed.subtract( &gravSpeed );
	}

	float fraction = 1.0;
	if ( m_secondsToArrival > 0 )
	{
		fraction = es->delta_time / m_secondsToArrival;
		if ( fraction > 1 )
		{
			fraction = 1;
		}
	}
	if ( !m_updatedSinceLast )
	{
		// we haven't received an update from the game code since the last controller step
		// This means we haven't gotten feedback integrated into the motion plan, so the error may be 
		// exaggerated.  Assume that the first updated tick had valid information, and limit
		// all subsequent ticks to the same size impulses.
		// NOTE: Don't update the saved impulse - so any subsequent ticks will still have the last
		// known good information.
		float len = m_lastImpulse.real_length();
		// cap the max speed to the length of the last known good impulse
		IVP_U_Float_Point tmp;
		tmp.set( len, len, len );
		ComputeController( pCore->speed, delta_position, tmp, fraction / es->delta_time, m_dampFactor, NULL );
	}
	else
	{
		ComputeController( pCore->speed, delta_position, m_maxSpeed, fraction / es->delta_time, m_dampFactor, &m_lastImpulse );
	}
	pCore->speed.add( &baseVelocity );
	m_updatedSinceLast = false;

	// UNDONE: Assumes gravity points down
	Vector lastImpulseHL;
	ConvertPositionToHL( pCore->speed, lastImpulseHL );
	IPhysicsFrictionSnapshot *pSnapshot = CreateFrictionSnapshot( pivp );
	bool bGround = false;
	float invMass = pivp->get_core()->get_inv_mass();
	float limitVel = m_pushableSpeedLimit;
	CNormalList normalList;
	while (pSnapshot->IsValid())
	{
		Vector normal;
		pSnapshot->GetSurfaceNormal( normal );
		if ( normal.z < -0.7f )
		{
			bGround = true;
		}
		// remove this when clamp works better
		if ( normal.z > -0.99f )
		{
			IPhysicsObject *pOther = pSnapshot->GetObject(1);
			if ( !pOther->IsMoveable() || pOther->GetMass() > m_pushableMassLimit )
			{
				limitVel = 0.0f;
			}
			float pushSpeed = DotProduct( lastImpulseHL, normal );
			float contactVel = pSnapshot->GetNormalForce() * invMass;
			float pushTotal = pushSpeed + contactVel;
			if ( pushTotal > limitVel )
			{
				normalList.AddNormal( normal );
			}
		}
		
		pSnapshot->NextFrictionData();
	}
	DestroyFrictionSnapshot( pSnapshot );

	Vector clamped = normalList.ClampVector( lastImpulseHL, limitVel );
	Vector limit = clamped - lastImpulseHL;
	IVP_U_Float_Point limitIVP;
	ConvertPositionToIVP( limit, limitIVP );
	pivp->get_core()->speed.add( &limitIVP );
	m_lastImpulse.add( &limitIVP );

	if ( bGround )
	{
		float gravDt = gravSpeed.real_length();
		// moving down?  Press down with full gravity and no more
		if ( m_lastImpulse.k[1] >= 0 )
		{
			float delta = gravDt - m_lastImpulse.k[1];
			pivp->get_core()->speed.k[1] += delta;
			m_lastImpulse.k[1] += delta;
		}
	}

	// if we have time left, subtract it off
	m_secondsToArrival -= es->delta_time;
	if ( m_secondsToArrival < 0 )
	{
		m_secondsToArrival = 0;
	}
}

void CPlayerController::SetEventHandler( IPhysicsPlayerControllerEvent *handler ) 
{
	m_handler = handler;
}

void CPlayerController::Update( const Vector& position, const Vector& velocity, float secondsToArrival, bool onground, IPhysicsObject *ground )
{
	IVP_U_Point	targetPositionIVP;
	IVP_U_Float_Point targetSpeedIVP;

	ConvertPositionToIVP( position, targetPositionIVP );
	ConvertPositionToIVP( velocity, targetSpeedIVP );
	
	m_updatedSinceLast = true;
	// if the object hasn't moved, abort
	if ( targetSpeedIVP.quad_distance_to( &m_currentSpeed ) < 1e-6 )
	{
		if ( targetPositionIVP.quad_distance_to( &m_targetPosition ) < 1e-6 )
		{
			return;
		}
	}

	m_targetPosition.set( &targetPositionIVP );
	m_secondsToArrival = secondsToArrival < 0 ? 0 : secondsToArrival;
	// Sanity check to make sure the position is good.
#ifdef _DEBUG
	float large = 1024 * 512;
	Assert( m_targetPosition.k[0] >= -large && m_targetPosition.k[0] <= large );
	Assert( m_targetPosition.k[1] >= -large && m_targetPosition.k[1] <= large );
	Assert( m_targetPosition.k[2] >= -large && m_targetPosition.k[2] <= large );
#endif

	m_currentSpeed.set( &targetSpeedIVP );

	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_Core *pCore = pivp->get_core();
	IVP_Environment *pEnv = pivp->get_environment();
	pEnv->get_controller_manager()->ensure_core_in_simulation(pCore);

	m_enable = true;
	// m_onground makes this object anti-grav
	// UNDONE: Re-evaluate this
	m_onground = false;//onground;
	if ( velocity.LengthSqr() <= 0.1f )
	{
		// no input velocity, just go where physics takes you.
		m_enable = false;
		ground = NULL;
	}
	else
	{
		MaxSpeed( velocity );
	}

	CPhysicsObject *pGroundObject = static_cast<CPhysicsObject *>(ground);
	SetGround( pGroundObject );
	if ( m_pGround )
	{
		const IVP_U_Matrix *pMatrix = m_pGround->GetObject()->get_core()->get_m_world_f_core_PSI();
		pMatrix->vimult4( &m_targetPosition, &m_groundPosition );
	}
}

void CPlayerController::MaxSpeed( const Vector &velocity )
{
	IVP_Core *pCore = m_pObject->GetObject()->get_core();
	IVP_U_Float_Point ivpVel;
	ConvertPositionToIVP( velocity, ivpVel );
	IVP_U_Float_Point available = ivpVel;
	
	// normalize and save length
	float length = ivpVel.real_length_plus_normize();

	IVP_U_Float_Point baseVelocity;
	baseVelocity.set_to_zero();

	float dot = ivpVel.dot_product( &pCore->speed );
	if ( dot > 0 )
	{
		ivpVel.mult( dot * length );
		available.subtract( &ivpVel );
	}

	pCore->speed.add( &baseVelocity );
	IVP_Float_PointAbs( m_maxSpeed, available );
}


void CPlayerController::GetShadowVelocity( Vector *velocity )
{
	IVP_Core *core = m_pObject->GetObject()->get_core();
	if ( velocity )
	{
		IVP_U_Float_Point speed;
		speed.add( &core->speed, &core->speed_change );
		if ( m_pGround )
		{
			IVP_U_Float_Point baseVelocity;
			m_pGround->GetObject()->get_core()->get_surface_speed( &m_groundPosition, &baseVelocity );
			speed.subtract( &baseVelocity );
		}
		ConvertPositionToHL( speed, *velocity );
	}
}


bool CPlayerController::IsInContact( void )
{
	IVP_Real_Object *pivp = m_pObject->GetObject();
	if ( !pivp->flags.collision_detection_enabled )
		return false;

	IVP_Synapse_Friction *pfriction = pivp->get_first_friction_synapse();
	while ( pfriction )
	{
		extern IVP_Real_Object *GetOppositeSynapseObject( IVP_Synapse_Friction *pfriction );

		IVP_Real_Object *pobj = GetOppositeSynapseObject( pfriction );
		if ( pobj->flags.collision_detection_enabled )
		{
			// skip if this is a static object
			if ( !pobj->get_core()->physical_unmoveable && !pobj->get_core()->pinned )
			{
				CPhysicsObject *pPhys = static_cast<CPhysicsObject *>(pobj->client_data);
				// If this is a game-controlled shadow object, then skip it.
				// otherwise, we're in contact with something physically simulated
				if ( !pPhys->IsControlledByGame() )
					return true;
			}
		}

		pfriction = pfriction->get_next();
	}

	return false;
}


IPhysicsPlayerController *CreatePlayerController( CPhysicsObject *pObject )
{
	return new CPlayerController( pObject );
}

void DestroyPlayerController( IPhysicsPlayerController *pController )
{
	delete pController;
}

void QuaternionDiff( const IVP_U_Quat &p, const IVP_U_Quat &q, IVP_U_Quat &qt )
{
	IVP_U_Quat q2;

	// decide if one of the quaternions is backwards
	q2.set_invert_unit_quat( &q );
	qt.set_mult_quat( &q2, &p );
	qt.normize_quat();
}


void QuaternionAxisAngle( const IVP_U_Quat &q, Vector &axis, float &angle )
{
	angle = 2 * acos(q.w);
	if ( angle > M_PI )
	{
		angle -= 2*M_PI;
	}

	axis.Init( q.x, q.y, q.z );
	VectorNormalize( axis );
}


void GetObjectPosition_IVP( IVP_U_Point &origin, IVP_Real_Object *pivp )
{
	const IVP_U_Matrix *m_world_f_core = pivp->get_core()->get_m_world_f_core_PSI();
	origin.set( m_world_f_core->get_position() );
	if (!pivp->flags.shift_core_f_object_is_zero)
	{
		IVP_U_Float_Point shift_cs_os_ws;
		m_world_f_core->vmult3( pivp->get_shift_core_f_object(), &shift_cs_os_ws );
		origin.add(&shift_cs_os_ws);
	}
}


bool IsZeroVector( const IVP_U_Point &vec )
{
	return (vec.k[0] == 0.0 && vec.k[1] == 0.0 && vec.k[2] == 0.0 ) ? true : false;
}

float ComputeShadowControllerIVP( IVP_Real_Object *pivp, shadowcontrol_params_t &params, float secondsToArrival, float dt )
{
	// resample fraction
	// This allows us to arrive at the target at the requested time
	float fraction = 1.0;
	if ( secondsToArrival > 0 )
	{
		fraction = dt / secondsToArrival;
		if ( fraction > 1 )
		{
			fraction = 1;
		}
	}

	secondsToArrival -= dt;
	if ( secondsToArrival < 0 )
	{
		secondsToArrival = 0;
	}

	if ( fraction <= 0 )
		return secondsToArrival;

	// ---------------------------------------------------------
	// Translation
	// ---------------------------------------------------------

	IVP_U_Point positionIVP;
	GetObjectPosition_IVP( positionIVP, pivp );

	IVP_U_Float_Point delta_position;
	delta_position.subtract( &params.targetPosition, &positionIVP);

	// BUGBUG: Save off velocities and estimate final positions
	// measure error against these final sets
	// also, damp out 100% saved velocity, use max additional impulses
	// to correct error and damp out error velocity
	// extrapolate position
	if ( params.teleportDistance > 0 )
	{
		IVP_DOUBLE qdist;
		if ( !IsZeroVector(params.lastPosition) )
		{
			IVP_U_Float_Point tmpDelta;
			tmpDelta.subtract( &positionIVP, &params.lastPosition );
			qdist = tmpDelta.quad_length();
		}
		else
		{
			// UNDONE: This is totally bogus!  Measure error using last known estimate
			// not current position!
			qdist = delta_position.quad_length();
		}

		if ( qdist > params.teleportDistance * params.teleportDistance )
		{
			if ( pivp->is_collision_detection_enabled() )
			{
				pivp->enable_collision_detection( IVP_FALSE );
				pivp->beam_object_to_new_position( &params.targetRotation, &params.targetPosition, IVP_TRUE );
				pivp->enable_collision_detection( IVP_TRUE );
			}
			else
			{
				pivp->beam_object_to_new_position( &params.targetRotation, &params.targetPosition, IVP_TRUE );
			}
			delta_position.set_to_zero();
		}
	}

	float invDt = 1.0f / dt;
	IVP_Core *pCore = pivp->get_core();
	ComputeController( pCore->speed, delta_position, params.maxSpeed, params.maxDampSpeed, fraction * invDt, params.dampFactor, &params.lastImpulse );

	params.lastPosition.add_multiple( &positionIVP, &pCore->speed, dt );

	IVP_U_Float_Point deltaAngles;
	// compute rotation offset
	IVP_U_Quat deltaRotation;
	QuaternionDiff( params.targetRotation, pCore->q_world_f_core_next_psi, deltaRotation );

	// convert offset to angular impulse
	Vector axis;
	float angle;
	QuaternionAxisAngle( deltaRotation, axis, angle );
	VectorNormalize(axis);

	deltaAngles.k[0] = axis.x * angle;
	deltaAngles.k[1] = axis.y * angle;
	deltaAngles.k[2] = axis.z * angle;

	ComputeController( pCore->rot_speed, deltaAngles, params.maxAngular, params.maxDampAngular, fraction * invDt, params.dampFactor, NULL );

	return secondsToArrival;
}

void ConvertShadowControllerToIVP( const hlshadowcontrol_params_t &in, shadowcontrol_params_t &out )
{
	ConvertPositionToIVP( in.targetPosition, out.targetPosition );
	ConvertRotationToIVP( in.targetRotation, out.targetRotation );
	out.teleportDistance = ConvertDistanceToIVP( in.teleportDistance );

	out.maxSpeed = ConvertDistanceToIVP( in.maxSpeed );
	out.maxDampSpeed = ConvertDistanceToIVP( in.maxDampSpeed );
	out.maxAngular = ConvertAngleToIVP( in.maxAngular );
	out.maxDampAngular = ConvertAngleToIVP( in.maxDampAngular );

	out.dampFactor = in.dampFactor;
}

void ConvertShadowControllerToHL( const shadowcontrol_params_t &in, hlshadowcontrol_params_t &out )
{
	ConvertPositionToHL( in.targetPosition, out.targetPosition );
	ConvertRotationToHL( in.targetRotation, out.targetRotation );
	out.teleportDistance = ConvertDistanceToHL( in.teleportDistance );

	out.maxSpeed = ConvertDistanceToHL( in.maxSpeed );
	out.maxDampSpeed = ConvertDistanceToHL( in.maxDampSpeed );
	out.maxAngular = ConvertAngleToHL( in.maxAngular );
	out.maxDampAngular = ConvertAngleToHL( in.maxDampAngular );

	out.dampFactor = in.dampFactor;
}

float ComputeShadowControllerHL( CPhysicsObject *pObject, const hlshadowcontrol_params_t &params, float secondsToArrival, float dt )
{
	shadowcontrol_params_t ivpParams;
	ConvertShadowControllerToIVP( params, ivpParams );
	return ComputeShadowControllerIVP( pObject->GetObject(), ivpParams, secondsToArrival, dt );
}

class CShadowController : public IVP_Controller_Independent, public IPhysicsShadowController, public CAlignedNewDelete<16>
{
public:
	CShadowController();
	CShadowController( CPhysicsObject *pObject, bool allowTranslation, bool allowRotation );
	~CShadowController( void );

	// ipion interfaces
    void do_simulation_controller( IVP_Event_Sim *es,IVP_U_Vector<IVP_Core> *cores);
    virtual IVP_CONTROLLER_PRIORITY get_controller_priority() { return IVP_CP_MOTION; }
	virtual const char *get_controller_name() { return "vphysics:shadow"; }

	void SetObject( IPhysicsObject *pObject );
	void Update( const Vector &position, const QAngle &angles, float secondsToArrival );
	void MaxSpeed( float maxSpeed, float maxAngularSpeed );
	virtual void StepUp( float height );
	virtual void SetTeleportDistance( float teleportDistance );
	virtual bool AllowsTranslation() { return m_allowsTranslation; }
	virtual bool AllowsRotation() { return m_allowsRotation; }

	virtual void GetLastImpulse( Vector *pOut )
	{
		ConvertPositionToHL( m_shadow.lastImpulse, *pOut );
	}
	bool IsEnabled() { return m_enabled; }
	void Enable( bool bEnable ) 
	{ 
		m_enabled = bEnable;
	}

	void WriteToTemplate( vphysics_save_cshadowcontroller_t &controllerTemplate );
	void InitFromTemplate( const vphysics_save_cshadowcontroller_t &controllerTemplate );
	
	virtual void SetPhysicallyControlled( bool isPhysicallyControlled ) { m_isPhysicallyControlled = isPhysicallyControlled; }
	virtual bool IsPhysicallyControlled() { return m_isPhysicallyControlled; }

	virtual void UseShadowMaterial( bool bUseShadowMaterial )
	{
		if ( !m_pObject )
			return;

		int current = m_pObject->GetMaterialIndexInternal();
		int target = bUseShadowMaterial ? MATERIAL_INDEX_SHADOW : m_savedMaterialIndex;
		if ( target != current )
		{
			m_pObject->SetMaterialIndex( target );
		}
	}

	virtual void ObjectMaterialChanged( int materialIndex )
	{
		if ( !m_pObject )
			return;
		m_savedMaterialIndex = materialIndex;
	}

	//Basically get the last inputs to IPhysicsShadowController::Update(), returns last input to timeOffset in Update()
	virtual float GetTargetPosition( Vector *pPositionOut, QAngle *pAnglesOut );

	virtual float GetTeleportDistance( void );
	virtual void GetMaxSpeed( float *pMaxSpeedOut, float *pMaxAngularSpeedOut );

private:
	void AttachObject( void );
	void DetachObject( void );

	shadowcontrol_params_t	m_shadow;
	IVP_U_Float_Point	m_saveRot;
	IVP_U_Float_Point	m_savedRI;
	CPhysicsObject		*m_pObject;
	float				m_secondsToArrival;
	float				m_savedMass;
	unsigned int		m_savedFlags;

	unsigned short		m_savedMaterialIndex;
	bool				m_enabled : 1;
	bool				m_allowsTranslation : 1;
	bool				m_allowsRotation : 1;
	bool				m_isPhysicallyControlled : 1;
};

CShadowController::CShadowController()
{ 
	m_shadow.targetPosition.set_to_zero(); 
	m_shadow.targetRotation.init(); 
}

CShadowController::CShadowController( CPhysicsObject *pObject, bool allowTranslation, bool allowRotation )
{
	m_pObject = pObject;
	m_shadow.dampFactor = 1.0f;
	m_shadow.teleportDistance = 0;
	m_shadow.maxDampSpeed = 0;
	m_shadow.maxDampAngular = 0;
	m_shadow.targetPosition.set_to_zero(); 
	m_shadow.targetRotation.init(); 

	m_allowsTranslation = allowTranslation;
	m_allowsRotation = allowRotation;

	m_isPhysicallyControlled = false;
	m_enabled = false;

	AttachObject();
}

CShadowController::~CShadowController( void ) 
{
	DetachObject();
}

void CShadowController::AttachObject( void )
{
	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_Core *pCore = pivp->get_core();
	m_saveRot = pCore->rot_speed_damp_factor;
	m_savedRI = *pCore->get_rot_inertia();
	m_savedMass = pCore->get_mass();
	m_savedMaterialIndex = m_pObject->GetMaterialIndexInternal();

	Vector position;
	QAngle angles;
	m_pObject->GetPosition( &position, &angles );
	ConvertPositionToIVP( position, m_shadow.targetPosition );
	ConvertRotationToIVP( angles, m_shadow.targetRotation );

	UseShadowMaterial( true );

	pCore->rot_speed_damp_factor = IVP_U_Float_Point( 100, 100, 100 );

	if ( !m_allowsRotation )
	{
		IVP_U_Float_Point ri( 1e14f, 1e14f, 1e14f );
		pCore->set_rotation_inertia( &ri );
	}
	if ( !m_allowsTranslation )
	{
		m_pObject->SetMass( VPHYSICS_MAX_MASS );
		//pCore->inv_rot_inertia.hesse_val = 0.0f;
		m_pObject->EnableGravity( false );
	}

	m_savedFlags = m_pObject->CallbackFlags();
	unsigned int flags = m_savedFlags | CALLBACK_SHADOW_COLLISION;
	flags &= ~CALLBACK_GLOBAL_FRICTION;
	flags &= ~CALLBACK_GLOBAL_COLLIDE_STATIC;
	m_pObject->SetCallbackFlags( flags );
	m_pObject->EnableDrag( false );

	pCore->calc_calc();
	pivp->get_environment()->get_controller_manager()->add_controller_to_core( this, pCore );
	m_shadow.lastPosition.set_to_zero();
}

void CShadowController::DetachObject( void )
{
	IVP_Real_Object *pivp = m_pObject->GetObject();
	IVP_Core *pCore = pivp->get_core();
	// don't bother if we're just doing this to delete the object
	if ( !(m_pObject->GetCallbackFlags() & CALLBACK_MARKED_FOR_DELETE) )
	{
		pCore->rot_speed_damp_factor = m_saveRot;
		pCore->set_mass( m_savedMass );
		m_pObject->SetCallbackFlags( m_savedFlags );
		m_pObject->EnableDrag( true );
		m_pObject->EnableGravity( true );
		UseShadowMaterial( false );
		pCore->set_rotation_inertia( &m_savedRI ); // this calls calc_calc()
	}
	m_pObject = NULL;
	pivp->get_environment()->get_controller_manager()->remove_controller_from_core( this, pCore );
}

void CShadowController::SetObject( IPhysicsObject *pObject )
{
	CPhysicsObject *obj = (CPhysicsObject *)pObject;
	if ( obj == m_pObject )
		return;

	DetachObject();
	m_pObject = obj;
	AttachObject();
}


void CShadowController::StepUp( float height )
{
	Vector step( 0, 0, height );

	IVP_Real_Object *pIVP = m_pObject->GetObject();
	IVP_U_Quat world_f_object;
	IVP_U_Point positionIVP, deltaIVP;
	ConvertPositionToIVP( step, deltaIVP );
	pIVP->get_quat_world_f_object_AT( &world_f_object, &positionIVP );
	positionIVP.add( &deltaIVP );
	pIVP->beam_object_to_new_position( &world_f_object, &positionIVP, IVP_TRUE );
}

void CShadowController::SetTeleportDistance( float teleportDistance )
{
	m_shadow.teleportDistance = ConvertDistanceToIVP( teleportDistance );
}

float CShadowController::GetTeleportDistance( void )
{
	return ConvertDistanceToHL( m_shadow.teleportDistance );
}

void CShadowController::do_simulation_controller( IVP_Event_Sim *es,IVP_U_Vector<IVP_Core> *)
{
	if ( IsEnabled() )
	{
		IVP_Real_Object *pivp = m_pObject->GetObject();
		Assert(!pivp->get_core()->pinned && !pivp->get_core()->physical_unmoveable);

		ComputeShadowControllerIVP( pivp, m_shadow, m_secondsToArrival, es->delta_time );
		if ( m_allowsTranslation )
		{
			// UNDONE: Assumes gravity points down
			const IVP_U_Point *pgrav = pivp->get_environment()->get_gravity();
			float gravDt = pgrav->k[1] * es->delta_time;
			
			if ( m_shadow.lastImpulse.k[1] > gravDt )
			{
				if ( IsOnGround( pivp ) )
				{
					float delta = gravDt - m_shadow.lastImpulse.k[1];
					pivp->get_core()->speed.k[1] += delta;
					m_shadow.lastImpulse.k[1] += delta;
				}
			}
		}

		// if we have time left, subtract it off
		m_secondsToArrival -= es->delta_time;
		if ( m_secondsToArrival < 0 )
		{
			m_secondsToArrival = 0;
		}
	}
	else
	{
		m_shadow.lastPosition.set_to_zero();
	}
}

// NOTE: This isn't a test for equivalent orientations, it's a test for calling update
// with EXACTLY the same data repeatedly
static bool IsEqual( const IVP_U_Point &pt0, const IVP_U_Point &pt1 )
{
	return pt0.quad_distance_to( &pt1 ) < 1e-8f ? true : false;
}

// NOTE: This isn't a test for equivalent orientations, it's a test for calling update
// with EXACTLY the same data repeatedly
static bool IsEqual( const IVP_U_Quat &pt0, const IVP_U_Quat &pt1 )
{
	float delta = fabs(pt0.x - pt1.x);
	delta += fabs(pt0.y - pt1.y);
	delta += fabs(pt0.z - pt1.z);
	delta += fabs(pt0.w - pt1.w);
	return delta < 1e-8f ? true : false;
}

void CShadowController::Update( const Vector &position, const QAngle &angles, float secondsToArrival )
{
	IVP_U_Point targetPosition = m_shadow.targetPosition;
	IVP_U_Quat targetRotation = m_shadow.targetRotation;

	ConvertPositionToIVP( position, m_shadow.targetPosition );
	m_secondsToArrival = secondsToArrival < 0 ? 0 : secondsToArrival;
	ConvertRotationToIVP( angles, m_shadow.targetRotation );

	Enable( true );

	if ( IsEqual( targetPosition, m_shadow.targetPosition ) && IsEqual( targetRotation, m_shadow.targetRotation ) )
		return;

	m_pObject->Wake();
}

float CShadowController::GetTargetPosition( Vector *pPositionOut, QAngle *pAnglesOut )
{
	if( pPositionOut )
		ConvertPositionToHL( m_shadow.targetPosition, *pPositionOut );

	if( pAnglesOut )
		ConvertRotationToHL( m_shadow.targetRotation, *pAnglesOut );

	return m_secondsToArrival;
}

void CShadowController::MaxSpeed( float maxSpeed, float maxAngularSpeed )
{
	// UNDONE: Turn this on when shadow controllers are having velocity updated per frame
	// right now this has the effect of making dampspeed zero by default.
#if 0
	IVP_Core *pCore = m_pObject->GetObject()->get_core();
	{
		// limit additional velocity to that which is not amplifying the current velocity
		float availableSpeed = ConvertDistanceToIVP( maxSpeed );
		float currentSpeed = pCore->speed.real_length();

		m_shadow.maxDampSpeed = min(currentSpeed, availableSpeed);
		m_shadow.maxSpeed = availableSpeed - m_shadow.maxDampSpeed;
	}

	{
		// limit additional velocity to that which is not amplifying the current velocity
		float availableAngularSpeed = ConvertAngleToIVP( maxAngularSpeed );
		float currentAngularSpeed = pCore->rot_speed.real_length();
		m_shadow.maxDampAngular = min(currentAngularSpeed, availableAngularSpeed);
		m_shadow.maxAngular = availableAngularSpeed - m_shadow.maxDampAngular;
	}
#else
	m_shadow.maxSpeed = maxSpeed;
	m_shadow.maxDampSpeed = maxSpeed;
	m_shadow.maxAngular = maxAngularSpeed;
	m_shadow.maxDampAngular = maxAngularSpeed;
#endif
}

void CShadowController::GetMaxSpeed( float *pMaxSpeedOut, float *pMaxAngularSpeedOut )
{
	if( pMaxSpeedOut )
		*pMaxSpeedOut = m_shadow.maxSpeed;

	if( pMaxAngularSpeedOut )
		*pMaxAngularSpeedOut = m_shadow.maxAngular;
}

struct vphysics_save_shadowcontrolparams_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( vphysics_save_shadowcontrolparams_t )
	DEFINE_FIELD( targetPosition,		FIELD_POSITION_VECTOR	),
	DEFINE_FIELD( targetRotation,		FIELD_VECTOR	),
	DEFINE_FIELD( maxSpeed,			FIELD_FLOAT	),
	DEFINE_FIELD( maxDampSpeed,		FIELD_FLOAT	),
	DEFINE_FIELD( maxAngular,			FIELD_FLOAT	),
	DEFINE_FIELD( maxDampAngular,		FIELD_FLOAT	),
	DEFINE_FIELD( dampFactor,			FIELD_FLOAT	),
	DEFINE_FIELD( teleportDistance,	FIELD_FLOAT	),
END_DATADESC()

struct vphysics_save_cshadowcontroller_t
{
	CPhysicsObject		*pObject;
	float				secondsToArrival;
	IVP_U_Float_Point	saveRot;
	IVP_U_Float_Point	savedRI;
	IVP_U_Float_Point	currentSpeed;

	float				savedMass;
	int					savedMaterial;
	unsigned int		savedFlags;
	bool				enable;
	bool				allowPhysicsMovement;
	bool				allowPhysicsRotation;
	bool				isPhysicallyControlled;

	hlshadowcontrol_params_t	shadowParams;

	DECLARE_SIMPLE_DATADESC();
};


BEGIN_SIMPLE_DATADESC( vphysics_save_cshadowcontroller_t )
	//DEFINE_VPHYSPTR( pObject ),
	DEFINE_FIELD( secondsToArrival,		FIELD_FLOAT	),
	DEFINE_ARRAY( saveRot.k,				FIELD_FLOAT, 3 ),
	DEFINE_ARRAY( savedRI.k,				FIELD_FLOAT, 3 ),
	DEFINE_ARRAY( currentSpeed.k,			FIELD_FLOAT, 3 ),
	DEFINE_FIELD( savedMass,				FIELD_FLOAT	),
	DEFINE_FIELD( savedFlags,				FIELD_INTEGER	),
	DEFINE_CUSTOM_FIELD( savedMaterial, MaterialIndexDataOps() ),
	DEFINE_FIELD( enable,						FIELD_BOOLEAN	),
	DEFINE_FIELD( allowPhysicsMovement,		FIELD_BOOLEAN	),
	DEFINE_FIELD( allowPhysicsRotation,		FIELD_BOOLEAN	),
	DEFINE_FIELD( isPhysicallyControlled,	FIELD_BOOLEAN	),

	DEFINE_EMBEDDED_OVERRIDE( shadowParams, vphysics_save_shadowcontrolparams_t ),
END_DATADESC()

void CShadowController::WriteToTemplate( vphysics_save_cshadowcontroller_t &controllerTemplate )
{
	controllerTemplate.pObject = m_pObject;
	controllerTemplate.secondsToArrival = m_secondsToArrival;
	controllerTemplate.saveRot = m_saveRot;
	controllerTemplate.savedRI = m_savedRI;
	controllerTemplate.savedMass = m_savedMass;
	controllerTemplate.savedFlags = m_savedFlags;

	controllerTemplate.savedMaterial = m_savedMaterialIndex;
	controllerTemplate.enable = IsEnabled();
	controllerTemplate.allowPhysicsMovement = m_allowsTranslation;
	controllerTemplate.allowPhysicsRotation = m_allowsRotation;
	controllerTemplate.isPhysicallyControlled = m_isPhysicallyControlled;

	ConvertShadowControllerToHL( m_shadow, controllerTemplate.shadowParams );
}

void CShadowController::InitFromTemplate( const vphysics_save_cshadowcontroller_t &controllerTemplate )
{
	m_pObject = controllerTemplate.pObject;
	m_secondsToArrival = controllerTemplate.secondsToArrival;
	m_saveRot = controllerTemplate.saveRot;
	m_savedRI = controllerTemplate.savedRI;
	m_savedMass = controllerTemplate.savedMass;
	m_savedFlags = controllerTemplate.savedFlags;
	m_savedMaterialIndex = controllerTemplate.savedMaterial;

	Enable( controllerTemplate.enable );
	m_allowsTranslation = controllerTemplate.allowPhysicsMovement;
	m_allowsRotation = controllerTemplate.allowPhysicsRotation;
	m_isPhysicallyControlled = controllerTemplate.isPhysicallyControlled;
	
	ConvertShadowControllerToIVP( controllerTemplate.shadowParams, m_shadow );
	m_pObject->GetObject()->get_environment()->get_controller_manager()->add_controller_to_core( this, m_pObject->GetObject()->get_core() );
}


IPhysicsShadowController *CreateShadowController( CPhysicsObject *pObject, bool allowTranslation, bool allowRotation )
{
	return new CShadowController( pObject, allowTranslation, allowRotation );
}



bool SavePhysicsShadowController( const physsaveparams_t &params, IPhysicsShadowController *pIShadow )
{
	vphysics_save_cshadowcontroller_t controllerTemplate;
	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );

	CShadowController *pShadowController = (CShadowController *)pIShadow;
	pShadowController->WriteToTemplate( controllerTemplate );
	params.pSave->WriteAll( &controllerTemplate );
	return true;
}

bool RestorePhysicsShadowController( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController )
{
	return false;
}

bool RestorePhysicsShadowControllerInternal( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController, CPhysicsObject *pObject )
{
	vphysics_save_cshadowcontroller_t controllerTemplate;

	memset( &controllerTemplate, 0, sizeof(controllerTemplate) );
	params.pRestore->ReadAll( &controllerTemplate );
	
	// HACKHACK: pass this in
	controllerTemplate.pObject = pObject;

	CShadowController *pShadow = new CShadowController();
	pShadow->InitFromTemplate( controllerTemplate );

	*ppShadowController = pShadow;
	return true;
}

bool SavePhysicsPlayerController( const physsaveparams_t &params, CPlayerController *pPlayerController )
{
	return false;
}

bool RestorePhysicsPlayerController( const physrestoreparams_t &params, CPlayerController **ppPlayerController )
{
	return false;
}

//HACKHACK: The physics object transfer system needs to temporarily detach a shadow controller from an object, then reattach without other repercussions
void ControlPhysicsShadowControllerAttachment_Silent( IPhysicsShadowController *pController, IVP_Real_Object *pivp, bool bAttach )
{
	if( bAttach )
	{
		pivp->get_environment()->get_controller_manager()->add_controller_to_core( (CShadowController *)pController, pivp->get_core() );
	}
	else
	{
		pivp->get_environment()->get_controller_manager()->remove_controller_from_core( (CShadowController *)pController, pivp->get_core() );
	}
}

void ControlPhysicsPlayerControllerAttachment_Silent( IPhysicsPlayerController *pController, IVP_Real_Object *pivp, bool bAttach )
{
	if( bAttach )
	{
		pivp->get_environment()->get_controller_manager()->add_controller_to_core( (CPlayerController *)pController, pivp->get_core() );
	}
	else
	{
		pivp->get_environment()->get_controller_manager()->remove_controller_from_core( (CPlayerController *)pController, pivp->get_core() );
	}
}

