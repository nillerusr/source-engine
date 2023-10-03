//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ASW_ENTITYFLAME_H
#define ASW_ENTITYFLAME_H
#ifdef _WIN32
#pragma once
#endif

#include "EntityFlame.h"

#define FLAME_DAMAGE_INTERVAL			0.2f // How often to deal damage.
#define FLAME_DIRECT_DAMAGE_PER_SEC		5.0f
#define FLAME_RADIUS_DAMAGE_PER_SEC		4.0f

#define FLAME_DIRECT_DAMAGE ( FLAME_DIRECT_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL )
#define FLAME_RADIUS_DAMAGE ( FLAME_RADIUS_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL )

#define FLAME_MAX_LIFETIME_ON_DEAD_NPCS	10.0f

class CASW_EntityFlame : public CEntityFlame 
{
	DECLARE_CLASS( CASW_EntityFlame, CEntityFlame );	
public:
	CASW_EntityFlame( void );
	virtual void Spawn();

	DECLARE_DATADESC();

protected:
	void ASWFlameThink( void );

	float m_fDamageInterval;
	float m_flDamagePerInterval;
};

#endif // ASW_ENTITYFLAME_H
