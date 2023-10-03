#ifndef _INCLUDED_ASW_ALIEN_H
#define _INCLUDED_ASW_ALIEN_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien_shared.h"
#include "ai_basenpc.h"
#include "ai_blended_movement.h"
#include "ai_behavior.h"
#include "ai_behavior_actbusy.h"
#include "iasw_spawnable_npc.h"
#include "asw_lag_compensation.h"
#include "asw_shareddefs.h"

class CASW_AI_Senses;
class TakeDamageInfo;
class CASW_Spawner;
class CAI_ASW_FlinchBehavior;
class CAI_ASW_Behavior;
enum BehaviorEvent_t;

// Keep track of recent damage events for use in asw_ai_behavior_combat_stun.cpp
static const int ASW_NUM_RECENT_DAMAGE = 8;
#define ASW_ALIEN_HEALTH_BITS 14

// Combat Data.
struct CombatConditionData_t 
{
	float m_flMinDist;
	float m_flMaxDist;
	float m_flDotAngle;
	bool m_bCheck;

	CombatConditionData_t()
	{
		m_flMinDist = 0.0f;
		m_flMaxDist = 0.0f;
		m_flDotAngle = 0.0f;
		m_bCheck = false;
	}

	void Init( float flMinDist, float flMaxDist, float flDot, bool bCheck = true )
	{
		m_flMinDist = flMinDist;
		m_flMaxDist = flMaxDist;
		m_flDotAngle = flDot;
		m_bCheck = bCheck;
	}
};

#define COMBAT_COND_NO_FACING_CHECK		-9999.0f

// Base class for all Alien Swarm alien npcs

typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BaseNPC> > CAI_BaseAlienBase;
//typedef CAI_BehaviorHost<CAI_BaseNPC> CAI_BaseAlienBase;

DECLARE_AUTO_LIST( IAlienAutoList );

class CASW_Alien : public CAI_BaseAlienBase, public IASW_Spawnable_NPC, public IAlienAutoList
{
	DECLARE_CLASS( CASW_Alien, CAI_BaseAlienBase );
	DECLARE_SERVERCLASS();

	// shared class members
#include "asw_alien_shared_classmembers.h"

public:
	virtual void NPCInit();	
	virtual void NPCThink();
	virtual void OnRestore();
	virtual void CallBehaviorThink();
	virtual void StartTouch( CBaseEntity *pOther );
	virtual void Spawn();
	float m_flLastThinkTime;

	IMPLEMENT_AUTO_LIST_GET();

	// custom sensing through walls
	virtual void OnSwarmSensed(int iDistance);
	virtual void OnSwarmSenseEntity(CBaseEntity* pEnt) { }
	CAI_Senses* CreateSenses();
	CASW_AI_Senses* GetASWSenses();
	virtual bool QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC );
	void SetDistSwarmSense( float flDistSense );
	bool MarineNearby(float radius, bool bCheck3D = false);
	bool FInViewCone( const Vector &vecSpot );
	bool Knockback(Vector vecForce);
	virtual int DrawDebugTextOverlays();
	virtual void SetDefaultEyeOffset();

	// make the aliens wake up when a marine gets within a certain distance
	void UpdateSleepState( bool bInPVS );
	void UpdateEfficiency( bool bInPVS );
	virtual void UpdateOnRemove();
	bool m_bRegisteredAsAwake;
	float m_fLastSleepCheckTime;
	bool m_bVisibleWhenAsleep;
	
	DECLARE_DATADESC();
	CASW_Alien();
	virtual ~CASW_Alien();

	virtual void Precache();
	
	void ForceFlinch( const Vector &vecSrc );

	// schedule/task stuff
	virtual void StartTask(const Task_t *pTask);
	virtual void RunTask(const Task_t *pTask);
	virtual bool IsCurTaskContinuousMove();
	int TranslateSchedule( int scheduleType );
	virtual void BuildScheduleTestBits();
	virtual void HandleAnimEvent( animevent_t *pEvent );

	// zig zagging
	void AddZigZagToPath();
	bool m_bPerformingZigZag;
	bool ShouldUpdateEnemyPos();
	float GetGoalRepathTolerance( CBaseEntity *pGoalEnt, GoalType_t type, const Vector &curGoal, const Vector &curTargetPos );

	bool m_bRunAtChasingPathEnds;
	
	// movement
	virtual bool ShouldMoveSlow() const;	// has this alien been hurt and so should move slow?
	virtual bool ModifyAutoMovement( Vector &vecNewPos );
	virtual float GetIdealSpeed() const;

	virtual bool ShouldPlayerAvoid( void );


	// soft drone collision
	virtual bool CanBePushedAway();
	virtual void PerformPushaway();
	virtual void SetupPushawayVector();
	virtual float GetSpringColRadius();
	Vector m_vecLastPushAwayOrigin;
	Vector m_vecLastPush;
	bool m_bPushed;
	int m_nAlienCollisionGroup;
	// bleeding
	virtual CBaseEntity		*CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float forceScale = 1.0f, bool bDamageAnyNPC = false );
	virtual CBaseEntity		*CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float flForceScale = 1.0f, bool bDamageAnyNPC = false );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	virtual void DoBloodDecal( float flDamage, const Vector &vecPos, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	void MeleeBleed(CTakeDamageInfo* info);
	bool ShouldGib( const CTakeDamageInfo &info );

	//death
	virtual Activity GetDeathActivity( void );
	virtual bool CanBecomeRagdoll( void );
	bool m_bTimeToRagdoll;
	bool m_bNeverRagdoll;		// set this to true if you only want the alien to die by gibbing or fancy animation - they will never ragdoll
	bool m_bNeverInstagib;		// set this to true if you never want the alien to instagib - make sur eyou have another method of dying or it will fall back to instagib
	float m_fFancyDeathChance;
	virtual bool CanDoFancyDeath( void );
	virtual bool HasDeadBodyGroup() { return false; };
	int m_iDeadBodyGroup;

	// set to true on aliens if they have break pieces that are used for ragdoll gibs 
	virtual bool CanBreak ( void ) { return false; };

	virtual void BreakAlien( const CTakeDamageInfo &info );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual Vector CalcDeathForceVector( const CTakeDamageInfo &info );
	virtual	bool		AllowedToIgnite( void ) { return true; }
	float	m_fNextPainSound;
	float	m_fNextStunSound;
	void Event_Killed( const CTakeDamageInfo &info );
	float m_fHurtSlowMoveTime;
	CNetworkVar(bool, m_bElectroStunned);
	//CNetworkVar(bool, m_bGibber);
	CNetworkVar( DeathStyle_t, m_nDeathStyle );
	CUtlQueueFixed< CTakeDamageInfo, ASW_NUM_RECENT_DAMAGE >	m_RecentDamage;
	// act busy
	bool			CreateBehaviors();
	int SelectSchedule();
	CAI_ActBusyBehavior			m_ActBusyBehavior;
	void	InputBreakWaitForScript( inputdata_t &inputdata );

	// attacking
	virtual bool IsMeleeAttacking();
	virtual bool ShouldStopBeforeMeleeAttack() { return false; }
	virtual int	MeleeAttack1Conditions( float flDot, float flDist );
	virtual int	MeleeAttack2Conditions( float flDot, float flDist );
	virtual int	RangeAttack1Conditions( float flDot, float flDist );
	virtual int	RangeAttack2Conditions( float flDot, float flDist );

	virtual float		InnateRange1MinRange( void ) { return rangeAttack1.m_flMinDist; }
	virtual float		InnateRange1MaxRange( void ) { return rangeAttack1.m_flMaxDist; }

	CombatConditionData_t meleeAttack1;
	CombatConditionData_t meleeAttack2;
	CombatConditionData_t rangeAttack1;
	CombatConditionData_t rangeAttack2;

	// footstepping
	//void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	//surfacedata_t* GetGroundSurface();

	// IASW_Spawnable_NPC implementation
	CHandle<CASW_Base_Spawner> m_hSpawner;
	virtual void SetSpawner(CASW_Base_Spawner* spawner);
	virtual CAI_BaseNPC* GetNPC() { return this; }
	virtual void SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject);
	virtual AlienOrder_t GetAlienOrders();
	virtual void ClearAlienOrders();
	virtual int SelectAlienOrdersSchedule();
	virtual void OnMovementComplete();
	virtual bool ShouldClearOrdersOnMovementComplete();
	virtual void GatherConditions();
	virtual void IgnoreMarines(bool bIgnoreMarines);
	virtual void MoveAside();
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	virtual void Extinguish();
	virtual void ElectroStun( float flStunTime );
	bool IsElectroStunned() { return m_bElectroStunned.Get(); }
	CNetworkVar(bool, m_bOnFire);
	virtual void SetHoldoutAlien() { m_bHoldoutAlien = true; }
	virtual bool IsHoldoutAlien() { return m_bHoldoutAlien; }

	AlienOrder_t m_AlienOrders;
	Vector m_vecAlienOrderSpot;
	EHANDLE m_AlienOrderObject;
	bool m_bIgnoreMarines;
	bool m_bFailedMoveTo;

	// burrowing
	bool		m_bStartBurrowed;
	void	Unburrow( void );
	void	CheckForBlockingTraps();
	void	ClearBurrowPoint( const Vector &origin );
	void	LookupBurrowActivities();
	float	m_flBurrowTime;
	Activity	m_UnburrowActivity;
	Activity	m_UnburrowIdleActivity;
	Vector m_vecUnburrowEndPoint;
	string_t	m_iszUnburrowActivityName;
	string_t	m_iszUnburrowIdleActivityName;

	virtual bool CanStartBurrowed() { return true; }
	virtual void StartBurrowed() { m_bStartBurrowed = true; }
	virtual void SetUnburrowActivity( string_t iszActivityName );
	virtual void SetUnburrowIdleActivity( string_t iszActivityName );


	// move clone
	virtual bool OverrideMove( float flInterval );
	void InputSetMoveClone( inputdata_t &inputdata );
	void SetMoveClone( string_t EntityName, CBaseEntity *pActivator );
	void SetMoveClone( CBaseEntity *pEnt );
	EHANDLE m_hMoveClone;
	string_t m_iMoveCloneName;
	matrix3x4_t m_moveCloneOffset;

	virtual void	SetHealth( int amt )	{ Assert( amt < ( pow( 2.0f, ASW_ALIEN_HEALTH_BITS ) ) ); m_iHealth = amt; }
	virtual void SetHealthByDifficultyLevel();

	// freezeing
	virtual void Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay );
	virtual bool ShouldBecomeStatue( void );
	virtual bool IsMovementFrozen( void ) { return GetFrozenAmount() > 0.5f; }
	void UpdateThawRate();
	float m_flFreezeResistance;
	float m_flFrozenTime;
	float m_flBaseThawRate;

	CASW_Lag_Compensation m_LagCompensation;

	// can a marine see us?
	bool MarineCanSee(int padding, float interval);
	float m_fLastMarineCanSeeTime;
	bool m_bLastMarineCanSee;
	
	int m_iNumASWOrderRetries;

	// notification that an alien we spawned has been killed (used by Harvesters/Queens, which spawn new aliens as part of their attacks)
	virtual void ChildAlienKilled(CASW_Alien *pAlien) { }

	virtual float		StepHeight() const			{ return 24.0f; }

	// dropping money
	virtual void DropMoney( const CTakeDamageInfo &info );
	virtual int GetMoneyCount( const CTakeDamageInfo &info );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );

	CASW_AlienVolley *GetVolley( const char *pszVolleyName );
	int				GetVolleyIndex( const char *pszVolleyName );
	void			CreateVolley( const char *pszVolleyName, const CASW_AlienVolley *pVolley );
	CASW_AlienShot *GetShot( const char *pszShotName );
	int				GetShotIndex( const char *pszShotName );
	void			CreateShot( const char *pszShotName, const CASW_AlienShot *pShot );
	void			CalcMissileVelocity( const Vector &vTargetPos, Vector &vVelocity, float flSpeed, float flAngle );
	virtual void	UpdateRangedAttack();
	void			LaunchMissile( const char *pszVolleyName, Vector &vTargetPosition );
	void	AddBehaviorParam( const char *pszParmName, int nDefaultValue );
	int		GetBehaviorParam( CUtlSymbol ParmName );
	void	SetBehaviorParam( CUtlSymbol ParmName, int nValue );
	CAI_ASW_Behavior	*GetPrimaryASWBehavior();
	virtual void	OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior, CAI_BehaviorBase *pNewBehavior );
	void	SendBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t Event, int nParm, bool bToAllBehaviors );

	int		m_nVolleyType;
	float	m_flRangeAttackStartTime;
	float	m_flRangeAttackLastUpdateTime;
	Vector	m_vecRangeAttackTargetPosition;

	enum
	{
		COND_ASW_BEGIN_COMBAT_STUN = BaseClass::NEXT_CONDITION,
		COND_ASW_FLINCH,
		NEXT_CONDITION,
	};

protected:	
	
	static float sm_flLastHurlTime;

	const char *m_pszAlienModelName;
	bool m_bHoldoutAlien;
	bool m_bBehaviorParameterChanged;
	CUtlMap< CUtlSymbol, int >		m_BehaviorParms;	
	CAI_ASW_FlinchBehavior* m_pFlinchBehavior;
	CAI_BehaviorBase	*m_pPreviousBehavior;
	CUtlVector<CASW_AlienVolley>	m_volleys;
	CUtlVector<CASW_AlienShot>		m_shots;
	DEFINE_CUSTOM_AI;
};

// activities
extern int ACT_MELEE_ATTACK1_HIT;
extern int ACT_MELEE_ATTACK2_HIT;
extern int ACT_ALIEN_FLINCH_SMALL;
extern int ACT_ALIEN_FLINCH_MEDIUM;
extern int ACT_ALIEN_FLINCH_BIG;
extern int ACT_ALIEN_FLINCH_GESTURE;
extern int ACT_DEATH_FIRE;
extern int ACT_DEATH_ELEC;
extern int ACT_DIE_FANCY;

//typedef CAI_BlendingHost< CAI_BehaviorHost<CASW_Alien> > CASW_BlendedAlien;
typedef CASW_Alien  CASW_BlendedAlien;

//class CASW_BlendedAlien : public CAI_BlendingHost<CASW_Alien>
//{
	//DECLARE_CLASS( CASW_BlendedAlien, CAI_BlendingHost<CASW_Alien> );
//};

enum
{
	SCHED_ASW_ALIEN_CHASE_ENEMY = LAST_SHARED_SCHEDULE,
	SCHED_ASW_SPREAD_THEN_HIBERNATE,
	SCHED_ASW_ORDER_MOVE,
	SCHED_ASW_RETRY_ORDERS,
	SCHED_ASW_ALIEN_MELEE_ATTACK1,
	SCHED_ASW_ALIEN_SLOW_MELEE_ATTACK1,
	SCHED_WAIT_FOR_CLEAR_UNBORROW,
	SCHED_BURROW_WAIT,
	SCHED_BURROW_OUT,
	LAST_ASW_ALIEN_SHARED_SCHEDULE,
};

enum 
{
	TASK_ASW_ALIEN_ZIGZAG = LAST_SHARED_TASK,
	TASK_ASW_SPREAD_THEN_HIBERNATE,
	TASK_ASW_BUILD_PATH_TO_ORDER,
	TASK_ASW_ORDER_RETRY_WAIT,
	TASK_ASW_REPORT_BLOCKING_ENT,
	TASK_UNBURROW,
	TASK_CHECK_FOR_UNBORROW,
	TASK_BURROW_WAIT,
	TASK_SET_UNBURROW_ACTIVITY,
	TASK_SET_UNBURROW_IDLE_ACTIVITY,
	TASK_ASW_WAIT_FOR_ORDER_MOVE,
	LAST_ASW_ALIEN_SHARED_TASK,
};

extern ConVar asw_alien_speed_scale_easy;
extern ConVar asw_alien_speed_scale_normal;
extern ConVar asw_alien_speed_scale_hard;
extern ConVar asw_alien_speed_scale_insane;
extern int AE_ALIEN_MELEE_HIT;

// general groundchecks flag used by our alien classes
#define	SF_ANTLION_USE_GROUNDCHECKS		( 1 << 17 )

#endif // _INCLUDED_ASW_ALIEN_H
