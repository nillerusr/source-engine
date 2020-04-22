//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base combat character with no AI
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef HL1TALKNPC_H
#define HL1TALKNPC_H

#ifdef _WIN32
#pragma once
#endif

#include "soundflags.h"

#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_default.h"
#include "ai_speech.h"
#include "ai_basenpc.h"
#include "ai_behavior.h"
#include "ai_behavior_follow.h"
#include "npc_talker.h"


#define SF_NPC_PREDISASTER			( 1 << 16 )	// This is a predisaster scientist or barney. Influences how they speak.



//=========================================================
// Talking NPC base class
// Used for scientists and barneys
//=========================================================

//=============================================================================
// >> CHL1NPCTalker
//=============================================================================

class CHL1NPCTalker : public CNPCSimpleTalker
{
	DECLARE_CLASS( CHL1NPCTalker, CNPCSimpleTalker );
	
public:
	CHL1NPCTalker( void )
	{
	}

	virtual void Precache();

	void	StartTask( const Task_t *pTask );
	void	RunTask( const Task_t *pTask );
	int		SelectSchedule ( void );
	bool	HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);
	bool	ShouldGib( const CTakeDamageInfo &info );

	int		TranslateSchedule( int scheduleType );
	void	IdleHeadTurn( CBaseEntity *pTarget, float flDuration = 0.0, float flImportance = 1.0f );
	void    SetHeadDirection( const Vector &vTargetPos, float flInterval);
	bool	CorpseGib( const CTakeDamageInfo &info );

	Disposition_t IRelationType( CBaseEntity *pTarget );

	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	void			StartFollowing( CBaseEntity *pLeader );
	void			StopFollowing( void );
	int				PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener );

	
	void			Touch( CBaseEntity *pOther );

	float			PickLookTarget( bool bExcludePlayers = false, float minTime = 1.5, float maxTime = 2.5 );

	bool			OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, CBaseDoor *pDoor, float distClear, AIMoveResult_t *pResult );

	// Hacks! HL2 has a system for avoiding the player, we don't
	// This ensures that we fall back to the real player avoidance
	// Essentially does the opposite of what it says
	virtual bool ShouldPlayerAvoid( void ) { return false; }

	bool IsValidSpeechTarget( int flags, CBaseEntity *pEntity );

protected:
	virtual void 	FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int FIdleSpeak ( void );

private:
	virtual void	DeclineFollowing( void ) {}

	virtual int		SelectDeadSchedule( void );
	
public:

	bool	m_bInBarnacleMouth;


	enum
	{
		SCHED_HL1TALKER_FOLLOW_MOVE_AWAY = BaseClass::NEXT_SCHEDULE,
		SCHED_HL1TALKER_IDLE_SPEAK_WAIT,
		SCHED_HL1TALKER_BARNACLE_HIT,
		SCHED_HL1TALKER_BARNACLE_PULL,
		SCHED_HL1TALKER_BARNACLE_CHOMP,
		SCHED_HL1TALKER_BARNACLE_CHEW,

		NEXT_SCHEDULE,
	};

	enum
	{
		TASK_HL1TALKER_FOLLOW_WALK_PATH_FOR_UNITS = BaseClass::NEXT_TASK,

		NEXT_TASK,
	};

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};



#endif		//HL1TALKNPC_H
