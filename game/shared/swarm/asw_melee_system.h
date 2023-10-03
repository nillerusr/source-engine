#ifndef _INCLUDED_ASW_MELEE_SYSTEM_H
#define _INCLUDED_ASW_MELEE_SYSTEM_H

#include "asw_marine_profile.h"
#include "UtlSortVector.h"

class CASW_Player;
class CASW_Marine;
class CMoveData;

DECLARE_LOGGING_CHANNEL( LOG_ASW_Melee );
#define ASW_MEL_MSG(string, ...) ASW_MSG( LOG_ASW_Melee, ##string, ##__VA_ARGS__ )
#define ASW_MEL_MSG_SIMPLE(string, ...) ASW_MSG_SIMPLE( LOG_ASW_Melee, ##string, ##__VA_ARGS__ )
#define ASW_MEL_WAR(string, ...) ASW_WAR( LOG_ASW_Melee, ##string, ##__VA_ARGS__ )

enum ASW_ControlDirection_t
{
	ASW_CD_ANY,
	ASW_CD_NONE,
	ASW_CD_FORWARD,
	ASW_CD_BACK,
	ASW_CD_LEFT,
	ASW_CD_RIGHT,	
};

enum ASW_Melee_Attack_t
{
	ASW_MA_LIGHT,
	ASW_MA_HEAVY,
	ASW_MA_CHARGE,
	ASW_MA_CHARGE_RELEASE
};

class CASW_Melee_Attack
{
public:
	CASW_Melee_Attack();
	virtual ~CASW_Melee_Attack();

	void ApplyKeyValues( KeyValues *pKeys );

#ifdef CLIENT_DLL
	bool CheckContact( CASW_Marine *pAttacker );			// checks if this melee attack would hit something
#endif
	bool AllowNormalAnimEvent( CASW_Marine *pMarine, int event );
	void MovementCollision( CASW_Marine *pMarine, CMoveData *pMoveData, trace_t *tr );
	bool CanComboFrom( CASW_Melee_Attack *pAttack );

	int m_nAttackID;
	const char *m_szAttackName;
	const char *m_szSequenceName;
	float m_flPriority;
	ASW_Melee_Movement_t m_iAllowMovement;
	bool m_bAllowRotation;

	int m_iMinMeleeSkill;
	int m_iMaxMeleeSkill;
	float m_flDamageScale;
	float m_flForceScale;
	float m_flTraceDistance;
	float m_flTraceHullSize;
	float m_flRequiredChargeTime;
	float m_flSpeedScale;
	float m_flKnockbackForce;
	float m_flBlendIn;
	float m_flBlendOut;

	bool m_bHoldAtEnd;

	bool m_bAllowHitsBehindMarine;

	Vector m_vTraceAttackOffset;

	CUtlVector<CASW_Melee_Attack *>	m_CombosFromAttacks;

	CASW_Melee_Attack *m_pOnCollisionDoAttack;
	CASW_Melee_Attack *m_pForceComboAttack;
	
	CUtlVector<const char *> m_CombosFromAttackNames;
	
	const char *m_szOnCollisionDoAttackName;
	const char *m_szForceComboAttackName;
	ASW_ControlDirection_t m_ControlDirection;
	const char *m_szActiveWeapon;
	ASW_Marine_Class m_MarineClass;
	ASW_Melee_Attack_t m_AttackType;
	bool m_bIsLooping;
};

class CASW_Melee_Attack_LessFunc
{
public:
	bool Less( CASW_Melee_Attack * const & lhs, CASW_Melee_Attack * const & rhs, void *pContext )
	{
		return lhs->m_flPriority < rhs->m_flPriority;
	}
};

class CASW_Melee_System
{
public:
	CASW_Melee_System();
	virtual ~CASW_Melee_System();

	void ProcessMovement( CASW_Marine *pMarine, CMoveData *pMoveData );
	void OnMeleePressed( CASW_Marine *pMarine, CMoveData *pMoveData );
	void UpdateCandidateMeleeAttacks( CASW_Marine *pMarine, CMoveData *pMoveData );
	void StartMeleeAttack( CASW_Melee_Attack *pAttack, CASW_Marine *pMarine, CMoveData *pMoveData, float flBaseMeleeDamage = -1 );
	void OnJumpPressed( CASW_Marine *pMarine, CMoveData *pMoveData );
	
	void SetupMeleeMovement( CASW_Marine *pMarine, CMoveData *pMoveData );
	void OnMeleeAttackFinished( CASW_Marine *pMarine );
	bool ComboTransition( CASW_Marine *pMarine, CMoveData *pMoveData, bool bUpdateCandidateAttacks = true );

	void LoadMeleeAttacks();
	void ValidateMeleeAttacks( CASW_Marine *pMarine );
	void LinkUpCombos();
	void Reload();

	void DoMeleeDamage( CASW_Marine *pMarine, float flYawStart, float flYawEnd );

#ifdef CLIENT_DLL
	void FindMeleeLockTarget( CASW_Marine *pMarine );
	void CreateMove( float flInputSampleTime, CUserCmd *pCmd, CASW_Marine *pMarine );
#endif

	void ComputeTraceOffset( CASW_Marine *pMarine, Vector &vecTraceOffset );

	CASW_Melee_Attack* GetMeleeAttackByName( const char *szAttackName );
	CASW_Melee_Attack* GetMeleeAttackByID( int iMeleeID );
	CUtlVector<CASW_Melee_Attack*> m_MeleeAttacks;
	CUtlSortVector< CASW_Melee_Attack *, CASW_Melee_Attack_LessFunc >	m_candidateMeleeAttacks;

	bool m_bAllowNormalAnimEvents;
	bool m_bAttacksValidated;

	static int s_nRollAttackID;
	static int s_nKnockdownForwardAttackID;
	static int s_nKnockdownBackwardAttackID;
};

CASW_Melee_System *ASWMeleeSystem();

#endif // _INCLUDED_ASW_MELEE_SYSTEM_H