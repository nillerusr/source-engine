#ifndef _DEFINED_ASW_SIMPLE_DRONE_H
#define _DEFINED_ASW_SIMPLE_DRONE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_simple_alien.h"

class CASW_Simple_Drone : public CASW_Simple_Alien
{
public:
	DECLARE_CLASS( CASW_Simple_Drone, CASW_Simple_Alien  );	
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CASW_Simple_Drone();
	virtual ~CASW_Simple_Drone();

	virtual void Spawn();
	virtual void Precache();

	// anims
	virtual void PlayRunningAnimation();
	virtual void PlayAttackingAnimation();

	// sounds
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound();
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AttackSound();
};


#endif // _DEFINED_ASW_SIMPLE_DRONE_H
