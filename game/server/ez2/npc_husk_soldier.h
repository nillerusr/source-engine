//=============================================================================//
//
// Purpose:		Combine husks.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_SOLDIER_H
#define NPC_HUSK_SOLDIER_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"
#include "npc_husk_base.h"

enum HuskSoldierType_t
{
	HUSK_DEFAULT,	// Default chooses based on model

	HUSK_SOLDIER,
	HUSK_ELITE,

	HUSK_GRUNT,
	HUSK_ORDINAL,
	HUSK_CHARGER,
	HUSK_SUPPRESSOR,

	NUM_HUSK_SOLDIER_TYPES,
};

void HuskSoldier_AscertainModelType( CBaseAnimating *pAnimating, const model_t *pModel, HuskSoldierType_t &tType );

//=========================================================
//	>> CNPC_HuskSoldier
//=========================================================
class CNPC_HuskSoldier : public CAI_BaseHusk<CNPC_Combine>
{
	DECLARE_CLASS( CNPC_HuskSoldier, CAI_BaseHusk<CNPC_Combine> );
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_SERVERCLASS();

public:
	CNPC_HuskSoldier();

	// From CAI_HuskSink
	const char	*GetBaseNPCClassname() override { return "npc_combine_s"; }

	void		Spawn( void );
	void		Precache( void );
	void		ModifyOrAppendCriteria( AI_CriteriaSet &set );
	bool		GetGameTextSpeechParams( hudtextparms_t &params );
	const char	*GetEZVariantString();

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void		DeathSound( const CTakeDamageInfo &info );
	void		BashSound();

	void		SuspiciousSound() { SpeakIfAllowed( TLK_HUSK_SUSPICIOUS, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_ALWAYS ); }
	void		StartledSound() { SpeakIfAllowed( TLK_HUSK_SUSPICIOUS_TO_ANGRY, SENTENCE_PRIORITY_HIGH, SENTENCE_CRITERIA_NORMAL ); }

	float		GetHostileRadius();

	bool		IsCommandable();
	float		ShouldDelayEscalation();
	void		PickupItem( CBaseEntity *pItem );
	bool		ShouldAllowPlayerGive() { return !IsAngry() && m_iCanPlayerGive != TRS_FALSE; }
	bool		IsGiveableWeapon( CBaseCombatWeapon *pWeapon );
	bool		IsGiveableItem( CBaseEntity *pItem );
	void		StartPlayerGive( CBasePlayer *pPlayer );
	void		OnCantBeGivenObject( CBaseEntity *pItem );
	const char	*GetBackupWeaponClass();

	bool		RemovePassiveTarget( CBaseEntity *pTarget );

	void		PrescheduleThink( void );
	void		BuildScheduleTestBits( void );
	int			SelectSchedule ( void );
	int			SelectCombatSchedule();

	float		GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		Event_Killed( const CTakeDamageInfo &info );
	void		TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	int			TestEyeShot( const Vector &vecShotPos );

	bool		IsLightDamage( const CTakeDamageInfo &info );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );

	bool		AllowedToIgnite( void ) { return true; }
	bool		HasHumanGibs( void ) { return true; }

	bool		Crouch( void );

	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired = NULL );
	Activity	NPC_TranslateActivity( Activity eNewActivity );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskAggressionLevel );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskCognizanceFlags );

	DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_HUSK_SOLDIER_ = BaseClass::NEXT_CONDITION,

		SCHED_HUSK_SOLDIER_REJECT_GIFT = BaseClass::NEXT_SCHEDULE,

		//TASK_HUSK_SOLDIER_ = BaseClass::NEXT_TASK,
	};

	HuskSoldierType_t	m_tHuskSoldierType;

	bool			m_bNoDynamicDamage;

	COutputEvent	m_OnGiftAccept;
	COutputEvent	m_OnGiftReject;
};

#endif // NPC_HUSK_SOLDIER_H
