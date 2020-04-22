//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource collection entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_FUNC_RESOURCE_H
#define TF_FUNC_RESOURCE_H
#pragma once

#include "utlvector.h"
#include "props.h"
#include "techtree.h"
#include "entityoutput.h"
#include "ihasbuildpoints.h"

class CBaseTFPlayer;
class CTFTeam;
class CResourceChunk;
class CResourceSpawner;

//-----------------------------------------------------------------------------
// Purpose: Defines an area from which resources can be collected
//-----------------------------------------------------------------------------
class CResourceZone : public CBaseEntity, public IHasBuildPoints
{
	DECLARE_CLASS( CResourceZone, CBaseEntity );
public:
	CResourceZone();

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );

	// Inputs
	void	InputSetAmount( inputdata_t &inputdata );
	void	InputResetAmount( inputdata_t &inputdata );
	void	InputSetActive( inputdata_t &inputdata );
	void	InputSetInactive( inputdata_t &inputdata );
	void	InputToggleActive( inputdata_t &inputdata );

	void	SetActive( bool bActive );
	bool	GetActive() const;

	bool	IsEmpty( void );
	bool	PointIsWithin( const Vector &vecPoint );

	// need to transmit to players who are in commander mode
	int 	ShouldTransmit( const CCheckTransmitInfo *pInfo );

	// Team handling
	void	SetOwningTeam( int iTeamNumber );
	CTFTeam *GetOwningTeam( void );

	// Resource handling
	bool	RemoveResources( int nResourcesRemoved );
	int		GetResources( void ) const	{ return m_nResourcesLeft; }

	// Resource Chunks
	bool	ShouldSpawnChunk( void );
	void	SpawnChunk( const Vector &vecOrigin );
	void	RemoveChunk( CResourceChunk *pChunk, bool bReturn );

	// Resource Spawners
	void	AddSpawner( CResourceSpawner *pSpawner );

	// Zone increasing....
	void	AddZoneIncreaser( float rate );
	void	RemoveZoneIncreaser( float rate );

	CNetworkVar( float, m_flClientResources );	// Amount sent to clients (0->1 range)

	float	GetResourceRate() const { return m_flResourceRate; }

// IHasBuildPoints
public:
	virtual	int		GetNumBuildPoints( void ) const;
	virtual bool	CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );
	virtual bool	GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );
	virtual int		GetBuildPointAttachmentIndex( int iPoint ) const;
	virtual void	SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );
	virtual float	GetMaxSnapDistance( int iPoint ) { return 128; }
	virtual int		GetNumObjectsOnMe( void );
	virtual CBaseEntity	*GetFirstObjectOnMe( void );
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );
	virtual void	RemoveAllObjects( void );
	virtual bool	ShouldCheckForMovement( void ) { return false; }
	virtual int		FindObjectOnBuildPoint( CBaseObject *pObject );
	virtual void	GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles );

private:
	void	RecomputeClientResources( );

	// Team handling
	float	m_flTestTime;			// Time to next check to see if we've been captured.

	bool	m_bActive;

	// Resource handling
	int		m_iTeamGathering;		// Team that's gathering from this resource
	Vector	m_vecGatherPoint;		// Position for the resource collector to be to gather from this point
	QAngle	m_angGatherPoint;		// Angles for the collector to be at on the point

	// Resources
	CNetworkVar( int, m_nResourcesLeft );		// Amount of the resource that's left
	int		m_nMaxResources;		// Max resources at this zone
	float	m_flResourceRate;	// Time between each suck from this zone by resource pumps
	float	m_flBaseResourceRate;

	// Resource chunks in this zone
	int								m_iMaxChunks;
	float							m_flRespawnTimeModifier;	// speed deltas imposed by zone increasers
	float							m_flChunkValueMin;
	float							m_flChunkValueMax;
	CUtlVector< CResourceChunk* >	m_aChunks;

	COutputEvent	m_OnEmpty;

	EHANDLE	m_hResourcePump;

	// Resource spawners in this zone
	CUtlVector< CResourceSpawner* > m_aSpawners;
};

//-----------------------------------------------------------------------------
// Purpose: A resource chunk spawning point
//-----------------------------------------------------------------------------
class CResourceSpawner : public CBaseAnimating
{
	DECLARE_CLASS( CResourceSpawner, CBaseAnimating );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn( void );
	void	Precache( void );
	void	Activate( void );
	void	SetActive( bool bActive );

	void	SpawnChunkThink( void );

public:
	CHandle<CResourceZone>	m_hZone;
	CNetworkVar( bool, m_bActive );
};

void ConvertResourceValueToChunks( int iResources, int *iNumProcessed, int *iNumNormal );

#endif // TF_FUNC_RESOURCE_H
