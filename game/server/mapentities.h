//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPENTITIES_H
#define MAPENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "mapentities_shared.h"


class CPointTemplate;


// This class provides hooks into the map-entity loading process that allows CS to do some tricks
// when restarting the round. The main trick it tries to do is recreate all 
abstract_class IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname ) = 0;
	virtual CBaseEntity* CreateNextEntity( const char *pClassname ) = 0;
};

// Use the filter so you can prevent certain entities from being created out of the map.
// CSPort does this when restarting rounds. It wants to reload most entities from the map, but certain
// entities like the world entity need to be left intact.
void MapEntity_ParseAllEntities( const char *pMapData, IMapEntityFilter *pFilter=NULL, bool bActivateEntities=false );

const char *MapEntity_ParseEntity( CBaseEntity *&pEntity, const char *pEntData, IMapEntityFilter *pFilter );
void MapEntity_PrecacheEntity( const char *pEntData, int &nStringSize );


//-----------------------------------------------------------------------------
// Hierarchical spawn 
//-----------------------------------------------------------------------------
struct HierarchicalSpawn_t
{
	CBaseEntity *m_pEntity;
	int			m_nDepth;
	CBaseEntity	*m_pDeferredParent;			// attachment parents can't be set until the parents are spawned
	const char	*m_pDeferredParentAttachment; // so defer setting them up until the second pass
};

struct HierarchicalSpawnMapData_t
{
	const char	*m_pMapData;
	int			m_iMapDataLength;
};

// Shared by mapentities.cpp and Foundry for spawning entities.
class CMapEntitySpawner
{
public:
	CMapEntitySpawner();
	~CMapEntitySpawner();
	
	void AddEntity( CBaseEntity *pEntity, const char *pMapData, int iMapDataLength );
	void HandleTemplates();
	void SpawnAndActivate( bool bActivateEntities );
	void PurgeRemovedEntities();

public:
	bool m_bFoundryMode;

private:

	HierarchicalSpawnMapData_t *m_pSpawnMapData;
	HierarchicalSpawn_t *m_pSpawnList;
	CUtlVector< CPointTemplate* > m_PointTemplates;
	int m_nEntities;
};

void SpawnHierarchicalList( int nEntities, HierarchicalSpawn_t *pSpawnList, bool bActivateEntities );
void MapEntity_ParseAllEntites_SpawnTemplates( CPointTemplate **pTemplates, int iTemplateCount, CBaseEntity **pSpawnedEntities, HierarchicalSpawnMapData_t *pSpawnMapData, int iSpawnedEntityCount );

#endif // MAPENTITIES_H
