//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Deals with remote perf testing on customer machines
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//

#include "server_pch.h"
#include "cl_rcon.h"
#include "cl_steamauth.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef ENABLE_RPT
//-----------------------------------------------------------------------------
// Remote perf testing
// 
// Description: Here's how rpt works. The 'customer' machine is the one
// we want to do testing on. The 'valve' machine is the programmer machine
// that has rcon access to the customer machine. The 'server' machine is the
// machine running a dedicated or listen server which both the customer + valve
// machines are connected to.
//
// Step 0) Customer runs with -rpt on the commandline, which turns on -condebug,
// clears out the console.log, and potentially other things.
//
// Step 1) Customer machine types rpt_password <password>. We store the password
// on the customer machine only and never forward it to the server to avoid 
// having to worry about malicious servers. The password is stored in the RPTServer
// password The customer machine then forwards
// a command "rpt_client_enable 1" to the server indicating that it is a client
// which can be controlled via rpt.
//
// Step 2) Server machine receives the "rpt_client_enable 1" command. It stores
// some state indicating the client it received that message from can be rpt controlled.
//
// Step 3) Valve machine types rpt_start <password> [<port #>]. The port specification
// is optional. The valve machine's rpt rcon client puts itself into a listening 
// mode, waiting for connection requests from the customer machine. Once 
// connected, the RPTClient rcon client will be the mechanism by which the valve 
// machine sends rcon commands to the customer machine.	After this is done,
// the Valve machine sends a command "rpt_server_enable 1" to the server, indicating
// it wishes to connect to an rpt_client.
//
// Step 4) Server machine receives the 'rpt_server_enable 1' command.  This
// command can only be received from IP addresses we know are at Valve,
// and from steam clients in the Valve group. This will turn cheats on on the
// server and will forward a message to the customer machine 'rpt_connect <valve ip address>'.
// It will only try to send this message to a client which has previously
// enabled rpt with the 'rpt_client_enable 1' command.
//
// Step 5) Customer machine receives the 'rpt_connect' command. This command
// can only work if the ip address specified comes from IP addresses we know
// are at Valve. The customer machine then attempts to connect to the RPTClient
// on the Valve machine, which establishes the connection to the customer
// RPTServer. The rcon classes will handle password authentication.
//
// Step 6) Valve machine types rpt <console commands> which are then sent
// to the customer machine through the custom tcp connection set up by the previous steps
//
// Step 7) Customer types 'rpt_password' with no password, which sends the
// message to the server 'rpt_client_enable 0'.
//
// Step 8) Server receives 'rpt_client_enable 0', which marks it as not
// willing to accept rpt connections. Future attempts to connect via
// 'rpt_server_enable 1' will fail.
//
// Step 9) Valve machine types rpt_end, which sends a command 'rpt_server_enable 0'
// to the server, which deactivates cheats on the server. It also disconnects
// the tcp connection to the customer machine.
//-----------------------------------------------------------------------------

static int g_nRptClientSlot = -1;
static int g_nRptServerSlot = -1;


// NOTE: This is expected to be executed by the customer, using rpt_password <password>
// This data will be stored in the client only. Server doesn't get access to the password
// to prevent malicious servers from running nasty commands on clients
#ifndef SWDS 

CON_COMMAND_F( rpt_password, "", FCVAR_DONTRECORD | FCVAR_HIDDEN )
{
	if ( CommandLine()->FindParm( "-rpt" ) == 0 )
	{
		ConMsg( "This command will not work unless the game is launched with -rpt\n" );
		return;
	}

	if ( args.ArgC() > 2 )
	{
		ConMsg( "Incorrect # of arguments.\n" );
		return;
	}

	// Doesn't work on dedicated servers
	if ( NET_IsDedicated() || !NET_IsMultiplayer() || ( args.ArgC() > 2 ) )
	{
		ConMsg( "Failed!\n" );
		return;
	}

	bool bWasEnabled = RPTServer().HasPassword();
	char buf[255];
	if ( args.ArgC() == 1 )
	{
		if ( !bWasEnabled )
			return;

		RPTServer().SetPassword( NULL );
		ConMsg( "Disabling...\n" );
		Q_snprintf( buf, sizeof( buf ), "rpt_client_enable 0" );
	}
	else if ( args.ArgC() == 2 )
	{
		RPTServer().SetPassword( args.Arg( 1 ) );
		ConMsg( "New password : %s\n", args.Arg( 1 ) );
		if ( bWasEnabled )
			return;

		ConMsg( "Enabling...\n" );
		Q_snprintf( buf, sizeof( buf ), "rpt_client_enable 1" );
	}

	// Send a command to the server indicating this client can be rpted
	CCommand argsClient;
	argsClient.Tokenize( buf );
	Cmd_ForwardToServer( argsClient, true );
}

#endif


// NOTE: This is autogenerated by clients typing rpt_password. The client keeps the
// password, but tells the server it can be rpted. Only 1 client can be rpted at a time.
CON_COMMAND_F( rpt_client_enable, "", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() != 2 )
		return;

	if ( cmd_clientslot < 0 )
		return;

	CGameClient *pClient = sv.Client( cmd_clientslot );
	if ( !pClient )
		return;

	bool bEnable = ( atoi( args.Arg( 1 ) ) != 0 );
	g_nRptClientSlot = bEnable ? cmd_clientslot : -1;
}

void SV_NotifyRPTOfDisconnect( int nClientSlot )
{
	if ( nClientSlot == g_nRptClientSlot )
	{
		g_nRptClientSlot = -1;
	}

	if ( nClientSlot == g_nRptServerSlot )
	{
		g_nRptServerSlot = -1;
	}

	RPTServer().SetPassword( NULL );
#ifndef SWDS
	RPTClient().SetPassword( NULL );
	RPTClient().CloseListenSocket( );
#endif
}

#ifndef SWDS 

void CL_NotifyRPTOfDisconnect( )
{
	RPTServer().SetPassword( NULL );
	RPTClient().SetPassword( NULL );
	RPTClient().CloseListenSocket( );
}

#endif

#ifndef SWDS 
				
// This runs on the valve client machine
CON_COMMAND_F( rpt_start, "", FCVAR_DONTRECORD | FCVAR_HIDDEN )
{
	if ( args.ArgC() < 2 || args.ArgC() > 3 )
	{
		ConMsg( "Incorrect # of arguments.\n" );
		return;
	}

	// Listen for connections from the customer machine
	netadr_t rptAddr = net_local_adr;
	if ( args.ArgC() == 3 )
	{
		rptAddr.SetPort( atoi( args.Arg( 2 ) ) );
	}
	else
	{
		rptAddr.SetPort( PORT_RPT_LISTEN );
	}

	RPTClient().SetPassword( args.Arg( 1 ) );
	RPTClient().CreateListenSocket( rptAddr );

	char pDir[MAX_PATH];
#ifdef WIN32
	int nDay, nMonth, nYear;
	GetCurrentDate( &nDay, &nMonth, &nYear );
	Q_snprintf( pDir, sizeof(pDir), "rpt/%d_%d_%d", nMonth, nDay, nYear );
#elif POSIX
	time_t now = time(NULL);
	struct tm *tm = localtime( &now );
	Q_snprintf( pDir, sizeof(pDir), "rpt/%d_%d_%d", tm->tm_mon, tm->tm_wday, tm->tm_year + 1900 );
#else
#error
#endif
	RPTClient().SetRemoteFileDirectory( pDir );

	// Send a command to the server indicating we want to connect to a remote client
	char pBuf[256];
	Q_snprintf( pBuf, sizeof(pBuf), "rpt_server_enable 1 %s\n", rptAddr.ToString() );
	CCommand argsClient;
	argsClient.Tokenize( pBuf );
	Cmd_ForwardToServer( argsClient, true );
}

CON_COMMAND_F( rpt_end, "", FCVAR_DONTRECORD | FCVAR_HIDDEN )
{
	if ( args.ArgC() != 1 )
	{
		ConMsg( "Incorrect # of arguments.\n" );
		return;
	}

	RPTClient().SetPassword( NULL );
	RPTClient().CloseListenSocket( );

	// Send a command to the server indicating we want to disconnect from a remote client
	char pBuf[256];
	Q_snprintf( pBuf, sizeof(pBuf), "rpt_server_enable 0\n" );
	CCommand argsClient;
	argsClient.Tokenize( pBuf );
	Cmd_ForwardToServer( argsClient, true );
}

#endif

// This is the steam id for user 'remote_perf_test'. See wiki for password.
// http://intranet.valvesoftware.com/wiki/index.php/Debugging_problems_on_customer_machines
static uint64 s_ValveMask = 0xFAB2423BFFA352AFull;
static uint64 s_pValveIDs[] =
{
	76561197995463203ll ^ s_ValveMask,
};
	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool IsValveIPAddress( const netadr_t &adr )
{
	// Only accept this from clients inside of valve
	netadr_t valveIP1, valveIP2;
	valveIP1.SetIP( 0xCF, 0xAD, 0xB2, 0xFF );
	valveIP2.SetIP( 0xCF, 0xAD, 0xB3, 0xFF );
	return ( adr.CompareClassCAdr( valveIP1 ) || adr.CompareClassCAdr( valveIP2 ) || adr.IsLoopback() );
}

static bool PlayerIsValveEmployee( int nClientSlot )
{
	CGameClient *pClient = sv.Client( nClientSlot );
	if ( !pClient )
		return false;

	const netadr_t& adr = pClient->m_NetChannel->GetRemoteAddress();
	if ( !IsValveIPAddress( adr ) )
		return false;

	// If Steam is running and connected to beta, player is valve
	if ( k_EUniverseBeta == GetSteamUniverse() )
		return true;
	
	if ( !pClient->IsFullyAuthenticated() )
		return false;

	player_info_t pi;
	if ( !sv.GetPlayerInfo( nClientSlot, &pi ) )
		return false;

	if ( !pi.friendsID )
		return false;
	
	CSteamID steamIDForPlayer( pi.friendsID, 1, k_EUniversePublic, k_EAccountTypeIndividual );
	for ( int i = 0; i < ARRAYSIZE(s_pValveIDs); i++ )
	{
		if ( steamIDForPlayer.ConvertToUint64() == (s_pValveIDs[i] ^ s_ValveMask) )
			return true;
	}

	return false;
}

// NOTE: This is expected to be called on the server as a result of rpt running
CON_COMMAND_F( rpt_server_enable, "", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() != 5 && args.ArgC() != 2 )
		return;

	if ( cmd_clientslot < 0 )
		return;

	if ( g_nRptServerSlot >= 0 && g_nRptServerSlot != cmd_clientslot )
		return;

	if ( !PlayerIsValveEmployee( cmd_clientslot ) )
		return;

	if ( g_nRptClientSlot < 0 )
	{
		ConMsg( "No valid clients.\n" );
		g_nRptServerSlot = -1;
		return;
	}

	bool bEnable = atoi( args.Arg( 1 ) ) != 0;
	if ( !bEnable )
	{
		g_nRptServerSlot = -1;
		return;
	}

	// Tell the customer machine to connect to the ip specified if the password matches
	CGameClient *pClient = sv.Client( cmd_clientslot );
	netadr_t adr( args.Arg( 2 ) );
	netadr_t adrClient = pClient->m_NetChannel->GetRemoteAddress();
	if ( !adrClient.IsLoopback() && !adr.CompareAdr( adrClient, true ) )
	{
		ConMsg( "Invalid server IP address.\n" );
		return;
	}

	adr.SetPort( atoi( args.Arg( 4 ) ) );

	// NOTE: Trickiness. Need to activate cheats, and that *must* be controlled via
	// the server in a method protected by IP checks.
	if ( g_nRptServerSlot < 0 )
	{
		g_nRptServerSlot = cmd_clientslot; 
	}

	char pBuf[256];
	Q_snprintf( pBuf, sizeof(pBuf), "rpt_connect %s\n", adr.ToString() );
	SV_ExecuteRemoteCommand( pBuf, g_nRptClientSlot );
}

#ifndef SWDS

// NOTE: This executes on the client, and is generated by the server automatically
// when another client tries to connect
CON_COMMAND_F( rpt_connect, "", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_SERVER_CAN_EXECUTE )
{
	if ( CommandLine()->FindParm( "-rpt" ) == 0 )
		return;

	if ( args.ArgC() != 4 )
		return;

	const char *pAddress = args.Arg( 1 );
	netadr_t adr( pAddress );
	adr.SetPort( atoi( args.Arg( 3 ) ) );

	// Only accept this from clients inside of valve
	if ( !IsValveIPAddress( adr ) )
		return;

	RPTServer().ConnectToListeningClient( adr, true );
}


// Method to run rcon commands on an rpt-controlled machine
CON_COMMAND_F( rpt, "Issue an rpt command.", FCVAR_DONTRECORD | FCVAR_HIDDEN )
{
	char	message[1024];   // Command message
	char    szParam[ 256 ];
	message[0] = 0;
	for (int i=1 ; i<args.ArgC() ; i++)
	{
		const char *pParam = args[i];
		// put quotes around empty arguments so we can pass things like this: rcon sv_password ""
		// otherwise the "" on the end is lost
		if ( strchr( pParam, ' ' ) || ( Q_strlen( pParam ) == 0 ) )
		{
			Q_snprintf( szParam, sizeof( szParam ), "\"%s\"", pParam );
			Q_strncat( message, szParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncat( message, pParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		if ( i != ( args.ArgC() - 1 ) )
		{
			Q_strncat (message, " ", sizeof( message ), COPY_ALL_CHARACTERS);
		}
	}

	RPTClient().SendCmd( message );
}


#endif   // SWDS

#endif // ENABLE_RPT
