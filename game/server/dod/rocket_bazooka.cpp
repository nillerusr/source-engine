//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "rocket_bazooka.h"

LINK_ENTITY_TO_CLASS( rocket_bazooka, CBazookaRocket );
PRECACHE_WEAPON_REGISTER( rocket_bazooka );

void CBazookaRocket::Spawn( void )
{
	SetModel( BAZOOKA_ROCKET_MODEL );

	BaseClass::Spawn();
}

void CBazookaRocket::Precache( void )
{
	PrecacheModel( BAZOOKA_ROCKET_MODEL );

	BaseClass::Precache();
}

CBazookaRocket *CBazookaRocket::Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner )
{
	return static_cast<CBazookaRocket *> ( CDODBaseRocket::Create( "rocket_bazooka", vecOrigin, vecAngles, pOwner ) );
}