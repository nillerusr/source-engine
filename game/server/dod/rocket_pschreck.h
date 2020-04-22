//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#ifndef ROCKET_PSCHRECK_H
#define ROCKET_PSCHRECK_H
#ifdef _WIN32
#pragma once
#endif

#define PSCHRECK_ROCKET_MODEL	"models/weapons/w_panzerschreck_rocket.mdl"

#include "dod_baserocket.h"

class CPschreckRocket : public CDODBaseRocket
{
public:
	DECLARE_CLASS( CPschreckRocket, CDODBaseRocket );

	CPschreckRocket() {}

	virtual void Spawn();
	virtual void Precache();

	static CPschreckRocket *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner );

	virtual DODWeaponID GetEmitterWeaponID() { return WEAPON_PSCHRECK; }

private:
	CPschreckRocket( const CPschreckRocket & );
};

#endif //ROCKET_PSCHRECK_H