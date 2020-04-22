//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "tier1/utldict.h"
#include <pdh.h>
#include <pdhmsg.h>
#include "perf_counters.h"


#if 1 

class CPerfTracker : public IPerfTracker
{
public:

	CPerfTracker()
	{
		m_hProcessorTimeCounter = NULL;
		m_dwProcessID = 0;
		if ( PdhOpenQuery( NULL, 0, &m_hQuery ) != ERROR_SUCCESS )
			m_hQuery = NULL;

		SYSTEM_INFO info;
		GetSystemInfo( &info );
		m_nProcessors = (int)info.dwNumberOfProcessors;
	}
	
	~CPerfTracker()
	{
		if ( m_hQuery )
			PdhCloseQuery( m_hQuery );
	}
	
	virtual void Init( unsigned long dwProcessID )
	{
		Term();
		
		m_dwProcessID = dwProcessID;
		
		char instanceName[512];
		if ( GetInstanceNameFromProcessID( m_dwProcessID, instanceName, sizeof( instanceName ) ) )
		{
			// Create a counter to watch this process' time.
			char str[512];
			V_snprintf( str, sizeof( str ), "\\Process(%s)\\%% Processor Time", instanceName );
			if ( PdhAddCounter( m_hQuery, str, 0, &m_hProcessorTimeCounter ) != ERROR_SUCCESS )
			{
				m_hProcessorTimeCounter = NULL;
			}
			
			V_snprintf( str, sizeof( str ), "\\Process(%s)\\Private Bytes", instanceName );
			if ( PdhAddCounter( m_hQuery, str, 0, &m_hPrivateBytesCounter ) != ERROR_SUCCESS )
			{
				m_hPrivateBytesCounter = NULL;
			}
		}
	}
	
	void Term()
	{
		if ( m_hProcessorTimeCounter )
			PdhRemoveCounter( m_hProcessorTimeCounter );

		if ( m_hPrivateBytesCounter )
			PdhRemoveCounter( m_hPrivateBytesCounter );
			
		m_hProcessorTimeCounter = NULL;
		m_hPrivateBytesCounter = NULL;
	}
	
	virtual void Release()
	{
		delete this;
	}
	
	virtual unsigned long GetProcessID()
	{
		return m_dwProcessID;
	}
	
	virtual void GetPerfData( int &processorPercentage, int &memoryUsageMegabytes )
	{
		processorPercentage = 101;
		memoryUsageMegabytes = 0;
		
		// Collect query data..
		PDH_STATUS ret = PdhCollectQueryData( m_hQuery );
		if ( ret != ERROR_SUCCESS )
			return;

		// Check processor usage.
		DWORD dwType;
		PDH_FMT_COUNTERVALUE counterValue;
		if ( PdhGetFormattedCounterValue( m_hProcessorTimeCounter, PDH_FMT_LONG | PDH_FMT_NOCAP100, &dwType, &counterValue ) == ERROR_SUCCESS )
			processorPercentage = counterValue.longValue / m_nProcessors;
		else
			processorPercentage = 101;

		// Check memory usage.
		if ( PdhGetFormattedCounterValue( m_hPrivateBytesCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &dwType, &counterValue ) == ERROR_SUCCESS )
			memoryUsageMegabytes = (int)(counterValue.doubleValue / (1024.0 * 1024.0));
		else
			memoryUsageMegabytes = 0;
	}
	

private:

	bool GetInstanceNameFromProcessID( DWORD processID, char *instanceName, int instanceNameLen )
	{
		instanceName[0] = 0;

		bool bRet = false;

		// This refreshes the object list. If we don't do this, it won't get new process IDs correctly.
		DWORD dummy = 0;
		PdhEnumObjects( NULL, NULL, NULL, &dummy, PERF_DETAIL_NOVICE, true );

		// Find out how much data we need.
		DWORD counterListLen=2, instanceListLen=2;
		char *counterList = new char[counterListLen];
		char *instanceList = new char[instanceListLen];
		PDH_STATUS stat = PdhEnumObjectItems( NULL, NULL, "Process", counterList, &counterListLen, instanceList, &instanceListLen, PERF_DETAIL_NOVICE, 0 );
		if ( stat == PDH_MORE_DATA )
		{
			delete [] counterList;
			delete [] instanceList;
			char *counterList = new char[counterListLen];
			char *instanceList = new char[instanceListLen];

			stat = PdhEnumObjectItems( NULL, NULL, "Process", counterList, &counterListLen, instanceList, &instanceListLen, PERF_DETAIL_NOVICE, 0 );
			if ( stat == ERROR_SUCCESS )
			{
				// We need the # of each one..
				CUtlDict<int,int> counts;
				
				// The instance name list is a bunch of strings terminated with nulls. The final one has two nulls after it.
				// Walk through the list and get the process ID associated with each instance name.
				const char *pCur = instanceList;
				while ( *pCur )
				{
					int index = counts.Find( pCur );
					if ( index == counts.InvalidIndex() )
						counts.Insert( pCur, 1 );
					else
						counts[index]++;

					pCur += strlen( pCur ) + 1;
				}

				// Each instance (like "vrad") might have multiple versions, like if you're running multiple vrad processes at the same time.
				for ( int i=counts.First(); i != counts.InvalidIndex(); i=counts.Next( i ) )
				{				
					const char *pInstanceName = counts.GetElementName( i );
					int nInstances = counts[i];
					for ( int iInstance=0; iInstance < nInstances; iInstance++ )
					{
						char testInstanceName[256], fullObjectName[256];
						V_snprintf( testInstanceName, sizeof( testInstanceName ), "%s#%d", pInstanceName, iInstance );
						V_snprintf( fullObjectName, sizeof( fullObjectName ), "\\Process(%s)\\ID Process", testInstanceName );
						
						HCOUNTER hCounter = NULL;
						stat = PdhAddCounter( m_hQuery, fullObjectName, 0, &hCounter );
						if ( stat == ERROR_SUCCESS )
						{
							stat = PdhCollectQueryData( m_hQuery );
							if ( stat == ERROR_SUCCESS )
							{
								DWORD dwType;
								PDH_FMT_COUNTERVALUE counterValue;
								stat = PdhGetFormattedCounterValue( hCounter, PDH_FMT_LONG, &dwType, &counterValue );
								if ( stat == 0 && counterValue.longValue == (long)processID )
								{
									// Finall! We found it.
									V_strncpy( instanceName, testInstanceName, instanceNameLen );
									bRet = true;
									PdhRemoveCounter( hCounter );
									break;
								}
							}		
							
							PdhRemoveCounter( hCounter );
						}
					}
					
					if ( bRet )
						break;
				}
			}
			
			delete [] counterList;
			delete [] instanceList;
		}
		
		return bRet;
	}

private:
	DWORD m_dwProcessID;
	PDH_HQUERY m_hQuery;
	HCOUNTER m_hProcessorTimeCounter;
	HCOUNTER m_hPrivateBytesCounter;
	int m_nProcessors;
};


IPerfTracker* CreatePerfTracker()
{
	return new CPerfTracker;
}


#else

#include <winperf.h>

// --------------------------------------------------------------------------------------------------------------------- //
// NOTE: THIS IS THE OLD, UGLY WAY TO DO IT.
// --------------------------------------------------------------------------------------------------------------------- //

class CPerfTracker
{
public:
	CPerfTracker();
	void Init( unsigned long dwProcessID );

	unsigned long GetProcessID();
	
	// Get the percentage of CPU time that the process is using.
	int GetCPUPercentage();

private:
	DWORD m_dwProcessID;
	LONGLONG m_lnOldValue;
	LARGE_INTEGER m_OldPerfTime100nSec;
	int m_nProcessors;
};

#define TOTALBYTES    100*1024
#define BYTEINCREMENT 10*1024

#define SYSTEM_OBJECT_INDEX					2		// 'System' object
#define PROCESS_OBJECT_INDEX				230		// 'Process' object
#define PROCESSOR_OBJECT_INDEX				238		// 'Processor' object
#define TOTAL_PROCESSOR_TIME_COUNTER_INDEX	240		// '% Total processor time' counter (valid in WinNT under 'System' object)
#define PROCESSOR_TIME_COUNTER_INDEX		6		// '% processor time' counter (for Win2K/XP)


//
//	The performance data is accessed through the registry key 
//	HKEY_PEFORMANCE_DATA.
//	However, although we use the registry to collect performance data, 
//	the data is not stored in the registry database.
//	Instead, calling the registry functions with the HKEY_PEFORMANCE_DATA key 
//	causes the system to collect the data from the appropriate system 
//	object managers.
//
//	QueryPerformanceData allocates memory block for getting the
//	performance data.
//
//
void QueryPerformanceData(PERF_DATA_BLOCK **pPerfData, DWORD dwObjectIndex, DWORD dwCounterIndex)
{
	//
	// Since i want to use the same allocated area for each query,
	// i declare CBuffer as static.
	// The allocated is changed only when RegQueryValueEx return ERROR_MORE_DATA
	//
	static CUtlVector<char> Buffer;
	if ( Buffer.Count() == 0 )
		Buffer.SetSize( TOTALBYTES );

	DWORD BufferSize = Buffer.Count();
	LONG lRes;

	char keyName[32];
	V_snprintf(keyName, sizeof(keyName), "%d",dwObjectIndex);

	memset( Buffer.Base(), 0, Buffer.Count() );
	while( (lRes = RegQueryValueEx( HKEY_PERFORMANCE_DATA,
							   keyName,
							   NULL,
							   NULL,
							   (LPBYTE)Buffer.Base(),
							   &BufferSize )) == ERROR_MORE_DATA )
	{
		// Get a buffer that is big enough.
		BufferSize += BYTEINCREMENT;
		Buffer.SetSize( BufferSize );
	}
	
	*pPerfData = (PPERF_DATA_BLOCK)Buffer.Base();
}


/*****************************************************************
 *                                                               *
 * Functions used to navigate through the performance data.      *
 *                                                               *
 *****************************************************************/

inline PPERF_OBJECT_TYPE FirstObject( PPERF_DATA_BLOCK PerfData )
{
	return( (PPERF_OBJECT_TYPE)((PBYTE)PerfData + PerfData->HeaderLength) );
}

inline PPERF_OBJECT_TYPE NextObject( PPERF_OBJECT_TYPE PerfObj )
{
	return( (PPERF_OBJECT_TYPE)((PBYTE)PerfObj + PerfObj->TotalByteLength) );
}

inline PPERF_COUNTER_DEFINITION FirstCounter( PPERF_OBJECT_TYPE PerfObj )
{
	return( (PPERF_COUNTER_DEFINITION) ((PBYTE)PerfObj + PerfObj->HeaderLength) );
}

inline PPERF_COUNTER_DEFINITION NextCounter( PPERF_COUNTER_DEFINITION PerfCntr )
{
	return( (PPERF_COUNTER_DEFINITION)((PBYTE)PerfCntr + PerfCntr->ByteLength) );
}

inline PPERF_INSTANCE_DEFINITION FirstInstance( PPERF_OBJECT_TYPE PerfObj )
{
	return( (PPERF_INSTANCE_DEFINITION)((PBYTE)PerfObj + PerfObj->DefinitionLength) );
}

inline PPERF_INSTANCE_DEFINITION NextInstance( PPERF_INSTANCE_DEFINITION PerfInst )
{
	PPERF_COUNTER_BLOCK PerfCntrBlk;

	PerfCntrBlk = (PPERF_COUNTER_BLOCK)((PBYTE)PerfInst + PerfInst->ByteLength);

	return( (PPERF_INSTANCE_DEFINITION)((PBYTE)PerfCntrBlk + PerfCntrBlk->ByteLength) );
}


template<class T>
T GetCounterValueForProcessID(PPERF_OBJECT_TYPE pPerfObj, DWORD dwCounterIndex, DWORD dwProcessID)
{
	unsigned long PROC_ID_COUNTER = 784;

	BOOL	bProcessIDExist = FALSE;
	PPERF_COUNTER_DEFINITION pPerfCntr = NULL;
	PPERF_COUNTER_DEFINITION pTheRequestedPerfCntr = NULL;
	PPERF_COUNTER_DEFINITION pProcIDPerfCntr = NULL;
	PPERF_INSTANCE_DEFINITION pPerfInst = NULL;
	PPERF_COUNTER_BLOCK pCounterBlock = NULL;

	// Get the first counter.

	pPerfCntr = FirstCounter( pPerfObj );

	for( DWORD j=0; j < pPerfObj->NumCounters; j++ )
	{
		if (pPerfCntr->CounterNameTitleIndex == PROC_ID_COUNTER)
		{
			pProcIDPerfCntr = pPerfCntr;
			if (pTheRequestedPerfCntr)
				break;
		}

		if (pPerfCntr->CounterNameTitleIndex == dwCounterIndex)
		{
			pTheRequestedPerfCntr = pPerfCntr;
			if (pProcIDPerfCntr)
				break;
		}

		// Get the next counter.

		pPerfCntr = NextCounter( pPerfCntr );
	}

	if( pPerfObj->NumInstances == PERF_NO_INSTANCES )		
	{
		pCounterBlock = (PPERF_COUNTER_BLOCK) ((LPBYTE) pPerfObj + pPerfObj->DefinitionLength);
	}
	else
	{
		pPerfInst = FirstInstance( pPerfObj );
	
		for( int k=0; k < pPerfObj->NumInstances; k++ )
		{
			pCounterBlock = (PPERF_COUNTER_BLOCK) ((LPBYTE) pPerfInst + pPerfInst->ByteLength);
			if (pCounterBlock)
			{
				DWORD processID = *(DWORD*)((LPBYTE) pCounterBlock + pProcIDPerfCntr->CounterOffset);
				if (processID == dwProcessID)
				{
					bProcessIDExist = TRUE;
					break;
				}
			}
			
			// Get the next instance.
			pPerfInst = NextInstance( pPerfInst );
		}
	}

	if (bProcessIDExist && pCounterBlock)
	{
		T *lnValue = NULL;
		lnValue = (T*)((LPBYTE) pCounterBlock + pTheRequestedPerfCntr->CounterOffset);
		return *lnValue;
	}
	return -1;
}


template<class T>
T GetCounterValueForProcessID(PERF_DATA_BLOCK **pPerfData, DWORD dwObjectIndex, DWORD dwCounterIndex, DWORD dwProcessID)
{
	QueryPerformanceData(pPerfData, dwObjectIndex, dwCounterIndex);

    PPERF_OBJECT_TYPE pPerfObj = NULL;
	T lnValue = {0};

	// Get the first object type.
	pPerfObj = FirstObject( *pPerfData );

	// Look for the given object index

	for( DWORD i=0; i < (*pPerfData)->NumObjectTypes; i++ )
	{

		if (pPerfObj->ObjectNameTitleIndex == dwObjectIndex)
		{
			lnValue = GetCounterValueForProcessID<T>(pPerfObj, dwCounterIndex, dwProcessID);
			break;
		}

		pPerfObj = NextObject( pPerfObj );
	}
	return lnValue;
}


// ------------------------------------------------------------------------------------------- //
// CPerfTracker implementation.
// ------------------------------------------------------------------------------------------- //

CPerfTracker::CPerfTracker()
{
	Init( 0 );

	SYSTEM_INFO info;
	GetSystemInfo( &info );
	m_nProcessors = (int)info.dwNumberOfProcessors;
}


void CPerfTracker::Init( unsigned long dwProcessID )
{
	m_dwProcessID = dwProcessID;
	m_lnOldValue = 0;
}


unsigned long CPerfTracker::GetProcessID()
{
	return m_dwProcessID;
}


int CPerfTracker::GetCPUPercentage()
{
	DWORD dwObjectIndex = PROCESS_OBJECT_INDEX;
	DWORD dwCpuUsageIndex = PROCESSOR_TIME_COUNTER_INDEX;

	PPERF_DATA_BLOCK pPerfData = NULL;
	LONGLONG lnNewValue = GetCounterValueForProcessID<LONGLONG>( &pPerfData, dwObjectIndex, dwCpuUsageIndex, m_dwProcessID );
	LARGE_INTEGER NewPerfTime100nSec = pPerfData->PerfTime100nSec;

	if ( m_lnOldValue == 0 )
	{
		m_lnOldValue = lnNewValue;
		m_OldPerfTime100nSec = NewPerfTime100nSec;
		return 0;
	}

	LONGLONG lnValueDelta = lnNewValue - m_lnOldValue;
	double DeltaPerfTime100nSec = (double)NewPerfTime100nSec.QuadPart - (double)m_OldPerfTime100nSec.QuadPart;

	m_lnOldValue = lnNewValue;
	m_OldPerfTime100nSec = NewPerfTime100nSec;

	double a = (double)lnValueDelta / DeltaPerfTime100nSec;

	int CpuUsage = (int) (a*100);
	if (CpuUsage < 0)
		return 0;

	return CpuUsage / m_nProcessors;
}

#endif