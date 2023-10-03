//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================
#include "asw_spawn_selection.h"
#include "filesystem.h"
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_nSpawnDefIDs = 0;

const char *g_SpawnTypes[] =
{
	"ANY",
	"FIXED",
	"WANDERER",
	"HORDE",
	"BOSS",
	"PROP",
	"SPECIAL1",
	"SPECIAL2"
};


int SpawnTypeFromString( const char *szText )
{
	int nSpawnTypeCount = ( ( sizeof( g_SpawnTypes ) ) / sizeof( g_SpawnTypes[0] ) );
	for ( int i = 0 ; i < nSpawnTypeCount; i++ )
	{
		if ( !V_stricmp( szText, g_SpawnTypes[i] ) )
			return i;
	}

	return ASW_NPC_SPAWN_TYPE_INVALID;
}


// copies a string
char* ASW_AllocString( const char *szString )
{
	if ( !szString )
		return NULL;

	int len = Q_strlen( szString ) + 1;
	if ( len <= 1 )
		return NULL;

	char *text = new char[ len ];
	Q_strncpy( text, szString, len );
	return text;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Definition::CASW_Spawn_Definition( int nID )
{
	m_flSelectionWeight = 1.0f;
	m_nID = nID;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Definition::~CASW_Spawn_Definition()
{
	m_Entries.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Definition::LoadFromKeyValues( KeyValues *pKeys )
{
	m_flSelectionWeight = pKeys->GetFloat( "SelectionWeight" );
	for ( KeyValues *pKey = pKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		if ( !Q_stricmp( pKey->GetName(), "NPC" ) )
		{
			CASW_Entry *pEntry = new CASW_Entry();
			pEntry->m_nMinPerSpawn = pKey->GetInt( "MinPerSpawn", 1 );
			pEntry->m_nMaxPerSpawn = pKey->GetInt( "MaxPerSpawn", 1 );
			pEntry->m_pszAlienClass = ASW_AllocString( pKey->GetString( "AlienClass" ) );
			pEntry->m_bUseSpawners = pKey->GetBool( "UseSpawners" );
			m_Entries.AddToTail( pEntry );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CASW_Spawn_Definition::ContainsAlienClass( const char *szAlienClass )
{
	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		if ( !Q_stricmp( m_Entries[i]->m_pszAlienClass, szAlienClass ) )
			return true;
	}
	return false;
}

void CASW_Spawn_Definition::ListSpawnDef()
{
	Msg( "SpawnDef %d\n", GetID() );
	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		Msg( "  Entry %d class = %s min = %d max = %d\n", i, m_Entries[i]->m_pszAlienClass, m_Entries[i]->m_nMinPerSpawn, m_Entries[i]->m_nMaxPerSpawn );
	}
}

// ==================================================================

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Set::CASW_Spawn_Set()
{
	for ( int i = 0; i < ASW_NPC_SPAWN_TYPE_COUNT; i++ )
	{
		m_flTotalWeight[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Set::~CASW_Spawn_Set()
{
	for ( int t = 0; t < ASW_NPC_SPAWN_TYPE_COUNT; t++ )
	{
		m_SpawnDefs[t].PurgeAndDeleteElements();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Definition* CASW_Spawn_Set::GetSpawnDef( int nType )
{
	int nCount = m_SpawnDefs[nType].Count();
	float flChosen = RandomFloat( 0, m_flTotalWeight[nType] );
	for ( int i = 0; i < nCount; i++ )
	{
		flChosen -= m_SpawnDefs[nType][i]->GetSelectionWeight();
		if ( flChosen <= 0 )
		{
			return m_SpawnDefs[nType][i];
		}
	}
	return NULL;
}

CASW_Spawn_Definition* CASW_Spawn_Set::GetSpawnDefByID( int nID )
{
	for ( int t = 0; t < ASW_NPC_SPAWN_TYPE_COUNT; t++ )
	{
		for ( int i = 0; i < m_SpawnDefs[t].Count(); i++ )
		{
			if ( m_SpawnDefs[t][i]->GetID() == nID )
				return m_SpawnDefs[t][i];
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Set::LoadFromKeyValues( KeyValues *pKeys )
{
	m_iMinDifficulty = pKeys->GetInt( "MinDifficulty" );
	m_iMaxDifficulty = pKeys->GetInt( "MaxDifficulty" );
	m_flFeederChance = pKeys->GetFloat( "FeederChance" );
	m_iMinEncounters = pKeys->GetInt( "MinEncounters" );
	m_iMaxEncounters = pKeys->GetInt( "MaxEncounters" );
	m_iMinSpawnsPerEncounter = pKeys->GetInt( "MinSpawnsPerEncounter" );
	m_iMaxSpawnsPerEncounter = pKeys->GetInt( "MaxSpawnsPerEncounter" );
	m_pszSetName = ASW_AllocString( pKeys->GetString( "Name", "Unnamed" ) );
	//Msg( "Loaded spawn set %s mindiff=%d maxdiff=%d\n", m_pszSetName, m_iMinDifficulty, m_iMaxDifficulty );

	for ( KeyValues *pKey = pKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		int nType = SpawnTypeFromString( pKey->GetName() );
		if ( nType != ASW_NPC_SPAWN_TYPE_INVALID )
		{
			CASW_Spawn_Definition *pSpawnDef = new CASW_Spawn_Definition( g_nSpawnDefIDs++ );
			pSpawnDef->LoadFromKeyValues( pKey );
			m_SpawnDefs[nType].AddToTail( pSpawnDef );
			m_flTotalWeight[nType] += pSpawnDef->GetSelectionWeight();
		}
	}
}


// ==================================================================
extern CASW_Spawn_Selection g_SpawnSelection;
CASW_Spawn_Set* CurrentSpawnSet()
{
	return g_SpawnSelection.GetCurrentSpawnSet();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Selection::CASW_Spawn_Selection()
{
	m_pSpawnSetKeys = NULL;
	m_pCurrentSpawnSet = NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Selection::~CASW_Spawn_Selection()
{
	Reset();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Selection::Init()
{
	Reset();
	LoadSpawnSets();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Selection::SetCurrentSpawnSet( int iMissionDifficulty )
{
	DevMsg( "[AS] Setting spawn set from mission difficulty %d\n", iMissionDifficulty );

	// look through our spawn sets to find any that match the difficulty
	CUtlVector<CASW_Spawn_Set*> candidates;
	for ( int i = 0; i < m_SpawnSets.Count(); i++ )
	{
		if ( iMissionDifficulty >= m_SpawnSets[i]->GetMinDifficulty() && iMissionDifficulty <= m_SpawnSets[i]->GetMaxDifficulty() )
		{
			candidates.AddToTail( m_SpawnSets[i] );
		}
	}
	if ( candidates.Count() <= 0 )
	{
		Warning( "Failed to SetCurrentSpawnSet by difficulty as couldn't find a default SpawnSelection block which covers this difficulty range.  Requested difficulty=%d.  Supported difficulties:\n", iMissionDifficulty );
		for ( int i = 0; i < m_SpawnSets.Count(); i++ )
		{
			Msg( "%s Min=%d Max=%d\n", m_SpawnSets[i]->GetSetName(), m_SpawnSets[i]->GetMinDifficulty(), m_SpawnSets[i]->GetMaxDifficulty() );
		}
		return;
	}

	m_pCurrentSpawnSet = candidates[ RandomInt( 0, candidates.Count() - 1 ) ];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CASW_Spawn_Selection::SetCurrentSpawnSet( const char *szSetName )
{
	m_pCurrentSpawnSet = GetSpawnSet( szSetName );

	return m_pCurrentSpawnSet != NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Definition* CASW_Spawn_Selection::GetSpawnDefByID( int nID )
{
	CASW_Spawn_Definition *pDef = NULL;
	for ( int i = 0; i < m_SpawnSets.Count(); i++ )
	{
		pDef = m_SpawnSets[i]->GetSpawnDefByID( nID );
		if ( pDef )
			return pDef;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Selection::DumpCurrentSpawnSet()
{
	if ( !m_pCurrentSpawnSet )
	{
		Msg( "No current spawn set!\n");
		return;
	}
	Msg( "Current spawn set: %s  Feeder chance: %f\n", m_pCurrentSpawnSet->GetSetName(), m_pCurrentSpawnSet->GetFeederChance() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_Spawn_Set* CASW_Spawn_Selection::GetSpawnSet( const char *szSetName )
{
	for ( int i = 0; i < m_SpawnSets.Count(); i++ )
	{
		if ( !Q_stricmp( m_SpawnSets[i]->GetSetName(), szSetName ) )
		{
			return m_SpawnSets[i];
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Selection::LoadSpawnSets()
{
	g_nSpawnDefIDs = 0;
	m_pSpawnSetKeys = new KeyValues( "DefaultSpawnSets" );
	//Msg( "LoadSpawnSets\n" );
	if ( !m_pSpawnSetKeys->LoadFromFile( g_pFullFileSystem, "resource/alien_selection.txt", "GAME" ) )
	{
		Warning( "Error loading resource/alien_selections.txt\n" );
	}
	for ( KeyValues *pKey = m_pSpawnSetKeys; pKey; pKey = pKey->GetNextKey() )
	{
		if ( !Q_stricmp( pKey->GetName(), "SpawnSet" ) )
		{
			CASW_Spawn_Set *pSet = new CASW_Spawn_Set();
			pSet->LoadFromKeyValues( pKey );
			if ( !m_pCurrentSpawnSet )
			{
				m_pCurrentSpawnSet = pSet;
			}
			m_SpawnSets.AddToTail( pSet );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Spawn_Selection::Reset()
{
	m_SpawnSets.PurgeAndDeleteElements();
	if ( m_pSpawnSetKeys )
	{
		m_pSpawnSetKeys->deleteThis();
		m_pSpawnSetKeys = NULL;
	}	
	m_pCurrentSpawnSet = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szName - 
// Output : Returns true on success
//-----------------------------------------------------------------------------
bool CASW_Spawn_Selection::IsAvailableNPC( const char *szName )
{
	return true;
}


