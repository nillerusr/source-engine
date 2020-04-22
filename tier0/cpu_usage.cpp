//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: return the cpu usage as a float value
//
// On win32 this is 0.0 to 1.0 indicating the amount of CPU time used
// On posix its the load avg from the last minute
// 
// On win32 you need to call this once in a while.  Every few seconds.
// First call returns zero
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/platform.h"

#ifdef WIN32

#include <windows.h>

#define SystemBasicInformation 0
#define SystemPerformanceInformation 2
#define SystemTimeInformation 3

#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

typedef struct
{
	DWORD dwUnknown1;
	ULONG uKeMaximumIncrement;
	ULONG uPageSize;
	ULONG uMmNumberOfPhysicalPages;
	ULONG uMmLowestPhysicalPage;
	ULONG uMmHighestPhysicalPage;
	ULONG uAllocationGranularity;
	PVOID pLowestUserAddress;
	PVOID pMmHighestUserAddress;
	ULONG uKeActiveProcessors;
	BYTE bKeNumberProcessors;
	BYTE bUnknown2;
	WORD wUnknown3;
} SYSTEM_BASIC_INFORMATION;

typedef struct
{
	LARGE_INTEGER liIdleTime;
	DWORD dwSpare[80];
} SYSTEM_PERFORMANCE_INFORMATION;

typedef struct
{
	LARGE_INTEGER liKeBootTime;
	LARGE_INTEGER liKeSystemTime;
	LARGE_INTEGER liExpTimeZoneBias;
	ULONG uCurrentTimeZoneId;
	DWORD dwReserved;
} SYSTEM_TIME_INFORMATION;

typedef LONG (WINAPI *PROCNTQSI)(UINT,PVOID,ULONG,PULONG);

static PROCNTQSI NtQuerySystemInformation;

float GetCPUUsage() 
{
	SYSTEM_PERFORMANCE_INFORMATION SysPerfInfo;
	SYSTEM_TIME_INFORMATION SysTimeInfo;
	SYSTEM_BASIC_INFORMATION SysBaseInfo;
	double dbIdleTime;
	double dbSystemTime;
	LONG status;
	static LARGE_INTEGER liOldIdleTime = {0,0};
	static LARGE_INTEGER liOldSystemTime = {0,0};

	if ( !NtQuerySystemInformation)
	{
		NtQuerySystemInformation = (PROCNTQSI)GetProcAddress( GetModuleHandle("ntdll"), "NtQuerySystemInformation" );

		if ( !NtQuerySystemInformation )
			return(0);
	}

	// get number of processors in the system
	status = NtQuerySystemInformation( SystemBasicInformation,&SysBaseInfo,sizeof(SysBaseInfo),NULL );
	if ( status != NO_ERROR )
		return(0);

	// get new system time
	status = NtQuerySystemInformation( SystemTimeInformation,&SysTimeInfo,sizeof(SysTimeInfo),0 );
	if ( status!=NO_ERROR )
		return(0);

	// get new CPU's idle time
	status = NtQuerySystemInformation( SystemPerformanceInformation,&SysPerfInfo,sizeof(SysPerfInfo),NULL );
	if ( status != NO_ERROR )
		return(0);

	// if it's a first call - skip it
	if ( liOldIdleTime.QuadPart != 0 )
	{
		// CurrentValue = NewValue - OldValue
		dbIdleTime = Li2Double(SysPerfInfo.liIdleTime) - Li2Double(liOldIdleTime);
		dbSystemTime = Li2Double(SysTimeInfo.liKeSystemTime) - Li2Double(liOldSystemTime);

		// CurrentCpuIdle = IdleTime / SystemTime
		dbIdleTime = dbIdleTime / dbSystemTime / (double)SysBaseInfo.bKeNumberProcessors;

		// CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors
		// dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)SysBaseInfo.bKeNumberProcessors + 0.5;
	}
	else
	{
		dbIdleTime = 1.0f;
	}

	// store new CPU's idle and system time
	liOldIdleTime = SysPerfInfo.liIdleTime;
	liOldSystemTime = SysTimeInfo.liKeSystemTime;

	return (float)(1.0f - dbIdleTime);
}

#endif // WIN32

#ifdef POSIX
#include <stdlib.h>

float GetCPUUsage() 
{
	double loadavg[3];

	getloadavg( loadavg, 3 );
	return loadavg[0];
}
#endif //POSIX
