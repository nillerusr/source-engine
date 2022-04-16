//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_VORTIGAUNT_H
#define NPC_VORTIGAUNT_H

#define	VORTIGAUNT_MAX_BEAMS	8

#include "hl1_ai_basenpc.h"
//=========================================================
//=========================================================
class CNPC_Vortigaunt : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Vortigaunt, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );
	Class_T	Classify ( void );

	void AlertSound( void );
	void IdleSound( void );
	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );
	
	int	 GetSoundInterests ( void );
	
	float MaxYawSpeed ( void );

	void Event_Killed( const CTakeDamageInfo &info );
	void CallForHelp( char *szClassname, float flDist, CBaseEntity * pEnemy, Vector &vecLocation );

	int  RangeAttack1Conditions( float flDot, float flDist );

	int  OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	
	void StartTask( const Task_t *pTask );

	int  SelectSchedule( void );
	int  TranslateSchedule( int scheduleType );

	void ArmBeam( int side );
	void BeamGlow( void );
	void WackBeam( int side, CBaseEntity *pEntity );
	void ZapBeam( int side );
	void ClearBeams( void );

	void HandleAnimEvent( animevent_t *pEvent );

	virtual Disposition_t IRelationType ( CBaseEntity *pTarget );

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

private:
	int m_iVoicePitch;
	int	  m_iBeams;

	int m_iBravery;

	CBeam *m_pBeam[VORTIGAUNT_MAX_BEAMS];

	float m_flNextAttack;

	EHANDLE m_hDead;
};


#endif //NPC_VORTIGAUNT_