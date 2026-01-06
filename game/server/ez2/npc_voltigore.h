//=============================================================================//
//
// Purpose: Large Race X predator that fires unstable portals
//
// Author: 1upD
//
//=============================================================================//

#ifndef NPC_VOLTIGORE_H
#define NPC_VOLTIGORE_H

#include "ai_basenpc.h"
#include "npc_basepredator.h"

class CNPC_Voltigore : public CNPC_BasePredator
{
	DECLARE_CLASS( CNPC_Voltigore, CNPC_BasePredator );
	DECLARE_DATADESC();

public:
	CNPC_Voltigore() { SetDisplacementImpossible( true ); }

	void Spawn( void );
	void Precache( void );
	Class_T	Classify( void );
	
	void IdleSound( void );
	void PainSound( const CTakeDamageInfo &info );
	void AlertSound( void );
	void DeathSound( const CTakeDamageInfo &info );
	void FoundEnemySound( void );
	void AttackSound( void );
	void GrowlSound( void );
	void BiteSound( void );
	void EatSound( void );

	virtual void BuildScheduleTestBits();

	float MaxYawSpeed ( void );

	int RangeAttack1Conditions( float flDot, float flDist );

	virtual Activity NPC_TranslateActivity( Activity eNewActivity );
	void HandleAnimEvent( animevent_t *pEvent );

	// Projectile methods
	virtual float GetProjectileDamge();
	virtual float GetBiteDamage( void );
	virtual float GetWhipDamage( void );

	// bool ShouldGib( const CTakeDamageInfo &info );


	bool IsPrey( CBaseEntity* pTarget ) { return pTarget->Classify() == CLASS_PLAYER_ALLY; }

	virtual float GetMaxSpitWaitTime( void ) { return 2.0f; };
	virtual float GetMinSpitWaitTime( void ) { return 3.0f; };

	DEFINE_CUSTOM_AI;

private:	
	float m_nextSoundTime;
};
#endif // NPC_SPINETHROWINGPREDATOR_H