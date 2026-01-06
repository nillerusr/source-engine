//=============================================================================//
//
// Purpose:		Portal 2 Personality Core NPC created from scratch in Source 2013
//				to fit into the world and mechanics of Half-Life 2. It has the same
//				response and choreography abilities as other ally NPCs and is based
//				largely on the Wilson NPC.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_ARBEIT_PERSONALITY_CORE_H
#define NPC_ARBEIT_PERSONALITY_CORE_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_playerally.h"
#include "ai_sensorydummy.h"
#include "player_pickup.h"

typedef CAI_SensingDummy<CAI_PlayerAlly> CAI_PersonalityCoreBase;

//=========================================================
//	>> CNPC_Arbeit_PersonalityCore
//=========================================================
class CNPC_Arbeit_PersonalityCore : public CAI_PersonalityCoreBase, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CNPC_Arbeit_PersonalityCore, CAI_PersonalityCoreBase );
	DECLARE_DATADESC();
	//DECLARE_SERVERCLASS();

public:
	CNPC_Arbeit_PersonalityCore();

	void		Spawn( void );
	void		Activate( void );
	void		Precache( void );

	bool			CreateVPhysics( void );
	bool			ShouldSavePhysics() { return true; }
	unsigned int	PhysicsSolidMaskForEntity( void ) const;

	int			OnTakeDamage( const CTakeDamageInfo &info );
	void		Event_Killed( const CTakeDamageInfo &info );

	Activity	TranslateCustomizableActivity( const string_t &iszSequence, Activity actDefault );
	Activity 	NPC_TranslateActivity( Activity activity );
	Activity 	GetFlinchActivity( bool bHeavyDamage, bool bGesture );

	void		DoCustomCombatAI( void );


	void		OnSeeEntity( CBaseEntity *pEntity );
	bool		Remark( AI_CriteriaSet &modifiers, CBaseEntity *pRemarkable ) { return SpeakIfAllowed( TLK_REMARK, modifiers ); }
	void 		OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttacker );
	bool		IsValidEnemy( CBaseEntity *pEnemy );
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy );

	void		UpdatePanicLevel();
	void		GatherConditions();
	void		PrescheduleThink();
	int 		TranslateSchedule( int scheduleType );
	void		StartTask( const Task_t *pTask );

	void		HandleAnimEvent( animevent_t *pEvent );

	void		Touch( CBaseEntity *pOther );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	Class_T		Classify( void ) { return CLASS_ARBEIT_TECH; }
	int			GetSoundInterests( void ) { return SOUND_COMBAT | SOUND_DANGER | SOUND_BULLET_IMPACT; }

	float		MaxYawSpeed( void ) { return /*m_bMotionDisabled ? 50 :*/ 0; }
	void		PlayerPenetratingVPhysics( void ) {}
	bool		CanBecomeServerRagdoll( void ) { return false; }

	virtual int		GetNumGlows() { return 1; }

	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );

	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	virtual bool	OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	CBasePlayer		*HasPhysicsAttacker( float dt );

	bool			IsBeingCarriedByPlayer() { return m_flCarryTime != -1.0f; }
	bool			WasJustDroppedByPlayer() { return m_flPlayerDropTime > gpGlobals->curtime; }
	bool			IsHeldByPhyscannon() { return VPhysicsGetObject() && (VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD); }

	bool	ShouldRegenerateHealth( void ) { return false; }
	int		BloodColor( void ) { return DONT_BLEED; }

	void	InputExplode( inputdata_t &inputdata );

	void	InputEnableMotion( inputdata_t &inputdata );
	void	InputDisableMotion( inputdata_t &inputdata );

	void	InputForcePickup( inputdata_t &inputdata );
	void	InputEnablePickup( inputdata_t &inputdata );
	void	InputDisablePickup( inputdata_t &inputdata );

	void	InputPlayAttach( inputdata_t &inputdata );
	void	InputPlayDetach( inputdata_t &inputdata );
	void	InputPlayLock( inputdata_t &inputdata );

	void	InputSetCustomIdleSequence( inputdata_t &inputdata );
	void	InputSetCustomAlertSequence( inputdata_t &inputdata );
	void	InputSetCustomStareSequence( inputdata_t &inputdata );
	void	InputSetCustomPanicSequence( inputdata_t &inputdata );

	void	InputClearCustomIdleSequence( inputdata_t &inputdata );
	void	InputClearCustomAlertSequence( inputdata_t &inputdata );
	void	InputClearCustomStareSequence( inputdata_t &inputdata );
	void	InputClearCustomPanicSequence( inputdata_t &inputdata );

	void	InputSetUprightPose( inputdata_t &inputdata );

	DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		COND_PERSONALITYCORE_PANIC_LEVEL_CHANGE = BaseClass::NEXT_CONDITION,

		SCHED_PERSONALITYCORE_ALERT_STAND = BaseClass::NEXT_SCHEDULE,
		SCHED_PERSONALITYCORE_COMBAT_STAND,
		SCHED_PERSONALITYCORE_PLUG_ATTACH,
		SCHED_PERSONALITYCORE_PLUG_DETACH,
		SCHED_PERSONALITYCORE_PLUG_LOCK,

		//TASK_PERSONALITYCORE_ = BaseClass::NEXT_TASK,
	};

	int m_nPoseUpright;

	COutputEvent m_OnPlayerUse;
	COutputEvent m_OnPlayerPickup;
	COutputEvent m_OnPlayerDrop;
	COutputEvent m_OnDestroyed;

	int		m_nPanicLevel;

	float	m_flCarryTime;
	bool	m_bUseCarryAngles;
	float	m_flPlayerDropTime;
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	bool	m_bMotionDisabled;

	// See CNPC_Arbeit_PersonalityCore::CanBeAnEnemyOf().
	bool	m_bCanBeEnemy;

	string_t	m_iszCustomIdleSequence;
	string_t	m_iszCustomAlertSequence;
	string_t	m_iszCustomStareSequence;
	string_t	m_iszCustomPanicSequence;
};

#endif // NPC_ARBEIT_PERSONALITY_CORE_H
