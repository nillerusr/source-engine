//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_tier0.h"
#include "tier0/tslist.h"
#include <list>
#include <stdlib.h>
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

namespace TSListTests
{
int NUM_TEST = 10000;
int NUM_THREADS;
int MAX_THREADS = 8;
int NUM_PROCESSORS = 1;

CInterlockedInt g_nTested;
CInterlockedInt g_nThreads;
CInterlockedInt g_nPushThreads;
CInterlockedInt g_nPopThreads;
CInterlockedInt g_nPushes;
CInterlockedInt g_nPops;
CTSQueue<int, true> g_TestQueue;
CTSList<int> g_TestList;
volatile bool g_bStart;
std::list<ThreadHandle_t> g_ThreadHandles;

int *g_pTestBuckets;

CTSListBase g_Test;
TSLNodeBase_t **g_nodes;
int idx = 0;

const char *g_pListType;

class CTestOps
{
public:
	virtual void Push( int item ) = 0;
	virtual bool Pop( int *pResult ) = 0;
	virtual bool Validate() { return true; }
	virtual bool IsEmpty() = 0;
};

class CQueueOps : public CTestOps
{
	void Push( int item )
	{
		g_TestQueue.PushItem( item );
		g_nPushes++;
	}
	bool Pop( int *pResult )
	{
		if ( g_TestQueue.PopItem( pResult ) )
		{
			g_nPops++;
			return true;
		}
		return false;
	}
	bool Validate()
	{
		return g_TestQueue.ValidateQueue();
	}
	bool IsEmpty()
	{
		return ( g_TestQueue.Count() == 0 );
	}
} g_QueueOps;

class CListOps : public CTestOps
{
	void Push( int item )
	{
		g_TestList.PushItem( item );
		g_nPushes++;
	}
	bool Pop( int *pResult )
	{
		if ( g_TestList.PopItem( pResult ) )
		{
			g_nPops++;
			return true;
		}
		return false;
	}
	bool Validate()
	{
		return true;
	}
	bool IsEmpty()
	{
		return ( g_TestList.Count() == 0 );
	}
} g_ListOps;

CTestOps *g_pTestOps;

void ClearBuckets()
{
	memset( g_pTestBuckets, 0, sizeof(int) * NUM_TEST );
}

void IncBucket( int i )
{
	if ( i < NUM_TEST ) // tests can slop over a bit
	{
		ThreadInterlockedIncrement( &g_pTestBuckets[i] );
	}
}

void DecBucket( int i )
{
	if ( i < NUM_TEST ) // tests can slop over a bit
	{
		ThreadInterlockedDecrement( &g_pTestBuckets[i] );
	}
}

void ValidateBuckets()
{
	for ( int i = 0; i < NUM_TEST; i++ )
	{
		if ( g_pTestBuckets[i] != 0 )
		{
			Msg( "Test bucket %d has an invalid value %d\n", i, g_pTestBuckets[i] );
			DebuggerBreakIfDebugging();
			return;
		}
	}
}

unsigned PopThreadFunc( void *)
{
	ThreadSetDebugName( "PopThread" );
	g_nPopThreads++;
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadSleep( 0 );
	}
	int ignored;
	for (;;)
	{
		if ( !g_pTestOps->Pop( &ignored ) )
		{
			if ( g_nPushThreads == 0 )
			{
				// Pop the rest 
				while ( g_pTestOps->Pop( &ignored ) )
				{
					ThreadSleep( 0 );
				}
				break;
			}
		}
	}
	g_nThreads--;
	g_nPopThreads--;
	return 0;
}

unsigned PushThreadFunc( void * )
{
	ThreadSetDebugName( "PushThread" );
	g_nPushThreads++;
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadSleep( 0 );
	}

	while ( g_nTested < NUM_TEST )
	{
		g_pTestOps->Push( g_nTested );
		g_nTested++;
	}
	g_nThreads--;
	g_nPushThreads--;
	return 0;
}

void TestStart()
{
	g_nTested = 0;
	g_nThreads = 0;
	g_nPushThreads = 0;
	g_nPopThreads = 0;
	g_bStart = false;
	g_nPops = g_nPushes = 0;
	ClearBuckets();
}

void TestWait()
{
	while ( g_nThreads < NUM_THREADS )
	{
		ThreadSleep( 0 );
	}
	g_bStart = true;
	while ( g_nThreads > 0 )
	{
		ThreadSleep( 50 );
	}
}

void TestEnd( bool bExpectEmpty = true )
{
	ValidateBuckets();

	if ( g_nPops != g_nPushes )
	{
		Msg( "FAIL: Not all items popped\n" );
		return;
	}

	if ( g_pTestOps->Validate() )
	{
		if ( !bExpectEmpty || g_pTestOps->IsEmpty() )
		{
			Msg("pass\n");
		}
		else
		{
			Msg("FAIL: !IsEmpty()\n");
		}
	}
	else
	{
		Msg("FAIL: !Validate()\n");
	}
	while ( g_ThreadHandles.size() )
	{
		ThreadJoin( g_ThreadHandles.front(), 0 );

		ReleaseThreadHandle( g_ThreadHandles.front() );
		g_ThreadHandles.pop_front();
	}
}


//--------------------------------------------------
//
//	Shared Tests for CTSQueue and CTSList
//
//--------------------------------------------------
void PushPopTest()
{
	Msg( "%s test: single thread push/pop, in order... ", g_pListType );
	ClearBuckets();
	g_nTested = 0;
	int value;
	while ( g_nTested < NUM_TEST )
	{
		value = g_nTested++;
		g_pTestOps->Push( value );
		IncBucket( value );
	}

	g_pTestOps->Validate();

	while ( g_pTestOps->Pop( &value ) )
	{
		DecBucket( value );
	}
	TestEnd();
}

void PushPopInterleavedTestGuts()
{
	int value;
	for (;;)
	{
		bool bPush = ( rand() % 2 == 0 );
		if ( bPush && ( value = g_nTested++ ) < NUM_TEST )
		{
			g_pTestOps->Push( value );
			IncBucket( value );
		}
		else if ( g_pTestOps->Pop( &value ) )
		{
			DecBucket( value );
		}
		else
		{
			if ( g_nTested >= NUM_TEST )
			{
				break;
			}
		}
	}
}

void PushPopInterleavedTest()
{
	Msg( "%s test: single thread push/pop, interleaved... ", g_pListType );
	srand( Plat_MSTime() );
	g_nTested = 0;
	ClearBuckets();
	PushPopInterleavedTestGuts();
	TestEnd();
}

unsigned PushPopInterleavedTestThreadFunc( void * )
{
	ThreadSetDebugName( "PushPopThread" );
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadSleep( 0 );
	}
	PushPopInterleavedTestGuts();
	g_nThreads--;
	return 0;
}

void STPushMTPop( bool bDistribute )
{
	Msg( "%s test: single thread push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	g_ThreadHandles.push_back( CreateSimpleThread( &PushThreadFunc, NULL ) );
	for ( int i = 0; i < NUM_THREADS - 1; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PopThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (i % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	TestEnd();
}

void MTPushSTPop( bool bDistribute )
{
	Msg( "%s test: multithread push, single thread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	g_ThreadHandles.push_back( 	CreateSimpleThread( &PopThreadFunc, NULL ) );
	for ( int i = 0; i < NUM_THREADS - 1; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PushThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (i % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	TestEnd();
}

void MTPushMTPop( bool bDistribute )
{
	Msg( "%s test: multithread push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	int ct = 0;
	for ( int i = 0; i < NUM_THREADS / 2 ; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PopThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (ct++ % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}
	for ( int i = 0; i < NUM_THREADS / 2 ; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PushThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (ct++ % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	TestEnd();
}

void MTPushPopPopInterleaved( bool bDistribute )
{
	Msg( "%s test: multithread interleaved push/pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	srand( Plat_MSTime() );
	TestStart();
	for ( int i = 0; i < NUM_THREADS; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PushPopInterleavedTestThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (i % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}
	TestWait();
	TestEnd();
}

void MTPushSeqPop( bool bDistribute )
{
	Msg( "%s test: multithread push, sequential pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	for ( int i = 0; i < NUM_THREADS; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PushThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (i % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	int ignored;
	g_pTestOps->Validate();
	while ( g_pTestOps->Pop( &ignored ) )
	{
	}
	TestEnd();
}

void SeqPushMTPop( bool bDistribute )
{
	Msg( "%s test: sequential push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	while ( g_nTested++ < NUM_TEST )
	{
		g_pTestOps->Push( g_nTested );
	}
	for ( int i = 0; i < NUM_THREADS; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PopThreadFunc, NULL );
		g_ThreadHandles.push_back( hThread );
		if ( bDistribute )
		{
			int32 mask = 1 << (i % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	TestEnd();
}

}
void RunSharedTests( int nTests )
{
	using namespace TSListTests;

	const CPUInformation &pi = *GetCPUInformation();
	NUM_PROCESSORS = pi.m_nLogicalProcessors;
	MAX_THREADS = NUM_PROCESSORS * 2;
	g_pTestBuckets = new int[NUM_TEST];
	while ( nTests-- )
	{
		for ( NUM_THREADS = 2; NUM_THREADS <= MAX_THREADS; NUM_THREADS *= 2)
		{
			Msg( "\nTesting %d threads:\n", NUM_THREADS );
			PushPopTest();
			PushPopInterleavedTest();
			SeqPushMTPop( false );
			STPushMTPop( false );
			MTPushSeqPop( false );
			MTPushSTPop( false );
			MTPushMTPop( false );
			MTPushPopPopInterleaved( false );
			if ( NUM_PROCESSORS > 1 )
			{
				SeqPushMTPop( true );
				STPushMTPop( true );
				MTPushSeqPop( true );
				MTPushSTPop( true );
				MTPushMTPop( true );
				MTPushPopPopInterleaved( true );
			}
		}
	}
	delete[] g_pTestBuckets;
}

bool RunTSListTests( int nListSize, int nTests )
{
	using namespace TSListTests;
	NUM_TEST = nListSize;

	TSLHead_t foo;
	(void)foo; // Avoid warning about unused variable.
#ifdef USE_NATIVE_SLIST
	int maxSize = ( 1 << (sizeof( foo.Depth ) * 8) ) - 1;
#else
	int maxSize = ( 1 << (sizeof( foo.value.Depth ) * 8) ) - 1;
#endif
	if ( NUM_TEST > maxSize )
	{
		Msg( "TSList cannot hold more that %d nodes\n", maxSize );
		return false;
	}


	g_pTestOps = &g_ListOps;
	g_pListType = "CTSList";

	RunSharedTests( nTests );

	Msg("Tests done, purging test memory..." );
	g_TestList.Purge();
	Msg( "done\n");
	return true;
}

bool RunTSQueueTests( int nListSize, int nTests )
{
	using namespace TSListTests;
	NUM_TEST = nListSize;

	g_pTestOps = &g_QueueOps;
	g_pListType = "CTSQueue";

	RunSharedTests( nTests );

	Msg("Tests done, purging test memory..." );
	g_TestQueue.Purge();
	Msg( "done\n");
	return true;
}
