//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PERF_COUNTERS_H
#define PERF_COUNTERS_H
#ifdef _WIN32
#pragma once
#endif


class IPerfTracker
{
public:
	virtual void Init( unsigned long dwProcessID ) = 0;
	virtual void Release() = 0;
	
	virtual unsigned long GetProcessID() = 0;
	virtual void GetPerfData( int &processorPercentage, int &memoryUsageMegabytes ) = 0;
};


IPerfTracker* CreatePerfTracker();


#endif // PERF_COUNTERS_H
