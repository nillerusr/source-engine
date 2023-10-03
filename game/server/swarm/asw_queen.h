#ifndef _INCLUDED_ASW_QUEEN_H
#define _INCLUDED_ASW_QUEEN_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"
#include "util_shared.h"

class CASW_Queen_Divers;
class CASW_Queen_Grabber;
class CASW_Marine;
class CASW_Sentry_Base;

class CASW_Queen : public CASW_Alien
{
public:

	DECLARE_CLASS( CASW_Queen, CASW_Alien  );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CASW_Queen( void );
	virtual ~CASW_Queen( void );

	virtual void Spawn();
	virtual void NPCInit();
	virtual void Precache();
	virtual float GetIdealSpeed() const;
	virtual float GetIdealAccel( ) const;
	virtual float MaxYawSpeed( void );
	
	//virtual void NPCThink();
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_QUEEN; }

	virtual bool OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );
	virtual bool OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult );
	
	virtual void BuildScheduleTestBits();
	virtual void GatherConditions();
	virtual int SelectSchedule();
	virtual int SelectQueenCombatSchedule();
	virtual int SelectDeadSchedule();
	virtual int SelectFlinchSchedule_ASW();
	virtual bool IsHeavyDamage( const CTakeDamageInfo &info );
	virtual int SelectSummonSchedule();	
	virtual void RunTask( const Task_t *pTask );
	virtual void StartTask( const Task_t *pTask );
	virtual int TranslateSchedule( int scheduleType );	
	virtual bool FCanCheckAttacks();
	virtual bool WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions);
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual bool ShouldGib( const CTakeDamageInfo &info );
	virtual void SlashAttack( bool bRightClaw = false );
	virtual int MeleeAttack1Conditions ( float flDot, float flDist );
	virtual int MeleeAttack2Conditions ( float flDot, float flDist );
	virtual float InnateRange1MinRange( void );
	virtual float InnateRange1MaxRange( void );
	virtual int RangeAttack1Conditions ( float flDot, float flDist );
	virtual float GetAttackDamageScale( CBaseEntity *pVictim );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool PassesDamageFilter( const CTakeDamageInfo &info );

	virtual void SpitProjectile();
	Vector GetQueenAutoaimVector(Vector &spitSrc, QAngle &angSpit);

	virtual	bool		AllowedToIgnite( void ) { return false; }

	// head turning
	bool ShouldWatchEnemy();
	void UpdatePoseParams();
	void PrescheduleThink();

	// opening/closing the vulnerable chest
	void SetChestOpen(bool bOpen);
	CNetworkVar(bool, m_bChestOpen);
	float m_fLastHeadYaw;
	float m_fLastShieldPose;
	int DrawDebugTextOverlays();
	void DrawDebugGeometryOverlays();
	
	// sounds
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound();
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AttackSound();
	virtual void IdleSound();
	virtual void SummonWaveSound();

	QAngle m_angQueenFacing;
	int m_iSpitNum;

	CNetworkHandle(CBaseEntity, m_hQueenEnemy);

	// i/o
	COutputEvent m_OnSummonWave1;
	COutputEvent m_OnSummonWave2;
	COutputEvent m_OnSummonWave3;
	COutputEvent m_OnSummonWave4;
	COutputEvent m_OnQueenKilled;

	int m_iSummonWave;
	Vector m_vecLastClawPos;

	virtual void SetHealthByDifficultyLevel();

	// various states of the digger attack	
	int m_iDiverState;
	float m_fNextDiverState;
	float m_fLastDiverAttack;
	void UpdateDiver();
	void AdvanceDiverState();
	void SetDiverState(int iNewState);
	CHandle<CASW_Queen_Divers> m_hDiver;	
	void NotifyGrabberKilled(CASW_Queen_Grabber* pGrabber);
	int m_iLiveGrabbers;
	CHandle<CASW_Marine> m_hPreventMovementMarine;
	CHandle<CASW_Queen_Grabber> m_hPrimaryGrabber;
	EHANDLE m_hGrabbingEnemy;
	Vector GetDiverSpot();

	// retreating to a fixed spot
	EHANDLE m_hRetreatSpot;

	// ranged attacks
	float m_fLastRangedAttack;

	// spawning parasites
	CAI_BaseNPC* SpawnParasite();
	virtual void ChildAlienKilled(CASW_Alien* pAlien);
	int m_iCrittersAlive;		// how many spawned critters we currently have
	float m_fLayParasiteTime;
	int m_iCrittersSpawnedRecently;

	// blocked by a sentry gun
	CHandle<CASW_Sentry_Base> m_hBlockingSentry;

	enum
	{
		COND_QUEEN_BLOCKED_BY_DOOR = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

private:
	DEFINE_CUSTOM_AI;
};

class CASW_TraceFilterOnlyQueenTargets : public CTraceFilterSimple
{
public:
	CASW_TraceFilterOnlyQueenTargets( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask );
};

enum
{
	SCHED_ASW_SUMMON_WAVE = LAST_ASW_ALIEN_SHARED_SCHEDULE,	
	SCHED_WAIT_DIVER,
	SCHED_START_DIVER_ATTACK,
	SCHED_ASW_RETREAT_AND_SUMMON,
	SCHED_QUEEN_RANGE_ATTACK,
	SCHED_ASW_SPAWN_PARASITES,
	SCHED_SMASH_SENTRY,
	LAST_ASW_QUEEN_SHARED_SCHEDULE,
};

enum
{
	TASK_DRONE_YAW_TO_DOOR = LAST_ASW_ALIEN_SHARED_TASK,
	TASK_ASW_SUMMON_WAVE,
	TASK_ASW_SOUND_SUMMON,
	TASK_ASW_WAIT_DIVER,
	TASK_ASW_START_DIVER_ATTACK,
	TASK_ASW_GET_PATH_TO_RETREAT_SPOT,
	TASK_SPAWN_PARASITES,
	TASK_FACE_SENTRY,
	TASK_CLEAR_BLOCKING_SENTRY,
	LAST_ASW_QUEEN_SHARED_TASK,
};

// queen diver states
enum 
{
	ASW_QUEEN_DIVER_NONE,
	ASW_QUEEN_DIVER_IDLE,
	ASW_QUEEN_DIVER_PLUNGING,
	ASW_QUEEN_DIVER_CHASING,
	ASW_QUEEN_DIVER_GRABBING,
	ASW_QUEEN_DIVER_RETRACTING,
	ASW_QUEEN_DIVER_UNPLUNGING
};

#endif	//_INCLUDED_ASW_QUEEN_H
