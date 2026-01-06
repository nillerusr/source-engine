//================= Copyright LOLOLOLOL, All rights reserved. ==================//
//
// Purpose: Surrender behavior component which allows NPCs to be detained. Partially based on ai_behavior_fear.
// 
// Author: Blixibon, 1upD
//
//=============================================================================//

#include "cbase.h"
#include "ai_motor.h"
#include "ai_behavior_surrender.h"
#include "ai_hint.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_link.h"
#include "ai_navigator.h"
#include "ai_playerally.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Custom activities
int ACT_ARREST_FLOOR;
int ACT_ARREST_FLOOR_CURIOUS;
int ACT_ARREST_FLOOR_ENTER;
int ACT_ARREST_FLOOR_ENTER_PANICKED;
int ACT_ARREST_FLOOR_EXIT;
int ACT_ARREST_STANDING_GESTURE;

// Citizen activities
extern int ACT_CROUCH_PANICKED;
extern int ACT_WALK_PANICKED;
extern int ACT_RUN_PANICKED;

extern int ACT_CIT_HEAL;

BEGIN_DATADESC( CAI_SurrenderBehavior )
	DEFINE_FIELD( m_bCanSurrender, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_bSurrendered, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bJustSurrendered, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bCancelSurrender, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bShouldBeStanding, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOnGround, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUsingCustomBounds, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iSurrenderFlags, FIELD_INTEGER ),

	DEFINE_FIELD( m_flSurrenderIdleTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSurrenderIdleNextCheck, FIELD_TIME ),

	DEFINE_FIELD( m_iResistanceValue, FIELD_INTEGER ),

	DEFINE_EMBEDDED( m_SafePlaceMoveMonitor ),
	DEFINE_FIELD( m_hSafePlaceHint, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hMovingToHint, FIELD_EHANDLE ),

	DEFINE_FIELD( m_hCaptor, FIELD_EHANDLE ),
END_DATADESC();

#define SURRENDER_IDLE_CHECK_INTERVAL		5
#define SURRENDER_SAFE_PLACE_TOLERANCE		36.0f
#define SURRENDER_ENEMY_TOLERANCE_CLOSE_DIST_SQR		Square(300.0f) // (25 feet)
#define SURRENDER_ENEMY_TOLERANCE_TOO_CLOSE_DIST_SQR	Square( 60.0f ) // (5 Feet)

ConVar ai_enable_surrender_behavior( "ai_enable_surrender_behavior", "1" );
ConVar ai_debug_surrender_behavior( "ai_debug_surrender_behavior", "0" );

ConVar ai_surrender_safe_spot_dist( "ai_surrender_safe_spot_dist", "1000", FCVAR_NONE, "The maximum distance for safe spots." );
ConVar ai_surrender_safe_spot_enemy_dist( "ai_surrender_safe_spot_enemy_dist", "360", FCVAR_NONE, "The distance an enemy has to be from a potential safe spot in order to spoil it." );
ConVar ai_surrender_safe_spot_hint_dist_multiplier( "ai_surrender_safe_spot_hint_dist_multiplier", "0.5", FCVAR_NONE, "The multiplier for the distance to a hint, making hints closer than they appear." );

ConVar ai_surrender_resist_base_time( "ai_surrender_resist_base_time", "30", FCVAR_NONE, "The amount of time before a NPC automatically resists. This is divided by their resistance value." );
ConVar ai_surrender_potential_captor_dist( "ai_surrender_potential_captor_dist", "500", FCVAR_NONE, "Maximum distance for a NPC or player to be considered a potential captor." );
ConVar ai_surrender_potential_rescuer_dist( "ai_surrender_potential_rescuer_dist", "300", FCVAR_NONE, "Maximum distance for a NPC or player to be considered a potential rescuer." );

ConVar ai_surrender_make_auto_after_first_surrender( "ai_surrender_make_auto_after_first_surrender", "1", FCVAR_NONE, "Determines whether or not to turn on automatic surrender for NPCs after they've been manually ordered to surrender, which means they can surrender again automatically if they resist later." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_SurrenderBehavior::CAI_SurrenderBehavior()
{
	ReleaseAllHints();
	m_SafePlaceMoveMonitor.ClearMark();

	m_bCanSurrender = true;
	m_flSurrenderIdleTime = FLT_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::Precache( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SURRENDER_AT_SAFE_PLACE:
		// We've arrived!
		if (m_hMovingToHint)
		{
			// Lock the hint and set the marker. we're safe for now.
			m_hSafePlaceHint = m_hMovingToHint;
			m_hSafePlaceHint->Lock( GetOuter() );
			m_SafePlaceMoveMonitor.SetMark( GetOuter(), SURRENDER_SAFE_PLACE_TOLERANCE );
			m_hSafePlaceHint->NPCStartedUsing( GetOuter() );
		}
		else
		{
			// Just set the marker if there's no hint
			m_SafePlaceMoveMonitor.SetMark( GetOuter(), SURRENDER_SAFE_PLACE_TOLERANCE );
		}
		TaskComplete();
		break;

	case TASK_SURRENDER_GET_PATH_TO_SAFE_PLACE:
		// Using TaskInterrupt() optimizations. See RunTask().
		break;

	case TASK_SURRENDER_IDLE:
	case TASK_SURRENDER_IDLE_LOOP:
		if (m_flSurrenderIdleTime == FLT_MAX)
		{
			m_flSurrenderIdleTime = gpGlobals->curtime;
			m_flSurrenderIdleNextCheck = gpGlobals->curtime + SURRENDER_IDLE_CHECK_INTERVAL;
		}

		if (IsCurSchedule( SCHED_SURRENDER_FLOOR_IDLE, false ))
			m_bOnGround = true;

		break;

	case TASK_SURRENDER_SET_CUSTOM_BOUNDS:
		ComputeAndSetRenderBounds();
		TaskComplete();
		break;

	case TASK_SURRENDER_RESET_BOUNDS:
		ResetBounds();
		TaskComplete();
		break;

	case TASK_SURRENDER_FACE_CAPTOR:
		if (m_hCaptor && m_hCaptor->IsPlayer())
		{
			//GetOuter()->AddFacingTarget( m_hCaptor, 5.0f, 3.0f );
			//TaskComplete();

			SetTarget( m_hCaptor );
			ChainStartTask( TASK_FACE_TARGET );
		}
		else
		{
			TaskComplete();
		}
		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SURRENDER_GET_PATH_TO_SAFE_PLACE:
		{
			switch( GetOuter()->GetTaskInterrupt() )
			{
			case 0:// Find the hint node
				{
					ReleaseAllHints();
					CAI_Hint *pHint = NULL;
					Vector vecDest, vecDir;
					FindSurrenderDest( &pHint, vecDest, vecDir );

					if (ai_debug_surrender_behavior.GetBool())
					{
						if (pHint)
							NDebugOverlay::HorzArrow( GetAbsOrigin(), pHint->GetAbsOrigin(), 8, 0, 255, 255, 64, true, 5.0f );
						else
							NDebugOverlay::HorzArrow( GetAbsOrigin(), vecDest, 8, 0, 255, 0, 64, true, 5.0f );
					}

					if( pHint == NULL )
					{
						if (vecDest != vec3_invalid)
						{
							if (GetNavigator()->SetGoal( vecDest ))
							{
								if (ai_debug_surrender_behavior.GetBool())
									Msg( "	%s (%i): Setting non-hint safe place goal\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

								GetNavigator()->SetArrivalDirection( vecDir );
								GetOuter()->TaskComplete();
							}
							else
							{
								if (ai_debug_surrender_behavior.GetBool())
									Warning( "	%s (%i): Failed to set non-hint safe place goal\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
							}

							m_hMovingToHint.Set( NULL );
						}
						else
						{
							//SetCondition( COND_SURRENDER_ANYWHERE ); // For now, surrender here
							//TaskFail("Surrender: Couldn't find hint node\n");
							GetOuter()->TaskComplete();
						}
					}
					else
					{
						m_hMovingToHint.Set( pHint );
						GetOuter()->TaskInterrupt();
					}
				}
				break;

			case 1:// Do the pathfinding.
				{
					if (m_hMovingToHint)
					{
						AI_NavGoal_t goal(m_hMovingToHint->GetAbsOrigin());
						goal.pTarget = NULL;
						if (GetNavigator()->SetGoal( goal ) == false)
						{
							m_hMovingToHint.Set( NULL );

							if (ai_debug_surrender_behavior.GetBool())
								Warning( "	%s (%i): Failed to set hint safe place goal\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
						}
						else
						{
							m_hMovingToHint->NPCHandleStartNav( GetOuter(), true );

							if (ai_debug_surrender_behavior.GetBool())
								Msg( "	%s (%i): Setting hint safe place goal\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
						}
					}
				}
				break;
			}
		}
		break;

	case TASK_SURRENDER_IDLE:
		TaskComplete();
	case TASK_SURRENDER_IDLE_LOOP:
		if (m_flSurrenderIdleNextCheck < gpGlobals->curtime)
		{
			RefreshSurrenderIdle();
			m_flSurrenderIdleNextCheck = gpGlobals->curtime + SURRENDER_IDLE_CHECK_INTERVAL;

			// Suggest idle state after 10 seconds
			if (gpGlobals->curtime - m_flSurrenderIdleTime > 10.0f)
				GetOuter()->SetIdealState( NPC_STATE_IDLE );
		}
		break;

	case TASK_SURRENDER_FACE_CAPTOR:
		if (m_hCaptor && m_hCaptor->IsPlayer())
		{
			ChainRunTask( TASK_FACE_TARGET );
		}
		else
		{
			TaskComplete();
		}
		break;

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::ShouldPlayerAvoid( void )
{
	if ( IsSurrenderMovingToIdle() || IsSurrenderIdle() || IsCurSchedule( SCHED_SURRENDER_FLOOR_ENTER, false ) || IsCurSchedule( SCHED_SURRENDER_FLOOR_EXIT, false ) )
	{
		return true;
	}

	return BaseClass::ShouldPlayerAvoid();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::IsNavigationUrgent( void )
{
	if ( IsCurSchedule( SCHED_SURRENDER_MOVE_TO_SAFE_PLACE, false ) )
	{
		// HACKHACK: There seems to be some issues with the way NPCs navigate in places like c6_2 unless urgent navigation is enabled?
		// Do this for now until we figure out why that happens
		return true;
	}

	return BaseClass::IsNavigationUrgent();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "cansurrender" ) )
	{
		m_bCanSurrender = (atoi( szValue ) != 0);
		return true;
	}
	else if ( FStrEq( szKeyName, "surrenderstate" ) )
	{
		switch (atoi( szValue ))
		{
			// 0 = Not surrendered

			// 1 = Start surrendered (standing)
			case 1:
				SetCondition( COND_SURRENDER_ANYWHERE ); // Surrender where the NPC spawns
				m_bShouldBeStanding = true;
				Surrender( NULL );
				break;

			// 2 = Start surrendered (on ground)
			case 2:
				SetCondition( COND_SURRENDER_ANYWHERE ); // Surrender where the NPC spawns
				Surrender( NULL );
				break;
		}
	}
	else if ( FStrEq( szKeyName, "surrenderflags" ) )
	{
		m_iSurrenderFlags = atoi( szValue );
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	if ( !m_bSurrendered )
		return;

	criteriaSet.AppendCriteria( "surrendered", "1" );

	criteriaSet.AppendCriteria( "surrendered_resistance_value", UTIL_VarArgs( "%i", m_iResistanceValue ) );
}

//-----------------------------------------------------------------------------
// This place is definitely no longer safe. Stop picking it for a while.
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::MarkAsUnsafe()
{
	Assert( m_hSafePlaceHint );

	// Disable the node to stop anyone from picking it for a while.
	m_hSafePlaceHint->DisableForSeconds( 5.0f );
}

//-----------------------------------------------------------------------------
// Am I in safe place from my enemy?
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::IsInASafePlace()
{
	// Bad Cop said this place is safe, whether you like it or not
	if ( !HasCondition( COND_SURRENDER_ANYWHERE ) )
	{
		// No safe place in mind.
		if( !m_SafePlaceMoveMonitor.IsMarkSet() )
			return false;

		// I have a safe place, but I'm not there.
		if( m_SafePlaceMoveMonitor.TargetMoved(GetOuter()) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::SpoilSafePlace()
{
	m_SafePlaceMoveMonitor.ClearMark();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::ReleaseAllHints()
{
	if( m_hSafePlaceHint )
	{
		// If I have a safe place, unlock it for others.
		m_hSafePlaceHint->Unlock();
		m_hSafePlaceHint->NPCStoppedUsing(GetOuter());

		// Don't make it available right away. I probably left for a good reason.
		// We also don't want to oscillate
		m_hSafePlaceHint->DisableForSeconds( 4.0f );
		m_hSafePlaceHint = NULL;
	}

	if( m_hMovingToHint )
	{
		m_hMovingToHint->Unlock();
		m_hMovingToHint = NULL;
	}

	m_SafePlaceMoveMonitor.ClearMark();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::ComputeAndSetRenderBounds()
{
	if (!m_bUsingCustomBounds)
	{
		Vector mins, maxs;
		if ( GetOuter()->ComputeHitboxSurroundingBox( &mins, &maxs ) )
		{
			UTIL_SetSize( GetOuter(), mins - GetAbsOrigin(), maxs - GetAbsOrigin());
			if ( GetOuter()->VPhysicsGetObject() )
			{
				GetOuter()->SetupVPhysicsHull();
			}
			m_bUsingCustomBounds = true;

			// For now, don't allow surrendered NPCs to fall
			// (fixes falling through displacements)
			// TODO: Cover cases where citizens surrender on moveable ground?
			GetOuter()->AddFlag( FL_FLY );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::ResetBounds()
{
	if ( m_bUsingCustomBounds )
	{
		GetOuter()->SetHullSizeNormal( true );
		GetOuter()->RemoveFlag( FL_FLY );
		m_bUsingCustomBounds = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
// Notes  : This behavior runs when I have an enemy that I fear, but who
//			does NOT hate or fear me (meaning they aren't going to fight me)
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::CanSelectSchedule()
{
	if ( !m_bSurrendered )
		return false;

	if( !GetOuter()->IsInterruptable() && GetOuter()->GetRunningBehavior() != this )
		return false;

	//if( !HasCondition(COND_SEE_PLAYER) )
	//	return false;

	if( !ai_enable_surrender_behavior.GetBool() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_SURRENDER_ENEMY_CLOSE );
	ClearCondition( COND_SURRENDER_ENEMY_TOO_CLOSE );
	if( GetEnemy() )
	{
		float flEnemyDistSqr = GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin());

		if( flEnemyDistSqr < SURRENDER_ENEMY_TOLERANCE_TOO_CLOSE_DIST_SQR )
		{
			SetCondition( COND_SURRENDER_ENEMY_TOO_CLOSE );
			if( IsInASafePlace() )
			{
				SpoilSafePlace();
			}
		}
		else if( flEnemyDistSqr < SURRENDER_ENEMY_TOLERANCE_CLOSE_DIST_SQR && GetEnemy()->GetEnemy() == GetOuter() )	
		{
			// Only become scared of an enemy at this range if they're my enemy, too
			SetCondition( COND_SURRENDER_ENEMY_CLOSE );
			if( IsInASafePlace() )
			{
				SpoilSafePlace();
			}
		}
	}

	// TODO: Check for COND_SURRENDER_CANCEL?
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	bool bShouldPlaySurrenderLayer = m_bSurrendered;

	if (m_bCancelSurrender)
	{
		if (!m_bOnGround)
		{
			// Un-surrender
			m_bSurrendered = false;
			bShouldPlaySurrenderLayer = false;
		}
	}

	Activity curActivity = GetActivity();

	if (curActivity == ACT_ARREST_FLOOR_ENTER ||
		curActivity == ACT_ARREST_FLOOR_ENTER_PANICKED ||
		curActivity == ACT_ARREST_FLOOR ||
		curActivity == ACT_ARREST_FLOOR_EXIT ||
		curActivity == ACT_ARREST_FLOOR_CURIOUS ||
		curActivity == ACT_CIT_HEAL ||
		m_hSafePlaceHint && m_hSafePlaceHint->HintActivityName() != NULL_STRING)
	{
		bShouldPlaySurrenderLayer = false;
	}

	if (bShouldPlaySurrenderLayer)
	{
		// Check the surrendering layer
		int iIndex = GetOuter()->FindGestureLayer( (Activity)ACT_ARREST_STANDING_GESTURE );
		if (iIndex == -1)
		{
			//Msg( "Adding gesture layer\n" );

			iIndex = GetOuter()->AddGesture( (Activity)ACT_ARREST_STANDING_GESTURE );
			GetOuter()->SetLayerLooping( iIndex, true );

			GetOuter()->SetLayerBlendIn( iIndex, 0.1f );
			GetOuter()->SetLayerBlendOut( iIndex, 0.1f );
		}

		// HACKHACK: Refresh the cycle each think
		if (GetOuter()->GetLayerCycle(iIndex) >= 0.5f)
			GetOuter()->SetLayerCycle( iIndex, 0.5f );
	}
	else
	{
		// Remove the surrendering layer
		int iIndex = GetOuter()->FindGestureLayer( (Activity)ACT_ARREST_STANDING_GESTURE );
		if (iIndex != -1)
		{
			//Msg( "Removing gesture layer\n" );

			if (GetOuter()->GetLayerCycle( iIndex ) < 0.89f)
			{
				if (curActivity == ACT_CIT_HEAL)
					GetOuter()->SetLayerCycle( iIndex, 0.95f ); // Give it a head start
				else
					GetOuter()->SetLayerCycle( iIndex, 0.89f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::BeginScheduleSelection()
{
	if( m_hSafePlaceHint )
	{
		// We think we're safe. Is it true?
		if( !IsInASafePlace() )
		{
			// no! So mark it so.
			ReleaseAllHints();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::EndScheduleSelection()
{
	// We don't have to release our hints or markers or anything here. 
	// Just because we ran other AI for a while doesn't mean we aren't still in a safe place.
	//ReleaseAllHints();
}




//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
// Notes  : If fear behavior is running at all, we know we're afraid of our enemy
//-----------------------------------------------------------------------------
int CAI_SurrenderBehavior::SelectSchedule()
{
	bool bInterrupted = (HasCondition( COND_HEAR_DANGER ) || HasCondition( COND_SURRENDER_ENEMY_TOO_CLOSE ) || HasCondition( COND_SURRENDER_ENEMY_CLOSE ) /*|| HasCondition( COND_BETTER_WEAPON_AVAILABLE )*/);
	if (m_bOnGround /*&& m_bShouldBeStanding*/)
	{
		if (!bInterrupted && !HasCondition( COND_SURRENDER_STATE_CHANGE ) && !m_bCancelSurrender)
		{
			// Stay on the ground, but something interrupted us; Play the curious animation
			//Msg("Staying on ground\n");
			Speak( TLK_SURRENDER_BEHAVIOR_STARTLE );
			return SCHED_SURRENDER_FLOOR_CURIOUS;
		}
		else
		{
			// Get up before doing anything else
			//Msg( "Interrupted: %d, State change: %d, Cancel: %d\n", bInterrupted, HasCondition( COND_SURRENDER_STATE_CHANGE ), m_bCancelSurrender );
			m_bOnGround = false;
			return SCHED_SURRENDER_FLOOR_EXIT;
		}
	}

	ClearCondition( COND_SURRENDER_STATE_CHANGE );
	m_flSurrenderIdleTime = FLT_MAX;

	if (!m_bCancelSurrender)
	{
		bool bInSafePlace = IsInASafePlace();

		if (!bInterrupted)
		{
			if (!bInSafePlace)
			{
				// Always move to a safe place if we're not running from a danger sound
				//return SCHED_SURRENDER_MOVE_TO_SAFE_PLACE;

				//if (InCombatZone())
				//{
				//	// 
				//}
				//else
				if (m_bJustSurrendered) //(HasCondition( COND_SURRENDER_INITIAL ))
				{
					// TODO: Don't do this in intense combat?
					m_bJustSurrendered = false; //ClearCondition( COND_SURRENDER_INITIAL );
					SetTarget( m_hCaptor );
					Speak( TLK_SURRENDER_BEHAVIOR_START );
					return SCHED_SURRENDER_FACE_CAPTOR;
				}
				else
				{
					return SCHED_SURRENDER_MOVE_TO_SAFE_PLACE;
				}
			}
			else
			{
				// We ARE in a safe place
				m_bJustSurrendered = false; //ClearCondition( COND_SURRENDER_INITIAL );
				return SCHED_SURRENDER_STANDING_IDLE;
			}
		}
	}
	else
	{
		// Un-surrender
		m_bSurrendered = false;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAI_SurrenderBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CAI_SurrenderBehavior::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_SURRENDER_STANDING_IDLE:
		if ( IsInASafePlace() && !m_bShouldBeStanding )
		{
			if (m_hSafePlaceHint)
			{
				// If this hint doesn't have a custom activity, get on the ground
				if (m_hSafePlaceHint->HintActivityName() == NULL_STRING)
					return SCHED_SURRENDER_FLOOR_ENTER;
			}
			else
			{
				// Check if there's enough room to get down
				Vector vecMins = GetOuter()->GetHullMins();
				Vector vecMaxs = GetOuter()->GetHullMaxs();

				trace_t tr;
				AI_TraceHull( GetAbsOrigin(), GetAbsOrigin(), vecMins, vecMaxs, MASK_NPCSOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );
				if (!tr.startsolid)
				{
					//Msg("Entering floor idle\n");
					return SCHED_SURRENDER_FLOOR_ENTER;
				}
				else
				{
					//NDebugOverlay::Box( GetAbsOrigin(), vecMins, vecMaxs, 255, 0, 0, 128, 4.0f );
					Msg("Can't enter floor idle\n");
				}
			}
		}
		break;

	case SCHED_MOVE_AWAY:
		return SCHED_SURRENDER_MOVE_AWAY;

	case SCHED_INVESTIGATE_SOUND:
		return SCHED_ALERT_FACE_BESTSOUND;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::IsInterruptable( void )
{
	//if (m_bSurrendered)
	//	return false;

	if (IsCurSchedule( SCHED_SURRENDER_FLOOR_ENTER, false ) || IsCurSchedule( SCHED_SURRENDER_FLOOR_EXIT, false ))
		return false;

	return BaseClass::IsInterruptable();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::CanSurrender()
{
	if (!m_bCanSurrender)
		return false;

	if (GetNpcState() == NPC_STATE_SCRIPT || GetOuter()->IsInAScript())
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::Surrender( CBaseCombatCharacter *pCaptor )
{
	m_bSurrendered = true;
	m_hCaptor = pCaptor;
	m_bCancelSurrender = false;
	ClearCondition( COND_SURRENDER_CANCEL );
	NotifyChangeBehaviorStatus();

	if (ai_surrender_make_auto_after_first_surrender.GetBool())
		m_iSurrenderFlags |= SURRENDER_FLAGS_SURRENDER_AUTOMATICALLY;

	m_bJustSurrendered = true; //SetCondition( COND_SURRENDER_INITIAL );

	SetCondition( COND_RECEIVED_ORDERS ); // Interrupts running schedules

	GetOuter()->FireNamedOutput( "OnSurrender", variant_t(), pCaptor, GetOuter(), 0.0f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::StopSurrendering()
{
	m_bCancelSurrender = true;
	SetCondition( COND_SURRENDER_CANCEL );
	SpoilSafePlace();

	GetOuter()->FireNamedOutput( "OnStopSurrendering", variant_t(), m_hCaptor, GetOuter(), 0.0f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::IsSurrenderIdle()
{
	return IsCurSchedule( SCHED_SURRENDER_FLOOR_IDLE, false ) || IsCurSchedule( SCHED_SURRENDER_STANDING_IDLE, false );
}

bool CAI_SurrenderBehavior::IsSurrenderIdleStanding()
{
	return IsCurSchedule( SCHED_SURRENDER_STANDING_IDLE, false );
}

bool CAI_SurrenderBehavior::IsSurrenderIdleOnGround()
{
	return IsCurSchedule( SCHED_SURRENDER_FLOOR_IDLE, false ) || IsCurSchedule( SCHED_SURRENDER_FLOOR_CURIOUS, false );
}

bool CAI_SurrenderBehavior::IsSurrenderMovingToIdle()
{
	return IsCurSchedule( SCHED_SURRENDER_MOVE_TO_SAFE_PLACE, false );
}

void CAI_SurrenderBehavior::SurrenderStand()
{
	if (!m_bShouldBeStanding)
	{
		m_bShouldBeStanding = true;
		SetCondition( COND_SURRENDER_STATE_CHANGE );
	}
}

void CAI_SurrenderBehavior::SurrenderToGround()
{
	if (m_bShouldBeStanding)
	{
		m_bShouldBeStanding = false;
		SetCondition( COND_SURRENDER_STATE_CHANGE );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseCombatCharacter *CAI_SurrenderBehavior::GetNearestPotentialCaptor( bool bIncludingPlayer, bool bFirst, int *pNumCaptors, bool *bCanBeSeen )
{
	// Check if there's any Combine units who are nearby or can see me
	Vector vecOrigin = GetAbsOrigin();
	float flMaxDistSqr = Square( ai_surrender_potential_captor_dist.GetFloat() );
	float flBestDistSqr = Square( ai_surrender_potential_captor_dist.GetFloat() );
	CBaseCombatCharacter *pBestCaptor = NULL;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();
	int nCaptors = 0;
	int nHarmlessCaptors = 0;

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if ( pNPC->IsAlive() )
		{
			// TODO: Affiliation check
			bool bCombine = false;
			Class_T classify = pNPC->Classify();
			switch (classify)
			{
			case CLASS_COMBINE:
			case CLASS_COMBINE_HUNTER:
			case CLASS_COMBINE_GUNSHIP:
			case CLASS_MANHACK:
			case CLASS_METROPOLICE:
			case CLASS_MILITARY:
			case CLASS_PROTOSNIPER:
			case CLASS_SCANNER:
			case CLASS_ARBEIT_TECH: // Arbeit turrets can be used. Oh, and Wilson too
			case CLASS_COMBINE_NEMESIS:
				bCombine = true;
				break;
			}

			if (!bCombine)
				continue;

			// Now check their distance
			Vector vecToNPC = (vecOrigin - pNPC->GetAbsOrigin());
			float flDistSqr = vecToNPC.LengthSqr();

			bool bValid = flDistSqr < flMaxDistSqr;
			if (pNPC->FInViewCone(GetOuter()) && pNPC->FVisible( GetOuter() ))
			{
				if (bCanBeSeen)
					*bCanBeSeen = true;
				bValid = true;
			}

			if (bValid)
			{
				// If it can't be an enemy, only count it if we have no other captors
				// (handles Wilson, turrets with no ammo, etc.)
				if (!GetOuter()->IsValidEnemy( pNPC ))
					nHarmlessCaptors++;
				else
					nCaptors++;

				if (bFirst)
					return pNPC;

				if (flDistSqr < flBestDistSqr)
				{
					pBestCaptor = pNPC;
					flBestDistSqr = flDistSqr;
				}
			}
		}
	}

	// TODO: Check for player's affiliation before surrender
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (bIncludingPlayer && pPlayer && pPlayer->IsAlive() && !(pPlayer->GetFlags() & FL_NOTARGET))
	{
		// Now check their distance
		Vector vecToNPC = (vecOrigin - pPlayer->GetAbsOrigin());
		float flDistSqr = vecToNPC.LengthSqr();

		bool bValid = flDistSqr < flMaxDistSqr;
		if (pPlayer->FInViewCone( GetOuter() ) && pPlayer->FVisible( GetOuter() ))
		{
			if (bCanBeSeen)
				*bCanBeSeen = true;
			bValid = true;
		}

		if (bValid)
		{
			nCaptors++;
			if (flDistSqr < flBestDistSqr)
			{
				pBestCaptor = pPlayer;
				flBestDistSqr = flDistSqr;
			}
		}
	}

	if (nCaptors <= 0)
		nCaptors = nHarmlessCaptors;

	if (pNumCaptors)
		*pNumCaptors = nCaptors;

	return pBestCaptor;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseCombatCharacter *CAI_SurrenderBehavior::GetNearestPotentialRescuer( bool bIncludingPlayer, bool bFirst, int *pNumRescuers )
{
	// Check if there's any citizens/resistance units who are nearby or can see me
	Vector vecOrigin = GetAbsOrigin();
	float flMaxDistSqr = Square( ai_surrender_potential_rescuer_dist.GetFloat() );
	float flBestDistSqr = Square( ai_surrender_potential_rescuer_dist.GetFloat() );
	CBaseCombatCharacter *pBestRescuer = NULL;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();
	int nRescuers = 0;

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if ( pNPC->IsAlive() )
		{
			// TODO: Affiliation check
			bool bResistance = false;
			Class_T classify = pNPC->Classify();
			switch (classify)
			{
			case CLASS_PLAYER_ALLY:
			case CLASS_PLAYER_ALLY_VITAL:
			case CLASS_CITIZEN_REBEL:
			case CLASS_HACKED_ROLLERMINE:
				bResistance = true;
				break;
			}

			if (!bResistance)
				continue;

			// Unarmed rebels can't help us, but perhaps they can motivate us to resist
			//if (classify == CLASS_PLAYER_ALLY && pNPC->GetActiveWeapon() == NULL)
			//	continue;

			// Now check their distance
			Vector vecToNPC = (vecOrigin - pNPC->GetAbsOrigin());
			float flDistSqr = vecToNPC.LengthSqr();

			bool bValid = flDistSqr < flMaxDistSqr;
			if (pNPC->FInViewCone(GetOuter()) && pNPC->FVisible( GetOuter() ))
			{
				//if (bCanBeSeen)
				//	*bCanBeSeen = true;
				bValid = true;
			}

			if (bValid)
			{
				nRescuers++;
				if (bFirst)
					return pNPC;

				if (flDistSqr < flBestDistSqr)
				{
					pBestRescuer = pNPC;
					flBestDistSqr = flDistSqr;
				}
			}
		}
	}

	// TODO: Check for player's affiliation before surrender
	/*
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (bIncludingPlayer && pPlayer)
	{
		// Now check their distance
		Vector vecToNPC = (vecOrigin - pPlayer->GetAbsOrigin());
		float flDistSqr = vecToNPC.LengthSqr();

		if (flDistSqr < flBestDistSqr || pPlayer->FVisible( GetOuter() ))
		{
			pBestRescuer = pPlayer;
			flBestDistSqr = flDistSqr;
		}
	}
	*/

	if (pNumRescuers)
		*pNumRescuers = nRescuers;

	return pBestRescuer;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::RefreshSurrenderIdle()
{
	// Calculate the value
	m_iResistanceValue = 0;

	CBaseCombatCharacter *pPotentialCaptor = NULL;
	if (CanResist())
	{
		int nCaptors = 0;
		bool bCanBeSeen = false;
		pPotentialCaptor = GetNearestPotentialCaptor(true, false, &nCaptors, &bCanBeSeen);
		if (pPotentialCaptor)
		{
			m_iResistanceValue -= nCaptors;

			if (!bCanBeSeen)
			{
				m_iResistanceValue++;

				if (ai_debug_surrender_behavior.GetBool())
					Msg( "	%s (%i): Resistance increasing due to not being seen\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
			}
			else if (IsSurrenderIdleOnGround())
			{
				// We're on the ground and they see us, so trying to resist in this state leaves us vulnerable
				m_iResistanceValue--;

				if (ai_debug_surrender_behavior.GetBool())
					Msg( "	%s (%i): Resistance decreasing due to being seen and on the ground\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
			}
		}
		else
		{
			// No potential captor gives a huge resistance bonus
			m_iResistanceValue += 3;

			if (ai_debug_surrender_behavior.GetBool())
				Msg( "	%s (%i): Resistance increasing due to lack of potential captor\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
		}

		int nRescuers = 0;
		CBaseCombatCharacter *pPotentialRescuer = GetNearestPotentialRescuer(true, false, &nRescuers);
		if (pPotentialRescuer)
		{
			// There's someone nearby who could rescue/protect me
			m_iResistanceValue += nRescuers;
		}

		if (ai_debug_surrender_behavior.GetBool())
			Msg( "	%s (%i): Potential captors: %i, Potential rescuers: %i\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), nCaptors, nRescuers );

		// They don't seem to be in control of the situation!
		if (GetEnemy())
		{
			if (ai_debug_surrender_behavior.GetBool())
				Msg( "	%s (%i): Resistance increasing due to enemy presence\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
			m_iResistanceValue++;
		}

		// If there's a weapon nearby, maybe we could get it
		if (HasCondition( COND_BETTER_WEAPON_AVAILABLE ))
		{
			if (ai_debug_surrender_behavior.GetBool())
				Msg( "	%s (%i): Resistance increasing due to available weapon(s)\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
			m_iResistanceValue++;
		}

		if (GetOuter()->GetMaxHealth() > 0)
		{
			float flHealthRatio = ((float)GetOuter()->GetHealth() / (float)GetOuter()->GetMaxHealth());

			if (flHealthRatio >= 0.75f)
				m_iResistanceValue++;
			else if (flHealthRatio <= 0.3f)
				m_iResistanceValue--;

			if (ai_debug_surrender_behavior.GetBool())
				Msg( "	%s (%i): Health ratio is %f\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), flHealthRatio );
		}

		// Use the method now in case of any derived classes (e.g. citizen willpower)
		m_iResistanceValue = ModifyResistanceValue( m_iResistanceValue );
	}

	// Calculate the multiplier
	float flMultiplier = 1.0f;
	if (m_iResistanceValue != 0)
	{
		if (m_iResistanceValue > 0)
			flMultiplier = 1.0f / (float)m_iResistanceValue;
		else
			flMultiplier = 2.0f - (1.0f / (float)(-m_iResistanceValue));
	}

	float flTimeSpentIdle = gpGlobals->curtime - m_flSurrenderIdleTime;

	if (ai_debug_surrender_behavior.GetBool())
	{
		Msg( "%s (%i): Evaluating surrender idle (time spent: %.2f; base time %.2f; resistance %i/%.2f = %f until next potential resist)\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), flTimeSpentIdle, ai_surrender_resist_base_time.GetFloat(), m_iResistanceValue, flMultiplier, (ai_surrender_resist_base_time.GetFloat() * flMultiplier) - flTimeSpentIdle );
	}

	if (flTimeSpentIdle > (ai_surrender_resist_base_time.GetFloat() * flMultiplier))
	{
		// If there's captors nearby and we don't have any resistance, there's no point in doing anything.
		if ((pPotentialCaptor || !CanResist()) && m_iResistanceValue <= 0)
		{
			/*
			if (!GetOuter()->IsInAScript() && !HasCondition( COND_IN_PVS ))
			{
				// If the player's gone but we still have a captor, we might've been left alone with Combine units who could potentially take us into custody.
				// In that scenario, we can remove ourselves under the pretense that our captors have taken us away to a secure location.
				// TODO: Better failsafes
				Msg( "Can remove self\n" );
				UTIL_Remove( GetOuter() );
			}
			else
			*/
			{
				// If we can't remove ourselves, just reset the idle time
				m_flSurrenderIdleTime = gpGlobals->curtime;
			}
		}
		else
		{
			// If that's not why, then RESIST!!!
			SetCondition( COND_SURRENDER_CANCEL );
			m_bCancelSurrender = true;

			if (ai_debug_surrender_behavior.GetBool())
			{
				Msg( "%s (%i): Can stop surrendering\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SurrenderBehavior::FindSurrenderDest( CAI_Hint **pOutHint, Vector &vecDest, Vector &vecDir )
{
#if 0
	CAI_Hint *pHint;
	CHintCriteria hintCriteria;
	CAI_BaseNPC *pOuter = GetOuter();

	Assert(pOuter != NULL);

	hintCriteria.AddHintType( HINT_PLAYER_ALLY_FEAR_DEST );
	hintCriteria.SetFlag( bits_HINT_NOT_CLOSE_TO_ENEMY | bits_HINT_NODE_USE_GROUP /*| bits_HINT_NODE_IN_VIEWCONE | bits_HINT_NPC_IN_NODE_FOV*/ );
	hintCriteria.AddIncludePosition( pOuter->GetAbsOrigin(), (ai_surrender_hint_dist.GetFloat()) );

	pHint = CAI_HintManager::FindHint( pOuter, hintCriteria );

	if( pHint )
	{
		// Reserve this node while I try to get to it. When I get there I will lock it.
		// Otherwise, if I fail to get there, the node will come available again soon.
		pHint->DisableForSeconds( 4.0f );
	}
#if 0
	else
	{
		Msg("DID NOT FIND HINT\n");
		NDebugOverlay::Cross3D( GetOuter()->WorldSpaceCenter(), 32, 255, 255, 0, false, 10.0f );
	}
#endif

	return pHint;
#endif

	// Travel to the nearest node with large permission
	Vector vecSearchOrigin = GetAbsOrigin();
	Vector vecMins = GetOuter()->GetHullMins();
	Vector vecMaxs = GetOuter()->GetHullMaxs();

	vecDest = vec3_invalid;
	vecDir = vec3_origin;

	float flNearestDistSqr = Square( ai_surrender_safe_spot_dist.GetFloat() );
	float flNearestPathSqr = Square( ai_surrender_safe_spot_dist.GetFloat() );
	const float flEnemyHintDistSqr = Square( ai_surrender_safe_spot_enemy_dist.GetFloat() );

	for (int node = 0; node < g_pBigAINet->NumNodes(); node++)
	{
		CAI_Node *pNode = g_pBigAINet->GetNode( node );
		Vector vecNodeOrigin = pNode->GetOrigin();

		float flDistSqr = (vecSearchOrigin - vecNodeOrigin).LengthSqr();

		// Previously used HINT_PLAYER_ALLY_FEAR_DEST
		CAI_Hint *pHint = pNode->GetHint();
		if (pHint && pHint->HintType() == HINT_SURRENDER_IDLE_DEST && (pHint->GetGroup() == NULL_STRING || pHint->GetGroup() == GetOuter()->GetHintGroup()))
		{
			// Reject this hint if it there's a hintgroup mismatch
			if (pHint->GetGroup() != NULL_STRING && (pHint->GetGroup() != GetOuter()->GetHintGroup()))
				continue;

			flDistSqr *= ai_surrender_safe_spot_hint_dist_multiplier.GetFloat();
		}

		if (flDistSqr > flNearestDistSqr)
			continue;

		if ( GetEnemy() )
		{
			float flDistHintToEnemySqr = vecNodeOrigin.DistToSqr( GetEnemy()->GetAbsOrigin() );

			// Reject this node if it's too close to our enemy
			if (flDistHintToEnemySqr < flEnemyHintDistSqr)
			{
				continue;
			}
		}

		// Only use ground nodes
		if (pNode->GetType() != NODE_GROUND)
			continue;

		bool bAcceptable = false;

		CAI_Link *pLink = NULL;
		for (int i = 0; i < pNode->NumLinks(); i++)
		{
			pLink = pNode->GetLinkByIndex( i );
			if (pLink)
			{
				if (pLink->m_iAcceptedMoveTypes[HULL_LARGE] & bits_CAP_MOVE_GROUND)
				{
					bAcceptable = true;
					break;
				}
			}
		}

		if (!bAcceptable)
			continue;

		// Check if there's enough room to get down
		trace_t tr;
		AI_TraceHull( vecNodeOrigin, vecNodeOrigin, vecMins, vecMaxs, MASK_NPCSOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );
		if (tr.startsolid)
			continue;

		// Ensure it isn't about to be occupied by anyone else
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		int nAIs = g_AI_Manager.NumAIs();
		for ( int i = 0; i < nAIs; i++ )
		{
			CAI_BaseNPC *pNPC = ppAIs[ i ];

			if ( pNPC->GetNavigator()->GetGoalPos() == vecNodeOrigin && pNPC->IsAlive() )
			{
				bAcceptable = false;
				break;
			}
		}

		if (!bAcceptable)
			continue;

		// HACKHACK: Is there a better way to do this?
		if (GetNavigator()->SetGoal( vecNodeOrigin ))
		{
			float flPathSqr = GetNavigator()->BuildAndGetPathDistToGoal();
			if (GetNavigator()->BuildAndGetPathDistToGoal() < flNearestPathSqr)
			{
				vecDest = vecNodeOrigin;
				*pOutHint = pHint;
				flNearestDistSqr = flDistSqr;
				flNearestPathSqr = flPathSqr;

				if (pLink)
					vecDir = (vecNodeOrigin - g_pBigAINet->GetNode( pLink->m_iDestID )->GetOrigin()).Normalized();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
Activity CAI_SurrenderBehavior::NPC_TranslateActivity( Activity activity )
{
	if ( activity == ACT_IDLE && m_hSafePlaceHint && m_hSafePlaceHint->HintActivityName() != NULL_STRING )
	{
		return GetOuter()->GetHintActivity(m_hSafePlaceHint->HintType(), (Activity)CAI_BaseNPC::GetActivityID( STRING(m_hSafePlaceHint->HintActivityName()) ) );
	}

	if (activity == (Activity)ACT_ARREST_FLOOR_ENTER)
	{
		//Activity curActivity = GetActivity();
		//if (curActivity == ACT_CROUCH_PANICKED ||
		//	curActivity == ACT_WALK_PANICKED ||
		//	curActivity == ACT_RUN_PANICKED)

		// HACKHACK
		if (GetOuter()->NPC_TranslateActivity( ACT_WALK ) == (Activity)ACT_WALK_PANICKED)
		{
			activity = (Activity)ACT_ARREST_FLOOR_ENTER_PANICKED;
		}
	}

	return BaseClass::NPC_TranslateActivity( activity );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::Speak( AIConcept_t concept )
{
	CAI_Expresser *pExpresser = GetOuter()->GetExpresser();
	if ( !pExpresser )
		return false;

	CAI_PlayerAlly *pAlly = dynamic_cast<CAI_PlayerAlly*>(GetOuter());
	if ( pAlly )
		return pAlly->SpeakIfAllowed( concept );
	
	return pExpresser->Speak( concept );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::Speak( AIConcept_t concept, AI_CriteriaSet &modifiers )
{
	CAI_Expresser *pExpresser = GetOuter()->GetExpresser();
	if ( !pExpresser )
		return false;

	CAI_PlayerAlly *pAlly = dynamic_cast<CAI_PlayerAlly*>(GetOuter());
	if ( pAlly )
		return pAlly->SpeakIfAllowed( concept, modifiers );
	
#ifdef NEW_RESPONSE_SYSTEM
	return pExpresser->Speak( concept, &modifiers );
#else
	return pExpresser->Speak( concept, modifiers );
#endif
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_SurrenderBehavior::IsSpeaking()
{
	CAI_Expresser *pExpresser = GetOuter()->GetExpresser();
	if ( !pExpresser )
		return false;
		
	return pExpresser->IsSpeaking();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_SurrenderBehavior )

	DECLARE_ACTIVITY( ACT_ARREST_FLOOR )
	DECLARE_ACTIVITY( ACT_ARREST_FLOOR_CURIOUS )
	DECLARE_ACTIVITY( ACT_ARREST_FLOOR_ENTER )
	DECLARE_ACTIVITY( ACT_ARREST_FLOOR_ENTER_PANICKED )
	DECLARE_ACTIVITY( ACT_ARREST_FLOOR_EXIT )
	DECLARE_ACTIVITY( ACT_ARREST_STANDING_GESTURE )

	DECLARE_TASK( TASK_SURRENDER_FACE_CAPTOR )
	DECLARE_TASK( TASK_SURRENDER_GET_PATH_TO_SAFE_PLACE )
	DECLARE_TASK( TASK_SURRENDER_AT_SAFE_PLACE )
	DECLARE_TASK( TASK_SURRENDER_IDLE )
	DECLARE_TASK( TASK_SURRENDER_IDLE_LOOP )
	DECLARE_TASK( TASK_SURRENDER_SET_CUSTOM_BOUNDS )
	DECLARE_TASK( TASK_SURRENDER_RESET_BOUNDS )

	DECLARE_CONDITION( COND_SURRENDER_STATE_CHANGE )
	//DECLARE_CONDITION( COND_SURRENDER_INITIAL )
	DECLARE_CONDITION( COND_SURRENDER_ANYWHERE )
	DECLARE_CONDITION( COND_SURRENDER_CANCEL )
	DECLARE_CONDITION( COND_SURRENDER_ENEMY_CLOSE )
	DECLARE_CONDITION( COND_SURRENDER_ENEMY_TOO_CLOSE )

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_FACE_CAPTOR,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SURRENDER_FACE_CAPTOR						0"
		"		TASK_WAIT								1"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_SURRENDER_MOVE_TO_SAFE_PLACE"
		""
		"	Interrupts"
		""
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_FEAR_ENEMY_TOO_CLOSE"
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_MOVE_TO_SAFE_PLACE,

		"	Tasks"
		"		TASK_SURRENDER_FACE_CAPTOR						0"
		"		TASK_SURRENDER_GET_PATH_TO_SAFE_PLACE	0"
		"		TASK_WALK_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_SURRENDER_AT_SAFE_PLACE				0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_SURRENDER_STANDING_IDLE"
		""
		"	Interrupts"
		""
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SURRENDER_CANCEL"
		"		COND_SURRENDER_ENEMY_CLOSE"
		"		COND_SURRENDER_ENEMY_TOO_CLOSE"
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_MOVE_AWAY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_MOVE_AWAY_FAIL"
		"		TASK_MOVE_AWAY_PATH						120"
		"		TASK_WALK_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_SURRENDER_AT_SAFE_PLACE			0"
		//"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_MOVE_AWAY_END"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
		"		COND_SURRENDER_CANCEL"
		"		COND_SURRENDER_ENEMY_TOO_CLOSE"
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_STANDING_IDLE,

		"	Tasks"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_SURRENDER_IDLE_LOOP			0"
		""
		"	Interrupts"
		""
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_PLAYER_PUSHING"
		"		COND_SURRENDER_CANCEL"
		"		COND_SURRENDER_STATE_CHANGE"
		"		COND_SURRENDER_ENEMY_CLOSE"
		"		COND_SURRENDER_ENEMY_TOO_CLOSE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_FLOOR_ENTER,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_PLAY_SEQUENCE	ACTIVITY:ACT_ARREST_FLOOR_ENTER"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_SURRENDER_FLOOR_IDLE"
		""
		"	Interrupts"
		""
	);

	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_FLOOR_IDLE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_SURRENDER_FLOOR_IDLE"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_ARREST_FLOOR"
		"		TASK_SURRENDER_SET_CUSTOM_BOUNDS			0"
		"		TASK_SURRENDER_IDLE_LOOP			0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_SURRENDER_FLOOR_EXIT"
		""
		"	Interrupts"
		""
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_SPOOKY"
		//"		COND_BETTER_WEAPON_AVAILABLE"
		"		COND_NEW_ENEMY"
		"		COND_SURRENDER_CANCEL"
		"		COND_SURRENDER_STATE_CHANGE"
		"		COND_SURRENDER_ENEMY_CLOSE"
		"		COND_SURRENDER_ENEMY_TOO_CLOSE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_FLOOR_CURIOUS,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_SURRENDER_FLOOR_IDLE"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_ARREST_FLOOR_CURIOUS"
		"		TASK_SURRENDER_IDLE			0"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_SURRENDER_FLOOR_IDLE"
		""
		"	Interrupts"
		""
		"		COND_SURRENDER_CANCEL"
		"		COND_SURRENDER_STATE_CHANGE"
		"		COND_SURRENDER_ENEMY_CLOSE"
		"		COND_SURRENDER_ENEMY_TOO_CLOSE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SURRENDER_FLOOR_EXIT,

		"	Tasks"
		"		TASK_SURRENDER_RESET_BOUNDS			0"
		"		TASK_PLAY_SEQUENCE	ACTIVITY:ACT_ARREST_FLOOR_EXIT"
		""
		"	Interrupts"
		""
	);


AI_END_CUSTOM_SCHEDULE_PROVIDER()
