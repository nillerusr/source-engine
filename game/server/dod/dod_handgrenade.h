//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_HANDGRENADE_H
#define DOD_HANDGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_basegrenade.h"

class CDODHandGrenade : public CDODBaseGrenade
{
public:
	DECLARE_CLASS( CDODHandGrenade, CDODBaseGrenade );

// Overrides.
public:
	CDODHandGrenade() {}
	virtual void Spawn();
	virtual void Precache();
	virtual void BounceSound( void );

// Grenade stuff.
public:

	static CDODHandGrenade* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner, 
		float timer,
		DODWeaponID weaponID );

	void SetTimer( float timer );

	virtual char *GetExplodingClassname();

private:
	float m_flDetonateTime;
};


#endif // DOD_HANDGRENADE_H
