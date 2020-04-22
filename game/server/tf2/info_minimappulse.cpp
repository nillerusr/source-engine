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
// Purpose: Map entity that makes a pulse on the minimap
//-----------------------------------------------------------------------------
class CInfoMinimapPulse : public CBaseEntity
{
	DECLARE_CLASS( CInfoMinimapPulse, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn( void );

	// Inputs
	void InputPulseForAll( inputdata_t &inputdata );
	void InputPulseForTeam1( inputdata_t &inputdata );
	void InputPulseForTeam2( inputdata_t &inputdata );
	void InputPulseForPlayer( inputdata_t &inputdata );

	// Pulsing
	void PulseForPlayer( CBaseTFPlayer *pPlayer );
	void PulseForTeam( CTFTeam *pTeam );
};

BEGIN_DATADESC( CInfoMinimapPulse )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "PulseForAll", InputPulseForAll ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PulseForTeam1", InputPulseForTeam1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PulseForTeam2", InputPulseForTeam2 ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "PulseForPlayer", InputPulseForPlayer ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_minimappulse, CInfoMinimapPulse );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Pulse for all the players
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::InputPulseForAll( inputdata_t &inputdata )
{
	for ( int i = 0; i < MAX_TF_TEAMS; i++ )
	{
		PulseForTeam( GetGlobalTFTeam(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pulse for all of team 1's players
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::InputPulseForTeam1( inputdata_t &inputdata )
{
	PulseForTeam( GetGlobalTFTeam(1) );
}

//-----------------------------------------------------------------------------
// Purpose: Pulse for all of team 2's players
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::InputPulseForTeam2( inputdata_t &inputdata )
{
	PulseForTeam( GetGlobalTFTeam(2) );
}

//-----------------------------------------------------------------------------
// Purpose: Pulse for a specific player
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::InputPulseForPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		PulseForPlayer( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pulse for the team
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::PulseForTeam( CTFTeam *pTeam )
{
	// Pulse all the players
	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		PulseForPlayer( (CBaseTFPlayer*)pTeam->GetPlayer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a minimap pulse for the player
//-----------------------------------------------------------------------------
void CInfoMinimapPulse::PulseForPlayer( CBaseTFPlayer *pPlayer )
{
	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();
	UserMessageBegin( user, "MinimapPulse" );
		WRITE_VEC3COORD( GetAbsOrigin() );
	MessageEnd();
}