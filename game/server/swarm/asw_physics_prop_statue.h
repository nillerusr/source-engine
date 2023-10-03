//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ASW_PHYSICS_PROP_STATUE_H
#define ASW_PHYSICS_PROP_STATUE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "physics_prop_statue.h"

//-----------------------------------------------------------------------------
// Purpose: entity class for simple ragdoll physics
//-----------------------------------------------------------------------------

class CASWStatueProp : public CStatueProp
{
	DECLARE_CLASS( CASWStatueProp, CStatueProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	CASWStatueProp( void );

	virtual void	Spawn( void );
	virtual void	Precache();

	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	Event_Killed( const CTakeDamageInfo &info );

	virtual void	Freeze( float flFreezeAmount = -1.0f, CBaseEntity *pFreezer = NULL, Ray_t *pFreezeRay = NULL );

	void EnableAutoShatter( const CTakeDamageInfo &dmgInfo );
	void AutoShatterThink();
	void BreakThink();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_STATUE; }

	const char *m_pszSourceClass;

	CTakeDamageInfo m_ShatterDamageInfo;
};


CBaseEntity *CreateASWServerStatue( CBaseAnimating *pAnimating, int collisionGroup, const CTakeDamageInfo &dmgInfo, bool bAutoShatter );
CBaseEntity *CreateASWServerStatueFromOBBs( const CUtlVector<outer_collision_obb_t> &vecSphereOrigins, CBaseAnimating *pChildEntity );


#endif // ASW_PHYSICS_PROP_STATUE_H
