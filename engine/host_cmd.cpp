//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
 
#include "tier0/vprof.h"
#include "server.h"
#include "host_cmd.h"
#include "keys.h"
#include "screen.h"
#include "vengineserver_impl.h"
#include "host_saverestore.h"
#include "sv_filter.h"
#include "gl_matsysiface.h"
#include "pr_edict.h"
#include "world.h"
#include "checksum_engine.h"
#include "const.h"
#include "sv_main.h"
#include "host.h"
#include "demo.h"
#include "cdll_int.h"
#include "networkstringtableserver.h"
#include "networkstringtableclient.h"
#include "host_state.h"
#include "string_t.h"
#include "tier0/dbg.h"
#include "testscriptmgr.h"
#include "r_local.h"
#include "PlayerState.h"
#include "enginesingleuserfilter.h"
#include "profile.h"
#include "proto_version.h"
#include "protocol.h"
#include "cl_main.h"
#include "sv_steamauth.h"
#include "zone.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "cmd.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "GameEventManager.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#include "replayserver.h"
#endif
#include "cdll_engine_int.h"
#include "cl_steamauth.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "sound.h"
#include "voice.h"
#include "sv_rcon.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xbox/xbox_launch.h"
#endif
#include "filesystem/IQueuedLoader.h"
#include "sys.h"

#include "ixboxsystem.h"
extern IXboxSystem *g_pXboxSystem;

#include <sys/stat.h>
#include <stdio.h>
#ifdef POSIX
// sigh, microsoft put _ in front of its type defines for stat
#define _stat stat
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define STEAM_PREFIX "STEAM_"

#define STATUS_COLUMN_LENGTH_LINEPREFIX	1
#define STATUS_COLUMN_LENGTH_USERID		6
#define STATUS_COLUMN_LENGTH_USERID_STR	"6"
#define STATUS_COLUMN_LENGTH_NAME		19
#define STATUS_COLUMN_LENGTH_STEAMID	19
#define STATUS_COLUMN_LENGTH_TIME		9
#define STATUS_COLUMN_LENGTH_PING		4
#define STATUS_COLUMN_LENGTH_PING_STR	"4"
#define STATUS_COLUMN_LENGTH_LOSS		4
#define STATUS_COLUMN_LENGTH_LOSS_STR	"4"
#define STATUS_COLUMN_LENGTH_STATE		6
#define STATUS_COLUMN_LENGTH_ADDR		21

#define KICKED_BY_CONSOLE "Kicked from server"

#ifndef SWDS
bool g_bInEditMode = false;
bool g_bInCommentaryMode = false;
#endif

static void host_name_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	Steam3Server().NotifyOfServerNameChange();
}

ConVar host_name( "hostname", "", 0, "Hostname for server.", host_name_changed_f );
ConVar host_map( "host_map", "", 0, "Current map name." );

void Host_VoiceRecordStop_f(void);
static void voiceconvar_file_changed_f( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
#ifndef SWDS
	ConVarRef var( pConVar );
	if ( var.GetInt() == 0 )
	{
		// Force voice recording to stop if they turn off voice_inputfromfile or if sv_allow_voice_from_file is set to 0. 
		// Prevents an exploit where clients turn it on, start voice sending a long file, and then turn it off immediately.
		Host_VoiceRecordStop_f();
	}
#endif
}

ConVar voice_recordtofile("voice_recordtofile", "0", 0, "Record mic data and decompressed voice data into 'voice_micdata.wav' and 'voice_decompressed.wav'");
ConVar voice_inputfromfile("voice_inputfromfile", "0", 0, "Get voice input from 'voice_input.wav' rather than from the microphone.", &voiceconvar_file_changed_f );
ConVar sv_allow_voice_from_file( "sv_allow_voice_from_file", "1", FCVAR_REPLICATED, "Allow or disallow clients from using voice_inputfromfile on this server.", &voiceconvar_file_changed_f );

class CStatusLineBuilder
{
public:
	CStatusLineBuilder() { Reset(); }
	void Reset() { m_curPosition = 0; m_szLine[0] = '\0'; }
	void AddColumnText( const char *pszText, unsigned int columnWidth )
	{
		size_t len = strlen( m_szLine );

		if ( m_curPosition > len )
		{
			for ( size_t i = len; i < m_curPosition; i++ )
			{
				m_szLine[i] = ' ';
			}
			m_szLine[m_curPosition] = '\0';
		}
		else if ( len != 0 )
		{
			// There is always at least one space between columns.
			m_szLine[len] = ' ';
			m_szLine[len+1] = '\0';
		}

		V_strncat( m_szLine, pszText, sizeof( m_szLine ) );
		m_curPosition += columnWidth + 1;
	}

	void InsertEmptyColumn( unsigned int columnWidth )
	{
		m_curPosition += columnWidth + 1;
	}

	const char *GetLine() { return m_szLine; }

private:
	size_t m_curPosition;
	char m_szLine[512];
};

uint GetSteamAppID()
{
	static uint sunAppID = 0;
	static bool bHaveValidSteamInterface = false;
	
	if ( !bHaveValidSteamInterface )
	{
#ifndef SWDS
		if ( Steam3Client().SteamUtils() )
		{
			bHaveValidSteamInterface = true;
			sunAppID = Steam3Client().SteamUtils()->GetAppID();
		}
#endif
		if ( Steam3Server().SteamGameServerUtils() )
		{
			bHaveValidSteamInterface = true;
			sunAppID = Steam3Server().SteamGameServerUtils()->GetAppID();
		}

		if ( !sunAppID )
			sunAppID = 215;	// defaults to Source SDK Base (215) if no steam.inf can be found.
	}
	
	return sunAppID;
}

EUniverse GetSteamUniverse()
{
#ifndef SWDS
	if ( Steam3Client().SteamUtils() )
		return Steam3Client().SteamUtils()->GetConnectedUniverse();
#endif
	if ( Steam3Server().SteamGameServerUtils() )
		return Steam3Server().SteamGameServerUtils()->GetConnectedUniverse();

	return k_EUniverseInvalid;
}

// Globals
int	gHostSpawnCount = 0;

// If any quit handlers balk, then aborts quit sequence
bool EngineTool_CheckQuitHandlers();

#if defined( _X360 )
CON_COMMAND( quit_x360, "" )
{
	int launchFlags = LF_EXITFROMGAME;

	// allocate the full payload
	int nPayloadSize = XboxLaunch()->MaxPayloadSize();
	byte *pPayload = (byte *)stackalloc( nPayloadSize );
	V_memset( pPayload, 0, sizeof( nPayloadSize ) );

	// payload is at least the command line
	// any user data needed must be placed AFTER the command line
	const char *pCmdLine = CommandLine()->GetCmdLine();
	int nCmdLineLength = (int)strlen( pCmdLine ) + 1;
	V_memcpy( pPayload, pCmdLine, min( nPayloadSize, nCmdLineLength ) );

	// add any other data here to payload, after the command line
	// ...

	// storage device may have changed since previous launch
	XboxLaunch()->SetStorageID( XBX_GetStorageDeviceId() );

	// Close the storage devices
	g_pXboxSystem->CloseContainers();
	// persist the user id
	bool bInviteRestart = args.FindArg( "invite" );
	DWORD nUserID = ( bInviteRestart ) ? XBX_GetInvitedUserId() : XBX_GetPrimaryUserId();
	XboxLaunch()->SetUserID( nUserID );

	if ( args.FindArg( "restart" ) )
	{
		launchFlags |= LF_GAMERESTART;
	}
	
	// If we're relaunching due to invite
	if ( bInviteRestart )
	{
		launchFlags |= LF_INVITERESTART;
		XNKID nSessionID = XBX_GetInviteSessionId();
		XboxLaunch()->SetInviteSessionID( &nSessionID );
	}

	bool bLaunch = XboxLaunch()->SetLaunchData( pPayload, nPayloadSize, launchFlags );
	if ( bLaunch )
	{
		COM_TimestampedLog( "Launching: \"%s\" Flags: 0x%8.8x", pCmdLine, XboxLaunch()->GetLaunchFlags() );
		g_pMaterialSystem->PersistDisplay();
		XBX_DisconnectConsoleMonitor();
		XboxLaunch()->Launch();
	}
}
#endif

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f( const CCommand &args )
{
#if !defined(SWDS)
	
	if ( args.FindArg( "prompt" ) )
	{
		// confirm they want to quit
		EngineVGui()->ConfirmQuit();
		return;
	}

	if ( !EngineTool_CheckQuitHandlers() )
	{
		return;
	}
#endif

	IGameEvent *event = g_GameEventManager.CreateEvent( "host_quit" );
	if ( event )
	{
		g_GameEventManager.FireEventClientSide( event );
	}

	HostState_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( _restart, "Shutdown and restart the engine." )
{
	/*
	// FIXME:  How to handle restarts?
#ifndef SWDS
	if ( !EngineTool_CheckQuitHandlers() )
	{
		return;
	}
#endif
	*/

	HostState_Restart();
}

#ifndef SWDS
//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
void Host_LightCrosshair (void);

static ConCommand light_crosshair( "light_crosshair", Host_LightCrosshair, "Show texture color at crosshair", FCVAR_CHEAT );

void Host_LightCrosshair (void)
{
	Vector endPoint;
	Vector lightmapColor;

	// max_range * sqrt(3)
	VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );
	
	R_LightVec( MainViewOrigin(), endPoint, true, lightmapColor );
	int r = LinearToTexture( lightmapColor.x );
	int g = LinearToTexture( lightmapColor.y );
	int b = LinearToTexture( lightmapColor.z );

	ConMsg( "Luxel Value: %d %d %d\n", r, g, b );
}
#endif

/*
==================
Host_Status_PrintClient

Print client info to console 
==================
*/
void Host_Status_PrintClient( IClient *client, bool bShowAddress, void (*print) (const char *fmt, ...) )
{
	INetChannelInfo *nci = client->GetNetChannel();

	const char *state = "challenging";
	if ( client->IsActive() )
		state = "active";
	else if ( client->IsSpawned() )
		state = "spawning";
	else if ( client->IsConnected() )
		state = "connecting";

	CStatusLineBuilder builder;
	builder.AddColumnText( "#", STATUS_COLUMN_LENGTH_LINEPREFIX );
	builder.AddColumnText( va( "%" STATUS_COLUMN_LENGTH_USERID_STR "i", client->GetUserID() ), STATUS_COLUMN_LENGTH_USERID );
	builder.AddColumnText( va( "\"%s\"", client->GetClientName() ), STATUS_COLUMN_LENGTH_NAME );
	builder.AddColumnText( client->GetNetworkIDString(), STATUS_COLUMN_LENGTH_STEAMID );

	if ( nci != NULL )
	{
		builder.AddColumnText( COM_FormatSeconds( nci->GetTimeConnected() ), STATUS_COLUMN_LENGTH_TIME );
		builder.AddColumnText( va( "%" STATUS_COLUMN_LENGTH_PING_STR "i", (int)(1000.0f*nci->GetAvgLatency( FLOW_OUTGOING )) ), STATUS_COLUMN_LENGTH_PING );
		builder.AddColumnText( va( "%" STATUS_COLUMN_LENGTH_LOSS_STR "i", (int)(100.0f*nci->GetAvgLoss(FLOW_INCOMING)) ), STATUS_COLUMN_LENGTH_LOSS );
		builder.AddColumnText( state, STATUS_COLUMN_LENGTH_STATE );
		if ( bShowAddress )
			builder.AddColumnText( nci->GetAddress(), STATUS_COLUMN_LENGTH_ADDR );
	}
	else
	{
		builder.InsertEmptyColumn( STATUS_COLUMN_LENGTH_TIME );
		builder.InsertEmptyColumn( STATUS_COLUMN_LENGTH_PING );
		builder.InsertEmptyColumn( STATUS_COLUMN_LENGTH_LOSS );
		builder.AddColumnText( state, STATUS_COLUMN_LENGTH_STATE );
	}

	print( "%s\n", builder.GetLine() );
}

void Host_Client_Printf(const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	host_client->ClientPrintf( "%s", string );
}

#define LIMIT_PER_CLIENT_COMMAND_EXECUTION_ONCE_PER_INTERVAL(seconds) \
	{ \
		static float g_flLastTime__Limit[ABSOLUTE_PLAYER_LIMIT] = { 0.0f }; /* we don't have access to any of the three MAX_PLAYERS #define's here unfortunately */ \
		int playerindex = cmd_clientslot; \
		if ( playerindex >= 0 && playerindex < (ARRAYSIZE(g_flLastTime__Limit)) && realtime - g_flLastTime__Limit[playerindex] > (seconds) ) \
		{ \
			g_flLastTime__Limit[playerindex] = realtime; \
		} \
		else \
		{ \
			return; \
		} \
	}

//-----------------------------------------------------------------------------
// Host_Status_f
//-----------------------------------------------------------------------------
CON_COMMAND( status, "Display map and connection status." )
{
	IClient	*client;
	int j;
	void (*print) (const char *fmt, ...);

#if defined( _X360 )
	Vector org;
	QAngle ang;
	const char *pName;

	if ( cl.IsActive() )
	{
		pName = cl.m_szLevelNameShort;
		org = MainViewOrigin();
		VectorAngles( MainViewForward(), ang );
		IClientEntity *localPlayer = entitylist->GetClientEntity( cl.m_nPlayerSlot + 1 );
		if ( localPlayer )
		{
			org = localPlayer->GetAbsOrigin();
		}
	}
	else
	{
		pName = "";
		org.Init();
		ang.Init();
	}

	// send to vxconsole
	xMapInfo_t mapInfo;
	mapInfo.position[0] = org[0];
	mapInfo.position[1] = org[1];
	mapInfo.position[2] = org[2];
	mapInfo.angle[0]    = ang[0];
	mapInfo.angle[1]    = ang[1];
	mapInfo.angle[2]    = ang[2];
	mapInfo.build       = build_number();
	mapInfo.skill       = skill.GetInt();

	// generate the qualified path where .sav files are expected to be written
	char savePath[MAX_PATH];
	V_snprintf( savePath, sizeof( savePath ), "%s", saverestore->GetSaveDir() );
	V_StripTrailingSlash( savePath );
	g_pFileSystem->RelativePathToFullPath( savePath, "MOD", mapInfo.savePath, sizeof( mapInfo.savePath ) );
	V_FixSlashes( mapInfo.savePath );

	if ( pName[0] )
	{
		// generate the qualified path from where the map was loaded
		char mapPath[MAX_PATH];
		Q_snprintf( mapPath, sizeof( mapPath ), "maps/%s.360.bsp", pName );
		g_pFileSystem->GetLocalPath( mapPath, mapInfo.mapPath, sizeof( mapInfo.mapPath ) );
		Q_FixSlashes( mapInfo.mapPath );
	}
	else
	{
		mapInfo.mapPath[0] = '\0';
	}

	XBX_rMapInfo( &mapInfo );
#endif

	if ( cmd_source == src_command )
	{
		if ( !sv.IsActive() )
		{
			Cmd_ForwardToServer( args );
			return;
		}
		print = ConMsg;
	}
	else
	{
		print = Host_Client_Printf;

		// limit this to once per 5 seconds
		LIMIT_PER_CLIENT_COMMAND_EXECUTION_ONCE_PER_INTERVAL(5.0);
	}

	// ============================================================
	// Server status information.
	print( "hostname: %s\n", host_name.GetString() );

	const char *pchSecureReasonString = "";
	const char *pchUniverse = "";
	bool bGSSecure = Steam3Server().BSecure();
	if ( !bGSSecure && Steam3Server().BWantsSecure() )
	{
		if ( Steam3Server().BLoggedOn() )
		{
			pchSecureReasonString = " (secure mode enabled, connected to Steam3)";
		}
		else
		{
			pchSecureReasonString = " (secure mode enabled, disconnected from Steam3)";
		}
	}

	switch ( GetSteamUniverse() )
	{
		case k_EUniversePublic:
			pchUniverse = "";
			break;
		case k_EUniverseBeta:
			pchUniverse = " (beta)";
			break;
		case k_EUniverseInternal:
			pchUniverse = " (internal)";
			break;
		case k_EUniverseDev:
			pchUniverse = " (dev)";
			break;
		default:
			pchUniverse = " (unknown)";
			break;
	}
	

	print( "version : %s/%d %d %s%s%s\n", GetSteamInfIDVersionInfo().szVersionString,
		PROTOCOL_VERSION, build_number(), bGSSecure ? "secure" : "insecure", pchSecureReasonString, pchUniverse );

	if ( NET_IsMultiplayer() )
	{
		CUtlString sPublicIPInfo;
		if ( !Steam3Server().BLanOnly() )
		{
			uint32 unPublicIP = Steam3Server().GetPublicIP();
			if ( unPublicIP != 0 )
			{
				netadr_t addr;
				addr.SetIP( unPublicIP );
				sPublicIPInfo.Format("  (public ip: %s)", addr.ToString( true ) );
			}
		}
		print( "udp/ip  : %s:%i%s\n", net_local_adr.ToString(true), sv.GetUDPPort(), sPublicIPInfo.String() );

		if ( !Steam3Server().BLanOnly() )
		{
			if ( Steam3Server().BLoggedOn() )
				print( "steamid : %s (%llu)\n", Steam3Server().SteamGameServer()->GetSteamID().Render(), Steam3Server().SteamGameServer()->GetSteamID().ConvertToUint64() );
			else
				print( "steamid : not logged in\n" );
		}
	}

	// Check if this game uses server registration, then output status
	ConVarRef sv_registration_successful( "sv_registration_successful", true );
	if ( sv_registration_successful.IsValid() )
	{
		CUtlString sExtraInfo;
		ConVarRef sv_registration_message( "sv_registration_message", true );
		if ( sv_registration_message.IsValid() )
		{
			const char *msg = sv_registration_message.GetString();
			if ( msg && *msg )
			{
				sExtraInfo.Format("  (%s)", msg );
			}
		}

		if ( sv_registration_successful.GetBool() )
		{
			print( "account : logged in%s\n", sExtraInfo.String() );
		}
		else
		{
			print( "account : not logged in%s\n", sExtraInfo.String() );
		}
	}

	print( "map     : %s at: %d x, %d y, %d z\n", sv.GetMapName(), (int)MainViewOrigin()[0], (int)MainViewOrigin()[1], (int)MainViewOrigin()[2]);
	static ConVarRef sv_tags( "sv_tags" );
	print( "tags    : %s\n", sv_tags.GetString() );

	if ( hltv && hltv->IsActive() )
	{
		print( "sourcetv:  port %i, delay %.1fs\n", hltv->GetUDPPort(), hltv->GetDirector()->GetDelay() );
	}

#if defined( REPLAY_ENABLED )
	if ( replay && replay->IsActive() )
	{
		print( "replay  :  %s\n", replay->IsRecording() ? "recording" : "not recording" );
	}
#endif

	int players = sv.GetNumClients();
	int nBots = sv.GetNumFakeClients();
	int nHumans = players - nBots;

	print( "players : %i humans, %i bots (%i max)\n", nHumans, nBots, sv.GetMaxClients() );
	// ============================================================

	print( "edicts  : %d used of %d max\n", sv.num_edicts - sv.free_edicts, sv.max_edicts );

	if ( ( g_iServerGameDLLVersion >= 10 ) && serverGameDLL )
	{
		serverGameDLL->Status( print );
	}

	// Early exit for this server.
	if ( args.ArgC() == 2 )
	{
		if ( !Q_stricmp( args[1], "short" ) )
		{
			for ( j=0 ; j < sv.GetClientCount() ; j++ )
			{
				client = sv.GetClient( j );

				if ( !client->IsActive() )
					continue;

				print( "#%i - %s\n" , j + 1, client->GetClientName() );
			}
			return;
		}
	}

	// the header for the status rows
	// print( "# userid %-19s %-19s connected ping loss state%s\n", "name", "uniqueid", cmd_source == src_command ? "  adr" : "" );
	CStatusLineBuilder header;
	header.AddColumnText( "#", STATUS_COLUMN_LENGTH_LINEPREFIX );
	header.AddColumnText( "userid", STATUS_COLUMN_LENGTH_USERID );
	header.AddColumnText( "name", STATUS_COLUMN_LENGTH_NAME );
	header.AddColumnText( "uniqueid", STATUS_COLUMN_LENGTH_STEAMID );
	header.AddColumnText( "connected", STATUS_COLUMN_LENGTH_TIME );
	header.AddColumnText( "ping", STATUS_COLUMN_LENGTH_PING );
	header.AddColumnText( "loss", STATUS_COLUMN_LENGTH_LOSS );
	header.AddColumnText( "state", STATUS_COLUMN_LENGTH_STATE );
	if ( cmd_source == src_command )
	{
		header.AddColumnText( "adr", STATUS_COLUMN_LENGTH_ADDR );
	}

	print( "%s\n", header.GetLine() );

	for ( j=0 ; j < sv.GetClientCount() ; j++ )
	{
		client = sv.GetClient( j );

		if ( !client->IsConnected() )
			continue; // not connected yet, maybe challenging
		
		Host_Status_PrintClient( client, (cmd_source == src_command), print );
	}
}


//-----------------------------------------------------------------------------
// Host_Ping_f
//-----------------------------------------------------------------------------
CON_COMMAND( ping, "Display ping to server." )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer( args );
		return;
	}
	// limit this to once per 5 seconds
	LIMIT_PER_CLIENT_COMMAND_EXECUTION_ONCE_PER_INTERVAL(5.0);

	host_client->ClientPrintf( "Client ping times:\n" );

	for ( int i=0; i< sv.GetClientCount(); i++ )
	{
		IClient	*client = sv.GetClient(i);

		if ( !client->IsConnected() || client->IsFakeClient() )
			continue;

		host_client->ClientPrintf ("%4.0f ms : %s\n", 
			1000.0f * client->GetNetChannel()->GetAvgLatency( FLOW_OUTGOING ), client->GetClientName() );
	}
}

bool CL_HL2Demo_MapCheck( const char *name )
{
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() )
	{
		if (    !Q_stricmp( name, "d1_trainstation_01" ) || 
				!Q_stricmp( name, "d1_trainstation_02" ) || 
				!Q_stricmp( name, "d1_town_01" ) || 
				!Q_stricmp( name, "d1_town_01a" ) || 
				!Q_stricmp( name, "d1_town_02" ) || 
				!Q_stricmp( name, "d1_town_03" ) ||
				!Q_stricmp( name, "background01" ) ||
				!Q_stricmp( name, "background03" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

bool CL_PortalDemo_MapCheck( const char *name )
{
	if ( IsPC() && CL_IsPortalDemo() && !sv.IsDedicated() )
	{
		if (    !Q_stricmp( name, "testchmb_a_00" ) || 
				!Q_stricmp( name, "testchmb_a_01" ) || 
				!Q_stricmp( name, "testchmb_a_02" ) || 
				!Q_stricmp( name, "testchmb_a_03" ) || 
				!Q_stricmp( name, "testchmb_a_04" ) || 
				!Q_stricmp( name, "testchmb_a_05" ) ||
				!Q_stricmp( name, "testchmb_a_06" ) ||
				!Q_stricmp( name, "background1" ) 
			)
		{
			return true;
		}
		return false;
	}

	return true;
}

int _Host_Map_f_CompletionFunc( char const *cmdname, char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

// Note, leaves name alone if no match possible
static bool Host_Map_Helper_FuzzyName( const CCommand &args, char *name, size_t bufsize )
{
	char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
	CUtlString argv0;
	argv0 = args.Arg( 0 );
	argv0 += " ";

	if ( _Host_Map_f_CompletionFunc( argv0, args.ArgS(), commands ) > 0 )
	{
		Q_strncpy( name, &commands[ 0 ][ argv0.Length() ], bufsize );
		return true;
	}
	return false;
}

void Host_Map_Helper( const CCommand &args, bool bEditmode, bool bBackground, bool bCommentary )
{
	if ( cmd_source != src_command )
		return;
	if (args.ArgC() < 2)
	{
		Warning("No map specified\n");
		return;
	}

	const char *pszReason = NULL;
	if ( ( g_iServerGameDLLVersion >= 10 ) && !serverGameDLL->IsManualMapChangeOkay( &pszReason ) )
	{
		if ( pszReason && pszReason[0] )
		{
			Warning( "%s\n", pszReason );
		}
		return;
	}

	char szMapName[ MAX_QPATH ] = { 0 };
	V_strncpy( szMapName, args[ 1 ], sizeof( szMapName ) );

	// Call find map, proceed for any value besides NotFound
	IVEngineServer::eFindMapResult eResult = g_pVEngineServer->FindMap( szMapName, sizeof( szMapName ) );
	if ( eResult == IVEngineServer::eFindMap_NotFound )
	{
		Warning( "map load failed: %s not found or invalid\n", args[ 1 ] );
		return;
	}

	COM_TimestampedLog( "*** Map Load: %s", szMapName );

	// There is a precision issue here, as described Bruce Dawson's blog.
	// In our case, we don't care because we're looking for anything on the order of second precision, which 
	// covers runtime up to around 4 months.
	static ConVarRef dev_loadtime_map_start( "dev_loadtime_map_start" );
	dev_loadtime_map_start.SetValue( (float)Plat_FloatTime() );

	// If I was in edit mode reload config file
	// to overwrite WC edit key bindings
#if !defined(SWDS)
	if ( !bEditmode )
	{
		if ( g_bInEditMode )
		{
			// Re-read config from disk
			Host_ReadConfiguration();
			g_bInEditMode = false;
		}
	}
	else
	{
		g_bInEditMode = true;
	}

	g_bInCommentaryMode = bCommentary;
#endif

	if ( !CL_HL2Demo_MapCheck( szMapName ) )
	{
		Warning( "map load failed: %s not found or invalid\n", szMapName );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( szMapName ) )
	{
		Warning( "map load failed: %s not found or invalid\n", szMapName );
		return;	
	}

#if defined( REPLAY_ENABLED )
	// If we're recording the game, finalize the replay so players can download it.
	if ( g_pReplay && g_pReplay->IsRecording() )
	{
		g_pReplay->SV_EndRecordingSession();
	}
#endif

	// Stop demo loop
	cl.demonum = -1;

	Host_Disconnect( false );	// stop old game

	HostState_NewGame( szMapName, false, bBackground );

	if (args.ArgC() == 10)
	{
		if (Q_stricmp(args[2], "setpos") == 0
			&& Q_stricmp(args[6], "setang") == 0) 
		{
			Vector newpos;
			newpos.x = atof( args[3] );
			newpos.y = atof( args[4] );
			newpos.z = atof( args[5] );

			QAngle newangle;
			newangle.x = atof( args[7] );
			newangle.y = atof( args[8] );
			newangle.z = atof( args[9] );
			
			HostState_SetSpawnPoint(newpos, newangle);
		}
	}
}

/*
======================
Host_Map_f

handle a 
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f( const CCommand &args )
{
	Host_Map_Helper( args, false, false, false );
}


//-----------------------------------------------------------------------------
// handle a map_edit <servername> command from the console. 
// Active clients are kicked off.
// UNDONE: protect this from use if not in dev. mode
//-----------------------------------------------------------------------------
#ifndef SWDS
CON_COMMAND( map_edit, "" )
{
	Host_Map_Helper( args, true, false, false );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Runs a map as the background
//-----------------------------------------------------------------------------
void Host_Map_Background_f( const CCommand &args )
{
	Host_Map_Helper( args, false, true, false );
}


//-----------------------------------------------------------------------------
// Purpose: Runs a map in commentary mode
//-----------------------------------------------------------------------------
void Host_Map_Commentary_f( const CCommand &args )
{
	Host_Map_Helper( args, false, false, true );
}


//-----------------------------------------------------------------------------
// Restarts the current server for a dead player
//-----------------------------------------------------------------------------
CON_COMMAND( restart, "Restart the game on the same level (add setpos to jump to current view position on restart)." )
{
	if ( 
#if !defined(SWDS)
		demoplayer->IsPlayingBack() || 
#endif
		!sv.IsActive() )
		return;

	if ( sv.IsMultiplayer() )
		return;

	if ( cmd_source != src_command )
		return;

	bool bRememberLocation = ( args.ArgC() == 2 && !Q_stricmp( args[1], "setpos" ) );

	Host_Disconnect(false);	// stop old game

	if ( !CL_HL2Demo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	if ( !CL_PortalDemo_MapCheck( sv.GetMapName() ) )
	{
		Warning( "map load failed: %s not found or invalid\n", sv.GetMapName() );
		return;	
	}

	HostState_NewGame( sv.GetMapName(), bRememberLocation, false );
}


//-----------------------------------------------------------------------------
// Restarts the current server for a dead player
//-----------------------------------------------------------------------------
CON_COMMAND( reload, "Reload the most recent saved game (add setpos to jump to current view position on reload).")
{
#ifndef SWDS
	const char *pSaveName;
	char name[MAX_OSPATH];
#endif

	if ( 
#if !defined(SWDS)
		demoplayer->IsPlayingBack() || 
#endif
		!sv.IsActive() )
		return;

	if ( sv.IsMultiplayer() )
		return;

	if (cmd_source != src_command)
		return;

	bool remember_location = false;
	if ( args.ArgC() == 2 && 
		!Q_stricmp( args[1], "setpos" ) )
	{
		remember_location = true;
	}

	// See if there is a most recently saved game
	// Restart that game if there is
	// Otherwise, restart the starting game map
#ifndef SWDS
	pSaveName = saverestore->FindRecentSave( name, sizeof( name ) );

	// Put up loading plaque
  	SCR_BeginLoadingPlaque();

	Host_Disconnect( false );	// stop old game

	if ( pSaveName && saverestore->SaveFileExists( pSaveName ) )
	{
		HostState_LoadGame( pSaveName, remember_location );
	}
	else
#endif
	{
		if ( !CL_HL2Demo_MapCheck( host_map.GetString() ) )
		{
			Warning( "map load failed: %s not found or invalid\n", host_map.GetString() );
			return;	
		}

		if ( !CL_PortalDemo_MapCheck( host_map.GetString() ) )
		{
			Warning( "map load failed: %s not found or invalid\n", host_map.GetString() );
			return;	
		} 

		HostState_NewGame( host_map.GetString(), remember_location, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Goes to a new map, taking all clients along
// Output : void Host_Changelevel_f
//-----------------------------------------------------------------------------
void Host_Changelevel_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "changelevel <levelname> : continue game on a new level\n" );
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "Can't changelevel, not running server\n" );
		return;
	}

	char szName[MAX_PATH] = { 0 };
	V_strncpy( szName, args[1], sizeof( szName ) );

	// Call find map to attempt to resolve fuzzy/non-canonical map names
	IVEngineServer::eFindMapResult eResult = g_pVEngineServer->FindMap( szName, sizeof( szName ) );
	if ( eResult == IVEngineServer::eFindMap_NotFound )
	{
		// Warn, but but proceed even if the map is not found, such that we hit the proper server_levelchange_failed
		// codepath and event later on.
		Warning( "Failed to find map %s\n", args[ 1 ] );
	}

	if ( !CL_HL2Demo_MapCheck(szName) )
	{
		Warning( "changelevel failed: %s not found\n", szName );
		return;
	}

	if ( !CL_PortalDemo_MapCheck(szName) )
	{
		Warning( "changelevel failed: %s not found\n", szName );
		return;
	}

	const char *pszReason = NULL;
	if ( ( g_iServerGameDLLVersion >= 10 ) && !serverGameDLL->IsManualMapChangeOkay( &pszReason ) )
	{
		if ( pszReason && pszReason[0] )
		{
			Warning( "%s", pszReason );
		}
		return;
	}

	HostState_ChangeLevelMP( szName, args[2] );
}

//-----------------------------------------------------------------------------
// Purpose: Changing levels within a unit, uses save/restore
//-----------------------------------------------------------------------------
void Host_Changelevel2_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg ("changelevel2 <levelname> : continue game on a new level in the unit\n");
		return;
	}

	if ( !sv.IsActive() || sv.IsMultiplayer() )
	{
		ConMsg( "Can't changelevel2, not in a single-player map\n" );
		return;
	}

	char szName[MAX_PATH] = { 0 };
	V_strncpy( szName, args[1], sizeof( szName ) );
	IVEngineServer::eFindMapResult eResult = g_pVEngineServer->FindMap( szName, sizeof( szName ) );
	if ( eResult == IVEngineServer::eFindMap_NotFound )
	{
		if ( !CL_IsHL2Demo() || (CL_IsHL2Demo() && !(!Q_stricmp( szName, "d1_trainstation_03" ) || !Q_stricmp( szName, "d1_town_02a" ))) )	
		{
			Warning( "changelevel2 failed: %s not found\n", szName );
			return;
		}
	}

#if !defined(SWDS)
	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() && !Q_stricmp( szName, "d1_trainstation_03" ) ) 
	{
		void CL_DemoTransitionFromTrainstation();
		CL_DemoTransitionFromTrainstation();
		return; 
	}

	// needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't a valid map
	if ( IsPC() && CL_IsHL2Demo() && !sv.IsDedicated() && !Q_stricmp( szName, "d1_town_02a" ) && !Q_stricmp( args[2], "d1_town_02_02a" )) 
	{
		void CL_DemoTransitionFromRavenholm();
		CL_DemoTransitionFromRavenholm();
		return; 
	}

	if ( IsPC() && CL_IsPortalDemo() && !sv.IsDedicated() && !Q_stricmp( szName, "testchmb_a_07" ) ) 
	{
		void CL_DemoTransitionFromTestChmb();
		CL_DemoTransitionFromTestChmb();
		return; 
	}

#endif

	// allow a level transition to d1_trainstation_03 so the Host_Changelevel() can act on it
	if ( !CL_HL2Demo_MapCheck( szName ) ) 
	{
		Warning( "changelevel failed: %s not found\n", szName );
		return;	
	}

	HostState_ChangeLevelSP( szName, args[2] );
}


//-----------------------------------------------------------------------------
// Purpose: Shut down client connection and any server
//-----------------------------------------------------------------------------
void Host_Disconnect( bool bShowMainMenu, const char *pszReason )
{
	if ( IsX360() )
	{
		g_pQueuedLoader->EndMapLoading( false );
	}

#ifndef SWDS
	if ( !sv.IsDedicated() )
	{
		cl.Disconnect( pszReason, bShowMainMenu );
	}
#endif
	Host_AllowQueuedMaterialSystem( false );
	HostState_GameShutdown();
}

void Disconnect()
{
	cl.demonum = -1;
	Host_Disconnect(true);

#if defined( REPLAY_ENABLED )
	// Finalize the recording replay on the server, if is recording.
	// NOTE: We don't want this in Host_Disconnect() as that would be called more
	// than necessary.
	if ( g_pReplay && g_pReplay->IsReplayEnabled() && sv.IsDedicated() )
	{
		g_pReplay->SV_EndRecordingSession();
	}
#endif
}

//-----------------------------------------------------------------------------
// Kill the client and any local server.
//-----------------------------------------------------------------------------
CON_COMMAND( disconnect, "Disconnect game from server." )
{
#if !defined( SWDS )
	// Just run the regular Disconnect function if we're not the client or the client didn't handle it for us
	if( !g_ClientDLL || !g_ClientDLL->DisconnectAttempt() )
	{
		Disconnect();
	}
#else
	Disconnect();
#endif
}

#ifdef _WIN32
// manually pull in the GetEnvironmentVariableA defn so we don't need to include windows.h
extern "C"
{
DWORD __declspec(dllimport) __stdcall GetEnvironmentVariableA( const char *, char *, DWORD );
}
#endif // _WIN32

CON_COMMAND( version, "Print version info string." )
{
	ConMsg( "Build Label:          %8d   # Uniquely identifies each build\n", GetSteamInfIDVersionInfo().ServerVersion );
	ConMsg( "Network PatchVersion: %8s   # Determines client and server compatibility\n", GetSteamInfIDVersionInfo().szVersionString );
	ConMsg( "Protocol version:     %8d   # High level network protocol version\n", PROTOCOL_VERSION );

	if ( sv.IsDedicated() || serverGameDLL )
	{
		ConMsg( "Server version:       %8i\n", GetSteamInfIDVersionInfo().ServerVersion );
		ConMsg( "Server AppID:         %8i\n", GetSteamInfIDVersionInfo().ServerAppID );
	}
	if ( !sv.IsDedicated() )
	{
		ConMsg( "Client version:       %8i\n", GetSteamInfIDVersionInfo().ClientVersion );
		ConMsg( "Client AppID:         %8i\n", GetSteamInfIDVersionInfo().AppID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( pause, "Toggle the server pause state." )
{
#ifndef SWDS
	if ( !sv.IsDedicated() )
	{
		if ( !cl.m_szLevelFileName[ 0 ] )
			return;
	}
#endif

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer( args );
		return;
	}

	if ( !sv.IsPausable() )
		return;

	// toggle paused state
	sv.SetPaused( !sv.IsPaused() );
	
	// send text messaage who paused the game
	sv.BroadcastPrintf( "%s %s the game\n", host_client->GetClientName(), sv.IsPaused() ? "paused" : "unpaused" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( setpause, "Set the pause state of the server." )
{
#ifndef SWDS
	if ( !cl.m_szLevelFileName[ 0 ] )
		return;
#endif

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer( args );
		return;
	}

	sv.SetPaused( true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( unpause, "Unpause the game." )
{
#ifndef SWDS
	if ( !cl.m_szLevelFileName[ 0 ] )
		return;
#endif

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer( args );
		return;
	}
	
	sv.SetPaused( false );
}

// No non-testing use for this at the moment, though server mods in public will expose similar functionality
#if defined( STAGING_ONLY ) || defined( _DEBUG )
//-----------------------------------------------------------------------------
// Purpose: Send a string command to a client by userid
//-----------------------------------------------------------------------------
CON_COMMAND( clientcmd, "Send a clientside command to a player by userid" )
{
	if ( args.ArgC() <= 2 )
	{
		ConMsg( "Usage:  clientcmd < userid > { command string }\n" );
		return;
	}

	// Args
	int userid = Q_atoi( args[1] );
	int messageArgStart = 2;

	// Concatenate other arguments into string
	CUtlString commandString;

	commandString.SetLength( Q_strlen( args.ArgS() ) );
	commandString.Set( args[ messageArgStart ] );
	for ( int i = messageArgStart + 1; i < args.ArgC(); i++ )
	{
		commandString.Append( " " );
		commandString.Append( args[i] );
	}

	// find client
	IClient *client = NULL;
	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		IClient *searchclient = sv.GetClient( i );

		if ( !searchclient->IsConnected() )
			continue;

		if ( userid != -1 && searchclient->GetUserID() == userid )
		{
			client = searchclient;
			break;
		}
	}

	if ( !client )
	{
		ConMsg( "userid \"%d\" not found\n", userid );
		return;
	}

	NET_StringCmd cmdMsg( commandString ) ;
	client->SendNetMsg( cmdMsg, true );
}
#endif // defined( STAGING_ONLY ) || defined( _DEBUG )

//-----------------------------------------------------------------------------
// Kicks a user off of the server using their userid or uniqueid
//-----------------------------------------------------------------------------
CON_COMMAND( kickid, "Kick a player by userid or uniqueid, with a message." )
{
	char		*who = NULL;
	const char	*pszArg1 = NULL, *pszMessage = NULL;
	IClient		*client = NULL;
	int			iSearchIndex = -1;
	char		szSearchString[128];
	int			argsStartNum = 1;
	bool		bSteamID = false;
	int			i = 0;

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kickid < userid | uniqueid > { message }\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// if the first letter is a character then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( !Q_strnicmp( pszArg1, STEAM_PREFIX, strlen( STEAM_PREFIX ) ) && Q_strstr( args[2], ":" ) )
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
			argsStartNum = 5;
			bSteamID = true;
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s", pszArg1 );
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = Q_atoi( pszArg1 );
	}

	// check for a message
	if ( args.ArgC() > argsStartNum )
	{
		int j;
		int dataLen = 0;

		pszMessage = args.ArgS();
		for ( j = 1; j <= argsStartNum; j++ )
		{
			dataLen += Q_strlen( args[j] ) + 1; // +1 for the space between args
		}

		if ( bSteamID )
		{
			dataLen -= 5; // SteamIDs don't have spaces between the args[) values
		}

		if ( dataLen > Q_strlen( pszMessage ) ) // saftey check
		{
			pszMessage = NULL;
		}
		else
		{
			pszMessage += dataLen;
		}
	}

	// find this client
	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.GetClient( i );

		if ( !client->IsConnected() )
			continue;

#if defined( REPLAY_ENABLED )
		if ( client->IsReplay() )
			continue;
#endif

		if ( client->IsHLTV() )
			continue;

		// searching by UserID
		if ( iSearchIndex != -1 )
		{
			if ( client->GetUserID() == iSearchIndex )
			{
				// found!
				break;
			}
		}
		// searching by UniqueID
		else	
		{
			if ( Q_stricmp( client->GetNetworkIDString(), szSearchString ) == 0 ) 
			{
				// found!
				break;
			}
		}
	}

	// now kick them
	if ( i < sv.GetClientCount() )
	{
		if ( cmd_source != src_command )
		{
			who = host_client->m_Name;
		}

		// can't kick yourself!
		if ( cmd_source != src_command && host_client == client && !sv.IsDedicated() )
		{
			return;
		}

		if ( iSearchIndex != -1 || !client->IsFakeClient() )
		{
			if ( who == NULL )
			{
				if ( pszMessage )
				{
					client->Disconnect( "%s", pszMessage );
				}
				else
				{
					client->Disconnect( KICKED_BY_CONSOLE );
				}
			}
			else
			{
				if ( pszMessage )
				{
					client->Disconnect( "Kicked by %s : %s", who, pszMessage );
				}
				else
				{
					client->Disconnect( "Kicked by %s", who );
				}
			}
		}
	}
	else
	{
		if ( iSearchIndex != -1 )
		{
			ConMsg( "userid \"%d\" not found\n", iSearchIndex );
		}
		else
		{
			ConMsg( "uniqueid \"%s\" not found\n", szSearchString );			
		}
	}
}

/*
==================
Host_Kick_f

Kicks a user off of the server using their name
==================
*/
CON_COMMAND( kick, "Kick a player by name." )
{
	char		*who = NULL;
	char		*pszName = NULL;
	IClient		*client = NULL;
	int			i = 0;
	char		name[64];

	if ( args.ArgC() <= 1 )
	{
		ConMsg( "Usage:  kick < name >\n" );
		return;
	}

	// copy the name to a local buffer
	memset( name, 0, sizeof(name) );
	Q_strncpy( name, args.ArgS(), sizeof(name) );
	pszName = name;

	// safety check
	if ( pszName && pszName[0] != 0 )
	{
		//HACK-HACK
		// check for the name surrounded by quotes (comes in this way from rcon)
		int len = Q_strlen( pszName ) - 1; // (minus one since we start at 0)
		if ( pszName[0] == '"' && pszName[len] == '"' )
		{
			// get rid of the quotes at the beginning and end
			pszName[len] = 0;
			pszName++;
		}

		for ( i = 0; i < sv.GetClientCount(); i++ )
		{
			client = sv.GetClient(i);

			if ( !client->IsConnected() )
				continue;

#if defined( REPLAY_ENABLED )
			if ( client->IsReplay() )
				continue;
#endif

			if ( client->IsHLTV() )
				continue;

			// found!
			if ( Q_strcasecmp( client->GetClientName(), pszName ) == 0 ) 
				break;
		}

		// now kick them
		if ( i < sv.GetClientCount() )
		{
			if ( cmd_source != src_command )
			{
				who = host_client->m_Name;
			}

			// can't kick yourself!
			if ( cmd_source != src_command && host_client == client && !sv.IsDedicated() )
				return;

			if ( who )
			{
				client->Disconnect( "Kicked by %s", who );
			}
			else
			{
				client->Disconnect( KICKED_BY_CONSOLE );
			}
		}
		else
		{
			ConMsg( "name \"%s\" not found\n", pszName );
		}
	}
}

//-----------------------------------------------------------------------------
// Kicks all users off of the server
//-----------------------------------------------------------------------------
CON_COMMAND( kickall, "Kicks everybody connected with a message." )
{
	char		*who = NULL;
	IClient		*client = NULL;
	int			i = 0;
	char		szMessage[128];

	// copy the message to a local buffer
	memset( szMessage, 0, sizeof(szMessage) );
	V_strcpy_safe( szMessage, args.ArgS() );

	if ( cmd_source != src_command )
	{
		who = host_client->m_Name;
	}

	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.GetClient(i);

		if ( !client->IsConnected() )
			continue;

		// can't kick yourself!
		if ( cmd_source != src_command && host_client == client && !sv.IsDedicated() )
			continue;

#if defined( REPLAY_ENABLED )
		if ( client->IsReplay() )
			continue;
#endif

		if ( client->IsHLTV() )
			continue;

		if ( who )
		{
			if ( szMessage[0] )
			{
				client->Disconnect( "Kicked by %s : %s", who, szMessage );
			}
			else
			{
				client->Disconnect( "Kicked by %s", who );
			}
		}
		else
		{
			if ( szMessage[0] )
			{
				client->Disconnect( "%s", szMessage );
			}
			else
			{
				client->Disconnect( KICKED_BY_CONSOLE );
			}
		}
	}
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/


//-----------------------------------------------------------------------------
// Dump memory stats
//-----------------------------------------------------------------------------
CON_COMMAND( memory, "Print memory stats." )
{
#if !defined(NO_MALLOC_OVERRIDE)
	ConMsg( "Heap Used:\n" );
	int nTotal = MemAlloc_GetSize( 0 );
	if (nTotal == -1)
	{
		ConMsg( "Corrupted!\n" );
	}
	else
	{
		ConMsg( "%5.2f MB (%d bytes)\n", nTotal/(1024.0f*1024.0f), nTotal );
	}
#endif

#ifdef VPROF_ENABLED
	ConMsg("\nVideo Memory Used:\n");
	CVProfile *pProf = &g_VProfCurrentProfile;
	int prefixLen = strlen( "TexGroup_Global_" );
	float total = 0.0f;
	for ( int i=0; i < pProf->GetNumCounters(); i++ )
	{
		if ( pProf->GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
		{
			float value = pProf->GetCounterValue( i ) * (1.0f/(1024.0f*1024.0f) );
			total += value;
			const char *pName = pProf->GetCounterName( i );
			if ( !Q_strnicmp( pName, "TexGroup_Global_", prefixLen ) )
			{
				pName += prefixLen;
			}
			ConMsg( "%5.2f MB: %s\n", value, pName );
		}
	}
	ConMsg("------------------\n");
	ConMsg( "%5.2f MB: total\n", total );
#endif

	ConMsg( "\nHunk Memory Used:\n" );
	Hunk_Print();
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


#ifndef SWDS

//MOTODO move all demo commands to demoplayer


//-----------------------------------------------------------------------------
// Purpose: Gets number of valid demo names
// Output : int
//-----------------------------------------------------------------------------
int Host_GetNumDemos()
{
	int c = 0;
#ifndef SWDS
	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = cl.demos[ i ].Get();
		if ( !demoname[ 0 ] )
			break;

		++c;
	}
#endif
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_PrintDemoList()
{
	int count = Host_GetNumDemos();

	int next = cl.demonum;
	if ( next >= count || next < 0 )
	{
		next = 0;
	}

#ifndef SWDS
	for ( int i = 0; i < MAX_DEMOS; ++i )
	{
		const char *demoname = cl.demos[ i ].Get();
		if ( !demoname[ 0 ] )
			break;

		bool isnextdemo = next == i ? true : false;

		DevMsg( "%3s % 2i : %20s\n", isnextdemo ? "-->" : "   ", i, cl.demos[ i ].Get() );
	}
#endif

	if ( !count )
	{
		DevMsg( "No demos in list, use startdemos <demoname> <demoname2> to specify\n" );
	}
}


#ifndef SWDS
//-----------------------------------------------------------------------------
//
// Con commands related to demos, not available on dedicated servers
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Specify list of demos for the "demos" command
//-----------------------------------------------------------------------------
CON_COMMAND( startdemos, "Play demos in demo sequence." )
{
	int	c = args.ArgC() - 1;
	if (c > MAX_DEMOS)
	{
		Msg ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Msg ("%i demo(s) in loop\n", c);

	for ( int i=1 ; i<c+1 ; i++ )
	{
		cl.demos[i-1] = args[i];
	}

	cl.demonum = 0;

	Host_PrintDemoList();

	if ( !sv.IsActive() && !demoplayer->IsPlayingBack() )
	{
		CL_NextDemo ();
	}
	else
	{
		cl.demonum = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return to looping demos, optional resume demo index
//-----------------------------------------------------------------------------
CON_COMMAND( demos, "Demo demo file sequence." )
{
	int oldn = cl.demonum;
	cl.demonum = -1;
	Host_Disconnect(false);
	cl.demonum = oldn;

	if (cl.demonum == -1)
		cl.demonum = 0;

	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			cl.demonum = clamp( Q_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", cl.demos[ cl.demonum ].Get() );
		}
	}

	Host_PrintDemoList();

	CL_NextDemo ();
}

//-----------------------------------------------------------------------------
// Purpose: Stop current demo
//-----------------------------------------------------------------------------
CON_COMMAND_F( stopdemo, "Stop playing back a demo.", FCVAR_DONTRECORD )
{
	if ( !demoplayer->IsPlayingBack() )
		return;
	
	Host_Disconnect (true);
}

//-----------------------------------------------------------------------------
// Purpose: Skip to next demo
//-----------------------------------------------------------------------------
CON_COMMAND( nextdemo, "Play next demo in sequence." )
{
	if ( args.ArgC() == 2 )
	{
		int numdemos = Host_GetNumDemos();
		if ( numdemos >= 1 )
		{
			cl.demonum = clamp( Q_atoi( args[1] ), 0, numdemos - 1 );
			DevMsg( "Jumping to %s\n", cl.demos[ cl.demonum ].Get() );
		}
	}
	Host_EndGame( false, "Moving to next demo..." );
}

//-----------------------------------------------------------------------------
// Purpose: Print out the current demo play order
//-----------------------------------------------------------------------------
CON_COMMAND( demolist, "Print demo sequence list." )
{
	Host_PrintDemoList();
}


//-----------------------------------------------------------------------------
// Purpose: Host_Soundfade_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( soundfade, "Fade client volume.", FCVAR_SERVER_CAN_EXECUTE )
{
	float percent;
	float inTime, holdTime, outTime;

	if (args.ArgC() != 3 && args.ArgC() != 5)
	{
		Msg("soundfade <percent> <hold> [<out> <int>]\n");
		return;
	}

	percent = clamp( (float) atof(args[1]), 0.0f, 100.0f );
	
	holdTime = max( 0., atof(args[2]) );

	inTime = 0.0f;
	outTime = 0.0f;
	if (args.ArgC() == 5)
	{
		outTime = max( 0., atof(args[3]) );
		inTime = max( 0., atof( args[4]) );
	}

	S_SoundFade( percent, holdTime, outTime, inTime );
}

#endif // !SWDS

#endif


//-----------------------------------------------------------------------------
// Shutdown the server
//-----------------------------------------------------------------------------
CON_COMMAND( killserver, "Shutdown the server." )
{
	Host_Disconnect(true);
	
	if ( !sv.IsDedicated() )
	{
		// close network sockets
		NET_SetMutiplayer( false );
	}
}

#if !defined(SWDS)
void Host_VoiceRecordStart_f(void)
{
#ifdef VOICE_VOX_ENABLE
	ConVarRef voice_vox( "voice_vox" );
	if ( voice_vox.IsValid() && voice_vox.GetBool() )
		return;
#endif // VOICE_VOX_ENABLE

	if ( cl.IsActive() )
	{
		const char *pUncompressedFile = NULL;
		const char *pDecompressedFile = NULL;
		const char *pInputFile = NULL;
		
		if (voice_recordtofile.GetInt())
		{
			pUncompressedFile = "voice_micdata.wav";
			pDecompressedFile = "voice_decompressed.wav";
		}

		if (voice_inputfromfile.GetInt())
		{
			pInputFile = "voice_input.wav";
		}
		if ( !sv_allow_voice_from_file.GetBool() )
		{
			pInputFile = NULL;
		}
#if !defined( NO_VOICE )
		if (Voice_RecordStart(pUncompressedFile, pDecompressedFile, pInputFile))
		{
		}
#endif
	}
}


void Host_VoiceRecordStop_f(void)
{
#ifdef VOICE_VOX_ENABLE
	ConVarRef voice_vox( "voice_vox" );
	if ( voice_vox.IsValid() && voice_vox.GetBool() )
		return;
#endif // VOICE_VOX_ENABLE

	if ( cl.IsActive() )
	{
#if !defined( NO_VOICE )
		if (Voice_IsRecording())
		{
			CL_SendVoicePacket( g_bUsingSteamVoice ? false : true );
			Voice_UserDesiresStop();
		}
#endif
	}
}

#ifdef VOICE_VOX_ENABLE
void Host_VoiceToggle_f( const CCommand &args )
{
	if ( cl.IsActive() )
	{
#if !defined( NO_VOICE )	
		bool bToggle = false;

		if ( args.ArgC() == 2 && V_strcasecmp( args[1], "on" ) == 0 )
		{
			bToggle = true;
		}

		if ( Voice_IsRecording() && bToggle == false )
		{
			CL_SendVoicePacket( g_bUsingSteamVoice ? false : true );
			Voice_UserDesiresStop();
		}
		else if ( !Voice_IsRecording() && bToggle == true )
		{
			const char *pUncompressedFile = NULL;
			const char *pDecompressedFile = NULL;
			const char *pInputFile = NULL;

			if (voice_recordtofile.GetInt())
			{
				pUncompressedFile = "voice_micdata.wav";
				pDecompressedFile = "voice_decompressed.wav";
			}

			if (voice_inputfromfile.GetInt())
			{
				pInputFile = "voice_input.wav";
			}
			if ( !sv_allow_voice_from_file.GetBool() )
			{
				pInputFile = NULL;
			}

			Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
		}
#endif // NO_VOICE
	}
}
#endif // VOICE_VOX_ENABLE
	
#endif // SWDS

//-----------------------------------------------------------------------------
// Purpose: Wrapper for modelloader->Print() function call
//-----------------------------------------------------------------------------
CON_COMMAND( listmodels, "List loaded models." )
{
	modelloader->Print();
}

/*
==================
Host_IncrementCVar
==================
*/
CON_COMMAND_F( incrementvar, "Increment specified convar value.", FCVAR_DONTRECORD )
{
	if( args.ArgC() != 5 )
	{
		Warning( "Usage: incrementvar varName minValue maxValue delta\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "Host_IncrementCVar_f without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float delta = atof( args[ 4 ] );
	float newValue = currentValue + delta;
	if( newValue > endValue )
	{
		newValue = startValue;
	}
	else if ( newValue < startValue )
	{
		newValue = endValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Host_MultiplyCVar_f
//-----------------------------------------------------------------------------
CON_COMMAND_F( multvar, "Multiply specified convar value.", FCVAR_DONTRECORD )
{
	if (( args.ArgC() != 5 ))
	{
		Warning( "Usage: multvar varName minValue maxValue factor\n" );
		return;
	}

	const char *varName = args[ 1 ];
	if( !varName )
	{
		ConDMsg( "multvar without a varname\n" );
		return;
	}

	ConVar *var = ( ConVar * )g_pCVar->FindVar( varName );
	if( !var )
	{
		ConDMsg( "cvar \"%s\" not found\n", varName );
		return;
	}

	float currentValue = var->GetFloat();
	float startValue = atof( args[ 2 ] );
	float endValue = atof( args[ 3 ] );
	float factor = atof( args[ 4 ] );
	float newValue = currentValue * factor;
	if( newValue > endValue )
	{
		newValue = endValue;
	}
	else if ( newValue < startValue )
	{
		newValue = startValue;
	}

	// Conver incrementvar command to direct sets to avoid any problems with state in a demo loop.
	Cbuf_AddText( va("%s %f", varName, newValue) );

	ConDMsg( "%s = %f\n", var->GetName(), newValue );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( dumpstringtables, "Print string tables to console." )
{
	SV_PrintStringTables();
#ifndef SWDS
	CL_PrintStringTables();
#endif
}

// Register shared commands
ConCommand quit("quit", Host_Quit_f, "Exit the engine.");
static ConCommand cmd_exit("exit", Host_Quit_f, "Exit the engine.");

#ifndef SWDS
#ifdef VOICE_OVER_IP
static ConCommand startvoicerecord("+voicerecord", Host_VoiceRecordStart_f);
static ConCommand endvoicerecord("-voicerecord", Host_VoiceRecordStop_f);
#ifdef VOICE_VOX_ENABLE
static ConCommand togglevoicerecord("voicerecord_toggle", Host_VoiceToggle_f);
#endif // VOICE_VOX_ENABLE
#endif // VOICE_OVER_IP

#endif // SWDS


#if defined( STAGING_ONLY )

// From Kyle: For the GC we added this so we could call it over and
//  over until we got the crash reporter fixed.

// Visual studio optimizes this away unless we disable optimizations.
#pragma optimize( "", off )

class PureCallBase
{
public:
	virtual void PureFunction() = 0;

	PureCallBase()
	{
		NonPureFunction();
	}

	void NonPureFunction()
	{
		PureFunction();
	}
};
 
class PureCallDerived : public PureCallBase
{
public:
	void PureFunction() OVERRIDE
	{
	}
};

//-----------------------------------------------------------------------------
// Purpose: Force various crashes. useful for testing minidumps.
//  crash : Write 0 to address 0.
//  crash sys_error : Call Sys_Error().
//  crash hang : Hang.
//  crash purecall : Call virtual function in ctor.
//-----------------------------------------------------------------------------
CON_COMMAND( crash, "[ sys_error | hang | purecall | segfault | minidump ]: Cause the engine to crash." )
{ 
	if ( cmd_source != src_command )
		return;

	CUtlString cmd( ( args.ArgC() > 1 ) ? args[ 1 ] : "" );

	if ( cmd == "hang" )
	{
		// Hang. Useful to test watchdog code.
		Msg( "Hanging... Watchdog time: %d.\n ", Plat_GetWatchdogTime() );
		for ( ;; )
		{
			Msg( "%d ", Plat_MSTime() );
			ThreadSleep( 5000 );
		}
	}
	else if ( cmd == "purecall" )
	{
		Msg( "Instantiating PureCallDerived_derived...\n" );
		PureCallDerived derived;
	}
	else if ( cmd == "sys_error" )
	{
		Msg( "Calling Sys_Error...\n" );
		Sys_Error( "%s: Sys_Error()!!!", __FUNCTION__ );
	}
	else if ( cmd == "minidump" )
	{
		Msg( "Forcing minidump. build_number: %d.\n", build_number() );
		SteamAPI_WriteMiniDump( 0, NULL, build_number() );
	}
	else
	{
		Msg( "Segfault...\n" );
		char *p = 0;
		*p = 0;
	}
}

#pragma optimize( "", on )

#endif // STAGING_ONLY

CON_COMMAND_F( flush, "Flush unlocked cache memory.", FCVAR_CHEAT )
{
#if !defined( SWDS )
	g_ClientDLL->InvalidateMdlCache();
#endif // SWDS
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( true );
}

CON_COMMAND_F( flush_locked, "Flush unlocked and locked cache memory.", FCVAR_CHEAT )
{
#if !defined( SWDS )
	g_ClientDLL->InvalidateMdlCache();
#endif // SWDS
	serverGameDLL->InvalidateMdlCache();
	g_pDataCache->Flush( false );
}

CON_COMMAND( cache_print, "cache_print [section]\nPrint out contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT, pszSection );
}

CON_COMMAND( cache_print_lru, "cache_print_lru [section]\nPrint out contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_DETAIL_REPORT_LRU, pszSection );
}

CON_COMMAND( cache_print_summary, "cache_print_summary [section]\nPrint out a summary contents of cache memory." )
{
	const char *pszSection = NULL;
	if ( args.ArgC() == 2 )
	{
		pszSection = args[ 1 ];
	}
	g_pDataCache->OutputReport( DC_SUMMARY_REPORT, pszSection );
}

CON_COMMAND( sv_dump_edicts, "Display a list of edicts allocated on the server." )
{
	if ( !sv.IsActive() )
		return;

	CUtlMap<CUtlString, int> classNameCountMap;
	classNameCountMap.SetLessFunc( UtlStringLessFunc );

	Msg( "\nCurrent server edicts:\n");
	for ( int i = 0; i < sv.num_edicts; ++i )
	{
		CUtlMap<CUtlString, int>::IndexType_t index = classNameCountMap.Find( sv.edicts[ i ].GetClassName() );
		if ( index == classNameCountMap.InvalidIndex() )
		{
			index = classNameCountMap.Insert( sv.edicts[ i ].GetClassName(), 0 );
		}

		classNameCountMap[ index ]++;
	}

	Msg( "Count Classname\n");
	FOR_EACH_MAP( classNameCountMap, i )
	{
		Msg("%5d %s\n", classNameCountMap[ i ], classNameCountMap.Key(i).String() );
	}
	Msg( "NumEdicts: %d\n", sv.num_edicts );
	Msg( "FreeEdicts: %d\n\n", sv.free_edicts );
}

// make valve_ds only?
CON_COMMAND_F( memory_list, "dump memory list (linux only)", FCVAR_CHEAT )
{
	DumpMemoryLog( 128 * 1024 );
}

// make valve_ds only?
CON_COMMAND_F( memory_status, "show memory stats (linux only)", FCVAR_CHEAT )
{
	DumpMemorySummary();
}

// make valve_ds only?
CON_COMMAND_F( memory_mark, "snapshot current allocation status", FCVAR_CHEAT )
{
	SetMemoryMark();
}
// make valve_ds only?
CON_COMMAND_F( memory_diff, "show memory stats relative to snapshot", FCVAR_CHEAT )
{
	DumpChangedMemory( 64 * 1024 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CON_COMMAND( namelockid, "Prevent name changes for this userID." )
{
	if ( args.ArgC() <= 2 )
	{
		ConMsg( "Usage:  namelockid < userid > < 0 | 1 >\n" );
		return;
	}

	CBaseClient	*pClient = NULL;

	int iIndex = Q_atoi( args[1] );
	if ( iIndex > 0 )
	{
		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			pClient = static_cast< CBaseClient* >( sv.GetClient( i ) );

			if ( !pClient->IsConnected() )
				continue;

#if defined( REPLAY_ENABLED )
			if ( pClient->IsReplay() )
				continue;
#endif

			if ( pClient->IsHLTV() )
				continue;

			if ( pClient->GetUserID() == iIndex )
				break;

			pClient = NULL;
		}
	}

	if ( pClient )
	{
		pClient->SetPlayerNameLocked( ( Q_atoi( args[2] ) == 0 ) ? false : true );
	}
	else
	{
		ConMsg( "Player id \"%d\" not found.\n", iIndex );
	}
}

#if defined( STAGING_ONLY ) || defined( _DEBUG )
CON_COMMAND( fs_find, "Run virtual filesystem find" )
{
	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage:  fs_find wildcard pathid\n" );
		return;
	}

	const char *pWildcard = args.Arg(1);
	const char *pPathID = args.Arg(2);

	FileFindHandle_t findhandle;
	const char *pFile = NULL;
	size_t matches = 0;
	for ( pFile = g_pFullFileSystem->FindFirstEx( pWildcard, pPathID, &findhandle );
	      pFile;
	      pFile = g_pFullFileSystem->FindNext( findhandle ) )
	{
		ConMsg( "%s\n", pFile );
		matches++;
	}

	ConMsg( "  %u matching files/directories\n", matches );
}
#endif // defined( STAGING_ONLY ) || defined( _DEBUG )
