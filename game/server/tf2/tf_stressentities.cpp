//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "test_stressentities.h"
#include "tf_flare.h"
#include "tf_shareddefs.h"
#include "plasmaprojectile.h"


// ------------------------------------------------------------------------------------ //
// Functions to create random entities.
// ------------------------------------------------------------------------------------ //

CBaseEntity* CreateSignalFlare()
{
	CBaseEntity *pLocalPlayer = UTIL_GetLocalPlayer();
	if ( pLocalPlayer )
	{
		return CSignalFlare::Create( GetRandomSpot(), QAngle( 0, 0, 0 ), pLocalPlayer, 3 );
	}
	else
	{
		return NULL;
	}
}


CBaseEntity* CreateResourceChunk()
{
	CBaseEntity *pChunk = MoveToRandomSpot( CreateEntityByName( "resource_chunk" ) );
	if ( pChunk )
	{
		pChunk->Spawn();
	}

	return pChunk;
}


CBaseEntity* CreateResourceBox()
{
	CBaseEntity *pRet = MoveToRandomSpot( CreateEntityByName( "obj_box" ) );
	if ( pRet )
	{
		pRet->Spawn();
	}

	return pRet;
}

CBaseEntity* CreatePlasmaProjectile()
{
	CBaseEntity *pLocalPlayer = UTIL_GetLocalPlayer();
	if ( pLocalPlayer )
	{
		Vector vForward;
		vForward.Random(-1,1);
		VectorNormalize( vForward );

		CBasePlasmaProjectile *pRet = CBasePlasmaProjectile::Create( 
			Vector(0,0,0),
			vForward,
			0,
			pLocalPlayer );

		if ( pRet )
			MoveToRandomSpot( pRet );

		return pRet;
	}

	return NULL;
}

CBaseEntity* CreatePlasmaShot()
{
	CBaseEntity *pEnt = MoveToRandomSpot( CreateEntityByName( "base_plasmaprojectile" ) );
	if ( pEnt )
	{
		CBasePlasmaProjectile *pRet = dynamic_cast< CBasePlasmaProjectile* >( pEnt );
		if ( pRet )
		{
			return pRet;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return NULL;
}


REGISTER_STRESS_ENTITY( CreateResourceChunk );
REGISTER_STRESS_ENTITY( CreateResourceBox );
REGISTER_STRESS_ENTITY( CreatePlasmaProjectile );
REGISTER_STRESS_ENTITY( CreatePlasmaShot );
REGISTER_STRESS_ENTITY( CreateSignalFlare );



