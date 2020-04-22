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
// Purpose: Map entity that fires its output with all the players in a team
//-----------------------------------------------------------------------------
class CInfoOutputTeam : public CBaseEntity
{
	DECLARE_CLASS( CInfoOutputTeam, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn( void );

	// Inputs
	void InputFire( inputdata_t &inputdata );

public:
	// Outputs
	COutputEHANDLE		m_Player;
};

BEGIN_DATADESC( CInfoOutputTeam )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Fire", InputFire ),

	// outputs
	DEFINE_OUTPUT( m_Player, "Player" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_output_team, CInfoOutputTeam );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoOutputTeam::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoOutputTeam::InputFire( inputdata_t &inputdata )
{
	// Loop through all the players on the team and fire our output with each of them.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
		if ( pPlayer )
		{
			// If we don't belong to a team, loop through all players
			if ( GetTeamNumber() == 0 || pPlayer->GetTeamNumber() == GetTeamNumber() )
			{
				EHANDLE hHandle;
				hHandle = pPlayer;
				m_Player.Set( hHandle, inputdata.pActivator, this ); 
			}
		}
	}
}