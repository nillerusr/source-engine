#ifndef _INCLUDED_ASW_BOOMER_H
#define _INCLUDED_ASW_BOOMER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien.h"
#include "ai_blended_movement.h"
#include "util_shared.h"
#include "ai_speech.h"
#include "asw_ai_behavior_chase_enemy.h"
#include "asw_ai_behavior_melee.h"
#include "asw_ai_behavior_explode.h"

class CASW_Boomer : public CAI_ExpresserHostWithData<CASW_BlendedAlien, CAI_Expresser>
{
public:

	DECLARE_CLASS( CASW_Boomer, CAI_ExpresserHostWithData  );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Boomer();

	virtual void		Spawn();
	virtual void		SetHealthByDifficultyLevel();
	virtual void		Precache();

	virtual float		MaxYawSpeed( void );

	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BOOMER; }

	virtual int			SelectDeadSchedule();
	virtual void		BuildScheduleTestBits();

	virtual bool		CurrentWeaponLOSCondition(const Vector &targetPos, bool bSetConditions) { return true; }
	virtual void		HandleAnimEvent( animevent_t *pEvent );

	virtual int			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void		Event_Killed( const CTakeDamageInfo &info );
	virtual bool		CanDoFancyDeath( void );
	virtual bool		ShouldGib( const CTakeDamageInfo &info ) { return false; }
	virtual bool		CorpseGib( const CTakeDamageInfo &info );
	virtual bool		CanBreak() { return true; };

	// sounds
	virtual void DeathSound( const CTakeDamageInfo &info );

	virtual Activity NPC_TranslateActivity( Activity baseAct );

	// aim target interface
	virtual float GetRadius() { return 34; }

	virtual bool CreateBehaviors();

	CAI_ASW_ExplodeBehavior m_ExplodeBehavior;
	CAI_ASW_ChaseEnemyBehavior m_ChaseEnemyBehavior;
	CAI_ASW_MeleeBehavior m_MeleeBehavior;

	void SetInflating( bool bInflating ) { m_bInflating = bInflating; }

	bool m_bInflating;
	CNetworkVar( bool, m_bInflated );

	CUtlVector<EHANDLE> m_hMarineAttackers;

private:

protected:
	DEFINE_CUSTOM_AI;

};

#endif	// _INCLUDED_ASW_BOOMER_H
