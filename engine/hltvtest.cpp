//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// hltvtest.cpp: implementation of the CHLTVTestSystem class.
//
//////////////////////////////////////////////////////////////////////

#include "cmd.h"
#include "convar.h"
#include "hltvtest.h"
#include "hltvserver.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CHLTVTestSystem *hltvtest = NULL;

CHLTVTestSystem::CHLTVTestSystem(void)
{

}

CHLTVTestSystem::~CHLTVTestSystem(void)
{
	StopsTest();
}

void CHLTVTestSystem::RunFrame()
{
	FOR_EACH_VEC( m_Servers, iServer )
	{
		m_Servers[iServer]->RunFrame();
	}
}

bool CHLTVTestSystem::StartTest(int nClients, const char *pszAddress)
{
	Assert( m_Servers.Count() == 0 );

	while ( m_Servers.Count() < nClients )
	{
		CHLTVServer *pServer = new CHLTVServer();
		m_Servers.AddToTail( pServer );
		pServer->Init( NET_IsDedicated() );
		pServer->m_ClientState.m_Socket = NET_AddExtraSocket( PORT_ANY );
	}

	for( int i=0; i<nClients; i++ )
	{
		m_Servers[i]->ConnectRelay( pszAddress );
	}

	return true;
}

void CHLTVTestSystem::RetryTest(int nClients)
{
	int maxClients = min( nClients+1, m_Servers.Count() );
		
	for ( int i=0; i<maxClients; i++ )
	{
		CHLTVServer *pHLTV = m_Servers[i];
		pHLTV->ConnectRelay( pHLTV->m_ClientState.m_szRetryAddress );
	}
}

bool CHLTVTestSystem::StopsTest()
{
	FOR_EACH_VEC( m_Servers, iServer )
	{
		m_Servers[iServer]->Shutdown();
	}

	m_Servers.PurgeAndDeleteElements();

	NET_RemoveAllExtraSockets();

	return true;
}


#ifdef _HLTVTEST

CON_COMMAND( tv_test_start, "Starts the SourceTV test system" )
{
	if ( args.ArgC() < 3 )
	{
		Msg( "Usage: tv_test_start <number> <ip:port>\n" );
		return;
	}

	int nClients = Q_atoi( args[1] );

	char address[MAX_PATH];
	
	if ( args.ArgC() == 3 )
	{
		Q_strncpy( address, args[2], MAX_PATH );
	}
	else
	{
		Q_snprintf( address, MAX_PATH, "%s:%s", args[2], args[4] );
	}

	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( !Q_strncmp( address, "localhost", 9 ) )
	{
		Msg( "SourceTV test can't connect to localhost.\n" );
		return;
	}

	if ( !hltvtest )
	{
		// create new HLTV test system
		hltvtest = new CHLTVTestSystem();
	}
	else
	{
		// stop old test
		hltvtest->StopsTest();
	}
	
	// shutdown anything else
	Host_Disconnect( false );

	// start networking
	NET_SetMutiplayer( true );	

	hltvtest->StartTest( nClients, address );
}

CON_COMMAND( tv_test_retry, "Tell test clients to reconnect" )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Usage: tv_test_retry <number>\n" );
		return;
	}

	int nClients = Q_atoi( args[1] );

	if ( hltvtest )
	{
		hltvtest->RetryTest( nClients );
	}
	else
	{
		Msg("No test running.\n");
	}
}

CON_COMMAND( tv_test_stop, "Stops the SourceTV test system" )
{
	if ( hltvtest )
	{
		hltvtest->StopsTest();
	}
}

#endif