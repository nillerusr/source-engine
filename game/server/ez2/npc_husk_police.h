//=============================================================================//
//
// Purpose:		Metrocop husks.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_POLICE_H
#define NPC_HUSK_POLICE_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_metropolice.h"
#include "npc_husk_base.h"

//=========================================================
//	>> CNPC_HuskPolice
//=========================================================
class CNPC_HuskPolice : public CAI_BaseHusk<CNPC_MetroPolice>
{
	DECLARE_CLASS( CNPC_HuskPolice, CAI_BaseHusk<CNPC_MetroPolice> );
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_SERVERCLASS();

public:
	CNPC_HuskPolice();

	// From CAI_HuskSink
	const char	*GetBaseNPCClassname() override { return "npc_metropolice"; }

	void		Spawn( void );
	void		Precache( void );
	bool		GetGameTextSpeechParams( hudtextparms_t &params );

	void		HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void		SuspiciousSound() { SpeakIfAllowed( TLK_HUSK_SUSPICIOUS, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_ALWAYS ); }
	void		StartledSound() { SpeakIfAllowed( TLK_HUSK_SUSPICIOUS_TO_ANGRY, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL ); }

	NPC_STATE	SelectIdealState( void );
	int			SelectAlertSchedule( void );
	int			SelectCombatSchedule( void );

	bool		HasHumanGibs( void ) { return true; }

	WeaponProficiency_t		CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskAggressionLevel );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskCognizanceFlags );

	DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		COND_HUSK_POLICE_SHOULD_PATROL = BaseClass::NEXT_CONDITION,

		//SCHED_HUSK_POLICE_ = BaseClass::NEXT_SCHEDULE,

		//TASK_HUSK_POLICE_ = BaseClass::NEXT_TASK,
	};
};

#endif // NPC_HUSK_POLICE_H
