//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hl1_npc_talker.h"
#include "scripted.h"
#include "soundent.h"
#include "animation.h"
#include "entitylist.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "player.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "npcevent.h"
#include "ai_interactions.h"
#include "doors.h"

#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "hl1_ai_basenpc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

ConVar hl1_debug_sentence_volume( "hl1_debug_sentence_volume", "0" );
ConVar hl1_fixup_sentence_sndlevel( "hl1_fixup_sentence_sndlevel", "1" );

//#define TALKER_LOOK 0

BEGIN_DATADESC( CHL1NPCTalker )

	DEFINE_ENTITYFUNC( Touch ),
	DEFINE_FIELD( m_bInBarnacleMouth,	FIELD_BOOLEAN ),
	DEFINE_USEFUNC( FollowerUse ),

END_DATADESC()

void CHL1NPCTalker::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_HL1TALKER_FOLLOW_WALK_PATH_FOR_UNITS:
			{
				float distance;

				distance = (m_vecLastPosition - GetLocalOrigin()).Length2D();

				// Walk path until far enough away
				if ( distance > pTask->flTaskData || 
					 GetNavigator()->GetGoalType() == GOALTYPE_NONE )
				{
					TaskComplete();
					GetNavigator()->ClearGoal();		// Stop moving
				}
				break;
			}



		case TASK_TALKER_CLIENT_STARE:
		case TASK_TALKER_LOOK_AT_CLIENT:
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			
			// track head to the client for a while.
			if ( m_NPCState == NPC_STATE_IDLE		&& 
				 !IsMoving()								&&
				 !GetExpresser()->IsSpeaking() )
			{
			
				if ( pPlayer )
				{
					IdleHeadTurn( pPlayer );
				}
			}
			else
			{
				// started moving or talking
				TaskFail( "moved away" );
				return;
			}

			if ( pTask->iTask == TASK_TALKER_CLIENT_STARE )
			{
				// fail out if the player looks away or moves away.
				if ( ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).Length2D() > TALKER_STARE_DIST )
				{
					// player moved away.
					TaskFail( NO_TASK_FAILURE );
				}

				Vector vForward;
				AngleVectors( GetAbsAngles(), &vForward );
				if ( UTIL_DotPoints( pPlayer->GetAbsOrigin(), GetAbsOrigin(), vForward ) < m_flFieldOfView )
				{
					// player looked away
					TaskFail( "looked away" );
				}
			}

			if ( gpGlobals->curtime > m_flWaitFinished )
			{
				TaskComplete( NO_TASK_FAILURE );
			}

			break;
		}

		case TASK_WAIT_FOR_MOVEMENT:
		{
			if ( GetExpresser()->IsSpeaking() && GetSpeechTarget() != NULL)
			{
				// ALERT(at_console, "walking, talking\n");
				IdleHeadTurn( GetSpeechTarget(), GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime );
			}
			else if ( GetEnemy() )
			{
				IdleHeadTurn( GetEnemy() );
			}

			BaseClass::RunTask( pTask );

			break;
		}

		case TASK_FACE_PLAYER:
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			
			if ( pPlayer )
			{
				//GetMotor()->SetIdealYaw( pPlayer->GetAbsOrigin() );
				IdleHeadTurn( pPlayer );
				if ( gpGlobals->curtime > m_flWaitFinished && GetMotor()->DeltaIdealYaw() < 10 )
				{
					TaskComplete();
				}
			}
			else
			{
				TaskFail( FAIL_NO_PLAYER );
			}

			break;
		}

		case TASK_TALKER_EYECONTACT:
		{
			if (!IsMoving() && GetExpresser()->IsSpeaking() && GetSpeechTarget() != NULL)
			{
				// ALERT( at_console, "waiting %f\n", m_flStopTalkTime - gpGlobals->time );
				IdleHeadTurn( GetSpeechTarget(), GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime );
			}
			
			BaseClass::RunTask( pTask );
			
			break;

		}

				
		default:
		{
			if ( GetExpresser()->IsSpeaking() && GetSpeechTarget() != NULL)
			{
				IdleHeadTurn( GetSpeechTarget(), GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime );
			}
			else if ( GetEnemy() && m_NPCState == NPC_STATE_COMBAT )
			{
				IdleHeadTurn( GetEnemy() );
			}
			else if ( GetFollowTarget() )
			{
				IdleHeadTurn( GetFollowTarget() );
			}

			BaseClass::RunTask( pTask );
			break;
		}
	}
}

bool CHL1NPCTalker::ShouldGib( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_NEVERGIB )
		 return false;

	if ( ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && m_iHealth < GIB_HEALTH_VALUE ) || ( info.GetDamageType() & DMG_ALWAYSGIB ) )
		 return true;
	
	return false;
	
}

void CHL1NPCTalker::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_HL1TALKER_FOLLOW_WALK_PATH_FOR_UNITS:
		{
			GetNavigator()->SetMovementActivity( ACT_WALK );
			break;
		}
		case TASK_TALKER_SPEAK:
			// ask question or make statement
			FIdleSpeak();
			TaskComplete();
			break;
		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//=========================================================
// FIdleSpeak
// ask question of nearby friend, or make statement
//=========================================================
int CHL1NPCTalker::FIdleSpeak ( void )
{ 
	if (!IsOkToSpeak())
		return FALSE;

	// if there is a friend nearby to speak to, play sentence, set friend's response time, return
	// try to talk to any standing or sitting scientists nearby
	CBaseEntity *pentFriend = FindNearestFriend( false );
	CHL1NPCTalker *pentTalker = dynamic_cast<CHL1NPCTalker *>( pentFriend );
	if (pentTalker && random->RandomInt(0,1) )
	{
		Speak( TLK_QUESTION );
		SetSpeechTarget( pentFriend );

		pentTalker->SetSpeechTarget( this );
		pentTalker->SetCondition( COND_TALKER_RESPOND_TO_QUESTION );
		pentTalker->SetSchedule( SCHED_TALKER_IDLE_RESPONSE );
		pentTalker->GetExpresser()->BlockSpeechUntil( GetExpresser()->GetTimeSpeechComplete() );

		GetExpresser()->BlockSpeechUntil( gpGlobals->curtime + random->RandomFloat(4.8, 5.2) );

		//DevMsg( "Asking some question!\n" );
		return TRUE;
	}
	else if ( random->RandomInt(0,1)) 	// otherwise, play an idle statement
	{
		//DevMsg( "Making idle statement!\n" );

		Speak( TLK_IDLE );
		// set global min delay for next conversation
		GetExpresser()->BlockSpeechUntil( gpGlobals->curtime + random->RandomFloat(4.8, 5.2) );
		return TRUE;
	}

	// never spoke
	GetExpresser()->BlockSpeechUntil( 0 );
	m_flNextIdleSpeechTime = gpGlobals->curtime + 3;
	return FALSE;
}



bool CHL1NPCTalker::IsValidSpeechTarget( int flags, CBaseEntity *pEntity )
{
	if ( pEntity == this )
		return false;

	CHL1NPCTalker *pentTarget = dynamic_cast<CHL1NPCTalker *>( pEntity );
	if ( pentTarget )
	{
		if ( !(flags & AIST_IGNORE_RELATIONSHIP) )
		{
			if ( pEntity->IsPlayer() )
			{
				if ( !IsPlayerAlly( (CBasePlayer *)pEntity ) )
					return false;
			}
			else
			{
				if ( IRelationType( pEntity ) != D_LI )
					return false;
			}
		}		

		if ( !pEntity->IsAlive() )
			// don't dead people
			return false;

		// Ignore no-target entities
		if ( pEntity->GetFlags() & FL_NOTARGET )
			return false;

		CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
		if ( pNPC )
		{
			// If not a NPC for some reason, or in a script.
			//if ( (pNPC->m_NPCState == NPC_STATE_SCRIPT || pNPC->m_NPCState == NPC_STATE_PRONE))
			//	return false;

			if ( pNPC->IsInAScript() )
				return false;

			// Don't bother people who don't want to be bothered
			if ( !pNPC->CanBeUsedAsAFriend() )
				return false;
		}

		if ( flags & AIST_FACING_TARGET )
		{
			if ( pEntity->IsPlayer() )
				return HasCondition( COND_SEE_PLAYER );
			else if ( !FInViewCone( pEntity ) )
				return false;
		}

		return FVisible( pEntity );
	}
	else
		return BaseClass::IsValidSpeechTarget( flags, pEntity );
}


int CHL1NPCTalker::SelectSchedule ( void )
{
	switch( m_NPCState )
	{
	case NPC_STATE_PRONE:
		{
			if (m_bInBarnacleMouth)
			{
				return SCHED_HL1TALKER_BARNACLE_CHOMP;
			}
			else
			{
				return SCHED_HL1TALKER_BARNACLE_HIT;
			}
		}
	}

	return BaseClass::SelectSchedule();
}

void CHL1NPCTalker::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Barney.Close" );
}

bool CHL1NPCTalker::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if (interactionType == g_interactionBarnacleVictimDangle)
	{
		// Force choosing of a new schedule
		ClearSchedule( "NPC talker being eaten by a barnacle" );
		m_bInBarnacleMouth	= true;
		return true;
	}
	else if ( interactionType == g_interactionBarnacleVictimReleased )
	{
		SetState ( NPC_STATE_IDLE );

		CPASAttenuationFilter filter( this );

		CSoundParameters params;

		if ( GetParametersForSound( "Barney.Close", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_nPitch = GetExpresser()->GetVoicePitch();

			EmitSound( filter, entindex(), ep );
		}

		m_bInBarnacleMouth	= false;
		SetAbsVelocity( vec3_origin );
		SetMoveType( MOVETYPE_STEP );
		return true;
	}
	else if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		if ( GetFlags() & FL_ONGROUND )
		{
			SetGroundEntity( NULL );
		}
		
		if ( GetState() == NPC_STATE_SCRIPT )
		{
			if ( m_hCine )
			{
				 m_hCine->CancelScript();
			}
		}

		SetState( NPC_STATE_PRONE );
		ClearSchedule( "NPC talker grabbed by a barnacle" );
		
		CTakeDamageInfo info;
		PainSound( info );
		return true;
	}
	return false;
}

void CHL1NPCTalker::StartFollowing(	CBaseEntity *pLeader )
{
	if ( !HasSpawnFlags( SF_NPC_GAG ) )
	{
		if ( m_iszUse != NULL_STRING )
		{
			PlaySentence( STRING( m_iszUse ), 0.0f );
		}
		else
		{
			Speak( TLK_STARTFOLLOW );
		}

		SetSpeechTarget( pLeader );
	}

	BaseClass::StartFollowing( pLeader );
}

int CHL1NPCTalker::PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener )
{
	if( hl1_debug_sentence_volume.GetBool() )
	{
		Msg( "SENTENCE: %s Vol:%f SndLevel:%d\n", GetDebugName(), volume, soundlevel );
	}

	if( hl1_fixup_sentence_sndlevel.GetBool() )
	{
		if( soundlevel < SNDLVL_TALKING )
		{
			soundlevel = SNDLVL_TALKING;
		}
	}

	return BaseClass::PlayScriptedSentence( pszSentence, delay, volume, soundlevel, bConcurrent, pListener );
}

Disposition_t CHL1NPCTalker::IRelationType( CBaseEntity *pTarget )
{
	if ( pTarget->IsPlayer() )
	{
		if ( HasMemory( bits_MEMORY_PROVOKED ) )
		{
			return D_HT;
		}
	}

	return BaseClass::IRelationType( pTarget );
}

void CHL1NPCTalker::Touch( CBaseEntity *pOther )
{
	if ( m_NPCState == NPC_STATE_SCRIPT )
		 return;

	BaseClass::Touch(pOther);
}

void CHL1NPCTalker::StopFollowing( void )
{
	if ( !(m_afMemory & bits_MEMORY_PROVOKED) )
	{
		if ( !HasSpawnFlags( SF_NPC_GAG ) )
		{
			if ( m_iszUnUse != NULL_STRING )
			{
				PlaySentence( STRING( m_iszUnUse ), 0.0f );
			}
			else
			{
				Speak( TLK_STOPFOLLOW );
			}

			SetSpeechTarget( GetFollowTarget() );
		}
	}

	BaseClass::StopFollowing();
}

void CHL1NPCTalker::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( info.GetDamage() >= 1.0 && !(info.GetDamageType() & DMG_SHOCK ) )
	{
		UTIL_BloodImpact( ptr->endpos, vecDir, BloodColor(), 4 );
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

void CHL1NPCTalker::FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Don't allow use during a scripted_sentence
	if ( GetUseTime() > gpGlobals->curtime )
		return;

	if ( m_hCine && !m_hCine->CanInterrupt() )
		 return;

	if ( pCaller != NULL && pCaller->IsPlayer() )
	{
		// Pre-disaster followers can't be used
		if ( m_spawnflags & SF_NPC_PREDISASTER )
		{
			SetSpeechTarget( pCaller );
			DeclineFollowing();
			return;
		}
	}

	BaseClass::FollowerUse( pActivator, pCaller, useType, value );
}

int CHL1NPCTalker::TranslateSchedule( int scheduleType )
{
	return BaseClass::TranslateSchedule( scheduleType );
}

float CHL1NPCTalker::PickLookTarget( bool bExcludePlayers, float minTime, float maxTime )
{
	return random->RandomFloat( 5.0f, 10.0f );
}

void CHL1NPCTalker::IdleHeadTurn( CBaseEntity *pTarget, float flDuration, float flImportance )
{
	// Must be able to turn our head
	if (!(CapabilitiesGet() & bits_CAP_TURN_HEAD))
		return;

	// If the target is invalid, or we're in a script, do nothing
	if ( ( !pTarget ) || ( m_NPCState == NPC_STATE_SCRIPT ) )
		return;

	// Fill in a duration if we haven't specified one
	if ( flDuration == 0.0f )
	{
		 flDuration = random->RandomFloat( 2.0, 4.0 );
	}

	// Add a look target
	AddLookTarget( pTarget, 1.0, flDuration );
}

void CHL1NPCTalker::SetHeadDirection( const Vector &vTargetPos, float flInterval)
{
#ifdef TALKER_LOOK
	// Draw line in body, head, and eye directions
	Vector vEyePos = EyePosition();
	Vector vHeadDir = HeadDirection3D();
	Vector vBodyDir = BodyDirection2D();

	//UNDONE <<TODO>>
	// currently eye dir just returns head dir, so use vTargetPos for now
	//Vector vEyeDir;	w
	//EyeDirection3D(&vEyeDir);
	NDebugOverlay::Line( vEyePos, vEyePos+(50*vHeadDir), 255, 0, 0, false, 0.1 );
	NDebugOverlay::Line( vEyePos, vEyePos+(50*vBodyDir), 0, 255, 0, false, 0.1 );
	NDebugOverlay::Line( vEyePos, vTargetPos, 0, 0, 255, false, 0.1 );
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL1NPCTalker::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;
	
	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );

    data.m_nMaterial = 1;
	data.m_nHitBox = -m_iHealth;

	data.m_nColor = BloodColor();
	
	DispatchEffect( "HL1Gib", data );

	CSoundEnt::InsertSound( SOUND_MEAT, GetAbsOrigin(), 256, 0.5f, this );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL1NPCTalker::OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, CBaseDoor *pDoor, float distClear, AIMoveResult_t *pResult )
{
	// If we can't get through the door, try and open it
	if ( BaseClass::OnObstructingDoor( pMoveGoal, pDoor, distClear, pResult ) )
	{
		if  ( IsMoveBlocked( *pResult ) && pMoveGoal->directTrace.vHitNormal != vec3_origin )
		{
			// Can't do anything if the door's locked
			if ( !pDoor->m_bLocked && !pDoor->HasSpawnFlags(SF_DOOR_NONPCS) )
			{
				// Tell the door to open
				variant_t emptyVariant;
				pDoor->AcceptInput( "Open", this, this, emptyVariant, USE_TOGGLE );
				*pResult = AIMR_OK;
			}
		}
		return true;
	}

	return false;
}

// HL1 version - never return Ragdoll as the automatic schedule at the end of a 
// scripted sequence
int CHL1NPCTalker::SelectDeadSchedule()
{
	// Alread dead (by animation event maybe?)
	// Is it safe to set it to SCHED_NONE?
	if ( m_lifeState == LIFE_DEAD )
		 return SCHED_NONE;

	CleanupOnDeath();
	return SCHED_DIE;
}


AI_BEGIN_CUSTOM_NPC( monster_hl1talker, CHL1NPCTalker )

	DECLARE_TASK( TASK_HL1TALKER_FOLLOW_WALK_PATH_FOR_UNITS )

	//=========================================================
	// > SCHED_HL1TALKER_MOVE_AWAY_FOLLOW
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_HL1TALKER_FOLLOW_MOVE_AWAY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_TARGET_FACE"
		"		 TASK_STORE_LASTPOSITION				0"
		"		 TASK_MOVE_AWAY_PATH					100"
		"		 TASK_HL1TALKER_FOLLOW_WALK_PATH_FOR_UNITS	100"
		"		 TASK_STOP_MOVING						0"
		"		 TASK_FACE_PLAYER						0"
		"		 TASK_SET_ACTIVITY						ACT_IDLE"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_HL1TALKER_IDLE_SPEAK_WAIT
	//=========================================================
	DEFINE_SCHEDULE 
	(
		SCHED_HL1TALKER_IDLE_SPEAK_WAIT,

		"	Tasks"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"	// Stop and talk
		"		TASK_FACE_PLAYER			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_HL1TALKER_BARNACLE_HIT
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_HL1TALKER_BARNACLE_HIT,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_HIT"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_HL1TALKER_BARNACLE_PULL"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_HL1TALKER_BARNACLE_PULL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_HL1TALKER_BARNACLE_PULL,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_PULL"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_HL1TALKER_BARNACLE_CHOMP
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_HL1TALKER_BARNACLE_CHOMP,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_CHOMP"
		"		 TASK_SET_SCHEDULE			SCHEDULE:SCHED_HL1TALKER_BARNACLE_CHEW"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_HL1TALKER_BARNACLE_CHEW
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_HL1TALKER_BARNACLE_CHEW,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_CHEW"
	)

AI_END_CUSTOM_NPC()
