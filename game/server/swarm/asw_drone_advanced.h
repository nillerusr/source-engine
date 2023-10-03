#ifndef _DEFINED_ASW_DRONE_ADVANCED_H
#define _DEFINED_ASW_DRONE_ADVANCED_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien_jumper.h"
#include "ai_blended_movement.h"
#include "util_shared.h"

//typedef CAI_BlendingHost< CAI_BehaviorHost<CASW_BlendedAlien> > CAI_DroneBase;

class CASW_Door;
class CASW_Drone_Movement;

class CASW_Drone_Advanced : public CASW_Alien_Jumper
{
public:

	DECLARE_CLASS( CASW_Drone_Advanced, CASW_Alien_Jumper  );
	DECLARE_SERVERCLASS();
	//DECLARE_PREDICTABLE();
	DECLARE_DATADESC();
	CASW_Drone_Advanced( void );
	virtual ~CASW_Drone_Advanced( void );

	virtual void Spawn();
	virtual void Precache();
	CAI_Navigator* CreateNavigator();

	virtual int			MeleeAttack1Conditions( float flDot, float flDist );	
	virtual int			MeleeAttack2Conditions( float flDot, float flDist );
	void MeleeAttack( float distance, float damage, QAngle &viewPunch, Vector &shove );

	virtual bool MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	
	virtual float GetIdealSpeed() const;
	virtual float GetIdealAccel() const;
	virtual float MaxYawSpeed();	
	virtual float GetSequenceGroundSpeed( int iSequence );	
	virtual bool ModifyAutoMovement( Vector &vecNewPos );
	virtual bool IsNavigationUrgent();
	virtual void NPCThink();
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_DRONE; }
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual bool CorpseGib( const CTakeDamageInfo &info );
	virtual bool IsHeavyDamage( const CTakeDamageInfo &info );
	virtual void BuildScheduleTestBits( void );
	virtual bool OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );
	virtual void GatherEnemyConditions( CBaseEntity *pEnemy );
	virtual Activity NPC_TranslateActivity( Activity eNewActivity );
	virtual void RunTaskOverlay();
	virtual void RunTask( const Task_t *pTask );
	virtual bool ShouldGib( const CTakeDamageInfo &info );
	virtual bool CanBreak() { return true; };
	virtual bool HasDeadBodyGroup() { return true; };
	virtual int OnTakeDamage_Dead( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual bool IsMeleeAttacking();
	virtual bool ShouldStopBeforeMeleeAttack() { return !m_bHasAttacked; }
	bool m_bHasAttacked;
	//void RunAttackTask( int task );
	//void ReachedEndOfSequence();

	virtual bool ValidBlockingDoor();
	virtual void GatherConditions();
	virtual int SelectSchedule();
	virtual int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	virtual int SelectFlinchSchedule_ASW();
	virtual void CheckFlinches();
	virtual Activity GetFlinchActivity( bool bHeavyDamage, bool bGesture );
	virtual int TranslateSchedule( int scheduleType );
	virtual void StartTask( const Task_t *pTask );
	virtual bool IsCurTaskContinuousMove();
	virtual bool OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal,
										float distClear,
										AIMoveResult_t *pResult );
	virtual bool OnObstructingASWDoor( AILocalMoveGoal_t *pMoveGoal, 
								 CASW_Door *pDoor,
								 float distClear, 
								 AIMoveResult_t *pResult );
	Activity SelectDoorBash();
	void SetSmallDoorBashHull(bool bSmall);
	bool m_bUsingSmallDoorBashHull;
	float LinearDistanceToDoor();
	void MoveAwayFromDoor();
	bool IsBehindDoor(CBaseEntity *pOther);
	void SetDoorBashYaw();
	virtual bool ShouldClearOrdersOnMovementComplete();
	int m_iDoorPos;	// which part of the door we're going to head towards (0=middle, 1 and 2 are the sides)

	virtual void StartTouch( CBaseEntity *pOther );
	float m_fLastTouchHurtTime;
	float m_flNextSmallFlinchTime;

	// sounds
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound();
	virtual void DeathSound( const CTakeDamageInfo &info );
	
	// overriden movement
	virtual bool OverrideMove( float flInterval );
	virtual bool MoveExecute_Alive(float flInterval);
	Vector m_vecSavedVelocity;
	float m_flSavedSpeed;	
	virtual bool IsMoving();
	bool IsPerformingOverrideMove() const;
	bool m_bPerformingOverride;
	bool FailedOverrideMove() const;	// did we get stuck last time we tried to do an override move?
	bool m_bFailedOverrideMove;
	float m_fFailedOverrideTime;
	virtual bool HasOverridePathTo(CBaseEntity *pEnt);
	bool CheckStuck();
	Vector m_vecLastGoodPosition;
	virtual bool CanBePushedAway();

	// jumping
	virtual bool ShouldJump();

	virtual int DrawDebugTextOverlays();

	virtual void SetHealthByDifficultyLevel();

	static float s_fNextTooCloseChatterTime;

	CNetworkVar( EHANDLE, m_hAimTarget );

	enum
	{
		COND_DRONE_BLOCKED_BY_DOOR = BaseClass::NEXT_CONDITION,
		COND_DRONE_DOOR_OPENED,
		COND_DRONE_LOS,		// drone has soft line of sight to his enemy
		COND_DRONE_GAINED_LOS,		// drone didn't have soft los last think, but does now
		COND_DRONE_LOST_LOS,	// drone had soft line of sight last think, but has lost it this think
		NEXT_CONDITION,
	};

private:
	CHandle< CASW_Door > m_hBlockingDoor;
	float				 m_flDoorBashYaw;
	bool				m_bJumper;	// can this drone jump?
	
	CSimTimer 		 m_DurationDoorBash;
	bool m_bDoneAlienCloseChatter;	// has this alien made a marine shout out in fear yet?

	Activity m_FlinchActivity;

	float m_fLastLostLOSTime;
	bool m_bLastSoftLOS;

	Vector m_vecEnemyStandoffPosition;

	DEFINE_CUSTOM_AI;	
};

enum
{
	TASK_DRONE_YAW_TO_DOOR = LAST_ASW_ALIEN_JUMPER_SHARED_TASK,
	TASK_DRONE_ATTACK_DOOR,
	TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE,
	TASK_DRONE_GET_PATH_TO_DOOR,
	TASK_DRONE_WAIT_FOR_DOOR_MOVEMENT,
	TASK_DRONE_DOOR_WAIT,
	TASK_DRONE_GET_CIRCLE_PATH,
	TASK_DRONE_WAIT_FACE_ENEMY,
	LAST_ASW_DRONE_SHARED_TASK,
};

class CDroneTraceFilterLOS : public CTraceFilterSimple
{
public:
	CDroneTraceFilterLOS( IHandleEntity *pHandleEntity, int collisionGroup );
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask );
};


#endif // _DEFINED_ASW_DRONE_ADVANCED_H
