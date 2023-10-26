#include "cbase.h"
#include "asw_spawn_group.h"
#include "asw_base_spawner.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_spawn_group, CASW_Spawn_Group );

BEGIN_DATADESC( CASW_Spawn_Group )
	DEFINE_KEYFIELD( m_iszSpawnerNames[0], FIELD_STRING, "SpawnerName00"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[1], FIELD_STRING, "SpawnerName01"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[2], FIELD_STRING, "SpawnerName02"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[3], FIELD_STRING, "SpawnerName03"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[4], FIELD_STRING, "SpawnerName04"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[5], FIELD_STRING, "SpawnerName05"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[6], FIELD_STRING, "SpawnerName06"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[7], FIELD_STRING, "SpawnerName07"),
	DEFINE_KEYFIELD( m_iszSpawnerNames[8], FIELD_STRING, "SpawnerName08"),
END_DATADESC()

CASW_Spawn_Group::CASW_Spawn_Group()
{
}

CASW_Spawn_Group::~CASW_Spawn_Group()
{

}

void CASW_Spawn_Group::Spawn()
{
	BaseClass::Spawn();
}

void CASW_Spawn_Group::Activate()
{
	BaseClass::Activate();

	FindSpawners();
}

// searches for all our named spawners and adds them to the list
void CASW_Spawn_Group::FindSpawners()
{
	for ( int i = 0; i < MAX_SPAWNER_NAMES_PER_GROUP; i++ )
	{
		if ( m_iszSpawnerNames[ i ] == NULL_STRING )
			continue;

		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = gEntList.FindEntityByName( pEnt, m_iszSpawnerNames[ i ], NULL ) ) != NULL )
		{
			if ( pEnt->Classify() != CLASS_ASW_SPAWNER && pEnt->Classify() != CLASS_ASW_HOLDOUT_SPAWNER )
				continue;

			CASW_Base_Spawner *pSpawner = assert_cast<CASW_Base_Spawner*>( pEnt );
			m_hSpawners.AddToTail( pSpawner );
		}
	}
}

// picks a number of spawners randomly from this spawngroup
void CASW_Spawn_Group::PickSpawnersRandomly( int nNumSpawners, bool bIncludeRecentlySpawned, CUtlVector< CASW_Base_Spawner* > *pSpawners )
{
	pSpawners->RemoveAll();
	CUtlVector< CASW_Base_Spawner* > candidates;

	for ( int i = 0; i < m_hSpawners.Count(); i++ )
	{
		if ( !m_hSpawners[i].Get() || !m_hSpawners[i]->IsEnabled() )
			continue;

		if ( !bIncludeRecentlySpawned && m_hSpawners[i]->HasRecentlySpawned() )
			continue;

		candidates.AddToTail( m_hSpawners[i].Get() );
	}
	for ( int i = 0; ( (i < nNumSpawners) && (candidates.Count() > 0) ); i++ )
	{
		int nChosen = RandomInt( 0, candidates.Count() - 1 );
		pSpawners->AddToTail( candidates[ nChosen ] );
		candidates.Remove( nChosen );
	}
}