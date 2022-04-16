//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_SMOKEGRENADE_US_H
#define DOD_SMOKEGRENADE_US_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_smokegrenade.h"

class CDODSmokeGrenadeUS : public CDODSmokeGrenade
{
public:
	DECLARE_CLASS( CDODSmokeGrenadeUS, CDODSmokeGrenade );

	// Overrides.
public:
	CDODSmokeGrenadeUS() {}
	virtual void Spawn();
	virtual void Precache();

	// Grenade stuff.
public:
	static CDODSmokeGrenadeUS* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner );

	virtual DODWeaponID GetEmitterWeaponID() { return WEAPON_SMOKE_US; }
};

#endif // DOD_SMOKEGRENADE_US_H
