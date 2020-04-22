//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include "framefunction.h"
#include "gclogger.h"
#include "gcsdk/scheduledfunction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//-------------------------------------------------------------------------
CBaseFrameFunction::CBaseFrameFunction( const char *pchName, EFrameType eFrameType ) :
	m_sName( pchName ),
	m_EFrameType( eFrameType ),
	m_nNumCalls( 0 ),
	m_nTrackedTime( 0 ),
	m_bRegistered( false )
{
}

//-------------------------------------------------------------------------
CBaseFrameFunction::~CBaseFrameFunction()
{
	Deregister();
}

//called to handle registering for updates and unregistering. Not registered by default
void CBaseFrameFunction::Register()
{
	if( !m_bRegistered )
	{
		GFrameFunctionMgr().Register( this );
		m_bRegistered = true;
	}
}

void CBaseFrameFunction::Deregister()
{
	if( m_bRegistered )
	{
		GFrameFunctionMgr().Deregister( this );
		m_bRegistered = false;
	}
}


//-------------------------------------------------------------------------
CFrameFunctionMgr::CFrameFunctionMgr() :
	m_nNumProfileFrames( 0 ),
	m_bCompletedHighPri( false )
{
}

//-------------------------------------------------------------------------
void CFrameFunctionMgr::Register( CBaseFrameFunction* pFrameFunc )
{
	if( !pFrameFunc )
		return;

	//see which type this frame function is
	CBaseFrameFunction::EFrameType eType = pFrameFunc->m_EFrameType;
	if( eType >= CBaseFrameFunction::k_EFrameType_Count )
		return;

	//don't allow for duplicates
	if( m_MainLoopFrameFuncs[ eType ].Find( pFrameFunc ) == m_MainLoopFrameFuncs[ eType ].InvalidIndex() )
	{
		m_MainLoopFrameFuncs[ eType ].AddToTail( pFrameFunc );
	}
}

//-------------------------------------------------------------------------
void CFrameFunctionMgr::Deregister( CBaseFrameFunction* pFrameFunc )
{
	if( !pFrameFunc )
		return;

	//see which type this frame function is
	CBaseFrameFunction::EFrameType eType = pFrameFunc->m_EFrameType;
	if( eType >= CBaseFrameFunction::k_EFrameType_Count )
		return;

	m_MainLoopFrameFuncs[ eType ].FindAndRemove( pFrameFunc );
}


//-------------------------------------------------------------------------
void CFrameFunctionMgr::RunFrame( const CLimitTimer& limitTimer )
{
	VPROF_BUDGET( "CFrameFunctionMgr::RunFrame", VPROF_BUDGETGROUP_STEAM );

	//track the number of frames we've profiled
	m_nNumProfileFrames++;
	m_bCompletedHighPri = false;
	
	//run through each of our 'once a frame' updates
	RunFrameList( CBaseFrameFunction::k_EFrameType_RunOnce, limitTimer );
}

//-------------------------------------------------------------------------
bool CFrameFunctionMgr::RunFrameTick( const CLimitTimer& limitTimer )
{
	VPROF_BUDGET( "CFrameFunctionMgr::RunFrameTick", VPROF_BUDGETGROUP_STEAM );

	//run high priority if we haven't finished it yet
	if( !m_bCompletedHighPri )
	{
		if( RunFrameList( CBaseFrameFunction::k_EFrameType_RunUntilCompleted, limitTimer ) )
		{
			//we need to update another frame
			return true;
		}
		else
		{
			//we have completed high priority
			m_bCompletedHighPri = true;
		}	
	}

	//if we are still here, we have completed high priority, so run until we are done with low priority
	return RunFrameList( CBaseFrameFunction::k_EFrameType_RunUntilCompletedLowPri, limitTimer );
}

//-------------------------------------------------------------------------
bool CFrameFunctionMgr::RunFrameList( CBaseFrameFunction::EFrameType eType, const CLimitTimer& limitTimer )
{
	bool bResult = false;

	//run through each of our 'once a frame' updates
	FOR_EACH_VEC( m_MainLoopFrameFuncs[ eType ], nCurrFunc )
	{
		CBaseFrameFunction* pFunc = m_MainLoopFrameFuncs[ eType ][ nCurrFunc ];

		uint64 nTimeStart = limitTimer.CMicroSecLeft();

		{
			VPROF_BUDGET( pFunc->m_sName.Get(), VPROF_BUDGETGROUP_STEAM );
			bResult |= pFunc->BRun( limitTimer );
		}

		//track the time spent in this function
		pFunc->m_nNumCalls++;
		pFunc->m_nTrackedTime += nTimeStart - limitTimer.CMicroSecLeft();
	}

	return bResult;
}

//-------------------------------------------------------------------------

bool CFrameFunctionMgr::SortFrameFuncByTime( const CBaseFrameFunction* pLhs, const CBaseFrameFunction* pRhs )
{
	//sort by time first, then name if the same time (unlikely)
	if( pLhs->m_nTrackedTime != pRhs->m_nTrackedTime )
		return pLhs->m_nTrackedTime > pRhs->m_nTrackedTime;
	return stricmp( pLhs->m_sName.Get(), pRhs->m_sName.Get() ) < 0;
}

void CFrameFunctionMgr::DumpProfile()
{
	//collect all of our functions and sort them based upon time elapsed
	CUtlVector< CBaseFrameFunction* > FrameFuncs;
	uint64 nTotalTime = 0;
	uint32 nTotalCalls = 0;

	for( uint32 nCurrType = 0; nCurrType < CBaseFrameFunction::k_EFrameType_Count; nCurrType++ )
	{
		FOR_EACH_VEC (m_MainLoopFrameFuncs[ nCurrType ], nCurrFunc )
		{
			CBaseFrameFunction* pFunc = m_MainLoopFrameFuncs[ nCurrType ][ nCurrFunc ];
			FrameFuncs.AddToTail( pFunc );
			nTotalTime += pFunc->m_nTrackedTime;
			nTotalCalls += pFunc->m_nNumCalls;
		}
	}

	double fInvFrame = ( m_nNumProfileFrames > 0 ) ? 1.0 / m_nNumProfileFrames : 1.0;

	std::sort( FrameFuncs.begin(), FrameFuncs.end(), SortFrameFuncByTime );

	//now dump out the timings in a grid
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "Name                                         Time(ms)      Calls  Time%     Time/F    Calls/F\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---------------------------------------- ------------ ---------- ------ ---------- ----------\n" );

	//print out each API
	FOR_EACH_VEC( FrameFuncs, nFunc )
	{
		CBaseFrameFunction* pFunc = FrameFuncs[ nFunc ];
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-40s %12.2f %10d %5.1f%% %10.3f %10.1f\n",
			pFunc->m_sName.Get(), pFunc->m_nTrackedTime / 1000.0, pFunc->m_nNumCalls, pFunc->m_nTrackedTime / ( double )nTotalTime, pFunc->m_nTrackedTime * fInvFrame / 1000.0,  pFunc->m_nNumCalls * fInvFrame );
	}

	//print out the footer and totals
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---------------------------------------- ------------ ---------- ------ ---------- ----------\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-40s %12.2f %10d %5.1f%% %10.3f %10.1f\n",
		"Totals", nTotalTime / 1000.0, nTotalCalls, 100.0, nTotalTime * fInvFrame / 1000.0, nTotalCalls * fInvFrame );
}

//-------------------------------------------------------------------------
void CFrameFunctionMgr::ClearProfile()
{
	//run through all of our functions and clear timings
	for( uint32 nCurrType = 0; nCurrType < CBaseFrameFunction::k_EFrameType_Count; nCurrType++ )
	{
		FOR_EACH_VEC (m_MainLoopFrameFuncs[ nCurrType ], nCurrFunc )
		{
			CBaseFrameFunction* pFunc = m_MainLoopFrameFuncs[ nCurrType ][ nCurrFunc ];
			pFunc->m_nTrackedTime = 0;
			pFunc->m_nNumCalls = 0;
		}
	}

	m_nNumProfileFrames = 0;
}

//-------------------------------------------------------------------------
CFrameFunctionMgr& GFrameFunctionMgr()
{
	static CFrameFunctionMgr s_Singleton;
	return s_Singleton;	
}

//-------------------------------------------------------------------------

//utility class for dumping out the profile results after time has expired
static void DumpFrameFunctionProfile()
{
	GFrameFunctionMgr().DumpProfile();
}

GC_CON_COMMAND( framefunc_profile, "Turns on frame function profiling for N seconds and dumps the results" )
{
	if( args.ArgC() < 2 )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Proper usage is framefunc_profile <n> where n is the number of seconds to profile for\n" );
		return;
	}

	float fSeconds = MAX( 1.0f, atof( args[ 1 ] ) );
	GFrameFunctionMgr().ClearProfile();
	static CGlobalScheduledFunction s_DumpProfile;
	s_DumpProfile.ScheduleMS( DumpFrameFunctionProfile, fSeconds * 1000.0f );
}

} //namespace GCSDK
