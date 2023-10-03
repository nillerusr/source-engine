#ifndef _DEFINED_ASW_DRONE_ANTLION_H
#define _DEFINED_ASW_DRONE_ANTLION_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_antlion.h"

// a test ASW drone, based on the antlion

class CASW_Drone_Antlion : public CNPC_Antlion
{
public:

	DECLARE_CLASS( CASW_Drone_Antlion, CNPC_Antlion  );
	//DECLARE_SERVERCLASS();
	//DECLARE_PREDICTABLE();
	DECLARE_DATADESC();
	CASW_Drone_Antlion( void );

	void Spawn();
	void Precache();

	virtual int			MeleeAttack2Conditions( float flDot, float flDist );
	bool FInViewCone( const Vector &vecSpot );
	bool ShouldGib( const CTakeDamageInfo &info );
	virtual float GetIdealSpeed() const;
	virtual float MaxYawSpeed( void );	
	virtual float GetSequenceGroundSpeed( int iSequence );
	virtual void NPCThink();

	virtual bool ShouldPlayerAvoid( void );
};


#endif // _DEFINED_ASW_DRONE_ANTLION_H
