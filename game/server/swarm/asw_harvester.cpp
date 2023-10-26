#include "cbase.h"
#include "asw_harvester.h"
#include "te_effect_dispatch.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "asw_parasite.h"
#include "soundenvelope.h"
//#include "npc_antlion.h"
#include "ai_memory.h"
#include "asw_gamerules.h"
#include "asw_weapon.h"
#include "asw_shareddefs.h"
#include "asw_weapon_assault_shotgun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_HARVESTER_MAX_ATTACK_DISTANCE 1500

LINK_ENTITY_TO_CLASS( asw_harvester, CASW_Harvester );

float CASW_Harvester::s_fNextSpawnSoundTime = 0;
float CASW_Harvester::s_fNextPainSoundTime = 0;

BEGIN_DATADESC( CASW_Harvester )
	DEFINE_FIELD( m_fLastLayTime, FIELD_TIME ),	
	DEFINE_FIELD( m_iCrittersAlive, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastTouchHurtTime, FIELD_TIME ),
	DEFINE_FIELD( m_fGibTime, FIELD_TIME ),
	DEFINE_FIELD( m_flIdleDelay,			FIELD_TIME ),
END_DATADESC()

ConVar asw_harvester_speedboost( "asw_harvester_speedboost", "1.0",FCVAR_CHEAT , "boost speed for the harvesters" );
ConVar asw_harvester_max_critters( "asw_harvester_max_critters", "5",FCVAR_CHEAT , "maximum critters the harvester can spawn" );
ConVar asw_harvester_touch_damage( "asw_harvester_touch_damage", "5",FCVAR_CHEAT , "Damage caused by harvesters on touch" );
ConVar asw_harverter_suppress_children( "asw_harverter_suppress_children", "0", FCVAR_CHEAT, "If set to 1, harvesters won't spawn harvesites");
ConVar asw_harvester_new( "asw_harvester_new", "1", FCVAR_CHEAT, "If set to 1, use the new model");
ConVar asw_harvester_spawn_height( "asw_harvester_spawn_height", "16", FCVAR_CHEAT, "Height above harvester origin to spawn harvesites at" );
ConVar asw_harvester_spawn_interval( "asw_harvester_spawn_interval", "1.0", FCVAR_CHEAT, "Time between spawning a harvesite and starting to spawn another" );

// Anim Events
int AE_HARVESTER_SPAWN_CRITTER;
int AE_HARVESTER_SPAWN_SOUND;

// Activities
int ACT_HARVESTER_LAY_CRITTER;

CASW_Harvester::CASW_Harvester()
{
	m_iCrittersAlive = 0;
	m_fLastLayTime = 0;
	m_fLastTouchHurtTime = 0;
	if ( asw_harvester_new.GetBool() )
		m_pszAlienModelName = SWARM_NEW_HARVESTER_MODEL;
	else
		m_pszAlienModelName = SWARM_HARVESTER_MODEL;
	m_nAlienCollisionGroup = ASW_COLLISION_GROUP_ALIEN;
}

CASW_Harvester::~CASW_Harvester()
{
}

void CASW_Harvester::Spawn( void )
{
	SetHullType(HULL_WIDE_SHORT);

	BaseClass::Spawn();
	
	SetHullType(HULL_WIDE_SHORT);
	UTIL_SetSize(this, Vector(-23,-23,0), Vector(23,23,69));
				
	m_iHealth = ASWGameRules()->ModifyAlienHealthBySkillLevel(200);

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 );

	m_takedamage = DAMAGE_NO;	// alien is invulnerable until she finds her first enemy
	m_bNeverRagdoll = true;
}

void CASW_Harvester::Precache( void )
{
	PrecacheScriptSound( "ASW_Harvester.Death" );
	PrecacheScriptSound( "ASW_Harvester.Pain" );
	PrecacheScriptSound( "ASW_Harvester.Scared" );
	PrecacheScriptSound( "ASW_Harvester.SpawnCritter" );
	PrecacheScriptSound( "ASW_Harvester.Alert" );
	PrecacheScriptSound( "ASW_Harvester.Sniffing" );

	UTIL_PrecacheOther( "asw_parasite_defanged" );

	BaseClass::Precache();
}

float CASW_Harvester::GetIdealSpeed() const
{
	return asw_harvester_speedboost.GetFloat() * BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}


float CASW_Harvester::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

float CASW_Harvester::MaxYawSpeed( void )
{
	if ( m_bElectroStunned.Get() )
		return 0.1f;

	return 32.0f;

	switch ( GetActivity() )
	{
	case ACT_IDLE:		
		return 64.0f;
		break;
	
	case ACT_WALK:
		return 64.0f;
		break;
	
	default:
	case ACT_RUN:
		return 64.0f;
		break;
	}

	return 64.0f;
}

void CASW_Harvester::AlertSound()
{
	EmitSound("ASW_Harvester.Alert");
}

void CASW_Harvester::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound && gpGlobals->curtime > s_fNextPainSoundTime)
	{		
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
		s_fNextPainSoundTime = gpGlobals->curtime + 1.0f;
		EmitSound("ASW_Harvester.Pain");
	}
}

void CASW_Harvester::AttackSound()
{
	if (gpGlobals->curtime > s_fNextSpawnSoundTime)
	{
		EmitSound("ASW_Harvester.SpawnCritter");
		s_fNextSpawnSoundTime = gpGlobals->curtime + 2.0f;
	}
}

void CASW_Harvester::IdleSound()
{
	EmitSound("ASW_Harvester.Sniffing");
	m_flIdleDelay = gpGlobals->curtime + 4.0f;
}

void CASW_Harvester::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound("ASW_Harvester.Death");
}

// make the harvester look at his enemy
bool CASW_Harvester::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
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
	if ( bFaceTarget )
	{
		AddFacingTarget( pFaceTarget, vecFacePosition, 1.0, 0.2 );
	}

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

void CASW_Harvester::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_HARVESTER_SPAWN_CRITTER )
	{
		// The point in our laying animation where we should actually spawn the critter		
		SpawnAlien();
		float spawn_interval = asw_harvester_spawn_interval.GetFloat();
		m_flNextAttack = gpGlobals->curtime + spawn_interval;
		return;
	}
	if ( nEvent == AE_HARVESTER_SPAWN_SOUND )
	{
		AttackSound();
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

// harvester can attack without LOS so long as they're near enough
bool CASW_Harvester::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions)
{
	if (targetPos.DistTo(ownerPos) < ASW_HARVESTER_MAX_ATTACK_DISTANCE)
		return true;

	return false;
}

bool CASW_Harvester::FCanCheckAttacks()
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
float CASW_Harvester::InnateRange1MinRange( void )
{
	return 0;
}

float CASW_Harvester::InnateRange1MaxRange( void )
{
	return ASW_HARVESTER_MAX_ATTACK_DISTANCE;
}

// make sure the harvester backs away when he's near us (he should sit back and lay critters)
int CASW_Harvester::RangeAttack1Conditions( float flDot, float flDist )
{
	if (GetEnemy() == NULL)
	{
		return( COND_NONE );
	}

	if ( gpGlobals->curtime < m_flNextAttack )
	{
		return( COND_NONE );
	}

	if ( m_iCrittersAlive > asw_harvester_max_critters.GetInt() )
		return( COND_NONE );

	float flTooClose = InnateRange1MinRange();
	if ( flDist > ASW_HARVESTER_MAX_ATTACK_DISTANCE )
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

int CASW_Harvester::SelectSchedule()
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
		return SelectHarvesterCombatSchedule();

	return BaseClass::SelectSchedule();
}

int CASW_Harvester::SelectHarvesterCombatSchedule()
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
#ifdef ASW_FEARFUL_HARVESTERS
	// If I'm scared of this enemy run away
	if ( IRelationType( GetEnemy() ) == D_FR )
	{
		if (HasCondition( COND_SEE_ENEMY )	|| 
			HasCondition( COND_LIGHT_DAMAGE )|| 
			HasCondition( COND_HEAVY_DAMAGE ))
		{
			FearSound();
			//ClearCommandGoal();
			return SCHED_RUN_FROM_ENEMY;
		}

		// If I've seen the enemy recently, cower. Ignore the time for unforgettable enemies.
		AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy() );
		if ( (pMemory && pMemory->bUnforgettable) || (GetEnemyLastTimeSeen() > (gpGlobals->curtime - 5.0)) )
		{
			// If we're facing him, just look ready. Otherwise, face him.
			if ( FInAimCone( GetEnemy()->EyePosition() ) )
				return SCHED_COMBAT_STAND;

			return SCHED_FEAR_FACE;
		}
	}
#endif
	// Check if need to reload
	if ( HasCondition( COND_LOW_PRIMARY_AMMO ) || HasCondition( COND_NO_PRIMARY_AMMO ) )
	{
		return SCHED_HIDE_AND_RELOAD;
	}

#ifdef ASW_FEARFUL_HARVESTERS
	if ( HasCondition(COND_TOO_CLOSE_TO_ATTACK) ) 
		return SCHED_RUN_FROM_ENEMY; //SCHED_BACK_AWAY_FROM_ENEMY;
#endif

	if ( GetShotRegulator()->IsInRestInterval() )
	{
		if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
			return SCHED_COMBAT_FACE;
	}

	// we can see the enemy
	if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
	{
		return SCHED_RANGE_ATTACK1;
	}

	if ( HasCondition(COND_CAN_RANGE_ATTACK2) )
		return SCHED_RANGE_ATTACK2;

	if ( HasCondition(COND_CAN_MELEE_ATTACK1) )
		return SCHED_MELEE_ATTACK1;

	if ( HasCondition(COND_CAN_MELEE_ATTACK2) )
		return SCHED_MELEE_ATTACK2;

	if ( HasCondition(COND_NOT_FACING_ATTACK) )
		return SCHED_COMBAT_FACE;

	// if we're not attacking, then back away
	return SCHED_RUN_FROM_ENEMY;
}

int CASW_Harvester::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_RANGE_ATTACK1 )
	{
		RemoveAllGestures();
		return SCHED_ASW_HARVESTER_LAY_CRITTER;
	}

	if ( scheduleType == SCHED_COMBAT_FACE && IsUnreachable( GetEnemy() ) )
		return SCHED_TAKE_COVER_FROM_ENEMY;

	return BaseClass::TranslateSchedule( scheduleType );
}

CAI_BaseNPC* CASW_Harvester::SpawnAlien()
{
	if (asw_harverter_suppress_children.GetBool())
		return NULL;

	CBaseEntity	*pEntity = CreateEntityByName( "asw_parasite_defanged" );
	CAI_BaseNPC	*pNPC = dynamic_cast<CAI_BaseNPC*>(pEntity);

	if ( !pNPC )
	{
		Warning("NULL Ent in CASW_Harvester!\n");
		return NULL;
	}

	pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
	
	pNPC->SetAbsOrigin( GetAbsOrigin() + Vector( 0, 0, asw_harvester_spawn_height.GetFloat() ) );

	// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
	QAngle angles = GetAbsAngles();
	angles.x = 0.0;
	angles.z = 0.0;
	pNPC->SetAbsAngles( angles );

	IASW_Spawnable_NPC* pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pNPC);
	ASSERT(pSpawnable);	
	if ( !pSpawnable )
	{
		Warning("NULL Spawnable Ent in CASW_Harvester!\n");
		UTIL_Remove(pNPC);
		return NULL;
	}

	DispatchSpawn( pNPC );
	pNPC->SetOwnerEntity( this );
	pNPC->Activate();

	CASW_Parasite *pParasite = dynamic_cast<CASW_Parasite*>(pNPC);
	if (pParasite)
	{
		m_iCrittersAlive++;
		pParasite->SetMother(this);
	}

	return pNPC;
}

void CASW_Harvester::ChildAlienKilled(CASW_Alien* pParasite)
{
	m_iCrittersAlive--;
}

void CASW_Harvester::StartTouch( CBaseEntity *pOther )
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
		CTakeDamageInfo info( this, this, asw_harvester_touch_damage.GetInt(), DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, pMarine->GetAbsOrigin() );
		pMarine->TakeDamage( info );
		m_fLastTouchHurtTime = gpGlobals->curtime;
	}
}

bool CASW_Harvester::ShouldGib( const CTakeDamageInfo &info )
{
	return false;
}

int CASW_Harvester::SelectDeadSchedule()
{
	// Adrian - Alread dead (by animation event maybe?)
	// Is it safe to set it to SCHED_NONE?
	if ( m_lifeState == LIFE_DEAD )
		 return SCHED_NONE;

	CleanupOnDeath();
	return SCHED_DIE;
}

int CASW_Harvester::SelectFlinchSchedule_ASW()
{
	// only flinch in easy mode
	if (ASWGameRules() && ASWGameRules()->GetSkillLevel() != 1)
		return SCHED_NONE;

	if ( !HasCondition(COND_HEAVY_DAMAGE) && !HasCondition(COND_LIGHT_DAMAGE) )
		return SCHED_NONE;

	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
		return SCHED_NONE;

	// only flinch if shot during a laying a critter
	if (! (GetTask() && (GetTask()->iTask == TASK_LAY_CRITTER)) )
		return SCHED_NONE;
		
	// Any damage. Break out of my current schedule and flinch.
	Activity iFlinchActivity = GetFlinchActivity( true, false );
	if ( HaveSequenceForActivity( iFlinchActivity ) )
		return SCHED_BIG_FLINCH;

	return SCHED_NONE;
}

void CASW_Harvester::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	if ( IsCurSchedule( SCHED_RUN_FROM_ENEMY ) )
	{
		SetCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
	}
}

void CASW_Harvester::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_LAY_CRITTER:
		{
			ResetIdealActivity((Activity) ACT_HARVESTER_LAY_CRITTER);
			break;
		}
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

void CASW_Harvester::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_LAY_CRITTER:
		{
			if (IsActivityFinished())
			{
				TaskComplete();
			}
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
bool CASW_Harvester::IsHeavyDamage( const CTakeDamageInfo &info )
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

void CASW_Harvester::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed(info);

	// spawn a bunch of harvesites
	int iNumParasites = 4 + RandomInt(0,2);
	QAngle angParasiteFacing[6];
	float fJumpDistance[6];
	// for some reason if we calculate these inside the loop, the random numbers all come out the same.  Worrying.
	angParasiteFacing[0] = GetAbsAngles(); angParasiteFacing[0].y = RandomFloat( -180.0f, 180.0f );
	angParasiteFacing[1] = GetAbsAngles(); angParasiteFacing[1].y = RandomFloat( -180.0f, 180.0f );
	angParasiteFacing[2] = GetAbsAngles(); angParasiteFacing[2].y = RandomFloat( -180.0f, 180.0f );
	angParasiteFacing[3] = GetAbsAngles(); angParasiteFacing[3].y = RandomFloat( -180.0f, 180.0f );
	angParasiteFacing[4] = GetAbsAngles(); angParasiteFacing[4].y = RandomFloat( -180.0f, 180.0f );
	angParasiteFacing[5] = GetAbsAngles(); angParasiteFacing[5].y = RandomFloat( -180.0f, 180.0f );
	fJumpDistance[0] = RandomFloat( 30.0f, 70.0f );
	fJumpDistance[1] = RandomFloat( 30.0f, 70.0f );
	fJumpDistance[2] = RandomFloat( 30.0f, 70.0f );
	fJumpDistance[3] = RandomFloat( 30.0f, 70.0f );
	fJumpDistance[4] = RandomFloat( 30.0f, 70.0f );
	fJumpDistance[5] = RandomFloat( 30.0f, 70.0f );

	for ( int i = 0; i < iNumParasites; i++ )
	{
		bool bBlocked = true;			
		int k = 0;

		Vector vecSpawnPos = GetAbsOrigin();
		float fCircleDegree = ( static_cast< float >( i ) / iNumParasites ) * 2.0f * M_PI;
		vecSpawnPos.x += sinf( fCircleDegree ) * RandomFloat( 3.0f, 20.0f );
		vecSpawnPos.y += cosf( fCircleDegree ) * RandomFloat( 3.0f, 20.0f );
		vecSpawnPos.z += RandomFloat( 20.0f, 40.0f );

		while ( bBlocked && k < 6 )
		{
			if ( k > 0 )
			{
				// Scooch it up
				vecSpawnPos.z += NAI_Hull::Maxs( HULL_TINY ).z - NAI_Hull::Mins( HULL_TINY ).z;
			}

			// check if there's room at this position
			trace_t tr;
			UTIL_TraceHull( vecSpawnPos, vecSpawnPos + Vector( 0.0f, 0.0f, 1.0f ), 
				NAI_Hull::Mins(HULL_TINY) + Vector( -4.0f, -4.0f, -4.0f ),NAI_Hull::Maxs(HULL_TINY) + Vector( 4.0f, 4.0f, 4.0f ),
				MASK_NPCSOLID, this, ASW_COLLISION_GROUP_PARASITE, &tr );	
			
			//NDebugOverlay::Box(vecSpawnPos[i], NAI_Hull::Mins(HULL_TINY),NAI_Hull::Maxs(HULL_TINY), 255,255,0,255,15.0f);
			if ( tr.fraction == 1.0 )
			{
				bBlocked = false;
			}

			k++;				
		}

		if ( bBlocked )
			continue;	// couldn't find room for parasites

		CASW_Parasite *pParasite = dynamic_cast< CASW_Parasite* >( CreateNoSpawn("asw_parasite_defanged",
			vecSpawnPos, angParasiteFacing[i], this));

		if ( pParasite )
		{
			DispatchSpawn( pParasite );
			pParasite->SetSleepState(AISS_WAITING_FOR_INPUT);
			pParasite->SetJumpFromEgg(true, fJumpDistance[i]);
			pParasite->Wake();

			if ( IsOnFire() )
			{
				pParasite->ASW_Ignite( 30.0f, 0, info.GetAttacker(), info.GetWeapon() );
			}
		}
	}

	m_fGibTime = gpGlobals->curtime + random->RandomFloat(20.0f, 30.0f);
}

int CASW_Harvester::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
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

void CASW_Harvester::NPCThink()
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


bool CASW_Harvester::ShouldPlayIdleSound( void )
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

bool CASW_Harvester::CanBePushedAway()
{
	return ( ( gpGlobals->curtime - m_fLastLayTime ) > 2.0f ) && BaseClass::CanBePushedAway();
}

AI_BEGIN_CUSTOM_NPC( asw_harvester, CASW_Harvester )

	// Tasks
	DECLARE_TASK( TASK_LAY_CRITTER )
	// Activities
	DECLARE_ACTIVITY( ACT_HARVESTER_LAY_CRITTER )
	// Events
	DECLARE_ANIMEVENT( AE_HARVESTER_SPAWN_CRITTER )
	DECLARE_ANIMEVENT( AE_HARVESTER_SPAWN_SOUND )

	DEFINE_SCHEDULE
	(
		SCHED_ASW_HARVESTER_LAY_CRITTER,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_LAY_CRITTER		0"
		""
		"	Interrupts"
		//"		COND_HEAVY_DAMAGE"
	)
	
AI_END_CUSTOM_NPC()
