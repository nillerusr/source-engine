//=============================================================================//
//
// Purpose:		Combine assassins restored in Source 2013. Created mostly from scratch
//				as a new Combine soldier variant focused on stealth, flanking, and fast
//				movements. Initially inspired by Black Mesa's female assassins.
// 
//				This was originally created for Entropy : Zero 2.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_ASSASSIN_H
#define NPC_ASSASSIN_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"
#include "SpriteTrail.h"
#include "soundent.h"

//=========================================================
//=========================================================
class CNPC_Assassin : public CNPC_Combine
{
public:
	DECLARE_CLASS( CNPC_Assassin, CNPC_Combine );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CNPC_Assassin( void );
	
	Class_T		Classify( void )			{ return CLASS_COMBINE;	}

	void		Precache( void );
	void		Spawn( void );
	void		HandleAnimEvent( animevent_t *pEvent );

	void		StartEye( void ); // Start glow effects for this NPC
	void		KillSprites( float flDelay ); // Stop all glow effects

	void		StartEyeTrail();
	void		StopEyeTrail( float flDelay = 0.0f );
	void		InputTurnOnEyeTrail( inputdata_t &inputdata ) { m_bUseEyeTrail = true; StartEyeTrail(); }
	void		InputTurnOffEyeTrail( inputdata_t &inputdata ) { m_bUseEyeTrail = false; StopEyeTrail(); }

	void		PrescheduleThink( void );

	int			TranslateSchedule( int scheduleType );
	int			SelectSchedule();
	int			SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	Vector		GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy );
	Vector		GetActualShootPosition( const Vector &shootOrigin );

#ifdef EZ2
	void		TaskFindDodgeActivity();
	bool		CheckDodgeConditions( bool bSeeEnemy );
	bool		CheckDodge( CBaseEntity *pTarget );
#endif

	void		GatherConditions();
	void		GatherEnemyConditions( CBaseEntity *pEnemy );
	void		BuildScheduleTestBits( void );
	void		OnScheduleChange();
	void		DecalTrace( trace_t *pTrace, char const *decalName );
	bool		FCanCheckAttacks( void );
	bool		CanBeHitByMeleeAttack( CBaseEntity *pAttacker );
	bool		HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );
	void		ModifyOrAppendCriteria( AI_CriteriaSet &set );
	void		Event_Killed( const CTakeDamageInfo &info );
	bool		IsLightDamage( const CTakeDamageInfo &info );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );
	void		AddLeftHandGun( CBaseCombatWeapon *pWeapon );
	void		Weapon_HandleEquip( CBaseCombatWeapon *pWeapon );
	void		Weapon_EquipHolstered( CBaseCombatWeapon *pWeapon );
	void		Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	void		Weapon_Equip( CBaseCombatWeapon *pWeapon );
	Vector		GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );
	float		GetSpreadBias( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget );

	Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	Activity	NPC_TranslateActivity( Activity eNewActivity );
	void		OnChangeActivity( Activity eNewActivity );

	CAI_Hint	*FindVantagePoint();
	bool		FValidateHintType( CAI_Hint *pHint );

	bool		IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
	bool		MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	bool		CanBeSeenBy( CAI_BaseNPC *pNPC );
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy );

	void		StartTask( const Task_t *pTask );
	void		RunTask( const Task_t *pTask );

	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	inline bool		HasDualWeapons() { return m_bDualWeapons; }

	void		KickAttack( bool bLow );
	bool		ShouldFlip();

	void		CheckCloak();
	void		StartCloaking();
	void		StopCloaking();
	bool		ShouldCloak();

	void		InputStartCloaking( inputdata_t &inputdata ) { StartCloaking(); }
	void		InputStopCloaking( inputdata_t &inputdata ) { StopCloaking(); }

	int			DrawDebugTextOverlays( void );
	void		DrawDebugGeometryOverlays( void );

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		COND_ASSASSIN_ENEMY_TARGETING_ME = BaseClass::NEXT_CONDITION,
		COND_ASSASSIN_CLOAK_RETREAT,
		COND_ASSASSIN_INCOMING_PROJECTILE,

		SCHED_ASSASSIN_RANGE_ATTACK1 = BaseClass::NEXT_SCHEDULE,
		SCHED_ASSASSIN_MELEE_ATTACK1,
		SCHED_ASSASSIN_WAIT_FOR_CLOAK,				// When already cloaked
		SCHED_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE,		// When not cloaked
		SCHED_ASSASSIN_GO_TO_VANTAGE_POINT,
		SCHED_ASSASSIN_PERCH,
		SCHED_ASSASSIN_EVADE,
		SCHED_ASSASSIN_FLANK_RANDOM,
		SCHED_ASSASSIN_FLANK_FALLBACK,
		SCHED_ASSASSIN_DODGE_KICK,
		SCHED_ASSASSIN_DODGE_FLIP,

		TASK_ASSASSIN_WAIT_FOR_CLOAK = BaseClass::NEXT_TASK,
		TASK_ASSASSIN_WAIT_FOR_CLOAK_RECHARGE,
		TASK_ASSASSIN_CHECK_CLOAK,
		TASK_ASSASSIN_START_PERCHING,
		TASK_ASSASSIN_PERCH,
		TASK_ASSASSIN_FIND_DODGE_POSITION,
		TASK_ASSASSIN_DODGE_FLIP,
	};

	bool						m_bDualWeapons;
	CHandle<CBaseAnimating>		m_hLeftHandGun;

#ifndef EZ
	CHandle<CSprite>			m_pEyeGlow;
#endif
	CHandle<CSpriteTrail>		m_pEyeTrail;
	float						m_flEyeBrightness;
	bool						m_bUseEyeTrail;

	float		m_flLastFlipEndTime;

#ifdef EZ2
	// Borrowed from hunters
	Activity		m_eDodgeActivity;
	EHANDLE			m_hDodgeTarget;
	CSimpleSimTimer m_IgnoreProjectileTimer;
#endif

	bool		m_bCanCloakDuringAI;	// Whether we can automatically cloak while in combat
	bool		m_bNoCloakSound;		// Turns off default cloak/uncloak sounds
	float		m_flMaxCloakTime;		// The time at which we can no longer be cloaked
	float		m_flNextCloakTime;		// The next time we can be cloaked

	bool		m_bCloaking;			// Indicates cloaking device is activated

	bool		m_bStartCloaked;		// Spawn already cloaked

	float		m_flNextPerchTime;		// The next time we can search for a vantage point
	float		m_flEndPerchTime;		// The time at which we should give up the vantage point (if no enemies appear)

	COutputEvent	m_OnDualWeaponDrop;
	COutputEvent	m_OnStartCloaking;
	COutputEvent	m_OnStopCloaking;
	COutputEvent	m_OnFullyCloaked;
	COutputEvent	m_OnFullyUncloaked;

	DEFINE_CUSTOM_AI;

public:
	CNetworkVar( float, m_flCloakFactor );
};


#endif // NPC_ASSASSIN_H
