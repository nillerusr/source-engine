//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENGINESTATS_H
#define ENGINESTATS_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "sysexternal.h"
#include "filesystem.h" // FileHandle_t define

enum EngineTimedStatId_t
{
	ENGINE_STATS_FRAME_TIME,
	ENGINE_STATS_FPS, // this is calculated at EndFrame!
	ENGINE_STATS_FPS_VARIABILITY,
	ENGINE_STATS_NUM_TIMED_STATS,
};
class CEngineStats
{
public:
	CEngineStats();

	//
	// stats input
	//

	void BeginRun( void );

	// Advances the next frame for the stats...
	void NextFrame(); 

	void BeginFrame( void );

	// Timed stat gathering
	void BeginTimedStat( EngineTimedStatId_t stat );
	void EndTimedStat( EngineTimedStatId_t stat );

	// Adds to a timed stat...
	void AddToTimedStat( EngineTimedStatId_t stat, float time );

	// Slams a timed stat
	void SetTimedStat( EngineTimedStatId_t stat, float time );

	// returns timed stats
	double TimedStatInFrame( EngineTimedStatId_t stat ) const;
	double TotalTimedStat( EngineTimedStatId_t stat ) const;

	void BeginDrawWorld( void );
	void EndDrawWorld( void );

	void EndFrame( void );
	void EndRun( void );

	void PauseStats( bool bPaused );

	//
	// stats output
	// call these outside of a BeginFrame/EndFrame pair
	//

	double GetRunTime( void );
	
	void SetFrameTime( float flFrameTime ) { m_flFrameTime = flFrameTime; }
	void SetFPSVariability( float flFPSVariability ) { m_flFPSVariability = flFPSVariability; }

	int FrameCount() const { return m_totalNumFrames; }

private:
	void ComputeFrameTimeStats( void );

	// How many frames worth of data have we logged?
	int m_totalNumFrames;

	// run timing data
	double m_runStartTime;
	double m_runEndTime;

	struct StatGroupInfo_t
	{
		double m_StatFrameTime[ENGINE_STATS_NUM_TIMED_STATS];
		double m_StatStartTime[ENGINE_STATS_NUM_TIMED_STATS];
		double m_TotalStatTime[ENGINE_STATS_NUM_TIMED_STATS];
	};
	StatGroupInfo_t m_StatGroup;
	bool m_InFrame;

	bool m_bPaused;
	bool m_bInRun;

	float m_flFrameTime;
	float m_flFPSVariability;
};


//-----------------------------------------------------------------------------
// Inlined stat gathering methods
//-----------------------------------------------------------------------------
inline void CEngineStats::BeginTimedStat( EngineTimedStatId_t stat )
{
	if (m_InFrame)
	{
		m_StatGroup.m_StatStartTime[stat] = 
			Sys_FloatTime();
	}
}

inline void CEngineStats::EndTimedStat( EngineTimedStatId_t stat )
{
	if (m_InFrame)
	{
		float dt = (float)Sys_FloatTime() - (float)(m_StatGroup.m_StatStartTime[stat]);
		m_StatGroup.m_StatFrameTime[stat] += dt; 
	}
}

// Adds to a timed stat...
inline void CEngineStats::AddToTimedStat( EngineTimedStatId_t stat, float dt )
{
	if (m_InFrame)
	{
		m_StatGroup.m_StatFrameTime[stat] += dt; 
	}
}

// Slams a timed stat
inline void CEngineStats::SetTimedStat( EngineTimedStatId_t stat, float time )
{
	m_StatGroup.m_StatFrameTime[stat] = time; 
}
extern CEngineStats g_EngineStats;

#endif // ENGINESTATS_H
