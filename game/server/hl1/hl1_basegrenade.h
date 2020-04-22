//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL1_BASEGRENADE_H
#define HL1_BASEGRENADE_H
#ifdef _WIN32
#pragma once
#endif


#include "basegrenade_shared.h"


class CHL1BaseGrenade : public CBaseGrenade
{
	DECLARE_CLASS( CHL1BaseGrenade, CBaseGrenade );
public:

	virtual void Precache();

	void Explode( trace_t *pTrace, int bitsDamageType );
	unsigned int	PhysicsSolidMaskForEntity( void ) const;
};

class CHandGrenade : public CHL1BaseGrenade
{
public:
	DECLARE_CLASS( CHandGrenade, CHL1BaseGrenade );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Precache( void );
	void	BounceSound( void );
	void	BounceTouch( CBaseEntity *pOther );

	void ShootTimed( CBaseCombatCharacter *pOwner, Vector vecVelocity, float flTime );
};

#endif // HL1_BASEGRENADE_H
