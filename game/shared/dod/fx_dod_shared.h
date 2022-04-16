//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FX_DOD_SHARED_H
#define FX_DOD_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#ifdef CLIENT_DLL
	#include "c_dod_player.h"
#else
	#include "dod_player.h"
#endif

#include "dod_weapon_parse.h"


// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayer,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float flSpread
	);


#ifndef CLIENT_DLL

	void TE_DODExplosion( IRecipientFilter &filter, float flDelay, const Vector &vecOrigin, const Vector &vecNormal );

#endif


#endif // FX_DOD_SHARED_H
