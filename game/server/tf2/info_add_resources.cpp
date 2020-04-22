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
#include "tf_stats.h"

//-----------------------------------------------------------------------------
// Purpose: Map entity that gives resources to players passed into it
//-----------------------------------------------------------------------------
class CInfoAddResources : public CBaseEntity
{
	DECLARE_CLASS( CInfoAddResources, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn( void );

	// Inputs
	void InputPlayer( inputdata_t &inputdata );

public:
	// Outputs
	COutputEvent	m_OnAdded;

	// Data
	int		m_iResourceAmount;
};

BEGIN_DATADESC( CInfoAddResources )

	// inputs
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "Player", InputPlayer ),

	// outputs
	DEFINE_OUTPUT( m_OnAdded, "OnAdded" ),

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iResourceAmount, FIELD_INTEGER, "ResourceAmount" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_add_resources, CInfoAddResources );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoAddResources::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoAddResources::InputPlayer( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = (inputdata.value.Entity()).Get();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
		pPlayer->AddBankResources( m_iResourceAmount );
		TFStats()->IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_RESOURCES_ACQUIRED, m_iResourceAmount );
	}

	m_OnAdded.FireOutput( inputdata.pActivator, this );
}
