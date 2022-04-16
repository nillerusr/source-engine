//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_RIFLEGRENADE_US_H
#define DOD_RIFLEGRENADE_US_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_basegrenade.h"

class CDODRifleGrenadeUS : public CDODBaseGrenade
{
public:
	DECLARE_CLASS( CDODRifleGrenadeUS, CDODBaseGrenade );

	DECLARE_NETWORKCLASS();

	// Overrides.
public:
	CDODRifleGrenadeUS() {}
	virtual void Spawn();
	virtual void Precache();

	// Grenade stuff.
public:

	static CDODRifleGrenadeUS* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner, 
		float timer,
		DODWeaponID weaponID );

	virtual char *GetExplodingClassname();

	virtual float GetElasticity() { return 0.05; }

private:
	float m_flDetonateTime;
};


#endif // DOD_RIFLEGRENADE_US_H
