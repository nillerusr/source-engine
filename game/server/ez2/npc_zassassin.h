//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Gonome. Big scary-ass headcrab zombie, based heavily on the Bullsquid.+
//				+++Repurposed into the Zombie Assassin
//				++Can now Jump and Climb like the fast zombie
//
// 			Originally, the Gonome / Zombie Assassin was created by Sergeant Stacker
// 			and provided to the EZ2 team. 
//			It has been heavily modified for its role as the boss of EZ2 chapter 3. 
//			It now inherits from a base class, CNPC_BasePredator			
// $NoKeywords: $
//=============================================================================//
#ifndef NPC_GONOME_H
#define NPC_GONOME_H

#include "ai_basenpc.h"
#include "npc_basepredator.h"
#include "npc_BaseZombie.h"

typedef CAI_BlendingHost< CNPC_BasePredator > CAI_GonomeBase;

class CNPC_Gonome : public CAI_GonomeBase
{
	DECLARE_CLASS( CNPC_Gonome, CAI_GonomeBase );

public:
	void Spawn( void );
	void Precache( void );
	Class_T	Classify( void );

	void	SetupGlobalModelData( void );

	bool	CreateBehaviors();
	
	virtual const char * GetSoundscriptClassname() { return "Gonome"; }
	void IdleSound( void );
	void AttackSound( void );
	void GrowlSound( void );
	void RetreatModeSound(void);
	void BerserkModeSound(void);
	void EatSound( void );
	void BeginSpawnSound( void );
	void EndSpawnSound( void );

	void HandleAnimEvent( animevent_t *pEvent );

	float GetMaxSpitWaitTime( void );
	float GetMinSpitWaitTime( void );
	float GetWhipDamage( void );
	float GetEatInCombatPercentHealth( void );
	
	void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );

	bool IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
	bool ShouldIgnite( const CTakeDamageInfo &info );

	void OnChangeActivity( Activity eNewActivity );
	void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	bool OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );
	float MaxYawSpeed( void );		// Get max yaw speed

	mutable float	m_flJumpDist;

	void RemoveIgnoredConditions( void );
	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	virtual void OnFed();

	bool IsPrey( CBaseEntity* pTarget ) { return pTarget->Classify() == CLASS_PLAYER_ALLY; }
	bool SpawnNPC( const Vector position );

	virtual bool	QueryHearSound( CSound *pSound );

	int TranslateSchedule( int scheduleType );
	int	SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void BuildScheduleTestBits();
	virtual Activity NPC_TranslateActivity( Activity eNewActivity );

	void StartTask ( const Task_t *pTask );
	void RunTask( const Task_t * pTask );

	// Override to handle player kicks - zassassins are immune
	bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	Disposition_t		IRelationType( CBaseEntity *pTarget );

	virtual bool CanFlinch(void) { return false; } // Gonomes cannot flinch. 

	void PrescheduleThink( void );

	void	InputGoHome( inputdata_t &inputdata );
	void	InputGoHomeInstant( inputdata_t &inputdata );

	float	m_flBurnDamage;				// Keeps track of how much burn damage we've incurred in the last few seconds.
	float	m_flBurnDamageResetTime;	// Time at which we reset the burn damage.

	void Scorch( int rate, int floor ) {}; // The gonome model doesn't look right scorched

	CAI_BeastBehavior	m_BeastBehavior;

private:
	int	m_nGonomeSpitSprite;

	int m_poseArmsOut;

	COutputEvent m_OnBeastHome;
	COutputEvent m_OnBeastLeaveHome;

protected:

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC()

// Boss stuff
public:
	virtual bool	BecomeRagdollOnClient(const Vector &force); // Need to override this to play sound when killed from Hammer - kind of a hackhack
};
#endif // NPC_GONOME_H