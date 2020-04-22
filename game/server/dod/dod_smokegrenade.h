//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_SMOKEGRENADE_H
#define DOD_SMOKEGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_basegrenade.h" 

class CDODSmokeGrenade : public CDODBaseGrenade
{
public:
	DECLARE_CLASS( CDODSmokeGrenade, CDODBaseGrenade );
	DECLARE_DATADESC();

	DECLARE_SERVERCLASS();

	virtual void Spawn();
	virtual void Precache();
	virtual void BounceSound( void ); 
	virtual void Detonate();

	virtual bool CanBePickedUp( void ) { return false; }

	void Think_Emit();
	void Think_Fade();
	void Think_Remove();

private:
	bool m_bFading;
	bool m_bInitialSmoke;
	float m_flRemoveTime;

	CNetworkVar( float, m_flSmokeSpawnTime );
};

#endif // DOD_SMOKEGRENADE_H
