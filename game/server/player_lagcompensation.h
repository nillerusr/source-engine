//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Manages the server moving things back in time to match up to where clients thought they were 
//			when the client commited an action
//
// $NoKeywords: $
//=============================================================================//

#ifndef _PLAYER_LAG_COMPENSATION_H_
#define _PLAYER_LAG_COMPENSATION_H_

#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "ilagcompensationmanager.h"
#include "utllinkedlist.h"

#define MAX_LAYER_RECORDS (CBaseAnimatingOverlay::MAX_OVERLAYS)

struct LayerRecord
{
	int m_sequence;
	float m_cycle;
	float m_weight;
	int m_order;

	LayerRecord()
	{
		Clear();
	}

	LayerRecord( const LayerRecord& src )
	{
		m_sequence = src.m_sequence;
		m_cycle = src.m_cycle;
		m_weight = src.m_weight;
		m_order = src.m_order;
	}

	void Clear()
	{
		m_sequence = 0;
		m_cycle = 0;
		m_weight = 0;
		m_order = 0;
	}
};

struct LagRecord
{
public:
	LagRecord()
	{
		Clear();
	}

	LagRecord( const LagRecord& src )
	{
		m_fFlags = src.m_fFlags;
		m_vecOrigin = src.m_vecOrigin;
		m_vecAngles = src.m_vecAngles;
		m_vecMins = src.m_vecMins;
		m_vecMaxs = src.m_vecMaxs;
		m_flSimulationTime = src.m_flSimulationTime;
		for( int layerIndex = 0; layerIndex < MAX_LAYER_RECORDS; ++layerIndex )
		{
			m_layerRecords[layerIndex] = src.m_layerRecords[layerIndex];
		}
		m_masterSequence = src.m_masterSequence;
		m_masterCycle = src.m_masterCycle;
	}

	void Clear()
	{
		m_fFlags = 0;
		m_vecOrigin.Init();
		m_vecAngles.Init();
		m_vecMins.Init();
		m_vecMaxs.Init();
		m_flSimulationTime = -1;
		m_masterSequence = 0;
		m_masterCycle = 0;
		for( int layerIndex = 0; layerIndex < MAX_LAYER_RECORDS; ++layerIndex )
		{
			m_layerRecords[layerIndex].Clear();
		}
	}

	// Did player die this frame
	int						m_fFlags;

	// Player position, orientation and bbox
	Vector					m_vecOrigin;
	QAngle					m_vecAngles;
	Vector					m_vecMins;
	Vector					m_vecMaxs;

	float					m_flSimulationTime;	

	// Player animation details, so we can get the legs in the right spot.
	LayerRecord				m_layerRecords[MAX_LAYER_RECORDS];
	int						m_masterSequence;
	float					m_masterCycle;
};

typedef CUtlFixedLinkedList< LagRecord > LagRecordList;

//-----------------------------------------------------------------------------
class CLagCompensationManager : public CAutoGameSystemPerFrame, public ILagCompensationManager
{
public:
	CLagCompensationManager( char const *name ) : 
		CAutoGameSystemPerFrame( name ), 
		m_CompensatedEntities( 0, 0, DefLessFunc( EHANDLE ) ),
		m_AdditionalEntities( 0, 0, DefLessFunc( EHANDLE ) )
	{
		m_bNeedToRestore = false;
		m_weaponRange = 0.0f;
		m_isCurrentlyDoingCompensation = false;
	}

	// IServerSystem stuff
	virtual void Shutdown()
	{
		ClearHistory();
	}

	virtual void LevelShutdownPostEntity()
	{
		ClearHistory();
	}

	// called after entities think
	virtual void FrameUpdatePostEntityThink();

	// ILagCompensationManager stuff

	// Called during player movement to set up/restore after lag compensation
	void			StartLagCompensation( CBasePlayer *player, LagCompensationType lagCompensationType, const Vector& weaponPos = vec3_origin, const QAngle &weaponAngles = vec3_angle, float weaponRange = 0.0f );
	void			FinishLagCompensation( CBasePlayer *player );

	// Mappers can flag certain additional entities to lag compensate, this handles them
	virtual void	AddAdditionalEntity( CBaseEntity *pEntity );
	virtual void	RemoveAdditionalEntity( CBaseEntity *pEntity );

	void RecordDataIntoTrack( CBaseEntity *entity, LagRecordList *track, bool wantsAnims );
	bool BacktrackEntity( CBaseEntity *entity, float flTargetTime, LagRecordList *track, LagRecord *restore, LagRecord *change, bool wantsAnims );
	void RestoreEntityFromRecords( CBaseEntity *entity, LagRecord *restore, LagRecord *change, bool wantsAnims );
private:


	void ClearHistory()
	{
		FOR_EACH_MAP( m_CompensatedEntities, i )
		{
			delete m_CompensatedEntities[ i ];
		}
		m_CompensatedEntities.Purge();
	}

	struct EntityLagData
	{
		EntityLagData() : m_bRestoreEntity( false )
		{
		}

		// True if lag compensation altered entity data
		bool			m_bRestoreEntity;			   
		// keep a list of lag records for each player
		LagRecordList	m_LagRecords;				   

		// Entity data before we moved him back
		LagRecord		m_RestoreData;
		// Entity data where we moved him back
		LagRecord		m_ChangeData;
	};

	CUtlMap< EHANDLE, EntityLagData * > m_CompensatedEntities;

	// True if at least one entity was changed
	bool					m_bNeedToRestore;
	CBasePlayer				*m_pCurrentPlayer;	// The player we are doing lag compensation for

	LagCompensationType		m_lagCompensationType;
	Vector					m_weaponPos;
	QAngle					m_weaponAngles;
	float					m_weaponRange;
	bool					m_isCurrentlyDoingCompensation;	// Sentinel to prevent calling StartLagCompensation a second time before a Finish.

    // List of additional entities flagged by mappers for lag compensation (shouldn't be more than a few)
	CUtlRBTree< EHANDLE >	m_AdditionalEntities;
};

#endif