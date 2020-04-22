//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// A non-trivial number of Valve customers hit performance problems because their CPUs overheat
// and are thermally throttled. While thermal throttling is better than melting it is still a
// hardware flaw and it leads to a bad user experience. In some cases the CPU frequency drops
// (constantly or occasionally) by 50-75%, leading to equal or greater framerate drops.
// 
// This is equivalent to a car that goes into limp-home mode to let it continue running after the
// radiator fails -- it's better than destroying the engine, but clearly it needs to be fixed.
// 
// When CPU monitoring is enabled a bunch of background threads are created that wake up at
// the set frequency, spin in a loop to measure the actual usable CPU frequency, then sleep again.
// A delay loop is used to measure the frequency because this is portable (it works for Intel
// and AMD and handles both frequency throttling and duty-cycle reductions) and it doesn't
// require administrator privileges. This technique has been used in VTrace for a while.
//
// This code doesn't use normal worker threads because of the special purpose nature of this
// work. The threads are started on demand and are never terminated, in order to simplify
// the code.
//
//===============================================================================

#include "pch_tier0.h"
#include "tier0/cpumonitoring.h"

#ifdef PLATFORM_WINDOWS_PC32
#include "tier0/threadtools.h"
#define NOMINMAX
#undef min
#undef max
#include <windows.h>
#include "PowrProf.h"
#include <algorithm>
#pragma comment(lib, "PowrProf.lib")

// This lock protects s_results and s_nDelayMilliseconds.
static CThreadMutex s_lock;
static CPUFrequencyResults s_results;
static unsigned s_nDelayMilliseconds;
// Has monitoring been enabled? If not measurements may still continue
// if kDelayMillisecondsWhenDisabled is non-zero.
static bool s_fEnabled = false;

// This is the delay between measurements when measurements are 'disabled'. If it
// is zero then the measurements are truly disabled.
const unsigned kDelayMillisecondsWhenDisabled = 0; //5000;
// Delay before first measurement
const unsigned kFirstInterval = 500;
const unsigned kPostMeasureInterval = 5;
const unsigned kMinimumDelay = 300;

const int nMaxCPUs = 32;

// This loop spins spinCount times and should take about 50 times spinCount
// cycles to execute. This should be true on any reasonable modern processor
// since the latency of integer add is almost always one cycle.
// The Xbox 360 and PS3 CPUs are the one known exception but this code will
// never run on them.
static void SpinALot( int spinCount )
{
	__asm
	{
		mov ecx, spinCount
start:
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax

		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax

		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax

		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax

		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax
		add eax, eax

		sub ecx,1
		jne start
	}
}

static LARGE_INTEGER s_QPCfrequency;
static LARGE_INTEGER s_QPCbase;

static void InitializeGetTime()
{
	QueryPerformanceFrequency( &s_QPCfrequency );
	QueryPerformanceCounter( &s_QPCbase );
}

static double GetTime()
{
	LARGE_INTEGER value;
	QueryPerformanceCounter( &value );

	// Subtracting off the base time gives us a zero point at application start up and
	// gives us more precision.
	return ( value.QuadPart - s_QPCbase.QuadPart ) / double( s_QPCfrequency.QuadPart );
}

static float GetFrequency()
{
	double start = GetTime();
	// This should cause a delay of 500,000 cycles (50 * spinCount) which should be a
	// fraction of a millisecond on any reasonable processor, thus ensuring that the
	// sampling interrupt will not be hit too frequently.
	SpinALot( 10000 );
	double elapsed = GetTime() - start;
	double frequency = ( 500000 / elapsed ) / 1e9;
	return (float)frequency;
}

// This semaphore is used to release all of the measurement threads simultaneously.
static HANDLE g_releaseSemaphore;
// This semaphore is used to wait for all of the measurement threads to complete.
static HANDLE g_workCompleteSemaphore;
static DWORD g_numCPUs;

// This function measures the CPU frequency by doing repeated integer adds.
// It measures it multiple times and records the highest frequency -- the
// assumption is that any given test might be slowed by interrupts or
// context switches so the fastest run should indicate the true performance.
static float GetSampledFrequency( int iterations )
{
	float maxFrequency = 0.0;
	for ( int i = 0; i < iterations; ++i )
	{
		float frequency = GetFrequency();
		if ( frequency > maxFrequency )
			maxFrequency = frequency;
	}

	return maxFrequency;
}

// The measured frequency of all of the threads
static float s_frequencies[ nMaxCPUs ];

// Measurement thread, designed to be one per core.
static DWORD WINAPI MeasureThread( LPVOID vThreadNum )
{
	ThreadSetDebugName( "CPUMonitoringMeasureThread" );
	int threadNum = (int)vThreadNum;
	for ( ; ; )
	{
		// Wait until the MCP says it's time to wake up and measure CPU speed
		WaitForSingleObject( g_releaseSemaphore, INFINITE );

		// Seven seems like a good number of times to measure the frequency -- it makes
		// it likely that a couple of the tests will not hit any interrupts.
		float frequency = GetSampledFrequency( 7 );
		s_frequencies[ threadNum ] = frequency;

		// Tell the heartbeat thread that one thread has completed.
		ReleaseSemaphore( g_workCompleteSemaphore, 1, NULL );
	}

	// This will never be hit.
	return 0;
}

/*
Note that this structure definition was accidentally omitted from WinNT.h. This error will be corrected in the future. In the meantime, to compile your application, include the structure definition contained in this topic in your source code.
*/
typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

// Master control thread to periodically wake the measurement threads.
static DWORD WINAPI HeartbeatThread( LPVOID )
{
	ThreadSetDebugName( "CPUMonitoringHeartbeatThread" );
	// Arbitrary/hacky time to wait for results to become available.
	Sleep( kFirstInterval );
	for ( ; ; )
	{
		unsigned delay;
		{
			// Read and write all the state that is shared with the main thread while holding the lock.
			AUTO_LOCK( s_lock );
			delay = s_nDelayMilliseconds;
		}

		// If monitoring is currently enabled then do the work.
		if ( delay )
		{
			// First ask Windows what the processor speed is -- this *might* reflect
			// some types of thermal throttling, but doesn't seem to.
			PROCESSOR_POWER_INFORMATION processorInfo[ nMaxCPUs ] = {};
			CallNtPowerInformation( ProcessorInformation, NULL, 0, &processorInfo, sizeof(processorInfo[0]) * g_numCPUs );

			ULONG MaxMHz = processorInfo[ 0 ].MaxMhz;
			ULONG LimitMHz = processorInfo[ 0 ].MhzLimit;
			ULONG MinCurrentMHz = processorInfo[ 0 ].CurrentMhz;
			ULONG MaxCurrentMHz = processorInfo[ 0 ].CurrentMhz;
			for ( DWORD i = 0; i < g_numCPUs; ++i )
			{
				MinCurrentMHz = std::min( MinCurrentMHz, processorInfo[ i ].CurrentMhz );
				MaxCurrentMHz = std::max( MaxCurrentMHz, processorInfo[ i ].CurrentMhz );
				MaxMHz = std::max( MaxMHz, processorInfo[ i ].MaxMhz );
				LimitMHz = std::max( LimitMHz, processorInfo[ i ].MhzLimit );
			}

			// This will wake up all of the worker threads. It is possible that some of the
			// threads will take a long time to wake up in which case the same thread might
			// wake up multiple times but this should be harmless.
			ReleaseSemaphore( g_releaseSemaphore, g_numCPUs, NULL );

			// Wait until all of the measurement threads should have run.
			// This is just to avoid having the heartbeat thread fighting for cycles
			// but isn't strictly necessary.
			Sleep( kPostMeasureInterval );

			// Wait for all of the worker threads to finish.
			for ( DWORD i = 0; i < g_numCPUs; ++i )
			{
				WaitForSingleObject( g_workCompleteSemaphore, INFINITE );
			}

			// Find the minimum and maximum measured frequencies.
			float minActualFreq = s_frequencies[ 0 ];
			float maxActualFreq = s_frequencies[ 0 ];
			for ( DWORD i = 1; i < g_numCPUs; ++i )
			{
				minActualFreq = std::min( minActualFreq, s_frequencies[ i ] );
				maxActualFreq = std::max( maxActualFreq, s_frequencies[ i ] );
			}

			{
				// Read and write all the state that is shared with the main thread while holding the lock.
				AUTO_LOCK( s_lock );
				float freqPercentage = maxActualFreq / (MaxCurrentMHz * 1e-5f);
				const float kFudgeFactor = 1.03f;	// Make results match reality better
				s_results.m_timeStamp = Plat_FloatTime();
				s_results.m_GHz = maxActualFreq * kFudgeFactor;
				s_results.m_percentage = freqPercentage * kFudgeFactor;

				if ( s_results.m_lowestPercentage == 0 || s_results.m_percentage < s_results.m_lowestPercentage )
					s_results.m_lowestPercentage = s_results.m_percentage;
				// delay may get set to zero at this point
				delay = s_nDelayMilliseconds;
			}

			Sleep( delay );
		}
		else
		{
			// If there is nothing to do then just sleep for a bit.
			Sleep( kMinimumDelay );
		}
	}

	// This will never be hit.
	return 0;
}

PLATFORM_INTERFACE CPUFrequencyResults GetCPUFrequencyResults( bool fGetDisabledResults )
{
	AUTO_LOCK( s_lock );
	if ( s_fEnabled || fGetDisabledResults )
	{
		// Return actual results.
		return s_results;
	}
	else
	{
		// Return zero initialized struct.
		return CPUFrequencyResults();
	}
}

PLATFORM_INTERFACE void SetCPUMonitoringInterval( unsigned nDelayMilliseconds )
{
	static bool s_initialized = false;

	// Clamp the delay to a minimum value to save users from running the
	// measurements too frequently.
	if ( nDelayMilliseconds && nDelayMilliseconds <= kMinimumDelay )
		nDelayMilliseconds = kMinimumDelay;

	// If not yet initialized then do one-time thread initialization
	if ( !s_initialized )
	{
		s_initialized = true;

		InitializeGetTime();

		g_releaseSemaphore = CreateSemaphore( NULL, 0, 1000, NULL );
		if ( !g_releaseSemaphore )
			return;
		g_workCompleteSemaphore = CreateSemaphore( NULL, 0, 1000, NULL );
		if ( !g_workCompleteSemaphore )
			return;

		SYSTEM_INFO systemInfo;
		GetSystemInfo( &systemInfo );

		g_numCPUs = systemInfo.dwNumberOfProcessors;
		if ( g_numCPUs > nMaxCPUs )
			g_numCPUs = nMaxCPUs;

		// Create n threads, affinitize them, and set them to high priority. This will (mostly)
		// ensure that they will run promptly on a specific CPU.
		for ( DWORD i = 0; i < g_numCPUs; ++i )
		{
			HANDLE thread = CreateThread( NULL, 0x10000, MeasureThread, (void*)i, 0, NULL );
			SetThreadAffinityMask( thread, 1u << i );
			SetThreadPriority( thread, THREAD_PRIORITY_HIGHEST );
		}

		// Create the thread which tells the measurement threads to wake up periodically
		CreateThread( NULL, 0x10000, HeartbeatThread, NULL, 0, NULL );
	}

	AUTO_LOCK( s_lock );
	if ( nDelayMilliseconds && s_nDelayMilliseconds == 0 )
	{
		// If we are enabling/re-enabling then reset the stats.
		memset( &s_results, 0, sizeof(s_results) );
	}
	// Set the specified delay time or 5,000 if it is disabled.
	s_nDelayMilliseconds = nDelayMilliseconds ? nDelayMilliseconds : kDelayMillisecondsWhenDisabled;
	s_fEnabled = nDelayMilliseconds != 0;
}

class CPUMonitoringStarter
{
public:
	CPUMonitoringStarter()
	{
		// Start up the disabled CPU monitoring at low frequency.
		if ( kDelayMillisecondsWhenDisabled )
			SetCPUMonitoringInterval( 0 );
	}
} s_CPUMonitoringStarter;

#else
PLATFORM_INTERFACE CPUFrequencyResults GetCPUFrequencyResults(bool)
{
	// Return zero initialized results which means no data available.
	CPUFrequencyResults results = {};
	return results;
}

PLATFORM_INTERFACE void SetCPUMonitoringInterval( unsigned nDelayMilliseconds )
{
	NOTE_UNUSED( nDelayMilliseconds );
}

#endif
