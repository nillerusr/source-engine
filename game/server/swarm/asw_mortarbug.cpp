#include "cbase.h"
#include "asw_mortarbug.h"
#include "te_effect_dispatch.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "asw_parasite.h"
#include "soundenvelope.h"
#include "ai_memory.h"
#include "asw_gamerules.h"
#include "asw_weapon.h"
#include "asw_shareddefs.h"
#include "asw_weapon_assault_shotgun_shared.h"
#include "asw_mortarbug_shell_shared.h"
#include "particle_parse.h"
#include "ai_network.h"
#include "ai_networkmanager.h"
#include "ai_pathfinder.h"
#include "ai_link.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_MORTARBUG_MAX_ATTACK_DISTANCE 1500

LINK_ENTITY_TO_CLASS( asw_mortarbug, CASW_Mortarbug );

float CASW_Mortarbug::s_fNextSpawnSoundTime = 0;
float CASW_Mortarbug::s_fNextPainSoundTime = 0;

#define ASW_MORTARBUG_PROJECTILE "asw_mortarbug_shell"

BEGIN_DATADESC( CASW_Mortarbug )
	DEFINE_FIELD( m_fLastFireTime, FIELD_TIME ),	
	DEFINE_FIELD( m_fLastTouchHurtTime, FIELD_TIME ),
	DEFINE_FIELD( m_fGibTime, FIELD_TIME ),
	DEFINE_FIELD( m_flIdleDelay,			FIELD_TIME ),
	DEFINE_FIELD( m_vecSaveSpitVelocity,	FIELD_VECTOR ),
END_DATADESC()

ConVar asw_mortarbug_speedboost( "asw_mortarbug_speedboost", "1.0",FCVAR_CHEAT , "boost speed for the mortarbug" );
ConVar asw_mortarbug_touch_damage( "asw_mortarbug_touch_damage", "5",FCVAR_CHEAT , "Damage caused by mortarbug on touch" );
ConVar asw_mortarbug_spitspeed( "asw_mortarbug_spitspeed", "350", FCVAR_CHEAT, "Speed at which mortarbug grenade travels." );
ConVar asw_debug_mortarbug( "asw_debug_mortarbug", "0", FCVAR_NONE, "Display mortarbug debug info" );
ConVar asw_mortarbug_face_target("asw_mortarbug_face_target", "1", FCVAR_CHEAT, "Mortarbug faces his target when moving" );

extern ConVar sv_gravity;
extern ConVar asw_mortarbug_shell_gravity;	// TODO: Replace with proper spit projectile's gravity

// Anim Events
int AE_MORTARBUG_CHARGE;		// charging up to spit
int AE_MORTARBUG_LAUNCH;		// actual launch of the projectile

// Activities
int ACT_MORTARBUG_SPIT;

CASW_Mortarbug::CASW_Mortarbug()
{
	m_fLastFireTime = 0;
	m_fLastTouchHurtTime = 0;
	m_pszAlienModelName = SWARM_MORTARBUG_MODEL;
	m_nAlienCollisionGroup = ASW_COLLISION_GROUP_ALIEN;
}

CASW_Mortarbug::~CASW_Mortarbug()
{
}

void CASW_Mortarbug::Spawn( void )
{
	SetHullType(HULL_WIDE_SHORT);

	BaseClass::Spawn();
	
	SetHullType(HULL_WIDE_SHORT);
	UTIL_SetSize(this, Vector(-23,-23,0), Vector(23,23,69));
				
	m_iHealth	= ASWGameRules()->ModifyAlienHealthBySkillLevel(350);

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 );
		
	m_takedamage = DAMAGE_NO;	// alien is invulnerable until she finds her first enemy
}

void CASW_Mortarbug::Precache( void )
{
	PrecacheScriptSound( "ASW_MortarBug.Idle" );
	PrecacheScriptSound( "ASW_MortarBug.Pain" );
	PrecacheScriptSound( "ASW_MortarBug.Spit" );
	PrecacheScriptSound( "ASW_MortarBug.OnFire" );
	PrecacheScriptSound( "ASW_MortarBug.Death" );
	PrecacheParticleSystem( "mortar_launch" );

	UTIL_PrecacheOther( ASW_MORTARBUG_PROJECTILE );

	BaseClass::Precache();
}

float CASW_Mortarbug::GetIdealSpeed() const
{
	return asw_mortarbug_speedboost.GetFloat() * BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}


float CASW_Mortarbug::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

float CASW_Mortarbug::MaxYawSpeed( void )
{
	if ( m_bElectroStunned.Get() )
		return 0.1f;

	return 16.0f * asw_mortarbug_speedboost.GetFloat();
}

void CASW_Mortarbug::AlertSound()
{
	EmitSound( "ASW_MortarBug.Idle" );
}

void CASW_Mortarbug::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound && gpGlobals->curtime > s_fNextPainSoundTime)
	{		
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
		s_fNextPainSoundTime = gpGlobals->curtime + 1.0f;
		EmitSound("ASW_MortarBug.Pain");
	}
}

void CASW_Mortarbug::AttackSound()
{
	if (gpGlobals->curtime > s_fNextSpawnSoundTime)
	{
		EmitSound("ASW_MortarBug.Spit");
		s_fNextSpawnSoundTime = gpGlobals->curtime + 2.0f;
	}
}

void CASW_Mortarbug::IdleSound()
{
	EmitSound( "ASW_MortarBug.Idle" );
	m_flIdleDelay = gpGlobals->curtime + 4.0f;
}

void CASW_Mortarbug::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound("ASW_MortarBug.Death");
}

// make the mortarbug look at his enemy
bool CASW_Mortarbug::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
  	Vector		vecFacePosition = vec3_origin;
	CBaseEntity	*pFaceTarget = NULL;
	bool		bFaceTarget = false;

	if ( GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN )
  	{
		Vector vecEnemyLKP = GetEnemyLKP();
		
		// Only start facing when we're close enough
		if ( ( UTIL_DistApprox( vecEnemyLKP, GetAbsOrigin() ) < 1500 ) )
		{
			vecFacePosition = vecEnemyLKP;
			pFaceTarget = GetEnemy();
			bFaceTarget = true;
		}
	}

	// Face
	if ( bFaceTarget && asw_mortarbug_face_target.GetBool() )
	{
		AddFacingTarget( pFaceTarget, vecFacePosition, 1.0, 0.2 );
	}

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

void CASW_Mortarbug::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_MORTARBUG_LAUNCH )
	{
		// The point in our spit animation where we should actually spawn the projectile	
		AttackSound();
		SpawnProjectile();
		m_flNextAttack = gpGlobals->curtime + 2.0f;
		return;
	}
	else if ( nEvent == AE_MORTARBUG_CHARGE )
	{
		// TODO: Get ivan to make a pre-attack sound and play it here?
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

// harvester can attack without LOS so long as they're near enough
bool CASW_Mortarbug::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions)
{
	if (targetPos.DistTo(ownerPos) < ASW_MORTARBUG_MAX_ATTACK_DISTANCE)
		return true;

	return false;
}

bool CASW_Mortarbug::FCanCheckAttacks()
{
	if ( GetNavType() == NAV_CLIMB || GetNavType() == NAV_JUMP )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CASW_Mortarbug::InnateRange1MinRange( void )
{
	return 0;
}

float CASW_Mortarbug::InnateRange1MaxRange( void )
{
	return ASW_MORTARBUG_MAX_ATTACK_DISTANCE;
}

// make sure the harvester backs away when he's near us (he should sit back and lay critters)
int CASW_Mortarbug::RangeAttack1Conditions( float flDot, float flDist )
{
	if (GetEnemy() == NULL)
	{
		return( COND_NONE );
	}

	if ( gpGlobals->curtime < m_flNextAttack )
	{
		return( COND_NONE );
	}

	float flTooClose = InnateRange1MinRange();
	if ( flDist > InnateRange1MaxRange() )
	{
		return( COND_TOO_FAR_TO_ATTACK );
	}
	else if ( flDist < flTooClose )
	{
		return( COND_TOO_CLOSE_TO_ATTACK );
	}
	else if ( flDot < 0.65 )
	{
		return( COND_NOT_FACING_ATTACK );
	}

	return( COND_CAN_RANGE_ATTACK1 );
}

int CASW_Mortarbug::SelectSchedule()
{
	if ( HasCondition( COND_NEW_ENEMY ) && GetHealth() > 0 )
	{
		m_takedamage = DAMAGE_YES;
	}

	if ( HasCondition( COND_FLOATING_OFF_GROUND ) )
	{
		SetGravity( 1.0 );
		SetGroundEntity( NULL );
		return SCHED_FALL_TO_GROUND;
	}

	if (m_NPCState == NPC_STATE_COMBAT)
		return SelectMortarbugCombatSchedule();

	return BaseClass::SelectSchedule();
}

int CASW_Mortarbug::SelectMortarbugCombatSchedule()
{
	int nSched = SelectFlinchSchedule_ASW();
	if ( nSched != SCHED_NONE )
	{
		// if we flinch, push forward the next attack
		float spawn_interval = 2.0f;
		m_flNextAttack = gpGlobals->curtime + spawn_interval;
		return nSched;
	}

	if ( HasCondition(COND_NEW_ENEMY) && gpGlobals->curtime - GetEnemies()->FirstTimeSeen(GetEnemy()) < 2.0 )
	{
		return SCHED_WAKE_ANGRY;
	}
	
	if ( HasCondition( COND_ENEMY_DEAD ) )
	{
		// clear the current (dead) enemy and try to find another.
		SetEnemy( NULL );
		 
		if ( ChooseEnemy() )
		{
			ClearCondition( COND_ENEMY_DEAD );
			return SelectSchedule();
		}

		SetState( NPC_STATE_ALERT );
		return SelectSchedule();
	}

	if ( GetShotRegulator()->IsInRestInterval() )
	{
		if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
			return SCHED_COMBAT_FACE;
	}

	// we can see the enemy
	if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
	{
		if ( GetEnemy() )
		{
			m_vSavePosition = GetEnemy()->BodyTarget( GetAbsOrigin() );
			//if ( CanFireMortar( GetMortarFireSource( &GetAbsOrigin() ), m_vSavePosition, false ) )
			{
				return SCHED_RANGE_ATTACK1;
			}
		}		
	}

	if ( HasCondition(COND_CAN_RANGE_ATTACK2) )
		return SCHED_RANGE_ATTACK2;

	if ( HasCondition(COND_CAN_MELEE_ATTACK1) )
		return SCHED_MELEE_ATTACK1;

	if ( HasCondition(COND_CAN_MELEE_ATTACK2) )
		return SCHED_MELEE_ATTACK2;

	if ( HasCondition(COND_NOT_FACING_ATTACK) )
		return SCHED_COMBAT_FACE;

	return SCHED_ESTABLISH_LINE_OF_MORTAR_FIRE;

	// if we're not attacking, then back away
	//return SCHED_RUN_FROM_ENEMY;
}

int CASW_Mortarbug::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_RANGE_ATTACK1 )
	{
		RemoveAllGestures();
		return SCHED_ASW_MORTARBUG_SPIT;
	}

	if ( scheduleType == SCHED_COMBAT_FACE && IsUnreachable( GetEnemy() ) )
		return SCHED_TAKE_COVER_FROM_ENEMY;

	return BaseClass::TranslateSchedule( scheduleType );
}

CBaseEntity* CASW_Mortarbug::SpawnProjectile()
{
	if ( !GetEnemy() )
	{
		return NULL;
	}
	
	// TODO: Replace with launch attachment point
	Vector vSpitPos = GetAbsOrigin() + Vector( 0, 0, 10 );
	GetAttachment( "attach_spit", vSpitPos );

	Vector	vTarget;

	// If our enemy is looking at us and far enough away, lead him
	if ( HasCondition( COND_ENEMY_FACING_ME ) && UTIL_DistApprox( GetAbsOrigin(), GetEnemy()->GetAbsOrigin() ) > (40*12) )
	{
		UTIL_PredictedPosition( GetEnemy(), 0.5f, &vTarget ); 
		vTarget.z = GetEnemy()->GetAbsOrigin().z;
	}
	else
	{
		// Otherwise he can't see us and he won't be able to dodge
		vTarget = GetEnemy()->BodyTarget( vSpitPos, true );
	}

	vTarget[2] += random->RandomFloat( 0.0f, 32.0f );

	// Try and spit at our target
	Vector	vecToss;				
	if ( GetSpitVector( vSpitPos, vTarget, &vecToss ) == false )
	{
		// Now try where they were
		if ( GetSpitVector( vSpitPos, m_vSavePosition, &vecToss ) == false )
		{
			// Failing that, just shoot with the old velocity we calculated initially!
			vecToss = m_vecSaveSpitVelocity;
		}
	}

	// Find what our vertical theta is to estimate the time we'll impact the ground
	Vector vecToTarget = ( vTarget - vSpitPos );
	VectorNormalize( vecToTarget );
	float flVelocity = VectorNormalize( vecToss );
	float flCosTheta = DotProduct( vecToTarget, vecToss );
	float flTime = (vSpitPos-vTarget).Length2D() / ( flVelocity * flCosTheta );

	// Emit a sound where this is going to hit so that targets get a chance to act correctly
	CSoundEnt::InsertSound( SOUND_DANGER, vTarget, (15*12), flTime, this );

	// Don't fire again until this volley would have hit the ground (with some lag behind it)
	SetNextAttack( gpGlobals->curtime + flTime + random->RandomFloat( 0.5f, 2.0f ) );

	CASW_Mortarbug_Shell *pShell = NULL;
	for ( int i = 0; i < 3; i++ )
	{
		pShell = CASW_Mortarbug_Shell::CreateShell( vSpitPos, vecToss, this );
		pShell->m_bDoScreenShake = ( i == 1 );
		pShell->SetAbsVelocity( vecToss * ( flVelocity + 40.0f * (i-1) ) );
		//pShell->SetAbsVelocity( ( vecToss + RandomVector( -0.035f, 0.035f ) ) * flVelocity );
	}

	// only do effects if we havent done them in the last second
	if ( gpGlobals->curtime > m_fLastFireTime + 1.0f )
	{
		DispatchParticleEffect( "mortar_launch", PATTACH_POINT_FOLLOW, this, "attach_spit" );
		EmitSound( "ASW_MortarBug.Spit" );		// TODO: Replace with launching sound
	}
	
	m_fLastFireTime = gpGlobals->curtime;

	return pShell;
}

void CASW_Mortarbug::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine)
	{
		// don't hurt him if he was hurt recently
		if (m_fLastTouchHurtTime + 0.6f > gpGlobals->curtime)
		{
			return;
		}
		// hurt the marine
		Vector vecForceDir = (pMarine->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info( this, this, asw_mortarbug_touch_damage.GetInt(), DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, pMarine->GetAbsOrigin() );
		pMarine->TakeDamage( info );
		m_fLastTouchHurtTime = gpGlobals->curtime;
	}
}

bool CASW_Mortarbug::ShouldGib( const CTakeDamageInfo &info )
{
	return false;
}

int CASW_Mortarbug::SelectDeadSchedule()
{
	// Adrian - Alread dead (by animation event maybe?)
	// Is it safe to set it to SCHED_NONE?
	if ( m_lifeState == LIFE_DEAD )
		 return SCHED_NONE;

	CleanupOnDeath();
	return SCHED_DIE;
}

int CASW_Mortarbug::SelectFlinchSchedule_ASW()
{
	// only flinch in easy mode
	if (ASWGameRules() && ASWGameRules()->GetSkillLevel() != 1)
		return SCHED_NONE;

	if ( !HasCondition(COND_HEAVY_DAMAGE) && !HasCondition(COND_LIGHT_DAMAGE) )
		return SCHED_NONE;

	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
		return SCHED_NONE;

	// only flinch if shot during a spit
	if (! (GetTask() && (GetTask()->iTask == TASK_MORTARBUG_SPIT)) )
		return SCHED_NONE;
		
	// Any damage. Break out of my current schedule and flinch.
	Activity iFlinchActivity = GetFlinchActivity( true, false );
	if ( HaveSequenceForActivity( iFlinchActivity ) )
		return SCHED_BIG_FLINCH;

	return SCHED_NONE;
}

void CASW_Mortarbug::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	if ( IsCurSchedule( SCHED_RUN_FROM_ENEMY ) )
	{
		SetCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
	}
}

void CASW_Mortarbug::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_MORTARBUG_SPIT:
		{
			ResetIdealActivity((Activity) ACT_MORTARBUG_SPIT);
			break;
		}
	case TASK_GET_PATH_TO_MORTAR_ENEMY:
		{
			if ( !GetEnemy() )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			float flMaxRange = 2000;
			float flMinRange = 100;

			Vector vecEnemy = GetEnemy()->BodyTarget( GetAbsOrigin() );
			

			int iNode = FindMortarNode( vecEnemy, flMinRange, flMaxRange, 1.0f );
			if ( iNode != NO_NODE )
			{
				// move to the spot with line of sight
				m_vInterruptSavePosition = g_pBigAINet->GetNode(iNode)->GetPosition(GetHullType());
			}
			else
			{
				TaskFail( FAIL_NO_SHOOT );
			}
			break;
		}
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

void CASW_Mortarbug::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_MORTARBUG_SPIT:
		{
			if (IsActivityFinished())
			{
				TaskComplete();
			}
			break;
		}
	case TASK_GET_PATH_TO_MORTAR_ENEMY:
		{
			if ( GetEnemy() == NULL )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
			if ( GetTaskInterrupt() > 0 )
			{
				ClearTaskInterrupt();

				Vector vecEnemy = GetEnemy()->GetAbsOrigin();
				AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );

				GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET );
				GetNavigator()->SetArrivalDirection( vecEnemy - goal.dest );
			}
			else
				TaskInterrupt();
			break;
		}
	default:
		{
			BaseClass::RunTask(pTask);
			break;
		}
	}
}

// only shock damage counts as heavy (and thus causes a flinch even during normal running)
bool CASW_Mortarbug::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// shock damage never causes flinching
	if (( info.GetDamageType() & DMG_SHOCK ) != 0 )
		return false;

	// explosions always cause a flinch
	if (( info.GetDamageType() & DMG_BLAST ) != 0 )
		return true;
	
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());	
	if (pMarine && pMarine->GetActiveASWWeapon())
	{		
		return pMarine->GetActiveASWWeapon()->ShouldAlienFlinch(this, info);
	}

	return false;
}

void CASW_Mortarbug::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed(info);

	m_fGibTime = gpGlobals->curtime + random->RandomFloat(20.0f, 30.0f);
}

int CASW_Mortarbug::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( info.GetInflictor() && info.GetInflictor()->Classify() == CLASS_ASW_MORTAR_SHELL )
		return 0;

	CTakeDamageInfo newInfo(info);

	float damage = info.GetDamage();
	// reduce damage from shotguns and mining laser
	if (info.GetDamageType() & DMG_ENERGYBEAM)
	{
		damage *= 0.4f;
	}
	if (info.GetDamageType() & DMG_BUCKSHOT)
	{
		// hack to reduce vindicator damage (not reducing normal shotty as much as it's not too strong)
		if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
		{
			CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
			if (pMarine)
			{
				CASW_Weapon_Assault_Shotgun *pVindicator = dynamic_cast<CASW_Weapon_Assault_Shotgun*>(pMarine->GetActiveASWWeapon());
				if (pVindicator)
					damage *= 0.45f;
				else
					damage *= 0.6f;
			}
		}		
	}

	newInfo.SetDamage(damage);

	return BaseClass::OnTakeDamage_Alive(newInfo);
}

void CASW_Mortarbug::NPCThink()
{
	BaseClass::NPCThink();

	if (m_fGibTime != 0 && gpGlobals->curtime > m_fGibTime)
	{
		CEffectData	data;

		data.m_vOrigin = WorldSpaceCenter();
		data.m_vNormal = Vector(0,0,1);
		
		data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
		data.m_flScale = clamp( data.m_flScale, 1, 3 );
		data.m_nColor = 1;
		data.m_fFlags = IsOnFire() ? ASW_GIBFLAG_ON_FIRE : 0;

		DispatchEffect( "HarvesterGib", data );
		UTIL_Remove( this );
		SetThink( NULL );
	}
}


bool CASW_Mortarbug::ShouldPlayIdleSound( void )
{
	return false;	// asw temp

	//Only do idles in the right states
	if ( ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT ) )
		return false;

	//Gagged monsters don't talk
	if ( m_spawnflags & SF_NPC_GAG )
		return false;

	//Don't cut off another sound or play again too soon
	if ( m_flIdleDelay > gpGlobals->curtime )
		return false;

	//Randomize it a bit
	if ( random->RandomInt( 0, 20 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the enemy has been seen within the time period supplied
// Input  : flTime - Timespan we consider
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Mortarbug::SeenEnemyWithinTime( float flTime )
{
	float flLastSeenTime = GetEnemies()->LastTimeSeen( GetEnemy() );
	return ( flLastSeenTime != 0.0f && ( gpGlobals->curtime - flLastSeenTime ) < flTime );
}

//-----------------------------------------------------------------------------
// Purpose: Test whether this mortarbug can hit the target
//-----------------------------------------------------------------------------
bool CASW_Mortarbug::InnateWeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	if ( GetNextAttack() > gpGlobals->curtime )
		return false;

	// If we can see the enemy, or we've seen them in the last few seconds just try to lob in there
	if ( SeenEnemyWithinTime( 3.0f ) )
	{
		Vector vSpitPos = GetAbsOrigin() + Vector(0, 0, 10);		// TODO: replace with attachment point to fire from

		return GetSpitVector( vSpitPos, targetPos, &m_vecSaveSpitVelocity );
	}

	return BaseClass::InnateWeaponLOSCondition( ownerPos, targetPos, bSetConditions );
}

//
//	FIXME: Create this in a better fashion!
//

Vector VecCheckThrowTolerance( CBaseEntity *pEdict, const Vector &vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance )
{
	flSpeed = MAX( 1.0f, flSpeed );

	float flGravity = sv_gravity.GetFloat() * asw_mortarbug_shell_gravity.GetFloat();

	Vector vecGrenadeVel = (vecSpot2 - vecSpot1);

	// throw at a constant time
	float time = vecGrenadeVel.Length( ) / flSpeed;
	vecGrenadeVel = vecGrenadeVel * (1.0 / time);

	// adjust upward toss to compensate for gravity loss
	vecGrenadeVel.z += flGravity * time * 0.5;

	Vector vecApex = vecSpot1 + (vecSpot2 - vecSpot1) * 0.5;
	vecApex.z += 0.5 * flGravity * (time * 0.5) * (time * 0.5);


	trace_t tr;
	UTIL_TraceLine( vecSpot1, vecApex, MASK_SOLID, pEdict, COLLISION_GROUP_NONE, &tr );
	if (tr.fraction != 1.0)
	{
		// fail!
		if ( asw_debug_mortarbug.GetBool() )
		{
			NDebugOverlay::Line( vecSpot1, vecApex, 255, 0, 0, true, 5.0 );
		}

		return vec3_origin;
	}

	if ( asw_debug_mortarbug.GetBool() )
	{
		NDebugOverlay::Line( vecSpot1, vecApex, 0, 255, 0, true, 5.0 );
	}

	UTIL_TraceLine( vecApex, vecSpot2, MASK_SOLID_BRUSHONLY, pEdict, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		bool bFail = true;

		// Didn't make it all the way there, but check if we're within our tolerance range
		if ( flTolerance > 0.0f )
		{
			float flNearness = ( tr.endpos - vecSpot2 ).LengthSqr();
			if ( flNearness < Square( flTolerance ) )
			{
				if ( asw_debug_mortarbug.GetBool() )
				{
					NDebugOverlay::Sphere( tr.endpos, vec3_angle, flTolerance, 0, 255, 0, 0, true, 5.0 );
				}

				bFail = false;
			}
		}

		if ( bFail )
		{
			if ( asw_debug_mortarbug.GetBool() )
			{
				NDebugOverlay::Line( vecApex, vecSpot2, 255, 0, 0, true, 5.0 );
				NDebugOverlay::Sphere( tr.endpos, vec3_angle, flTolerance, 255, 0, 0, 0, true, 5.0 );
			}
			return vec3_origin;
		}
	}

	if ( asw_debug_mortarbug.GetBool() )
	{
		NDebugOverlay::Line( vecApex, vecSpot2, 0, 255, 0, true, 5.0 );
	}

	return vecGrenadeVel;
}

//-----------------------------------------------------------------------------
// Purpose: Get a toss direction that will properly lob spit to hit a target
// Input  : &vecStartPos - Where the spit will start from
//			&vecTarget - Where the spit is meant to land
//			*vecOut - The resulting vector to lob the spit
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Mortarbug::GetSpitVector( const Vector &vecStartPos, const Vector &vecTarget, Vector *vecOut )
{
	// Try the most direct route
	Vector vecToss = VecCheckThrowTolerance( this, vecStartPos, vecTarget, asw_mortarbug_spitspeed.GetFloat(), (10.0f*12.0f) );

	// If this failed then try a little faster (flattens the arc)
	if ( vecToss == vec3_origin )
	{
		vecToss = VecCheckThrowTolerance( this, vecStartPos, vecTarget, asw_mortarbug_spitspeed.GetFloat() * 1.5f, (10.0f*12.0f) );
		if ( vecToss == vec3_origin )
			return false;
	}

	// Save out the result
	if ( vecOut )
	{
		*vecOut = vecToss;
	}

	return true;
}

bool CASW_Mortarbug::CanFireMortar( const Vector &vecSrc, const Vector &vecDest, bool bDrawArc )
{
	float flGravity = asw_mortarbug_shell_gravity.GetFloat();

	Vector vecVelocity = UTIL_LaunchVector( vecSrc, vecDest, flGravity ) * 28.0f;

	Vector vecResult = UTIL_Check_Throw( vecSrc, vecVelocity, flGravity,
		-Vector( 12,12,12 ), Vector( 12,12,12 ), MASK_NPCSOLID, COLLISION_GROUP_PROJECTILE, this, bDrawArc );

	float flDist = vecResult.DistTo( vecDest );
	return ( flDist < 50.0f );
}

Vector CASW_Mortarbug::GetMortarFireSource( const Vector *vecStandingPos )
{
	Vector vecOrigin = GetAbsOrigin();
	Vector vSpitPos = GetAbsOrigin() + Vector( 0, 0, 10 );
	GetAttachment( "attach_spit", vSpitPos );

	
	return *vecStandingPos + vSpitPos - vecOrigin;
}

int CASW_Mortarbug::FindMortarNode( const Vector &vThreatPos, float flMinThreatDist, float flMaxThreatDist, float flBlockTime )
{
	if ( !CAI_NetworkManager::NetworksLoaded() )
		return NO_NODE;

	AI_PROFILE_SCOPE( CASW_Mortarbug::FindMortarNode );

	Remember( bits_MEMORY_TASK_EXPENSIVE );

	int iMyNode	= GetPathfinder()->NearestNodeToNPC();
	if ( iMyNode == NO_NODE )
	{
		Vector pos = GetAbsOrigin();
		DevWarning( 2, "FindMortarNode() - %s has no nearest node! (Check near %f %f %f)\n", GetClassname(), pos.x, pos.y, pos.z);
		return NO_NODE;
	}

	// ------------------------------------------------------------------------------------
	// We're going to search for a shoot node by expanding to our current node's neighbors
	// and then their neighbors, until a shooting position is found, or all nodes are beyond MaxDist
	// ------------------------------------------------------------------------------------
	AI_NearNode_t *pBuffer = (AI_NearNode_t *)stackalloc( sizeof(AI_NearNode_t) * g_pBigAINet->NumNodes() );
	CNodeList list( pBuffer, g_pBigAINet->NumNodes() );
	CVarBitVec wasVisited(g_pBigAINet->NumNodes());	// Nodes visited

	// mark start as visited
	wasVisited.Set( iMyNode );
	list.Insert( AI_NearNode_t(iMyNode, 0) );

	static int nSearchRandomizer = 0;		// tries to ensure the links are searched in a different order each time;

	while ( list.Count() )
	{
		int nodeIndex = list.ElementAtHead().nodeIndex;
		// remove this item from the list
		list.RemoveAtHead();

		const Vector &nodeOrigin = g_pBigAINet->GetNode(nodeIndex)->GetPosition(GetHullType());

		// HACKHACK: Can't we rework this loop and get rid of this?
		// skip the starting node, or we probably wouldn't have called this function.
		if ( nodeIndex != iMyNode )
		{
			bool skip = false;

			// Don't accept climb nodes, and assume my nearest node isn't valid because
			// we decided to make this check in the first place.  Keep moving
			if ( !skip && !g_pBigAINet->GetNode(nodeIndex)->IsLocked() &&
				g_pBigAINet->GetNode(nodeIndex)->GetType() != NODE_CLIMB )
			{
				// Now check its distance and only accept if in range
				float flThreatDist = ( nodeOrigin - vThreatPos ).Length();

				if ( flThreatDist < flMaxThreatDist &&
					flThreatDist > flMinThreatDist )
				{
					//CAI_Node *pNode = g_pBigAINet->GetNode(nodeIndex);
					if ( CanFireMortar( GetMortarFireSource( &nodeOrigin ), vThreatPos, asw_debug_mortarbug.GetInt() == 2 ) )
					{
						// Note when this node was used, so we don't try 
						// to use it again right away.
						g_pBigAINet->GetNode(nodeIndex)->Lock( flBlockTime );

						if ( asw_debug_mortarbug.GetBool() )
						{
							NDebugOverlay::Text( nodeOrigin, CFmtStr( "%d:los", nodeIndex), false, 1 );

							// draw the arc
							CanFireMortar( GetMortarFireSource( &nodeOrigin ), vThreatPos, true );
						}

						// The next NPC who searches should use a slight different pattern
						nSearchRandomizer = nodeIndex;
						return nodeIndex;
					}
					else
					{
						if ( asw_debug_mortarbug.GetBool() )
						{
							NDebugOverlay::Text( nodeOrigin, CFmtStr( "%d:!throw", nodeIndex), false, 1 );
						}
					}
				}
				else
				{
					if ( asw_debug_mortarbug.GetBool() )
					{
						CFmtStr msg( "%d:%s", nodeIndex, ( flThreatDist < flMaxThreatDist ) ? "too close" : "too far" );
						NDebugOverlay::Text( nodeOrigin, msg, false, 1 );
					}
				}
			}
		}

		// Go through each link and add connected nodes to the list
		for (int link=0; link < g_pBigAINet->GetNode(nodeIndex)->NumLinks();link++) 
		{
			int index = (link + nSearchRandomizer) % g_pBigAINet->GetNode(nodeIndex)->NumLinks();
			CAI_Link *nodeLink = g_pBigAINet->GetNode(nodeIndex)->GetLinkByIndex(index);

			if ( !GetPathfinder()->IsLinkUsable( nodeLink, iMyNode ) )
				continue;

			int newID = nodeLink->DestNodeID(nodeIndex);

			// If not already visited, add to the list
			if (!wasVisited.IsBitSet(newID))
			{
				float dist = (GetLocalOrigin() - g_pBigAINet->GetNode(newID)->GetPosition(GetHullType())).LengthSqr();
				list.Insert( AI_NearNode_t(newID, dist) );
				wasVisited.Set( newID );
			}
		}
	}
	// We failed.  No range attack node node was found
	return NO_NODE;
}

AI_BEGIN_CUSTOM_NPC( asw_mortarbug, CASW_Mortarbug )

	// Tasks
	DECLARE_TASK( TASK_MORTARBUG_SPIT )
	DECLARE_TASK( TASK_GET_PATH_TO_MORTAR_ENEMY )
	// Activities
	DECLARE_ACTIVITY( ACT_MORTARBUG_SPIT )
	// Events
	DECLARE_ANIMEVENT( AE_MORTARBUG_CHARGE )
	DECLARE_ANIMEVENT( AE_MORTARBUG_LAUNCH )

	DEFINE_SCHEDULE
	(
		SCHED_ASW_MORTARBUG_SPIT,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MORTARBUG_SPIT		0"
		""
		"	Interrupts"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_ESTABLISH_LINE_OF_MORTAR_FIRE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ESTABLISH_LINE_OF_FIRE"
		"		TASK_GET_PATH_TO_MORTAR_ENEMY		0"
		"		TASK_SPEAK_SENTENCE				1"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
		""
		"	Interrupts "
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LOST_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
	)


AI_END_CUSTOM_NPC()
