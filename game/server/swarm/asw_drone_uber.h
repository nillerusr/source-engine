#ifndef _INCLUDED_ASW_DRONE_UBER_H
#define _INCLUDED_ASW_DRONE_UBER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_drone_advanced.h"

class CASW_Drone_Uber : public CASW_Drone_Advanced
{
public:
	DECLARE_CLASS( CASW_Drone_Uber, CASW_Drone_Advanced  );	
	DECLARE_DATADESC();

	CASW_Drone_Uber( void );
	virtual ~CASW_Drone_Uber( void );

	virtual void Spawn();
	virtual void Precache();
	virtual void SetHealthByDifficultyLevel();
	virtual float GetIdealSpeed() const;
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual bool ModifyAutoMovement( Vector &vecNewPos );
};

#endif // _INCLUDED_ASW_DRONE_UBER_H
