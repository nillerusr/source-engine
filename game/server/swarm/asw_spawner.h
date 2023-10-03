#ifndef _INCLUDED_ASW_SPAWNER_H
#define _INCLUDED_ASW_SPAWNER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_base_spawner.h"

//===============================================
//  An entity that spawns aliens over time
//===============================================
class CASW_Spawner : public CASW_Base_Spawner
{
public:
	DECLARE_CLASS( CASW_Spawner, CASW_Base_Spawner );
	DECLARE_DATADESC();

	CASW_Spawner();
	virtual ~CASW_Spawner();

	virtual void Spawn();
	virtual void Precache();
	virtual void InitAlienClassName();
	virtual int  DrawDebugTextOverlays();

	Class_T Classify() { return (Class_T) CLASS_ASW_SPAWNER; }

	virtual IASW_Spawnable_NPC*	SpawnAlien( const char *szAlienClassName, const Vector &vecHullMins, const Vector &vecHullMaxs );
	virtual bool			CanSpawn( const Vector &vecHullMins, const Vector &vecHullMaxs );
	bool					MoveSpawnableTo(IASW_Spawnable_NPC* pAlien, CBaseEntity* pGoalEnt, bool bIgnoreMarines);
	void					SpawnerThink();
	void					SpawnOneAlien();

	// get the hull size for the type of alien we're spawning
	const Vector&			GetAlienMins();
	const Vector&			GetAlienMaxs();

	void					SpawnedAllAliens();
	virtual void			AlienKilled( CBaseEntity *pVictim );
	void					MissionStart(); // called by gamerules when mission begins

	bool					ApplyCarnageMode( float fScaler, float fInvScaler );

	enum SpawnerState_t
	{
		SST_StartSpawningWhenMissionStart,
		SST_WaitForInputs,
		SST_Spawning,
		SST_Finished,
	};
	
	void SetSpawnerState( SpawnerState_t newState );

	// inputs
	void InputSpawnAlien( inputdata_t &inputdata );
	void InputStartSpawning( inputdata_t &inputdata );
	void InputStopSpawning( inputdata_t &inputdata );
	void InputToggleSpawning( inputdata_t &inputdata );

protected:

	// outputs
	COutputEvent m_OnAllSpawned;
	COutputEvent m_OnAllSpawnedDead;

	int			m_nMaxLiveAliens;			// max number of NPCs that this spawner may have out at one time.
	int			m_nNumAliens;				// max number of NPCs that this spawner can create total
	bool		m_bInfiniteAliens;
	float		m_flSpawnInterval;			// time between spawns
	float		m_flSpawnIntervalJitter;	// fractional variation applied to spawn interval each time to give it some randomness

	int			m_AlienClassNum;			// integer from choice in Hammer, which sets the classname on Spawn
	string_t	m_AlienClassName;			// classname of the NPC(s) that will be created.

	int			m_nCurrentLiveAliens;		// current number of live aliens

	SpawnerState_t m_SpawnerState;

};

// scales up alien amounts (but makes them weaker)
void ASW_ApplyCarnage_f(float fScaler);

#endif /* _INCLUDED_ASW_SPAWNER_H */