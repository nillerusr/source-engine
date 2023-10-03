// Swarm Civilian corrupted by SynTek chemicals into being a crazed zombie like thing
#include "cbase.h"

#include "doors.h"

#include "simtimer.h"
#include "npc_BaseZombie.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "gib.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "asw_zombie.h"
#include "asw_fx_shared.h"
#include "EntityFlame.h"
#include "asw_zombie.h"
#include "asw_gamerules.h"
#include "asw_burning.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ACT_FLINCH_PHYSICS

extern ConVar showhitlocation;
ConVar	sk_asw_zombie_health( "sk_asw_zombie_health","100");
ConVar asw_zombie_eye_glow("asw_zombie_eye_glow", "1");
// asw - how much extra damage to do to burning aliens
ConVar asw_fire_zombie_damage_scale("asw_fire_zombie_damage_scale", "3.0", FCVAR_CHEAT );

#define ZOMBIE_GLOW_SPRITE "sprites/glow1.vmt"

envelopePoint_t envASWZombieMoanVolumeFast[] =
{
	{	7.0f, 7.0f,
		0.1f, 0.1f,
	},
	{	0.0f, 0.0f,
		0.2f, 0.3f,
	},
};

envelopePoint_t envASWZombieMoanVolume[] =
{
	{	1.0f, 1.0f,
		0.1f, 0.1f,
	},
	{	1.0f, 1.0f,
		0.2f, 0.2f,
	},
	{	0.0f, 0.0f,
		0.3f, 0.4f,
	},
};

envelopePoint_t envASWZombieMoanVolumeLong[] =
{
	{	1.0f, 1.0f,
		0.3f, 0.5f,
	},
	{	1.0f, 1.0f,
		0.6f, 1.0f,
	},
	{	0.0f, 0.0f,
		0.3f, 0.4f,
	},
};

envelopePoint_t envASWZombieMoanIgnited[] =
{
	{	1.0f, 1.0f,
		0.5f, 1.0f,
	},
	{	1.0f, 1.0f,
		30.0f, 30.0f,
	},
	{	0.0f, 0.0f,
		0.5f, 1.0f,
	},
};


//=============================================================================
//=============================================================================

LINK_ENTITY_TO_CLASS( asw_zombie, CASW_Zombie );
// asw: no placing just torsos
//LINK_ENTITY_TO_CLASS( asw_zombie_torso, CASW_Zombie );

//---------------------------------------------------------
//---------------------------------------------------------
const char *CASW_Zombie::pASWMoanSounds[] =
{
	 "ASW_Zombie.Moan1",
	 "ASW_Zombie.Moan2",
	 "ASW_Zombie.Moan3",
	 "ASW_Zombie.Moan4",
};

//=========================================================
// Conditions
//=========================================================
enum
{
	COND_ASW_BLOCKED_BY_DOOR = LAST_BASE_ZOMBIE_CONDITION,
	COND_ASW_DOOR_OPENED,
	COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED,
};

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_ASW_ZOMBIE_BASH_DOOR = LAST_BASE_ZOMBIE_SCHEDULE,
	SCHED_ASW_ZOMBIE_WANDER_ANGRILY,
	SCHED_ASW_ZOMBIE_CHARGE_ENEMY,
	SCHED_ASW_ZOMBIE_FAIL,
};

//=========================================================
// Tasks
//=========================================================
enum
{
	TASK_ASW_ZOMBIE_EXPRESS_ANGER = LAST_BASE_ZOMBIE_TASK,
	TASK_ASW_ZOMBIE_YAW_TO_DOOR,
	TASK_ASW_ZOMBIE_ATTACK_DOOR,
	TASK_ASW_ZOMBIE_CHARGE_ENEMY,
};

//-----------------------------------------------------------------------------

int ACT_ASW_ZOMBIE_TANTRUM;
int ACT_ASW_ZOMBIE_WALLPOUND;

IMPLEMENT_SERVERCLASS_ST(CASW_Zombie, DT_ASW_Zombie)
	SendPropBool(SENDINFO(m_bOnFire)),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Zombie )
	DEFINE_FIELD( m_hBlockingDoor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDoorBashYaw, FIELD_FLOAT ),
	DEFINE_EMBEDDED( m_DurationDoorBash ),
	DEFINE_EMBEDDED( m_NextTimeToStartDoorBash ),
	DEFINE_FIELD( m_vPositionCharged, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hSpawner, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHoldoutAlien, FIELD_BOOLEAN ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Zombie::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/Swarm/CivilianZ/CivilianZ.mdl" );
	PrecacheModel( "models/zombie/classic_torso.mdl" );
	PrecacheModel( "models/zombie/classic_legs.mdl" );

	PrecacheScriptSound( "Zombie.FootstepRight" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombie.ScuffRight" );
	PrecacheScriptSound( "Zombie.ScuffLeft" );
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombie.Pain" );
	PrecacheScriptSound( "Zombie.Die" );
	PrecacheScriptSound( "Zombie.Alert" );
	PrecacheScriptSound( "Zombie.Idle" );
	PrecacheScriptSound( "Zombie.Attack" );

	PrecacheScriptSound( "NPC_BaseZombie.Moan1" );
	PrecacheScriptSound( "NPC_BaseZombie.Moan2" );
	PrecacheScriptSound( "NPC_BaseZombie.Moan3" );
	PrecacheScriptSound( "NPC_BaseZombie.Moan4" );

	PrecacheScriptSound( "ASW_Zombie.Pain" );
	PrecacheScriptSound( "ASW_Zombie.Die" );
	PrecacheScriptSound( "ASW_Zombie.Alert" );
	PrecacheScriptSound( "ASW_Zombie.Idle" );
	PrecacheScriptSound( "ASW_Zombie.Attack" );
	PrecacheScriptSound( "ASW_Zombie.Moan1" );
	PrecacheScriptSound( "ASW_Zombie.Moan2" );
	PrecacheScriptSound( "ASW_Zombie.Moan3" );
	PrecacheScriptSound( "ASW_Zombie.Moan4" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CASW_Zombie::Spawn( void )
{
	Precache();

	// asw: no placing just torsos
	//if( FClassnameIs( this, "asw_zombie" ) )
	//{
		m_fIsTorso = false;
	//}
	//else
	//{
		// This was placed as an npc_zombie_torso
		//m_fIsTorso = true;
	//}


	m_fIsHeadless = false;

	SetBloodColor( BLOOD_COLOR_RED );
	SetHealthByDifficultyLevel();
	m_flFieldOfView		= 0.2;

	SetDistLook( 768.0f );
	m_flDistTooFar = 1024.0f;
	if ( HasSpawnFlags( SF_NPC_LONG_RANGE ) )
	{
		m_flDistTooFar = 2248.0f;
		SetDistLook( 1200.0f );
	}

	CapabilitiesClear();

	//GetNavigator()->SetRememberStaleNodes( false );

	BaseClass::Spawn();

	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 4.0 );

	if (asw_zombie_eye_glow.GetBool())
		CreateEyeGlows();
}

void CASW_Zombie::UpdateOnRemove( void )
{
	RemoveEyeGlows(0.0f);
	BaseClass::UpdateOnRemove();
}

// give our zombies 360 degree vision
bool CASW_Zombie::FInViewCone( const Vector &vecSpot )
{
	return true;
}

// player (and player controlled marines) always avoid zombies
bool CASW_Zombie::ShouldPlayerAvoid( void )
{
	return true;
}

float CASW_Zombie::GetIdealSpeed() const
{
	return BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CASW_Zombie::PrescheduleThink( void )
{
  	if( gpGlobals->curtime > m_flNextMoanSound )
  	{
  		if( CanPlayMoanSound() )
  		{
			// Classic guy idles instead of moans.
			IdleSound();

  			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.0, 5.0 );
  		}
  		else
 		{
  			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 2.0 );
  		}
  	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CASW_Zombie::SelectSchedule ( void )
{
	if( HasCondition( COND_PHYSICS_DAMAGE ) && !m_ActBusyBehavior.IsActive() )
	{
		return SCHED_FLINCH_PHYSICS;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CASW_Zombie::FootstepSound( bool fRightFoot )
{
	if( fRightFoot )
	{
		EmitSound(  "Zombie.FootstepRight" );
	}
	else
	{
		EmitSound( "Zombie.FootstepLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CASW_Zombie::FootscuffSound( bool fRightFoot )
{
	if( fRightFoot )
	{
		EmitSound( "Zombie.ScuffRight" );
	}
	else
	{
		EmitSound( "Zombie.ScuffLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CASW_Zombie::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CASW_Zombie::AttackMissSound( void )
{
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Zombie::PainSound(  const CTakeDamageInfo &info  )
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if ( IsOnFire() )
	{
		return;
	}

	EmitSound( "ASW_Zombie.Pain" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CASW_Zombie::DeathSound() 
{
	EmitSound( "ASW_Zombie.Die" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Zombie::AlertSound( void )
{
	EmitSound( "ASW_Zombie.Alert" );

	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat( 2.0, 4.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of zombie.
//-----------------------------------------------------------------------------
const char *CASW_Zombie::GetMoanSound( int nSound )
{
	return pASWMoanSounds[ nSound % ARRAYSIZE( pASWMoanSounds ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CASW_Zombie::IdleSound( void )
{
	if( GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0 )
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if( IsSlumped() )
	{
		// Sleeping zombies are quiet.
		return;
	}

	EmitSound( "ASW_Zombie.Idle" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CASW_Zombie::AttackSound( void )
{
	EmitSound( "ASW_Zombie.Attack" );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
const char *CASW_Zombie::GetHeadcrabClassname( void )
{
	return "npc_headcrab";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CASW_Zombie::GetHeadcrabModel( void )
{
	return "models/headcrabclassic.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CASW_Zombie::GetLegsModel( void )
{
	return "models/zombie/classic_legs.mdl";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CASW_Zombie::GetTorsoModel( void )
{
	return "models/zombie/classic_torso.mdl";
}


//---------------------------------------------------------
//---------------------------------------------------------
void CASW_Zombie::SetZombieModel( void )
{
	Hull_t lastHull = GetHullType();

	// asw: no placing just torsos
	//if ( m_fIsTorso )
	//{
		//SetModel( "models/zombie/classic_torso.mdl" );
		//SetHullType( HULL_TINY );
	//}
	//else
	//{
		SetModel( "models/Swarm/CivilianZ/CivilianZ.mdl" );
		SetHullType( HULL_HUMAN );
	//}

	//SetBodygroup( ZOMBIE_BODYGROUP_HEADCRAB, !m_fIsHeadless );

	SetHullSizeNormal( true );
	SetDefaultEyeOffset();
	SetActivity( ACT_IDLE );

	// hull changed size, notify vphysics
	// UNDONE: Solve this generally, systematically so other
	// NPCs can change size
	if ( lastHull != GetHullType() )
	{
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}
}

//---------------------------------------------------------
// Classic zombie only uses moan sound if on fire.
//---------------------------------------------------------
void CASW_Zombie::MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize )
{
	if( IsOnFire() )
	{
		BaseClass::MoanSound( pEnvelope, iEnvelopeSize );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CASW_Zombie::ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold )
{
	// asw: no placing just torsos
	return false;

	if( IsSlumped() ) 
	{
		// Never break apart a slouched zombie. This is because the most fun
		// slouched zombies to kill are ones sleeping leaning against explosive
		// barrels. If you break them in half in the blast, the force of being
		// so close to the explosion makes the body pieces fly at ridiculous 
		// velocities because the pieces weigh less than the whole.
		return false;
	}

	return BaseClass::ShouldBecomeTorso( info, flDamageThreshold );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_Zombie::GatherConditions( void )
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] = 
	{
		COND_ASW_BLOCKED_BY_DOOR,
		COND_ASW_DOOR_OPENED,
		COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	if ( m_hBlockingDoor == NULL || 
		 ( m_hBlockingDoor->m_toggle_state == TS_AT_TOP || 
		   m_hBlockingDoor->m_toggle_state == TS_GOING_UP )  )
	{
		ClearCondition( COND_ASW_BLOCKED_BY_DOOR );
		if ( m_hBlockingDoor != NULL )
		{
			SetCondition( COND_ASW_DOOR_OPENED );
			m_hBlockingDoor = NULL;
		}
	}
	else
		SetCondition( COND_ASW_BLOCKED_BY_DOOR );

	if ( ConditionInterruptsCurSchedule( COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED ) )
	{
		if ( GetNavigator()->IsGoalActive() )
		{
			const float CHARGE_RESET_TOLERANCE = 60.0;
			if ( !GetEnemy() ||
				 ( m_vPositionCharged - GetEnemyLKP()  ).Length() > CHARGE_RESET_TOLERANCE )
			{
				SetCondition( COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED );
			}
				 
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

int CASW_Zombie::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( HasCondition( COND_ASW_BLOCKED_BY_DOOR ) && m_hBlockingDoor != NULL )
	{
		ClearCondition( COND_ASW_BLOCKED_BY_DOOR );
		if ( m_NextTimeToStartDoorBash.Expired() && failedSchedule != SCHED_ASW_ZOMBIE_BASH_DOOR )
			return SCHED_ASW_ZOMBIE_BASH_DOOR;
		m_hBlockingDoor = NULL;
	}

	if ( failedSchedule != SCHED_ASW_ZOMBIE_CHARGE_ENEMY && 
		 IsPathTaskFailure( taskFailCode ) &&
		 random->RandomInt( 1, 100 ) < 50 )
	{
		return SCHED_ASW_ZOMBIE_CHARGE_ENEMY;
	}

	if ( failedSchedule != SCHED_ASW_ZOMBIE_WANDER_ANGRILY &&
		 ( failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY || 
		   failedSchedule == SCHED_CHASE_ENEMY_FAILED ) )
	{
		return SCHED_ASW_ZOMBIE_WANDER_ANGRILY;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//---------------------------------------------------------
//---------------------------------------------------------

int CASW_Zombie::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_COMBAT_FACE && IsUnreachable( GetEnemy() ) )
		return SCHED_TAKE_COVER_FROM_ENEMY;

	if ( !m_fIsTorso && scheduleType == SCHED_FAIL )
		return SCHED_ASW_ZOMBIE_FAIL;

	return BaseClass::TranslateSchedule( scheduleType );
}

//---------------------------------------------------------

Activity CASW_Zombie::NPC_TranslateActivity( Activity newActivity )
{
	newActivity = BaseClass::NPC_TranslateActivity( newActivity );

	if ( newActivity == ACT_RUN )
		return ACT_WALK;

	return newActivity;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CASW_Zombie::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );
}

//---------------------------------------------------------
//---------------------------------------------------------

void CASW_Zombie::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ASW_ZOMBIE_EXPRESS_ANGER:
		{
			if ( random->RandomInt( 1, 4 ) == 2 )
			{
				SetIdealActivity( (Activity)ACT_ASW_ZOMBIE_TANTRUM );
			}
			else
			{
				TaskComplete();
			}

			break;
		}

	case TASK_ASW_ZOMBIE_YAW_TO_DOOR:
		{
			AssertMsg( m_hBlockingDoor != NULL, "Expected condition handling to break schedule before landing here" );
			if ( m_hBlockingDoor != NULL )
			{
				GetMotor()->SetIdealYaw( m_flDoorBashYaw );
			}
			TaskComplete();
			break;
		}

	case TASK_ASW_ZOMBIE_ATTACK_DOOR:
		{
		 	m_DurationDoorBash.Reset();
			SetIdealActivity( SelectDoorBash() );
			break;
		}

	case TASK_ASW_ZOMBIE_CHARGE_ENEMY:
		{
			if ( !GetEnemy() )
				TaskFail( FAIL_NO_ENEMY );
			else if ( GetNavigator()->SetVectorGoalFromTarget( GetEnemy()->GetLocalOrigin() ) )
			{
				m_vPositionCharged = GetEnemy()->GetLocalOrigin();
				TaskComplete();
			}
			else
				TaskFail( FAIL_NO_ROUTE );
			break;
		}

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

void CASW_Zombie::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ASW_ZOMBIE_ATTACK_DOOR:
		{
			if ( IsActivityFinished() )
			{
				if ( m_DurationDoorBash.Expired() )
				{
					TaskComplete();
					m_NextTimeToStartDoorBash.Reset();
				}
				else
					ResetIdealActivity( SelectDoorBash() );
			}
			break;
		}

	case TASK_ASW_ZOMBIE_CHARGE_ENEMY:
		{
			break;
		}

	case TASK_ASW_ZOMBIE_EXPRESS_ANGER:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

bool CASW_Zombie::OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, CBaseDoor *pDoor, 
							  float distClear, AIMoveResult_t *pResult )
{
	if ( BaseClass::OnObstructingDoor( pMoveGoal, pDoor, distClear, pResult ) )
	{
		if  ( IsMoveBlocked( *pResult ) && pMoveGoal->directTrace.vHitNormal != vec3_origin )
		{
			m_hBlockingDoor = pDoor;
			m_flDoorBashYaw = UTIL_VecToYaw( pMoveGoal->directTrace.vHitNormal * -1 );	
		}
		return true;
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------

Activity CASW_Zombie::SelectDoorBash()
{
	if ( random->RandomInt( 1, 3 ) == 1 )
		return ACT_MELEE_ATTACK1;
	return (Activity)ACT_ASW_ZOMBIE_WALLPOUND;
}

//---------------------------------------------------------
// Zombies should scream continuously while burning, so long
// as they are alive.
//---------------------------------------------------------
void CASW_Zombie::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	return;		// asw asw_ignite instead
	/*
 	if( !IsOnFire() && IsAlive() )
	{
		BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );

		RemoveSpawnFlags( SF_NPC_GAG );
		
		MoanSound( envASWZombieMoanIgnited, ARRAYSIZE( envASWZombieMoanIgnited ) );

		if( m_pMoanSound )
		{
			ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, 120, 1.0 );
			ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 1, 1.0 );
		}
	}*/
}

//---------------------------------------------------------
//---------------------------------------------------------
int CASW_Zombie::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
#ifndef HL2_EPISODIC
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		if( !m_fIsTorso && info.GetDamage() > (m_iMaxHealth/3) )
		{
			// Always flinch if damaged a lot by buckshot, even if not shot in the head.
			// The reason for making sure we did at least 1/3rd of the zombie's max health
			// is so the zombie doesn't flinch every time the odd shotgun pellet hits them,
			// and so the maximum number of times you'll see a zombie flinch like this is 2.(sjb)
			AddGesture( ACT_GESTURE_FLINCH_HEAD );
		}
	}
#endif // HL2_EPISODIC
	// catching on fire
	int result = 0;

	// scale burning damage up
	if (dynamic_cast<CEntityFlame*>(info.GetAttacker()))
	{
		CTakeDamageInfo newDamage = info;		
		newDamage.ScaleDamage(asw_fire_zombie_damage_scale.GetFloat());
		result = BaseClass::OnTakeDamage_Alive(newDamage);
	}
	else
	{
		result = BaseClass::OnTakeDamage_Alive(info);
	}

	// if we take fire damage, catch on fire
	if (result > 0 && (info.GetDamageType() & DMG_BURN))
		ASW_Ignite(30.0f, 0, info.GetAttacker());

	//CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
	//if (pMarine)
		//pMarine->HurtAlien(this);

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CASW_Zombie::IsHeavyDamage( const CTakeDamageInfo &info )
{
#ifdef HL2_EPISODIC
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		if ( !m_fIsTorso && info.GetDamage() > (m_iMaxHealth/3) )
			return true;
	}

	// Randomly treat all damage as heavy
	if ( info.GetDamageType() & (DMG_BULLET | DMG_BUCKSHOT) )
	{
		// Don't randomly flinch if I'm melee attacking
		if ( !HasCondition(COND_CAN_MELEE_ATTACK1) && (RandomFloat() > 0.5) )
		{
			// Randomly forget I've flinched, so that I'll be forced to play a big flinch
			// If this doesn't happen, it means I may not fully flinch if I recently flinched
			if ( RandomFloat() > 0.75 )
			{
				Forget(bits_MEMORY_FLINCHED);
			}

			return true;
		}
	}
#endif // HL2_EPISODIC

	return BaseClass::IsHeavyDamage(info);
}


//---------------------------------------------------------
//---------------------------------------------------------
void CASW_Zombie::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if( !m_fIsTorso && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) && !m_ActBusyBehavior.IsActive() )
	{
		SetCustomInterruptCondition( COND_PHYSICS_DAMAGE );
	}
}

HeadcrabRelease_t CASW_Zombie::ShouldReleaseHeadcrab( const CTakeDamageInfo &info, float flDamageThreshold )
{	
	return RELEASE_NO;
}

void CASW_Zombie::StudioFrameAdvance()
{
	// vary out playback rate for crazy zombie-ness
	//m_flPlaybackRate = RandomFloat(2.0f, 3.0f);
	m_flPlaybackRate = 1.5f;
	BaseClass::StudioFrameAdvance();
}

// make the bleeding more pronounced
void CASW_Zombie::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	bool bDebug = showhitlocation.GetBool();

	switch ( ptr->hitgroup )
	{
	case HITGROUP_GENERIC:
		if( bDebug ) DevMsg("Hit Location: Generic\n");
		break;

	// hit gear, react but don't bleed
	case HITGROUP_GEAR:
		subInfo.SetDamage( 0.01 );
		ptr->hitgroup = HITGROUP_GENERIC;
		if( bDebug ) DevMsg("Hit Location: Gear\n");
		break;

	case HITGROUP_HEAD:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Head\n");
		break;

	case HITGROUP_CHEST:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Chest\n");
		break;

	case HITGROUP_STOMACH:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Stomach\n");
		break;

	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Arm\n");
		break
			;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Leg\n");
		break;

	default:
		if( bDebug ) DevMsg("Hit Location: UNKNOWN\n");
		break;
	}

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK ) )
	{
		if( !IsPlayer() || ( IsPlayer() && gpGlobals->maxClients > 1 ) )
		{
			// NPC's always bleed. Players only bleed in multiplayer.
			//SpawnBlood( ptr->endpos, vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.
			//UTIL_ASW_DroneBleed( ptr->endpos, vecDir, 4 );
			UTIL_ASW_BloodDrips( GetAbsOrigin()+Vector(0,0,60)+vecDir*3, vecDir, BloodColor(), 5 );
		}

		TraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );

		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
	}

	// Airboat gun will impart major force if it's about to kill him....
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		if ( subInfo.GetDamage() >= GetHealth() )
		{
			float flMagnitude = subInfo.GetDamageForce().Length();
			if ( (flMagnitude != 0.0f) && (flMagnitude < 400.0f * 65.0f) )
			{
				subInfo.ScaleDamageForce( 400.0f * 65.0f / flMagnitude );
			}
		}
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

void CASW_Zombie::OnRestore()
{
	BaseClass::OnRestore();

	CreateEyeGlows();
}

void CASW_Zombie::CreateEyeGlows( void )
{
	//Create our Eye sprites
	if ( m_pEyeGlow[0] == NULL )
	{
		m_pEyeGlow[0] = CSprite::SpriteCreate( ZOMBIE_GLOW_SPRITE, GetLocalOrigin(), false );
		m_pEyeGlow[0]->SetAttachment( this, LookupAttachment( "LeftEye" ) );
	
		m_pEyeGlow[0]->SetTransparency( kRenderTransAdd, 128, 0, 0, 128, kRenderFxNoDissipation );
		m_pEyeGlow[0]->SetBrightness( 164, 0.1f );
		m_pEyeGlow[0]->SetScale( 0.1f, 0.1f );
		m_pEyeGlow[0]->SetColor( 128, 0, 0 );
		m_pEyeGlow[0]->SetAsTemporary();
	}
	if ( m_pEyeGlow[1] == NULL )
	{
		m_pEyeGlow[1] = CSprite::SpriteCreate( ZOMBIE_GLOW_SPRITE, GetLocalOrigin(), false );
		m_pEyeGlow[1]->SetAttachment( this, LookupAttachment( "RightEye" ) );
		
		m_pEyeGlow[1]->SetTransparency( kRenderTransAdd, 128, 0, 0, 128, kRenderFxNoDissipation );
		m_pEyeGlow[1]->SetBrightness( 164, 0.1f );
		m_pEyeGlow[1]->SetScale( 0.1f, 0.1f );
		m_pEyeGlow[1]->SetColor( 128, 0, 0 );
		m_pEyeGlow[1]->SetAsTemporary();
	}
}

void CASW_Zombie::RemoveEyeGlows( float flDelay )
{
	if( m_pEyeGlow[0] )
	{
		m_pEyeGlow[0]->FadeAndDie( flDelay );
		m_pEyeGlow[0] = NULL;
	}

	if( m_pEyeGlow[1] )
	{
		m_pEyeGlow[1]->FadeAndDie( flDelay );
		m_pEyeGlow[1] = NULL;
	}
}

void CASW_Zombie::Event_Killed( const CTakeDamageInfo &info )
{	
	RemoveEyeGlows( 0.0f );

	BaseClass::Event_Killed( info );
}

void CASW_Zombie::SetSpawner(CASW_Base_Spawner* spawner)
{
	m_hSpawner = spawner;
}

void CASW_Zombie::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon /*= NULL */ )
{
	if (AllowedToIgnite())
	{
		if( IsOnFire() )
			return;

		m_bOnFire = true;
		if (ASWBurning())
			ASWBurning()->BurnEntity(this, pAttacker, flFlameLifetime, 0.4f, 5.0f * 0.4f);	// 5 dps, applied every 0.4 seconds

		/*
		CEntityFlame *pFlame = CEntityFlame::Create( this );
		if (pFlame)
		{
			if (pAttacker)
				pFlame->SetOwnerEntity(pAttacker);
			pFlame->SetLifetime( flFlameLifetime );
			AddFlag( FL_ONFIRE );

			SetEffectEntity( pFlame );

			if ( flSize > 0.0f )
			{
				pFlame->SetSize( flSize );
			}
		}
		*/

		m_OnIgnite.FireOutput( this, this );

		RemoveSpawnFlags( SF_NPC_GAG );
		
		MoanSound( envASWZombieMoanIgnited, ARRAYSIZE( envASWZombieMoanIgnited ) );

		if( m_pMoanSound )
		{
			ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, 120, 1.0 );
			ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 1, 1.0 );
		}
	}
}

void CASW_Zombie::Extinguish()
{
	m_bOnFire = false;
	if( m_pMoanSound )
	{
		ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 0, 2.0 );
		ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, 100, 2.0 );
		m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.0, 4.0 );
	}

	if (ASWBurning())
		ASWBurning()->Extinguish(this);
	RemoveFlag( FL_ONFIRE );
}


void CASW_Zombie::SetHealthByDifficultyLevel()
{
	SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(sk_asw_zombie_health.GetInt()));
}
	
//=============================================================================

AI_BEGIN_CUSTOM_NPC( asw_zombie, CASW_Zombie )

	DECLARE_CONDITION( COND_ASW_BLOCKED_BY_DOOR )
	DECLARE_CONDITION( COND_ASW_DOOR_OPENED )
	DECLARE_CONDITION( COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED )

	DECLARE_TASK( TASK_ASW_ZOMBIE_EXPRESS_ANGER )
	DECLARE_TASK( TASK_ASW_ZOMBIE_YAW_TO_DOOR )
	DECLARE_TASK( TASK_ASW_ZOMBIE_ATTACK_DOOR )
	DECLARE_TASK( TASK_ASW_ZOMBIE_CHARGE_ENEMY )
	
	DECLARE_ACTIVITY( ACT_ASW_ZOMBIE_TANTRUM );
	DECLARE_ACTIVITY( ACT_ASW_ZOMBIE_WALLPOUND );

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_ZOMBIE_BASH_DOOR,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_ASW_ZOMBIE_TANTRUM"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		"		TASK_ASW_ZOMBIE_YAW_TO_DOOR			0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_ASW_ZOMBIE_ATTACK_DOOR			0"
		""
		"	Interrupts"
		"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ASW_DOOR_OPENED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_ZOMBIE_WANDER_ANGRILY,

		"	Tasks"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			4"
		""
		"	Interrupts"
		"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ASW_DOOR_OPENED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_ZOMBIE_CHARGE_ENEMY,


		"	Tasks"
		"		TASK_ASW_ZOMBIE_CHARGE_ENEMY		0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_MELEE_ATTACK1" /* placeholder until frustration/rage/fence shake animation available */
		""
		"	Interrupts"
		"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ASW_DOOR_OPENED"
		"		COND_ASW_ZOMBIE_CHARGE_TARGET_MOVED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_ZOMBIE_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_ASW_ZOMBIE_TANTRUM"
		"		TASK_WAIT				1"
		"		TASK_WAIT_PVS			0"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1 "
		"		COND_CAN_RANGE_ATTACK2 "
		"		COND_CAN_MELEE_ATTACK1 "
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_GIVE_WAY"
		"		COND_ASW_DOOR_OPENED"
	)

AI_END_CUSTOM_NPC()

//=============================================================================
