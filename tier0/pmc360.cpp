//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Inner workings of Performance Monitor Counters on the xbox 360; 
// they let vprof track L2 dcache misses, LHS, etc.
//
//=============================================================================//

#include "pch_tier0.h"

#ifndef _X360
#error pmc360.cpp must only be compiled for XBOX360! 
#else

#include "tier0/platform.h"
#include "tier0/vprof.h"
#include <pmcpbsetup.h>
#include "tier0/dbg.h"
#include "pmc360.h"

#include "tier0/memdbgon.h"

static bool s_bInitialized = false;

CPMCData::CPMCData() 
{
}

void CPMCData::InitializeOnceProgramWide( void )
{
#if !defined( _CERT )
	// Select a set of sixteen counters
	DmPMCInstallAndStart( PMC_SETUP_FLUSHREASONS_PB0T0 );
	// Reset the Performance Monitor Counters in preparation for a new sampling run.
	DmPMCResetCounters();
#endif
	s_bInitialized = true;
}

bool CPMCData::IsInitialized()
{
	return s_bInitialized;
}

void CPMCData::Start()
{
#if !defined( _CERT )
	// stop the stopwatches, save off the counter, start them again.
	DmPMCStop();

	PMCState pmcstate;
	// Get the counters.
	DmPMCGetCounters( &pmcstate );

	// in the default state as set up by InitializeOnceProgramWide, 
	// counters 9 and 6 are L2 misses and LHS respectively
	m_OnStart.L2CacheMiss = pmcstate.pmc[9];
	m_OnStart.LHS = pmcstate.pmc[6];

	DmPMCStart();
#endif
}

void CPMCData::End()
{
#if !defined( _CERT )
	DmPMCStop();

	// get end-state counters
	PMCState pmcstate;
	// Get the counters.
	DmPMCGetCounters( &pmcstate );

	// in the default state as set up by InitializeOnceProgramWide, 
	// counters 9 and 6 are l2 misses and LHS respectively
	const uint64 &endL2 = pmcstate.pmc[9];
	const uint64 &endLHS = pmcstate.pmc[6];

	// compute delta between end and start. Because these are 
	// unsigned nums, even in overflow this still works out
	// correctly under modular arithmetic.
	m_Delta.L2CacheMiss = endL2 - m_OnStart.L2CacheMiss;
	m_Delta.LHS = endLHS - m_OnStart.LHS;

	DmPMCStart();
#endif
}

#endif
