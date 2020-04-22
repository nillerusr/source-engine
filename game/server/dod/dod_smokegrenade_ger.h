//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_SMOKEGRENADE_GER_H
#define DOD_SMOKEGRENADE_GER_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_smokegrenade.h"

class CDODSmokeGrenadeGER : public CDODSmokeGrenade
{
public:
	DECLARE_CLASS( CDODSmokeGrenadeGER, CDODSmokeGrenade );

	// Overrides.
public:
	CDODSmokeGrenadeGER() {}
	virtual void Spawn();
	virtual void Precache();

	// Grenade stuff.
public:
	static CDODSmokeGrenadeGER* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner );

	virtual DODWeaponID GetEmitterWeaponID() { return WEAPON_SMOKE_GER; }
};

#endif // DOD_SMOKEGRENADE_GER_H
