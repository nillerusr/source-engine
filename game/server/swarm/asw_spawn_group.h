#ifndef _INCLUDED_ASW_SPAWN_GROUP_H
#define _INCLUDED_ASW_SPAWN_GROUP_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_base_spawner.h"

#define MAX_SPAWNER_NAMES_PER_GROUP 8

//===============================================
//  Groups spawners by name
//===============================================
class CASW_Spawn_Group : public CLogicalEntity
{
public:
	DECLARE_CLASS( CASW_Spawn_Group, CLogicalEntity );
	DECLARE_DATADESC();

	CASW_Spawn_Group();
	virtual ~CASW_Spawn_Group();

	virtual void Spawn();
	virtual void Activate();

	void				FindSpawners();
	int					GetSpawnerCount() { return m_hSpawners.Count(); }
	CASW_Base_Spawner*	GetSpawner( int i ) { return m_hSpawners[ i ]; }

	// picks a number of spawners randomly from this spawngroup
	void				PickSpawnersRandomly( int nNumSpawners, bool bIncludeRecentlySpawned, CUtlVector< CASW_Base_Spawner* > *pSpawners );

	Class_T Classify() { return (Class_T) CLASS_ASW_SPAWN_GROUP; }

protected:
	typedef CHandle<CASW_Base_Spawner> SpawnerHandle_t;
	CUtlVector<SpawnerHandle_t> m_hSpawners;

	string_t m_iszSpawnerNames[ MAX_SPAWNER_NAMES_PER_GROUP ];
};

#endif /* _INCLUDED_ASW_SPAWN_GROUP_H */