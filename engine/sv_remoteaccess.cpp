//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles all the functions for implementing remote access to the engine
//
//===========================================================================//

#include "server_pch.h"
#include "iclient.h"
#include "net.h"
#include "utlbuffer.h"
#include "utllinkedlist.h"
#include "igameserverdata.h"
#include "sv_remoteaccess.h"
#include "sv_rcon.h"
#include "sv_filter.h"
#include "sys.h"
#include "vprof_engine.h"
#include "PlayerState.h"
#include "sv_log.h"
#ifndef SWDS
#include "zip/XZip.h"
#endif
#include "cl_main.h"

extern IServerGameDLL	*serverGameDLL;

CServerRemoteAccess g_ServerRemoteAccess;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerRemoteAccess, IGameServerData, GAMESERVERDATA_INTERFACE_VERSION, g_ServerRemoteAccess);

ConVar sv_rcon_log( "sv_rcon_log", "1", 0, "Enable/disable rcon logging." );

//-----------------------------------------------------------------------------
// Host_Stats_f - prints out interesting stats about the server...
//-----------------------------------------------------------------------------
void Host_Stats_f (void)
{
	char stats[512];
	g_ServerRemoteAccess.GetStatsString(stats, sizeof(stats));
	ConMsg("CPU    In_(KB/s)  Out_(KB/s)  Uptime  Map_changes  FPS      Players  Connects\n%s\n", stats);
}
static ConCommand stats("stats", Host_Stats_f, "Prints server performance variables" );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerRemoteAccess::CServerRemoteAccess()
{
	m_iBytesSent = 0;
	m_iBytesReceived = 0;
	m_NextListenerID = 0;
	m_AdminUIID = INVALID_LISTENER_ID;
	m_nScreenshotListener = -1;
}

//-----------------------------------------------------------------------------
// Purpose: unique id to associate data transfers with sessions
//-----------------------------------------------------------------------------
ra_listener_id CServerRemoteAccess::GetNextListenerID( bool authConnection, const netadr_t *adr )
{
	int i = m_ListenerIDs.AddToTail();
	m_ListenerIDs[i].listenerID = i;
	m_ListenerIDs[i].authenticated = !authConnection;
	m_ListenerIDs[i].m_bHasAddress = ( adr != NULL );
	if ( adr )
	{
		m_ListenerIDs[i].adr = *adr;
	}
	return i;
}


bool GetStringHelper( CUtlBuffer & cmd, char *outBuf, int bufSize )
{
	outBuf[0] = 0;
	cmd.GetStringManualCharCount( outBuf, bufSize );
	if ( !cmd.IsValid() )
	{
		cmd.Purge();
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: handles a request
//-----------------------------------------------------------------------------
void CServerRemoteAccess::WriteDataRequest( CRConServer *pNetworkListener, ra_listener_id listener, const void *buffer, int bufferSize)
{
	m_iBytesReceived += bufferSize;
	// ConMsg("RemoteAccess: bytes received: %d\n", m_iBytesReceived);

	if ( bufferSize < 2*sizeof(int) ) // check that the buffer contains at least the id and type
	{
		return;
	}

	CUtlBuffer cmd(buffer, bufferSize, CUtlBuffer::READ_ONLY);
	bool invalidRequest = false;

	while ( invalidRequest == false && (int)cmd.TellGet() < (int)(cmd.Size() - 2 * sizeof(int) ) ) // while there is commands to read
	{
		// parse out the buffer
		int requestID = cmd.GetInt();
		pNetworkListener->SetRequestID( listener, requestID ); // tell the rcon server the ID so it can reflect it when the console redirect flushes
		int requestType = cmd.GetInt();

		switch (requestType)
		{
			case SERVERDATA_REQUESTVALUE:
				{
					if ( IsAuthenticated(listener) )
					{
						char variable[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						RequestValue( listener, requestID, variable);
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
					}
					else
					{
						char variable[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
					}
				}
				break;

			case SERVERDATA_SETVALUE:
				{
					if ( IsAuthenticated(listener) )
					{
						char variable[256];
						char value[256];
						if ( !GetStringHelper( cmd, variable, sizeof(variable) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, value, sizeof(value) ) )
						{
							invalidRequest = true;
							break;
						}
						SetValue(variable, value);
					}
					else
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
					}
				}
				break;

			case SERVERDATA_EXECCOMMAND:
				{
					if ( IsAuthenticated(listener) )
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						
						ExecCommand(command);
						
						if ( listener != m_AdminUIID )
						{
							LogCommand( listener, va( "command \"%s\"", command) );
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
					}
					else
					{
						char command[512];
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						if ( !GetStringHelper( cmd, command, sizeof(command) ) )
						{
							invalidRequest = true;
							break;
						}
						LogCommand( listener, "Bad Password" );
					}
				}
				break;

			case SERVERDATA_AUTH:
				{
					char password[512];
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					CheckPassword( pNetworkListener, listener, requestID, password );
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}

					if ( m_ListenerIDs[ listener ].authenticated )
					{
						// if the second string has a non-zero value, it is a userid.
						int userID = atoi( password );
						const ConCommandBase *var = g_pCVar->GetCommands();
						while ( var )
						{
							if ( var->IsCommand() )
							{
								if ( Q_stricmp( var->GetName(), "mp_disable_autokick" ) == 0 )
								{
									Cbuf_AddText( va( "mp_disable_autokick %d\n", userID ) );
									Cbuf_Execute();
									break;
								}
							}
							var = var->GetNext();
						}
					}
				}
				break;

			case SERVERDATA_TAKE_SCREENSHOT:
#ifndef SWDS
				m_nScreenshotListener = listener;
				CL_TakeJpeg( );
#endif
				break;

			case SERVERDATA_SEND_CONSOLE_LOG:
				{
#ifndef SWDS
					CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
					if ( GetConsoleLogFileData( buf ) )
					{
						HZIP hZip = CreateZipZ( 0, 1024 * 1024, ZIP_MEMORY );
						void *pMem;
						unsigned long nLen;
						ZipAdd( hZip, "console.log", buf.Base(), buf.TellMaxPut(), ZIP_MEMORY );
						ZipGetMemory( hZip, &pMem, &nLen );
						SendResponseToClient( listener, SERVERDATA_CONSOLE_LOG_RESPONSE, pMem, nLen );
						CloseZip( hZip );
					}
					else
					{
						LogCommand( listener, "Failed to read console log!\n" );
						RespondString( listener, requestID, "Failed to read console log!\n" );
					}
#endif
				}
				break;

#ifdef VPROF_ENABLED
			case SERVERDATA_VPROF:
				{
					char password[25];
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( IsAuthenticated(listener) )
					{
						RegisterVProfDataListener( listener );
						LogCommand( listener, "Remote VProf started!\n" );
						RespondString( listener, requestID, "Remote VProf started!\n" );
					}
				}
				break;
		
			case SERVERDATA_REMOVE_VPROF:
				{
					char password[25];
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( !GetStringHelper( cmd, password, sizeof(password) ) )
					{
						invalidRequest = true;
						break;
					}
					if ( IsAuthenticated(listener) )
					{
						RemoveVProfDataListener( listener );
						LogCommand( listener, "Remote VProf finished!\n" );
						RespondString( listener, requestID, "Remote VProf finished!\n" );
					}
				}
				break;
#endif

			default:
				Assert(!("Unknown requestType in CServerRemoteAccess::WriteDataRequest()"));
				cmd.Purge();
				invalidRequest = true;
				break;
		};
	}
}

// NOTE: This version is used by the server DLL or server plugins
void CServerRemoteAccess::WriteDataRequest( ra_listener_id listener, const void *buffer, int bufferSize )
{
	WriteDataRequest( &RCONServer(), listener, buffer, bufferSize );
}


//-----------------------------------------------------------------------------
// Uploads a screenshot to a particular listener
//-----------------------------------------------------------------------------
void CServerRemoteAccess::UploadScreenshot( const char *pFileName )
{
#ifndef SWDS
	if ( m_nScreenshotListener < 0 )
		return;

	CUtlBuffer buf( 128 * 1024, 0 );
	if ( g_pFullFileSystem->ReadFile( pFileName, "MOD", buf ) )
	{
		HZIP hZip = CreateZipZ( 0, 1024 * 1024, ZIP_MEMORY );
		void *pMem;
		unsigned long nLen;
		ZipAdd( hZip, "screenshot.jpg", buf.Base(), buf.TellMaxPut(), ZIP_MEMORY );
		ZipGetMemory( hZip, &pMem, &nLen );
		SendResponseToClient( m_nScreenshotListener, SERVERDATA_SCREENSHOT_RESPONSE, pMem, nLen );
		CloseZip( hZip );
	}
	else
	{
		LogCommand( m_nScreenshotListener, "Failed to read screenshot!\n" );
		RespondString( m_nScreenshotListener, 0, "Failed to read screenshot!\n" );
	}

	m_nScreenshotListener = -1;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: log information about a command that ran
//-----------------------------------------------------------------------------
void CServerRemoteAccess::LogCommand( ra_listener_id listener, const char *msg )
{
	if ( !sv_rcon_log.GetBool() )
		return;
		
	if ( listener < (ra_listener_id)m_ListenerIDs.Count() && m_ListenerIDs[listener].m_bHasAddress )
	{
		Log( "rcon from \"%s\": %s\n", m_ListenerIDs[listener].adr.ToString(), msg );
	}
	else
	{
		Log( "rcon from \"unknown\": %s\n", msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: checks if this user has provided the correct password
//-----------------------------------------------------------------------------
void CServerRemoteAccess::CheckPassword( CRConServer *pNetworkListener, ra_listener_id listener, int requestID, const char *password )
{
	// If the pw does not match, then not authed
	if ( !pNetworkListener->IsPassword( password ) )
	{
		BadPassword( pNetworkListener, listener );
		return;
	}

		// allocate a spot in the list for the response
	int i = m_ResponsePackets.AddToTail();
	m_ResponsePackets[i].responderID = listener; // record who we need to respond to

	CUtlBuffer &response = m_ResponsePackets[i].packet;

	// build the response
	response.PutInt(requestID);
	response.PutInt(SERVERDATA_AUTH_RESPONSE);
	response.PutString("");
	response.PutString("");

	m_ListenerIDs[ listener ].authenticated = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if this connection has provided the correct password
//-----------------------------------------------------------------------------
bool CServerRemoteAccess::IsAuthenticated( ra_listener_id listener )
{
	// Checking for >= 0 is tautological because ra_listener_id is unsigned
	Assert( /*listener >= 0 &&*/ listener < (ra_listener_id)m_ListenerIDs.Count() );
	return m_ListenerIDs[listener].authenticated;
}

//-----------------------------------------------------------------------------
// Purpose: send a bad password packet
// Returns TRUE if socket was closed
//-----------------------------------------------------------------------------
void CServerRemoteAccess::BadPassword( CRConServer *pNetworkListener, ra_listener_id listener )
{
	ListenerStore_t& listenerStore = m_ListenerIDs[listener];

	listenerStore.authenticated = false;

	if ( pNetworkListener->HandleFailedRconAuth( listenerStore.adr ) )
	{
		// Close the socket if too many failed attempts
		pNetworkListener->BCloseAcceptedSocket( listener );
	}
	else
	{
		//
		// Respond to the rcon user
		//

		// allocate a spot in the list for the response
		int i = m_ResponsePackets.AddToTail();
		m_ResponsePackets[i].responderID = listener; // record who we need to respond to
		CUtlBuffer &response = m_ResponsePackets[i].packet;

		// build the response
		response.PutInt(-1); // special flag for bad password
		response.PutInt(SERVERDATA_AUTH_RESPONSE);
		response.PutString("");
		response.PutString("");
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns the number of bytes read
//-----------------------------------------------------------------------------
int CServerRemoteAccess::GetDataResponseSize( ra_listener_id listener )
{
	for( int i = m_ResponsePackets.Head(); m_ResponsePackets.IsValidIndex(i); i = m_ResponsePackets.Next(i) )
	{
		// copy response into buffer
		if ( m_ResponsePackets[i].responderID != listener ) // not for us, skip to the next entry
			continue;

		CUtlBuffer &response = m_ResponsePackets[i].packet;
		return response.TellPut();
	}
	return 0;
}

int CServerRemoteAccess::ReadDataResponse( ra_listener_id listener, void *buffer, int bufferSize )
{
	for( int i = m_ResponsePackets.Head(); m_ResponsePackets.IsValidIndex(i); i = m_ResponsePackets.Next(i) )
	{
		// copy response into buffer
		if ( m_ResponsePackets[i].responderID != listener ) // not for us, skip to the next entry
			continue;

		CUtlBuffer &response = m_ResponsePackets[i].packet;
		int bytesToCopy = response.TellPut();
		Assert(bufferSize >= bytesToCopy);
		if (bytesToCopy <= bufferSize)
		{
			memcpy(buffer, response.Base(), bytesToCopy);
		}
		else
		{
			// not enough room in buffer, don't return message
			bytesToCopy = 0;
		}

		m_iBytesSent += bytesToCopy;
		// ConMsg("RemoteAccess: bytes sent: %d\n", m_iBytesSent);

		// remove from list
		m_ResponsePackets.Remove(i);
		// return bytes copied
		return bytesToCopy;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: looks up a cvar and posts a return value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::RequestValue( ra_listener_id listener, int requestID, const char *variable)
{
	// look up the cvar
	CUtlBuffer value(0, 256, CUtlBuffer::TEXT_BUFFER);		// text-mode buffer
	LookupValue(variable, value);

	// allocate a spot in the list for the response
	int i = m_ResponsePackets.AddToTail();
	m_ResponsePackets[i].responderID = listener; // record who we need to respond to

	CUtlBuffer &response = m_ResponsePackets[i].packet;

	// build the response
	response.PutInt(requestID);
	response.PutInt(SERVERDATA_RESPONSE_VALUE);
	response.PutString(variable);

	//Assert(value.TellPut() > 0);
	response.PutInt(value.TellPut());
	if (value.TellPut())
	{
		response.Put(value.Base(), value.TellPut());
	}
}


//-----------------------------------------------------------------------------
// Purpose: looks up a cvar and posts a return value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::RespondString( ra_listener_id listener, int requestID, const char *pString )
{
	// allocate a spot in the list for the response
	int i = m_ResponsePackets.AddToTail();
	m_ResponsePackets[i].responderID = listener; // record who we need to respond to

	CUtlBuffer &response = m_ResponsePackets[i].packet;

	// build the response
	response.PutInt(requestID);
	response.PutInt(SERVERDATA_RESPONSE_STRING);
	response.PutString(pString);
}


//-----------------------------------------------------------------------------
// Purpose: Sets a cvar or value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SetValue(const char *variable, const char *value)
{
	// check for special types
	if (!stricmp(variable, "map"))
	{
		// push a map change command
		Cbuf_AddText( va( "changelevel %s\n", value ) );
		Cbuf_Execute();
	}
	else if (!stricmp(variable, "mapcycle"))
	{
		// write out a new mapcycle file
		ConVarRef mapcycle( "mapcyclefile" );
		if ( mapcycle.IsValid() )
		{
			FileHandle_t f = g_pFileSystem->Open(mapcycle.GetString(), "wt");
			if (!f)
			{
				// mapcycle file probably read only, fall pack to temporary file
				Msg("Couldn't write to read-only file %s, using file _temp_mapcycle.txt instead.\n", mapcycle.GetString());
				mapcycle.SetValue("_temp_mapcycle.txt" );
				f = g_pFileSystem->Open(mapcycle.GetString(), "wt");
				if (!f)
				{
					return;
				}
			}
			g_pFileSystem->Write(value, Q_strlen(value) + 1, f);
			g_pFileSystem->Close(f);
		}
	}
	else
	{
		// Stick the cvar set in the command string, so client notification, replication, etc happens
		Cbuf_AddText( va("%s %s", variable, value) );
		Cbuf_AddText("\n");
		Cbuf_Execute();
	}
}

//-----------------------------------------------------------------------------
// Purpose: execs a command
//-----------------------------------------------------------------------------
void CServerRemoteAccess::ExecCommand(const char *cmdString)
{
	Cbuf_AddText((char *)cmdString);
	Cbuf_AddText("\n");
	Cbuf_Execute();
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable
//-----------------------------------------------------------------------------
bool CServerRemoteAccess::LookupValue(const char *variable, CUtlBuffer &value)
{
	Assert(value.IsText());

	// first see if it's a cvar
	const char *strval = LookupStringValue(variable);
	if (strval)
	{
		value.PutString(strval);
		value.PutChar(0);
	}
	else if (!stricmp(variable, "stats"))
	{
		char szStats[512];
		GetStatsString( szStats, sizeof( szStats ) );
		value.PutString( szStats );
		value.PutChar(0);
	}
	else if (!stricmp(variable, "banlist"))
	{
		// returns a list of banned users and ip's
		GetUserBanList(value);
	}
	else if (!stricmp(variable, "playerlist"))
	{
		GetPlayerList(value);		
	}
	else if (!stricmp(variable, "maplist"))
	{
		GetMapList(value);
	}
	else if (!stricmp(variable, "uptime"))
	{
		int timeSeconds = (int)(Plat_FloatTime());
		value.PutInt(timeSeconds);
		value.PutChar(0);
	}
	else if (!stricmp(variable, "ipaddress"))
	{
		char addr[25];
		Q_snprintf( addr, sizeof(addr), "%s:%i", net_local_adr.ToString(true), sv.GetUDPPort());
		value.PutString( addr );
		value.PutChar(0);
	}
	else if (!stricmp(variable, "mapcycle"))
	{
		ConVarRef mapcycle( "mapcyclefile" );
		if ( mapcycle.IsValid() )
		{
			// send the mapcycle list file
			FileHandle_t f = g_pFileSystem->Open(mapcycle.GetString(), "rb" );

			if ( f == FILESYSTEM_INVALID_HANDLE )
				return true;
			
			int len = g_pFileSystem->Size(f);
			char *mapcycleData = (char *)_alloca( len+1 );
			if ( len && g_pFileSystem->Read( mapcycleData, len, f ) )
			{
				mapcycleData[len] = 0; // Make sure it's null terminated.
				value.PutString((const char *)mapcycleData);
				value.PutChar(0);
			}
			else
			{
				value.PutString( "" );
				value.PutChar(0);
			}

			g_pFileSystem->Close( f );


		}
	}
	else
	{
		// value not found, null terminate
		value.PutChar(0);
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable for simple string values
//-----------------------------------------------------------------------------
const char *CServerRemoteAccess::LookupStringValue(const char *variable)
{
	static char s_ReturnBuf[32];
	IConVar *pVar = g_pCVar->FindVar( variable );
	if ( pVar )
	{
		ConVarRef var( pVar );
		if ( var.IsValid() )
			return var.GetString();
	}

	// special types
	if ( !Q_stricmp( variable, "map" ) )
		return sv.GetMapName();

	if ( !Q_stricmp( variable, "playercount" ) )
	{
		Q_snprintf( s_ReturnBuf, sizeof(s_ReturnBuf) - 1, "%d", sv.GetNumClients() - sv.GetNumProxies());
		return s_ReturnBuf;
	}
	
	if ( !Q_stricmp( variable, "maxplayers" ) )
	{
		Q_snprintf( s_ReturnBuf, sizeof(s_ReturnBuf) - 1, "%d", sv.GetMaxClients() );
		return s_ReturnBuf;
	}
	
	if ( !Q_stricmp( variable, "gamedescription" ) && serverGameDLL )
		return serverGameDLL->GetGameDescription();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: fills a buffer with a list of all banned IP addresses
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetUserBanList(CUtlBuffer &value)
{
	// add user bans
	int i;
	for (i = 0; i < g_UserFilters.Count(); i++)
	{
		value.Printf("%i %s : %.3f min\n", i + 1, GetUserIDString(g_UserFilters[i].userid), g_UserFilters[i].banTime);
	}

	// add ip filters
	for (i = 0; i < g_IPFilters.Count() ; i++)
	{
		unsigned char b[4];
		*(unsigned *)b = g_IPFilters[i].compare;
		value.Printf("%i %i.%i.%i.%i : %.3f min\n", i + 1 + g_UserFilters.Count(), b[0], b[1], b[2], b[3], g_IPFilters[i].banTime);
	}

	value.PutChar(0);
}

void CServerRemoteAccess::GetStatsString(char *buf, int bufSize)
{
	float avgIn=0,avgOut=0;

	sv.GetNetStats( avgIn, avgOut );

	// format: CPU percent, Bandwidth in, Bandwidth out, uptime, changelevels, framerate, total players
	_snprintf(buf, bufSize - 1, "%-6.2f %-10.2f %-11.2f %-7i %-12i %-8.2f %-8i %-8i",
				sv.GetCPUUsage() * 100, 
				avgIn / 1024.0f,
				avgOut / 1024.0f,
				(int)(Sys_FloatTime()) / 60,
				sv.GetSpawnCount() - 1,
				1.0/host_frametime, // frame rate
				sv.GetNumClients() - sv.GetNumProxies(),
				sv.GetNumConnections());
	buf[bufSize - 1] = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Fills buffer with details on everyone in the server
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetPlayerList(CUtlBuffer &value)
{
	if ( !serverGameClients )
	{
		return;
	}

	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		CGameClient *client = sv.Client(i);
		if ( !client || !client->IsActive() )
			continue;

		CPlayerState *pl = serverGameClients->GetPlayerState( client->edict );
		if ( !pl )
			continue;

		// valid user, add to buffer
		// format per user, each user seperated by a newline '\n'
		//      "name authID ipAddress ping loss frags time"
		if ( client->IsFakeClient() )
		{
			value.Printf("\"%s\" %s 0 0 0 %d 0\n",
				client->GetClientName(),
				client->GetNetworkIDString(),
				pl->frags);
		}
		else
		{
			value.Printf("\"%s\" %s %s %d %d %d %d\n", 
				client->GetClientName(),
				client->GetNetworkIDString(),
				client->GetNetChannel()->GetAddress(),
				(int)(client->GetNetChannel()->GetAvgLatency(FLOW_OUTGOING) * 1000.0f),
				(int)(client->GetNetChannel()->GetAvgLoss(FLOW_INCOMING)),
				pl->frags,
				(int)(client->GetNetChannel()->GetTimeConnected()));
		}
	}

	value.PutChar(0);
}

//-----------------------------------------------------------------------------
// Purpose: Fills buffer with list of maps from this mod
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetMapList(CUtlBuffer &value)
{
	// search the directory structure.
	char mapwild[MAX_QPATH];
	char friendly_com_gamedir[ MAX_OSPATH ];
	strcpy(mapwild, "maps/*.bsp");
	Q_strncpy( friendly_com_gamedir, com_gamedir, sizeof(friendly_com_gamedir) );
	Q_strlower( friendly_com_gamedir );
	
	char const *findfn = Sys_FindFirst( mapwild, NULL, 0 );
	while ( findfn )
	{
		char curDir[MAX_PATH];
		_snprintf(curDir, MAX_PATH, "maps/%s", findfn);
		g_pFileSystem->GetLocalPath(curDir, curDir, MAX_PATH);
		
		// limit maps displayed to ones for the mod only
		if (strstr(curDir, friendly_com_gamedir))
		{
			// clean up the map name
			char mapName[MAX_PATH];
			strcpy(mapName, findfn);
			char *extension = strstr(mapName, ".bsp");
			if (extension)
			{
				*extension = 0;
			}

			// write into buffer
			value.PutString(mapName);
			value.PutString("\n");
		}
		findfn = Sys_FindNext( NULL, 0 );
	}

	Sys_FindClose();
	value.PutChar(0);
}

//-----------------------------------------------------------------------------
// Purpose: sends a message to all the watching admin UI's
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendMessageToAdminUI( ra_listener_id listenerID, const char *message)
{
	if ( listenerID != m_AdminUIID )
	{
		Warning( "ServerRemoteAccess: Sending AdminUI message to non-AdminUI listener\n" );
	}

	// allocate a spot in the list for the response
	int i = m_ResponsePackets.AddToTail();
	m_ResponsePackets[i].responderID = listenerID; // record who we need to respond to
	CUtlBuffer &response = m_ResponsePackets[i].packet;

	// post the message
	response.PutInt(0);
	response.PutInt(SERVERDATA_UPDATE);
	response.PutString(message);
}


//-----------------------------------------------------------------------------
// Purpose: Sends a response to the client
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendResponseToClient( ra_listener_id listenerID, ServerDataResponseType_t type, void *pData, int nDataLen )
{
	// allocate a spot in the list for the response
	int i = m_ResponsePackets.AddToTail();
	m_ResponsePackets[i].responderID = listenerID; // record who we need to respond to
	CUtlBuffer &response = m_ResponsePackets[i].packet;

	// post the message
	response.PutInt( 0 );
	response.PutInt( type );
	response.PutInt( nDataLen );
	response.Put( pData, nDataLen );
}


//-----------------------------------------------------------------------------
// Purpose: sends an opaque blob of data from VProf to a remote rcon listener
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendVProfData( ra_listener_id listenerID, bool bGroupData, void *data, int len )
{
	Assert( listenerID != m_AdminUIID ); // only RCON clients support this right now
	SendResponseToClient( listenerID, bGroupData ? SERVERDATA_VPROF_GROUPS : SERVERDATA_VPROF_DATA, data, len );
}

//-----------------------------------------------------------------------------
// Purpose: C function for rest of engine to access CServerRemoteAccess class
//-----------------------------------------------------------------------------
extern "C" void NotifyDedicatedServerUI(const char *message)
{
	if ( g_ServerRemoteAccess.GetAdminUIID() != INVALID_LISTENER_ID ) // if we have an admin UI actually registered
	{
		g_ServerRemoteAccess.SendMessageToAdminUI( g_ServerRemoteAccess.GetAdminUIID(), message);
	}
}
