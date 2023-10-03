//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TEAM_OBJECTIVERESOURCE_H
#define TEAM_OBJECTIVERESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"

#define TEAM_ARRAY( index, team )		(index + (team * MAX_CONTROL_POINTS))

//-----------------------------------------------------------------------------
// Purpose: An entity that networks the state of the game's objectives.
//			May contain data for objectives that aren't used by your mod, but
//			the extra data will never be networked as long as it's zeroed out.
//-----------------------------------------------------------------------------
class CBaseTeamObjectiveResource : public CBaseEntity
{
	DECLARE_CLASS( CBaseTeamObjectiveResource, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CBaseTeamObjectiveResource();
	~CBaseTeamObjectiveResource();

	virtual void Spawn( void );
	virtual int  UpdateTransmitState(void);

	virtual void ObjectiveThink( void );

	//--------------------------------------------------------------------
	// CONTROL POINT DATA
	//--------------------------------------------------------------------
public:
	void ResetControlPoints( void );

	// Data functions, called to set up the state at the beginning of a round
	void SetNumControlPoints( int num );
	int	 GetNumControlPoints( void ) { return m_iNumControlPoints; }
	void SetCPIcons( int index, int iTeam, int iIcon );
	void SetCPOverlays( int index, int iTeam, int iIcon );
	void SetTeamBaseIcons( int iTeam, int iBaseIcon );
	void SetCPPosition( int index, const Vector& vPosition );
	void SetCPVisible( int index, bool bVisible );
	void SetCPRequiredCappers( int index, int iTeam, int iReqPlayers );
	void SetCPCapTime( int index, int iTeam, float flTime );
	void SetCPCapPercentage( int index, float flTime );
	float GetCPCapPercentage( int index );
	void SetTeamCanCap( int index, int iTeam, bool bCanCap );
	void SetBaseCP( int index, int iTeam );
	void SetPreviousPoint( int index, int iTeam, int iPrevIndex, int iPrevPoint );
	int GetPreviousPointForPoint( int index, int team, int iPrevIndex );
	bool TeamCanCapPoint( int index, int team );
	void SetCapLayoutInHUD( const char *pszLayout ) { Q_strncpy(m_pszCapLayoutInHUD.GetForModify(), pszLayout, MAX_CAPLAYOUT_LENGTH ); }
	void SetWarnOnCap( int index, int iWarnLevel );
	void SetWarnSound( int index, string_t iszSound );

	// State functions, called many times
	void SetNumPlayers( int index, int team, int iNumPlayers );
	void StartCap( int index, int team );
	void SetOwningTeam( int index, int team );
	void SetCappingTeam( int index, int team );
	void SetTeamInZone( int index, int team );
	void SetCapBlocked( int index, bool bBlocked );
	int  GetOwningTeam( int index );

	void AssertValidIndex( int index )
	{
		Assert( 0 <= index && index <= MAX_CONTROL_POINTS && index < m_iNumControlPoints );
	}

	int GetBaseControlPointForTeam( int iTeam ) 
	{ 
		Assert( iTeam < MAX_TEAMS );
		return m_iBaseControlPoints[iTeam]; 
	}

	int GetCappingTeam( int index )
	{
		if ( index >= m_iNumControlPoints )
			return TEAM_UNASSIGNED;

		return m_iCappingTeam[index];
	}

	void SetTimerInHUD( CBaseEntity *pTimer )
	{
		m_iTimerToShowInHUD = pTimer ? pTimer->entindex() : 0;
	}


	void SetStopWatchTimer( CBaseEntity *pTimer )
	{
		m_iStopWatchTimer = pTimer ? pTimer->entindex() : 0;
	}

	int GetTimerInHUD( void ) { return m_iTimerToShowInHUD; }

	// Mini-rounds data
	void SetPlayingMiniRounds( bool bPlayingMiniRounds ){ m_bPlayingMiniRounds = bPlayingMiniRounds; }
	bool PlayingMiniRounds( void ){ return m_bPlayingMiniRounds; }
	void SetInMiniRound( int index, bool bInRound ) { m_bInMiniRound.Set( index, bInRound ); }
	bool IsInMiniRound( int index ) { return m_bInMiniRound[index]; }

	void UpdateCapHudElement( void );

	// Train Path data
	void SetTrainPathDistance( int index, float flDistance );

private:
	CNetworkVar( int, m_iTimerToShowInHUD );	
	CNetworkVar( int, m_iStopWatchTimer );	

	CNetworkVar( int, m_iNumControlPoints );	
	CNetworkVar( bool, m_bPlayingMiniRounds );	
	CNetworkVar( bool, m_bControlPointsReset );
	CNetworkVar( int, m_iUpdateCapHudParity );

	// data variables
	CNetworkArray(	Vector,		m_vCPPositions,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,		m_bCPIsVisible,		MAX_CONTROL_POINTS );
	CNetworkArray(  float,		m_flLazyCapPerc,	MAX_CONTROL_POINTS );
	CNetworkArray(	int,		m_iTeamIcons,		MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );
	CNetworkArray(	int,		m_iTeamOverlays,	MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );
	CNetworkArray(  int,		m_iTeamReqCappers,	MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );
	CNetworkArray(  float,		m_flTeamCapTime,	MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );
	CNetworkArray(  int,		m_iPreviousPoints,	MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS * MAX_PREVIOUS_POINTS );
	CNetworkArray(  bool,		m_bTeamCanCap,		MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );
	CNetworkArray(	int,		m_iTeamBaseIcons,	MAX_TEAMS );
	CNetworkArray(  int,		m_iBaseControlPoints, MAX_TEAMS );
	CNetworkArray(	bool,		m_bInMiniRound,		MAX_CONTROL_POINTS );
	CNetworkArray(	int,		m_iWarnOnCap,		MAX_CONTROL_POINTS );
	CNetworkArray(	string_t,	m_iszWarnSound,		MAX_CONTROL_POINTS );
	CNetworkArray(  float,		m_flPathDistance,   MAX_CONTROL_POINTS );

	// change when players enter/exit an area
	CNetworkArray(  int,	m_iNumTeamMembers,	MAX_CONTROL_POINTS * MAX_CONTROL_POINT_TEAMS );

	// changes when a cap starts. start and end times are calculated on client
	CNetworkArray(	int,	m_iCappingTeam,		MAX_CONTROL_POINTS );

	CNetworkArray(	int,	m_iTeamInZone,		MAX_CONTROL_POINTS );
	CNetworkArray(	bool,	m_bBlocked,			MAX_CONTROL_POINTS );

	// changes when a point is successfully captured
	CNetworkArray(  int,    m_iOwner,			MAX_CONTROL_POINTS );

	// describes how to lay out the cap points in the hud
	CNetworkString(  m_pszCapLayoutInHUD,		MAX_CAPLAYOUT_LENGTH );

	// Not networked, because the client recalculates it
	float	m_flCapPercentages[ MAX_CONTROL_POINTS ];
};

extern CBaseTeamObjectiveResource *g_pObjectiveResource;

inline CBaseTeamObjectiveResource *ObjectiveResource()
{
	return g_pObjectiveResource;
}

#endif // TEAM_OBJECTIVERESOURCE_H
