#ifndef _INCLUDED_ASW_HOLDOUT_MODE_H
#define _INCLUDED_ASW_HOLDOUT_MODE_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlSortVector.h"

#ifdef CLIENT_DLL
#define CASW_Holdout_Mode C_ASW_Holdout_Mode
#else
class CASW_Player;
class CASW_Marine_Resource;
class CASW_EquipItem;
#endif

class CASW_Holdout_Wave;
class CASW_Holdout_Wave_Entry;
class CASW_Spawn_Group;

enum ASW_Holdout_Event_t
{
	HOLDOUT_EVENT_START_WAVE_ENTRY = 0,
	HOLDOUT_EVENT_SINGLE_SPAWN,
	HOLDOUT_EVENT_STATE_CHANGE,
};

enum ASW_Holdout_State_t
{
	HOLDOUT_STATE_BRIEFING = 0,
	HOLDOUT_STATE_ANNOUNCE_NEW_WAVE,
	HOLDOUT_STATE_WAVE_IN_PROGRESS,
	HOLDOUT_STATE_SHOWING_WAVE_SCORE,
	HOLDOUT_STATE_RESUPPLY,
	HOLDOUT_STATE_COMPLETE,
};

class CASW_Holdout_Event
{
public:
	ASW_Holdout_Event_t			m_EventType;
	float						m_flEventTime;
};

class CASW_Holdout_Spawn_Event : public CASW_Holdout_Event
{
public:
	CASW_Holdout_Wave_Entry 	*m_pWaveEntry;
};

class CASW_Holdout_State_Change_Event : public CASW_Holdout_Event
{
public:
	ASW_Holdout_State_t 	m_NewState;
};

class CASW_Holdout_Event_LessFunc
{
public:
	bool Less( CASW_Holdout_Event * const & lhs, CASW_Holdout_Event * const & rhs, void *pContext )
	{
		return lhs->m_flEventTime < rhs->m_flEventTime;
	}
};

#define MAX_HOLDOUT_WAVES 10

//-----------------------------------------------------------------------------
// This entity is responsible for the holdout mode logic.
//  It process the holdout wave events and controls holdout mode state
//-----------------------------------------------------------------------------
class CASW_Holdout_Mode : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Holdout_Mode, CBaseEntity );
	DECLARE_NETWORKCLASS();
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CASW_Holdout_Mode();
	virtual ~CASW_Holdout_Mode();

	void					LevelInitPostEntity();
	virtual void			UpdateOnRemove();

	ASW_Holdout_State_t		GetHoldoutState() { return (ASW_Holdout_State_t) m_nHoldoutState.Get(); }
	int						GetCurrentWave() { return m_nCurrentWave.Get(); }
	int						GetCurrentScore() { return m_nCurrentScore.Get(); }
	int						GetAliensKilledThisWave() { return m_nAliensKilledThisWave.Get(); }
	float					GetResupplyEndTime()	{ return m_flResupplyEndTime.Get(); }
	float					GetWaveProgress();
	int						GetNumWaves() { return m_Waves.Count(); }

#ifdef GAME_DLL
	virtual void			Spawn();
	virtual void			Activate();
	virtual void			Precache();
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }
	
	void					InputUnlockResupply( inputdata_t &inputdata );
	void					OnMissionStart();
	void					OnAlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info );
	void					HoldoutThink();
	void					SetCurrentWave( int nWave ) { m_nCurrentWave = nWave; }
	void					StartWave( int nWave );
	void					LoadoutSelect( CASW_Marine_Resource *pMarineResource, int nEquipIndex, int nInvSlot );
#else
	void					OnDataChanged( DataUpdateType_t updateType );
#endif
	
protected:

#ifdef GAME_DLL
	void					AddEvent( CASW_Holdout_Event *pEvent );
	void					ProcessEvent( CASW_Holdout_Event *pEvent );
	void					ChangeHoldoutState( ASW_Holdout_State_t nNewState );
	int						SpawnAliens( int nQuantity, CASW_Holdout_Wave_Entry *pEntry, CASW_Spawn_Group *pSpawnGroup );
	void					RefillMarineAmmo();
	void					ResurrectDeadMarines();

	CUtlSortVector< CASW_Holdout_Event *, CASW_Holdout_Event_LessFunc >	m_Events;

	// Outputs
	COutputEvent m_OnWave[MAX_HOLDOUT_WAVES];				// Fired when the matching wave starts.
	COutputEvent m_OnWaveDefault;							// Fired on all waves, regardless of number.
	COutputEvent m_OnAnnounceWave[MAX_HOLDOUT_WAVES];		// Fired when the matching wave is announced.
	COutputEvent m_OnAnnounceWaveDefault;					// Fired when any wave is announced, regardless of number.

	string_t	m_holdoutFilename;

	int m_nUnlockedResupply;
	bool m_bResurrectingMarines;
#endif	

	CNetworkVar( int, m_nHoldoutState );
	CNetworkVar( int, m_nCurrentWave );
	CNetworkVar( int, m_nCurrentScore );
	CNetworkVar( int, m_nAliensKilledThisWave );
	CNetworkVar( float, m_flCurrentWaveStartTime );
	CNetworkVar( float, m_flResupplyEndTime );
	CNetworkString( m_netHoldoutFilename, MAX_PATH );

	void					LoadWaves();
	CUtlVector<CASW_Holdout_Wave*> m_Waves;	
};

// returns the holdout mode, if any in the current map
CASW_Holdout_Mode* ASWHoldoutMode();

#endif /* _INCLUDED_ASW_HOLDOUT_MODE_H */