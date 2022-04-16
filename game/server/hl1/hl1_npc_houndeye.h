//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_HOUNDEYE_H
#define NPC_HOUNDEYE_H
#pragma once

#include "hl1_ai_basenpc.h"
#define	HOUNDEYE_MAX_ATTACK_RADIUS		384

class CNPC_Houndeye : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Houndeye, CHL1BaseNPC );
	
public:
	void Spawn( void );
	void Precache( void );

	void Event_Killed( const CTakeDamageInfo &info );

	void WarmUpSound ( void );
	void AlertSound( void );
	void DeathSound( const CTakeDamageInfo &info );
	void WarnSound( void );
	void PainSound( const CTakeDamageInfo &info );
	void IdleSound( void );
	
	float MaxYawSpeed  ( void );

	Class_T	Classify ( void );
	
	void HandleAnimEvent( animevent_t *pEvent );

	void SonicAttack ( void );

	Vector WriteBeamColor( void );
	bool ShouldGoToIdleState( void );
	bool FValidateHintType ( CAI_Hint *pHint );
	
	void SetActivity ( Activity NewActivity );

	void StartTask( const Task_t *pTask );
	void RunTask ( const Task_t *pTask );
	void PrescheduleThink ( void );

	int TranslateSchedule( int scheduleType );
	int SelectSchedule( void );

	float FLSoundVolume( CSound *pSound );
	int RangeAttack1Conditions ( float flDot, float flDist );

	void StartNPC ( void );

	virtual float	InnateRange1MinRange( void ) { return 0.0f; }
	virtual float	InnateRange1MaxRange( void ) { return HOUNDEYE_MAX_ATTACK_RADIUS; }

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

private:
	int SquadRecruit( int searchRadius, int maxMembers );

	int m_iSpriteTexture;
	bool m_fAsleep;// some houndeyes sleep in idle mode if this is set, the houndeye is lying down
	bool m_fDontBlink;// don't try to open/close eye if this bit is set!
	Vector	m_vecPackCenter; // the center of the pack. The leader maintains this by averaging the origins of all pack members.
};

#endif // NPC_HOUNDEYE_H