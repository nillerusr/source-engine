//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// socket_tests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include "iphelpers.h"
#include "tcpsocket.h"
#include "utlvector.h"
#include "fragment_channel.h"
#include "reliable_channel.h"
#include "tier0/fasttimer.h"


#if defined( _DEBUG )
	#if defined( assert )
		#undef assert
	#endif

	#define assert(x)	if ( !x ) __asm int 3;
#else
	#define assert(x)
#endif


bool CompareArrays( const CUtlVector<unsigned char> &a1, const CUtlVector<unsigned char> &a2 )
{
	if ( a1.Count() != a2.Count() )
		return false;

	for ( int i=0; i < a1.Count(); i++ )
	{
		if ( a1[i] != a2[i] )
			return false;
	}
	return true;
}


// Test two reliable channels that are hooked up to each other.
void TestChannels( IChannel *pChannel1, IChannel *pChannel2, int maxPacketSize, int nTests )
{
	for ( int iTest=0; iTest < nTests; iTest++ )
	{
		float t = (float)rand() / VALVE_RAND_MAX;
		int testSize = (int)( t * (maxPacketSize-1) ) + 1;
		
		CUtlVector<unsigned char> rnd1, rnd2;
		rnd1.SetSize( testSize );
		rnd2.SetSize( testSize );
		for ( int i=0; i < testSize; i++ )
		{
			rnd1[i] = rand();
			rnd2[i] = rand();
		}

		pChannel1->Send( rnd1.Base(), testSize );
		pChannel2->Send( rnd2.Base(), testSize );

	
		// Now wait for up to 5 seconds for the data to come in.
		CUtlVector<unsigned char> tmp;
		tmp.SetSize( testSize );

		CUtlVector<unsigned char> testVec;
		bool bReceived;
		if ( !( bReceived = pChannel1->Recv( testVec, 15 ) ) || !CompareArrays( testVec, rnd2 ) )
		{
			assert( false );
		}

		if ( !( bReceived = pChannel2->Recv( testVec, 15 ) ) || !CompareArrays( testVec, rnd1 ) )
		{
			assert( false );
		}
	}
}


template<class T>
void TestChannels( T *pSock[2] )
{
	int iPorts[2];
	for ( int iPort=0; iPort < 2; iPort++ )
	{
		int nTries = 150;
		for ( int iTry=0; iTry < nTries; iTry++ )
		{
			iPorts[iPort] = 27111 + iTry;
			if ( pSock[iPort]->BindToAny( iPorts[iPort] ) )
				break;
		}
	}

	// Bind them to random ports.	
	pSock[0]->BeginListen();
	pSock[1]->BeginConnect( CIPAddr( 127, 0, 0, 1, iPorts[0] ) );
	while ( !pSock[0]->IsConnected() || !pSock[1]->IsConnected() )
	{
		CIPAddr remoteAddr;
		if ( !pSock[0]->IsConnected() )
			pSock[0]->UpdateListen( &remoteAddr );

		if ( !pSock[1]->IsConnected() )
			pSock[1]->UpdateConnect();
	}


// Measure ping-pong time.
__int64 totalMicroseconds = 0;
int nTests = 1500;
for ( int i=0; i < nTests; i++ )
{
	char buf[2116];
	CFastTimer timer;
	timer.Start();

		pSock[0]->Send( buf, sizeof( buf ) );

		CUtlVector<unsigned char> recvBuf;
		pSock[1]->Recv( recvBuf );
	timer.End();
	totalMicroseconds += timer.GetDuration().GetMicroseconds();
}


	// Now, test them with the fragmentation layer.
	IChannel *pFrag[2] = { CreateFragmentLayer( pSock[0] ), CreateFragmentLayer( pSock[1] ) };
	TestChannels( pFrag[0], pFrag[1], 1024*300, 5 );


	TestChannels( pSock[0], pSock[1], 1024, 1000 );
}


int main(int argc, char* argv[])
{
	// First, test two TCP sockets.
	for ( int iChannelType=0; iChannelType < 2; iChannelType++ )
	{	
		DWORD startTime = GetTickCount();

		srand( 0 );
		if ( iChannelType == 0 )
		{
			ITCPSocket *pTCPSockets[2] = { CreateTCPSocket(), CreateTCPSocket() };
			TestChannels( pTCPSockets );
		}
		else
		{
			IReliableChannel *pReliableChannels[2] = { CreateReliableChannel(), CreateReliableChannel() };
			TestChannels( pReliableChannels );
		}

		float flElapsed = (float)( GetTickCount() - startTime ) / 1000.0;
	}

	return 0;
}

