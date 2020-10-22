//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: determine CPU speed under linux
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef LINUX
#include <linux/sysctl.h>
#else
#include <sys/sysctl.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <tier0/platform.h>
#include <errno.h>

#define rdtsc(x) \
	__asm__ __volatile__ ("rdtsc" : "=A" (x))

class TimeVal
{
public:
	TimeVal() {}
	TimeVal& operator=(const TimeVal &val) { m_TimeVal = val.m_TimeVal; return *this; } 
	inline double operator-(const TimeVal &left)
	{
		uint64 left_us = (uint64) left.m_TimeVal.tv_sec * 1000000 + left.m_TimeVal.tv_usec;
		uint64 right_us = (uint64) m_TimeVal.tv_sec * 1000000 + m_TimeVal.tv_usec;
		uint64 diff_us = right_us - left_us;
		return diff_us * ( 1.0 / 1000000.0 );
	}

	timeval m_TimeVal;
};

// Compute the positive difference between two 64 bit numbers.
static inline uint64 diff(uint64 v1, uint64 v2)
{
	int64 d = v1 - v2;
	if (d >= 0)
		return d;
	else
		return -d;
}

#ifdef OSX

// Mac
uint64 GetCPUFreqFromPROC()
{
	int mib[2] = {CTL_HW, HW_CPU_FREQ};
	uint64 frequency = 0;
	size_t len = sizeof(frequency);

	if (sysctl(mib, 2, &frequency, &len, NULL, 0) == -1)
		return 0;
	return frequency;
}

#else

// Linux
uint64 GetCPUFreqFromPROC()
{
	double mhz = 0;
	char line[1024], *s, search_str[] = "cpu MHz";

	/* open proc/cpuinfo */
	FILE *fp = fopen( "/proc/cpuinfo", "r" );
	if (fp == NULL)
	{
		return 0;
	}

	/* ignore all lines until we reach MHz information */
	while (fgets(line, 1024, fp) != NULL) 
	{ 
		if (strstr(line, search_str) != NULL) 
		{
			/* ignore all characters in line up to : */
			for (s = line; *s && (*s != ':'); ++s)
				;

			/* get MHz number */
			if ( *s && ( sscanf( s + 1, "%lf", &mhz) == 1 ) )
				break;
		}
    }

    fclose(fp);

    return ( uint64 )( mhz * 1000000 );
}

#endif

uint64 CalculateCPUFreq()
{
#ifdef LINUX
	char const *pFreq = getenv( "CPU_MHZ" );
	if ( pFreq )
	{
		uint64 retVal = 1000000;
		return retVal * atoi( pFreq );
	}
#endif

	// Try to open cpuinfo_max_freq. If the kernel was built with cpu scaling support disabled, this will fail.
	FILE *fp = fopen( "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r" );
	if ( fp )
	{
		char buf[ 256 ];
		uint64 retVal = 0;

		buf[ 0 ] = 0;
		if( fread( buf, 1, ARRAYSIZE( buf ), fp ) )
		{
			retVal = ( uint64 )atoll( buf );
		}
		fclose(fp);

		if( retVal )
		{
			return retVal * 1000;
		}
	}

	// Compute the period. Loop until we get 3 consecutive periods that
	// are the same to within a small error. The error is chosen
	// to be +/- 0.02% on a P-200.
	const uint64 error = 40000;
	const int max_iterations = 600;
	int count;
	uint64 period, period1 = error * 2, period2 = 0,  period3 = 0;

	for (count = 0; count < max_iterations; count++)
	{
		TimeVal start_time, end_time;
		uint64 start_tsc, end_tsc;

		gettimeofday( &start_time.m_TimeVal, 0 );
		rdtsc( start_tsc );
		usleep( 5000 ); // sleep for 5 msec
		gettimeofday( &end_time.m_TimeVal, 0 );
		rdtsc( end_tsc );
	
		// end_time - start_time calls into the overloaded TimeVal operator- way above, and returns a double.
		period3 = ( end_tsc - start_tsc ) / ( end_time - start_time );

		if (diff ( period1, period2 ) <= error &&
			diff ( period2, period3 ) <= error &&
			diff ( period1, period3 ) <= error )
		{
			break;
		}

		period1 = period2;
		period2 = period3;
    }

	if ( count == max_iterations )
    {
		return GetCPUFreqFromPROC(); // fall back to /proc
    }

	// Set the period to the average period measured.
	period = ( period1 + period2 + period3 ) / 3;

	// Some Pentiums have broken TSCs that increment very
	// slowly or unevenly. 
	if (period < 10000000)
	{
		return GetCPUFreqFromPROC(); // fall back to /proc
	}

	return period;
}

