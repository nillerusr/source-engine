//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>

static double beginTime;


double I_FloatTime( void )
{
	static double freq = 0.0;
	static __int64 firstCount;
	__int64	curCount;

	if (freq == 0.0)
	{
		__int64 perfFreq;
		QueryPerformanceFrequency( (LARGE_INTEGER*)&perfFreq );
		QueryPerformanceCounter( (LARGE_INTEGER*)&firstCount );
		freq = 1.0 / (double)perfFreq;
	}

	QueryPerformanceCounter ( (LARGE_INTEGER*)&curCount );
	curCount -= firstCount;
	double time = (double)curCount * freq;
	return time;	
}


void I_BeginTime( void )
{
	beginTime = I_FloatTime();
}


double I_EndTime( void )
{
	return ( I_FloatTime() - beginTime );
}
