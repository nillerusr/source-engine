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

//-----------------------------------------------------------------------------
// Purpose: Map entity that resets player's objects
//-----------------------------------------------------------------------------
class CInfoInputResetObjects : public CBaseEntity
{
	DECLARE_CLASS( CInfoInputResetObjects, CBaseEntity );
public:
	DECLARE_DATADESC();

	// Inputs
	void InputResetAll( inputdata_t &inputdata );
	void InputResetTeam1( inputdata_t &inputdata );
	void InputResetTeam2( inputdata_t &inputdata );
	void InputResetPlayer( inputdata_t &inputdata );

	// Resetting
	void ResetPlayersObjects( CBaseTFPlayer *pPlayer );
	void ResetTeamsObjects( CTFTeam *pTeam );
};

BEGIN_DATADESC( CInfoInputResetObjects )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetAll", InputResetAll ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetTeam1", InputResetTeam1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetTeam2", InputResetTeam2 ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "ResetPlayer", InputResetPlayer ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_input_resetobjects, CInfoInputResetObjects );

//-----------------------------------------------------------------------------
// Purpose: Reset all the player's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::InputResetAll( inputdata_t &inputdata )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
		if ( pPlayer )
		{
			ResetPlayersObjects( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset all of team 1's player's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::InputResetTeam1( inputdata_t &inputdata )
{
	ResetTeamsObjects( GetGlobalTFTeam(1) );
}

//-----------------------------------------------------------------------------
// Purpose: Reset all of team 2's player's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::InputResetTeam2( inputdata_t &inputdata )
{
	ResetTeamsObjects( GetGlobalTFTeam(2) );
}

//-----------------------------------------------------------------------------
// Purpose: Reset a specific player's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::InputResetPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		ResetPlayersObjects( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset the team's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::ResetTeamsObjects( CTFTeam *pTeam )
{
	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		ResetPlayersObjects( (CBaseTFPlayer*)pTeam->GetPlayer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset the player's objects
//-----------------------------------------------------------------------------
void CInfoInputResetObjects::ResetPlayersObjects( CBaseTFPlayer *pPlayer )
{
	pPlayer->RemoveAllObjects( true, 0, true );
}