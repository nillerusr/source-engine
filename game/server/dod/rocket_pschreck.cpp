//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "rocket_pschreck.h"

LINK_ENTITY_TO_CLASS( rocket_pschreck, CPschreckRocket );
PRECACHE_WEAPON_REGISTER( rocket_pschreck );

void CPschreckRocket::Spawn( void )
{
	SetModel( PSCHRECK_ROCKET_MODEL );

	BaseClass::Spawn();
}

void CPschreckRocket::Precache( void )
{
	PrecacheModel( PSCHRECK_ROCKET_MODEL );

	BaseClass::Precache();
}

CPschreckRocket *CPschreckRocket::Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner )
{
	return static_cast<CPschreckRocket *> ( CDODBaseRocket::Create( "rocket_pschreck", vecOrigin, vecAngles, pOwner ) );
}