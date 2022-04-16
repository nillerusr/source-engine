//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_ICHTHYOSAUR_H
#define NPC_ICHTHYOSAUR_H


#include "hl1_ai_basenpc.h"

#define SEARCH_RETRY	16

#define ICHTHYOSAUR_SPEED 150

#define EYE_MAD		0
#define EYE_BASE	1
#define EYE_CLOSED	2
#define EYE_BACK	3
#define EYE_LOOK	4


//
// CNPC_Ichthyosaur
//

class CNPC_Ichthyosaur : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Ichthyosaur, CHL1BaseNPC );
public:

	void	Precache( void );
	void	Spawn( void );
	Class_T Classify ( void );
	void	NPCThink ( void );
	void	Swim ( void );
	void	StartTask(const Task_t *pTask);
	void	RunTask( const Task_t *pTask );
	int		RangeAttack1Conditions( float flDot, float flDist );
	int		MeleeAttack1Conditions ( float flDot, float flDist );
	void	BiteTouch( CBaseEntity *pOther );
	void	HandleAnimEvent( animevent_t *pEvent );
	int		TranslateSchedule( int scheduleType );
	int		SelectSchedule();
	virtual	bool FVisible ( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );

	Vector  DoProbe( const Vector &Probe );
	bool    ProbeZ( const Vector &position, const Vector &probe, float *pFraction);

	float	GetGroundSpeed ( void );

	bool	OverrideMove( float flInterval );
	void	MoveExecute_Alive(float flInterval);

	void InputStartCombat( inputdata_t &input );
	void InputEndCombat( inputdata_t &input );

	virtual void	IdleSound( void );
	virtual void	AlertSound( void );
	virtual void	DeathSound( const CTakeDamageInfo &info );
	virtual void	PainSound( const CTakeDamageInfo &info );

	void	AttackSound( void );
	void	BiteSound( void );

	virtual void GatherEnemyConditions( CBaseEntity *pEnemy );

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

private:
	Vector m_SaveVelocity;
	float m_idealDist;

	float m_flBlink;

	float m_flEnemyTouched;
	bool  m_bOnAttack;

	float m_flMaxSpeed;
	float m_flMinSpeed;
	float m_flMaxDist;

	float m_flNextAlert;
	float m_flLastAttackSound;

	//Save the info from that run
	Vector m_vecLastMoveTarget;
	bool m_bHasMoveTarget;

	float m_flFlyingSpeed;	
};


#endif //NPC_ICHTHYOSAUR_H
