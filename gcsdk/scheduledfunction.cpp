//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//=============================================================================//
#include "stdafx.h"
#include "scheduledfunction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{


IGCScheduledFunction::~IGCScheduledFunction()
{ 
	GScheduledFunctionMgr().Cancel( this ); 
}


//-------------------------------------------------------------------------
CGlobalScheduledFunction::CGlobalScheduledFunction() :
	m_pfn( NULL )
{}

//-------------------------------------------------------------------------
void CGlobalScheduledFunction::ScheduleMS( func_t pfn, uint32 nDelayMS )
{
	m_pfn = pfn;
	GScheduledFunctionMgr().ScheduleMS( this, nDelayMS );
}

void CGlobalScheduledFunction::ScheduleSecond( func_t pfn, uint32 nDelaySecond )
{
	m_pfn = pfn;
	GScheduledFunctionMgr().ScheduleSecond( this, nDelaySecond );
}

void CGlobalScheduledFunction::ScheduleMinute( func_t pfn, uint32 nDelayMinute )
{
	m_pfn = pfn;
	GScheduledFunctionMgr().ScheduleMinute( this, nDelayMinute );
}


//-------------------------------------------------------------------------
void CGlobalScheduledFunction::Cancel()
{
	GScheduledFunctionMgr().Cancel( this );
}

//-------------------------------------------------------------------------
void CGlobalScheduledFunction::OnEvent()
{
	m_pfn();
}


//-------------------------------------------------------------------------
CScheduledFunctionMgr::CScheduledFunctionMgr()
{
	const uint32 knSecond = 1000000;
	//frame rate resolution - 30s at 20Hz
	m_Resolutions[ 0 ].Init( m_ScheduleList, 600, k_cMicroSecPerShellFrame );
	//second resolution - 15 minutes
	m_Resolutions[ 1 ].Init( m_ScheduleList, 15 * 60, knSecond );
	//minute resolution - 5 hours
	m_Resolutions[ 2 ].Init( m_ScheduleList, 5 * 60, 60 * knSecond );
}

//-------------------------------------------------------------------------
void CScheduledFunctionMgr::InitStartingTime()
{
	uint64 nCurrTime = CJobTime::LJobTimeCur();
	for( uint32 nCurrBucket = 0; nCurrBucket < ARRAYSIZE( m_Resolutions ); nCurrBucket++ )
	{
		m_Resolutions[ nCurrBucket ].m_nAbsLastScheduleBucket = m_Resolutions[ nCurrBucket ].GetAbsScheduleBucketIndex( nCurrTime );
	}
}

//-------------------------------------------------------------------------

void CScheduledFunctionMgr::InternalSchedule( uint32 nResolution, IGCScheduledFunction* pEvent, uint32 nMSDelay )
{
	//if the event is already registered, deregister it, double registration would be a very bad thing
	if( pEvent->BIsScheduled() )
	{
		Cancel( pEvent );
	}

	//determine which bucket this belongs in
	uint32 nAbsBucket = m_Resolutions[ nResolution ].GetAbsScheduleBucketIndex( CJobTime::LJobTimeCur() + ( uint64 )nMSDelay * 1000 );
	//so we can remove it, and deal with wrapping
	pEvent->m_nAbsScheduleBucket = nAbsBucket;

	//add it to our list
	uint32 nInsertAfter = m_Resolutions[ nResolution ].m_pBuckets[ nAbsBucket % m_Resolutions[ nResolution ].m_nNumBuckets ];
	pEvent->m_nLLIndex = m_ScheduleList.InsertAfter( nInsertAfter, pEvent );
}

void CScheduledFunctionMgr::ScheduleMS( IGCScheduledFunction* pEvent, uint32 nMSDelay )
{
	InternalSchedule( 0, pEvent, nMSDelay );
}

void CScheduledFunctionMgr::ScheduleSecond( IGCScheduledFunction* pEvent, uint32 nSDelay )
{
	InternalSchedule( 1, pEvent, nSDelay * 1000 );
}

void CScheduledFunctionMgr::ScheduleMinute( IGCScheduledFunction* pEvent, uint32 nMinuteDelay )
{
	InternalSchedule( 2, pEvent, nMinuteDelay * 1000 * 60 );
}

//-------------------------------------------------------------------------
void CScheduledFunctionMgr::Cancel( IGCScheduledFunction* pEvent )
{
	//ignore it if not already registered
	if( pEvent->m_nAbsScheduleBucket == IGCScheduledFunction::knInvalidBucket )
		return;

	if( m_ScheduleList.IsValidIndex( pEvent->m_nLLIndex ) )
	{
		m_ScheduleList.Remove( pEvent->m_nLLIndex );
	}
	else
	{
		AssertMsg( false, "Warning: Ecountered a remove request for a scheduled event but was unable to find it in the bucket that it was supposed to be in" );
	}

	pEvent->m_nAbsScheduleBucket = IGCScheduledFunction::knInvalidBucket;
}

//-------------------------------------------------------------------------
void CScheduledFunctionMgr::RunFunctions()
{
	VPROF_BUDGET( "CGCBase::CallScheduledEvents", VPROF_BUDGETGROUP_STEAM );

	uint64 nCurrTime = CJobTime::LJobTimeCur();
	for( uint32 nCurrBucket = 0; nCurrBucket < ARRAYSIZE( m_Resolutions ); nCurrBucket++ )
	{
		m_Resolutions[ nCurrBucket ].RunFunctions( m_ScheduleList, nCurrTime );
	}	
}


//-------------------------------------------------------------------------
CScheduledFunctionMgr& GScheduledFunctionMgr()
{
	static CScheduledFunctionMgr s_Singleton;
	return s_Singleton;	
}

//-------------------------------------------------------------------------
CScheduledFunctionMgr::CScheduleBucket::CScheduleBucket() :
	m_pBuckets( NULL ),
	m_nNumBuckets( 0 ),
	m_nMicroSPerBucket( 0 ),
	m_nAbsLastScheduleBucket( 0 )
{}

CScheduledFunctionMgr::CScheduleBucket::~CScheduleBucket()
{
	delete [] m_pBuckets;
}

void CScheduledFunctionMgr::CScheduleBucket::Init( TScheduleList& MasterList, uint32 nNumBuckets, uint32 nMicroSPerBucket )
{
	//init should never happen more than once
	AssertMsg( !m_pBuckets, "Error: Schedule buckets should never be initialized multiple times" );

	m_pBuckets = new uint32[ nNumBuckets ];
	m_nNumBuckets = nNumBuckets;
	m_nMicroSPerBucket = nMicroSPerBucket;

	for( uint32 nCurrBucket = 0; nCurrBucket < nNumBuckets; nCurrBucket++ )
	{
		m_pBuckets[ nCurrBucket ] = MasterList.AddToTail( NULL );
	}
}

void CScheduledFunctionMgr::CScheduleBucket::RunFunctions( TScheduleList& MasterList, uint64 nMicroSTime )
{
	//determine the absolute bucket index of our current frame that we want to run to
	uint32 nCurrFrameBucket = GetAbsScheduleBucketIndex( nMicroSTime );

	//note that we include the starting bucket, but not the ending bucket. This addresses two issues: it avoids checking buckets multiple times, and since we may not have completely spent
	//all the time for the frame, events will not fire early
	for( uint32 nAbsBucket = m_nAbsLastScheduleBucket; nAbsBucket < nCurrFrameBucket; nAbsBucket++ )
	{
		//map this to a relative bucket
		uint32 nStartIndex = m_pBuckets[ nAbsBucket % m_nNumBuckets ];
		AssertMsg2( nStartIndex != MasterList.InvalidIndex(), "Error: Detected list corruption when accessing list bucket %d (%d micro s per bucket)", nAbsBucket % m_nNumBuckets, m_nMicroSPerBucket );
		for( uint32 nCurrEvent = MasterList.Next( nStartIndex ); nCurrEvent != MasterList.InvalidIndex(); )
		{
			//make sure to advance to the next item immediately since we'll be removing this element from our list
			uint32 nEventToRemove = nCurrEvent;
			IGCScheduledFunction* pEvent = MasterList[ nCurrEvent ];
			nCurrEvent = MasterList.Next( nCurrEvent );

			//see if we have hit the end of our bucket
			if( !pEvent )
			{
				break;
			}

			//skip any event that happens in the future (can occur since we wrap the frame list over itself to map to an actual bucket)
			if( pEvent->m_nAbsScheduleBucket > nAbsBucket )
			{
				//just skip over this element
				continue;
			}

			//sanity check that this bucket matches exactly. We'll process stale ones, but this warns us that we missed an event and cycled through the bucket, causing it be very delayed
			AssertMsg2( pEvent->m_nAbsScheduleBucket == nAbsBucket, "Warning: Encountered a scheduled event that was intended to be fired on bucket %d but wasn't until bucket %d", pEvent->m_nAbsScheduleBucket, nAbsBucket );

			//mark the object has having been removed
			pEvent->m_nAbsScheduleBucket = IGCScheduledFunction::knInvalidBucket;
			pEvent->m_nLLIndex = MasterList.InvalidIndex();

			//remove it from our list
			MasterList.Remove( nEventToRemove );

			//call into the object
			pEvent->OnEvent();
			//NOTE: You cannot use the object any more, it may have destroyed itself!!!!
			pEvent = NULL;
		}
	}

	//and update the event range we need to process
	m_nAbsLastScheduleBucket = nCurrFrameBucket;
}



} //namespace GCSDK

