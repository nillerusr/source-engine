#ifndef _INCLUDED_ASW_SPAWN_MANAGER_H
#define _INCLUDED_ASW_SPAWN_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CAI_Network;
class CTriggerMultiple;
struct AI_Waypoint_t;
class CAI_Node;
class CASW_Alien;

// The spawn manager can spawn aliens and groups of aliens

class ASW_Alien_Class_Entry
{
public:
	ASW_Alien_Class_Entry( const char *szClass, int nHullType ) { m_pszAlienClass = szClass; m_nHullType = nHullType; }

	const char *m_pszAlienClass;
	string_t m_iszAlienClass;
	int m_nHullType;
};

class CASW_Open_Area
{
public:
	CASW_Open_Area()
	{
		m_flArea = 0.0f;
		m_nTotalLinks = 0;
		m_vecOrigin = vec3_origin;
		m_pNode = NULL;
	}
	float m_flArea;
	int m_nTotalLinks;
	Vector m_vecOrigin;
	CAI_Node *m_pNode;
	CUtlVector<CAI_Node*> m_aAreaNodes;
};

class CASW_Spawn_Manager
{
public:
	CASW_Spawn_Manager();
	~CASW_Spawn_Manager();

	void LevelInitPreEntity();
	void LevelInitPostEntity();
	void Update();
	bool AddHorde( int iHordeSize );			// creates a large pack of aliens somewhere near the marines
	void AddAlien();							// creates a single alien somewhere near the marines

	int SpawnAlienBatch( const char *szAlienClass, int iNumAliens, const Vector &vecPosition, const QAngle &angle, float flMarinesBeyondDist = 0 );	
	CBaseEntity* SpawnAlienAt(const char* szAlienClass, const Vector& vecPos, const QAngle &angle);

	bool ValidSpawnPoint( const Vector &vecPosition, const Vector &vecMins, const Vector &vecMaxs, bool bCheckGround = true, float flMarineNearDistance = 0 );
	bool LineBlockedByGeometry( const Vector &vecSrc, const Vector &vecEnd );
	
	bool GetAlienBounds( const char *szAlienClass, Vector &vecMins, Vector &vecMaxs );
	bool GetAlienBounds( string_t iszAlienClass, Vector &vecMins, Vector &vecMaxs );

	int GetHordeToSpawn() { return m_iHordeToSpawn; }

	void OnAlienWokeUp( CASW_Alien *pAlien );
	void OnAlienSleeping( CASW_Alien *pAlien );
	int GetAwakeAliens() { return m_nAwakeAliens; }
	int GetAwakeDrones() { return m_nAwakeDrones; }

	int GetNumAlienClasses();
	ASW_Alien_Class_Entry* GetAlienClass( int i );

	// spawns a shieldbug somewhere randomly in the map
	bool SpawnRandomShieldbug();
	bool SpawnRandomParasitePack( int nParasites );

private:
	void UpdateCandidateNodes();
	bool FindHordePosition();
	CAI_Network* GetNetwork();
	bool SpawnAlientAtRandomNode();
	void FindEscapeTriggers();
	void DeleteRoute( AI_Waypoint_t *pWaypointList );

	// finds an area with good node connectivity.  Caller should take ownership of the CASW_Open_Area instance.
	CASW_Open_Area* FindNearbyOpenArea( const Vector &vecSearchOrigin, int nSearchHull );

	CountdownTimer m_batchInterval;
	Vector m_vecHordePosition;
	QAngle m_angHordeAngle;
	int m_iHordeToSpawn;
	int m_iAliensToSpawn;

	int m_nAwakeAliens;
	int m_nAwakeDrones;

	// maintaining a list of possible nodes to spawn aliens from
	CUtlVector<int> m_northCandidateNodes;
	CUtlVector<int> m_southCandidateNodes;
	CountdownTimer m_CandidateUpdateTimer;

	typedef CHandle<CTriggerMultiple> TriggerMultiple_t;
	CUtlVector<TriggerMultiple_t> m_EscapeTriggers;
};

extern const int g_nDroneClassEntry;
extern const int g_nDroneJumperClassEntry;

CASW_Spawn_Manager* ASWSpawnManager();

#endif // _INCLUDED_ASW_SPAWN_MANAGER_H