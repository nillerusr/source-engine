//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Definitions of all the entities that control logic flow within a map
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "EntityInput.h"
#include "EntityOutput.h"
#include "tf_team.h"
#include "tf_obj.h"


//-----------------------------------------------------------------------------
// Purpose: Detects a bunch of tf team state
//-----------------------------------------------------------------------------
class CSensorTFTeam : public CLogicalEntity
{
	DECLARE_CLASS( CSensorTFTeam, CLogicalEntity );

public:
	void Spawn( void );
	void Think( void );

private:
	DECLARE_DATADESC();

	// Computes the number of respawns stations on the sensed team
	int ComputeRespawnCount();

	// outputs
	COutputInt m_OnRespawnCountChanged;
	COutputInt m_OnResourceCountChanged;
	COutputInt m_OnMemberCountChanged;
	COutputInt m_OnRespawnCountChangedDelta;
	COutputInt m_OnResourceCountChangedDelta;
	COutputInt m_OnMemberCountChangedDelta;

	// What team am I sensing?
	int	m_nTeam;
	CTFTeam *m_pTeam;

	// So we can know when state changes...
	int m_nRespawnCount;
	int m_nResourceCount;
	int m_nMemberCount;
};


LINK_ENTITY_TO_CLASS( sensor_tf_team, CSensorTFTeam );


BEGIN_DATADESC( CSensorTFTeam )

	DEFINE_OUTPUT( m_OnRespawnCountChanged, "OnRespawnCountChanged" ),
	DEFINE_OUTPUT( m_OnResourceCountChanged, "OnResourceCountChanged" ),
	DEFINE_OUTPUT( m_OnMemberCountChanged, "OnMemberCountChanged" ),
	DEFINE_OUTPUT( m_OnRespawnCountChangedDelta, "OnRespawnCountChangedDelta" ),
	DEFINE_OUTPUT( m_OnResourceCountChangedDelta, "OnResourceCountChangedDelta" ),
	DEFINE_OUTPUT( m_OnMemberCountChangedDelta, "OnMemberCountChangedDelta" ),
	DEFINE_KEYFIELD( m_nTeam, FIELD_INTEGER, "team"),

END_DATADESC()




//-----------------------------------------------------------------------------
// Spawn!
//-----------------------------------------------------------------------------
void CSensorTFTeam::Spawn( void )
{
	// Hook us up to a team...
	m_pTeam = GetGlobalTFTeam( m_nTeam );

	// Gets us thinkin!
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Force an output message on our first think
	m_nRespawnCount = -1;
	m_nResourceCount = -1;
}


//-----------------------------------------------------------------------------
// Compute the number of respawn stations on this team
//-----------------------------------------------------------------------------
int CSensorTFTeam::ComputeRespawnCount()
{
	int nCount = 0;
	for (int i = m_pTeam->GetNumObjects(); --i >= 0; )
	{
		CBaseObject *pObject = m_pTeam->GetObject(i);
		if ( pObject && (pObject->GetType() == OBJ_RESPAWN_STATION) )
		{
			++nCount;
		}
	}
	return nCount;
}


//-----------------------------------------------------------------------------
// Purpose: Forces a recompare
//-----------------------------------------------------------------------------
void CSensorTFTeam::Think( )
{
	if (!m_pTeam)
		return;

	// Check for a difference in the number of respawn stations
	int nRespawnCount = ComputeRespawnCount();
	if ( nRespawnCount != m_nRespawnCount )
	{
		m_OnRespawnCountChangedDelta.Set( nRespawnCount - m_nRespawnCount, this, this );
		m_nRespawnCount = nRespawnCount;
		m_OnRespawnCountChanged.Set( m_nRespawnCount, this, this );
	}

	// Check for a difference in the number of resources harvested
	if ( m_nResourceCount != m_pTeam->m_flTotalResourcesSoFar )
	{
		m_OnResourceCountChangedDelta.Set( m_pTeam->m_flTotalResourcesSoFar - m_nResourceCount, this, this );
		m_nResourceCount = m_pTeam->m_flTotalResourcesSoFar;
		m_OnResourceCountChanged.Set( m_nResourceCount, this, this );
	}

	// Check for a difference in the number of team members
	if ( m_nMemberCount != m_pTeam->GetNumPlayers() )
	{
		m_OnMemberCountChangedDelta.Set( m_pTeam->GetNumPlayers() - m_nMemberCount, this, this );
		m_nMemberCount = m_pTeam->GetNumPlayers();
		m_OnMemberCountChanged.Set( m_nMemberCount, this, this );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

