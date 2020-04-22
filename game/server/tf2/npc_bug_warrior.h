//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_BUG_WARRIOR_H
#define NPC_BUG_WARRIOR_H
#ifdef _WIN32
#pragma once
#endif

#include "AI_BaseNPC.h"

// Animation events
#define BUG_WARRIOR_AE_MELEE_SOUND1			11	// Start attack sound
#define BUG_WARRIOR_AE_MELEE_HIT1			15	// Melee hit, one arm

// Attack range definitions
#define	BUG_WARRIOR_MELEE1_RANGE			128.0f
#define	BUG_WARRIOR_MELEE1_RANGE_MIN		128.0f

#define	BUG_WARRIOR_MODEL		"models/npcs/bugs/bug_warrior.mdl"

class CMaker_BugHole;

//-----------------------------------------------------------------------------
// Purpose: WARRIOR BUG
//-----------------------------------------------------------------------------
class CNPC_Bug_Warrior : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Bug_Warrior, CAI_BaseNPC );
public:
	CNPC_Bug_Warrior( void );

	virtual void Spawn( void );
	virtual void Precache( void );

	virtual int SelectSchedule( void );
	virtual int	TranslateSchedule( int scheduleType );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual bool FValidateHintType(CAI_Hint *pHint);

	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual void IdleSound( void );
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound( void );
	virtual bool HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sender );

	virtual bool ShouldPlayIdleSound( void );

	virtual int	MeleeAttack1Conditions( float flDot, float flDist );
	virtual int	OnTakeDamage_Alive( const CTakeDamageInfo &info );

	virtual Class_T	Classify( void ) { return CLASS_ANTLION; }

	virtual float MaxYawSpeed ( void );
	virtual	float CalcIdealYaw( const Vector &vecTarget );

	DECLARE_DATADESC();

	// Returns true if the bug should flee
	bool	ShouldFlee( void );

	// Squad handling
	bool	IsAlone( void );

	// BugHole handling
	void	SetBugHole( CMaker_BugHole *pBugHole );
	void	ReturnToBugHole( void );
	void	Assist( CAI_BaseNPC *pBug );

	// Patrolling
	bool	StartPatrolling( string_t iszPatrolPathName );
	bool	IsPatrolling( void );

private:
	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );
	virtual int				IRelationPriority( CBaseEntity *pTarget );

	void	Event_Killed( const CTakeDamageInfo &info );
	void	MeleeAttack( float distance, float damage, QAngle& viewPunch, Vector& shove );

	float	m_flIdleDelay;

	// BugHole handling
	CHandle< CMaker_BugHole >	m_hMyBugHole;
	CHandle< CAI_BaseNPC >		m_hAssistTarget;

	// Patrolling
	string_t	m_iszPatrolPathName;
	int			m_iPatrolPoint;

	// Bug's decided he's not fleeing anymore
	bool		m_bFightingToDeath;

	DEFINE_CUSTOM_AI;
};

#endif // NPC_BUG_WARRIOR_H
