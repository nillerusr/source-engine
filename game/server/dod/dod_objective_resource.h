//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DOD's objective resource, transmits all objective states to players
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_OBJECTIVE_RESOURCE_H
#define DOD_OBJECTIVE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_shareddefs.h"

class CDODObjectiveResource : public CBaseEntity
{
	DECLARE_CLASS( CDODObjectiveResource, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual int  UpdateTransmitState(void);

	void ResetControlPoints( void );

	// Data functions, called to set up the state at the beginning of a round
	void SetNumControlPoints( int num );
	void SetCPIcons( int index, int iAlliesIcon, int iAxisIcon, int iNeutralIcon, int iTimerCapIcon, int iBombedIcon );
	void SetCPPosition( int index, const Vector& vPosition );
	void SetCPVisible( int index, bool bVisible );
	void SetCPRequiredCappers( int index, int iReqAllies, int iReqAxis );
	void SetCPCapTime( int index, float flAlliesCapTime, float flAxisCapTime );

	// State functions, called many times
	void SetNumPlayers( int index, int team, int iNumPlayers );
	void StartCap( int index, int team );
	void SetOwningTeam( int index, int team );
	void SetCappingTeam( int index, int team );
	void SetBombPlanted( int index, bool bPlanted );
	void SetBombsRequired( int index, int iBombsRequired );
	void SetBombsRemaining( int index, int iBombsRemaining );
	void SetBombBeingDefused( int index, bool bBeingDefused );

	void AssertValidIndex( int index )
	{
		Assert( 0 <= index && index <= MAX_CONTROL_POINTS && index < m_iNumControlPoints );
	}

private:
	CNetworkVar( int,	m_iNumControlPoints );	

	// data variables
	CNetworkArray(	Vector,	m_vCPPositions,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_bCPIsVisible,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_iAlliesIcons,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_iAxisIcons,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_iNeutralIcons,	MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_iTimerCapIcons,	MAX_CONTROL_POINTS );
	CNetworkArray(	int,	m_iBombedIcons,		MAX_CONTROL_POINTS );
	CNetworkArray(  int,	m_iAlliesReqCappers,MAX_CONTROL_POINTS );
	CNetworkArray(  int,	m_iAxisReqCappers,	MAX_CONTROL_POINTS );
	CNetworkArray(  float,  m_flAlliesCapTime,  MAX_CONTROL_POINTS );
	CNetworkArray(  float,  m_flAxisCapTime,    MAX_CONTROL_POINTS );
	CNetworkArray(  int,    m_bBombPlanted,	    MAX_CONTROL_POINTS );
	CNetworkArray(  int,	m_iBombsRequired,   MAX_CONTROL_POINTS );
	CNetworkArray(  int,	m_iBombsRemaining,  MAX_CONTROL_POINTS );
	CNetworkArray(  int,    m_bBombBeingDefused,MAX_CONTROL_POINTS );

	// state variables

	// change when players enter/exit an area
	CNetworkArray(  int,	m_iNumAllies,		MAX_CONTROL_POINTS );
	CNetworkArray(  int,	m_iNumAxis,			MAX_CONTROL_POINTS );

	// changes when a cap starts. start and end times are calculated on client
	CNetworkArray(	int,	m_iCappingTeam,		MAX_CONTROL_POINTS );

	// changes when a point is successfully captured
	CNetworkArray(  int,    m_iOwner,			MAX_CONTROL_POINTS );
};

extern CDODObjectiveResource *g_pObjectiveResource;

#endif	//DOD_OBJECTIVE_RESOURCE_H