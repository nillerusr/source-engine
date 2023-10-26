#ifndef	_INCLUDED_ASW_COLONIST_H
#define	_INCLUDED_ASW_COLONIST_H
#ifdef _WIN32
#pragma once
#endif
#include "ai_playerally.h"
#include "asw_shareddefs.h"

class CASW_Marine;
class CASW_Alien;

class CASW_Colonist : public CAI_PlayerAlly
{
	DECLARE_CLASS( CASW_Colonist, CAI_PlayerAlly );
public:
	CASW_Colonist();
	virtual ~CASW_Colonist();

	void			Precache();	
	void			Spawn();
	
	Class_T 		Classify() { return (Class_T) CLASS_ASW_COLONIST; }
	Activity		NPC_TranslateActivity( Activity eNewActivity );
	int 			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void MeleeBleed(CTakeDamageInfo* info);
	bool			IsPlayerAlly( CBasePlayer *pPlayer = NULL );
	void DeathSound( const CTakeDamageInfo &info );
	void TaskFail( AI_TaskFailureCode_t code );

	void NPCThink();
	void ASWThinkEffects();
	float m_fLastASWThink;

	// healing
	void AddSlowHeal(int iHealAmount, CASW_Marine *pMedic);
	bool m_bSlowHeal;
	int m_iSlowHealAmount;
	float m_fNextSlowHealTick;

	// infestation
	bool Event_Gibbed( const CTakeDamageInfo &info );
	bool ShouldGib( const CTakeDamageInfo &info );
	void BecomeInfested(CASW_Alien* pAlien);
	void CureInfestation(CASW_Marine *pHealer, float fCureFraction);
	bool IsInfested() { return m_bInfested; }
	bool IsHeavyDamage( const CTakeDamageInfo &info );
	virtual bool HasHumanGibs() { return true; }
	float m_fInfestedTime;	// how much time left on the infestation
	int m_iInfestCycle;
	bool m_bInfested;
	CHandle<CASW_Marine> m_hInfestationCurer;	// the last medic to cure us of some infestation
	virtual int SelectSchedule();
	virtual int SelectFlinchSchedule_ASW();
	Activity GetFlinchActivity( bool bHeavyDamage, bool bGesture );

	bool					m_bNotifyNavFailBlocked;
	COutputEvent		m_OnNavFailBlocked;

private:
	DECLARE_DATADESC();
#ifdef _XBOX
protected:
#endif
	DEFINE_CUSTOM_AI;
};

//---------------------
#endif	// _INCLUDED_ASW_COLONIST_H
