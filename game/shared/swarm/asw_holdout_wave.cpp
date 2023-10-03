#include "cbase.h"
#include "asw_shareddefs.h"
#include "asw_holdout_wave.h"
#include "gamestringpool.h"
#ifdef GAME_DLL
#include "asw_spawn_group.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_holdout_debug;

//===============================================
//  An individual entry in a holdout wave
//===============================================
CASW_Holdout_Wave_Entry::CASW_Holdout_Wave_Entry()
{

}

CASW_Holdout_Wave_Entry::~CASW_Holdout_Wave_Entry()
{

}

void CASW_Holdout_Wave_Entry::LoadFromKeyValues( KeyValues *pKeys )
{
	m_flSpawnDelay = pKeys->GetFloat( "SpawnDelay", 1.0f );
	m_iszAlienClass = AllocPooledString( pKeys->GetString( "AlienClass" ) );
	m_nQuantity = pKeys->GetInt( "Quantity", 1 );
	m_flSpawnDuration = pKeys->GetFloat( "SpawnDuration", 0.0f );
	m_nModifiers = pKeys->GetInt( "Modifiers", 0 );	// TODO: Turn this into a string parser for the bit flag names
	m_iszSpawnGroupName = AllocPooledString( pKeys->GetString( "SpawnGroup" ) );
	m_hSpawnGroup = NULL;
}

#ifdef GAME_DLL
CASW_Spawn_Group* CASW_Holdout_Wave_Entry::GetSpawnGroup()
{
	if ( m_hSpawnGroup.Get() )
	{
		return m_hSpawnGroup.Get();
	}

	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_iszSpawnGroupName );
	if ( pEnt && pEnt->Classify() == CLASS_ASW_SPAWN_GROUP )
	{
		m_hSpawnGroup = assert_cast<CASW_Spawn_Group*>( pEnt );
		return m_hSpawnGroup.Get();
	}
	Warning( "Holdout wave entry can't find spawngroup %s\n", STRING( m_iszSpawnGroupName ) );
	return NULL;
}
#endif

//===============================================
//  A wave in holdout mode
//===============================================
CASW_Holdout_Wave::CASW_Holdout_Wave()
{
	m_nTotalAliens = 0;
}

CASW_Holdout_Wave::~CASW_Holdout_Wave()
{
	m_Entries.PurgeAndDeleteElements();
}

void CASW_Holdout_Wave::LoadFromKeyValues( int nWaveNumber, KeyValues *pKeys )
{
	m_Entries.PurgeAndDeleteElements();
	
	m_nTotalAliens = 0;
	m_nWaveNumber = nWaveNumber;
	m_iszWaveName = AllocPooledString( pKeys->GetString( "Name", "Unknown" ) );
	m_nEnvironmentModifiers = pKeys->GetInt( "EnvironmentModifiers" );	// TODO: Turn this into a string parser for the bit flag names
	m_bWaveHasResupply = pKeys->GetBool( "Resupply", false );

	for ( KeyValues *pKey = pKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		if ( !Q_stricmp( pKey->GetName(), "ENTRY" ) )
		{
			if ( asw_holdout_debug.GetBool() )
			{
				Msg( "    Loading a wave entry\n" );
			}
			CASW_Holdout_Wave_Entry *pEntry = new CASW_Holdout_Wave_Entry();
			pEntry->LoadFromKeyValues( pKey );
			m_Entries.AddToTail( pEntry );

			m_nTotalAliens += pEntry->GetQuantity();
		}
	}
}