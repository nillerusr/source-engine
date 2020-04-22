//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team spawnpoint entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_TEAMSPAWNPOINT_H
#define TF_TEAMSPAWNPOINT_H
#pragma once

#include "baseentity.h"
#include "EntityOutput.h"

class CTeam;

//-----------------------------------------------------------------------------
// Purpose: points at which the player can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS( CTeamSpawnPoint, CPointEntity );

	void	Activate( void );
	bool	IsValid( CBasePlayer *pPlayer );

	COutputEvent m_OnPlayerSpawn;

protected:	
	int		m_iDisabled;

	void	EnableSpawn( void );
	void	DisableSpawn( void );

	DECLARE_DATADESC();
};


#endif // TF_TEAMSPAWNPOINT_H
