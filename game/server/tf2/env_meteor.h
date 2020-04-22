//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENV_METEOR_H
#define ENV_METEOR_H
#pragma once

#include "BaseEntity.h"
#include "BaseAnimating.h"
#include "Env_Meteor_Shared.h"
#include "utlvector.h"

//=============================================================================
//
// Server-side Meteor Factory Class
//
class CMeteorFactory : public IMeteorFactory
{
public:

	void CreateMeteor( int nID, int iType, const Vector &vecPosition, 
		               const Vector &vecDirection, float flSpeed, float flStartTime,
					   float flDamageRadius,
					   const Vector &vecTriggerMins, const Vector &vecTriggerMaxs );
};

//=============================================================================
//
// Meteor Spawner Class
//
class CEnvMeteorSpawner : public CBaseEntity
{
public:
	
	DECLARE_CLASS( CEnvMeteorSpawner, CBaseEntity );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvMeteorSpawner();

	void	Spawn( void );
	void	Precache( void );
	void	MeteorSpawnerThink( void );
	int		UpdateTransmitState() { return SetTransmitState( FL_EDICT_FULLCHECK ); }
	int 	ShouldTransmit( const CCheckTransmitInfo *pInfo );
	void	Activate( void );

private:

	// Inputs
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );

	void	Get3DSkyboxWorldBounds( Vector &vecTriggerMins, Vector &vecTriggerMaxs );

	CMeteorFactory				m_Factory;
	CNetworkVarEmbedded( CEnvMeteorSpawnerShared, m_SpawnerShared );

	CNetworkVar( bool, m_fDisabled );				// Spawner active (trigger). NOTE: uses an f to remain consistent
	                                                        // with entity input system
};

//=============================================================================
//
// Meteor Target Class
//
class CEnvMeteorTarget : public CBaseEntity
{
public:

	DECLARE_CLASS( CEnvMeteorTarget, CBaseEntity );
	DECLARE_DATADESC();

	CEnvMeteorTarget();
	void	Spawn( void );
	
	int			m_iTargetID;
	float		m_flRadius;
};

//=============================================================================
//
// Meteor Class
//
class CEnvMeteor : public CBaseAnimating
{

	DECLARE_CLASS( CEnvMeteor, CBaseAnimating );

public:

	DECLARE_DATADESC();

	//-------------------------------------------------------------------------
	// Initialization
	//-------------------------------------------------------------------------
	CEnvMeteor();
	static CEnvMeteor  *Create( int nID, int iMeteorType, const Vector &vecOrigin, 
		                       const Vector &vecDirection, float flSpeed, float flStartTime,
							   float flDamageRadius,
							   const Vector &vecTriggerMins, const Vector &vecTriggerMaxs );
	void				Spawn( void );
	
	//-------------------------------------------------------------------------
	// Think(s)
	//-------------------------------------------------------------------------
	void				MeteorSkyboxThink( void );
	void				MeteorWorldThink( void );

private:

	CEnvMeteorShared			m_Meteor;
	bool						m_bPrevInSkybox;
	Vector						m_vecMin, m_vecMax;
};

//=============================================================================
//
// Shooting Star Spawner Class
//
class CShootingStarSpawner : public CBaseEntity
{
	DECLARE_CLASS( CShootingStarSpawner, CBaseEntity );

public:

	CShootingStarSpawner();

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual int		UpdateTransmitState() { return SetTransmitState( FL_EDICT_FULLCHECK ); }
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );

public:

	CNetworkVar( float, m_flSpawnInterval );		// How often do I spawn shooting stars?
	bool		m_bSkybox;				// Is the spawner in the skybox?
};

#endif // ENV_METEOR_H