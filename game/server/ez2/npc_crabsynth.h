//=============================================================================//
//
// Purpose:		Crab Synth NPC created from scratch in Source 2013 as a new type
//				of Combine armored unit, filling a role somewhere between a standard
//				soldier and a strider.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_CRABSYNTH_H
#define NPC_CRABSYNTH_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_baseactor.h"
#include "ai_tacticalservices.h"
#include "ai_hint.h"
#include "mapbase/ai_grenade.h"
#include "physics_bone_follower.h"

CBaseGrenade *Crabgrenade_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_CrabSynth : public CAI_GrenadeUser< CAI_BaseActor >
{
public:
	DECLARE_CLASS( CNPC_CrabSynth, CAI_GrenadeUser< CAI_BaseActor > );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	DEFINE_CUSTOM_AI;

	CNPC_CrabSynth();

	Class_T			Classify( void ) { return CLASS_COMBINE; }
	int				GetSoundInterests( void ) { return ( SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER | SOUND_BULLET_IMPACT | SOUND_PHYSICS_DANGER | SOUND_MOVE_AWAY ); }
	
	void			Precache( void );
	void			Spawn( void );
	void			Activate( void );
	void			PopulatePoseParameters( void );
	bool			CreateVPhysics();
	float			GetMass();
	void			InitBoneFollowers( void );
	bool			FInViewCone( const Vector &vecSpot );
	bool			FInViewCone( CBaseEntity *pEntity ) { return FInViewCone( pEntity->WorldSpaceCenter() ); }

	void			Event_Killed( const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	float			GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );
	bool			CanFlinch( void );
	bool			IsHeavyDamage( const CTakeDamageInfo &info );

	bool			IsBelowBelly( const Vector &vecPos );
	void			StartBleeding();
	void			BleedThink();

	void			Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy, bool bStrict = false );
	const char		*GetTracerType( void ) { return "AR2Tracer"; }
	void			UpdateMuzzleMatrix();
	void			DoMuzzleFlash();

	bool			OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );
	bool			ShouldAttackObstruction( CBaseEntity *pEntity );
	
	void			ChargeLookAhead( void );
	bool			EnemyIsRightInFrontOfMe( CBaseEntity **pEntity );
	bool			HandleChargeImpact( Vector vecImpact, CBaseEntity *pEntity );
	void			ImpactShock( const Vector &origin, float radius, float magnitude, CBaseEntity *pIgnored = NULL );
	void			ChargeDamage( CBaseEntity *pTarget );
	float			ChargeSteer( void );
	bool			ShouldCharge( const Vector &startPos, const Vector &endPos, bool useTime, bool bCheckForCancel );

	bool			InnateWeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	int				RangeAttack1Conditions( float flDot, float flDist );
	int				RangeAttack2Conditions( float flDot, float flDist );
	int				MeleeAttack1Conditions( float flDot, float flDist );

	void			NPCThink( void );
	void			GatherConditions( void );
	int				SelectSchedule( void );
	virtual int		SelectCombatSchedule( void );
	int				SelectScheduleAttack( void );
	int				SelectUnreachableSchedule( void );
	int				TranslateSchedule( int scheduleType );
	int				SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void			PrescheduleThink();
	void			BuildScheduleTestBits();
	void			OnScheduleChange();
	bool			OnBeginMoveAndShoot();
	void			OnEndMoveAndShoot();
	void			OnRangeAttack1();
	bool			CanGrenadeEnemy( bool bUseFreeKnowledge = true );
	bool			CanThrowGrenade( const Vector &vecTarget );
	bool			CheckCanThrowGrenade( const Vector &vecTarget );
	NPC_STATE		SelectIdealState( void );
	Activity		NPC_TranslateActivity( Activity eNewActivity );
	bool			IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const;
	void			HandleAnimEvent( animevent_t *pEvent );

	void			RunTask( const Task_t *pTask );
	void			StartTask( const Task_t *pTask );

	float			MaxYawSpeed( void );
	bool			ShouldMoveAndShoot();
	void			OnUpdateShotRegulator();
	void			MaintainLookTargets( float flInterval );
	void			SetAim( const Vector &aimDir );
	void			RelaxAim();

	const char		*GetSquadSlotDebugName( int iSquadSlot );

	bool			CanBeSneakAttacked( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr ) { return false; }

	void			UpdateOnRemove( void );
	
	virtual CSprite		*GetGlowSpritePtr( int i );
	virtual void		SetGlowSpritePtr( int i, CSprite *sprite );
	virtual EyeGlow_t	*GetEyeGlowData( int i );
	virtual int			GetNumGlows() { return 4; }

	void			IdleSound( void );
	void			AlertSound( void );
	void			DangerSound( void );
	void			GrenadeWarnSound( void );
	void			PainSound( const CTakeDamageInfo &info );
	void			DeathSound( const CTakeDamageInfo &info );

	void 			InputStartPatrolling( inputdata_t &inputdata );
	void 			InputStopPatrolling( inputdata_t &inputdata );
	void			InputSetChargeTarget( inputdata_t &inputdata );
	void			InputClearChargeTarget( inputdata_t &inputdata );

	void			InputTurnOnProjectedLights( inputdata_t &inputdata ) { m_bProjectedLightEnabled = true; }
	void			InputTurnOffProjectedLights( inputdata_t &inputdata ) { m_bProjectedLightEnabled = false; }
	void			InputSetProjectedLightEnableShadows( inputdata_t &inputdata ) { m_bProjectedLightShadowsEnabled = inputdata.value.Bool(); }
	void			InputSetProjectedLightBrightnessScale( inputdata_t &inputdata ) { m_flProjectedLightBrightnessScale = inputdata.value.Float(); }
	void			InputSetProjectedLightFOV( inputdata_t &inputdata ) { m_flProjectedLightFOV = inputdata.value.Float(); }
	void			InputSetProjectedLightHorzFOV( inputdata_t &inputdata ) { m_flProjectedLightHorzFOV = inputdata.value.Float(); }

private:

	int				m_nShots;
	float			m_flStopMoveShootTime;
	float			m_flBarrelSpinSpeed;

	matrix3x4_t		m_muzzleToWorld;
	int				m_muzzleToWorldTick;
	int				m_iAmmoType;

	//-----------------------------------------------------

	bool			m_bShouldPatrol;

	EHANDLE			m_hObstructor;
	float			m_flTimeSinceObstructed;

	float			m_flNextRoarTime;

	float			m_flChargeTime;
	EHANDLE			m_hChargeTarget;
	EHANDLE			m_hChargeTargetPosition;
	EHANDLE			m_hOldTarget;

	bool			m_bIsBleeding;
	bool			m_bLastDamageBelow;

	//-----------------------------------------------------

	CSprite			*m_pEyeGlow2;
	CSprite			*m_pEyeGlow3;
	CSprite			*m_pEyeGlow4;

	// Controls projected texture spotlights on the client.
	CNetworkVar( bool, m_bProjectedLightEnabled );
	CNetworkVar( bool, m_bProjectedLightShadowsEnabled );
	CNetworkVar( float, m_flProjectedLightBrightnessScale );
	CNetworkVar( float, m_flProjectedLightFOV );
	CNetworkVar( float, m_flProjectedLightHorzFOV );

	//-----------------------------------------------------

	int				m_nAttachmentMuzzle;
	int				m_nAttachmentHead;
	int				m_nAttachmentTopHole;
	int				m_nAttachmentPreLaunchPoint;
	int				m_nAttachmentBelly;
	int				m_nAttachmentMuzzleBehind;

	int				m_nPoseBarrelSpin;

	//-----------------------------------------------------

	bool			m_bDisableBoneFollowers;
	
	// Contained Bone Follower manager
	CBoneFollowerManager m_BoneFollowerManager;
	
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		COND_CRABSYNTH_SHOULD_PATROL = BaseClass::NEXT_CONDITION,
		COND_CRABSYNTH_ATTACK_SLOT_AVAILABLE,
		COND_CRABSYNTH_OBSTRUCTED,
		COND_CRABSYNTH_CAN_CHARGE,
		COND_CRABSYNTH_HAS_CHARGE_TARGET,
		NEXT_CONDITION,

		SCHED_CRABSYNTH_PATROL = BaseClass::NEXT_SCHEDULE,
		SCHED_CRABSYNTH_PATROL_ENEMY,
		SCHED_CRABSYNTH_RANGE_ATTACK1,
		SCHED_CRABSYNTH_ESTABLISH_LINE_OF_FIRE,
		SCHED_CRABSYNTH_TAKE_COVER_FROM_BEST_SOUND,
		SCHED_CRABSYNTH_RUN_AWAY_FROM_BEST_SOUND,
		SCHED_CRABSYNTH_SUPPRESS,
		SCHED_CRABSYNTH_PRESS_ATTACK,
		SCHED_CRABSYNTH_WAIT_IN_COVER,
		SCHED_CRABSYNTH_TAKE_COVER1,
		SCHED_CRABSYNTH_TAKECOVER_FAILED,
		SCHED_CRABSYNTH_BACK_AWAY_FROM_ENEMY,
		SCHED_CRABSYNTH_FACE_IDEAL_YAW,
		SCHED_CRABSYNTH_MOVE_TO_MELEE,
		SCHED_CRABSYNTH_FORCED_GRENADE_THROW,
		SCHED_CRABSYNTH_MOVE_TO_FORCED_GREN_LOS,
		SCHED_CRABSYNTH_RANGE_ATTACK2,
		SCHED_CRABSYNTH_GRENADE_COVER1,
		SCHED_CRABSYNTH_TOSS_GRENADE_COVER1,
		SCHED_CRABSYNTH_ATTACK_TARGET,
		SCHED_CRABSYNTH_ROAR,
		SCHED_CRABSYNTH_CANT_ATTACK,
		SCHED_CRABSYNTH_MELEE_ATTACK1,
		SCHED_CRABSYNTH_CHARGE,
		SCHED_CRABSYNTH_CHARGE_CANCEL,
		SCHED_CRABSYNTH_CHARGE_TARGET,
		SCHED_CRABSYNTH_FIND_CHARGE_POSITION,
		NEXT_SCHEDULE,

		TASK_CRABSYNTH_CHARGE = BaseClass::NEXT_TASK,
		TASK_CRABSYNTH_GET_PATH_TO_CHARGE_POSITION,
		TASK_CRABSYNTH_GET_PATH_TO_FORCED_GREN_LOS,
		TASK_CRABSYNTH_DEFER_SQUAD_GRENADES,
		TASK_CRABSYNTH_FACE_TOSS_DIR,
		TASK_CRABSYNTH_CHECK_STATE,
		NEXT_TASK,
	};
};

#endif // NPC_CRABSYNTH_H
