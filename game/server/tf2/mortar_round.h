//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MORTAR_ROUND_H
#define MORTAR_ROUND_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_vehicle_mortar.h"
#include "smoke_trail.h"


class CMortarRound : public CBaseAnimating
{
public:
	DECLARE_CLASS( CMortarRound, CBaseAnimating );

	CMortarRound();

	DECLARE_DATADESC();

public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void MissileTouch( CBaseEntity *pOther );
	virtual void FallThink( void );
	virtual void SetLauncher( CVehicleMortar *pLauncher );
	virtual void SetFallTime( float flFallTime );
	virtual void SetRoundType( int iRoundType );

	// Damage type accessors
	virtual int GetDamageType() const { return DMG_BLAST; }

	static CMortarRound* CMortarRound::Create( const Vector &vecOrigin, const Vector &vecVelocity, edict_t *pentOwner );

	SmokeTrail				*m_pSmokeTrail;
	CHandle<CVehicleMortar>	m_pLauncher;
	int						m_iRoundType;
};


#endif // MORTAR_ROUND_H
