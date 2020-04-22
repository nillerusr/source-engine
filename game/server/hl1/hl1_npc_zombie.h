//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_ZOMBIE_H
#define NPC_ZOMBIE_H


#include	"hl1_ai_basenpc.h"
//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	ZOMBIE_AE_ATTACK_RIGHT		0x01
#define	ZOMBIE_AE_ATTACK_LEFT		0x02
#define	ZOMBIE_AE_ATTACK_BOTH		0x03

#define ZOMBIE_FLINCH_DELAY			2		// at most one flinch every n secs

//=========================================================
//=========================================================
class CNPC_Zombie : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Zombie, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );
	float MaxYawSpeed( void ) { return 120.0f; };
	Class_T Classify( void );
	void HandleAnimEvent( animevent_t *pEvent );
//	int IgnoreConditions ( void );

	float m_flNextFlinch;

	void PainSound( const CTakeDamageInfo &info );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );

	void RemoveIgnoredConditions ( void );
	int MeleeAttack1Conditions ( float flDot, float flDist );
};

#endif //NPC_ZOMBIE_H