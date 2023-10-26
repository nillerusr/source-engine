#ifndef _INCLUDED_ASW_RANGER_H
#define _INCLUDED_ASW_RANGER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"
#include "ai_blended_movement.h"
#include "util_shared.h"
#include "ai_speech.h"
#include "asw_ai_behavior_combat_stun.h"
#include "asw_ai_behavior_flinch.h"
#include "asw_ai_behavior_prepare_to_engage.h"
#include "asw_ai_behavior_retreat.h"
#include "asw_ai_behavior_ranged_attack.h"
#include "asw_ai_behavior_chase_enemy.h"
#include "asw_ai_behavior_idle.h"

class CASW_Ranger : public CAI_ExpresserHostWithData<CASW_BlendedAlien, CAI_Expresser>
{
public:

	DECLARE_CLASS( CASW_Ranger, CAI_ExpresserHostWithData  );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Ranger();

	virtual void		Spawn();
	virtual void		Precache();

	virtual void		SetHealthByDifficultyLevel();

	virtual float		MaxYawSpeed( void );

	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_RANGER; }

	virtual int			SelectDeadSchedule();
	virtual void		BuildScheduleTestBits();

	virtual bool		CurrentWeaponLOSCondition(const Vector &targetPos, bool bSetConditions) { return true; }
	virtual void		HandleAnimEvent( animevent_t *pEvent );

	virtual bool		CreateBehaviors();
	virtual bool		ShouldGib( const CTakeDamageInfo &info ) { return false; }
	virtual void		Event_Killed( const CTakeDamageInfo &info );
	virtual bool		CorpseGib( const CTakeDamageInfo &info );
	virtual void		SetupRangerShot( CASW_AlienShot &shot );

	// sounds
	virtual void DeathSound( const CTakeDamageInfo &info );

private:
	CAI_ASW_CombatStunBehavior		m_CombatStunBehavior;
	CAI_ASW_FlinchBehavior			m_FlinchBehavior;
	//CAI_ASW_PrepareToEngageBehavior m_PrepareToEngageBehavior;
	CAI_ASW_RetreatBehavior			m_RetreatBehavior;
	CAI_ASW_RangedAttackBehavior	m_RangedAttackBehavior;
	CAI_ASW_ChaseEnemyBehavior		m_ChaseEnemyBehavior;
	CAI_ASW_IdleBehavior			m_IdleBehavior;

protected:
	DEFINE_CUSTOM_AI;

};

#endif	//_INCLUDED_ASW_RANGER_H
