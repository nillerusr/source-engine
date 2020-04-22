//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef DOD_CONTROL_POINT_MASTER_H
#define DOD_CONTROL_POINT_MASTER_H
#ifdef _WIN32
#pragma once
#endif

#include "utlmap.h"
#include "dod_shareddefs.h"
#include "dod_team.h"
#include "dod_gamerules.h"
#include "dod_control_point.h"
#include "dod_area_capture.h"
#include "dod_objective_resource.h"
#include "GameEventListener.h"

// ====================
// Control Point Master
// ====================
// One ControlPointMaster is spawned per level. Shortly after spawning it detects all the Control
// points in the map and puts them into the m_ControlPoints. From there it detects the state 
// where all points are captured and resets them if necessary It gives points every time interval to 
// the owners of the points
// ====================

#define TIMER_CONTEXT	"TIMER_CONTEXT"
#define FLAGS_CONTEXT	"FLAGS_CONTEXT"

class CControlPointMaster : public CBaseEntity
{
public:
	DECLARE_CLASS( CControlPointMaster, CBaseEntity );

	CControlPointMaster();
	
	virtual void Spawn( void );	
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	int GetNumPoints( void );
	int GetPointOwner( int point ); 
	void Reset( void );

	void RoundRespawn( void );

	int CountAdvantageFlags( int team );

	bool IsActive( void ) { return ( m_bDisabled == false ); }

	bool IsUsingRoundTimer( void ) { return m_bUseTimer; }
	void GetTimerData( int &iTimerSeconds, int &iTimerWinTeam );

	void FireTeamWinOutput( int iWinningTeam );

	void CheckWinConditions( void );

	bool WouldNewCPOwnerWinGame( CControlPoint *pPoint, int iNewOwner );

	CControlPoint *GetCPByIndex( int index );

private:
	void BecomeActive( void );
	void BecomeInactive( void );
	
	void EXPORT CPMThink( void );

	int TeamOwnsAllPoints( CControlPoint *pOverridePoint = NULL, int iOverrideNewTeam = TEAM_UNASSIGNED );

	void TeamWins( int team );

	bool FindControlPoints( void );	//look in the map to find active control points

	void InputAddTimerSeconds( inputdata_t &inputdata );

	CUtlMap<int, CControlPoint *>	m_ControlPoints;

	bool m_bFoundPoints;		//true when the control points have been found and the array is initialized
	
	float m_fGivePointsTime;	//the time at which we give points for holding control points

	DECLARE_DATADESC();

	bool m_bDisabled;				//is this CPM active or not
	void InputEnable( inputdata_t &inputdata ) { BecomeInactive(); }	
	void InputDisable( inputdata_t &inputdata ) { BecomeActive(); }

	void InputRoundInit( inputdata_t &inputdata );
	void InputRoundStart( inputdata_t &inputdata );

	bool m_bUseTimer;
	int m_iTimerTeam;
	int m_iTimerLength;

	COutputEvent m_AlliesWinOutput;
	COutputEvent m_AxisWinOutput;
};

class CDODCustomScoring : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CDODCustomScoring, CBaseEntity );

	DECLARE_DATADESC();

	CDODCustomScoring()
	{
		ListenForGameEvent( "dod_round_win" );
		ListenForGameEvent( "dod_round_active" );
	}

	virtual void Spawn( void )
	{
		Assert( m_iPointTeam == TEAM_ALLIES || m_iPointTeam == TEAM_AXIS );
    }

	virtual void FireGameEvent( IGameEvent *event )
	{
		const char *eventName = event->GetName();

		if ( !Q_strcmp( eventName, "dod_round_win" ) )
		{
			int team = event->GetInt( "team" );
			if ( team == m_iPointTeam )
			{
				GiveRemainingPoints();
			}

			// stop giving points, round is over
			SetThink( NULL );	// think no more!
		}
		else if ( !Q_strcmp( eventName, "dod_round_active" ) )
		{
			StartGivingPoints();
		}
	}

	// Needs to be activated by gamerules
	void StartGivingPoints( void )
	{
		if ( m_iNumPointGives <= 0 )
			return;

		if ( m_iPointsToGive <= 0 )
			return;

		m_iRemainingPoints = m_iNumPointGives * m_iPointsToGive;

		SetThink( &CDODCustomScoring::PointsThink );
		SetNextThink( gpGlobals->curtime + m_iTickLength );
	}

	// Give this team all the points that they would have gotten had we continued
	void GiveRemainingPoints( void )
	{
		GivePoints( m_iRemainingPoints );
	}

	// input to give tick points to our team
	void InputGivePoints( inputdata_t &inputdata )
	{
		int iPoints = inputdata.value.Int();
		GetGlobalTeam(m_iPointTeam)->AddScore( MAX( iPoints, 0 ) );
	}

	// accessor for our point team
	int GetPointTeam( void )
	{
		return m_iPointTeam;
	}

private:

	void GivePoints( int points )
	{
		GetGlobalTeam(m_iPointTeam)->AddScore( MAX( points, 0 ) );
		m_iRemainingPoints -= points;

		if ( points > 0 )
		{
			if ( points == 1 )
			{
				UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_allie_point" );
			}
			else
			{
				char buf[8];
				Q_snprintf( buf, sizeof(buf), "%d", points );
				UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_allie_points", buf );
			}

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_tick_points" );

			if ( event )
			{
				event->SetInt( "team", m_iPointTeam );
				event->SetInt( "score", points );
				event->SetInt( "totalscore", GetGlobalTeam(m_iPointTeam)->GetScore() );

				gameeventmanager->FireEvent( event );
			}
		}		
	}

	void PointsThink( void )
	{
		GivePoints( m_iPointsToGive );

		SetNextThink( gpGlobals->curtime + m_iTickLength );	
	}

	int m_iPointTeam;			// team to give points to
	int m_iPointsToGive;		// points to give per tick
	int m_iRemainingPoints;		// total number of points we have left to give
	int m_iTickLength;			// time between point gives
	int m_iNumPointGives;		// number of times we're planning on giving out points
};

#endif //DOD_CONTROL_POINT_MASTER_H
