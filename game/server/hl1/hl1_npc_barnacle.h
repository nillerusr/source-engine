//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base combat character with no AI
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	HL1_NPC_BARNACLE_H
#define	HL1_NPC_BARNACLE_H

#include "hl1_ai_basenpc.h"
#include "rope_physics.h"

#define	BARNACLE_BODY_HEIGHT	44 // how 'tall' the barnacle's model is.
#define BARNACLE_PULL_SPEED			8
#define BARNACLE_KILL_VICTIM_DELAY	5 // how many seconds after pulling prey in to gib them. 

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	BARNACLE_AE_PUKEGIB	2


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_Barnacle : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Barnacle, CHL1BaseNPC );

public:
	void			Spawn( void );
	void			InitTonguePosition( void );
	void			Precache( void );
	CBaseEntity*	TongueTouchEnt ( float *pflLength );
	Class_T			Classify ( void );
	void			HandleAnimEvent( animevent_t *pEvent );
	void			BarnacleThink ( void );
	void			WaitTillDead ( void );
	void			Event_Killed( const CTakeDamageInfo &info );
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );

	DECLARE_DATADESC();

	float			m_flAltitude;

	float			m_flKillVictimTime;
	int				m_cGibs;// barnacle loads up on gibs each time it kills something.
	bool			m_fLiftingPrey;
	float			m_flTongueAdj;
	float			m_flIgnoreTouchesUntil;

public:
	DEFINE_CUSTOM_AI;
};

#endif	//HL1_NPC_BARNACLE_H
