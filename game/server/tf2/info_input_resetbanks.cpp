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
// Purpose: Map entity that resets player's banks
//-----------------------------------------------------------------------------
class CInfoInputResetBanks : public CBaseEntity
{
	DECLARE_CLASS( CInfoInputResetBanks, CBaseEntity );
public:
	DECLARE_DATADESC();

	// Inputs
	void InputResetAll( inputdata_t &inputdata );
	void InputResetTeam1( inputdata_t &inputdata );
	void InputResetTeam2( inputdata_t &inputdata );
	void InputResetPlayer( inputdata_t &inputdata );
	void InputSetResetAmount( inputdata_t &inputdata );

	// Resetting
	void ResetPlayersBank( CBaseTFPlayer *pPlayer );
	void ResetTeamsBanks( CTFTeam *pTeam );

private:
	int		m_iResetAmount;
};

BEGIN_DATADESC( CInfoInputResetBanks )

	// variables
	DEFINE_KEYFIELD( m_iResetAmount, FIELD_INTEGER, "ResetAmount" ),

	// inputs
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetResetAmount", InputSetResetAmount ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetAll", InputResetAll ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetTeam1", InputResetTeam1 ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetTeam2", InputResetTeam2 ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "ResetPlayer", InputResetPlayer ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_input_resetbanks, CInfoInputResetBanks );

//-----------------------------------------------------------------------------
// Purpose: Reset all the player's resource banks
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::InputResetAll( inputdata_t &inputdata )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
		if ( pPlayer )
		{
			ResetPlayersBank( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset all of team 1's player's resource banks
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::InputResetTeam1( inputdata_t &inputdata )
{
	ResetTeamsBanks( GetGlobalTFTeam(1) );
}

//-----------------------------------------------------------------------------
// Purpose: Reset all of team 2's player's resource banks
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::InputResetTeam2( inputdata_t &inputdata )
{
	ResetTeamsBanks( GetGlobalTFTeam(2) );
}

//-----------------------------------------------------------------------------
// Purpose: Reset a specific player's resource banks
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::InputResetPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		ResetPlayersBank( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the reset amount
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::InputSetResetAmount( inputdata_t &inputdata )
{
	m_iResetAmount = inputdata.value.Int();
	Assert( m_iResetAmount >= 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Reset the team's resource banks
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::ResetTeamsBanks( CTFTeam *pTeam )
{
	pTeam->SetRecentBankSet( m_iResetAmount );
	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		ResetPlayersBank( (CBaseTFPlayer*)pTeam->GetPlayer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset the player's resource bank
//-----------------------------------------------------------------------------
void CInfoInputResetBanks::ResetPlayersBank( CBaseTFPlayer *pPlayer )
{
	pPlayer->SetBankResources( m_iResetAmount );
}
