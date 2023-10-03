#ifndef _DEFINED_ASW_ALIEN_SHOVER_H
#define _DEFINED_ASW_ALIEN_SHOVER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"
#include "ai_blended_movement.h"

// this class implements physics shoving 

#define	ALIEN_SHOVER_FARTHEST_PHYSICS_OBJECT	350

class CASW_Alien_Shover : public CASW_Alien
{
public:

	DECLARE_CLASS( CASW_Alien_Shover, CASW_Alien  );	
	DECLARE_DATADESC();
	CASW_Alien_Shover( void );

	const impactdamagetable_t &GetPhysicsImpactDamageTable( void );
	void	InputSetShoveTarget( inputdata_t &inputdata );

	DEFINE_CUSTOM_AI;

	void	StartTask( const Task_t *pTask );
	void	RunTask( const Task_t *pTask );
	int		SelectSchedule( void );
	void	Spawn( void );
	void	Activate( void );
	void	HandleAnimEvent( animevent_t *pEvent );
	void	PrescheduleThink( void );
	int		TranslateSchedule( int scheduleType );
	virtual void GatherConditions();
	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	bool OnInsufficientStopDist( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );

	enum
	{
		COND_ALIEN_SHOVER_PHYSICS_TARGET = BaseClass::NEXT_CONDITION,
		COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID,	
		NEXT_CONDITION,
	};

	virtual float GetMaxShoverObjectMass() { return 80.0f; }

protected:	
	virtual int		SelectCombatSchedule( void );
	int		SelectUnreachableSchedule( void );
	void	Shove( void );
	void	UpdatePhysicsTarget( bool bAllowFartherObjects, float flRadius = ALIEN_SHOVER_FARTHEST_PHYSICS_OBJECT );
	void	SweepPhysicsDebris( void );
	void	ImpactShock( const Vector &origin, float radius, float magnitude, CBaseEntity *pIgnored = NULL );

	CBaseEntity *FindPhysicsObjectTarget( CBaseEntity *pTarget, float radius, float targetCone, bool allowFartherObjects = false );
	bool FindNearestPhysicsObject( int iMaxMass );
	Vector	GetPhysicsHitPosition( CBaseEntity *pObject, Vector &vecTrajectory, float &flClearDistance );
	float DistToPhysicsEnt();
	
	bool m_bCanRoar;
	float			m_flNextRoarTime;
	float			m_flPhysicsCheckTime;
	float			m_flNextSwat;

	Vector			m_vecPhysicsTargetStartPos;
	Vector			m_vecPhysicsHitPosition;
					
	EHANDLE			m_hShoveTarget;
	EHANDLE			m_hOldTarget;
	EHANDLE			m_hLastFailedPhysicsTarget;
	EHANDLE			m_hPhysicsTarget;
	EHANDLE			m_hObstructor;
};

enum
{
	SCHED_ALIEN_SHOVER_PHYSICS_ATTACK = LAST_ASW_ALIEN_SHARED_SCHEDULE,
	SCHED_ALIEN_SHOVER_PHYSICS_ATTACK_MOVE,
	SCHED_FORCE_ALIEN_SHOVER_PHYSICS_ATTACK,	
	SCHED_ALIEN_SHOVER_ROAR,
	SCHED_ALIEN_SHOVER_CANT_ATTACK,
	SCHED_ALIEN_ATTACKITEM,
	SCHED_ALIEN_SHOVER_PHYSICS_ATTACKITEM_MOVE,
	SCHED_SHOVER_CHASE_ENEMY,
	LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE,
};

enum
{
	TASK_ALIEN_SHOVER_GET_PATH_TO_PHYSOBJECT = LAST_ASW_ALIEN_SHARED_TASK,
	TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT,		
	TASK_ALIEN_SHOVER_OPPORTUNITY_THROW,
	TASK_ALIEN_SHOVER_FIND_PHYSOBJECT,
	LAST_ASW_ALIEN_SHOVER_SHARED_TASK,
};

extern Activity ACT_ALIEN_SHOVER_ROAR;

#endif // _DEFINED_ASW_ALIEN_SHOVER_H
