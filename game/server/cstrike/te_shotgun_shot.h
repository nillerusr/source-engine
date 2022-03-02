//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TE_SHOTGUN_SHOT_H
#define TE_SHOTGUN_SHOT_H
#ifdef _WIN32
#pragma once
#endif


void TE_FireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread
	);

void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option );


#endif // TE_SHOTGUN_SHOT_H
