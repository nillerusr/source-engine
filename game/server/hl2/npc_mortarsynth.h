//======= Copyright © 2022, Obsidian Conflict Team, All rights reserved. =======//
//
// Purpose: Mortar Synth NPC
//
//==============================================================================//

#ifndef NPC_MORTARSYNTH_H
#define NPC_MORTARSYNTH_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_basescanner.h"
#ifdef EZ2
#include "SpriteTrail.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_Mortarsynth : public CNPC_BaseScanner
{
public:
	DECLARE_CLASS( CNPC_Mortarsynth, CNPC_BaseScanner );
	DEFINE_CUSTOM_AI;

	CNPC_Mortarsynth();

	Class_T			Classify( void ) { return( CLASS_COMBINE ); }
	int				GetSoundInterests( void ) { return ( SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER | SOUND_BULLET_IMPACT | SOUND_PHYSICS_DANGER | SOUND_MOVE_AWAY ); }
	
	void			Precache( void );
	void			Spawn( void );
	void			Activate( void );

#ifdef EZ2
	void			StartEye( void );
	void			KillSprites( float flDelay );
	void			StartPropulsionTrail( void );
	void			StopPropulsionTrail( float flDelay );

	QAngle			BodyAngles();
#endif

	void			Event_Killed( const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void	Gib( void );
	virtual bool	HasHumanGibs( void ) { return false; }; //Fenix: Hack to get rid of the human gibs set to CLASS_COMBINE

	Activity		NPC_TranslateActivity( Activity eNewActivity );

	int				SelectSchedule( void );
	virtual int		TranslateSchedule( int scheduleType );
	int				SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void			BuildScheduleTestBits();
	void			HandleAnimEvent( animevent_t *pEvent );

	void			RunTask( const Task_t *pTask );
	void			StartTask( const Task_t *pTask );

	void			UpdateOnRemove( void );

#ifdef EZ2
	void			StartSmokeTrail( void );
	void			BleedThink();
#endif

	virtual bool	IsHeavyDamage( const CTakeDamageInfo &info );

	bool			CanThrowGrenade( const Vector &vecTarget );
	bool			CheckCanThrowGrenade( const Vector &vecTarget );
	virtual	bool	CanGrenadeEnemy( bool bUseFreeKnowledge = true );

#ifdef EZ2
	int				RangeAttack1Conditions( float flDot, float flDist );
	int				MeleeAttack1Conditions( float flDot, float flDist );
#endif

	void			OnScheduleChange( void );

	virtual	bool	CanRepelEnemy();

	virtual void	StopLoopingSounds( void );

#ifdef EZ2
	// E:Z2 uses sound prefix system
	char			*GetScannerSoundPrefix( void ) { return "NPC_MortarSynth"; }

	void			DangerSound( void ) { EmitSound( "NPC_MortarSynth.Danger" ); };
#else
	//Fenix: Override baseclass functions to disable the sound prefix system
	void			IdleSound( void ) {};
	void			DeathSound( const CTakeDamageInfo &info ) {};
	void			AlertSound( void ) {};
	void			PainSound( const CTakeDamageInfo &info ) {};
#endif

protected:
#ifdef EZ2
	Vector	VelocityToAvoidObstacles( float flInterval );
	float GetMaxSpeed();
#endif
	virtual float	MinGroundDist( void );

	virtual void	MoveExecute_Alive( float flInterval );
	virtual bool	OverridePathMove( CBaseEntity *pMoveTarget, float flInterval );
	bool			OverrideMove( float flInterval );
	void			MoveToTarget( float flInterval, const Vector &MoveTarget );
	virtual void	MoveToAttack( float flInterval );

	void			PlayFlySound( void );

	void			UseRepulserWeapon( void );
	void			PushEntity( CBaseEntity *pTarget, CTakeDamageInfo &info );

	virtual void	TurnHeadToTarget( float flInterval, const Vector &moveTarget );


	float			m_flNextFlinch;
	float			m_flNextRepulse;

private:
	Vector			m_vecTossVelocity;

#ifdef EZ2
	CHandle<CSpriteTrail>		m_pPropulsionTrail;

	bool			m_bIsBleeding;
	bool			m_bDidBoostEffect;
	float			m_fHeadPitch;

	int				m_nPoseHeadPitch;
	int				m_nPoseInjured;
	int				m_nPosePropulsionYaw;
	int				m_nPosePropulsionPitch;
	int				m_nAttachmentBleed;
	int				m_nAttachmentPropulsion;
	int				m_nAttachmentPropulsionL;
	int				m_nAttachmentPropulsionR;
#endif

	// Custom schedules
	enum
	{
		SCHED_MORTARSYNTH_PATROL = BaseClass::NEXT_SCHEDULE,
		SCHED_MORTARSYNTH_ATTACK,
		SCHED_MORTARSYNTH_REPEL,
		SCHED_MORTARSYNTH_CHASE_ENEMY,
		SCHED_MORTARSYNTH_CHASE_TARGET,
		SCHED_MORTARSYNTH_HOVER,
#ifdef EZ2
		SCHED_MORTARSYNTH_MELEE_ATTACK1,
		SCHED_MORTARSYNTH_MOVE_TO_MELEE,
		SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE,
		SCHED_MORTARSYNTH_ALERT,
		SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY,
		SCHED_MORTARSYNTH_FLEE_FROM_BEST_SOUND,
#endif

		NEXT_SCHEDULE,
	};

	// Custom tasks
	enum
	{
		TASK_ENERGY_REPEL = BaseClass::NEXT_TASK,

		NEXT_TASK,
	};

	DECLARE_DATADESC();
};

#endif // NPC_DEFENDER_H
