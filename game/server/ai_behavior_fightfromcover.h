//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef AI_BEHAVIOR_FIGHTFROMCOVER_H
#define AI_BEHAVIOR_FIGHTFROMCOVER_H

#if defined( _WIN32 )
#pragma once
#endif

#include "ai_goalentity.h"
#include "ai_behavior.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CAI_FightFromCoverGoal : public CAI_GoalEntity
{
	DECLARE_CLASS( CAI_FightFromCoverGoal, CAI_GoalEntity );

public:
	CAI_FightFromCoverGoal();

	const Vector &GetFrontPosition();
	const Vector &GetFrontDirection();
	const QAngle &GetFrontAngles();

	virtual void OnActivate();
	virtual void OnDeactivate();

	virtual void EnableGoal( CAI_BaseNPC *pAI );
	virtual void DisableGoal( CAI_BaseNPC *pAI  );

	void FrontThink();

	virtual void ResolveNames();

	void InputSetDirectionalMarker( inputdata_t &inputdata );

	void BeginMovingFront();
	void EndMovingFront();

	int DrawDebugTextOverlays();

	string_t m_DirectionalMarker;
	string_t m_GenericHintType;

	EHANDLE m_hDirectionalMarker;
	float m_WidthZone;
	float m_LengthZone;
	float m_HeightZone;
	float m_BiasZone;

	Vector m_vFront;
	Vector m_vDir;

	DECLARE_DATADESC();
};


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CAI_FightFromCoverBehavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_FightFromCoverBehavior, CAI_SimpleBehavior );
	DECLARE_DATADESC();
	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
public:

	CAI_FightFromCoverBehavior();

	void SetGoal( CAI_FightFromCoverGoal *pGoal );
	void ClearGoal();

private:

	struct Animation_t
	{
		Animation_t() { Reset(); }
		void Reset() { bActivity = true; id = ACT_INVALID; }

		bool bActivity;
		int id;
	};

	void OnRestore() { UpdateAnimationsFromHint(); }

	void GatherConditions();

	bool CanSelectSchedule();
	int SelectSchedule();

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	bool StartAnimationTask( const Animation_t &animation, bool bReset = false, Activity defaultActivity = ACT_IDLE );

	bool FValidateHintType ( CAI_Hint *pHint );
	static bool HintSearchFilter( void *pContext, CAI_Hint *pCandidate );

	bool IsPointInZone( const Vector &v );

	void OnUpdateShotRegulator();

	void UpdateAnimationsFromHint();
	bool GetAnimation( CScriptScope &scope, const char *pszKey, Animation_t *pAnimation );

	enum
	{
		// Schedules
		SCHED_FFC_RUN_TO_HINT = BaseClass::NEXT_SCHEDULE,
		SCHED_FFC_HOLD_COVER,
		SCHED_FFC_PEEK,
		SCHED_FFC_HOLD_PEEK,
		SCHED_FFC_RELOAD,
		SCHED_FFC_ATTACK,
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_FFC_GET_PATH_TO_HINT = BaseClass::NEXT_TASK,
		TASK_FFC_COVER,
		TASK_FFC_PEEK,
		TASK_FFC_ATTACK,
		NEXT_TASK = BaseClass::NEXT_TASK,
		
		// Conditions
		COND_FFC_HINT_CHANGE = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	virtual const char *GetName() {	return "FightFromCover"; }

	CHandle<CAI_FightFromCoverGoal> m_hGoal;

	CAI_MoveMonitor m_FrontMoveMonitor;
	CSimpleSimTimer m_FrontTimer;

	Animation_t m_EntryAnim;
	Animation_t m_MoveAnim;
	Animation_t m_CoverAnim;
	Animation_t m_ReloadAnim;
	Animation_t m_PeekAnim;
	Animation_t m_ShootAnim;
	Animation_t m_ExitAnim;
};


#endif // AI_BEHAVIOR_FIGHTFROMCOVER_H
