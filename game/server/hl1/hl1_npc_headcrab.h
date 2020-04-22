//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_HEADCRAB_H
#define NPC_HEADCRAB_H
#pragma once


#include "hl1_ai_basenpc.h"

class CNPC_Headcrab : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Headcrab, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );

	void RunTask ( const Task_t *pTask );
	void StartTask ( const Task_t *pTask );
	void SetYawSpeed ( void );
	Vector	Center( void );
	Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true );
	
	float	MaxYawSpeed( void );
	Class_T Classify( void );

	void LeapTouch ( CBaseEntity *pOther );
	void BiteSound( void );
	void AttackSound( void );
	void TouchDamage( CBaseEntity *pOther );
	void HandleAnimEvent( animevent_t *pEvent );
	int	 SelectSchedule( void );
	void Touch( CBaseEntity *pOther );
	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	int TranslateSchedule( int scheduleType );
	void PrescheduleThink( void );
	int RangeAttack1Conditions ( float flDot, float flDist );
	float GetDamageAmount( void );
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void IdleSound();
	virtual void AlertSound();

	virtual int		GetVoicePitch( void ) { return 100; }
	virtual float	GetSoundVolume( void ) { return 1.0; }
	
	int	m_nGibCount;

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

protected:
	void HeadCrabSound( const char *pchSound );

	Vector	m_vecJumpVel;
};

#endif //NPC_HEADCRAB_H