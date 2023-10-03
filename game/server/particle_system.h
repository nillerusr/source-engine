//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
class CParticleSystem : public CBaseEntity
{
	DECLARE_CLASS( CParticleSystem, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CParticleSystem( void );

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual int  UpdateTransmitState(void);
	virtual int	 ObjectCaps( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );

	void		StartParticleSystem( void );
	void		StopParticleSystem( int nStopType = STOP_NORMAL );

	void		InputStart( inputdata_t &inputdata );
	void		InputStop( inputdata_t &inputdata );
	void		InputStopEndCap( inputdata_t &inputdata );
	void		InputDestroy( inputdata_t &inputdata );
	void		StartParticleSystemThink( void );
	bool		SetControlPointValue( int iControlPoint, const Vector &vValue ); //server controlled control points (variables in particle effects instead of literal follow points)
	void		DisableSaveRestore( bool bState ) { m_bNoSave = bState; }

	enum
	{	
		kSERVERCONTROLLEDPOINTS = 4,
		kMAXCONTROLPOINTS = 63, ///< actually one less than the total number of cpoints since 0 is assumed to be me
	}; 
	
	// stop types
	enum 
	{
		STOP_NORMAL = 0,
		STOP_DESTROY_IMMEDIATELY,
		STOP_PLAY_ENDCAP,
		NUM_STOP_TYPES
	};

protected:

	/// Load up and resolve the entities that are supposed to be the control points 
	void ReadControlPointEnts( void );

	bool				m_bNoSave;
	bool				m_bStartActive;
	string_t			m_iszEffectName;
	CNetworkString(		m_szSnapshotFileName, MAX_PATH );
	
	CNetworkVar( bool,	m_bActive );
	CNetworkVar( int,	m_nStopType );
	CNetworkVar( int,	m_iEffectIndex );
	CNetworkVar( float,	m_flStartTime );	// Time at which this effect was started.  This is used after restoring an active effect.
	
	//server controlled control points (variables in particle effects instead of literal follow points)
	CNetworkArray( Vector, m_vServerControlPoints, kSERVERCONTROLLEDPOINTS );
	CNetworkArray( uint8, m_iServerControlPointAssignments, kSERVERCONTROLLEDPOINTS );

	string_t			m_iszControlPointNames[kMAXCONTROLPOINTS];
	CNetworkArray( EHANDLE, m_hControlPointEnts, kMAXCONTROLPOINTS );
	CNetworkArray( unsigned char, m_iControlPointParents, kMAXCONTROLPOINTS );
};

#endif // PARTICLE_SYSTEM_H
