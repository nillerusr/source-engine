//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_team.h"
#include "baseentity.h"

// Spawnflags
const int SF_RESPAWNPLAYERS_RESETALL		= 0x01;	// Respawned players have their inventory completely reset
const int SF_RESPAWNPLAYERS_RESETAMMO		= 0x02;	// Respawned players have their ammo counts reset

//-----------------------------------------------------------------------------
// Purpose: Map entity that respawns players
//-----------------------------------------------------------------------------
class CInfoInputRespawnPlayers : public CBaseEntity
{
	DECLARE_CLASS( CInfoInputRespawnPlayers, CBaseEntity );
public:
	DECLARE_DATADESC();

	// Inputs
	void InputRespawnAll( inputdata_t &inputdata );
	void InputRespawnTeam1( inputdata_t &inputdata );
	void InputRespawnTeam2( inputdata_t &inputdata );
	void InputRespawnPlayer( inputdata_t &inputdata );

	// Respawning
	void RespawnPlayer( CBaseTFPlayer *pPlayer );
	void RespawnTeam( CTFTeam *pTeam );
};

BEGIN_DATADESC( CInfoInputRespawnPlayers )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "RespawnAll", InputRespawnAll ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RespawnTeam1", InputRespawnTeam1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RespawnTeam2", InputRespawnTeam2 ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "RespawnPlayer", InputRespawnPlayer ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_input_respawnplayers, CInfoInputRespawnPlayers );

//-----------------------------------------------------------------------------
// Purpose: Respawn all the players
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::InputRespawnAll( inputdata_t &inputdata )
{
	for ( int i = 0; i < MAX_TF_TEAMS; i++ )
	{
		RespawnTeam( GetGlobalTFTeam(i+1) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respawn all of team 1's players
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::InputRespawnTeam1( inputdata_t &inputdata )
{
	RespawnTeam( GetGlobalTFTeam(1) );
}

//-----------------------------------------------------------------------------
// Purpose: Respawn all of team 2's players
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::InputRespawnTeam2( inputdata_t &inputdata )
{
	RespawnTeam( GetGlobalTFTeam(2) );
}

//-----------------------------------------------------------------------------
// Purpose: Respawn a specific player
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::InputRespawnPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		RespawnPlayer( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respawn the team
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::RespawnTeam( CTFTeam *pTeam )
{
	Assert( pTeam );
	if ( !pTeam )
		return;

	// Respawn all the players
	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		RespawnPlayer( (CBaseTFPlayer*)pTeam->GetPlayer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respawn the player
//-----------------------------------------------------------------------------
void CInfoInputRespawnPlayers::RespawnPlayer( CBaseTFPlayer *pPlayer )
{
	// Reset ammo if the spawnflag's set
	if ( HasSpawnFlags( SF_RESPAWNPLAYERS_RESETALL ) )
	{
		pPlayer->ResupplyAmmo( 1.0, RESUPPLY_ALL_FROM_STATION );
	}
	else if ( HasSpawnFlags( SF_RESPAWNPLAYERS_RESETAMMO ) )
	{
		pPlayer->ResupplyAmmo( 1.0, RESUPPLY_RESPAWN );
	}

	pPlayer->ForceRespawn();
}