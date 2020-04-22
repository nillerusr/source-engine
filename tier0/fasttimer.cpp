//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#include <stdio.h>
#include "tier0/fasttimer.h"

// g_dwClockSpeed is only exported for backwards compatibility.
PLATFORM_INTERFACE unsigned long g_dwClockSpeed;

uint64 g_ClockSpeed;	// Clocks/sec
// Storing CPU clock speed in a 32-bit variable is dangerous and can already overflow
// on some CPUs. This variable is deprecated.
unsigned long g_dwClockSpeed;
#if defined( _X360 ) && defined( _CERT )
unsigned long g_dwFakeFastCounter;
#endif
double g_ClockSpeedMicrosecondsMultiplier;
double g_ClockSpeedMillisecondsMultiplier;
double g_ClockSpeedSecondsMultiplier;

// Constructor init the clock speed.
CClockSpeedInit g_ClockSpeedInit CONSTRUCT_EARLY;

void CClockSpeedInit::Init()
{
	const CPUInformation& cpuinfo = *GetCPUInformation();

	g_ClockSpeed = cpuinfo.m_Speed;

	// cycle counter runs as doc'd at 1/64 Xbox 3.2GHz clock speed, thus 50 Mhz
	if ( IsX360() )
	{
		g_ClockSpeed /= 64L;
	}

	// Avoid integer overflow when writing to g_dwClockSpeed
	if ( g_ClockSpeed <= ULONG_MAX )
		g_dwClockSpeed = (unsigned long)g_ClockSpeed;
	else
		g_dwClockSpeed = ULONG_MAX;

	g_ClockSpeedMicrosecondsMultiplier = 1000000.0 / (double)g_ClockSpeed;
	g_ClockSpeedMillisecondsMultiplier = 1000.0 / (double)g_ClockSpeed;
	g_ClockSpeedSecondsMultiplier = 1.0 / (double)g_ClockSpeed;
}
