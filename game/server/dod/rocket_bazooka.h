//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef ROCKET_BAZOOKA_H
#define ROCKET_BAZOOKA_H
#ifdef _WIN32
#pragma once
#endif

#define BAZOOKA_ROCKET_MODEL	"models/weapons/w_bazooka_rocket.mdl"

#include "dod_baserocket.h"

class CBazookaRocket : public CDODBaseRocket
{
public:
	DECLARE_CLASS( CBazookaRocket, CDODBaseRocket );

	CBazookaRocket() {}

	virtual void Spawn();
	virtual void Precache();

	static CBazookaRocket *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner );

	virtual DODWeaponID GetEmitterWeaponID() { return WEAPON_BAZOOKA; }

private:
	CBazookaRocket( const CBazookaRocket & );
};

#endif //ROCKET_BAZOOKA_H