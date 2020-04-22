//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "mapdata_shared.h"
#include "sharedinterface.h"
#include "baseentity.h"
#include "world.h"
#include "player.h"

class CMapData_Server : public IMapData
{
public:

	// World data queries.
	void	GetMapBounds( Vector &vecMins, Vector &vecMaxs );
	void	GetMapOrigin( Vector &vecOrigin );
	void	GetMapSize( Vector &vecSize );

	// 3D Skybox data queries.
	void	Get3DSkyboxOrigin( Vector &vecOrigin );
	float	Get3DSkyboxScale( void );
};

static CMapData_Server g_MapData;
IMapData *g_pMapData = &g_MapData;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapData_Server::GetMapBounds( Vector &vecMins, Vector &vecMaxs )
{
	CWorld *pWorld = static_cast<CWorld*>( GetWorldEntity() );
	if ( pWorld )
	{
		// Get the world bounds.
		pWorld->GetWorldBounds( vecMins, vecMaxs );

		// Backward compatability...
		if ( ( vecMins.LengthSqr() == 0.0f ) && ( vecMaxs.LengthSqr() == 0.0f ) )
		{
			vecMins.Init( -6500.0f, -6500.0f, -6500.0f );
			vecMaxs.Init( 6500.0f, 6500.0f, 6500.0f );
		}
	}
	else
	{
		Assert( 0 );
		vecMins.Init( 0.0f, 0.0f, 0.0f );
		vecMaxs.Init( 1.0f, 1.0f, 1.0f );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapData_Server::GetMapOrigin( Vector &vecOrigin )
{
	Vector vecMins, vecMaxs;
	GetMapBounds( vecMins, vecMaxs );
	VectorAdd( vecMins, vecMaxs, vecOrigin );
	VectorMultiply( vecOrigin, 0.5f, vecOrigin );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapData_Server::GetMapSize( Vector &vecSize )
{
	Vector vecMins, vecMaxs;
	GetMapBounds( vecMins, vecMaxs );
	VectorSubtract( vecMaxs, vecMins, vecSize );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapData_Server::Get3DSkyboxOrigin( Vector &vecOrigin )
{
	// NOTE: If the player hasn't been created yet -- this doesn't work!!!
	//       We need to pass the data along in the map - requires a tool change.
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		CPlayerLocalData *pLocalData = &pPlayer->m_Local;
		VectorCopy( pLocalData->m_skybox3d.origin, vecOrigin );
	}
	else
	{
		// Debugging!
		Assert( 0 );
		vecOrigin.Init();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CMapData_Server::Get3DSkyboxScale( void )
{
	// NOTE: If the player hasn't been created yet -- this doesn't work!!!
	//       We need to pass the data along in the map - requires a tool change.
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		CPlayerLocalData *pLocalData = &pPlayer->m_Local;
		return pLocalData->m_skybox3d.scale;
	}
	else
	{
		// Debugging!
		Assert( 0 );
		return 1.0f;
	}
}
