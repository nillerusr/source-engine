#ifndef _INCLUDED_ASW_ZOMBIE_H
#define _INCLUDED_ASW_ZOMBIE_H

#include "iasw_spawnable_npc.h"
#include "npc_BaseZombie.h"

class CSprite;

class CASW_Zombie : public CAI_BlendingHost<CNPC_BaseZombie>, public IASW_Spawnable_NPC
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CASW_Zombie, CAI_BlendingHost<CNPC_BaseZombie> );
	DECLARE_SERVERCLASS();

public:
	CASW_Zombie()
	 : m_DurationDoorBash( 2, 6),
	   m_NextTimeToStartDoorBash( 3.0 ),
	   m_bHoldoutAlien( false )
	{
	}

	void Spawn( void );
	void OnRestore();
	void Precache( void );

	void SetZombieModel( void );
	void MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize );
	bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold );
	bool CanBecomeLiveTorso() { return !m_fIsHeadless; }

	void GatherConditions( void );

	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int TranslateSchedule( int scheduleType );

	bool FInViewCone( const Vector &vecSpot );
	bool ShouldPlayerAvoid( void );
	virtual float GetIdealSpeed() const;

	virtual void StudioFrameAdvance();

#ifndef HL2_EPISODIC
	void CheckFlinches() {} // Zombie has custom flinch code
#endif // HL2_EPISODIC

	Activity NPC_TranslateActivity( Activity newActivity );

	void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	virtual const char *GetLegsModel( void );
	virtual const char *GetTorsoModel( void );
	virtual const char *GetHeadcrabClassname( void );
	virtual const char *GetHeadcrabModel( void );

	virtual bool OnObstructingDoor( AILocalMoveGoal_t *pMoveGoal, 
								 CBaseDoor *pDoor,
								 float distClear, 
								 AIMoveResult_t *pResult );

	virtual HeadcrabRelease_t ShouldReleaseHeadcrab( const CTakeDamageInfo &info, float flDamageThreshold );
	virtual bool IsChopped( const CTakeDamageInfo &info ) { return false; }
	virtual bool IsSquashed( const CTakeDamageInfo &info ) { return false; }

	Activity SelectDoorBash();

	void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );	
	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	bool IsHeavyDamage( const CTakeDamageInfo &info );
	void BuildScheduleTestBits( void );

	void PrescheduleThink( void );
	int SelectSchedule ( void );

	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void CreateEyeGlows( void );
	void RemoveEyeGlows( float flDelay );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void UpdateOnRemove();

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound();
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void AttackHitSound( void );
	void AttackMissSound( void );
	void FootstepSound( bool fRightFoot );
	void FootscuffSound( bool fRightFoot );

	const char *GetMoanSound( int nSound );
	
public:
	DEFINE_CUSTOM_AI;

	// IASW_Spawnable_NPC implementation
	CHandle<CASW_Base_Spawner> m_hSpawner;
	void SetSpawner(CASW_Base_Spawner* spawner);
	CAI_BaseNPC* GetNPC() { return this; }
	virtual bool CanStartBurrowed() { return false; }
	virtual void StartBurrowed() { }
	virtual void SetUnburrowActivity( string_t iszActivityName ) { }
	virtual void SetUnburrowIdleActivity( string_t iszActivityName ) { }
	virtual void SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject) { }
	AlienOrder_t GetAlienOrders() { return AOT_None; }
	virtual void MoveAside() { }
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL );
	virtual void ElectroStun( float flStunTime ) { }
	virtual void Extinguish();
	CNetworkVar(bool, m_bOnFire);
	virtual void OnSwarmSensed(int iDistance) { }
	virtual void OnSwarmSenseEntity(CBaseEntity* pEnt) { }
	virtual void SetHealthByDifficultyLevel();
	virtual void SetHoldoutAlien() { m_bHoldoutAlien = true; }
	virtual bool IsHoldoutAlien() { return m_bHoldoutAlien; }

protected:
	static const char *pASWMoanSounds[];


private:
	CHandle< CBaseDoor > m_hBlockingDoor;
	float				 m_flDoorBashYaw;
	
	CRandSimTimer 		 m_DurationDoorBash;
	CSimTimer 	  		 m_NextTimeToStartDoorBash;

	Vector				 m_vPositionCharged;
	bool m_bHoldoutAlien;

	CSprite			*m_pEyeGlow[2];
};

#endif // _INCLUDED_ASW_ZOMBIE_H