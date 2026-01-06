//=============================================================================//
//
// Purpose:		Beast behavior used primarily by the Chapter 3 monster.
// 
//				This behavior mainly centers around a "home" which the beast dwells within when idle.
//				When the beast hears a sound, it will leave home to investigate and attack any intruders.
//				If the beast doesn't sense anything for a while, it will return home and wait to be alerted again.
//
//=============================================================================//

#include "cbase.h"
#include "ai_behavior_beast.h"
#include "ai_hint.h"
#include "ai_moveprobe.h"
#include "hl2_gamerules.h"

// 
// Beast Behavior
// 

ConVar ez2_beast_return_time( "ez2_beast_return_time", "20.0", FCVAR_NONE, "How much time should pass after the beast becomes alert (e.g. due to investigating a sound with no enemy in sight or losing an existing enemy) before it should be slammed to idle, allowing it to return home." );

extern ConVar developer;	// developer mode

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_BeastBehavior )

	DEFINE_FIELD( m_bAtHome, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTeleport, FIELD_BOOLEAN ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_BeastBehavior::CAI_BeastBehavior()
{
	m_bAtHome = false;

	ListenForGameEvent( "zombie_scream" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::GoHome( bool bTeleport )
{
	if (bTeleport)
		m_bTeleport = true;

	SetCondition( COND_BEAST_FORCE_HOME );

	// Make sure the beast snaps into our behavior if we're not running
	if (!IsRunning())
		SetCondition( COND_PROVOKED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::HandleLeaveHome( void )
{
	m_bAtHome = false;

	if (GetHintNode())
	{
		GetHintNode()->NPCStoppedUsing( GetOuter() );
		float hintDelay = GetOuter()->GetHintDelay( GetHintNode()->HintType() );
		GetHintNode()->Unlock( hintDelay );
		ClearHintNode();
	}

	DevMsg( "BEAST BEHAVIOR: Leaving home!\n" );
	GetOuter()->FoundEnemySound();

	GetOuter()->FireNamedOutput( "OnBeastLeaveHome", variant_t(), GetOuter(), GetOuter() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::EndScheduleSelection( void )
{
	if ( m_bAtHome )
	{
		// Leaving home. Something happened
		HandleLeaveHome();
	}

	BaseClass::EndScheduleSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_BeastBehavior::SelectSchedule()
{
	int base = BaseClass::SelectSchedule();

	if ( !m_bAtHome )
	{
		if ( base == SCHED_IDLE_STAND || base == SCHED_NONE || HasCondition(COND_BEAST_FORCE_HOME) )
		{
			// Idle, go home
			GetOuter()->LostEnemySound();
			DevMsg("BEAST BEHAVIOR: Going home!\n");
			return m_bTeleport ? SCHED_BEAST_TELEPORT_HOME : SCHED_BEAST_COME_HOME;
		}
	}
	else
	{
		// Leaving home. Something happened
		HandleLeaveHome();
	}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::GatherConditions( void )
{
	BaseClass::GatherConditions();

	if ( GetOuter()->GetState() == NPC_STATE_ALERT )
	{
		// If we've been in this state for X seconds, go back home
		if (gpGlobals->curtime - GetOuter()->m_flLastStateChangeTime > ez2_beast_return_time.GetFloat())
		{
			//SetEnemy( NULL );
			GetOuter()->SetIdealState( NPC_STATE_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_BeastBehavior::TranslateSchedule( int scheduleType )
{
	int base = BaseClass::TranslateSchedule( scheduleType );

	//switch (base)
	//{
	//	case SCHED_INVESTIGATE_SOUND:
	//		{
	//
	//		} break;
	//}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::BuildScheduleTestBits( void )
{
	GetOuter()->SetCustomInterruptCondition( GetClassScheduleIdSpace()->ConditionLocalToGlobal(COND_BEAST_FORCE_HOME) );

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_BeastBehavior::CanSelectSchedule( void )
{
	if ( HL2GameRules()->IsBeastInStealthMode() == false )
		return false;

	return BaseClass::CanSelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BEAST_FIND_HOME:
		{
			CHintCriteria hintCriteria;
			hintCriteria.SetHintType( HINT_BEAST_HOME );

			int iBits = bits_HINT_NODE_NEAREST;
			if ( developer.GetInt() > 0 )
			{
				iBits |= bits_HINT_NODE_REPORT_FAILURES;
			}

			hintCriteria.SetFlag( iBits );
			hintCriteria.AddIncludePosition( GetAbsOrigin(), pTask->flTaskData );
			CAI_Hint *pHint = CAI_HintManager::FindHint( GetOuter(), hintCriteria );
			if (pHint)
			{
				DevMsg("Find Home: Found hint %i in radius %f\n", pHint->GetNodeId(), pTask->flTaskData);
				SetHintNode( pHint );
			}
			else
			{
				DevMsg("Find Home: Found no hint in radius %f\n", pTask->flTaskData);
			}

			ChainStartTask( TASK_GET_PATH_TO_HINTNODE );

			if ( !HasCondition(COND_TASK_FAILED) && pHint )
			{
				if (GetNavigator()->IsGoalSet())
				{
					pHint->Lock( GetOuter() );
					pHint->NPCHandleStartNav( GetOuter(), true );
					TaskComplete();
					break;
				}
			}

			TaskFail( FAIL_NO_ROUTE );
		} break;

	case TASK_BEAST_BE_HOME:
		{
			m_bAtHome = true;
			GetHintNode()->NPCStartedUsing( GetOuter() );
			GetOuter()->FireNamedOutput( "OnBeastHome", variant_t(), GetOuter(), GetOuter() );
			TaskComplete();
		} break;

	case TASK_TELEPORT_HOME:
		{
			m_bTeleport = false;

			CAI_Hint *pHint = GetHintNode();
			if (pHint)
			{
				Vector vecOrigin = pHint->GetAbsOrigin();
				QAngle angAngles = pHint->GetAbsAngles();
				GetOuter()->Teleport( &vecOrigin, &angAngles, NULL );

				// Ensure that they're on the ground
				if ( !GetOuter()->GetMoveParent() )
				{
					GetOuter()->GetMoveProbe()->FloorPoint( vecOrigin + Vector( 0, 0, 0.1 ), MASK_NPCSOLID, 0, -2048, &vecOrigin );
					SetLocalOrigin( vecOrigin );
				}

				TaskComplete();
				break;
			}

			TaskFail( FAIL_NO_TARGET );
		} break;

	default:
		BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::RunTask( const Task_t *pTask )
{
	//switch ( pTask->iTask )
	//{
	//default:
		BaseClass::RunTask( pTask );
	//}
}

bool CAI_BeastBehavior::QueryHearSound( CSound *pSound )
{
	// Don't hear turrets alerted by us
	if (pSound->m_hTarget == GetOuter())
		return false;

	if (!m_bAtHome && GetOuter()->FInViewCone(pSound->GetSoundReactOrigin()) && GetOuter()->FVisible(pSound->GetSoundReactOrigin()))
	{
		// Don't hear sounds that we don't need to investigate
		if (!pSound->m_hTarget || (pSound->m_hTarget->IsNPC() && GetOuter()->IRelationType( pSound->m_hTarget ) >= D_LI))
			return false;
	}

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_BeastBehavior::IsInterruptable( void )
{
	if ( IsCurSchedule( SCHED_BEAST_STAY_HOME ) )
		return false;

	return BaseClass::IsInterruptable();
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CAI_BeastBehavior::FireGameEvent( IGameEvent *event )
{
	DevMsg("Beast heard event\n");

	if ( FStrEq( "zombie_scream", event->GetName() ) )
	{
		// Alert the beast
		CBaseEntity *pZombie = UTIL_EntityByIndex( event->GetInt("zombie") );
		CBaseEntity *pTarget = UTIL_EntityByIndex( event->GetInt("target") );

		if (pTarget)
		{
			GetOuter()->UpdateEnemyMemory( pTarget, GetAbsOrigin(), pZombie );
		}
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_BeastBehavior )

	DECLARE_CONDITION( COND_BEAST_FORCE_HOME )

	DECLARE_TASK( TASK_BEAST_FIND_HOME )
	DECLARE_TASK( TASK_BEAST_BE_HOME )
	DECLARE_TASK( TASK_TELEPORT_HOME )

	//---------------------------------

	DEFINE_SCHEDULE
	(
		SCHED_BEAST_STAY_HOME,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_BEAST_TELEPORT_HOME"
		"		TASK_STOP_MOVING		1"
		"		TASK_PLAY_HINT_ACTIVITY		0"
		"		TASK_WAIT_INDEFINITE	5"
		""
		"	Interrupts"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_BEAST_COME_HOME,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_PATROL_WALK"
		"		TASK_BEAST_FIND_HOME		4096"
		"		TASK_REMEMBER				MEMORY:LOCKED_HINT"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_STOP_MOVING			0"
		"		TASK_BEAST_BE_HOME			0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_BEAST_STAY_HOME"
		""
		"	Interrupts"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_BEAST_TELEPORT_HOME,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_PATROL_WALK"
		"		TASK_BEAST_FIND_HOME		4096"
		"		TASK_REMEMBER				MEMORY:LOCKED_HINT"
		"		TASK_STOP_MOVING			0"
		"		TASK_TELEPORT_HOME			0"
		"		TASK_BEAST_BE_HOME			0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_BEAST_STAY_HOME"
		""
		"	Interrupts"
		"		COND_PROVOKED"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()
