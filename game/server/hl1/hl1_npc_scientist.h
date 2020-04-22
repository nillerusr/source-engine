//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef	NPC_SCIENTIST_H
#define	NPC_SCIENTIST_H

#include "hl1_npc_talker.h"

//=========================================================
//=========================================================
class CNPC_Scientist : public CHL1NPCTalker
{
	DECLARE_CLASS( CNPC_Scientist, CHL1NPCTalker );
public:
	
//	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();


	void	Precache( void );
	void	Spawn( void );
	void	Activate();
	Class_T Classify( void );
	int		GetSoundInterests ( void );

	virtual void ModifyOrAppendCriteria( AI_CriteriaSet& set );

	virtual int ObjectCaps( void ) { return UsableNPCObjectCaps(BaseClass::ObjectCaps()); }
	float	MaxYawSpeed( void );

	float	TargetDistance( void );
	bool	IsValidEnemy( CBaseEntity *pEnemy );


	int		OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	void	Event_Killed( const CTakeDamageInfo &info );

	void	Heal( void );
	bool	CanHeal( void );

	int		TranslateSchedule( int scheduleType );
	void	HandleAnimEvent( animevent_t *pEvent );
	int		SelectSchedule( void );
	void	StartTask( const Task_t *pTask );
	void	RunTask( const Task_t *pTask );
	
	NPC_STATE SelectIdealState ( void );
	
	int		FriendNumber( int arrayNumber );
	
	bool	DisregardEnemy( CBaseEntity *pEnemy ) { return !pEnemy->IsAlive() || (gpGlobals->curtime - m_flFearTime) > 15; }
	
	void	TalkInit( void );
	
	void	DeclineFollowing( void );

	bool	CanBecomeRagdoll( void );
	bool	ShouldGib( const CTakeDamageInfo &info );
	
	void	SUB_StartLVFadeOut( float delay = 10.0f, bool bNotSolid = true );
	void	SUB_LVFadeOut( void  );

	void	Scream( void );
	
	Activity GetStoppedActivity( void );
	Activity NPC_TranslateActivity( Activity newActivity );

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );

	enum
	{
		SCHED_SCI_HEAL = BaseClass::NEXT_SCHEDULE,
		SCHED_SCI_FOLLOWTARGET,
		SCHED_SCI_STOPFOLLOWING,
		SCHED_SCI_FACETARGET,
		SCHED_SCI_COVER,
		SCHED_SCI_HIDE,
		SCHED_SCI_IDLESTAND,
		SCHED_SCI_PANIC,
		SCHED_SCI_FOLLOWSCARED,
		SCHED_SCI_FACETARGETSCARED,
		SCHED_SCI_FEAR,
		SCHED_SCI_STARTLE,
	};

	enum
	{
		TASK_SAY_HEAL = BaseClass::NEXT_TASK,
		TASK_HEAL,
		TASK_SAY_FEAR,
		TASK_RUN_PATH_SCARED,
		TASK_SCREAM,
		TASK_RANDOM_SCREAM,
		TASK_MOVE_TO_TARGET_RANGE_SCARED,
	};

	DEFINE_CUSTOM_AI;

private:
	
	float m_flFearTime;
	float m_flHealTime;
	float m_flPainTime;
	//float	m_flResponseDelay;
};

//=========================================================
// Sitting Scientist PROP
//=========================================================

class CNPC_SittingScientist : public CNPC_Scientist // kdb: changed from public CBaseMonster so he can speak
{
	DECLARE_CLASS( CNPC_SittingScientist, CNPC_Scientist );
public:

//	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void  Spawn( void );
	void  Precache( void );

	int FriendNumber( int arrayNumber );
	
	void SittingThink( void );

	virtual void SetAnswerQuestion( CNPCSimpleTalker *pSpeaker );
	int		m_baseSequence;	
	int		m_iHeadTurn;
	float	m_flResponseDelay;

	//DEFINE_CUSTOM_AI;
};

#endif // NPC_SCIENTIST_H