//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_COMBINE_H
#define NPC_COMBINE_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_functank.h"
#include "ai_behavior_rappel.h"
#include "ai_behavior_actbusy.h"
#include "ai_sentence.h"
#include "ai_baseactor.h"
#ifdef EZ
#include "npc_playercompanion.h"
#endif
#ifdef MAPBASE
#include "mapbase/ai_grenade.h"
#include "ai_behavior_police.h"
#endif
#ifdef EXPANDED_RESPONSE_SYSTEM_USAGE
#include "mapbase/expandedrs_combine.h"
//#define CAI_Sentence CAI_SentenceTalker
#define COMBINE_SOLDIER_USES_RESPONSE_SYSTEM 1
#endif

// Used when only what combine to react to what the spotlight sees
#define SF_COMBINE_NO_LOOK	(1 << 16)
#define SF_COMBINE_NO_GRENADEDROP ( 1 << 17 )
#define SF_COMBINE_NO_AR2DROP ( 1 << 18 )
#ifdef EZ
#define SF_COMBINE_COMMANDABLE ( 1 << 19 ) // Added by 1upD - soldier commandable spawnflag
#define SF_COMBINE_REGENERATE ( 1 << 20 ) // Added by 1upD - soldier health regen spawnflag
#define SF_COMBINE_NO_MANHACK_DEPLOY ( 1 << 21 ) // Blixibon -- Manhack toss spawnflag
#endif

//=========================================================
//	>> CNPC_Combine
//=========================================================
#ifdef EZ
class CNPC_Combine : public CNPC_PlayerCompanion
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CNPC_Combine, CNPC_PlayerCompanion );
#else
#ifdef MAPBASE
class CNPC_Combine : public CAI_GrenadeUser<CAI_BaseActor>
{
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
	DECLARE_CLASS( CNPC_Combine, CAI_GrenadeUser<CAI_BaseActor> );
#else
class CNPC_Combine : public CAI_BaseActor
{
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
	DECLARE_CLASS( CNPC_Combine, CAI_BaseActor );
#endif
#endif

public:
	CNPC_Combine();

	// Create components
	virtual bool	CreateComponents();

#ifndef MAPBASE // CAI_GrenadeUser
	bool			CanThrowGrenade( const Vector &vecTarget );
	bool			CheckCanThrowGrenade( const Vector &vecTarget );
#endif
	virtual	bool	CanGrenadeEnemy( bool bUseFreeKnowledge = true );
	virtual bool	CanAltFireEnemy( bool bUseFreeKnowledge );
	int				GetGrenadeConditions( float flDot, float flDist );
	int				RangeAttack2Conditions( float flDot, float flDist ); // For innate grenade attack
	int				MeleeAttack1Conditions( float flDot, float flDist ); // For kick/punch
	bool			FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );
#ifdef EZ2
	void			OnSeeEntity( CBaseEntity *pEntity );
#endif
	virtual bool	IsCurTaskContinuousMove();

	virtual float	GetJumpGravity() const		{ return 1.8f; }

	virtual Vector  GetCrouchEyeOffset( void );

#ifdef MAPBASE
	virtual bool	IsCrouchedActivity( Activity activity );
#endif

	void Event_Killed( const CTakeDamageInfo &info );
#ifdef EZ
	void Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	virtual bool	PassesDamageFilter( const CTakeDamageInfo &info );
	virtual int 	OnTakeDamage_Alive( const CTakeDamageInfo &info );
#endif

	void SetActivity( Activity NewActivity );
	NPC_STATE		SelectIdealState ( void );

	// Input handlers.
	void InputLookOn( inputdata_t &inputdata );
	void InputLookOff( inputdata_t &inputdata );
	void InputStartPatrolling( inputdata_t &inputdata );
	void InputStopPatrolling( inputdata_t &inputdata );
	void InputAssault( inputdata_t &inputdata );
	void InputHitByBugbait( inputdata_t &inputdata );
#ifndef MAPBASE
	void InputThrowGrenadeAtTarget( inputdata_t &inputdata );
#else
	void InputSetElite( inputdata_t &inputdata );

	void InputDropGrenade( inputdata_t &inputdata );

	void InputSetTacticalVariant( inputdata_t &inputdata );

	void InputSetPoliceGoal( inputdata_t &inputdata );
#endif
#ifdef EZ
	void InputSetCommandable( inputdata_t &inputdata );  // New inputs to toggle player commands on / off
	void InputSetNonCommandable(inputdata_t &inputdata);
	void InputRemoveFromPlayerSquad( inputdata_t &inputdata ) { RemoveFromPlayerSquad(); }
	void InputAddToPlayerSquad( inputdata_t &inputdata ) { AddToPlayerSquad(); }
	void InputEnableManhackToss( inputdata_t &inputdata );
	void InputDisableManhackToss( inputdata_t &inputdata );
	void InputDeployManhack( inputdata_t &inputdata );
	void InputAddManhacks( inputdata_t &inputdata );
	void InputSetManhacks( inputdata_t &inputdata );
	void InputEnablePlayerUse( inputdata_t &inputdata );
	void InputDisablePlayerUse( inputdata_t &inputdata );
	void InputEnableOrderSurrender( inputdata_t &inputdata );
	void InputDisableOrderSurrender( inputdata_t &inputdata );
	void InputEnablePlayerGive( inputdata_t &inputdata );
	void InputDisablePlayerGive( inputdata_t &inputdata );
	COutputEHANDLE	m_OutManhack;

	//-----------------------------------------------------
	//	Outputs
	//-----------------------------------------------------	
	virtual void	Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	COutputEvent	m_OnPlayerUse;
	COutputEvent	m_OnFollowOrder;
	void 			ClearFollowTarget();
	// Added by 1upD - methods required for player squad behavior
	virtual bool	IsCommandable(); // This should check the spawn flag
	virtual bool	IsPlayerAlly(CBasePlayer *pPlayer = NULL) { return this->IsCommandable() || BaseClass::IsPlayerAlly(); } // If this NPC is commandable, it IS a player ally regardless of affiliation
	virtual CAI_BaseNPC *GetSquadCommandRepresentative();
	virtual void	GetSquadGoal();
	virtual void	SetCommandGoal(const Vector &vecGoal) { BaseClass::SetCommandGoal(vecGoal); }
	virtual void	ClearCommandGoal() { BaseClass::ClearCommandGoal(); }
	virtual bool	HaveCommandGoal() const;
	virtual bool	TargetOrder(CBaseEntity *pTarget, CAI_BaseNPC **Allies, int numAllies);
	virtual void	MoveOrder(const Vector &vecDest, CAI_BaseNPC **Allies, int numAllies);
	virtual void	OnTargetOrder() {}
	virtual void	OnMoveOrder() {}
	virtual void	ToggleSquadCommand();
	virtual void	AddToPlayerSquad();
	virtual void	RemoveFromPlayerSquad();
	virtual void	FixupPlayerSquad();
	virtual void	UpdateSquadGlow();
	virtual bool    ShouldSquadGlow();
	virtual bool	ShouldRegenerateHealth(void);
	virtual void	UpdateFollowCommandPoint();
	virtual bool	IsFollowingCommandPoint();
	virtual void	FollowSound();
	virtual void	StopFollowSound();
	virtual bool	ShouldAlwaysThink();
	virtual bool	ShouldBehaviorSelectSchedule( CAI_BehaviorBase *pBehavior );
	virtual bool	ShouldLookForBetterWeapon();
	virtual bool	ShouldLookForHealthItem();
	CBaseEntity		*FindHealthItem( const Vector &vecPosition, const Vector &range );
	void			PickupItem( CBaseEntity *pItem );
	virtual void	Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	virtual void	PickupWeapon( CBaseCombatWeapon *pWeapon );
	virtual const char *GetBackupWeaponClass();

	int 			SelectSchedulePriorityAction();
	virtual int 	SelectScheduleRetrieveItem();

	virtual bool	IgnorePlayerPushing( void );

	virtual bool	IsMajorCharacter() { return IsCommandable(); }

	virtual bool	CanOrderSurrender();

	virtual bool	ShouldAllowPlayerGive() { return (m_iCanPlayerGive == TRS_NONE && IsCommandable()) || m_iCanPlayerGive == TRS_TRUE; }
	virtual bool	IsGiveableWeapon( CBaseCombatWeapon *pWeapon ) { return true; }
	virtual bool	IsGiveableItem( CBaseEntity *pItem );
	virtual void	StartPlayerGive( CBasePlayer *pPlayer ) {}
	virtual void	OnCantBeGivenObject( CBaseEntity *pItem ) {}

	virtual bool	ShouldGib( const CTakeDamageInfo &info );
	virtual bool	CorpseGib( const CTakeDamageInfo &info );

	// Blixibon - Elites in ball attacks should aim while moving, even if they can't shoot
	bool			HasAttackSlot() { return BaseClass::HasAttackSlot() || HasStrategySlot( SQUAD_SLOT_SPECIAL_ATTACK ); }

	// 1upD - Accessor for "Disable Player Use" key value
	bool			IsPlayerUseDisabled() { return m_bDisablePlayerUse; };
#endif

	bool			UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	void			Spawn( void );
	void			Precache( void );
	void			Activate();

	Class_T			Classify( void );
	bool			IsElite() { return m_fIsElite; }
#ifdef MAPBASE
	bool			IsAltFireCapable();
	bool			IsGrenadeCapable();
#ifdef EZ2
	void			SetAlternateCapable( bool toggle ) { m_bAlternateCapable = toggle; }
#endif
	const char*		GetGrenadeAttachment() { return "lefthand"; }
#else
#endif
#ifndef MAPBASE // CAI_GrenadeUser
	void			DelayAltFireAttack( float flDelay );
	void			DelaySquadAltFireAttack( float flDelay );
#endif
	float			MaxYawSpeed( void );
	bool			ShouldMoveAndShoot();
	bool			OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );;
	void			HandleAnimEvent( animevent_t *pEvent );
	Vector			Weapon_ShootPosition( );

	Vector			EyeOffset( Activity nActivity );
	Vector			EyePosition( void );
	Vector			BodyTarget( const Vector &posSrc, bool bNoisy = true );
#ifndef MAPBASE // CAI_GrenadeUser
	Vector			GetAltFireTarget();
#endif

	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );
	void			PostNPCInit();
	void			GatherConditions();
	virtual void	PrescheduleThink();

#ifdef EZ
	Disposition_t	IRelationType( CBaseEntity *pTarget );
	int				IRelationPriority( CBaseEntity *pTarget );
#endif
#ifdef MAPBASE
	Activity		Weapon_TranslateActivity( Activity baseAct, bool *pRequired = NULL );
	Activity		NPC_BackupActivity( Activity eNewActivity );
#endif

	Activity		NPC_TranslateActivity( Activity eNewActivity );
	void			BuildScheduleTestBits( void );
	virtual int		SelectSchedule( void );
	virtual int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int				SelectScheduleAttack();

	bool			CreateBehaviors();

#ifdef EZ
	bool			CanDeployManhack( void );
	void			ReleaseManhack( void );
	void			OnAnimEventDeployManhack( animevent_t *pEvent );
	void			OnAnimEventStartDeployManhack( void );
	void			OnScheduleChange();
	virtual void	HandleManhackSpawn( CAI_BaseNPC *pNPC ) {}
#endif

#ifdef EZ2
	virtual bool	OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );
	virtual bool	ShouldAttackObstruction( CBaseEntity *pEntity );
#endif

	bool			OnBeginMoveAndShoot();
	void			OnEndMoveAndShoot();

#ifdef EZ
	bool			PickTacticalLookTarget( AILookTargetArgs_t *pArgs );
	void			OnStateChange( NPC_STATE OldState, NPC_STATE NewState );
	void			AimGun();
#endif

	// Combat
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );
	bool			HasShotgun();
	bool			ActiveWeaponIsFullyLoaded();

	bool			HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt);
#ifdef EZ
	bool			FValidateHintType( CAI_Hint *pHint );
#endif
	const char*		GetSquadSlotDebugName( int iSquadSlot );

	bool			IsUsingTacticalVariant( int variant );
	bool			IsUsingPathfindingVariant( int variant ) { return m_iPathfindingVariant == variant; }

	bool			IsRunningApproachEnemySchedule();

	// -------------
	// Sounds
	// -------------
#ifdef MAPBASE
	void			DeathSound( const CTakeDamageInfo &info );
	void			PainSound( const CTakeDamageInfo &info );
#else
	void			DeathSound( void );
	void			PainSound( void );
#endif
#ifdef EZ
	void			RegenSound(); // Added by 1upD
#endif
	void			IdleSound( void );
	void			AlertSound( void );
	void			LostEnemySound( void );
	void			FoundEnemySound( void );
	void			AnnounceAssault( void );
	void			AnnounceEnemyType( CBaseEntity *pEnemy );
	void			AnnounceEnemyKill( CBaseEntity *pEnemy );

	virtual void	BashSound() { EmitSound( "NPC_Combine.WeaponBash" ); }

	void			NotifyDeadFriend( CBaseEntity* pFriend );

	virtual float	HearingSensitivity( void ) { return 1.0; };
	int				GetSoundInterests( void );
	virtual bool	QueryHearSound( CSound *pSound );

	// Speaking
	void			SpeakSentence( int sentType );
#ifdef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	bool			SpeakIfAllowed( const char *concept, SentencePriority_t sentencepriority = SENTENCE_PRIORITY_NORMAL, SentenceCriteria_t sentencecriteria = SENTENCE_CRITERIA_IN_SQUAD )
	{
		return SpeakIfAllowed( concept, NULL, sentencepriority, sentencecriteria );
	}
	bool			SpeakIfAllowed( const char *concept, const char *modifiers, SentencePriority_t sentencepriority = SENTENCE_PRIORITY_NORMAL, SentenceCriteria_t sentencecriteria = SENTENCE_CRITERIA_IN_SQUAD );
	bool			SpeakIfAllowed( const char *concept, AI_CriteriaSet& modifiers, SentencePriority_t sentencepriority = SENTENCE_PRIORITY_NORMAL, SentenceCriteria_t sentencecriteria = SENTENCE_CRITERIA_IN_SQUAD );
	void			ModifyOrAppendCriteria( AI_CriteriaSet& set );
#endif

#ifdef EZ2
	void			ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet& set );
#endif

	virtual int		TranslateSchedule( int scheduleType );
	void			OnStartSchedule( int scheduleType );

	virtual bool	ShouldPickADeathPose( void );

#ifdef EZ
	// Blixibon - Created to use/deactivate certain code on CAI_PlayerAlly/CNPC_PlayerCompanion.
	bool			IsCombine() { return true; }
#endif

protected:
	void			SetKickDamage( int nDamage ) { m_nKickDamage = nDamage; }
#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	CAI_Sentence< CNPC_Combine > *GetSentences() { return &m_Sentences; }
#endif

#ifdef EZ
	int				m_iManhacks;
	AIHANDLE		m_hManhack = NULL;

	// 1upD - If true, player +USE has no effect
	bool m_bDisablePlayerUse;

	// TRS_NONE = Don't care, modified from global state if the player orders a surrender
	// TRS_TRUE = Can always order surrender
	// TRS_FALSE = Can never order surrender
	ThreeState_t m_iCanOrderSurrender;

	// TRS_NONE = Don't care, only allow give when commandable
	// TRS_TRUE = Can always be given weapons/items
	// TRS_FALSE = Can never be given weapons/items
	ThreeState_t m_iCanPlayerGive;
#endif

#ifdef EZ2
protected:
#else
private:
#endif
	//=========================================================
	// Combine S schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE_SUPPRESS = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE_COMBAT_FAIL,
		SCHED_COMBINE_VICTORY_DANCE,
		SCHED_COMBINE_COMBAT_FACE,
		SCHED_COMBINE_HIDE_AND_RELOAD,
		SCHED_COMBINE_SIGNAL_SUPPRESS,
		SCHED_COMBINE_ENTER_OVERWATCH,
		SCHED_COMBINE_OVERWATCH,
		SCHED_COMBINE_ASSAULT,
		SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE,
		SCHED_COMBINE_PRESS_ATTACK,
		SCHED_COMBINE_WAIT_IN_COVER,
		SCHED_COMBINE_RANGE_ATTACK1,
		SCHED_COMBINE_RANGE_ATTACK2,
		SCHED_COMBINE_TAKE_COVER1,
		SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND,
		SCHED_COMBINE_RUN_AWAY_FROM_BEST_SOUND,
		SCHED_COMBINE_GRENADE_COVER1,
		SCHED_COMBINE_TOSS_GRENADE_COVER1,
		SCHED_COMBINE_TAKECOVER_FAILED,
		SCHED_COMBINE_GRENADE_AND_RELOAD,
		SCHED_COMBINE_PATROL,
		SCHED_COMBINE_BUGBAIT_DISTRACTION,
		SCHED_COMBINE_CHARGE_TURRET,
		SCHED_COMBINE_DROP_GRENADE,
		SCHED_COMBINE_CHARGE_PLAYER,
		SCHED_COMBINE_PATROL_ENEMY,
		SCHED_COMBINE_BURNING_STAND,
		SCHED_COMBINE_AR2_ALTFIRE,
		SCHED_COMBINE_FORCED_GRENADE_THROW,
		SCHED_COMBINE_MOVE_TO_FORCED_GREN_LOS,
		SCHED_COMBINE_FACE_IDEAL_YAW,
		SCHED_COMBINE_MOVE_TO_MELEE,
#ifdef EZ
		SCHED_COMBINE_DEPLOY_MANHACK,
#endif
#ifdef EZ2
		SCHED_COMBINE_ORDER_SURRENDER,
		SCHED_COMBINE_ATTACK_TARGET,
#endif
		NEXT_SCHEDULE,
	};

	//=========================================================
	// Combine Tasks
	//=========================================================
	enum 
	{
		TASK_COMBINE_FACE_TOSS_DIR = BaseClass::NEXT_TASK,
		TASK_COMBINE_IGNORE_ATTACKS,
		TASK_COMBINE_SIGNAL_BEST_SOUND,
		TASK_COMBINE_DEFER_SQUAD_GRENADES,
		TASK_COMBINE_CHASE_ENEMY_CONTINUOUSLY,
		TASK_COMBINE_DIE_INSTANTLY,
		TASK_COMBINE_PLAY_SEQUENCE_FACE_ALTFIRE_TARGET,
		TASK_COMBINE_GET_PATH_TO_FORCED_GREN_LOS,
		TASK_COMBINE_SET_STANDING,
		NEXT_TASK
	};

	//=========================================================
	// Combine Conditions
	//=========================================================
	enum Combine_Conds
	{
		COND_COMBINE_NO_FIRE = BaseClass::NEXT_CONDITION,
		COND_COMBINE_DEAD_FRIEND,
		COND_COMBINE_SHOULD_PATROL,
		COND_COMBINE_HIT_BY_BUGBAIT,
		COND_COMBINE_DROP_GRENADE,
		COND_COMBINE_ON_FIRE,
		COND_COMBINE_ATTACK_SLOT_AVAILABLE,
#ifdef EZ2
		COND_COMBINE_CAN_ORDER_SURRENDER,
		COND_COMBINE_OBSTRUCTED,
#endif
		NEXT_CONDITION
	};

#ifdef EZ
	// Moved down here
	DEFINE_CUSTOM_AI;
#endif

#ifdef EZ2 // Virtual and protected for husks to override
protected:
	// Select the combat schedule
	virtual int SelectCombatSchedule();
private:
#else
private:
	// Select the combat schedule
	int SelectCombatSchedule();
#endif

	// Should we charge the player?
	bool ShouldChargePlayer();

	// Chase the enemy, updating the target position as the player moves
	void StartTaskChaseEnemyContinuously( const Task_t *pTask );
	void RunTaskChaseEnemyContinuously( const Task_t *pTask );

	class CCombineStandoffBehavior : public CAI_ComponentWithOuter<CNPC_Combine, CAI_StandoffBehavior>
	{
		typedef CAI_ComponentWithOuter<CNPC_Combine, CAI_StandoffBehavior> BaseClass;

		virtual int SelectScheduleAttack()
		{
			int result = GetOuter()->SelectScheduleAttack();
			if ( result == SCHED_NONE )
				result = BaseClass::SelectScheduleAttack();
			return result;
		}
	};

#ifdef EZ
	// Blixibon -- Special Combine follow behavior stub since follow behavior overrides a few important schedules.
	class CCombineFollowBehavior : public CAI_FollowBehavior
	{
		DECLARE_CLASS(CCombineFollowBehavior, CAI_FollowBehavior);

		virtual int SelectSchedule();
		virtual int	TranslateSchedule( int scheduleType );

		virtual bool PlayerIsPushing();

		inline CNPC_Combine *GetOuterS() { return static_cast<CNPC_Combine*>(GetOuter()); }
	};
#endif

	// Rappel
	virtual bool IsWaitingToRappel( void ) { return m_RappelBehavior.IsWaitingToRappel(); }
	void BeginRappel() { m_RappelBehavior.BeginRappel(); }
#ifdef EZ2
	// Used by the Arbeit helicopter
	virtual bool HasRappelBehavior() { return true; }
	virtual void StartWaitingForRappel() { m_RappelBehavior.StartWaitingForRappel(); }
#endif

private:
	int				m_nKickDamage;
#ifndef MAPBASE // CAI_GrenadeUser
	Vector			m_vecTossVelocity;
	EHANDLE			m_hForcedGrenadeTarget;
#else
	// Underthrow grenade at target
	bool			m_bUnderthrow;
	bool			m_bAlternateCapable;
#endif
	bool			m_bShouldPatrol;
	bool			m_bFirstEncounter;// only put on the handsign show in the squad's first encounter.
#ifdef EZ
	string_t		m_iszOriginalSquad;
	bool			m_bHoldPositionGoal; // Blixibon - For soldiers staying in their area even after being removed from player's squad

	float			m_flTimePlayerStare;
	bool			m_bTemporarilyNeedWeapon; // Soldiers who drop their weapons but aren't supposed to pick them up autonomously are given this so that they arm themselves again

	float			m_flNextHealthSearchTime;
protected:
	bool			m_bLookForItems;
private:
	EHANDLE			m_hObstructor;
	float			m_flTimeSinceObstructed;
#endif

	// Time Variables
	float			m_flNextPainSoundTime;
	float			m_flNextRegenSoundTime; // Added by 1upD
	float			m_flNextAlertSoundTime;
#ifndef MAPBASE // CAI_GrenadeUser
	float			m_flNextGrenadeCheck;	
#endif
	float			m_flNextLostSoundTime;
	float			m_flAlertPatrolTime;		// When to stop doing alert patrol
#ifndef MAPBASE // CAI_GrenadeUser
	float			m_flNextAltFireTime;		// Elites only. Next time to begin considering alt-fire attack.
#endif

	int				m_nShots;
	float			m_flShotDelay;
	float			m_flStopMoveShootTime;

#ifndef COMBINE_SOLDIER_USES_RESPONSE_SYSTEM
	CAI_Sentence< CNPC_Combine > m_Sentences;
#endif

#ifndef MAPBASE // CAI_GrenadeUser
	int			m_iNumGrenades;
#endif
#ifdef EZ // Blixibon - We inherit most of our behaviors from CNPC_PlayerCompanion now.
protected:
	virtual CAI_StandoffBehavior &GetStandoffBehavior( void ) { return m_StandoffBehavior; } // Blixibon - Added because soldiers have their own special standoff behavior
	virtual CAI_FollowBehavior &GetFollowBehavior( void ) { return m_FollowBehavior; }

	CCombineStandoffBehavior	m_StandoffBehavior;
	CCombineFollowBehavior		m_FollowBehavior;
	CAI_FuncTankBehavior		m_FuncTankBehavior;
	CAI_RappelBehavior			m_RappelBehavior;
#else
	CAI_AssaultBehavior			m_AssaultBehavior;
	CCombineStandoffBehavior	m_StandoffBehavior;
	CAI_FollowBehavior			m_FollowBehavior;
	CAI_FuncTankBehavior		m_FuncTankBehavior;
	CAI_RappelBehavior			m_RappelBehavior;
	CAI_ActBusyBehavior			m_ActBusyBehavior;
#endif
#ifdef MAPBASE
	CAI_PolicingBehavior		m_PolicingBehavior;
#endif

public:
#ifndef MAPBASE // CAI_GrenadeUser
	int				m_iLastAnimEventHandled;
#endif
	bool			m_fIsElite;
#ifndef MAPBASE // CAI_GrenadeUser
	Vector			m_vecAltFireTarget;
#endif

	int				m_iTacticalVariant;
	int				m_iPathfindingVariant;
};


#endif // NPC_COMBINE_H
