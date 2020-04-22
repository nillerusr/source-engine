//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "matchmakingqos.h"
#include "threadtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#if _X360

//-----------------------------------------------------------------------------
// Quality of Service detection routines
//-----------------------------------------------------------------------------

class CQosDetector
{
public:
	CQosDetector();
	~CQosDetector();

public:
	bool InitiateLookup();
	bool WaitForResults();

public:
	MM_QOS_t GetResults() const { return m_Results; }

private:
	void Release();
	XNQOS *m_pQos;

	void ProcessResults();
	MM_QOS_t m_Results;
};

CQosDetector::CQosDetector() : m_pQos( NULL )
{
	m_Results.nPingMsMin = 50;	// 50 ms default ping
	m_Results.nPingMsMed = 50;	// 50 ms default ping
	m_Results.flBwUpKbs = 32.0f;	// 32 KB/s = 256 kbps
	m_Results.flBwDnKbs = 32.0f;	// 32 KB/s = 256 kbps
	m_Results.flLoss = 0.0f;	// 0% packet loss
}

CQosDetector::~CQosDetector()
{
	Release();
}

void CQosDetector::Release()
{
	if ( m_pQos )
	{
		XNetQosRelease( m_pQos );
		m_pQos = NULL;
	}
}

bool CQosDetector::InitiateLookup()
{
	Release();

	//
	// Issue the asynchronous QOS service lookup query
	//
	XNQOS *pQos = NULL;
	INT err = XNetQosServiceLookup( NULL, NULL, &pQos );
	if ( err )
	{
		Msg( "CQosDetector::InitiateLookup failed, err = %d\n", err );
		return false;
	}
	if ( !pQos )
	{
		Msg( "CQosDetector::InitiateLookup failed, qos is null.\n" );
		return false;
	}
	m_pQos = pQos;
	return true;
}

bool CQosDetector::WaitForResults()
{
	if ( !m_pQos )
		return false;

	//
	// Spin-sleep here while waiting for the probes to come back
	//
	const int numSecQosTimeout = 30;	// Consider QOS query timed out if 30 seconds elapsed
	float flTimeStart = Plat_FloatTime(), flTimeEndTimeout = flTimeStart + numSecQosTimeout;
	for ( ; ; )
	{
		if ( Plat_FloatTime() > flTimeEndTimeout )
		{
			Msg( "CQosDetector::WaitForResults timed out after %d sec.\n", numSecQosTimeout );
			Release();
			return false;	// Timed out
		}

		if ( m_pQos->axnqosinfo->bFlags & XNET_XNQOSINFO_COMPLETE )
			break;	// QOS query has completed

		ThreadSleep( 10 );
	}

	if ( !  ( m_pQos->axnqosinfo->bFlags & XNET_XNQOSINFO_TARGET_CONTACTED ) ||
		( m_pQos->axnqosinfo->bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) )
	{
		// Failed to contact host or target disabled
		Msg( "CQosDetector::WaitForResults host unavailable.\n" );
		Release();
		return false;
	}

	ProcessResults();
	Release();
	return true;
}

void CQosDetector::ProcessResults()
{
	MM_QOS_t Results;

	Results.nPingMsMin = m_pQos->axnqosinfo->wRttMinInMsecs;
	Results.nPingMsMed = m_pQos->axnqosinfo->wRttMedInMsecs;

	Results.flBwUpKbs = m_pQos->axnqosinfo->dwUpBitsPerSec / 8192.0f;
	if ( Results.flBwUpKbs < 1.f )
	{
		Results.flBwUpKbs = 1.f;
	}
	Results.flBwDnKbs = m_pQos->axnqosinfo->dwDnBitsPerSec / 8192.0f;
	if ( Results.flBwDnKbs < 1.f )
	{
		Results.flBwDnKbs = 1.f;
	}

	Results.flLoss = m_pQos->axnqosinfo->cProbesXmit ? ( float( m_pQos->axnqosinfo->cProbesXmit - m_pQos->axnqosinfo->cProbesRecv ) * 100.f / m_pQos->axnqosinfo->cProbesXmit ) : 0.f;

	m_Results = Results;

	// Dump the results
	Msg(
		"[QOS] Fresh QOS results available:\n"
		"[QOS]   ping %d min, %d med\n"
		"[QOS]   bandwidth %.1f kB/s upstream, %.1f kB/s downstream\n"
		"[QOS]   avg packet loss %.0f percents\n",
		Results.nPingMsMin, Results.nPingMsMed,
		Results.flBwUpKbs, Results.flBwDnKbs,
		Results.flLoss
		);
}

//
// Global QOS detector thread
//

static class CQosThread
{
public:
	CQosThread();
	
	MM_QOS_t GetResults();

	static unsigned ThreadProc( void *pThis ) { return ( (CQosThread *) pThis )->Run(), 0; }
	void Run();
	
private:
	ThreadHandle_t m_hHandle;
	CThreadEvent m_hRequestResultsEvent;	// Auto reset event to trigger next query
	CQosDetector m_QosDetector;
}
s_QosThread;

CQosThread::CQosThread() : m_hHandle( NULL )
{
}

void CQosThread::Run()
{
	//
	// Sit here and fetch fresh QOS results whenever somebody needs them.
	// Every request of QOS data is instantaneous and returns the last successful QOS query result.
	//

	for ( unsigned int numQosRequestsMade = 0; ; ++ numQosRequestsMade )
	{
		m_QosDetector.InitiateLookup();
		m_QosDetector.WaitForResults();

		if ( numQosRequestsMade )
		{
			m_hRequestResultsEvent.Wait();
		}
	}

	ReleaseThreadHandle( m_hHandle );
	m_hHandle = NULL;
}

MM_QOS_t CQosThread::GetResults()
{
	// Launch the thread if this is the first time QOS is needed
	if ( !m_hHandle )
	{
		m_hHandle = CreateSimpleThread( ThreadProc, this );
		
		if( m_hHandle )
		{
			ThreadSetAffinity( m_hHandle, XBOX_PROCESSOR_3 );
		}
	}

	// Signal the event that we can make a fresh QOS query
	m_hRequestResultsEvent.Set();
	return m_QosDetector.GetResults();
}


//
// Function to retrieve the result of the last QOS query
//

MM_QOS_t MM_GetQos()
{
	return s_QosThread.GetResults();
}


#else

//
// Default implementation of QOS
//

static struct Default_MM_QOS_t : public MM_QOS_t
{
	Default_MM_QOS_t()
	{
		nPingMsMin = 50;	// 50 ms default ping
		nPingMsMed = 50;	// 50 ms default ping
		flBwUpKbs = 32.0f;	// 32 KB/s = 256 kbps
		flBwDnKbs = 32.0f;	// 32 KB/s = 256 kbps
		flLoss = 0.0f;	// 0% packet loss
	}
}
s_DefaultQos;

MM_QOS_t MM_GetQos()
{
	return s_DefaultQos;
}

#endif
