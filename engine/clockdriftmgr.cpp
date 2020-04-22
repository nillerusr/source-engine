//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "client.h"
#include "clockdriftmgr.h"
#include "demo.h"
#include "server.h"
#include "enginethreads.h"


ConVar cl_clock_correction( "cl_clock_correction", "1", FCVAR_CHEAT, "Enable/disable clock correction on the client." );

ConVar cl_clockdrift_max_ms( "cl_clockdrift_max_ms", "150", FCVAR_CHEAT, "Maximum number of milliseconds the clock is allowed to drift before the client snaps its clock to the server's." );
ConVar cl_clockdrift_max_ms_threadmode( "cl_clockdrift_max_ms_threadmode", "0", FCVAR_CHEAT, "Maximum number of milliseconds the clock is allowed to drift before the client snaps its clock to the server's." );

ConVar cl_clock_showdebuginfo( "cl_clock_showdebuginfo", "0", FCVAR_CHEAT, "Show debugging info about the clock drift. ");

ConVar cl_clock_correction_force_server_tick( "cl_clock_correction_force_server_tick", "999", FCVAR_CHEAT, "Force clock correction to match the server tick + this offset (-999 disables it)."  );

ConVar cl_clock_correction_adjustment_max_amount( "cl_clock_correction_adjustment_max_amount", "200", FCVAR_CHEAT, 
	"Sets the maximum number of milliseconds per second it is allowed to correct the client clock. "
	"It will only correct this amount if the difference between the client and server clock is equal to or larger than cl_clock_correction_adjustment_max_offset." );

ConVar cl_clock_correction_adjustment_min_offset( "cl_clock_correction_adjustment_min_offset", "10", FCVAR_CHEAT, 
	"If the clock offset is less than this amount (in milliseconds), then no clock correction is applied." );

ConVar cl_clock_correction_adjustment_max_offset( "cl_clock_correction_adjustment_max_offset", "90", FCVAR_CHEAT, 
	"As the clock offset goes from cl_clock_correction_adjustment_min_offset to this value (in milliseconds), "
	"it moves towards applying cl_clock_correction_adjustment_max_amount of adjustment. That way, the response "
	"is small when the offset is small." );



// Given the offset (in milliseconds) of the client clock from the server clock,
// returns how much correction we'd like to apply per second (in seconds).
static float GetClockAdjustmentAmount( float flCurDiffInMS )
{
	flCurDiffInMS = clamp( flCurDiffInMS, cl_clock_correction_adjustment_min_offset.GetFloat(), cl_clock_correction_adjustment_max_offset.GetFloat() );

	float flReturnValue = RemapVal( flCurDiffInMS,
		cl_clock_correction_adjustment_min_offset.GetFloat(), cl_clock_correction_adjustment_max_offset.GetFloat(),
		0, cl_clock_correction_adjustment_max_amount.GetFloat() / 1000.0f );

	return flReturnValue;
}
	 

// -------------------------------------------------------------------------------------------------- /
// CClockDriftMgr implementation.
// -------------------------------------------------------------------------------------------------- /

CClockDriftMgr::CClockDriftMgr()
{
	Clear();
}


void CClockDriftMgr::Clear()
{
	m_nClientTick = 0;
	m_nServerTick = 0;
	m_iCurClockOffset = 0;
	memset( m_ClockOffsets, 0, sizeof( m_ClockOffsets ) );
}


// when running in threaded host mode, the clock drifts by a predictable algorithm
// because the client lags the server by one frame
// so at each update from the network we have lastframeticks-1 pending ticks to execute
// on the client.  If the clock has drifted by exactly that amount, allow it to drift temporarily
// NOTE: When the server gets paused the tick count is still incorrect for a frame
// NOTE: It should be possible to fix this by applying pause before the tick is incremented
// NOTE: or decrementing the client tick after receiving pause
// NOTE: This is due to the fact that currently pause is applied at frame start on the server
// NOTE: and frame end on the client
void CClockDriftMgr::SetServerTick( int nTick )
{
#if !defined( SWDS )
	m_nServerTick = nTick;

	int nMaxDriftTicks = IsEngineThreaded() ? 
		TIME_TO_TICKS( (cl_clockdrift_max_ms_threadmode.GetFloat() / 1000.0) ) :
		TIME_TO_TICKS( (cl_clockdrift_max_ms.GetFloat() / 1000.0) );

	int clientTick = cl.GetClientTickCount() + g_ClientGlobalVariables.simTicksThisFrame - 1;
	if ( cl_clock_correction_force_server_tick.GetInt() == 999 )
	{
		// If this is the first tick from the server, or if we get further than cl_clockdrift_max_ticks off, then 
		// use the old behavior and slam the server's tick into the client tick.
		if ( !IsClockCorrectionEnabled() ||
			 clientTick == 0 || 
			 abs(nTick - clientTick) > nMaxDriftTicks
			)
		{
			cl.SetClientTickCount( nTick - (g_ClientGlobalVariables.simTicksThisFrame - 1) );
			if ( cl.GetClientTickCount() < cl.oldtickcount )
			{
				cl.oldtickcount = cl.GetClientTickCount();
			}
			memset( m_ClockOffsets, 0, sizeof( m_ClockOffsets ) );
		}
	}
	else
	{
		// Used for testing..
		cl.SetClientTickCount( nTick + cl_clock_correction_force_server_tick.GetInt() );
	}

	// adjust the clock offset by the clock with thread mode compensation
	m_ClockOffsets[m_iCurClockOffset] = clientTick - m_nServerTick;
	m_iCurClockOffset = (m_iCurClockOffset + 1) % NUM_CLOCKDRIFT_SAMPLES;
#endif // SWDS
}


float CClockDriftMgr::AdjustFrameTime( float inputFrameTime )
{
	float flAdjustmentThisFrame = 0;
	float flAdjustmentPerSec = 0;
	if ( IsClockCorrectionEnabled() 
#if !defined( _XBOX ) && !defined( SWDS )
		 && !demoplayer->IsPlayingBack()
#endif
		)
	{
		// Get the clock difference in seconds.
		float flCurDiffInSeconds = GetCurrentClockDifference() * host_state.interval_per_tick;
		float flCurDiffInMS = flCurDiffInSeconds * 1000.0f;

		// Is the server ahead or behind us?
		if ( flCurDiffInMS > cl_clock_correction_adjustment_min_offset.GetFloat() )
		{
			flAdjustmentPerSec = -GetClockAdjustmentAmount( flCurDiffInMS );
			flAdjustmentThisFrame = inputFrameTime * flAdjustmentPerSec;
			flAdjustmentThisFrame = max( flAdjustmentThisFrame, -flCurDiffInSeconds );
		}
		else if ( flCurDiffInMS < -cl_clock_correction_adjustment_min_offset.GetFloat() )
		{
			flAdjustmentPerSec = GetClockAdjustmentAmount( -flCurDiffInMS );
			flAdjustmentThisFrame = inputFrameTime * flAdjustmentPerSec;
			flAdjustmentThisFrame = min( flAdjustmentThisFrame, -flCurDiffInSeconds );
		}

		if ( IsEngineThreaded() )
		{
			flAdjustmentThisFrame = -flCurDiffInSeconds;
		}

		AdjustAverageDifferenceBy( flAdjustmentThisFrame );
	}

	ShowDebugInfo( flAdjustmentPerSec );
	return inputFrameTime + flAdjustmentThisFrame;
}


float CClockDriftMgr::GetCurrentClockDifference() const
{
	// Note: this could be optimized a little by updating it each time we add
	// a sample (subtract the old value from the total and add the new one in).
	float total = 0;
	for ( int i=0; i < NUM_CLOCKDRIFT_SAMPLES; i++ )
		total += m_ClockOffsets[i];

	return total / NUM_CLOCKDRIFT_SAMPLES;
}


void CClockDriftMgr::ShowDebugInfo( float flAdjustment )
{
	if ( !cl_clock_showdebuginfo.GetInt() )
		return;

	if ( IsClockCorrectionEnabled() )
	{
		int high=-999, low=999;
		int exactDiff = cl.GetClientTickCount() - m_nServerTick;
		for ( int i=0; i < NUM_CLOCKDRIFT_SAMPLES; i++ )
		{
			high = max( (float)high, m_ClockOffsets[i] );
			low = min( (float)low, m_ClockOffsets[i] );
		}

		Msg( "Clock drift: adjustment (per sec): %.2fms, avg: %.3f, lo: %d, hi: %d, ex: %d\n", flAdjustment*1000.0f, GetCurrentClockDifference(), low, high, exactDiff );
	}
	else
	{
		Msg( "Clock drift disabled.\n" );
	}
}


void CClockDriftMgr::AdjustAverageDifferenceBy( float flAmountInSeconds )
{
	// Don't adjust the average if it's already tiny.
	float c = GetCurrentClockDifference();
	if ( c < 0.05f )
		return;

	float flAmountInTicks = flAmountInSeconds / host_state.interval_per_tick;
	float factor = 1 + flAmountInTicks / c;

	for ( int i=0; i < NUM_CLOCKDRIFT_SAMPLES; i++ )
		m_ClockOffsets[i] *= factor;
	
	Assert( fabs( GetCurrentClockDifference() - (c + flAmountInTicks) ) < 0.001f );
}

extern float NET_GetFakeLag();
extern ConVar net_usesocketsforloopback;

bool CClockDriftMgr::IsClockCorrectionEnabled()
{
#ifdef SWDS
	return false;
#else

	bool bIsMultiplayer = NET_IsMultiplayer();
	// Assume we always want it in multiplayer
	bool bWantsClockDriftMgr = bIsMultiplayer;
	// If we're a multiplayer listen server, we can back off of that if we have zero latency (faked or due to sockets)
	if ( bIsMultiplayer )
	{
		bool bIsListenServer = sv.IsActive();
		bool bLocalConnectionHasZeroLatency = ( NET_GetFakeLag() <= 0.0f ) && !net_usesocketsforloopback.GetBool();

		if ( bIsListenServer && bLocalConnectionHasZeroLatency )
		{
			bWantsClockDriftMgr = false;
		}
	}

	// Only in multi-threaded client/server OR in multi player, but don't use it if we're the listen server w/ no fake lag
	return cl_clock_correction.GetInt() && 
		( IsEngineThreaded() ||	bWantsClockDriftMgr );		
#endif
}
