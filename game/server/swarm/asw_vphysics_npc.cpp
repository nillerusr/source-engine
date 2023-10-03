//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "asw_vphysics_npc.h"
#include "physobj.h"
#include "vphysics/player_controller.h"
#include "vcollide_parse.h"
#include "igamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// if this is defined then this npc is given a vphysics shadow, so he can push things, etc. (currently bugged)
//#define USE_VPHYSICS_SHADOW

//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
#define VPHYS_MAX_DISTANCE		2.0
#define VPHYS_MAX_VEL			10
#define VPHYS_MAX_DISTSQR		(VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE)
#define VPHYS_MAX_VELSQR		(VPHYS_MAX_VEL*VPHYS_MAX_VEL)
#define SMOOTHING_FACTOR 0.9

static ConVar marinephysicsshadowupdate_render( "marinephysicsshadowupdate_render", "0" );

//=========================================================
// Marine activities
//=========================================================


LINK_ENTITY_TO_CLASS( asw_vphysics_npc, CASW_VPhysics_NPC );

//---------------------------------------------------------
// 
//---------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CASW_VPhysics_NPC, DT_ASW_VPhysics_NPC)
	
END_SEND_TABLE()



//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_VPhysics_NPC )
	DEFINE_FIELD( m_vNewVPhysicsPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_vNewVPhysicsVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecLastSafePosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_afPhysicsFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_touchedPhysObject, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecSmoothedVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_oldOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_vphysicsCollisionState, FIELD_INTEGER ),
	//DEFINE_FIELD( m_pPhysicsController, FIELD_POINTER ),
	//DEFINE_FIELD( m_pShadowStand, FIELD_POINTER ),
	//DEFINE_FIELD( m_pShadowCrouch, FIELD_POINTER ),
END_DATADESC()


CASW_VPhysics_NPC::CASW_VPhysics_NPC()
{
	
}


CASW_VPhysics_NPC::~CASW_VPhysics_NPC()
{
	VPhysicsDestroyObject();
}

void CASW_VPhysics_NPC::UpdateOnRemove( void )
{
	VPhysicsDestroyObject();

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_VPhysics_NPC::Spawn( void )
{
	BaseClass::Spawn();

#ifdef USE_VPHYSICS_SHADOW
	m_afPhysicsFlags	= 0;
	m_vecSmoothedVelocity = vec3_origin;
	InitVCollision();
	m_vecLastSafePosition = GetAbsOrigin();
#endif
}

int CASW_VPhysics_NPC::Restore( IRestore &restore )
{
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;
		
	// clear this - it will get reset by touching the trigger again
	m_afPhysicsFlags &= ~PFLAG_VPHYSICS_MOTIONCONTROLLER;

	InitVCollision();

	// success
	return 1;
}

void CASW_VPhysics_NPC::InhabitedPhysicsSimulate()
{
#ifdef USE_VPHYSICS_SHADOW	
	return;

	// Update our vphysics object.
	if ( m_pPhysicsController )
	{
		// If simulating at 2 * TICK_INTERVAL, add an extra TICK_INTERVAL to position arrival computation
		//int additionalTick = CBaseEntity::IsSimulatingOnAlternateTicks() ? 1 : 0;

		//float flSecondsToArrival = ( ctx->numcmds + ctx->dropped_packets + additionalTick ) * TICK_INTERVAL;
		float flSecondsToArrival = gpGlobals->frametime;
		UpdateVPhysicsPosition( m_vNewVPhysicsPosition, m_vNewVPhysicsVelocity, flSecondsToArrival );
	}
#endif
}

void CASW_VPhysics_NPC::UpdateVPhysicsAfterMove()
{
#ifdef USE_VPHYSICS_SHADOW	
	// Update our vphysics object.
	if ( m_pPhysicsController )
	{
		// If simulating at 2 * TICK_INTERVAL, add an extra TICK_INTERVAL to position arrival computation
		//int additionalTick = CBaseEntity::IsSimulatingOnAlternateTicks() ? 1 : 0;

		//float flSecondsToArrival = ( ctx->numcmds + ctx->dropped_packets + additionalTick ) * TICK_INTERVAL;
		float flSecondsToArrival = gpGlobals->frametime;
		UpdateVPhysicsPosition( m_vNewVPhysicsPosition, m_vNewVPhysicsVelocity, flSecondsToArrival );
	}
#endif
}

void CASW_VPhysics_NPC::PostThink( void )
{	
	m_vecSmoothedVelocity = m_vecSmoothedVelocity * SMOOTHING_FACTOR + GetAbsVelocity() * ( 1 - SMOOTHING_FACTOR );

	//if ( IsAlive() )
	//{
		//PostThinkVPhysics();
	//}
}

void CASW_VPhysics_NPC::InitVCollision( void )
{
	// Cleanup any old vphysics stuff.
	VPhysicsDestroyObject();

	
	CPhysCollide *pModel = PhysCreateBbox( WorldAlignMins(), WorldAlignMaxs() );
	CPhysCollide *pCrouchModel = PhysCreateBbox( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );

	SetupVPhysicsShadow( pModel, "player_stand", pCrouchModel, "player_crouch" );
}

void CASW_VPhysics_NPC::VPhysicsDestroyObject()
{
#ifdef USE_VPHYSICS_SHADOW
	// Since CBasePlayer aliases its pointer to the physics object, tell CBaseEntity to 
	// clear out its physics object pointer so we don't wind up deleting one of
	// the aliased objects twice.
	VPhysicsSetObject( NULL );

	PhysRemoveShadow( this );
	
	if ( m_pPhysicsController )
	{
		physenv->DestroyPlayerController( m_pPhysicsController );
		m_pPhysicsController = NULL;
	}

	if ( m_pShadowStand )
	{
		PhysDestroyObject( m_pShadowStand );
		m_pShadowStand = NULL;
	}
	if ( m_pShadowCrouch )
	{
		PhysDestroyObject( m_pShadowCrouch );
		m_pShadowCrouch = NULL;
	}
#endif
	BaseClass::VPhysicsDestroyObject();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_VPhysics_NPC::SetVCollisionState( int collisionState )
{
	Vector vel = vec3_origin;
	Vector pos = vec3_origin;
	vel = GetAbsVelocity();
	pos = GetAbsOrigin();

	m_vphysicsCollisionState = collisionState;
	switch( collisionState )
	{
	case VPHYS_WALK:
 		m_pShadowStand->SetPosition( pos, vec3_angle, true );
		m_pShadowStand->SetVelocity( &vel, NULL );
		m_pShadowCrouch->EnableCollisions( false );
		m_pShadowStand->EnableCollisions( true );
		m_pPhysicsController->SetObject( m_pShadowStand );
		VPhysicsSwapObject( m_pShadowStand );
		break;

	case VPHYS_CROUCH:
		m_pShadowCrouch->SetPosition( pos, vec3_angle, true );
		m_pShadowCrouch->SetVelocity( &vel, NULL );
		m_pShadowStand->EnableCollisions( false );
		m_pShadowCrouch->EnableCollisions( true );
		m_pPhysicsController->SetObject( m_pShadowCrouch );
		VPhysicsSwapObject( m_pShadowCrouch );
		break;
	
	case VPHYS_NOCLIP:
		m_pShadowCrouch->EnableCollisions( false );
		m_pShadowStand->EnableCollisions( false );
		break;
	}
}

void CASW_VPhysics_NPC::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
#ifdef USE_VPHYSICS_SHADOW
	Vector newPosition;

	m_touchedPhysObject = false;	// asw try to clear

	bool bMarineStuck = false;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition( &newPosition, NULL ) > 0 ? true : false;

	// UNDONE: If the player is penetrating, but the player's game collisions are not stuck, teleport the physics shadow to the game position
	if ( pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING )
	{
		CUtlVector<CBaseEntity *> list;
		PhysGetListOfPenetratingEntities( this, list );
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			// filter out anything that isn't simulated by vphysics
			// UNDONE: Filter out motion disabled objects?
			if ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				// I'm currently stuck inside a moving object, so allow vphysics to 
				// apply velocity to the player in order to separate these objects
				m_touchedPhysObject = true;
			}
		}
	}

	if ( m_pPhysicsController->IsInContact() || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) )
	{
		m_touchedPhysObject = true;
	}

	if ( IsFollowingPhysics() )
	{
		m_touchedPhysObject = true;
	}

	if ( GetMoveType() == MOVETYPE_NOCLIP )
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if ( phys_timescale.GetFloat() == 0.0f )
	{
		physicsUpdated = false;
	}

	if ( !physicsUpdated )
		return;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition( &newPosition, 0 );
	m_pPhysicsController->GetShadowVelocity( &newVelocity );

	if ( marinephysicsshadowupdate_render.GetBool() )
	{
		//NDebugOverlay::Box( GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 24, 15.0f );
		NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,0,255, 24, 15.0f);
		NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,0,255, 24, .01f);
	}
	//if (m_touchedPhysObject)
		//Msg("touched physics object\n"); 

	Vector tmp = GetAbsOrigin() - newPosition;
	if ( !m_touchedPhysObject && !(GetFlags() & FL_ONGROUND) )
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if ( IsRideablePhysics(pPhysGround) )
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	if ( dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_touchedPhysObject) )
	{
		if ( m_touchedPhysObject || pPhysGround )
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if ( deltaV >= maxVelErrorSqr )
			{
				Vector dir = GetAbsVelocity();
				float len = VectorNormalize(dir);
				float dot = DotProduct( newVelocity, dir );
				if ( dot > len )
				{
					dot = len;
				}
				else if ( dot < -len )
				{
					dot = -len;
				}
				
				VectorMA( newVelocity, -dot, dir, newVelocity );
				
				if ( m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER )
				{
					float val = Lerp( 0.1f, len, dot );
					VectorMA( newVelocity, val - len, dir, newVelocity );
				}

				if ( !IsRideablePhysics(pPhysGround) )
				{
					if ( !(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER ) && IsSimulatingOnAlternateTicks() )
					{
						newVelocity *= 0.5f;
					}
					ApplyAbsVelocityImpulse( newVelocity );
				}
			}
			
			trace_t trace;
			UTIL_TraceEntity( this, newPosition, newPosition, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( !trace.allsolid && !trace.startsolid )
			{
				SetAbsOrigin( newPosition );
			}
			else
			{
				Warning( "Stuck3 on %s!!\n", trace.m_pEnt->GetClassname() );
				bMarineStuck = true;
			}
		}
		else
		{
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			
			// current position is not ok, fixup
			if ( trace.allsolid || trace.startsolid )
			{
				// STUCK!?!?!
				Warning( "Stuck2 on %s!!\n", trace.m_pEnt->GetClassname() );
				SetAbsOrigin( newPosition );
				bMarineStuck = true;
			}
		}
	}
	else
	{
		if ( m_touchedPhysObject )
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
				MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			
			// is current position ok?
			if ( trace.allsolid || trace.startsolid )
			{
				// stuck????!?!?
				Msg("Stuck on %s\n", trace.m_pEnt->GetClassname());
				SetAbsOrigin( newPosition );
				UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
					MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
				if ( trace.allsolid || trace.startsolid )
				{
					Msg("Double Stuck\n");
					SetAbsOrigin( m_oldOrigin );
					bMarineStuck = true;
				}
			}
		}
	}
	m_oldOrigin = GetAbsOrigin();
	// UNDONE: Force physics object to be at player position when not touching phys???

	// if we're stuck, move us to our last safe position and check that position is free of phys objects
	if (bMarineStuck)
	{
		// check if our last safe position is ok
		trace_t trace;
		UTIL_TraceEntity( this, m_vecLastSafePosition, m_vecLastSafePosition, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.allsolid && !trace.startsolid )
		{
			// position is free, so move there
			SetAbsOrigin(m_vecLastSafePosition);
			Msg("Moving marine to last safe position\n");
		}
		else
		{
			// safe position now has something in the way
			// could try: pushing the thing aside?
			//            giving it no collision with the marine? (temporarily?)
			if (trace.m_pEnt)
			{
				CAI_BaseNPC *pAI = dynamic_cast<CAI_BaseNPC*>(trace.m_pEnt);
				if (!pAI && trace.m_pEnt->entindex()>gpGlobals->maxClients)	// make sure it's not an NPC or the world
				{
					//m_BlockingEntities.AddToTail(trace.m_pEnt);
					//m_BlockingCollisionGroup.AddToTail(trace.m_pEnt->GetCollisionGroup());
					//trace.m_pEnt->SetCollisionGroup(ASW_COLLISION_GROUP_PASSABLE);
					//Msg("Turning off collision on object: %d\n", trace.m_pEnt->entindex());
				}
				Msg("Safe position was blocked by %s\n", trace.m_pEnt->GetClassname());
			}
			else
			{
				Msg("Safe position was blocked by unknown\n");
			}
		}
	}
	else
	{
		trace_t trace;
		UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.allsolid && !trace.startsolid )
		{
			m_vecLastSafePosition = GetAbsOrigin();
			// we're safe and free, so restore collision on any previous blocking entities
			//if (m_BlockingEntities.Count() > 0)
			//{
				//for (int i=0;i<m_BlockingEntities.Count();i++)
				//{
					//CBaseEntity *pEnt = m_BlockingEntities[i].Get();
					//if (pEnt)
					//{
						////pEnt->SetCollisionGroup(m_BlockingCollisionGroup[i]);
						////Msg("Restoring collision on object: %d\n", pEnt->entindex());
					//}
				//}
				//m_BlockingEntities.RemoveAll();
				//m_BlockingCollisionGroup.RemoveAll();
			//}
		}
	}
#endif
}

// recreate physics on save/load, don't try to save the state!
bool CASW_VPhysics_NPC::ShouldSavePhysics()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_VPhysics_NPC::PostThinkVPhysics(CMoveData* pMoveData)
{
#ifdef USE_VPHYSICS_SHADOW
	// Check to see if things are initialized!
	if ( !m_pPhysicsController )
	{
		//Msg("not updating physics for marine as he has no physics controller\n");
		return;
	}

	Vector newPosition = GetAbsOrigin();
	float frametime = gpGlobals->frametime;
	if ( frametime <= 0 || frametime > 0.1f )
		frametime = 0.1f;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	// asw phys fix:
	if ( !pPhysGround && m_touchedPhysObject && pMoveData->m_outStepHeight <= 0.f && (GetFlags() & FL_ONGROUND) )
	{
		newPosition = m_oldOrigin + frametime * pMoveData->m_outWishVel;
		newPosition = (GetAbsOrigin() * 0.5f) + (newPosition * 0.5f);
	}

	int collisionState = VPHYS_WALK;
	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
	{
		Msg(" setting vphys_noclip on marine\n");
		collisionState = VPHYS_NOCLIP;
	}
	//else if ( GetFlags() & FL_DUCKING )
	//{
		//Msg(" setting vphys_crouch on marine\n");
		//collisionState = VPHYS_CROUCH;
	//}

	if ( collisionState != m_vphysicsCollisionState )
	{
		SetVCollisionState( collisionState );
	}

	// asw phys fix:
	if ( !(TouchedPhysics() || pPhysGround) )
	{
		float maxSpeed = MaxSpeed();
		pMoveData->m_outWishVel.Init( maxSpeed, maxSpeed, maxSpeed );
	}

	// teleport the physics object up by stepheight (game code does this - reflect in the physics)
	// asw phys fix:
	if ( pMoveData->m_outStepHeight > 0.1f )
	{
		if ( pMoveData->m_outStepHeight > 4.0f )
		{
			VPhysicsGetObject()->SetPosition( GetAbsOrigin(), vec3_angle, true );
		}
		else
		{
			// don't ever teleport into solid
			Vector position, end;
			VPhysicsGetObject()->GetPosition( &position, NULL );
			end = position;
			end.z += pMoveData->m_outStepHeight;
			trace_t trace;
			UTIL_TraceEntity( this, position, end, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( trace.DidHit() )
			{
				pMoveData->m_outStepHeight = trace.endpos.z - position.z;
			}
			m_pPhysicsController->StepUp( pMoveData->m_outStepHeight );
		}
		m_pPhysicsController->Jump();
	}
	pMoveData->m_outStepHeight = 0.0f;

	
	// Store these off because after running the usercmds, it'll pass them
	// to UpdateVPhysicsPosition.	
	m_vNewVPhysicsPosition = newPosition;
	m_vNewVPhysicsVelocity = pMoveData->m_outWishVel;
	//Msg("updating marine vphys pos to %f %f %f\n", newPosition.x, newPosition.y, newPosition.z);
	m_oldOrigin = GetAbsOrigin();
#endif
}

void CASW_VPhysics_NPC::UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival )
{
#ifdef USE_VPHYSICS_SHADOW
	bool onground = (GetFlags() & FL_ONGROUND) ? true : false;
	IPhysicsObject *pPhysGround = GetGroundVPhysics();
	
	// if the object is much heavier than the player, treat it as a local coordinate system
	// the player controller will solve movement differently in this case.
	if ( !IsRideablePhysics(pPhysGround) )
	{
		pPhysGround = NULL;
	}

	m_pPhysicsController->Update( position, velocity, secondsToArrival, onground, pPhysGround );
#endif
}

void CASW_VPhysics_NPC::UpdatePhysicsShadowToCurrentPosition()
{
#ifdef USE_VPHYSICS_SHADOW
	UpdateVPhysicsPosition( GetAbsOrigin(), vec3_origin, 0 );
#endif
}

void CASW_VPhysics_NPC::SetupVPhysicsShadow( CPhysCollide *pStandModel, const char *pStandHullName, CPhysCollide *pCrouchModel, const char *pCrouchHullName )
{
	solid_t solid;
	Q_strncpy( solid.surfaceprop, "player", sizeof(solid.surfaceprop) );
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	solid.params.enableCollisions = false;
	//disable drag
	solid.params.dragCoefficient = 0;
	// create standing hull
	m_pShadowStand = PhysModelCreateCustom( this, pStandModel, GetLocalOrigin(), GetLocalAngles(), pStandHullName, false, &solid );
	m_pShadowStand->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

	// create crouchig hull
	m_pShadowCrouch = PhysModelCreateCustom( this, pCrouchModel, GetLocalOrigin(), GetLocalAngles(), pCrouchHullName, false, &solid );
	m_pShadowCrouch->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

	// default to stand
	VPhysicsSetObject( m_pShadowStand );

	// tell physics lists I'm a shadow controller object
	PhysAddShadow( this );	
	m_pPhysicsController = physenv->CreatePlayerController( m_pShadowStand );
	m_pPhysicsController->SetPushMassLimit( 350.0f );
	m_pPhysicsController->SetPushSpeedLimit( 50.0f );
	
	// Give the controller a valid position so it doesn't do anything rash.
	UpdatePhysicsShadowToCurrentPosition();

	// init state
	if ( GetFlags() & FL_DUCKING )
	{
		SetVCollisionState( VPHYS_CROUCH );
	}
	else
	{
		SetVCollisionState( VPHYS_WALK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Empty, just want to keep the baseentity version from being called
//          current so we don't kick up dust, etc.
//-----------------------------------------------------------------------------
void CASW_VPhysics_NPC::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_VPhysics_NPC::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	float savedImpact = m_impactEnergyScale;
	
	// HACKHACK: Reduce player's stress by 1/8th
	m_impactEnergyScale *= 0.125f;
	ApplyStressDamage( pPhysics, true );
	m_impactEnergyScale = savedImpact;
}

bool CASW_VPhysics_NPC::IsRideablePhysics( IPhysicsObject *pPhysics )
{
	if ( pPhysics )
	{
		if ( pPhysics->GetMass() > (VPhysicsGetObject()->GetMass()*2) )
			return true;
	}

	return false;
}

IPhysicsObject *CASW_VPhysics_NPC::GetGroundVPhysics()
{
	CBaseEntity *pGroundEntity = GetGroundEntity();
	if ( pGroundEntity && pGroundEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhysGround = pGroundEntity->VPhysicsGetObject();
		if ( pPhysGround && pPhysGround->IsMoveable() )
			return pPhysGround;
	}
	return NULL;
}

void CASW_VPhysics_NPC::Touch( CBaseEntity *pOther )
{
#ifdef USE_VPHYSICS_SHADOW
	if ( pOther == GetGroundEntity() )
		return;

	if ( pOther->GetMoveType() != MOVETYPE_VPHYSICS || pOther->GetSolid() != SOLID_VPHYSICS || (pOther->GetSolidFlags() & FSOLID_TRIGGER) )
		return;

	IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
	if ( !pPhys || !pPhys->IsMoveable() )
		return;

	SetTouchedPhysics( true );
#endif
}

float CASW_VPhysics_NPC::MaxSpeed() { return 300.0f; }