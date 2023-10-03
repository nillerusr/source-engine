#ifndef _INCLUDED_ASW_SHIELDBUG_H
#define _INCLUDED_ASW_SHIELDBUG_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien_shover.h"

DECLARE_AUTO_LIST( IShieldbugAutoList )

class CASW_Shieldbug : public CASW_Alien_Shover, public IShieldbugAutoList
{
	DECLARE_CLASS( CASW_Shieldbug, CASW_Alien_Shover );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	IMPLEMENT_AUTO_LIST_GET( );

	CASW_Shieldbug();
	virtual ~CASW_Shieldbug();

	void Spawn();
	void Precache();
	virtual void NPCThink();
	virtual int	DrawDebugTextOverlays();
	float GetIdealSpeed() const;
	float GetIdealAccel( ) const;
	float GetSequenceGroundSpeed( int iSequence );
	int MeleeAttack1Conditions( float flDot, float flDist );
	int MeleeAttack2Conditions ( float flDot, float flDist );
	void MeleeAttack( float distance, float damage, QAngle &viewPunch, Vector &shove );
	float MaxYawSpeed( void );
	void HandleAnimEvent( animevent_t *pEvent );	
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SHIELDBUG; }	
	bool CorpseGib( const CTakeDamageInfo &info );
	void BuildScheduleTestBits( void );
	void GatherEnemyConditions( CBaseEntity *pEnemy );
	void StartTask(const Task_t *pTask);
	void RunTask( const Task_t *pTask );
	int SelectSchedule();
	int TranslateSchedule( int scheduleType );
	bool OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );
	void SetTurnActivity();	
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL );
	virtual bool ShouldDefend();
	float m_fNextHeadhitAttack;
	float m_fLastHurtTime;
	bool m_bLastShouldDefend;
	int m_nExtraHeath;

	virtual Activity NPC_TranslateActivity( Activity baseAct );
	
	bool ShouldGib( const CTakeDamageInfo &info ) ;

	virtual void PrescheduleThink();
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound();
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AttackSound();
	virtual void IdleSound();
   	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual bool HasDeadBodyGroup() { return true; };
	virtual void GatherConditions();
	virtual bool CanBreak ( void ) { return true; };
	virtual Activity GetDeathActivity( void );

	bool m_bDefending;
	float m_fNextFlinchTime;
	virtual bool CanFlinch();
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool BlockedDamage( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	void CheckForShieldbugHint( const CTakeDamageInfo &info );

	virtual float GetMaxShoverObjectMass() { return 200.0f; }

	virtual void SetHealthByDifficultyLevel();

	virtual int GetMoneyCount( const CTakeDamageInfo &info );

	float m_fMarineBlockCounter;
	float m_fLastMarineBlockTime;
	float m_flDefendDuration;

	static float s_fNextSpottedChatterTime;

	enum
	{
		COND_SHIELDBUG_CHANGE_DEFEND = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	bool m_bHasBeenHurt;

protected:
	DEFINE_CUSTOM_AI;
};

enum
{
	SCHED_SHIELDBUG_FLINCH_LEFT = LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE,
	SCHED_SHIELDBUG_FLINCH_RIGHT = LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE+1,	
	SCHED_ASW_SHIELDBUG_MELEE_ATTACK1 = LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE+2,
	SCHED_ASW_SHIELDBUG_MELEE_ATTACK2 = LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE+3,
	SCHED_SHIELDBUG_START_DEFENDING,
	SCHED_SHIELDBUG_LEAVE_DEFENDING,
	LAST_ASW_SHIELDBUG_SHARED_SCHEDULE,
};

extern int ACT_START_DEFENDING;
extern int ACT_LEAVE_DEFENDING;
extern int ACT_RUN_DEFEND;
extern int ACT_IDLE_DEFEND;
extern int ACT_FLINCH_LEFTARM_DEFEND;
extern int ACT_FLINCH_RIGHTARM_DEFEND;
extern int ACT_MELEE_ATTACK1_DEFEND;
extern int ACT_TURN_LEFT_DEFEND;
extern int ACT_TURN_RIGHT_DEFEND;
extern int ACT_ALIEN_SHOVER_ROAR_DEFEND;

//enum 
//{
	//TASK_SHIELDBUG_FLINCH = LAST_ASW_ALIEN_SHARED_TASK,	
	//LAST_ASW_SHIELDBUG_SHARED_TASK,
//};

#endif // _INCLUDED_ASW_SHIELDBUG_H
