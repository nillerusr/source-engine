//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"

#include "ai_behavior_fightfromcover.h"
#include "ai_hint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

BEGIN_DATADESC( CAI_FightFromCoverBehavior )
	DEFINE_FIELD( m_hGoal, FIELD_EHANDLE ),
	DEFINE_EMBEDDED( m_FrontMoveMonitor ),
	DEFINE_EMBEDDED( m_FrontTimer ),
END_DATADESC();

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CAI_FightFromCoverBehavior::CAI_FightFromCoverBehavior()
{

}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::SetGoal( CAI_FightFromCoverGoal *pGoal )
{
	m_hGoal = pGoal;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::ClearGoal()
{
	m_hGoal = NULL;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::IsPointInZone( const Vector &v )
{
	if ( !m_hGoal->GetFrontDirection().IsValid() )
	{
		return true;
	}

	Vector vCenterZone;
	vCenterZone = ( m_hGoal->GetFrontDirection() * -m_hGoal->m_BiasZone ) + m_hGoal->GetFrontPosition();

	Vector vRelativePos ;
	VectorRotate( v - vCenterZone, -m_hGoal->GetFrontAngles(), vRelativePos );
	float w = m_hGoal->m_WidthZone * .5, l = m_hGoal->m_LengthZone * .5, h = m_hGoal->m_HeightZone * .5;

	Vector mins( -l, -w, -h ), maxs( l, w, h );

	if ( vRelativePos.x < mins.x || vRelativePos.x > maxs.x ||
		 vRelativePos.y < mins.y || vRelativePos.y > maxs.y ||
		 vRelativePos.z < mins.z || vRelativePos.z > maxs.z )
	{
//		NDebugOverlay::Cross3D( v, 36, 255, 0, 0, true, 3 );
		return false;
	}

//	NDebugOverlay::Cross3D( v, 36, 0, 255, 0, true, 3 );
	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::FValidateHintType ( CAI_Hint *pHint )
{
	if ( pHint->HintType() == HINT_GENERIC && pHint->GetGenericType() == m_hGoal->m_GenericHintType )
	{
		return true;
	}
	return BaseClass::FValidateHintType( pHint );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::HintSearchFilter( void *pContext, CAI_Hint *pCandidate )
{
	CAI_FightFromCoverBehavior *pThis = (CAI_FightFromCoverBehavior *)pContext;
	Vector vHintDir;
	if ( pThis->m_hGoal->GetFrontDirection().IsValid() )
	{
		pCandidate->GetVectors( &vHintDir, NULL, NULL );
		if ( vHintDir.Dot( pThis->m_hGoal->GetFrontDirection() ) < DOT_45DEGREE )
		{
			return false;
		}
	}
	return pThis->IsPointInZone( pCandidate->GetAbsOrigin() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_FFC_HINT_CHANGE );
	if ( !m_hGoal )
	{
		SetCondition( COND_FFC_HINT_CHANGE );
		ClearHintNode();
		return;
	}

	if ( GetHintNode() && m_FrontTimer.Expired() && m_FrontMoveMonitor.TargetMoved( m_hGoal->GetFrontPosition() ) )
	{
		float flFastRejectDist = MAX( m_hGoal->m_WidthZone, m_hGoal->m_LengthZone ) + m_hGoal->m_BiasZone;

		if ( ( GetHintNode()->GetAbsOrigin() - m_hGoal->GetFrontPosition() ).LengthSqr() > Square( flFastRejectDist ) || !IsPointInZone( GetHintNode()->GetAbsOrigin() ) )
		{
			GetHintNode()->Unlock();
			SetHintNode( NULL );
			SetCondition( COND_FFC_HINT_CHANGE );
		}
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::CanSelectSchedule()
{
	return ( m_hGoal != NULL );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CAI_FightFromCoverBehavior::SelectSchedule()
{
	Assert( m_hGoal != NULL );
	if ( !m_hGoal )
	{
		return BaseClass::SelectSchedule();
	}

	if ( !GetHintNode() )
	{
		if ( m_FrontTimer.Expired() )
		{
			m_FrontTimer.Set( .75, 1.5 );

			Vector vFront = m_hGoal->GetFrontPosition();
			CHintCriteria hintCriteria;
			hintCriteria.SetHintType( HINT_GENERIC );
			hintCriteria.SetGenericType( m_hGoal->m_GenericHintType );
			hintCriteria.SetFlag( bits_HINT_NODE_NEAREST | bits_HINT_NODE_USE_GROUP );

			// Add the search position
			hintCriteria.AddIncludePosition( vFront, MAX( m_hGoal->m_WidthZone, m_hGoal->m_LengthZone ) + m_hGoal->m_BiasZone );
			hintCriteria.AddExcludePosition( m_hGoal->GetGoalEntity()->GetAbsOrigin(), 2.5*12 );

			hintCriteria.SetFilterFunc( &HintSearchFilter, this );

			const Vector &vDir = m_hGoal->GetFrontDirection();
			if ( vDir.IsValid() )
			{
				vFront += vDir * m_hGoal->m_LengthZone;
			}

			CAI_Hint *pHint = CAI_HintManager::FindHint( GetOuter(), vFront, hintCriteria );

			if ( !pHint )
			{
				// @HACKHACK [5/21/2008 tom]
				float lengthSaved = m_hGoal->m_LengthZone, biasSaved = m_hGoal->m_BiasZone, widthSaved = m_hGoal->m_WidthZone;
				m_hGoal->m_LengthZone += 2000;
				m_hGoal->m_BiasZone += 1000;
				m_hGoal->m_WidthZone += 2000;
				pHint = CAI_HintManager::FindHint( GetOuter(), vFront, hintCriteria );
				m_hGoal->m_LengthZone = lengthSaved;
				m_hGoal->m_BiasZone = biasSaved;
				m_hGoal->m_WidthZone = widthSaved;
			}

			if ( pHint )
			{
				SetHintNode( pHint );
				pHint->Lock( GetOuter() );
				m_FrontMoveMonitor.SetMark( vFront, 3*12 );
			}
		}
	}

	if ( GetHintNode() )
	{
		if ( ( GetHintNode()->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() > Square( 12.0 ) )
		{
			return SCHED_FFC_RUN_TO_HINT;
		}
		else if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && !GetOuter()->GetShotRegulator()->IsInRestInterval() )
		{
			return SCHED_FFC_ATTACK;
		}
		else if ( HasCondition( COND_NO_PRIMARY_AMMO ) )
		{
			return SCHED_RELOAD;
		}
		else
		{
			GetOuter()->GetShotRegulator()->Reset( true );
			return SCHED_FFC_HOLD_COVER;
		}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_FFC_GET_PATH_TO_HINT:
		{
			ChainStartTask( TASK_GET_PATH_TO_HINTNODE );

			if ( !HasCondition(COND_TASK_FAILED) )
			{
				UpdateAnimationsFromHint();
			}
			else
			{
				// Should mark hint and try to find another one
				ClearHintNode( 5 );
			}

			break;
		}

	case TASK_FFC_ATTACK:
		{
			CBaseCombatWeapon *pWeapon = GetOuter()->GetActiveWeapon();
			if ( !pWeapon )
			{
				TaskFail( "No weapon" );
				return;
			}
			int clipSize = pWeapon->GetMaxClip1();
			GetOuter()->GetShotRegulator()->SetBurstShotCountRange( clipSize * .3, clipSize * .6 );
			GetOuter()->GetShotRegulator()->SetRestInterval( INT_MAX, INT_MAX );
			GetOuter()->GetShotRegulator()->Reset();
			StartAnimationTask( m_ShootAnim, true, ACT_RANGE_ATTACK1 );
			GetOuter()->SetLastAttackTime( gpGlobals->curtime );
			GetOuter()->OnRangeAttack1();
			break;
		}

	case TASK_RELOAD:
		{
			if ( !StartAnimationTask( m_ReloadAnim, true ) )
			{
				BaseClass::StartTask( pTask );
			}
			break;
		}

	case TASK_FFC_PEEK:
		{
			StartAnimationTask( m_PeekAnim );
			break;
		}
	case TASK_FFC_COVER:
		{
			StartAnimationTask( m_CoverAnim );
			break;
		}

	default:
		BaseClass::StartTask( pTask );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_FFC_ATTACK:
		{
			GetOuter()->AutoMovement( );

			if ( GetOuter()->IsActivityFinished() )
			{
				if ( !GetEnemy() || !GetEnemy()->IsAlive() )
				{
					TaskComplete();
					return;
				}

				if ( !GetOuter()->GetShotRegulator()->IsInRestInterval() )
				{
					if ( GetOuter()->GetShotRegulator()->ShouldShoot() )
					{
						GetOuter()->OnRangeAttack1();
						StartAnimationTask( m_ShootAnim, true, ACT_RANGE_ATTACK1 );
					}
					return;
				}
				TaskComplete();
			}
			break;
		}
	case TASK_FFC_PEEK:
	case TASK_FFC_COVER:
		{
			ChainRunTask( TASK_PLAY_SEQUENCE );
			break;
		}
	default:
		BaseClass::RunTask( pTask);
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::OnUpdateShotRegulator()
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAI_FightFromCoverBehavior::UpdateAnimationsFromHint()
{
	m_EntryAnim.Reset();
	m_MoveAnim.Reset();
	m_CoverAnim.Reset();
	m_ReloadAnim.Reset();
	m_PeekAnim.Reset();
	m_ShootAnim.Reset();
	m_ExitAnim.Reset();

	if ( !GetHintNode() )
	{
		return;
	}

	CScriptScope &hintScope = GetHintNode()->m_ScriptScope;
	ScriptVariant_t animationsVar;

	if ( hintScope.GetValue( "animations", &animationsVar ) )
	{
		if ( animationsVar.m_type == FIELD_HSCRIPT )
		{
			CScriptScope animations;
			ScriptVariant_t var;

			animations.Init( animationsVar.m_hScript );

			GetAnimation( animations, "movement", &m_MoveAnim );
			GetAnimation( animations, "entry", &m_EntryAnim );
			if ( m_EntryAnim.id != ACT_INVALID )
			{
				DevMsg( "entry anim not yet supported!\n" );
				m_EntryAnim.Reset();
			}
			GetAnimation( animations, "cover", &m_CoverAnim );
			GetAnimation( animations, "reload", &m_ReloadAnim );
			GetAnimation( animations, "peek", &m_PeekAnim );
			GetAnimation( animations, "shoot", &m_ShootAnim );
			GetAnimation( animations, "exit", &m_ExitAnim );
			if ( m_EntryAnim.id != ACT_INVALID )
			{
				DevMsg( "exit anim not yet supported!\n" );
				m_EntryAnim.Reset();
			}
		}
		else
		{
			DevMsg( "Unexpected type for script value \"animations\"\n" );
		}
		hintScope.ReleaseValue( animationsVar );
	}

	if ( m_MoveAnim.id != ACT_INVALID )
	{
		if ( m_MoveAnim.bActivity )
		{
			GetNavigator()->SetMovementActivity( (Activity)m_MoveAnim.id );
		}
		else
		{
			GetNavigator()->SetMovementSequence( m_MoveAnim.id );
		}
	}
	else
	{
		ChainStartTask( TASK_RUN_PATH );
	}

	if ( m_EntryAnim.id != ACT_INVALID )
	{
		Assert( 0 );
	}
	else if ( m_CoverAnim.id != ACT_INVALID )
	{
		if ( m_CoverAnim.bActivity )
		{
			GetNavigator()->SetArrivalActivity( (Activity)m_CoverAnim.id );
		}
		else
		{
			GetNavigator()->SetArrivalSequence( m_CoverAnim.id );
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::GetAnimation( CScriptScope &scope, const char *pszKey, Animation_t *pAnimation )
{
	ScriptVariant_t var;
	bool result = false;

	// Find the movement activity or sequence
	if ( scope.GetValue( pszKey, &var ) )
	{
		if ( var.m_type == FIELD_CSTRING )
		{
			pAnimation->bActivity = ( V_strncmp( var, "ACT_", 4 ) == 0 );
			if ( !pAnimation->bActivity )
			{
				pAnimation->id = GetOuter()->LookupSequence( var );
				if ( pAnimation->id < 0 )
				{
					pAnimation->Reset();
				}
			}
			else
			{
				pAnimation->id = CAI_BaseNPC::GetActivityID( var );
			}

			if ( pAnimation->id == ACT_INVALID )
			{
				DevMsg( "Failed to resolve animation \"%s\"\n", var.m_pszString );
			}
		}
		scope.ReleaseValue( var );
	}
	return result;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CAI_FightFromCoverBehavior::StartAnimationTask( const Animation_t &animation, bool bReset, Activity defaultActivity )
{
	if ( animation.id != ACT_INVALID )
	{
		if ( animation.bActivity )
		{
			if ( bReset )
				GetOuter()->ResetIdealActivity( (Activity)animation.id );
			else
				GetOuter()->SetIdealActivity( (Activity)animation.id );
		}
		else
		{
			GetOuter()->SetIdealSequence( animation.id, bReset );
		}
		return true;
	}
	else
	{
		if ( bReset )
			GetOuter()->SetIdealActivity( defaultActivity );
		return false;
	}
}

//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_FightFromCoverBehavior )

	DECLARE_CONDITION( COND_FFC_HINT_CHANGE )

	DECLARE_TASK( TASK_FFC_GET_PATH_TO_HINT )
	DECLARE_TASK( TASK_FFC_COVER )
	DECLARE_TASK( TASK_FFC_PEEK )
	DECLARE_TASK( TASK_FFC_ATTACK )

	//---------------------------------

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_RUN_TO_HINT,

		"	Tasks"
		"		TASK_FFC_GET_PATH_TO_HINT	0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_FFC_HOLD_COVER"
 		""
		"	Interrupts"
		"		COND_FFC_HINT_CHANGE"
 	)

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_HOLD_COVER,

		"	Tasks"
//		"		TASK_FACE_HINTNODE	0"
		"		TASK_FFC_COVER		0"
		"		TASK_WAIT_RANDOM	2.0"
		"		TASK_SET_SCHEDULE	SCHEDULE:SCHED_FFC_PEEK"
 		""
		"	Interrupts"
		"		COND_FFC_HINT_CHANGE"
		"		COND_NO_PRIMARY_AMMO"
 	)

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_RELOAD,

		"	Tasks"
		"		TASK_RELOAD		0"
		"		TASK_SET_SCHEDULE	SCHEDULE:SCHED_FFC_PEEK"
 		""
		"	Interrupts"
 	)

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_PEEK,

		"	Tasks"
		"		TASK_FFC_PEEK		0"
		"		TASK_SET_SCHEDULE	SCHEDULE:SCHED_FFC_HOLD_PEEK"
 		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_FFC_HINT_CHANGE"
 	)

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_HOLD_PEEK,

		"	Tasks"
		"		TASK_WAIT_RANDOM	1.0"
		"		TASK_SET_SCHEDULE	SCHEDULE:SCHED_FFC_HOLD_COVER"
 		""
		"	Interrupts"
		"		COND_FFC_HINT_CHANGE"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_NO_PRIMARY_AMMO"
 	)

 	DEFINE_SCHEDULE
 	( 
 		SCHED_FFC_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FFC_ATTACK			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_HEAR_DANGER"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_WEAPON_SIGHT_OCCLUDED"
 	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()


//-----------------------------------------------------------------------------
//
// CAI_FightFromCoverGoal
//
//-----------------------------------------------------------------------------

BEGIN_DATADESC( CAI_FightFromCoverGoal )
	DEFINE_KEYFIELD( m_DirectionalMarker, FIELD_STRING, "DirectionalMarker" ),
	DEFINE_KEYFIELD( m_GenericHintType, FIELD_STRING, "GenericHintType" ),
	DEFINE_KEYFIELD( m_WidthZone, FIELD_FLOAT, "width" ),
	DEFINE_KEYFIELD( m_LengthZone, FIELD_FLOAT, "length" ),
	DEFINE_KEYFIELD( m_HeightZone, FIELD_FLOAT, "height" ),
	DEFINE_KEYFIELD( m_BiasZone, FIELD_FLOAT, "bias" ),
	DEFINE_FIELD( m_vFront, FIELD_POSITION_VECTOR ),

	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetDirectionalMarker", InputSetDirectionalMarker ),
	DEFINE_THINKFUNC( FrontThink ),

END_DATADESC()

//-------------------------------------

LINK_ENTITY_TO_CLASS( ai_goal_fightfromcover, CAI_FightFromCoverGoal );

//-------------------------------------

CAI_FightFromCoverGoal::CAI_FightFromCoverGoal()
{
	m_vDir.Invalidate();
	m_WidthZone = 50*12;
	m_LengthZone = 40*12;
	m_BiasZone = 5*12;
	m_HeightZone = 200*12;
}

//-------------------------------------

const Vector &CAI_FightFromCoverGoal::GetFrontPosition()
{
	if ( m_hDirectionalMarker != NULL )
	{
		return m_vFront;
	}
	else if ( m_hGoalEntity != NULL )
	{
		return m_hGoalEntity->GetAbsOrigin();
	}

	return GetAbsOrigin();
}

//-------------------------------------

const Vector &CAI_FightFromCoverGoal::GetFrontDirection()
{
	return m_vDir;
}

//-------------------------------------

const QAngle &CAI_FightFromCoverGoal::GetFrontAngles()
{
	if ( m_hDirectionalMarker != NULL )
	{
		return m_hDirectionalMarker->GetAbsAngles();
	}
	static QAngle invalid( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
	return invalid;
}

//-------------------------------------

void CAI_FightFromCoverGoal::BeginMovingFront()
{
	if ( m_hDirectionalMarker != NULL )
	{
		if ( m_hGoalEntity != NULL )
		{
			if ( !m_pfnThink )
			{
				SetThink( &CAI_FightFromCoverGoal::FrontThink );
				SetNextThink( gpGlobals->curtime + .1 );
			}
		}
		else
		{
			m_vFront = m_hDirectionalMarker->GetAbsOrigin();
		}
	}
	else
	{
		EndMovingFront();
	}
}

//-------------------------------------

void CAI_FightFromCoverGoal::EndMovingFront()
{
	SetThink( NULL );
}

//-------------------------------------

void CAI_FightFromCoverGoal::OnActivate()
{
	BeginMovingFront();
}

//-------------------------------------

void CAI_FightFromCoverGoal::OnDeactivate()
{
	EndMovingFront();
}

//-------------------------------------

void CAI_FightFromCoverGoal::FrontThink()
{
	if ( m_hDirectionalMarker != NULL && m_hGoalEntity != NULL )
	{
		Vector vClosest;
		AngleVectors( m_hDirectionalMarker->GetAbsAngles(), &m_vDir );
		CalcClosestPointOnLineSegment( m_hGoalEntity->GetAbsOrigin(), m_vFront, m_vFront + m_vDir * 99999, vClosest );
		m_vFront = vClosest;
		SetNextThink( gpGlobals->curtime + 0.5 );
	}
	else
	{
		EndMovingFront();
	}
}

//-------------------------------------

void CAI_FightFromCoverGoal::EnableGoal( CAI_BaseNPC *pAI )
{
	CAI_FightFromCoverBehavior *pBehavior;
	if ( !pAI->GetBehavior( &pBehavior ) )
		return;
	
	CBaseEntity *pGoalEntity = GetGoalEntity();
	if ( pGoalEntity )
	{
		pBehavior->SetGoal( this );
	}
}

//-------------------------------------

void CAI_FightFromCoverGoal::DisableGoal( CAI_BaseNPC *pAI  )
{ 
	CAI_FightFromCoverBehavior *pBehavior;
	if ( !pAI || !pAI->GetBehavior( &pBehavior ) )
		return;
	
	pBehavior->ClearGoal();
}


//-------------------------------------

void CAI_FightFromCoverGoal::ResolveNames()
{
	BaseClass::ResolveNames();

	if ( m_hGoalEntity == NULL && AI_IsSinglePlayer() )
	{
		m_hGoalEntity = UTIL_GetLocalPlayer();
	}

	if ( m_DirectionalMarker != NULL_STRING )
	{
		EHANDLE hDirectionalMarker = gEntList.FindEntityByName( NULL, STRING(m_DirectionalMarker) );
		if ( m_hDirectionalMarker != hDirectionalMarker )
		{
			m_hDirectionalMarker = gEntList.FindEntityByName( NULL, STRING(m_DirectionalMarker) );
			m_vFront = m_hDirectionalMarker->GetAbsOrigin();
			AngleVectors( m_hDirectionalMarker->GetAbsAngles(), &m_vDir );
		}
		BeginMovingFront();
	}
	else
	{
		m_hDirectionalMarker = NULL;
		m_vDir.Invalidate();
		EndMovingFront();
	}
}

//-------------------------------------

void CAI_FightFromCoverGoal::InputSetDirectionalMarker( inputdata_t &inputdata )
{
	m_hDirectionalMarker = inputdata.value.Entity();
	if ( m_hDirectionalMarker != NULL )
	{
		m_DirectionalMarker = m_hDirectionalMarker->GetEntityName();
		m_vFront = m_hDirectionalMarker->GetAbsOrigin();
		AngleVectors( m_hDirectionalMarker->GetAbsAngles(), &m_vDir );
		BeginMovingFront();
	}
	else
	{
		m_DirectionalMarker = NULL_STRING;
		m_vDir.Invalidate();
		EndMovingFront();
	}
}

//-------------------------------------

int CAI_FightFromCoverGoal::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if ( m_debugOverlays & OVERLAY_TEXT_BIT )
	{
		CFmtStr str;
		if ( m_hDirectionalMarker != NULL )
		{
			EntityText( text_offset++, str.sprintf( "Dir ent: %s", m_hDirectionalMarker->GetEntityNameAsCStr() ), 0 );
			NDebugOverlay::YawArrow( m_hDirectionalMarker->GetAbsOrigin() + Vector( 0, 0, 6 ), m_hDirectionalMarker->GetAbsAngles().y, 60, 6, 255, 255, 255, 0, true, 0 );
			NDebugOverlay::Cross3DOriented( m_vFront + Vector( 0, 0, 6 ), m_hDirectionalMarker->GetAbsAngles(), 12, 255, 0, 0, true, 0 );

			Vector vBoxDrawCenter;
			AngleVectors( m_hDirectionalMarker->GetAbsAngles(), &vBoxDrawCenter );
			vBoxDrawCenter *= -m_BiasZone;
			vBoxDrawCenter += m_vFront;
			NDebugOverlay::BoxAngles( vBoxDrawCenter, Vector( -(m_LengthZone/2), -(m_WidthZone/2), -(m_HeightZone/2) ), Vector( m_LengthZone/2, m_WidthZone/2, m_HeightZone/2 ), m_hDirectionalMarker->GetAbsAngles(), 255, 0, 0, 16, 0 );

			for ( int i = 0; i < m_actors.Count(); i++ )
			{
				if ( m_actors[i] != NULL )
				{
					NDebugOverlay::Line( m_vFront, m_actors[i]->WorldSpaceCenter(), 0, 0, 127, true, 0 );
				}
			}
		}

		if ( m_hGoalEntity != NULL )
		{
			EntityText( text_offset++, str.sprintf( "Front ent: %s", m_hGoalEntity->GetEntityNameAsCStr() ), 0 );
		}
	}

	return text_offset;
}
