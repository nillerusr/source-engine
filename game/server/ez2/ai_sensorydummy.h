//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Sensory dummy for Bad Cop's NPC component and the Wilson NPC.
//
//=============================================================================//

#ifndef AI_SENSORYDUMMY_H
#define AI_SENSORYDUMMY_H
#pragma once

#include "cbase.h"
#include "ai_senses.h"
#include "ai_memory.h"

//-----------------------------------------------------------------------------
// Purpose: Sensory dummy that does nothing but sense.
// 
// You could have it keep track of sounds, enemies, etc. but it won't actually do anything about it.
// For example, E:Z2 creates a model-less dummy parented to the player that uses their relationships, positions, etc.
// to essentially function as a "NPC component" so the player code could know when there's enemies and when there's danger.
// 
// NPC created by Blixibon.
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_SensingDummy : public BASE_NPC
{
	DECLARE_CLASS(CAI_SensingDummy, BASE_NPC);
public:

	// Sensing flags applied to the NPC senses.
	// Derived classes should override this based on what senses they want to use.
	virtual int		GetSensingFlags( void ) { return 0; }

public:

	void	Spawn();

	float	MaxYawSpeed( void ) { return 0; }
	bool    CanBecomeRagdoll(void) { return false; }
	bool	CanBecomeServerRagdoll( void ) { return false; }

	int		GetSoundInterests( void ) { return SOUND_COMBAT | SOUND_DANGER | SOUND_BULLET_IMPACT; }

	void			GatherConditions( void );

	int				SelectSchedule( void );
	int				SelectIdleSchedule( void );
	int				SelectAlertSchedule( void );
	int				SelectCombatSchedule( void );

	void			StartTask( const Task_t *pTask );

	// Override loud or costly virtual functions sensory dummies aren't interested in.
	void		SetPlayerAvoidState( void ) {}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_SensingDummy<BASE_NPC>::Spawn()
{
	BaseClass::Spawn();

	this->NPCInit();

	this->SetState(NPC_STATE_IDLE);

	this->GetSenses()->AddSensingFlags( this->GetSensingFlags() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_SensingDummy<BASE_NPC>::GatherConditions()
{
	BaseClass::GatherConditions();

	if (this->GetState() == NPC_STATE_COMBAT && !this->GetEnemy())
	{
		// Prevents being stuck on SCHED_COMBAT_STAND forever when the enemy disappears.
		this->SetCondition(COND_IDLE_INTERRUPT);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Decides which type of schedule best suits the NPC's current 
// state and conditions. Then calls NPC's member function to get a pointer 
// to a schedule of the proper type.
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_SensingDummy<BASE_NPC>::SelectSchedule( void )
{
	// Sensing dummies only care about a small number of schedules, otherwise their AI can freak out or run irrelvant code.
	switch( this->m_NPCState )
	{
	case NPC_STATE_IDLE:
		AssertMsgOnce( this->GetEnemy() == NULL, "NPC has enemy but is not in combat state?" );
		return this->SelectIdleSchedule();

	case NPC_STATE_ALERT:
		AssertMsgOnce( this->GetEnemy() == NULL, "NPC has enemy but is not in combat state?" );
		return this->SelectAlertSchedule();

	case NPC_STATE_COMBAT:
		return this->SelectCombatSchedule();
	}

	// Dummies probably won't find themselves in any other state, but whatever.
	// Shouldn't be the end of the world.
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_SensingDummy<BASE_NPC>::SelectIdleSchedule( void )
{
	if ( this->HasCondition ( COND_HEAR_DANGER ) ||
		 this->HasCondition ( COND_HEAR_COMBAT ) ||
		 this->HasCondition ( COND_HEAR_WORLD  ) ||
		 this->HasCondition ( COND_HEAR_BULLET_IMPACT ) ||
		 this->HasCondition ( COND_HEAR_PLAYER ) )
	{
		return SCHED_ALERT_FACE_BESTSOUND;
	}

	return SCHED_IDLE_STAND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_SensingDummy<BASE_NPC>::SelectAlertSchedule( void )
{
	if ( this->HasCondition ( COND_HEAR_DANGER ) ||
		 this->HasCondition ( COND_HEAR_COMBAT ) ||
		 this->HasCondition ( COND_HEAR_WORLD  ) ||
		 this->HasCondition ( COND_HEAR_BULLET_IMPACT ) ||
		 this->HasCondition ( COND_HEAR_PLAYER ) )
	{
		return SCHED_ALERT_FACE_BESTSOUND;
	}

	if ( gpGlobals->curtime - this->GetEnemies()->LastTimeSeen( AI_UNKNOWN_ENEMY ) < TIME_CARE_ABOUT_DAMAGE )
		return SCHED_ALERT_FACE;

	return SCHED_ALERT_STAND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_SensingDummy<BASE_NPC>::SelectCombatSchedule( void )
{
	if ( this->HasCondition(COND_NEW_ENEMY) && gpGlobals->curtime - this->GetEnemies()->FirstTimeSeen(this->GetEnemy()) < 2.0 )
	{
		return SCHED_WAKE_ANGRY;
	}
	
	if ( this->HasCondition( COND_ENEMY_DEAD ) || !this->GetEnemy() )
	{
		// clear the current (dead) enemy and try to find another.
		this->SetEnemy( NULL );
		 
		if ( this->ChooseEnemy() )
		{
			this->ClearCondition( COND_ENEMY_DEAD );
			return this->SelectSchedule();
		}

		this->SetState( NPC_STATE_ALERT );
		return this->SelectSchedule();
	}

	return SCHED_COMBAT_STAND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_SensingDummy<BASE_NPC>::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// The issue is that Will-E/sensing dummies can't turn their bodies, so any orders to "face" end up stuck on the face tasks.
		// Below are all of the basic facing tasks in the base AI. All of them are set up to be completed immediately.
		// This is important to make sure we don't get stuck on one schedule forever, which causes problems.
		case TASK_FACE_HINTNODE:
		case TASK_FACE_LASTPOSITION:
		case TASK_FACE_SAVEPOSITION:
		case TASK_FACE_AWAY_FROM_SAVEPOSITION:
		case TASK_FACE_TARGET:
		case TASK_FACE_IDEAL:
		case TASK_FACE_SCRIPT:
		case TASK_FACE_PATH:
		case TASK_FACE_REASONABLE:
			{
				this->TaskComplete();
				break;
			}
	}

	BaseClass::StartTask(pTask);
}

#endif
