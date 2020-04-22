//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_CONTROLLER_H
#define NPC_CONTROLLER_H
#pragma once

#include	"ai_basenpc_flyer.h"

class CSprite;
class CNPC_Controller;

enum
{
	TASK_CONTROLLER_CHASE_ENEMY = LAST_SHARED_TASK,
	TASK_CONTROLLER_STRAFE,
	TASK_CONTROLLER_TAKECOVER,
	TASK_CONTROLLER_FAIL,
};

enum
{
	SCHED_CONTROLLER_CHASE_ENEMY = LAST_SHARED_SCHEDULE,
	SCHED_CONTROLLER_STRAFE,
	SCHED_CONTROLLER_TAKECOVER,
	SCHED_CONTROLLER_FAIL,
};

class CControllerNavigator : public CAI_ComponentWithOuter<CNPC_Controller, CAI_Navigator>
{
	typedef CAI_ComponentWithOuter<CNPC_Controller, CAI_Navigator> BaseClass;
public:
	CControllerNavigator( CNPC_Controller *pOuter )
	 :	BaseClass( pOuter )
	{
	}

	bool ActivityIsLocomotive( Activity activity ) { return true; }
};

class CNPC_Controller : public CAI_BaseFlyingBot
{
public:

	DECLARE_CLASS( CNPC_Controller, CAI_BaseFlyingBot );
	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	float MaxYawSpeed( void ) { return 120.0f; }
	Class_T	Classify ( void ) { return	CLASS_ALIEN_MILITARY; }

	void HandleAnimEvent( animevent_t *pEvent );

	void RunAI( void );

	int RangeAttack1Conditions ( float flDot, float flDist );	// balls
	int RangeAttack2Conditions ( float flDot, float flDist );	// head
	int MeleeAttack1Conditions ( float flDot, float flDist ) { return COND_NONE; }	
	int MeleeAttack2Conditions ( float flDot, float flDist ) { return COND_NONE; }

	int TranslateSchedule( int scheduleType );
	void StartTask ( const Task_t *pTask );
	void RunTask ( const Task_t *pTask );

	void Stop( void );
	bool OverridePathMove( float flInterval );
	bool OverrideMove( float flInterval );

	void MoveToTarget( float flInterval, const Vector &vecMoveTarget );

	void SetActivity ( Activity NewActivity );
	bool ShouldAdvanceRoute( float flWaypointDist );
	int LookupFloat( );

	friend class CControllerNavigator;
	CAI_Navigator *CreateNavigator()
	{
		return new CControllerNavigator( this );
	}

	bool ShouldGib( const CTakeDamageInfo &info );
	bool HasAlienGibs( void ) { return true; }
	bool HasHumanGibs( void ) { return false; }

	float m_flNextFlinch; 

	float m_flShootTime;
	float m_flShootEnd;

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];

	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void Event_Killed( const CTakeDamageInfo &info );

	CSprite *m_pBall[2];	// hand balls
	int m_iBall[2];			// how bright it should be
	float m_iBallTime[2];	// when it should be that color
	int m_iBallCurrent[2];	// current brightness

	Vector m_vecEstVelocity;

	Vector m_velocity;
	bool m_fInCombat;

	void SetSequence( int nSequence );
};

class CNPC_ControllerHeadBall : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CNPC_ControllerHeadBall, CAI_BaseNPC );

	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	void EXPORT HuntThink( void );
	void EXPORT KillThink( void );
	void EXPORT BounceTouch( CBaseEntity *pOther );
	void MovetoTarget( Vector vecTarget );

	int m_iTrail;
	int m_flNextAttack;
	float m_flSpawnTime;
	Vector m_vecIdeal;
	EHANDLE m_hOwner;

	CSprite *m_pSprite;
};

class CNPC_ControllerZapBall : public CAI_BaseNPC
{	
public:	
	DECLARE_CLASS( CNPC_ControllerHeadBall, CAI_BaseNPC );

	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	void EXPORT AnimateThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );

	void Kill( void );

	EHANDLE m_hOwner;
	float m_flSpawnTime;

	CSprite *m_pSprite;
};

#endif //NPC_CONTROLLER_H