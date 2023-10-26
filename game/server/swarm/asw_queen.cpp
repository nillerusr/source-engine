#include "cbase.h"
#include "asw_queen.h"
#include "asw_queen_spit.h"
#include "te_effect_dispatch.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_parasite.h"
#include "asw_buzzer.h"
#include "asw_game_resource.h"
#include "asw_gamerules.h"
#include "soundenvelope.h"
#include "ai_memory.h"
#include "ai_moveprobe.h"
#include "asw_util_shared.h"
#include "asw_queen_divers_shared.h"
#include "asw_queen_grabber_shared.h"
#include "asw_colonist.h"
#include "ndebugoverlay.h"
#include "asw_weapon_assault_shotgun_shared.h"
#include "asw_sentry_base.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SWARM_QUEEN_MODEL	"models/swarm/Queen/Queen.mdl"
//#define	SWARM_QUEEN_MODEL		"models/antlion_guard.mdl"
#define ASW_QUEEN_MAX_ATTACK_DISTANCE 1500

// define this to make the queen not move/turn
//#define ASW_QUEEN_STATIONARY

LINK_ENTITY_TO_CLASS( asw_queen, CASW_Queen );

IMPLEMENT_SERVERCLASS_ST( CASW_Queen, DT_ASW_Queen )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),

	SendPropEHandle( SENDINFO ( m_hQueenEnemy ) ),	
	SendPropBool( SENDINFO(m_bChestOpen) ),
	SendPropInt( SENDINFO(m_iMaxHealth), 14, SPROP_UNSIGNED ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Queen )
	DEFINE_FIELD( m_angQueenFacing, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecLastClawPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_bChestOpen, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fLastHeadYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLastShieldPose, FIELD_FLOAT ),
	DEFINE_FIELD( m_iSpitNum, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDiverState, FIELD_INTEGER ),
	DEFINE_FIELD( m_fLastDiverAttack, FIELD_TIME ),
	DEFINE_FIELD( m_fNextDiverState, FIELD_TIME ),
	DEFINE_FIELD( m_hPreventMovementMarine, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hGrabbingEnemy, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPrimaryGrabber, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hRetreatSpot, FIELD_EHANDLE ),
	DEFINE_FIELD( m_fLastRangedAttack, FIELD_FLOAT ),
	DEFINE_FIELD( m_iCrittersAlive, FIELD_INTEGER ),
	DEFINE_FIELD( m_hBlockingSentry, FIELD_EHANDLE ),
	
	DEFINE_OUTPUT( m_OnSummonWave1,		"OnSummonWave1" ),
	DEFINE_OUTPUT( m_OnSummonWave2,		"OnSummonWave2" ),
	DEFINE_OUTPUT( m_OnSummonWave3,		"OnSummonWave3" ),
	DEFINE_OUTPUT( m_OnSummonWave4,		"OnSummonWave4" ),
	DEFINE_OUTPUT( m_OnQueenKilled,		"OnQueenKilled" ),
END_DATADESC()

// Activities
int ACT_QUEEN_SCREAM;
int ACT_QUEEN_SCREAM_LOW;
int ACT_QUEEN_SINGLE_SPIT;
int ACT_QUEEN_TRIPLE_SPIT;
int AE_QUEEN_SPIT;
int AE_QUEEN_START_SPIT;
int ACT_QUEEN_LOW_IDLE;
int ACT_QUEEN_LOW_TO_HIGH;
int ACT_QUEEN_HIGH_TO_LOW;
int ACT_QUEEN_TENTACLE_ATTACK;

// AnimEvents
int AE_QUEEN_SLASH_HIT;
int AE_QUEEN_R_SLASH_HIT;
int AE_QUEEN_START_SLASH;

ConVar asw_queen_health_easy("asw_queen_health_easy", "2500", FCVAR_CHEAT, "Initial health of the Swarm Queen");
ConVar asw_queen_health_normal("asw_queen_health_normal", "3500", FCVAR_CHEAT, "Initial health of the Swarm Queen");
ConVar asw_queen_health_hard("asw_queen_health_hard", "5000", FCVAR_CHEAT, "Initial health of the Swarm Queen");
ConVar asw_queen_health_insane("asw_queen_health_insane", "6000", FCVAR_CHEAT, "Initial health of the Swarm Queen");
ConVar asw_queen_slash_damage("asw_queen_slash_damage", "5", FCVAR_CHEAT, "Damage caused by the Swarm Queen's slash attack");
ConVar asw_queen_slash_size("asw_queen_slash_size", "100", FCVAR_CHEAT, "Padding around the Swarm Queen's claw when calculating melee attack collision");
ConVar asw_queen_slash_debug("asw_queen_slash_debug", "0", FCVAR_CHEAT, "Visualize Swarm Queen slash collision");
ConVar asw_queen_slash_range("asw_queen_slash_range", "200", FCVAR_CHEAT, "Range of Swarm Queen slash attack");
ConVar asw_queen_min_mslash("asw_queen_min_mslash", "160", FCVAR_CHEAT, "Min Range of Swarm Queen moving slash attack");
ConVar asw_queen_max_mslash("asw_queen_max_mslash", "350", FCVAR_CHEAT, "Max Range of Swarm Queen moving slash attack");
ConVar asw_queen_spit_autoaim_angle("asw_queen_spit_autoaim_angle", "10", FCVAR_CHEAT, "Angle in degrees in which the Queen's spit attack will adjust to fire at a marine");
ConVar asw_queen_debug("asw_queen_debug", "0", FCVAR_CHEAT, "Display debug info about the queen");
ConVar asw_queen_flame_flinch_chance("asw_queen_flame_flinch_chance", "0", FCVAR_CHEAT, "Chance of queen flinching when she takes fire damage");
ConVar asw_queen_force_parasite_spawn("asw_queen_force_parasite_spawn", "0", FCVAR_CHEAT, "Set to 1 to force the queen to spawn parasites");
ConVar asw_queen_force_spit("asw_queen_force_spit", "0", FCVAR_CHEAT, "Set to 1 to force the queen to spit");

#define ASW_QUEEN_CLAW_MINS Vector(-asw_queen_slash_size.GetFloat(), -asw_queen_slash_size.GetFloat(), -asw_queen_slash_size.GetFloat() * 2.0f)
#define ASW_QUEEN_CLAW_MAXS Vector(asw_queen_slash_size.GetFloat(), asw_queen_slash_size.GetFloat(), asw_queen_slash_size.GetFloat() * 2.0f)
#define ASW_QUEEN_SLASH_DAMAGE asw_queen_slash_damage.GetInt()
#define ASW_QUEEN_MELEE_RANGE asw_queen_slash_range.GetFloat()
#define ASW_QUEEN_MELEE2_MIN_RANGE asw_queen_min_mslash.GetFloat()
#define ASW_QUEEN_MELEE2_MAX_RANGE asw_queen_max_mslash.GetFloat()

// health points at which the queen will stop to call in waves of allies
#define QUEEN_SUMMON_WAVE_POINT_1 0.8f		// wave of drones
#define QUEEN_SUMMON_WAVE_POINT_2 0.6f		// wave of buzzers
#define QUEEN_SUMMON_WAVE_POINT_3 0.4f		// wave of drone jumpers
#define QUEEN_SUMMON_WAVE_POINT_4 0.2f		// wave of shieldbugs
#define ASW_DIVER_ATTACK_CHANCE 0.5f
#define ASW_DIVER_ATTACK_INTERVAL 20.0f
#define ASW_RANGED_ATTACK_INTERVAL 30.0f
#define ASW_MAX_QUEEN_PARASITES 5

CASW_Queen::CASW_Queen()
{
	m_iDiverState = ASW_QUEEN_DIVER_NONE;
	m_fLastDiverAttack = 0;
	m_iCrittersAlive = 0;
	m_fLayParasiteTime = 0;
	m_iCrittersSpawnedRecently = 0;
	m_pszAlienModelName = SWARM_QUEEN_MODEL;
	m_nAlienCollisionGroup = ASW_COLLISION_GROUP_ALIEN;
}

CASW_Queen::~CASW_Queen()
{
}

void CASW_Queen::Spawn( void )
{
	SetHullType(HULL_LARGE_CENTERED);

	BaseClass::Spawn();
	
	SetHullType(HULL_LARGE_CENTERED);
	//UTIL_SetSize(this, Vector(-23,-23,0), Vector(23,23,69));
	//UTIL_SetSize(this,	Vector(-140, -140, 0), Vector(140, 140, 200) );
#ifdef ASW_QUEEN_STATIONARY
	UTIL_SetSize(this,	Vector(-140, -40, 0), Vector(140, 40, 200) );
#else
	UTIL_SetSize(this, Vector(-120,-120,0), Vector(120,120,160));
#endif

	SetHealthByDifficultyLevel();	

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 //| bits_CAP_INNATE_RANGE_ATTACK2
		| bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2);

#ifdef ASW_QUEEN_STATIONARY
	CapabilitiesRemove( bits_CAP_MOVE_GROUND );
#endif

	m_flDistTooFar = 9999999.0f;		
	m_angQueenFacing = GetAbsAngles();
	m_hDiver = CASW_Queen_Divers::Create_Queen_Divers(this);	
	//CollisionProp()->SetSurroundingBoundsType( USE_HITBOXES );

	m_takedamage = DAMAGE_NO;	// queen is invulnerable until she finds her first enemy
		
	m_hRetreatSpot = gEntList.FindEntityByClassname( NULL, "asw_queen_retreat_spot" );
}

void CASW_Queen::NPCInit()
{
	BaseClass::NPCInit();

	//SetDistSwarmSense(1024.0f);
	//SetDistLook(1024.0f);		
}


void CASW_Queen::Precache( void )
{
	PrecacheScriptSound( "ASW_Queen.Death" );
	PrecacheScriptSound( "ASW_Queen.Pain" );
	PrecacheScriptSound( "ASW_Queen.PainBig" );
	PrecacheScriptSound( "ASW_Queen.Slash" );
	PrecacheScriptSound( "ASW_Queen.SlashShort" );
	PrecacheScriptSound( "ASW_Queen.AttackWave" );
	PrecacheScriptSound( "ASW_Queen.Spit" );
	PrecacheScriptSound( "ASW_Queen.TentacleAttackStart" );

	BaseClass::Precache();
}


// queen doesn't move, like Kompressor does not dance
float CASW_Queen::GetIdealSpeed() const
{
#ifdef ASW_QUEEN_STATIONARY
	return 0;
#else
	return BaseClass::GetIdealSpeed() * m_flPlaybackRate;
#endif
}

float CASW_Queen::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

// queen doesn't turn
float CASW_Queen::MaxYawSpeed( void )
{
#ifdef ASW_QUEEN_STATIONARY
	return 0;
#else	
	Activity eActivity = GetActivity();
	//CBaseEntity *pEnemy = GetEnemy();

	// Stay still
	if (( eActivity == ACT_MELEE_ATTACK1 ) )
		return 0.0f;
	
	return 20;
#endif
}


// ============================= SOUNDS =============================

void CASW_Queen::AlertSound()
{
	// no alert sound atm
	//EmitSound("ASW_ShieldBug.Alert");
}

void CASW_Queen::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound )
	{
		if (info.GetInflictor() == m_hDiver.Get())	// if the damage comes from our vulnerable divers, then scream big time
			EmitSound("ASW_Queen.PainBig");
		else
			EmitSound("ASW_Queen.Pain");
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
	//SetChestOpen(!m_bChestOpen);
}

void CASW_Queen::AttackSound()
{
	if (IsCurSchedule(SCHED_MELEE_ATTACK2))
		EmitSound("ASW_Queen.SlashShort");
	else
		EmitSound("ASW_Queen.Slash");
}

void CASW_Queen::SummonWaveSound()
{
	EmitSound("ASW_Queen.AttackWave");
}

void CASW_Queen::IdleSound()
{
	// queen has no idle...
	//EmitSound("ASW_ShieldBug.Idle");
}

void CASW_Queen::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "ASW_Queen.Death" );
}

// ============================= END SOUNDS =============================

// make the queen always look in his starting direction

bool CASW_Queen::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
#ifdef ASW_QUEEN_STATIONARY
	Vector vForward;
	AngleVectors( m_angQueenFacing, &vForward );
	
	AddFacingTarget( vForward, 1.0f, 0.2f );
#endif
	return BaseClass::OverrideMoveFacing( move, flInterval );
}

void CASW_Queen::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_QUEEN_SLASH_HIT )
	{
		SlashAttack(false);
		return;
	}
	else if ( nEvent == AE_QUEEN_R_SLASH_HIT )
	{
		SlashAttack(true);
		return;
	}
	else if ( nEvent == AE_QUEEN_START_SLASH )
	{
		AttackSound();
		m_vecLastClawPos = vec3_origin;
		return;
	}
	else if ( nEvent == AE_QUEEN_START_SPIT)
	{
		m_iSpitNum = 0;
		return;
	}
	else if ( nEvent == AE_QUEEN_SPIT)
	{
		SpitProjectile();
		m_iSpitNum++;
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

// queen can attack without LOS so long as they're near enough
bool CASW_Queen::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions)
{
	if (targetPos.DistTo(ownerPos) < ASW_QUEEN_MAX_ATTACK_DISTANCE)
		return true;

	return false;
}

// is the enemy near enough to left slash at?
int CASW_Queen::MeleeAttack1Conditions ( float flDot, float flDist )
{
	float fRangeBoost = 1.0f;
	if (flDot > 0)
	{	
		fRangeBoost = 1.0f + (1.0f - flDot) * 0.25f;	// 25% range boost at fldot of 0
	}

	if ( flDist > ASW_QUEEN_MELEE_RANGE * fRangeBoost)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}	
	/*else if (GetEnemy() == NULL)
	{
		return 0;
	}

	// check he's to our left
	Vector diff = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	float yaw = UTIL_VecToYaw(diff);
	yaw = AngleDiff(yaw, GetAbsAngles()[YAW]);
	if (yaw < 0)
		return 0;*/

	return COND_CAN_MELEE_ATTACK1;
}

// attack 2 is the moving attack, enemy has to be over x units away
int CASW_Queen::MeleeAttack2Conditions ( float flDot, float flDist )
{
	float fRangeBoost = 1.0f;
	if (flDot > 0)
	{	
		fRangeBoost = 1.0f + (1.0f - flDot) * 0.25f;	// 25% range boost at fldot of 0
	}
	if ( flDist > ASW_QUEEN_MELEE2_MAX_RANGE * fRangeBoost)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}	
	if ( flDist < ASW_QUEEN_MELEE2_MIN_RANGE)
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}	
	/*else if (GetEnemy() == NULL)
	{
		return 0;
	}

	// check he's to our right
	
	Vector diff = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	float yaw = UTIL_VecToYaw(diff);
	yaw = AngleDiff(yaw, GetAbsAngles()[YAW]);
	if (yaw > 0)
		return 0;
	*/

	return COND_CAN_MELEE_ATTACK2;
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CASW_Queen::InnateRange1MinRange( void )
{
	return ASW_QUEEN_MELEE_RANGE;
}

float CASW_Queen::InnateRange1MaxRange( void )
{
	return ASW_QUEEN_MAX_ATTACK_DISTANCE;
}

int CASW_Queen::RangeAttack1Conditions ( float flDot, float flDist )
{
	if ( flDist < ASW_QUEEN_MELEE_RANGE)
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if (flDist > ASW_QUEEN_MAX_ATTACK_DISTANCE)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5)
	{
		return COND_NOT_FACING_ATTACK;
	}

	// we also have a timer that can prevent us from attacking, make sure we don't try while we're still in that time
	if (gpGlobals->curtime <= m_fLastRangedAttack + ASW_RANGED_ATTACK_INTERVAL)
		return COND_TOO_FAR_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

bool CASW_Queen::FCanCheckAttacks()
{
	if ( GetNavType() == NAV_CLIMB || GetNavType() == NAV_JUMP )
		return false;

	//if ( HasCondition(COND_SEE_ENEMY) && !HasCondition( COND_ENEMY_TOO_FAR))
	//{
		return true;
	//}

	//return false;
}

void CASW_Queen::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_QUEEN_BLOCKED_BY_DOOR );
	if (m_hBlockingSentry.Get())
	{
		SetCondition( COND_QUEEN_BLOCKED_BY_DOOR );
	}
}

int CASW_Queen::SelectSchedule()
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
		return SelectQueenCombatSchedule();

	return BaseClass::SelectSchedule();
}

int CASW_Queen::SelectQueenCombatSchedule()
{
	if (asw_queen_force_spit.GetBool())
	{
		asw_queen_force_spit.SetValue(false);
		return SCHED_RANGE_ATTACK1;
	}
	if (asw_queen_force_parasite_spawn.GetBool())
	{
		asw_queen_force_parasite_spawn.SetValue(false);
		return SCHED_ASW_SPAWN_PARASITES;
	}

	// see if we were hurt, if so, flinch!
	int nSched = SelectFlinchSchedule_ASW();
	if ( nSched != SCHED_NONE )
		return nSched;

	// if we're in the middle of a diver attack, just wait
	if (m_iDiverState > ASW_QUEEN_DIVER_IDLE)
	{
		return SCHED_WAIT_DIVER;
	}

	// wake up angrily when we first see a marine
	if ( HasCondition(COND_NEW_ENEMY) && gpGlobals->curtime - GetEnemies()->FirstTimeSeen(GetEnemy()) < 2.0 )
	{
		return SCHED_WAKE_ANGRY;
	}
	
	// if our enemy died, clear him and try to find another
	if ( HasCondition( COND_ENEMY_DEAD ) )
	{
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

	// see if it's time to call in a wave of allies
	nSched = SelectSummonSchedule();
	if ( nSched != SCHED_NONE )
	{
		return nSched;
	}

	// occasionally do a diver attack
	if (m_fLastDiverAttack == 0)
		m_fLastDiverAttack = gpGlobals->curtime;	// forces delay before first diver attack
	if ((m_fLastDiverAttack == 0 || gpGlobals->curtime > m_fLastDiverAttack + ASW_DIVER_ATTACK_INTERVAL)
		)
			//&& random->RandomFloat() > ASW_DIVER_ATTACK_CHANCE)
		return SCHED_START_DIVER_ATTACK;

	// if we're blocked by a sentry, smash that mofo
	if ( HasCondition(COND_QUEEN_BLOCKED_BY_DOOR) )
		return SCHED_SMASH_SENTRY;

	// melee if we can
	if ( HasCondition(COND_CAN_MELEE_ATTACK1) )
		return SCHED_MELEE_ATTACK1;

	if ( HasCondition(COND_CAN_MELEE_ATTACK2) )
		return SCHED_MELEE_ATTACK2;

#ifdef ASW_QUEEN_STATIONARY
	// we can see the enemy
	if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
	{
		return SCHED_RANGE_ATTACK1;
	}	

	if ( HasCondition(COND_CAN_RANGE_ATTACK2) )
		return SCHED_RANGE_ATTACK2;
#else
	if (m_fLastRangedAttack == 0)
		m_fLastRangedAttack = gpGlobals->curtime;	// forces delay before first diver attack
	if (gpGlobals->curtime > m_fLastRangedAttack + ASW_RANGED_ATTACK_INTERVAL)
	{		
		if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
		{
			m_fLastRangedAttack = gpGlobals->curtime;
			// randomly either spit or spawn parasites for our ranged attack
			//if (m_iCrittersAlive < ASW_MAX_QUEEN_PARASITES && random->RandomFloat() < 0.5f)
				//return SCHED_ASW_SPAWN_PARASITES;

			return SCHED_RANGE_ATTACK1;
		}	

		/*if ( HasCondition(COND_CAN_RANGE_ATTACK2) )
		{
			m_fLastRangedAttack = gpGlobals->curtime;
			return SCHED_RANGE_ATTACK2;
		}*/
	}
#endif

	if ( HasCondition(COND_NOT_FACING_ATTACK) )
		return SCHED_COMBAT_FACE;
	
	// if we're not attacking, then just look at them
#ifdef ASW_QUEEN_STATIONARY
	return SCHED_COMBAT_FACE;
#else
	return SCHED_CHASE_ENEMY;
#endif
	
	DevWarning( 2, "No suitable combat schedule!\n" );
	return SCHED_FAIL;
}

int CASW_Queen::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_RANGE_ATTACK1 )
	{
		RemoveAllGestures();
		return SCHED_QUEEN_RANGE_ATTACK;	
	}
	else if (scheduleType == SCHED_ASW_ALIEN_MELEE_ATTACK1 || scheduleType == SCHED_MELEE_ATTACK2)
	{
		RemoveAllGestures();
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

bool CASW_Queen::ShouldGib( const CTakeDamageInfo &info )
{
	return false;
}

void CASW_Queen::SetChestOpen(bool bOpen)
{
	if (bOpen != m_bChestOpen)
	{
		m_bChestOpen = bOpen;
	}
}

int CASW_Queen::SelectDeadSchedule()
{
	// Adrian - Alread dead (by animation event maybe?)
	// Is it safe to set it to SCHED_NONE?
	if ( m_lifeState == LIFE_DEAD )
		 return SCHED_NONE;

	CleanupOnDeath();
	return SCHED_DIE;
}

void CASW_Queen::UpdatePoseParams()
{
	/*
	float yaw = m_fLastHeadYaw;  //GetPoseParameter( LookupPoseParameter("head_yaw") );

	if ( m_hQueenEnemy.Get() != NULL )
	{
		Vector	enemyDir = m_hQueenEnemy->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize( enemyDir );
		
		float angle = VecToYaw( BodyDirection3D() );
		float angleDiff = VecToYaw( enemyDir );		
		angleDiff = UTIL_AngleDiff( angleDiff, angle + yaw );
		
		//ASW_ClampYaw(500.0f, yaw, yaw + angleDiff, gpGlobals->frametime);
		//Msg("yaw=%f targ=%f delta=%f ", yaw, yaw+angleDiff, gpGlobals->frametime * 3.0f);
		yaw = ASW_Linear_Approach(yaw, yaw + angleDiff, gpGlobals->frametime * 1200.0f);
		//Msg(" result=%f\n", yaw);
		SetPoseParameter( "head_yaw", yaw );
		m_fLastHeadYaw = yaw;
		//SetPoseParameter( "head_yaw", Approach( yaw + angleDiff, yaw, 5 ) );
	}
	else
	{
		// Otherwise turn the head back to its normal position
		//ASW_ClampYaw(500.0f, yaw, 0, gpGlobals->frametime);
		yaw = ASW_Linear_Approach(yaw, 0, gpGlobals->frametime * 1200.0f);
		SetPoseParameter( "head_yaw", yaw );
		m_fLastHeadYaw = yaw;
		//SetPoseParameter( "head_yaw",	Approach( 0, yaw, 10 ) );		
	}
*/
	float shield = m_fLastShieldPose; //GetPoseParameter( LookupPoseParameter("shield_open") );
	float targetshield = m_bChestOpen ? 1.0f : 0.0f;
	
	if (shield != targetshield)
	{
		shield = ASW_Linear_Approach(shield, targetshield, gpGlobals->frametime * 3.0f);		
		m_fLastShieldPose = shield;
	}
	SetPoseParameter( "shield_open", shield );
}

bool CASW_Queen::ShouldWatchEnemy()
{
	/*Activity nActivity = GetActivity();
	
	if ( ( nActivity == ACT_ANTLIONGUARD_SEARCH ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PEEK_ENTER ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PEEK_EXIT ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PEEK1 ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PEEK_SIGHTED ) || 
		 ( nActivity == ACT_ANTLIONGUARD_SHOVE_PHYSOBJECT ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PHYSHIT_FR ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PHYSHIT_FL ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PHYSHIT_RR ) || 
		 ( nActivity == ACT_ANTLIONGUARD_PHYSHIT_RL ) || 
		 ( nActivity == ACT_ANTLIONGUARD_CHARGE_CRASH ) || 
		 ( nActivity == ACT_ANTLIONGUARD_CHARGE_HIT ) ||
		 ( nActivity == ACT_ANTLIONGUARD_CHARGE_ANTICIPATION ) )
	{
		return false;
	}*/

	return true;
}

void CASW_Queen::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	// Don't do anything after death
	if ( m_NPCState == NPC_STATE_DEAD )
		return;

	m_hQueenEnemy = GetEnemy();
	UpdatePoseParams();
	UpdateDiver();
	//Msg("%f: UpdatePoseParams\n", gpGlobals->curtime);
}

int CASW_Queen::SelectFlinchSchedule_ASW()
{
	// don't flinch if we didn't take any heavy damage
	if ( !HasCondition(COND_HEAVY_DAMAGE) )	//  && !HasCondition(COND_LIGHT_DAMAGE)
		return SCHED_NONE;

	// don't flinch midway through a flinch
	if ( IsCurSchedule( SCHED_BIG_FLINCH ) )
		return SCHED_NONE;

	// only flinch if shot during a melee attack
	//if (! (GetTask() && (GetTask()->iTask == TASK_MELEE_ATTACK1)) )
		//return SCHED_NONE;

	// Do the flinch, if we have the anim
	Activity iFlinchActivity = GetFlinchActivity( true, false );
	if ( HaveSequenceForActivity( iFlinchActivity ) )
		return SCHED_BIG_FLINCH;

	return SCHED_NONE;
}

// see if we've been hurt enough to warrant summoning a wave
int CASW_Queen::SelectSummonSchedule()
{
	switch (m_iSummonWave)
	{
		case 0: if (GetHealth() <= QUEEN_SUMMON_WAVE_POINT_1 * GetMaxHealth()) return SCHED_ASW_RETREAT_AND_SUMMON; break;
		case 1: if (GetHealth() <= QUEEN_SUMMON_WAVE_POINT_2 * GetMaxHealth()) return SCHED_ASW_RETREAT_AND_SUMMON; break;
		case 2: if (GetHealth() <= QUEEN_SUMMON_WAVE_POINT_3 * GetMaxHealth()) return SCHED_ASW_RETREAT_AND_SUMMON; break;
		case 3: if (GetHealth() <= QUEEN_SUMMON_WAVE_POINT_4 * GetMaxHealth()) return SCHED_ASW_RETREAT_AND_SUMMON; break;
		default: return SCHED_NONE; break;
	}

	return SCHED_NONE;
}

void CASW_Queen::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
#ifdef ASW_QUEEN_STATIONARY
	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		{
			// stationary queen doesn't turn
			TaskComplete();
			break;
		}
#endif
	case TASK_CLEAR_BLOCKING_SENTRY:
		{
			m_hBlockingSentry = NULL;
			TaskComplete();
			break;
		}
	case TASK_FACE_SENTRY:
		{
			if (!m_hBlockingSentry.Get())
			{
				TaskFail("No sentry to smash\n");
			}
			else
			{
				Vector vecEnemyLKP = m_hBlockingSentry->GetAbsOrigin();
				if (!FInAimCone( vecEnemyLKP ))
				{
					GetMotor()->SetIdealYawToTarget( vecEnemyLKP );
					GetMotor()->SetIdealYaw( CalcReasonableFacing( true ) ); // CalcReasonableFacing() is based on previously set ideal yaw
					SetTurnActivity(); 
				}
				else
				{
					float flReasonableFacing = CalcReasonableFacing( true );
					if ( fabsf( flReasonableFacing - GetMotor()->GetIdealYaw() ) < 1 )
						TaskComplete();
					else
					{
						GetMotor()->SetIdealYaw( flReasonableFacing );
						SetTurnActivity();
					}
				}
			}
			break;
			
			break;
		}
	case TASK_ASW_SOUND_SUMMON:
		{
			SummonWaveSound();
			TaskComplete();
			break;
		}

	case TASK_ASW_SUMMON_WAVE:
		{
			// fire our outputs to summon waves
			switch (m_iSummonWave)
			{
				case 0: m_OnSummonWave1.FireOutput(this, this); break;
				case 1: m_OnSummonWave2.FireOutput(this, this); break;
				case 2: m_OnSummonWave3.FireOutput(this, this); break;
				case 3: m_OnSummonWave4.FireOutput(this, this); break;
				default: break;
			}
		 	m_iSummonWave++;			
			TaskComplete();
			break;
		}
	case TASK_ASW_WAIT_DIVER:
		{
			// make sure we're still doing this activity (can be broken out of it by a grenade flinch)
			SetIdealActivity((Activity) ACT_QUEEN_TENTACLE_ATTACK);
			break;
		}
	case TASK_ASW_START_DIVER_ATTACK:
		{
			if (m_iDiverState > ASW_QUEEN_DIVER_IDLE)
			{
				// already diver attacking
				TaskComplete();
			}
			m_fLastDiverAttack = gpGlobals->curtime;
			// set us plunging, which will set off the whole diver attacking routine and wait schedules			
			SetDiverState(ASW_QUEEN_DIVER_PLUNGING);
			RemoveAllGestures();
			// make us play an anim while we plunge those divers into the ground
			SetIdealActivity((Activity) ACT_QUEEN_TENTACLE_ATTACK);
			TaskComplete();
			break;
		}
	case TASK_ASW_GET_PATH_TO_RETREAT_SPOT:
		{
			if ( m_hRetreatSpot == NULL )
			{
				TaskFail( "Tried to find a path to NULL retreat spot!\n" );
				break;
			}
			
			Vector vecGoalPos = m_hRetreatSpot->GetAbsOrigin(); 
			AI_NavGoal_t goal( GOALTYPE_LOCATION, vecGoalPos, ACT_RUN );

			if ( GetNavigator()->SetGoal( goal ) )
			{
				if ( asw_queen_debug.GetInt() == 1 )
				{
					NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 0, 255, 0, true, 2.0f );
					NDebugOverlay::Line( vecGoalPos, m_hRetreatSpot->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
				}

				// Face the enemy
				GetNavigator()->SetArrivalDirection( m_hRetreatSpot->GetAbsAngles() );
				TaskComplete();
			}
			else
			{
				if ( asw_queen_debug.GetInt() == 1 )
				{
					NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 255, 0, 0, true, 2.0f );
					NDebugOverlay::Line( vecGoalPos, m_hRetreatSpot->WorldSpaceCenter(), 255, 0, 0, true, 2.0f );					
				}
				TaskFail( "Unable to navigate to retreat spot attack target!\n" );
				break;
			}
		}
		break;
	case TASK_SPAWN_PARASITES:
		{
			RemoveAllGestures();
			SetChestOpen(true);
			// make us play an anim while we spawn parasites
			SetIdealActivity((Activity) ACT_QUEEN_TENTACLE_ATTACK);
			m_fLayParasiteTime = gpGlobals->curtime + 1.0f;
			m_iCrittersSpawnedRecently = 0;
		}
		break;
	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

void CASW_Queen::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ASW_SUMMON_WAVE:
		{
			// should never get into here
			break;
		}
	case TASK_FACE_SENTRY:
		{
			// If the yaw is locked, this function will not act correctly
			Assert( GetMotor()->IsYawLocked() == false );
			if (!m_hBlockingSentry.Get())
			{
				TaskFail("No sentry!\n");
			}
			else
			{
				Vector vecEnemyLKP = m_hBlockingSentry->GetAbsOrigin();
				if (!FInAimCone( vecEnemyLKP ))
				{
					GetMotor()->SetIdealYawToTarget( vecEnemyLKP );
					GetMotor()->SetIdealYaw( CalcReasonableFacing( true ) ); // CalcReasonableFacing() is based on previously set ideal yaw
				}
				else
				{
					float flReasonableFacing = CalcReasonableFacing( true );
					if ( fabsf( flReasonableFacing - GetMotor()->GetIdealYaw() ) > 1 )
						GetMotor()->SetIdealYaw( flReasonableFacing );
				}

				GetMotor()->UpdateYaw();
				
				if ( FacingIdeal() )
				{
					TaskComplete();
				}
			}
			break;
		}
	case TASK_ASW_WAIT_DIVER:
		{
			if (m_iDiverState <= ASW_QUEEN_DIVER_IDLE)
				TaskComplete();
			break;
		}
	case TASK_SPAWN_PARASITES:
		{
			if (gpGlobals->curtime > m_fLayParasiteTime)
			{
				if (m_iCrittersAlive >= ASW_MAX_QUEEN_PARASITES || m_iCrittersSpawnedRecently > ASW_MAX_QUEEN_PARASITES)
				{
					SetChestOpen(false);
					TaskComplete();
				}
				else
				{
					SpawnParasite();
					m_fLayParasiteTime = gpGlobals->curtime + 1.5f;	// setup timer to spawn another one until we're at our max
				}
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

void CASW_Queen::SlashAttack(bool bRightClaw)
{
	//Msg("Queen slash attack\n");
	Vector vecClawBase;
	Vector vecClawTip;
	QAngle angClaw;

	if (bRightClaw)
	{
		if (!GetAttachment( "RightClawBase", vecClawBase, angClaw ))
		{
			Msg("Error, failed to find Queen claw attachment point\n");
			return;
		}
		if (!GetAttachment( "RightClawPoint", vecClawTip, angClaw ))
		{
			Msg("Error, failed to find Queen claw attachment point\n");
			return;
		}
	}
	else
	{
		if (!GetAttachment( "LeftClawBase", vecClawBase, angClaw ))
		{
			Msg("Error, failed to find Queen claw attachment point\n");
			return;
		}
		if (!GetAttachment( "LeftClawPoint", vecClawTip, angClaw ))
		{
			Msg("Error, failed to find Queen claw attachment point\n");
			return;
		}
	}
	if (asw_queen_slash_debug.GetBool())
		Msg("Slash trace: cycle = %f\n", GetCycle());

	// find the midpoint of the claw, then move it back towards the queen origin a bit (this makes her swipes occur as a volume inside her reach, still hitting marines up close)
	Vector vecMidPoint = (vecClawBase + vecClawTip) * 0.5f;
	Vector diff = vecMidPoint - GetAbsOrigin();
	vecMidPoint -= diff * 0.3f;

	// if we don't have a last claw pos, this must be the starting point of a sweep
	//  store current claw pos so we can do the sweeping hull check from here next time
	if (m_vecLastClawPos == vec3_origin)
	{
		m_vecLastClawPos = vecMidPoint;
		return;
	}	

	// sweep an expanded collision test hull from the tip of the claw to the base
	trace_t tr;
	Ray_t ray;
	CASW_TraceFilterOnlyQueenTargets filter( this, COLLISION_GROUP_NONE );						
	ray.Init( m_vecLastClawPos, vecMidPoint, ASW_QUEEN_CLAW_MINS, ASW_QUEEN_CLAW_MAXS );
	enginetrace->TraceRay( ray, MASK_SOLID, &filter, &tr );

	if (tr.m_pEnt)
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		CTakeDamageInfo	info( this, this, ASWGameRules()->ModifyAlienDamageBySkillLevel(ASW_QUEEN_SLASH_DAMAGE), DMG_SLASH );
		info.SetDamagePosition(vecMidPoint);
		Vector force = vecMidPoint - m_vecLastClawPos;
		force.NormalizeInPlace();
		if (force.IsZero())
			info.SetDamageForce( Vector(0.1, 0.1, 0.1) );
		else
			info.SetDamageForce(force * 10000);
		CASW_Alien* pAlien = dynamic_cast<CASW_Alien*>(pEntity);
		if (pAlien)
			pAlien->MeleeBleed(&info);
		CASW_Marine* pMarine = CASW_Marine::AsMarine( pEntity );
		if (pMarine)
			pMarine->MeleeBleed(&info);
		else
		{
			CASW_Colonist *pColonist = dynamic_cast<CASW_Colonist*>(pEntity);
			if (pColonist)
			{
				pColonist->MeleeBleed(&info);
			}
			else
			{
				CASW_Sentry_Base *pSentry = dynamic_cast<CASW_Sentry_Base*>(pEntity);
				if (pSentry)
				{
					// scale the damage up a bit so we don't take so many swipes to kill the sentry
					info.ScaleDamage(5.55f);
					Vector position = pSentry->GetAbsOrigin() + Vector(0,0,30);
					Vector sparkNormal = GetAbsOrigin() - position;
					sparkNormal.z = 0;
					sparkNormal.NormalizeInPlace();
					CPVSFilter filter( position );
					filter.SetIgnorePredictionCull(true);		
					te->Sparks( filter, 0.0, &position, 1, 1, &sparkNormal );
				}
			}
		}
		// change damage type to make sure we burst explosive barrels
		if (dynamic_cast<CBreakableProp*>(pEntity))
			info.SetDamageType(DMG_BULLET);
		pEntity->TakeDamage( info );

		if (asw_queen_slash_debug.GetBool())
		{
			Msg("Slash hit %d %s\n", tr.m_pEnt->entindex(), tr.m_pEnt->GetClassname());
			NDebugOverlay::SweptBox(m_vecLastClawPos, vecMidPoint, ASW_QUEEN_CLAW_MINS, ASW_QUEEN_CLAW_MAXS, vec3_angle, 255, 255, 0, 0 ,1.0f);
			NDebugOverlay::Line(vecMidPoint, tr.m_pEnt->GetAbsOrigin(), 255, 255, 0, false, 1.0f );
		}
	}
	else
	{
		if (asw_queen_slash_debug.GetBool())
		{
			NDebugOverlay::SweptBox(m_vecLastClawPos, vecMidPoint, ASW_QUEEN_CLAW_MINS, ASW_QUEEN_CLAW_MAXS, vec3_angle, 255, 0, 0, 0 ,1.0f);			
		}
	}

	m_vecLastClawPos = vecMidPoint;
}

void CASW_Queen::SpitProjectile()
{
	// Get angle from our head bone attachment (or do it by m_iSpitNum?)
	Vector vecSpitSource;
	QAngle angSpit;
	if (!GetAttachment( "SpitSource", vecSpitSource, angSpit ))
	{
		Msg("Error, failed to find Queen spit attachment point\n");
		return;
	}

	//Msg("SpitSource pos = %s ", VecToString(vecSpitSource));
	//Msg("ang = %s (", VecToString(angSpit));
	Vector vecAiming;
	AngleVectors(angSpit, &vecAiming);
	//Msg("%s)\n", VecToString(vecAiming));
	
	// angle it flat
	angSpit[PITCH] = 0;
	
	// do an autoaim routine in that rough direction to see if we can angle the shot to hit a marine
	Vector vecThrow = GetQueenAutoaimVector(vecSpitSource, angSpit);
	//Msg(" autoaim changed to %s\n", VecToString(vecThrow));

	// setup the speed
	VectorScale( vecThrow, 1000.0f, vecThrow );

	// create it!
	CASW_Queen_Spit::Queen_Spit_Create( vecSpitSource, angSpit, vecThrow, AngularImpulse(0,0,0), this );	

	// problems: shot comes from up high, meaning it should be very easy to dodge
	//   could make it an AoE explosion so if they don't dodge enough, they'll get caught in the splash damage

	// make a sound for the spit
	EmitSound("ASW_Queen.Spit");
}

// don't hurt ourselves ever
float CASW_Queen::GetAttackDamageScale( CBaseEntity *pVictim )
{
	if (pVictim == this)
		return 0;

	return BaseClass::GetAttackDamageScale(pVictim);
}

Vector CASW_Queen::GetQueenAutoaimVector(Vector &spitSrc, QAngle &angSpit)
{
	Vector vecResult;

	// find a marine close to this vector
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return Vector(0,0,0);

	CASW_Marine *pChosenMarine = NULL;
	float fClosestAngle = 999;
	float fChosenYaw = 0;
	Vector vecChosenDiff;
	
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if (!pMarineResource)
			continue;

		CASW_Marine* pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			continue;
		
		Vector diff = pMarine->GetAbsOrigin() - spitSrc;
		if (diff.Length2D() > ASW_QUEEN_MAX_ATTACK_DISTANCE * 1.5f)
			continue;

		// todo: do movement prediction
		float fYawToMarine = UTIL_VecToYaw(diff);
		float fYawDiff = AngleDiff(fYawToMarine, angSpit[YAW]);
		if (abs(fYawDiff) < abs(fClosestAngle))
		{
			fClosestAngle = fYawDiff;
			pChosenMarine = pMarine;
			fChosenYaw = fYawToMarine;
			vecChosenDiff = diff;
		}
	}

	if (pChosenMarine)
	{
		// we have a marine to autoaim at
		if (abs(fClosestAngle) < asw_queen_spit_autoaim_angle.GetFloat())
		{
			// adjust the yaw to point directly at him
			angSpit[YAW] = fChosenYaw;

			// adjust the pitch so it lands near him
			angSpit[PITCH] = UTIL_VecToPitch(vecChosenDiff);

			// convert to a vector
			AngleVectors(angSpit, &vecResult);
			return vecResult;
		}
	}

	AngleVectors(angSpit, &vecResult);
	return vecResult;
}

// todo: should depend on how far away the enemy is?
#define ASW_DIVER_CHASE_TIME 6.0f

void CASW_Queen::SetDiverState(int iNewState)
{
	if (!m_hDiver.Get())
		return;
	m_iDiverState = iNewState;
	if (iNewState >= ASW_QUEEN_DIVER_PLUNGING && iNewState <=ASW_QUEEN_DIVER_RETRACTING)
	{		
		m_hDiver.Get()->SetBurrowing(true);
	}
	else
	{
		m_hDiver.Get()->SetBurrowing(false);
	}

	if (iNewState >= ASW_QUEEN_DIVER_PLUNGING && iNewState <= ASW_QUEEN_DIVER_UNPLUNGING)
	{
		SetChestOpen(true);
		m_fLastDiverAttack = gpGlobals->curtime;
	}
	else
	{
		SetChestOpen(false);
	}
	
	if (iNewState == ASW_QUEEN_DIVER_CHASING)
	{
		if (m_iLiveGrabbers > 0)
		{
			Msg("WARNING: Queen started chasing when already had some live grabbers");
		}
		// we've started a chase! init the grabber pos and set it on its merry way
		
		Vector vecGrabberPos = GetDiverSpot();
		//vecGrabberPos.z += 30;

		CASW_Queen_Grabber* pGrabber = CASW_Queen_Grabber::Create_Queen_Grabber(this, vecGrabberPos, GetAbsAngles());
		if (pGrabber)
		{
			m_iLiveGrabbers = 1;
			pGrabber->MakePrimary();
			pGrabber->m_fMaxChasingTime = gpGlobals->curtime + 6.0f;	// 6 seconds of chasing
			m_hPrimaryGrabber = pGrabber;
		}		
	}
	else if (iNewState == ASW_QUEEN_DIVER_GRABBING)
	{
		m_hGrabbingEnemy = GetEnemy();
	}

	if (iNewState <= ASW_QUEEN_DIVER_IDLE)
		m_hDiver.Get()->SetVisible(false);

	// set time for next diver state
	switch (iNewState)
	{
		case ASW_QUEEN_DIVER_PLUNGING:  m_fNextDiverState = gpGlobals->curtime + 1.0f; break;
		case ASW_QUEEN_DIVER_CHASING:  m_fNextDiverState = 0; break; // Grabber will advance us to the grabbing state when he catches us 
		case ASW_QUEEN_DIVER_GRABBING:  m_fNextDiverState = 0; break; // Grabber will advance us to the retracting state when all grabbers are shot away
		case ASW_QUEEN_DIVER_RETRACTING:  m_fNextDiverState = 0; break; // Primary grabber will advance us to unplunging when he's back home
		case ASW_QUEEN_DIVER_UNPLUNGING:  m_fNextDiverState = gpGlobals->curtime + 1.5f; break;
		default: m_fNextDiverState = 0; break;
	}
	//Msg("Diver state set to %d\n", iNewState);
}

void CASW_Queen::NotifyGrabberKilled(CASW_Queen_Grabber* pGrabber)
{
	if (pGrabber == m_hPrimaryGrabber.Get())
		m_hPrimaryGrabber = NULL;
	m_iLiveGrabbers--;
	if (m_iLiveGrabbers < 0)
	{
		m_iLiveGrabbers = 0;
		Msg("WARNING: Live grabbers went below 0\n");
	}
	if (m_iLiveGrabbers <= 0)
	{
		// all grabbers are dead
		SetDiverState(ASW_QUEEN_DIVER_RETRACTING);

		if (m_hPreventMovementMarine.Get())
			m_hPreventMovementMarine->m_bPreventMovement = false;
	}
}

void CASW_Queen::AdvanceDiverState()
{
	if (m_iDiverState >= ASW_QUEEN_DIVER_UNPLUNGING)
		SetDiverState(ASW_QUEEN_DIVER_IDLE);
	else
		SetDiverState(m_iDiverState + 1);
}

void CASW_Queen::UpdateDiver()
{
	if (m_fNextDiverState != 0)
	{
		if (gpGlobals->curtime >= m_fNextDiverState)
		{
			AdvanceDiverState();
		}		
	}
	if (m_iDiverState == ASW_QUEEN_DIVER_CHASING)
	{
		// todo: pick the grabber target enemy when we start chasing and don't change midchase
		if (!GetEnemy() || !m_hDiver.Get())
		{
			// todo: stop chasing and retract? or change enemy?
		}
		//CASW_Queen_Divers *pDiver = m_hDiver.Get();
		// we're chasing down our enemy! duhh duh duh duh duuhh duh duh duh OH NOES!
		
	}
}

int CASW_Queen::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	CTakeDamageInfo newInfo(info);

	float damage = info.GetDamage();
	
	// reduce all damage because the queen is TUFF!
	damage *= 0.2f;

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

	// make queen immune to buzzers
	if (dynamic_cast<CASW_Buzzer*>(info.GetAttacker()))
	{
		return 0;
	}

	// make queen immune to crush damage
	if (info.GetDamageType() & DMG_CRUSH)
	{
		return 0;
	}

	newInfo.SetDamage(damage);

	return BaseClass::OnTakeDamage_Alive(newInfo);
}

void CASW_Queen::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed(info);

	m_OnQueenKilled.FireOutput(this, this);

	// make sure to kill our grabbers when queen dies
	if (m_hPrimaryGrabber.Get())
	{
		CTakeDamageInfo info2(info);
		info2.SetDamage(1000);
		m_hPrimaryGrabber->OnTakeDamage(info2);
	}

	CEffectData	data;

	data.m_nEntIndex = entindex();

	CPASFilter filter( GetAbsOrigin() );
	filter.SetIgnorePredictionCull(true);
	data.m_vOrigin = GetAbsOrigin();
	DispatchEffect( filter, 0.0, "QueenDie", data );

	// if we're in the middle of plunging divers into the ground, stop it (for other states, killing our grabber will trigger the whole retract sequence of events)
	if (m_iDiverState == ASW_QUEEN_DIVER_PLUNGING)
	{
		SetDiverState(ASW_QUEEN_DIVER_NONE);
		if (m_hDiver.Get())
			m_hDiver.Get()->SetBurrowing(false);
	}
}

Vector CASW_Queen::GetDiverSpot()
{
	Vector vecForward;
		AngleVectors(GetAbsAngles(), &vecForward);
		vecForward.z = 0;
	return GetAbsOrigin() + vecForward * 80;
}

// don't allow us to be hurt by allies
bool CASW_Queen::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (info.GetAttacker() && IsAlienClass( info.GetAttacker()->Classify() ) )
		return false;
	return BaseClass::PassesDamageFilter(info);
}

void CASW_Queen::SetHealthByDifficultyLevel()
{
	int health = 5000;
	if (ASWGameRules())
	{
		switch (ASWGameRules()->GetSkillLevel())
		{
		case 1: health = asw_queen_health_easy.GetInt(); break;
		case 2: health = asw_queen_health_normal.GetInt(); break;
		case 3: health = asw_queen_health_hard.GetInt(); break;
		case 4: health = asw_queen_health_insane.GetInt(); break;
		case 5: health = asw_queen_health_insane.GetInt(); break;
		default: 5000;
		}
	}
	SetHealth(health);
	SetMaxHealth(health);
}

int	CASW_Queen::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText(entindex(),text_offset,m_bChestOpen ? "Chest Open" : "Chest Closed",0);
		text_offset++;

		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "shieldpos %f\n", m_fLastShieldPose);
		NDebugOverlay::EntityText(entindex(),text_offset,buffer,0);
		text_offset++;

		NDebugOverlay::EntityText(entindex(),text_offset, HasCondition(COND_CAN_RANGE_ATTACK1) ? "Can Range Attack" : "No Range attack",0);
		text_offset++;
		NDebugOverlay::EntityText(entindex(),text_offset, HasCondition(COND_CAN_MELEE_ATTACK1) ? "Can Melee Attack1" : "No Melee Attack1",0);
		text_offset++;
		NDebugOverlay::EntityText(entindex(),text_offset, HasCondition(COND_CAN_MELEE_ATTACK2) ? "Can Melee Attack2" : "No Melee aAttack2",0);
		text_offset++;
	}
	return text_offset;
}

void CASW_Queen::DrawDebugGeometryOverlays()
{
	// draw arrows showing the extent of our melee attacks
	for (int i=0;i<360;i+=45)
	{
		float flBaseSize = 10;		

		Vector vBasePos = GetAbsOrigin() + Vector( 0, 0, 5 );
		QAngle angles( 0, 0, 0 );
		Vector vForward, vRight, vUp;		

		float flHeight = ASW_QUEEN_MELEE2_MAX_RANGE;
		angles[YAW] = i;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		NDebugOverlay::Triangle( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 128, 0, 0, 128, false, 0.1 );

		flHeight = ASW_QUEEN_MELEE2_MIN_RANGE;
		angles[YAW] = i+5;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		vBasePos.z += 1;
		NDebugOverlay::Triangle( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 128, false, 0.1 );

		flHeight = ASW_QUEEN_MELEE_RANGE;
		angles[YAW] = i+10;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		vBasePos.z += 2;
		NDebugOverlay::Triangle( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 255, 0, 128, false, 0.1 );
	}

	BaseClass::DrawDebugGeometryOverlays();
}

// the queen often flinches on explosions and fire damage
bool CASW_Queen::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// shock damage never causes flinching
	if (( info.GetDamageType() & DMG_SHOCK ) != 0 )
		return false;

	// explosions always cause a flinch
	if (( info.GetDamageType() & DMG_BLAST ) != 0 )
		return true;

	// flame causes a flinch some of the time
	if (( info.GetDamageType() & DMG_BURN ) != 0 )
	{
		float f = random->RandomFloat();
		bool bFlinch =  (f < asw_queen_flame_flinch_chance.GetFloat());
		if (bFlinch)
			Msg("Queen flinching from fire\n");

		return bFlinch;
	}
	return false;
}

void CASW_Queen::BuildScheduleTestBits( void )
{
	// Ignore damage if we were recently damaged or we're attacking.
	if ( GetActivity() == ACT_MELEE_ATTACK1 || GetActivity() == ACT_MELEE_ATTACK2 )
	{
		ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );	
	}
	BaseClass::BuildScheduleTestBits();
}


CAI_BaseNPC* CASW_Queen::SpawnParasite()
{
	CBaseEntity	*pEntity = CreateEntityByName( "asw_parasite" );
	CAI_BaseNPC	*pNPC = dynamic_cast<CAI_BaseNPC*>(pEntity);

	if ( !pNPC )
	{
		Warning("NULL Ent in CASW_Queen::SpawnParasite\n");
		return NULL;
	}

	// spawn slightly in front of us and up, to be where the chest is
	Vector vecSpawnOffset = Vector(70, 0, 0);	// was 10, if we're attempting the jump
	Vector vecSpawnPos(0,0,0);
	matrix3x4_t matrix;
	QAngle angFacing = GetAbsAngles();
	AngleMatrix( angFacing, matrix );
	VectorTransform(vecSpawnOffset, matrix, vecSpawnPos);
	vecSpawnPos+=GetAbsOrigin();

	//if (m_iCrittersAlive == 0)
	//{
		//NDebugOverlay::Axis(vecSpawnPos, angFacing, 10, false, 20.0f);
		//NDebugOverlay::Axis(GetAbsOrigin(), angFacing, 10, false, 20.0f);
	//}

	pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );	// stops it teleporting to the ground on spawn
	
	pNPC->SetAbsOrigin( vecSpawnPos );

	// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
	QAngle angles = GetAbsAngles();
	angles.x = 0.0;
	angles.z = 0.0;
	// vary the yaw a bit
	angles.y += random->RandomFloat(-30, 30);
	pNPC->SetAbsAngles( angles );

	IASW_Spawnable_NPC* pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pNPC);
	ASSERT(pSpawnable);	
	if ( !pSpawnable )
	{
		Warning("NULL Spawnable Ent in CASW_Queen!\n");
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
		//pParasite->DoJumpFromEgg();
	}	

	return pNPC;
}

void CASW_Queen::ChildAlienKilled(CASW_Alien* pAlien)
{
	m_iCrittersAlive--;
}

bool CASW_Queen::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.pObstruction )
	{
		// check if we collide with a door or door padding
		CASW_Sentry_Base *pSentry = dynamic_cast<CASW_Sentry_Base *>( pMoveGoal->directTrace.pObstruction );
		if (pSentry)
		{
			m_hBlockingSentry = pSentry;
			return true;
		}
	}
	
	return false;
}

// only hits NPCs and sentry gun
bool CASW_TraceFilterOnlyQueenTargets::ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity(pServerEntity, contentsMask) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		CASW_Sentry_Base* pSentry = dynamic_cast<CASW_Sentry_Base*>(pEntity);
		if (pSentry)
			return true;
		CBreakableProp* pProp = dynamic_cast<CBreakableProp*>( pServerEntity );
		if (pProp)
			return true;
		return (pEntity->IsNPC() || pEntity->IsPlayer());
	}
	return false;
}

AI_BEGIN_CUSTOM_NPC( asw_queen, CASW_Queen )

	// Tasks
	DECLARE_TASK( TASK_ASW_SUMMON_WAVE )
	DECLARE_TASK( TASK_ASW_SOUND_SUMMON )
	DECLARE_TASK( TASK_ASW_WAIT_DIVER )
	DECLARE_TASK( TASK_ASW_START_DIVER_ATTACK )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_RETREAT_SPOT )
	DECLARE_TASK( TASK_SPAWN_PARASITES )
	DECLARE_TASK( TASK_FACE_SENTRY )
	DECLARE_TASK( TASK_CLEAR_BLOCKING_SENTRY )

	// Activities
	DECLARE_ACTIVITY( ACT_QUEEN_SCREAM )
	DECLARE_ACTIVITY( ACT_QUEEN_SCREAM_LOW )
	DECLARE_ACTIVITY( ACT_QUEEN_TRIPLE_SPIT )
	DECLARE_ACTIVITY( ACT_QUEEN_SINGLE_SPIT )
	DECLARE_ACTIVITY( ACT_QUEEN_LOW_IDLE )
	DECLARE_ACTIVITY( ACT_QUEEN_LOW_TO_HIGH )
	DECLARE_ACTIVITY( ACT_QUEEN_HIGH_TO_LOW )
	DECLARE_ACTIVITY( ACT_QUEEN_TENTACLE_ATTACK )
	
	// Events
	DECLARE_ANIMEVENT( AE_QUEEN_SLASH_HIT )
	DECLARE_ANIMEVENT( AE_QUEEN_R_SLASH_HIT )
	DECLARE_ANIMEVENT( AE_QUEEN_START_SLASH )
	DECLARE_ANIMEVENT( AE_QUEEN_START_SPIT )
	DECLARE_ANIMEVENT( AE_QUEEN_SPIT )

	// conditions
	DECLARE_CONDITION( COND_QUEEN_BLOCKED_BY_DOOR )
	
	DEFINE_SCHEDULE
	(
		SCHED_QUEEN_RANGE_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_HIGH_TO_LOW"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1		0"
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

	DEFINE_SCHEDULE
	(
		SCHED_ASW_SUMMON_WAVE,

		"	Tasks"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ASW_SOUND_SUMMON	0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_SCREAM"
		"		TASK_ASW_SUMMON_WAVE	0"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_WAIT_DIVER,

		"	Tasks"		
		//"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_LOW_IDLE"
		"		TASK_ASW_WAIT_DIVER	0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_LOW_TO_HIGH"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)
	
	DEFINE_SCHEDULE
	(
		SCHED_START_DIVER_ATTACK,

		"	Tasks"		
		"		TASK_FACE_ENEMY			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_HIGH_TO_LOW"
		"		TASK_ASW_START_DIVER_ATTACK	0"		
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_RETREAT_AND_SUMMON,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_ASW_SUMMON_WAVE"
		"		TASK_ASW_GET_PATH_TO_RETREAT_SPOT	0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ASW_SOUND_SUMMON	0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_SCREAM"
		"		TASK_ASW_SUMMON_WAVE	0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_SMASH_SENTRY,

		"	Tasks"
		"		TASK_FACE_SENTRY		0"
		"		TASK_CLEAR_BLOCKING_SENTRY		0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_SPAWN_PARASITES,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_HIGH_TO_LOW"
		"		TASK_SPAWN_PARASITES	0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_QUEEN_LOW_TO_HIGH"
		""
		"	Interrupts"
		//"		COND_HEAVY_DAMAGE"		// can't do this as we need to be sure the chest closes when leaving this schedule
	)
	
	

AI_END_CUSTOM_NPC()
