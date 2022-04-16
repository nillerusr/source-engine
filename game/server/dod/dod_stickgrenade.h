//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_STICKGRENADE_H
#define DOD_STICKGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_basegrenade.h"

class CDODStickGrenade : public CDODBaseGrenade
{
public:
	DECLARE_CLASS( CDODStickGrenade, CDODBaseGrenade );


// Overrides.
public:
	CDODStickGrenade() {}
	virtual void Spawn();
	virtual void Precache();
	virtual void BounceSound( void );

// Grenade stuff.
public:

	static CDODStickGrenade* Create( 
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
	CDODStickGrenade( const CDODStickGrenade & );
};


#endif // DOD_STICKGRENADE_H
