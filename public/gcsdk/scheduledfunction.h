//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Provides a scheduled function manager that will bucket events into
//		time chunks and execute them as time elapses
//
//=============================================================================

#ifndef SCHEDULEDFUNCTION_H
#define SCHEDULEDFUNCTION_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

//interface for events that can be scheduled to run on the GC base
class IGCScheduledFunction
{
public:
	IGCScheduledFunction() : m_nAbsScheduleBucket( knInvalidBucket )	{}
	virtual ~IGCScheduledFunction();
	//called in response to our event time elapsing
	virtual void OnEvent() = 0;

	bool BIsScheduled()	const											{ return m_nAbsScheduleBucket != IGCScheduledFunction::knInvalidBucket; }

private:
	//the absolute bucket that we were scheduled in (or invalid). Used to enable deregistering
	friend class CScheduledFunctionMgr;
	uint32		m_nAbsScheduleBucket;
	uint32		m_nLLIndex;
	static const uint32 knInvalidBucket = ( uint32 )-1;
};

//utility for scheduling a global function
class CGlobalScheduledFunction :
	public IGCScheduledFunction
{
public:

	CGlobalScheduledFunction();
	
	typedef void ( *func_t )();

	void ScheduleMS( func_t pfn, uint32 nDelayMS );
	void ScheduleSecond( func_t pfn, uint32 nDelaySecond );
	void ScheduleMinute( func_t pfn, uint32 nDelayMinute );
	void Cancel();

	virtual void OnEvent() OVERRIDE;

private:
	func_t	m_pfn;
};

//the same as the above, but automatically deletes the object once the event is fired
class CGlobalScheduledFunctionAutoDelete :
	public CGlobalScheduledFunction
{
public:
	CGlobalScheduledFunctionAutoDelete()									{}

	virtual void OnEvent() OVERRIDE
	{
		CGlobalScheduledFunction::OnEvent();
		delete this;
	}
};

//utility for scheduling a member function
template< class T >
class CScheduledFunction :
	public IGCScheduledFunction
{
public:
	CScheduledFunction() : m_pObj( NULL ), m_pfn( NULL ) {}

	typedef void ( T::*func_t )();

	void ScheduleMS( T* pObj, func_t pfn, uint32 nDelayMS )
	{
		m_pObj = pObj;
		m_pfn = pfn;
		GScheduledFunctionMgr().ScheduleMS( this, nDelayMS );
	}

	void ScheduleSecond( T* pObj, func_t pfn, uint32 nDelaySecond )
	{
		m_pObj = pObj;
		m_pfn = pfn;
		GScheduledFunctionMgr().ScheduleSecond( this, nDelaySecond );
	}

	void ScheduleMinute( T* pObj, func_t pfn, uint32 nDelayMinute )
	{
		m_pObj = pObj;
		m_pfn = pfn;
		GScheduledFunctionMgr().ScheduleMinute( this, nDelayMinute );
	}


	void Cancel()
	{
		GScheduledFunctionMgr().Cancel( this );
	}

	virtual void OnEvent() OVERRIDE
	{
		( m_pObj->*m_pfn )();
	}

private:
	T*		m_pObj;
	func_t	m_pfn;
};

//similar to the above, but auto deletes once the event is fired
template< class T >
class CScheduledFunctionAutoDelete :
	public CScheduledFunction< T >
{
public:
	typedef void ( T::*func_t )();

	CScheduledFunctionAutoDelete()											{}
	CScheduledFunctionAutoDelete( T* pObj, func_t pfn, uint32 nDelayMS )	{ Schedule( pObj, pfn, nDelayMS ); }
	virtual void OnEvent() OVERRIDE
	{
		CScheduledFunction< T >::OnEvent();
		delete this;
	}
};


class CScheduledFunctionMgr
{
public:

	CScheduledFunctionMgr();

	//called to initialize the starting time for the scheduled function manager. It doesn't need to be globally absolute, just relative to the time values provided
	//to the run function
	void InitStartingTime();

	//called to register a scheduled event for a certain period in the future, with the delay specified in milliseconds. This has resolution of an individual frame, and
	//will unregister from any previously registered time slot
	void ScheduleMS( IGCScheduledFunction* pEvent, uint32 nMSDelay );
	//similar to the above, but instead of having frame resolution, this has second level resolution, and should be used for low granularity tasks
	void ScheduleSecond( IGCScheduledFunction* pEvent, uint32 nSDelay );
	//similar to the above but has minute level resolution which should be used for very low resolution tasks
	void ScheduleMinute( IGCScheduledFunction* pEvent, uint32 nMinuteDelay );

	//deregisters a previously registered event
	void Cancel( IGCScheduledFunction* pEvent );

	//called to run registered functions
	void RunFunctions();
	
private:
	
	//called internally by the other schedule functions to schedule the event at the specified resolution in our resolution array
	void InternalSchedule( uint32 nResolution, IGCScheduledFunction* pEvent, uint32 nMSDelay );

	//the list type that we store all of the entries in. We use a single list to avoid the overhead of so many lists 
	typedef CUtlLinkedList< IGCScheduledFunction*, uint32 >	TScheduleList;

	//all information tied to a specific resolution, including the time hash buckets, number of buckets, and which buckets it has executed
	class CScheduleBucket
	{
	public:
		CScheduleBucket();
		~CScheduleBucket();
		void Init( TScheduleList& MasterList, uint32 nNumBuckets, uint32 nMicroSPerBucket );		

		//maps a micro second time to a bucket time
		uint32 GetAbsScheduleBucketIndex( uint64 nMicroSTime ) const			{ return ( uint32 )( nMicroSTime / m_nMicroSPerBucket ); }

		//called to run registered functions
		void RunFunctions( TScheduleList& MasterList, uint64 nMicroSTime );

		//for each bucket, we insert a node into the master linked list, then our list runs from this node to the next empty node (or end) in the list
		uint32*		m_pBuckets;
		//the number of buckets that we have
		uint32		m_nNumBuckets;
		//how many micro seconds each bucket represents
		uint32		m_nMicroSPerBucket;
		//the last bucket that we had executed
		uint32		m_nAbsLastScheduleBucket;
	};

	//the list of all of our entries. We store bucket starts within here, and then insert the events in between them
	TScheduleList	m_ScheduleList;

	//the list of resolutions that we have
	CScheduleBucket	m_Resolutions[ 3 ];
};

//global singleton access
CScheduledFunctionMgr& GScheduledFunctionMgr();


} //namespace GCSDK

#endif
