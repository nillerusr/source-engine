//================= Copyright LOLOLOLOL, All rights reserved. ==================//
//
// Purpose: Surrender behavior component which allows NPCs to be detained. Partially based on ai_behavior_fear.
// 
// Author: Blixibon, 1upD
//
//=============================================================================//


#ifndef AI_BEHAVIOR_SURRENDER_H
#define AI_BEHAVIOR_SURRENDER_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_behavior.h"
#include "ai_speech.h"

// TODO: ai_goal_surrender?
/*
#include "ai_goalentity.h"

//=========================================================
//=========================================================
class CAI_FearGoal : public CAI_GoalEntity
{
	DECLARE_CLASS( CAI_FearGoal, CAI_GoalEntity );
public:
	CAI_FearGoal()
	{
	}

	void EnableGoal( CAI_BaseNPC *pAI );
	void DisableGoal( CAI_BaseNPC *pAI );

	// Inputs
	virtual void InputActivate( inputdata_t &inputdata );
	virtual void InputDeactivate( inputdata_t &inputdata );

	// Note that the outer is the caller in these outputs
	//COutputEvent	m_OnSeeFearEntity;
	COutputEvent	m_OnArriveAtFearNode;

	DECLARE_DATADESC();

protected:
	// Put something here
};
*/

// Speak concepts
#define TLK_SURRENDER_BEHAVIOR_START				"TLK_SURRENDER_BEHAVIOR_START"
#define TLK_SURRENDER_BEHAVIOR_STARTLE				"TLK_SURRENDER_BEHAVIOR_STARTLE"

class CAI_SurrenderBehavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_SurrenderBehavior, CAI_SimpleBehavior );

public:
	CAI_SurrenderBehavior();
	
	void Precache( void );
	virtual const char *GetName() {	return "Surrender"; }

	virtual bool 	CanSelectSchedule();
	virtual int		SelectSchedule();
	virtual int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	virtual void PrescheduleThink();
	void GatherConditions();
	
	virtual void BeginScheduleSelection();
	virtual void EndScheduleSelection();

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	bool	ShouldPlayerAvoid( void );
	virtual bool IsNavigationUrgent();

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet );

	void BuildScheduleTestBits();
	//int TranslateSchedule( int scheduleType );
	//void OnStartSchedule( int scheduleType );

	//void InitializeBehavior();
	
	void MarkAsUnsafe();
	bool IsInASafePlace();
	void SpoilSafePlace();
	void ReleaseAllHints();

	void	ComputeAndSetRenderBounds();
	void	ResetBounds();

	bool CanSurrender();
	virtual void Surrender( CBaseCombatCharacter *pCaptor );
	void StopSurrendering();
	inline bool IsSurrendered() const { return m_bSurrendered; }

	inline bool SurrenderAutomatically() const { return (m_iSurrenderFlags & SURRENDER_FLAGS_SURRENDER_AUTOMATICALLY); }
	inline void EnableSurrenderAutomatically() { m_iSurrenderFlags |= SURRENDER_FLAGS_SURRENDER_AUTOMATICALLY; }
	inline void DisableSurrenderAutomatically() { m_iSurrenderFlags &= ~SURRENDER_FLAGS_SURRENDER_AUTOMATICALLY; }

	inline int GetFlags() { return m_iSurrenderFlags; }
	inline void SetFlags( int nFlags ) { m_iSurrenderFlags = nFlags; }
	inline void AddFlags( int nFlags ) { m_iSurrenderFlags |= nFlags; }
	inline void RemoveFlags( int nFlags ) { m_iSurrenderFlags &= ~nFlags; }

	bool IsSurrenderIdle();
	bool IsSurrenderIdleStanding();
	bool IsSurrenderIdleOnGround();
	bool IsSurrenderMovingToIdle();
	void SurrenderStand();
	void SurrenderToGround();
	inline bool ShouldBeStanding() { return m_bShouldBeStanding; }

	inline int ResistanceValue() { return m_iResistanceValue; }
	virtual int ModifyResistanceValue( int iVal ) { return iVal; }
	bool CanResist() { return !(m_iSurrenderFlags & SURRENDER_FLAGS_NO_RESISTANCE); }
	CBaseCombatCharacter *GetNearestPotentialCaptor( bool bIncludingPlayer = true, bool bFirst = false, int *pNumCaptors = NULL, bool *bCanBeSeen = NULL );
	CBaseCombatCharacter *GetNearestPotentialRescuer( bool bIncludingPlayer = true, bool bFirst = false, int *pNumRescuers = NULL );

	void RefreshSurrenderIdle();
	inline float NextSurrenderIdleCheck() const { return m_flSurrenderIdleNextCheck; }

	void FindSurrenderDest( CAI_Hint **pOutHint, Vector &vecDest, Vector &vecDir );
	int TranslateSchedule( int scheduleType );

	virtual Activity	NPC_TranslateActivity( Activity activity );

	bool		IsInterruptable( void );

	bool		Speak( AIConcept_t concept );
	bool		Speak( AIConcept_t concept, AI_CriteriaSet &modifiers );
	bool		IsSpeaking();

	enum
	{
		SURRENDER_FLAGS_NONE,
		SURRENDER_FLAGS_SURRENDER_AUTOMATICALLY,
		SURRENDER_FLAGS_NO_RESISTANCE,		// Effectively makes the surrender infinite
	};

	enum
	{
		SCHED_SURRENDER_FACE_CAPTOR = BaseClass::NEXT_SCHEDULE,
		SCHED_SURRENDER_MOVE_TO_SAFE_PLACE,
		SCHED_SURRENDER_MOVE_AWAY,
		SCHED_SURRENDER_STANDING_IDLE,
		SCHED_SURRENDER_FLOOR_ENTER,
		SCHED_SURRENDER_FLOOR_IDLE,
		SCHED_SURRENDER_FLOOR_CURIOUS,
		SCHED_SURRENDER_FLOOR_EXIT,
		NEXT_SCHEDULE,

		TASK_SURRENDER_FACE_CAPTOR = BaseClass::NEXT_TASK,
		TASK_SURRENDER_GET_PATH_TO_SAFE_PLACE,
		TASK_SURRENDER_AT_SAFE_PLACE,
		TASK_SURRENDER_IDLE_LOOP,
		TASK_SURRENDER_IDLE,
		TASK_SURRENDER_SET_CUSTOM_BOUNDS,
		TASK_SURRENDER_RESET_BOUNDS,
		NEXT_TASK,

		COND_SURRENDER_STATE_CHANGE = BaseClass::NEXT_CONDITION,	// Should be standing
		//COND_SURRENDER_INITIAL,	// NOTE: We use a var for this now to prevent losing the value during base AI cycles
		COND_SURRENDER_ANYWHERE,	// Don't move to a hint
		COND_SURRENDER_CANCEL, // Stop surrendering (TODO: Use COND_PROVOKED instead like actbusy does?)
		COND_SURRENDER_ENEMY_CLOSE, // Enemy is close and is targeting me (from ai_behavior_fear)
		COND_SURRENDER_ENEMY_TOO_CLOSE, // Enemy isn't targeting me, but is too close (from ai_behavior_fear)
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

private:

	bool			m_bCanSurrender;		// 1upD - Can this rebel surrender?

	bool			m_bSurrendered;
	bool			m_bJustSurrendered;		// We just surrendered; Use stuff to communicate that fact
	bool			m_bCancelSurrender;
	bool			m_bShouldBeStanding;
	bool			m_bOnGround;
	bool			m_bUsingCustomBounds;

	int				m_iSurrenderFlags;

	float			m_flSurrenderIdleTime;
	float			m_flSurrenderIdleNextCheck;

	int				m_iResistanceValue;

	CAI_MoveMonitor		m_SafePlaceMoveMonitor;
	CHandle<CAI_Hint>	m_hSafePlaceHint;
	CHandle<CAI_Hint>	m_hMovingToHint;

	CHandle<CBaseCombatCharacter>	m_hCaptor;

	DECLARE_DATADESC();
};

#endif // AI_BEHAVIOR_SURRENDER_H


