//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_RIFLEGRENADE_GER_H
#define DOD_RIFLEGRENADE_GER_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_basegrenade.h"

class CDODRifleGrenadeGER : public CDODBaseGrenade
{
public:
	DECLARE_CLASS( CDODRifleGrenadeGER, CDODBaseGrenade );

	DECLARE_SERVERCLASS();

	// Overrides 
public:
	CDODRifleGrenadeGER() {}
	virtual void Spawn();
	virtual void Precache();

	// Grenade stuff.
public:

	static CDODRifleGrenadeGER* Create( 
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


#endif // DOD_RIFLEGRENADE_GER_H
