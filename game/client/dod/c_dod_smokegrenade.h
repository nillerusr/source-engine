//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef C_DOD_SMOKEGRENADE_H
#define C_DOD_SMOKEGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_dod_basegrenade.h"

class C_DODSmokeGrenade : public C_DODBaseGrenade
{
public:
	DECLARE_CLASS( C_DODSmokeGrenade, C_DODBaseGrenade );
	DECLARE_NETWORKCLASS(); 

	C_DODSmokeGrenade();

	virtual const char *GetOverviewSpriteName( void );

	virtual const char *GetParticleTrailName( void ) { return NULL; }

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink( void );

	float CalcSmokeCloudRadius( void );
	float CalcSmokeCloudAlpha( void );

private:
	float m_flSmokeSpawnTime;	// time the smoke starts emitting
};


#endif // C_DOD_SMOKEGRENADE_H
