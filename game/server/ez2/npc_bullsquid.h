//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base combat character with no AI
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_BULLSQUID_H
#define NPC_BULLSQUID_H

#include "ai_basenpc.h"
#include "npc_basepredator.h"
#include "npc_egg.h"

class CNPC_Bullsquid : public CNPC_BasePredator
{
	DECLARE_CLASS( CNPC_Bullsquid, CNPC_BasePredator );
	DECLARE_DATADESC();

public:
	void Spawn( void );
	void Precache( void );
	Class_T	Classify( void );
	
	virtual const char * GetSoundscriptClassname() { return m_bIsBaby ? "NPC_Babysquid" : "NPC_Bullsquid"; }

	float MaxYawSpeed ( void );

	int RangeAttack1Conditions( float flDot, float flDist );
	int MeleeAttack1Conditions( float flDot, float flDist );

	void HandleAnimEvent( animevent_t *pEvent );

	float GetMaxSpitWaitTime( void );
	float GetMinSpitWaitTime( void );

	float GetWhipDamage( void );
	float GetBiteDamage( void );
	float GetEatInCombatPercentHealth( void );

	// Antlion worker styled spit attack
	virtual bool GetSpitVector( const Vector &vecStartPos, const Vector &vecTarget, Vector *vecOut );

	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	virtual CBaseEntity * BiteAttack( float flDist, const Vector &mins, const Vector &maxs );

	bool IsPrey( CBaseEntity* pTarget ) { return pTarget->Classify() == CLASS_HEADCRAB || pTarget->Classify() == CLASS_EARTH_FAUNA  || pTarget->Classify() == CLASS_ALIEN_FAUNA; }
	virtual bool ShouldInfight( CBaseEntity * pTarget ); // Could this target npc be a rival I need to kill?

	void RunAI ( void );

	void StartTask ( const Task_t *pTask );

	int				SelectSchedule( void );
	int 			TranslateSchedule( int scheduleType );

	virtual Activity NPC_TranslateActivity( Activity eNewActivity );

	bool		ShouldGib( const CTakeDamageInfo &info );
	bool		CorpseGib( const CTakeDamageInfo &info );
	void		ExplosionEffect( void );

	bool SpawnNPC( const Vector position, const QAngle angle );
	CNPC_Egg * SpawnEgg( const Vector position, const QAngle angle );
	CNPC_Bullsquid * SpawnLive( const Vector position, bool isBaby );

	DEFINE_CUSTOM_AI;

private:	
	int   m_nSquidSpitSprite;

	// Consider moving these to BasePredator if other predators need them
	string_t		m_EggModelName;
	string_t		m_BabyModelName;
	string_t		m_AdultModelName;


	// Antlion worker styled spit attack
	Vector	m_vecSaveSpitVelocity;	// Saved when we start to attack and used if we failed to get a clear shot once we release
};
#endif // NPC_BULLSQUID_H