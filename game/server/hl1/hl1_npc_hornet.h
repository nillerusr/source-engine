//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_HORNET_H
#define NPC_HORNET_H

#include "hl1_ai_basenpc.h"

//=========================================================
// Hornets
//=========================================================

//=========================================================
// Hornet Defines
//=========================================================
#define HORNET_TYPE_RED			0
#define HORNET_TYPE_ORANGE		1
#define HORNET_RED_SPEED		(float)600
#define HORNET_ORANGE_SPEED		(float)800

extern int iHornetPuff;

//=========================================================
// Hornet - this is the projectile that the Alien Grunt fires.
//=========================================================
class CNPC_Hornet : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Hornet, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );
	Class_T	 Classify ( void );
	Disposition_t		IRelationType(CBaseEntity *pTarget);

	void DieTouch ( CBaseEntity *pOther );
	void DartTouch( CBaseEntity *pOther );
	void TrackTouch ( CBaseEntity *pOther );
	void TrackTarget ( void );
	void StartDart ( void );
	void IgniteTrail( void );
	void StartTrack(void);


	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;
	virtual bool ShouldGib( const CTakeDamageInfo &info ) { return false; }
	
	/*	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	
	void EXPORT StartTrack ( void );
	
	void EXPORT TrackTarget ( void );
	void EXPORT TrackTouch ( CBaseEntity *pOther );
	void EXPORT DartTouch( CBaseEntity *pOther );
	
	
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );*/

	float			m_flStopAttack;
	int				m_iHornetType;
	float			m_flFlySpeed;
	int				m_flDamage;

	DECLARE_DATADESC();

	Vector			m_vecEnemyLKP;
};

#endif //NPC_HORNET_H