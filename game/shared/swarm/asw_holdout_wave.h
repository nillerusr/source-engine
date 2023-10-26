#ifndef _INCLUDED_ASW_HOLDOUT_WAVE_H
#define _INCLUDED_ASW_HOLDOUT_WAVE_H
#ifdef _WIN32
#pragma once
#endif


class CASW_Spawn_Group;

//===============================================
//  An individual entry in a holdout wave
//===============================================
class CASW_Holdout_Wave_Entry
{
public:

	CASW_Holdout_Wave_Entry();
	virtual ~CASW_Holdout_Wave_Entry();

	void LoadFromKeyValues( KeyValues *pKeys );
	
	int					GetQuantity() const { return m_nQuantity; }
	string_t			GetAlienClass() const { return m_iszAlienClass; }
	float				GetSpawnDelay() const { return m_flSpawnDelay; }
	float				GetSpawnDuration() const { return m_flSpawnDuration; }
	int					GetModifiers() const { return m_nModifiers; }
	const char *		GetSpawnGroupName() { return STRING( m_iszSpawnGroupName ); }

#ifdef GAME_DLL
	CASW_Spawn_Group*	GetSpawnGroup();
#endif

protected:

	float		m_flSpawnDelay;			// time at which to spawn this entry, relative to the start of the wave
	string_t	m_iszAlienClass;	
	int			m_nQuantity;
	float		m_flSpawnDuration;		// spawns are spread out over this duration
	int			m_nModifiers;

	CHandle<CASW_Spawn_Group>	m_hSpawnGroup;
	string_t					m_iszSpawnGroupName;
};

//===============================================
//  A wave in holdout mode
//===============================================
class CASW_Holdout_Wave
{
public:

	CASW_Holdout_Wave();
	virtual ~CASW_Holdout_Wave();

	void LoadFromKeyValues( int nWaveNumber, KeyValues *pKeys );

	int							GetNumEntries() { return m_Entries.Count(); }
	CASW_Holdout_Wave_Entry*	GetEntry( int i ) { return m_Entries[i]; }
	string_t					GetWaveName() { return m_iszWaveName; }
	int							GetEnvironmentModifiers() { return m_nEnvironmentModifiers; }
	bool					WaveHasResupply() { return m_bWaveHasResupply; }

	// returns the total number of aliens in this wave
	int							GetTotalAliens() { return m_nTotalAliens; }

protected:

	int			m_nWaveNumber;
	string_t	m_iszWaveName;
	int			m_nEnvironmentModifiers;
	CUtlVector<CASW_Holdout_Wave_Entry*> m_Entries;
	int			m_nTotalAliens;
	bool		m_bWaveHasResupply;		// if true, this wave will show the resupply UI once it's completed

	// TODO: XP reward for this wave?
};

#endif /* _INCLUDED_ASW_HOLDOUT_WAVE_H */