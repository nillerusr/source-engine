#include "cbase.h"
#include "asw_alien_jumper.h"

#include "ai_route.h"
#include "ai_hint.h"
#include "ai_memory.h"
#include "ai_moveprobe.h"
#include "npcevent.h"
#include "IEffects.h"
#include "ndebugoverlay.h"
#include "soundent.h"
#include "soundenvelope.h"
#include "ai_squad.h"
#include "ai_network.h"
#include "ai_pathfinder.h"
#include "ai_navigator.h"
#include "ai_senses.h"
#include "npc_rollermine.h"
#include "ai_blended_movement.h"
#include "physics_prop_ragdoll.h"
#include "iservervehicle.h"
#include "player_pickup.h"
#include "props.h"
#include "antlion_dust.h"
#include "decals.h"
#include "prop_combine_ball.h"
#include "eventqueue.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar  asw_test_new_alien_jump( "asw_test_new_alien_jump", "1", FCVAR_ARCHIVE );
extern ConVar asw_debug_aliens;
extern ConVar sv_gravity;

int AE_ASW_ALIEN_START_JUMP;
int AE_ASW_ALIEN_GLIDE;

#define	ANTLION_JUMP_MIN			128.0f

#define	ANTLION_JUMP_MAX_RISE		512.0f
#define	ANTLION_JUMP_MAX			1024.0f


enum
{	
	SQUAD_SLOT_ALIEN_JUMP = LAST_SHARED_SQUADSLOT,
};

int ACT_ASW_ALIEN_JUMP_START;
int ACT_ASW_ALIEN_LAND;


CASW_Alien_Jumper::CASW_Alien_Jumper()
{
	m_flJumpTime	= 0.0f;
	m_flNextJumpPushTime = 0.0f;

	m_vecLastJumpAttempt.Init();
	m_vecSavedJump.Init();
	m_bForcedStuckJump = false;
}

LINK_ENTITY_TO_CLASS( asw_alien_jumper, CASW_Alien_Jumper );

BEGIN_DATADESC( CASW_Alien_Jumper )
	DEFINE_FIELD( m_flJumpTime,				FIELD_TIME ),
	DEFINE_FIELD( m_vecSavedJump,			FIELD_VECTOR ),
	DEFINE_FIELD( m_vecLastJumpAttempt,		FIELD_VECTOR ),
	DEFINE_FIELD( m_bDisableJump,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextJumpPushTime,		FIELD_TIME ),
	DEFINE_FIELD( m_bForcedStuckJump,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTriggerJumped,			FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"EnableJump", InputEnableJump ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableJump", InputDisableJump ),
END_DATADESC()

void CASW_Alien_Jumper::Spawn()
{
	BaseClass::Spawn();
	
	CapabilitiesAdd( bits_CAP_MOVE_JUMP );
	m_bDisableJump = false;
}

void CASW_Alien_Jumper::ManageFleeCapabilities( bool bEnable )
{
	if ( bEnable == false )
	{
		//Remove the jump capabilty when we build our route.
		//We'll enable it back again after the route has been built.
		CapabilitiesRemove( bits_CAP_MOVE_JUMP );

		if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false  )
			 CapabilitiesRemove( bits_CAP_SKIP_NAV_GROUND_CHECK );
	}
	else
	{
		if ( m_bDisableJump == false )
			 CapabilitiesAdd( bits_CAP_MOVE_JUMP );

		if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false  )
			 CapabilitiesAdd( bits_CAP_SKIP_NAV_GROUND_CHECK );
	}
}

void CASW_Alien_Jumper::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_ASW_ALIEN_START_JUMP )
	{
		StartJump();
		return;
	}
	else if ( nEvent == AE_ASW_ALIEN_GLIDE )
	{
		SetActivity(ACT_GLIDE);
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

bool CASW_Alien_Jumper::IsUnusableNode(int iNodeID, CAI_Hint *pHint)
{
	bool iBaseReturn = BaseClass::IsUnusableNode( iNodeID, pHint );

	if ( asw_test_new_alien_jump.GetBool() == 0 )
		 return iBaseReturn;

	CAI_Node *pNode = GetNavigator()->GetNetwork()->GetNode( iNodeID );

	if ( pNode )
	{
		if ( pNode->IsLocked() )
			 return true;
	}

	return iBaseReturn;
}

void CASW_Alien_Jumper::LockJumpNode( void )
{
	if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false )
		 return;
	
	if ( GetNavigator()->GetPath() == NULL )
		 return;

	if ( asw_test_new_alien_jump.GetBool() == false )
		 return;

	AI_Waypoint_t *pWaypoint = GetNavigator()->GetPath()->GetCurWaypoint();

	while ( pWaypoint )
	{
		AI_Waypoint_t *pNextWaypoint = pWaypoint->GetNext();
		if ( pNextWaypoint && pNextWaypoint->NavType() == NAV_JUMP && pWaypoint->iNodeID != NO_NODE )
		{
			CAI_Node *pNode = GetNavigator()->GetNetwork()->GetNode( pWaypoint->iNodeID );

			if ( pNode )
			{
				//NDebugOverlay::Box( pNode->GetOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 255, 0, 0, 0, 2 );
				pNode->Lock( 0.5f );
				break;
			}
		}
		else
		{
			pWaypoint = pWaypoint->GetNext();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CASW_Alien_Jumper::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
{
	bool iBaseReturn = BaseClass::OnObstructionPreSteer( pMoveGoal, distClear, pResult );

	if ( asw_test_new_alien_jump.GetBool() == false )
		 return iBaseReturn;

	if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false )
		 return iBaseReturn;

	CAI_BaseNPC *pBlocker = pMoveGoal->directTrace.pObstruction->MyNPCPointer();

	// push other jumpers out of the way?
	if ( pBlocker && pBlocker->Classify() == CLASS_ASW_DRONE )
	{
		// HACKHACK
		CASW_Alien_Jumper *pJumper = dynamic_cast< CASW_Alien_Jumper * > ( pBlocker );

		if ( pJumper )
		{
			if ( pJumper->AllowedToBePushed() == true && GetEnemy() == NULL )
			{
				//NDebugOverlay::Box( pAntlion->GetAbsOrigin(), GetHullMins(), GetHullMaxs(), 0, 255, 0, 0, 2 );
				pJumper->GetMotor()->SetIdealYawToTarget( WorldSpaceCenter() );
				pJumper->SetSchedule( SCHED_MOVE_AWAY );
				pJumper->m_flNextJumpPushTime = gpGlobals->curtime + 2.0f;
			}
		}
	}

	return iBaseReturn;
}

bool CASW_Alien_Jumper::AllowedToBePushed( void )
{
	if ( IsMoving() == false 
		 && GetNavType() != NAV_JUMP && m_flNextJumpPushTime <= gpGlobals->curtime )
	{
		return true;
	}

	return false;
}


void CASW_Alien_Jumper::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask ) 
	{
	case TASK_ASW_ALIEN_FACE_JUMP:
		break;

	case TASK_ASW_ALIEN_JUMP:
		
		if ( CheckLanding() )
		{
			TaskComplete();
		}
		break;
	case TASK_ASW_ALIEN_RETRY_JUMP:
		{			
			if (!DoJumpTo(m_vecLastJumpAttempt))
				WaitAndRetryJump(m_vecLastJumpAttempt);
		}
		break;
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

void CASW_Alien_Jumper::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ASW_ALIEN_FACE_JUMP:
		{
			Vector	jumpDir = m_vecSavedJump;
			VectorNormalize( jumpDir );
			
			QAngle	jumpAngles;
			VectorAngles( jumpDir, jumpAngles );

			GetMotor()->SetIdealYawAndUpdate( jumpAngles[YAW], AI_KEEP_YAW_SPEED );
			SetTurnActivity();
			
			if ( abs(GetMotor()->DeltaIdealYaw()) < 2 )
			{
				//Msg("Alien jumping: jumpyaw=%f delta=%f ang.y=%f\n",
					//jumpAngles[YAW], GetMotor()->DeltaIdealYaw(), GetLocalAngles().y);
				TaskComplete();
			}
		}

		break;
	case TASK_ASW_ALIEN_JUMP:

		if ( CheckLanding() )
		{
			TaskComplete();
		}

		break;
	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

// Purpose: Returns true if a reasonable jumping distance
bool CASW_Alien_Jumper::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE		= 512;
	//const float MIN_JUMP_RISE		= 16;
	const float MAX_JUMP_DROP		= 512;
	const float MAX_JUMP_DISTANCE	= 1024;
	const float MIN_JUMP_DISTANCE   = 128;	

	// make sure we don't do really flat jumps
	//float fHeight = (apex.z - startPos.z);
	//Msg("checking legality of jump with height %f\n", fHeight);
	//if ((apex.z - startPos.z) < 10 || fHeight != 0) 
		//return false;
	
	//Adrian: Don't try to jump if my destination is right next to me.
	if ( ( endPos - GetAbsOrigin()).Length() < MIN_JUMP_DISTANCE ) 
		 return false;

	if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) && asw_test_new_alien_jump.GetBool() == true )
	{
		trace_t	tr;
		AI_TraceHull( endPos, endPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
		
		if ( tr.m_pEnt )
		{
			CAI_BaseNPC *pBlocker = tr.m_pEnt->MyNPCPointer();

			if ( pBlocker && pBlocker->Classify() == CLASS_ASW_DRONE )
			{
				// HACKHACK	- push other jumpers out of the way
				CASW_Alien_Jumper *pJumper = dynamic_cast< CASW_Alien_Jumper * > ( pBlocker );

				if ( pJumper )
				{
					if ( pJumper->AllowedToBePushed() == true )
					{
					//	NDebugOverlay::Line( GetAbsOrigin(), endPos, 255, 0, 0, 0, 2 );
					//	NDebugOverlay::Box( pAntlion->GetAbsOrigin(), GetHullMins(), GetHullMaxs(), 0, 0, 255, 0, 2 );
						pJumper->GetMotor()->SetIdealYawToTarget( endPos );
						pJumper->SetSchedule( SCHED_MOVE_AWAY );
						pJumper->m_flNextJumpPushTime = gpGlobals->curtime + 2.0f;
					}
				}
			}
		}
	}

	return BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Alien_Jumper::ShouldJump( void )
{
	if ( GetEnemy() == NULL )
		return false;

	// don't jump if we're stunned
	if (m_bElectroStunned)
		return false;

	//Too soon to try to jump
	if ( m_flJumpTime > gpGlobals->curtime )
		return false;

	// only jump if you're on the ground
  	if (!(GetFlags() & FL_ONGROUND) || GetNavType() == NAV_JUMP )
		return false;

	// Don't jump if I'm not allowed
	if ( ( CapabilitiesGet() & bits_CAP_MOVE_JUMP ) == false )
		return false;

	Vector vEnemyForward, vForward;

	GetEnemy()->GetVectors( &vEnemyForward, NULL, NULL );
	GetVectors( &vForward, NULL, NULL );

	float flDot = DotProduct( vForward, vEnemyForward );

	if ( flDot < 0.5f )
		 flDot = 0.5f;

	Vector vecPredictedPos;

	//Get our likely position in two seconds
	UTIL_PredictedPosition( GetEnemy(), flDot * 2.5f, &vecPredictedPos );

	// Don't jump if we're already near the target
	if ( ( GetAbsOrigin() - vecPredictedPos ).LengthSqr() < (512*512) )
		return false;

	//Don't retest if the target hasn't moved enough
	//FIXME: Check your own distance from last attempt as well
	if ( ( ( m_vecLastJumpAttempt - vecPredictedPos ).LengthSqr() ) < (128*128) )
	{
		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );		
		return false;
	}

	Vector	targetDir = ( vecPredictedPos - GetAbsOrigin() );

	float flDist = VectorNormalize( targetDir );

	// don't jump at target it it's very close
	if (flDist < ANTLION_JUMP_MIN)
		return false;

	Vector	targetPos = vecPredictedPos + ( targetDir * (GetHullWidth()*4.0f) );

	// Try the jump
	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_JUMP, GetAbsOrigin(), targetPos, MASK_NPCSOLID, GetNavTargetEntity(), &moveTrace );

	//See if it succeeded
	if ( IsMoveBlocked( moveTrace.fStatus ) )
	{
		if ( asw_debug_aliens.GetInt() == 2 )
		{
			NDebugOverlay::Box( targetPos, GetHullMins(), GetHullMaxs(), 255, 0, 0, 0, 5 );
			NDebugOverlay::Line( GetAbsOrigin(), targetPos, 255, 0, 0, 0, 5 );
		}

		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
		return false;
	}

	if ( asw_debug_aliens.GetInt() == 2 )
	{
		NDebugOverlay::Box( targetPos, GetHullMins(), GetHullMaxs(), 0, 255, 0, 0, 5 );
		NDebugOverlay::Line( GetAbsOrigin(), targetPos, 0, 255, 0, 0, 5 );
	}

	//Save this jump in case the next time fails
	m_vecSavedJump = moveTrace.vJumpVelocity;
	m_vecLastJumpAttempt = targetPos;

	return true;
}

// note: mostly copied from shover parent (no clean way to call up and extend), with the jump part added
int	CASW_Alien_Jumper::SelectCombatSchedule( void )
{
	//Physics target
	if ( HasCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET ) )
	{
		//Msg("Alien shoving physics object from selectcombatschedule\n");
		return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK;
	}

	// Attack if we can
	if ( HasCondition(COND_CAN_MELEE_ATTACK1) )
		return SCHED_MELEE_ATTACK1;

	// See if we can bash what's in our way, or roar
	if ( HasCondition( COND_ENEMY_UNREACHABLE ) )
	{
		int iUnreach = SelectUnreachableSchedule();
		if (iUnreach != -1)
			return iUnreach;
	}

	// Try to jump
	if ( HasCondition( COND_ASW_ALIEN_CAN_JUMP ) )
		return SCHED_ASW_ALIEN_JUMP;

	return BaseClass::BaseClass::SelectSchedule();
}

void CASW_Alien_Jumper::StartJump( void )
{
	if ( m_bForcedStuckJump == false )
	{
		//Must be jumping at an enemy
		if ( GetEnemy() == NULL )
			return;

		//Don't jump if we're not on the ground
		if ( ( GetFlags() & FL_ONGROUND ) == false )
			return;
	}

	//Take us off the ground
	SetGroundEntity( NULL );
	//Msg("Drone doing jump vec %f %f %f\n", m_vecSavedJump.x, m_vecSavedJump.y, m_vecSavedJump.z);
	SetAbsVelocity( m_vecSavedJump );

	m_bForcedStuckJump = false;

	//Setup our jump time so that we don't try it again too soon
	m_flJumpTime = gpGlobals->curtime + random->RandomInt( 2, 6 );
}


// Purpose: Monitor the antlion's jump to play the proper landing sequence
bool CASW_Alien_Jumper::CheckLanding( void )
{
	trace_t	tr;
	Vector	testPos;

	//Amount of time to predict forward
	const float	timeStep = 0.1f;

	//Roughly looks one second into the future
	testPos = GetAbsOrigin() + ( GetAbsVelocity() * timeStep );
	testPos[2] -= ( 0.5 * sv_gravity.GetFloat() * GetGravity() * timeStep * timeStep);

	if ( asw_debug_aliens.GetInt() == 2 )
	{
		NDebugOverlay::Line( GetAbsOrigin(), testPos, 255, 0, 0, 0, 0.5f );
		NDebugOverlay::Cross3D( m_vecSavedJump, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, true, 0.5f );
	} 
	
	// Look below
	AI_TraceHull( GetAbsOrigin(), testPos, NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	//See if we're about to contact, or have already contacted the ground
	if ( ( tr.fraction != 1.0f ) || ( GetFlags() & FL_ONGROUND ) )
	{
		int	sequence = SelectWeightedSequence( (Activity)ACT_ASW_ALIEN_LAND );

		if ( GetSequence() != sequence )
		{
			VacateStrategySlot();
			SetIdealActivity( (Activity) ACT_ASW_ALIEN_LAND );

			CreateDust( false );
			EmitSound( "ASW_Drone.Land" );

			// asw todo: make the alien attack here?
			//if ( GetEnemy() && GetEnemy()->IsPlayer()  )
			//{
				//CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );

				//if ( pPlayer && pPlayer->IsInAVehicle() == false )
					 //MeleeAttack( ANTLION_MELEE1_RANGE, sk_antlion_swipe_damage.GetFloat(), QAngle( 4.0f, 0.0f, 0.0f ), Vector( -250.0f, 1.0f, 1.0f ) );
			//}

			SetAbsVelocity( GetAbsVelocity() * 0.33f );
			return false;
		}

		return IsActivityFinished();
	}

	return false;
}

void CASW_Alien_Jumper::CreateDust( bool placeDecal )
{
	trace_t	tr;
	AI_TraceLine( GetAbsOrigin()+Vector(0,0,1), GetAbsOrigin()-Vector(0,0,64), MASK_SOLID_BRUSHONLY | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0f )
	{
		surfacedata_t *pdata = physprops->GetSurfaceData( tr.surface.surfaceProps );

		if ( ( pdata->game.material == CHAR_TEX_CONCRETE ) || 
			 ( pdata->game.material == CHAR_TEX_DIRT ) ||
			 ( pdata->game.material == CHAR_TEX_SAND ) )
		{
			UTIL_CreateAntlionDust( tr.endpos + Vector(0,0,24), GetAbsAngles() );
			
			if ( placeDecal )
			{
				UTIL_DecalTrace( &tr, "Antlion.Unburrow" );
			}
		}
	}
}

void CASW_Alien_Jumper::BuildScheduleTestBits( void )
{
	//Don't allow any modifications when scripted
	if ( m_NPCState == NPC_STATE_SCRIPT )
		return;

	BaseClass::BuildScheduleTestBits();

	//Make sure we interrupt a run schedule if we can jump
	if ( IsCurSchedule(SCHED_CHASE_ENEMY) && GetNavType() == NAV_GROUND )
	{
		SetCustomInterruptCondition( COND_ASW_ALIEN_CAN_JUMP );
		SetCustomInterruptCondition( COND_ENEMY_UNREACHABLE );
	}

	
	//Interrupt any schedule unless already fleeing, burrowing, burrowed, or unburrowing.
	if(!IsCurSchedule(SCHED_ASW_ALIEN_JUMP)					&&	
		( GetFlags() & FL_ONGROUND ) )
	{
		// Only do these if not jumping as well
		if (!IsCurSchedule(SCHED_ASW_ALIEN_JUMP))
		{
			if ( GetEnemy() == NULL )
			{
				SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
			}
			//if ( GetNavType() != NAV_JUMP )
				 //SetCustomInterruptCondition( COND_ANTLION_RECEIVED_ORDERS );
		}

		//SetCustomInterruptCondition( COND_ANTLION_ON_NPC );
	}
}

void CASW_Alien_Jumper::PrescheduleThink( void )
{
	//New Enemy? Try to jump at him.
	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		m_flJumpTime = 0.0f;
	}

	if ( ShouldJump() )
	{
		SetCondition( COND_ASW_ALIEN_CAN_JUMP );
	}
	else
	{
		ClearCondition( COND_ASW_ALIEN_CAN_JUMP );
	}

	BaseClass::PrescheduleThink();
}

void CASW_Alien_Jumper::InputDisableJump( inputdata_t &inputdata )
{
	m_bDisableJump = true;
	CapabilitiesRemove( bits_CAP_MOVE_JUMP );
}

void CASW_Alien_Jumper::InputEnableJump( inputdata_t &inputdata )
{
	m_bDisableJump = false;
	CapabilitiesAdd( bits_CAP_MOVE_JUMP );
}

bool CASW_Alien_Jumper::IsJumping()
{	
	return IsCurSchedule(SCHED_ASW_ALIEN_JUMP);
}

// make this alien jump off the head of the ent he's standing on
bool CASW_Alien_Jumper::DoJumpOffHead()
{
	//Too soon to try to jump
	if ( m_flJumpTime > gpGlobals->curtime )
		return false;

	if (!(GetGroundEntity()) || GetNavType() == NAV_JUMP )
		return false;

	// force this drone to have jumping capabilities
	m_bDisableJump = false;
	CapabilitiesAdd( bits_CAP_MOVE_JUMP );

	//Vector vecDest = RandomVector(-1, 1);
	//vecDest.z = 0;
	//vecDest *= random->RandomFloat(30, 100);
	Vector vecDest;
	AngleVectors(GetAbsAngles(), &vecDest);
	vecDest *= 200.0f;
	vecDest += GetAbsOrigin();

	// Try the jump
	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_JUMP, GetAbsOrigin(), vecDest, MASK_NPCSOLID, NULL, &moveTrace );

	//See if it succeeded
	if ( IsMoveBlocked( moveTrace.fStatus ) )
	{
		if ( asw_debug_aliens.GetInt() == 2 )
		{
			NDebugOverlay::Box( vecDest, GetHullMins(), GetHullMaxs(), 255, 0, 0, 0, 5 );
			NDebugOverlay::Line( GetAbsOrigin(), vecDest, 255, 0, 0, 0, 5 );
		}

		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
		return false;
	}

	if ( asw_debug_aliens.GetInt() == 2 )
	{
		NDebugOverlay::Box( vecDest, GetHullMins(), GetHullMaxs(), 0, 255, 0, 0, 5 );
		NDebugOverlay::Line( GetAbsOrigin(), vecDest, 0, 255, 0, 0, 5 );
	}

	//Save this jump in case the next time fails	
	m_vecSavedJump = moveTrace.vJumpVelocity;
	m_vecLastJumpAttempt = vecDest;
	SetSchedule(SCHED_ASW_ALIEN_JUMP);
	m_bForcedStuckJump = true;

	return true;
}

bool CASW_Alien_Jumper::DoJumpTo(Vector &vecDest)
{	
	//Too soon to try to jump
	if ( m_flJumpTime > gpGlobals->curtime )
		return false;

	// only jump if you're on the ground
  	if (!(GetFlags() & FL_ONGROUND) || GetNavType() == NAV_JUMP )
		return false;

	// Don't jump if I'm not allowed
	if ( ( CapabilitiesGet() & bits_CAP_MOVE_JUMP ) == false )
		return false;

	// Try the jump
	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_JUMP, GetAbsOrigin(), vecDest, MASK_NPCSOLID, NULL, &moveTrace );

	//See if it succeeded
	if ( IsMoveBlocked( moveTrace.fStatus ) )
	{
		if ( asw_debug_aliens.GetInt() == 2 )
		{
			NDebugOverlay::Box( vecDest, GetHullMins(), GetHullMaxs(), 255, 0, 0, 0, 5 );
			NDebugOverlay::Line( GetAbsOrigin(), vecDest, 255, 0, 0, 0, 5 );
		}

		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
		return false;
	}

	if ( asw_debug_aliens.GetInt() == 2 )
	{
		NDebugOverlay::Box( vecDest, GetHullMins(), GetHullMaxs(), 0, 255, 0, 0, 5 );
		NDebugOverlay::Line( GetAbsOrigin(), vecDest, 0, 255, 0, 0, 5 );
	}

	//Save this jump in case the next time fails	
	m_vecSavedJump = moveTrace.vJumpVelocity;
	m_vecLastJumpAttempt = vecDest;
	SetSchedule(SCHED_ASW_ALIEN_JUMP);
	m_bForcedStuckJump = true;

	//Msg("Drone saving jump vec %f %f %f\n", m_vecSavedJump.x, m_vecSavedJump.y, m_vecSavedJump.z);

	return true;
}

bool CASW_Alien_Jumper::DoForcedJump( Vector &vecVelocity )
{
	//Too soon to try to jump
	if ( m_flJumpTime > gpGlobals->curtime )
		return false;

	// only jump if you're on the ground
	if (!(GetFlags() & FL_ONGROUND) || GetNavType() == NAV_JUMP )
		return false;

	// Don't jump if I'm not allowed
	if ( ( CapabilitiesGet() & bits_CAP_MOVE_JUMP ) == false )
		return false;

	m_vecSavedJump = vecVelocity;
	m_vecLastJumpAttempt = GetAbsOrigin() + vecVelocity * 100;
	SetSchedule(SCHED_ASW_ALIEN_JUMP);
	m_bForcedStuckJump = true;

	return true;
}	

void CASW_Alien_Jumper::WaitAndRetryJump(Vector &vecDest)
{
	m_vecLastJumpAttempt = vecDest;
	SetSchedule(SCHED_ASW_WAIT_AND_RETRY_JUMP);
}

AI_BEGIN_CUSTOM_NPC( asw_alien_jumper, CASW_Alien_Jumper )
	//Conditions
	DECLARE_CONDITION( COND_ASW_ALIEN_CAN_JUMP )
	//Squad slots
	DECLARE_SQUADSLOT( SQUAD_SLOT_ALIEN_JUMP )
	//Tasks
	DECLARE_TASK( TASK_ASW_ALIEN_JUMP )
	DECLARE_TASK( TASK_ASW_ALIEN_FACE_JUMP )
	DECLARE_TASK( TASK_ASW_ALIEN_RETRY_JUMP )
	//Activities
	DECLARE_ACTIVITY( ACT_ASW_ALIEN_JUMP_START )
	DECLARE_ACTIVITY( ACT_ASW_ALIEN_LAND )
	//Events
	DECLARE_ANIMEVENT( AE_ASW_ALIEN_START_JUMP )
	DECLARE_ANIMEVENT( AE_ASW_ALIEN_GLIDE )

	//Schedules

	//==================================================
	// Jump
	//==================================================

	DEFINE_SCHEDULE
	(
		SCHED_ASW_ALIEN_JUMP,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_ASW_ALIEN_FACE_JUMP		0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_ASW_ALIEN_JUMP_START"
		"		TASK_ASW_ALIEN_JUMP				0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_WAIT_AND_RETRY_JUMP,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT_RANDOM				1"
		"		TASK_ASW_ALIEN_RETRY_JUMP		0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)	
AI_END_CUSTOM_NPC()