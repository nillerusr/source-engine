//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclient.cpp: implementation of the CBaseClient class.
//
//===========================================================================//


#include "client_pch.h"
#include "tier0/etwprof.h"
#include "eiface.h"
#include "baseclient.h"
#include "server.h"
#include "host.h"
#include "networkstringtable.h"
#include "framesnapshot.h"
#include "GameEventManager.h"
#include "LocalNetworkBackdoor.h"
#include "dt_send_eng.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "sv_remoteaccess.h" // NotifyDedicatedServerUI()
#include "MapReslistGenerator.h"
#include "sv_steamauth.h"
#include "matchmaking.h"
#include "iregistry.h"
#include "sv_main.h"
#include "hltvserver.h"
#include <ctype.h>
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IServerGameDLL	*serverGameDLL;
extern ConVar tv_enable;

ConVar sv_namechange_cooldown_seconds( "sv_namechange_cooldown_seconds", "30.0", FCVAR_NONE, "When a client name change is received, wait N seconds before allowing another name change" );
ConVar sv_netspike_on_reliable_snapshot_overflow( "sv_netspike_on_reliable_snapshot_overflow", "0", FCVAR_NONE, "If nonzero, the server will dump a netspike trace if a client is dropped due to reliable snapshot overflow" );
ConVar sv_netspike_sendtime_ms( "sv_netspike_sendtime_ms", "0", FCVAR_NONE, "If nonzero, the server will dump a netspike trace if it takes more than N ms to prepare a snapshot to a single client.  This feature does take some CPU cycles, so it should be left off when not in use." );
ConVar sv_netspike_output( "sv_netspike_output", "1", FCVAR_NONE, "Where the netspike data be written?  Sum of the following values: 1=netspike.txt, 2=ordinary server log" );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBaseClient::CBaseClient()
{
	// init all pointers
	m_NetChannel = NULL;
	m_ConVars = NULL;
	m_Server = NULL;
	m_pBaseline = NULL;
	m_bIsHLTV = false;
#if defined( REPLAY_ENABLED )
	m_bIsReplay = false;
#endif
	m_bConVarsChanged = false;
	m_bInitialConVarsSet = false;
	m_bSendServerInfo = false;
	m_bFullyAuthenticated = false;
	m_fTimeLastNameChange = 0.0;
	m_szPendingNameChange[0] = '\0';
	m_bReportFakeClient = true;
	m_iTracing = 0;
	m_bPlayerNameLocked = false;
}

CBaseClient::~CBaseClient()
{
}


void CBaseClient::SetRate(int nRate, bool bForce )
{
	if ( m_NetChannel )
		m_NetChannel->SetDataRate( nRate );
}

int	CBaseClient::GetRate( void ) const
{
	if ( m_NetChannel )
	{
		return m_NetChannel->GetDataRate(); 
	}
	else
	{
		return 0;
	}
}

bool CBaseClient::FillUserInfo( player_info_s &userInfo )
{
	Q_memset( &userInfo, 0, sizeof(userInfo) );

	if ( !m_Name[0] || !IsConnected() )
		return false; // inactive user, no more data available

	Q_strncpy( userInfo.name, GetClientName(), MAX_PLAYER_NAME_LENGTH );
	V_strcpy_safe( userInfo.guid, GetNetworkIDString() );
	userInfo.friendsID = m_nFriendsID;
	Q_strncpy( userInfo.friendsName, m_FriendsName, sizeof(m_FriendsName) );
	userInfo.userID = GetUserID();
	userInfo.fakeplayer = IsFakeClient();
	userInfo.ishltv = IsHLTV();
#if defined( REPLAY_ENABLED )
	userInfo.isreplay = IsReplay();
#endif

	for( int i=0; i< MAX_CUSTOM_FILES; i++ )
		userInfo.customFiles[i] = m_nCustomFiles[i].crc;

	userInfo.filesDownloaded = m_nFilesDownloaded;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Send text to client
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseClient::ClientPrintf (const char *fmt, ...)
{
	if ( !m_NetChannel )
	{
		return;
	}

	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	SVC_Print print(string);
	m_NetChannel->SendNetMsg( print );
}

//-----------------------------------------------------------------------------
// Purpose: Send text to client
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
bool CBaseClient::SendNetMsg(INetMessage &msg, bool bForceReliable)
{
	if ( !m_NetChannel )
	{
		return true;
	}

	int nStartBit = m_NetChannel->GetNumBitsWritten( msg.IsReliable() || bForceReliable );
	bool bret = m_NetChannel->SendNetMsg( msg, bForceReliable );
	if ( IsTracing() )
	{
		int nBits = m_NetChannel->GetNumBitsWritten( msg.IsReliable() || bForceReliable ) - nStartBit;
		TraceNetworkMsg( nBits, "NetMessage %s", msg.GetName() );
	}
	return bret;
}

char const *CBaseClient::GetUserSetting(char const *pchCvar) const 
{
	if ( !m_ConVars || !pchCvar || !pchCvar[0] )
	{
		return "";
	}

	const char * value = m_ConVars->GetString( pchCvar, "" );

	if ( value[0]==0 )
	{
		// check if this var even existed
		if ( m_ConVars->GetDataType( pchCvar ) ==	KeyValues::TYPE_NONE )
		{
			DevMsg( "GetUserSetting: cvar '%s' unknown.\n", pchCvar );
		}
	}

	return value;
}

void CBaseClient::SetUserCVar( const char *pchCvar, const char *value)
{
	if ( !pchCvar || !value )
		return;

	// Name is handled differently
	if ( !Q_stricmp( pchCvar, "name") )
	{
		//Msg("CBaseClient::SetUserCVar[index=%d]('name', '%s')\n", m_nClientSlot, value );
		ClientRequestNameChange( value );
		return;
	}

	m_ConVars->SetString( pchCvar, value );
}

void CBaseClient::SetUpdateRate(int udpaterate, bool bForce)
{
	udpaterate = clamp( udpaterate, 1, 100 );

	m_fSnapshotInterval = 1.0f / udpaterate;
}

int CBaseClient::GetUpdateRate(void) const
{
	if ( m_fSnapshotInterval > 0 )
		return (int)(1.0f/m_fSnapshotInterval);
	else
		return 0;
}

void CBaseClient::FreeBaselines()
{
	if ( m_pBaseline )
	{
		m_pBaseline->ReleaseReference();
		m_pBaseline = NULL;
	}

	m_nBaselineUpdateTick = -1;
	m_nBaselineUsed = 0;
	m_BaselinesSent.ClearAll();
}

void CBaseClient::Clear()
{
	// Throw away any residual garbage in the channel.
	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown("Disconnect by server.\n");
		m_NetChannel = NULL;
	}

	if ( m_ConVars )
	{
		m_ConVars->deleteThis();
		m_ConVars = NULL;
	}

	FreeBaselines();

	// This used to be a memset, but memset will screw up any embedded classes
	// and we want to preserve some things like index.
	m_nSignonState = SIGNONSTATE_NONE;
	m_nDeltaTick = -1;
	m_nSignonTick = 0;
	m_nStringTableAckTick = 0;
	m_pLastSnapshot = NULL;
	m_nForceWaitForTick = -1;
	m_bFakePlayer = false;
	m_bIsHLTV = false;
#if defined( REPLAY_ENABLED )
	m_bIsReplay = false;
#endif
	m_fNextMessageTime = 0;
	m_fSnapshotInterval = 0;
	m_bReceivedPacket = false;
	m_UserID = 0;
	m_Name[0] = 0;
	m_nFriendsID = 0;
	m_FriendsName[0] = 0;
	m_nSendtableCRC = 0;
	m_nBaselineUpdateTick = -1;
	m_nBaselineUsed = 0;
	m_nFilesDownloaded = 0;
	m_bConVarsChanged = false;
	m_bSendServerInfo = false;
	m_bFullyAuthenticated = false;
	m_fTimeLastNameChange = 0.0;
	m_szPendingNameChange[0] = '\0';

	Q_memset( m_nCustomFiles, 0, sizeof(m_nCustomFiles) );
}

bool CBaseClient::SetSignonState(int state, int spawncount)
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	switch( m_nSignonState )
	{
		case SIGNONSTATE_CONNECTED :	// client is connected, leave client in this state and let SendPendingSignonData do the rest
										m_bSendServerInfo = true; 
										break;

		case SIGNONSTATE_NEW		:	// client got server info, send prespawn datam_Client->SendServerInfo()
										if ( !SendSignonData() )
											return false;
										
										break;

		case SIGNONSTATE_PRESPAWN	:	SpawnPlayer();
										break;

		case SIGNONSTATE_SPAWN		:	ActivatePlayer();
										break;

		case SIGNONSTATE_FULL		:	OnSignonStateFull();
										break;

		case SIGNONSTATE_CHANGELEVEL:	break;	

	}

	return true;
}

void CBaseClient::Reconnect( void )
{
	ConMsg("Forcing client reconnect (%i)\n", m_nSignonState );
	
	m_NetChannel->Clear();

	m_nSignonState = SIGNONSTATE_CONNECTED;
	
	NET_SignonState signon( m_nSignonState, -1 );
	m_NetChannel->SendNetMsg( signon );
}

void CBaseClient::Inactivate( void )
{
	FreeBaselines();

	m_nDeltaTick = -1;
	m_nSignonTick = 0;
	m_nStringTableAckTick = 0;
	m_pLastSnapshot = NULL;
	m_nForceWaitForTick = -1;

	m_nSignonState = SIGNONSTATE_CHANGELEVEL;

	if ( m_NetChannel )
	{
		// don't do that for fakeclients
		m_NetChannel->Clear();
		
		if ( NET_IsMultiplayer() )
		{
			NET_SignonState signon( m_nSignonState, m_Server->GetSpawnCount() );
			SendNetMsg( signon );

			// force sending message now
			m_NetChannel->Transmit();	
		}
	}

	// don't receive event messages anymore
	g_GameEventManager.RemoveListener( this );
}

//---------------------------------------------------------------------------
// Purpose: Determine whether or not a character should be ignored in a player's name.
//---------------------------------------------------------------------------
inline bool BIgnoreCharInName ( unsigned char cChar, bool bIsFirstCharacter )
{
	// Don't copy '%' or '~' chars across
	// Don't copy '#' chars across if they would go into the first position in the name
	// Don't allow color codes ( less than COLOR_MAX )
	return cChar == '%' || cChar == '~' || cChar < 0x09 || ( bIsFirstCharacter && cChar == '#' );
}

void ValidateName( char *pszName, int nBuffSize )
{
	if ( !pszName )
		return;

	// did we get an empty string for the name?
	if ( Q_strlen( pszName ) <= 0 )
	{
		Q_snprintf( pszName, nBuffSize, "unnamed" );
	}
	else
	{
		Q_RemoveAllEvilCharacters( pszName );

		const unsigned char *pChar = (unsigned char *)pszName;

		// also skip characters we're going to ignore
		while ( *pChar && ( isspace(*pChar) || BIgnoreCharInName( *pChar, true ) ) )
		{
			++pChar;
		}

		// did we get all the way to the end of the name without a non-whitespace character?
		if ( *pChar == '\0' )
		{
			Q_snprintf( pszName, nBuffSize, "unnamed" );
		}
	}
}

void CBaseClient::SetName(const char * playerName)
{
	char name[MAX_PLAYER_NAME_LENGTH];
	Q_strncpy( name, playerName, sizeof(name) );

	// Clear any pending name change
	m_szPendingNameChange[0] = '\0';

	// quick check to make sure the name isn't empty or full of whitespace
	ValidateName( name, sizeof(name) );

	if ( Q_strncmp( name, m_Name, sizeof(m_Name) ) == 0 )
		return; // didn't change

	int			i;
	int			dupc = 1;
	char		*p, *val;

	char	newname[MAX_PLAYER_NAME_LENGTH];

	// remove evil char '%'
	char *pFrom = (char *)name;
	char *pTo = m_Name;
	char *pLimit = &m_Name[sizeof(m_Name)-1];

	while ( *pFrom && pTo < pLimit )
	{
		// Don't copy '%' or '~' chars across
		// Don't copy '#' chars across if they would go into the first position in the name
		// Don't allow color codes ( less than COLOR_MAX )
		if ( !BIgnoreCharInName( *pFrom, pTo == &m_Name[0] ) )
		{
			*pTo++ = *pFrom;
		}

		pFrom++;
	}
	*pTo = 0;

	Assert( m_Name[ 0 ] != '\0' ); // this should've been caught by ValidateName
	if ( m_Name[ 0 ] == '\0' )
	{
		V_strncpy( m_Name, "unnamed", sizeof(m_Name) );
	}

	val = m_Name;

	// Don't care about duplicate names on the xbox. It can only occur when a player
	// is reconnecting after crashing, and we don't want to ever show the (X) then.
	if ( !IsX360() )
	{
		// Check to see if another user by the same name exists
		while ( true )
		{
			for ( i = 0; i < m_Server->GetClientCount(); i++ )
			{
				IClient *client = m_Server->GetClient( i );

				if( !client->IsConnected() || client == this )
					continue;
				
				// If it's 2 bots they're allowed to have matching names, otherwise there's a conflict
				if( !Q_stricmp( client->GetClientName(), val ) && !( IsFakeClient() && client->IsFakeClient() ) )
				{
					CBaseClient *pClient = dynamic_cast< CBaseClient* >( client );
					if ( IsFakeClient() && pClient )
					{
						// We're a bot so we get to keep the name... change the other guy
						pClient->m_Name[ 0 ] = '\0';
						pClient->SetName( val );
					}
					else
					{
						break;
					}
				}
			}

			if (i >= m_Server->GetClientCount())
				break;

			p = val;

			if (val[0] == '(')
			{
				if (val[2] == ')')
				{
					p = val + 3;
				}
				else if (val[3] == ')')	//assumes max players is < 100
				{
					p = val + 4;
				}
			}

			Q_snprintf(newname, sizeof(newname), "(%d)%-.*s", dupc++, MAX_PLAYER_NAME_LENGTH - 4, p );
			Q_strncpy(m_Name, newname, sizeof(m_Name));
			
			val = m_Name;		
		}	
	}

	m_ConVars->SetString( "name", m_Name );
	m_bConVarsChanged = true;

	m_Server->UserInfoChanged( m_nClientSlot );
}

void CBaseClient::ActivatePlayer()
{
	COM_TimestampedLog( "CBaseClient::ActivatePlayer" );

	// tell server to update the user info table (if not already done)
	m_Server->UserInfoChanged( m_nClientSlot );

	m_nSignonState = SIGNONSTATE_FULL;
	MapReslistGenerator().OnPlayerSpawn();
#ifndef _XBOX
	// update the UI
	NotifyDedicatedServerUI("UpdatePlayers");
#endif
}

void CBaseClient::SpawnPlayer( void )
{
	COM_TimestampedLog( "CBaseClient::SpawnPlayer" );

	if ( !IsFakeClient() )
	{
		// free old baseline snapshot
		FreeBaselines();
		
		// create baseline snapshot for real clients
		m_pBaseline = framesnapshotmanager->CreateEmptySnapshot( 0, MAX_EDICTS );
	}

	// Set client clock to match server's
	NET_Tick tick( m_Server->GetTick(), host_frametime_unbounded, host_frametime_stddeviation );
	SendNetMsg( tick, true );
	
	// Spawned into server, not fully active, though
	m_nSignonState = SIGNONSTATE_SPAWN;
	NET_SignonState signonState (m_nSignonState, m_Server->GetSpawnCount() );
	SendNetMsg( signonState );
}

bool CBaseClient::SendSignonData( void )
{
	COM_TimestampedLog( " CBaseClient::SendSignonData" );
#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_SENDSIGNONDATA);
#endif

	if ( m_Server->m_Signon.IsOverflowed() )
	{
		Host_Error( "Signon buffer overflowed %i bytes!!!\n", m_Server->m_Signon.GetNumBytesWritten() );
		return false;
	}

	m_NetChannel->SendData( m_Server->m_Signon );
		
	m_nSignonState = SIGNONSTATE_PRESPAWN;
	NET_SignonState signonState( m_nSignonState, m_Server->GetSpawnCount() );
	
	return m_NetChannel->SendNetMsg( signonState );
}

void CBaseClient::Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, int clientChallenge )
{
	COM_TimestampedLog( "CBaseClient::Connect" );
#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONCONNECT);
#endif
	Clear();

	m_ConVars = new KeyValues("userinfo");
	m_bInitialConVarsSet = false;

	m_UserID = nUserID;

	SetName( szName );
	m_fTimeLastNameChange = 0.0;

	m_bFakePlayer = bFakePlayer;
	m_NetChannel = pNetChannel;

	if ( m_NetChannel && m_Server && m_Server->IsMultiplayer() )
	{
		m_NetChannel->SetCompressionMode( true );
	}

	m_clientChallenge = clientChallenge;

	m_nSignonState = SIGNONSTATE_CONNECTED;

	if ( bFakePlayer )
	{
		// Hidden fake players and the HLTV/Replay bot will get removed by CSteam3Server::SendUpdatedServerDetails.
		Steam3Server().NotifyLocalClientConnect( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Drops client from server, with explanation
// Input  : *cl -
//			crash -
//			*fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseClient::Disconnect( const char *fmt, ... )
{
	va_list		argptr;
	char		string[1024];

	if ( m_nSignonState == SIGNONSTATE_NONE )
		return;	// no recursion

#if !defined( SWDS ) && defined( ENABLE_RPT )
	SV_NotifyRPTOfDisconnect( m_nClientSlot );
#endif

#ifndef _XBOX
	Steam3Server().NotifyClientDisconnect( this );
#endif
	m_nSignonState = SIGNONSTATE_NONE;

	// clear user info 
	m_Server->UserInfoChanged( m_nClientSlot );

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	ConMsg("Dropped %s from server (%s)\n", GetClientName(), string );

	// remove the client as listener
	g_GameEventManager.RemoveListener( this );

	// Send the remaining reliable buffer so the client finds out the server is shutting down.
	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown( string ) ;
		m_NetChannel = NULL;
	}

	Clear(); // clear state
#ifndef _XBOX
	NotifyDedicatedServerUI("UpdatePlayers");
#endif
	Steam3Server().SendUpdatedServerDetails(); // Update the master server.
}

void CBaseClient::FireGameEvent( IGameEvent *event )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	char buffer_data[MAX_EVENT_BYTES];

	SVC_GameEvent eventMsg;

	eventMsg.m_DataOut.StartWriting( buffer_data, sizeof(buffer_data) );

	// create bitstream from KeyValues
	if ( g_GameEventManager.SerializeEvent( event, &eventMsg.m_DataOut ) )
	{
		if ( m_NetChannel )
		{
			bool bSent = m_NetChannel->SendNetMsg( eventMsg );
			if ( !bSent )
				DevMsg("GameEventManager: failed to send event '%s'.\n", event->GetName() );
		}
	}
	else
	{
		DevMsg("GameEventManager: failed to serialize event '%s'.\n", event->GetName() );
	}
}

bool CBaseClient::SendServerInfo( void )
{
	COM_TimestampedLog( " CBaseClient::SendServerInfo" );

	// supporting smaller stack
	byte *buffer = (byte *)MemAllocScratch( NET_MAX_PAYLOAD );

	bf_write msg( "SV_SendServerinfo->msg", buffer, NET_MAX_PAYLOAD );

	// Only send this message to developer console, or multiplayer clients.
	if ( developer.GetBool() || m_Server->IsMultiplayer() )
	{
		char devtext[ 2048 ];
		int curplayers = m_Server->GetNumClients();

		Q_snprintf( devtext, sizeof( devtext ), 
			"\n%s\nMap: %s\nPlayers: %i / %i\nBuild: %d\nServer Number: %i\n\n",
			serverGameDLL->GetGameDescription(),
			m_Server->GetMapName(),
			curplayers, m_Server->GetMaxClients(),
			build_number(),
			m_Server->GetSpawnCount() );

		SVC_Print printMsg( devtext );

		printMsg.WriteToBuffer( msg );
	}

	SVC_ServerInfo serverinfo;	// create serverinfo message

	serverinfo.m_nPlayerSlot = m_nClientSlot; // own slot number

	m_Server->FillServerInfo( serverinfo ); // fill rest of info message
	
	serverinfo.WriteToBuffer( msg );

	if ( IsX360() && serverinfo.m_nMaxClients > 1 )
	{
		Msg( "Telling clients to connect" );
		g_pMatchmaking->TellClientsToConnect();
	}

	// send first tick
	m_nSignonTick = m_Server->m_nTickCount;
	
	NET_Tick signonTick( m_nSignonTick, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// write stringtable baselines
#ifndef SHARED_NET_STRING_TABLES
	m_Server->m_StringTables->WriteBaselines( msg );
#endif
	
	// Write replicated ConVars to non-listen server clients only
	if ( !m_NetChannel->IsLoopback() )
	{
		NET_SetConVar convars;
		Host_BuildConVarUpdateMessage( &convars, FCVAR_REPLICATED, true );

		convars.WriteToBuffer( msg );
	}

	m_bSendServerInfo = false;

	// send signon state
	m_nSignonState = SIGNONSTATE_NEW;
	NET_SignonState signonMsg( m_nSignonState, m_Server->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );

	// send server info as one data block
	if ( !m_NetChannel->SendData( msg ) )
	{
		MemFreeScratch();
		Disconnect("Server info data overflow");
		return false;
	}
		
	COM_TimestampedLog( " CBaseClient::SendServerInfo(finished)" );

	MemFreeScratch();

	return true;
}

CClientFrame *CBaseClient::GetDeltaFrame( int nTick )
{
	Assert( 0 ); // derive moe
	return NULL; // CBaseClient has no delta frames
}

void CBaseClient::WriteGameSounds(bf_write &buf)
{
	// CBaseClient has no events
}

void CBaseClient::ConnectionStart(INetChannel *chan)
{
	REGISTER_NET_MSG( Tick );
	REGISTER_NET_MSG( StringCmd );
	REGISTER_NET_MSG( SetConVar );
	REGISTER_NET_MSG( SignonState );

	REGISTER_CLC_MSG( ClientInfo );
	REGISTER_CLC_MSG( Move );
	REGISTER_CLC_MSG( VoiceData );
	REGISTER_CLC_MSG( BaselineAck );
	REGISTER_CLC_MSG( ListenEvents );
	
	REGISTER_CLC_MSG( RespondCvarValue );
	REGISTER_CLC_MSG( FileCRCCheck );
	REGISTER_CLC_MSG( FileMD5Check );

#if defined( REPLAY_ENABLED )
	REGISTER_CLC_MSG( SaveReplay );
#endif

	REGISTER_CLC_MSG( CmdKeyValues );
}

bool CBaseClient::ProcessTick( NET_Tick *msg )
{
	m_NetChannel->SetRemoteFramerate( msg->m_flHostFrameTime, msg->m_flHostFrameTimeStdDeviation );
	return UpdateAcknowledgedFramecount( msg->m_nTick );
}

bool CBaseClient::ProcessStringCmd( NET_StringCmd *msg )
{
	ExecuteStringCommand( msg->m_szCommand );
	return true;
}

bool CBaseClient::ProcessSetConVar( NET_SetConVar *msg )
{
	for ( int i=0; i<msg->m_ConVars.Count(); i++ )
	{
		const char *name = msg->m_ConVars[i].name;
		const char *value = msg->m_ConVars[i].value;

		// Discard any convar change request if contains funky characters
		bool bFunky = false;
		for (const char *s = name ; *s != '\0' ; ++s )
		{
			if ( !V_isalnum(*s) && *s != '_' )
			{
				bFunky = true;
				break;
			}
		}
		if ( bFunky )
		{
			Msg( "Ignoring convar change request for variable '%s' from client %s; invalid characters in the variable name\n", name, GetClientName() );
			continue;
		}

		// "name" convar is handled differently
		if ( V_stricmp( name, "name" ) == 0 )
		{
			ClientRequestNameChange( value );
			continue;
		}

		// The initial set of convars must contain all client convars that are flagged userinfo. This is a simple fix to
		// exploits that send bogus data later, and catches bugs (why are new userinfo convars appearing later?)
		if ( m_bInitialConVarsSet && !m_ConVars->FindKey( name ) )
		{
#ifndef _DEBUG	// warn all the time in debug build
			static double s_dblLastWarned = 0.0;
			double dblTimeNow = Plat_FloatTime();
			if ( dblTimeNow - s_dblLastWarned > 10 )
#endif
			{
#ifndef _DEBUG
				s_dblLastWarned = dblTimeNow;
#endif
				Warning( "Client \"%s\" userinfo ignored: \"%s\" = \"%s\"\n",
				         this->GetClientName(), name, value );
			}
			continue;
		}

		m_ConVars->SetString( name, value );

		// DevMsg( 1, " UserInfo update %s: %s = %s\n", m_Client->m_Name, name, value );
	}

	m_bConVarsChanged = true;
	m_bInitialConVarsSet = true;

	return true;
}

bool CBaseClient::ProcessSignonState( NET_SignonState *msg)
{
	if ( msg->m_nSignonState == SIGNONSTATE_CHANGELEVEL )
	{
		return true; // ignore this message
	}

	if ( msg->m_nSignonState > SIGNONSTATE_CONNECTED )
	{
		if ( msg->m_nSpawnCount != m_Server->GetSpawnCount() )
		{
			Reconnect();
			return true;
		}
	}

	// client must acknowledge our current state, otherwise start again
	if ( msg->m_nSignonState != m_nSignonState )
	{
		Reconnect();
		return true;
	}

	return SetSignonState( msg->m_nSignonState, msg->m_nSpawnCount );
}

bool CBaseClient::ProcessClientInfo( CLC_ClientInfo *msg )
{
	if ( m_nSignonState != SIGNONSTATE_NEW )
	{
		Warning( "Dropping ClientInfo packet from client not in appropriate state\n" );
		return false;
	}

	m_nSendtableCRC = msg->m_nSendTableCRC;

	// Protect against spoofed packets claiming to be HLTV clients
	if ( ( hltv && hltv->IsTVRelay() ) || tv_enable.GetBool() )
	{
		m_bIsHLTV = msg->m_bIsHLTV;
	}
	else
	{
		m_bIsHLTV = false;
	}

#if defined( REPLAY_ENABLED )
	m_bIsReplay = msg->m_bIsReplay;
#endif

	m_nFilesDownloaded = 0;
	m_nFriendsID = msg->m_nFriendsID;
	Q_strncpy( m_FriendsName, msg->m_FriendsName, sizeof(m_FriendsName) );

	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		m_nCustomFiles[i].crc = msg->m_nCustomFiles[i];
		m_nCustomFiles[i].reqID = 0;
	}

	if ( msg->m_nServerCount != m_Server->GetSpawnCount() )
	{
		Reconnect();	// client still in old game, reconnect
	}

	return true;
}

bool CBaseClient::ProcessBaselineAck( CLC_BaselineAck *msg )
{
	if ( msg->m_nBaselineTick != m_nBaselineUpdateTick )
	{
		// This occurs when there are multiple ack's queued up for processing from a client.
		return true;
	}

	if ( msg->m_nBaselineNr != m_nBaselineUsed )
	{
		DevMsg("CBaseClient::ProcessBaselineAck: wrong baseline nr received (%i)\n", msg->m_nBaselineTick );
		return true;
	}

	Assert( m_pBaseline );	

	// copy ents send as full updates this frame into baseline stuff
	CClientFrame *frame = GetDeltaFrame( m_nBaselineUpdateTick );
	if ( frame == NULL )
	{
		// Will get here if we have a lot of packet loss and finally receive a stale ack from 
		//  remote client.  Our "window" could be well beyond what it's acking, so just ignore the ack.
		return true;
	}

	CFrameSnapshot *pSnapshot = frame->GetSnapshot();

	if ( pSnapshot == NULL )
	{
		// TODO if client lags for a couple of seconds the snapshot is lost
		// fix: don't remove snapshots that are labled a possible basline candidates
		// or: send full update
		DevMsg("CBaseClient::ProcessBaselineAck: invalid frame snapshot (%i)\n", m_nBaselineUpdateTick );
		return false;
	}
	
	int index = m_BaselinesSent.FindNextSetBit( 0 );

	while ( index >= 0 )
	{
		// get new entity
		PackedEntityHandle_t hNewEntity = pSnapshot->m_pEntities[index].m_pPackedData;
		if ( hNewEntity == INVALID_PACKED_ENTITY_HANDLE )
		{
			DevMsg("CBaseClient::ProcessBaselineAck: invalid packet handle (%i)\n", index );
			return false;
		}

		PackedEntityHandle_t hOldEntity = m_pBaseline->m_pEntities[index].m_pPackedData;

		if ( hOldEntity != INVALID_PACKED_ENTITY_HANDLE )
		{
			// remove reference before overwriting packed entity
			framesnapshotmanager->RemoveEntityReference( hOldEntity );
		}

		// increase reference
		framesnapshotmanager->AddEntityReference( hNewEntity );
		
		// copy entity handle, class & serial number to
		m_pBaseline->m_pEntities[index] = pSnapshot->m_pEntities[index];

		// go to next entity
		index = m_BaselinesSent.FindNextSetBit( index + 1 );
	}

	m_pBaseline->m_nTickCount = m_nBaselineUpdateTick;

	// flip used baseline flag
	m_nBaselineUsed = (m_nBaselineUsed==1)?0:1;

	m_nBaselineUpdateTick = -1; // ready to update baselines again

	return true;
}

bool CBaseClient::ProcessListenEvents( CLC_ListenEvents *msg )
{
	// first remove the client as listener
	g_GameEventManager.RemoveListener( this );

	for ( int i=0; i < MAX_EVENT_NUMBER; i++ )
	{
		if ( msg->m_EventArray.Get(i) )
		{
			CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( i );

			if ( descriptor )
			{
				g_GameEventManager.AddListener( this, descriptor, CGameEventManager::CLIENTSTUB );
			}
			else
			{
				DevMsg("ProcessListenEvents: game event %i not found.\n", i );
				return false;
			}
		}
	}

	return true;
}

extern int GetNetSpikeValue();

void CBaseClient::StartTrace( bf_write &msg )
{

	// Should we be tracing?
	m_Trace.m_nMinWarningBytes = 0;
	if ( !IsHLTV() && !IsReplay() && !IsFakeClient() )
		m_Trace.m_nMinWarningBytes = GetNetSpikeValue();
	if ( m_iTracing < 2 )
	{
		if ( m_Trace.m_nMinWarningBytes <= 0 && sv_netspike_sendtime_ms.GetFloat() <= 0.0f )
		{
			m_iTracing = 0;
			return;
		}

		m_iTracing = 1;
	}
	m_Trace.m_nStartBit = msg.GetNumBitsWritten();
	m_Trace.m_nCurBit = m_Trace.m_nStartBit;
	m_Trace.m_StartSendTime = Plat_FloatTime();
}

#define SERVER_PACKETS_LOG	"netspike.txt"

void CBaseClient::EndTrace( bf_write &msg )
{
	if ( m_iTracing == 0 )
		return;
	VPROF_BUDGET( "CBaseClient::EndTrace", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	int bits = m_Trace.m_nCurBit - m_Trace.m_nStartBit;
	float flElapsedMs = ( Plat_FloatTime() - m_Trace.m_StartSendTime ) * 1000.0;
	int nBitThreshold = m_Trace.m_nMinWarningBytes << 3;
	if ( m_iTracing < 2 // not forced
		&& ( nBitThreshold <= 0 || bits < nBitThreshold ) // didn't exceed data threshold
		&& ( sv_netspike_sendtime_ms.GetFloat() <= 0.0f || flElapsedMs < sv_netspike_sendtime_ms.GetFloat() ) ) // didn't exceed time threshold
	{
		m_Trace.m_Records.RemoveAll();
		m_iTracing = 0;
		return;
	}

	CUtlBuffer logData( 0, 0, CUtlBuffer::TEXT_BUFFER );

	logData.Printf( "%f/%d Player [%s][%d][adr:%s] was sent a datagram %d bits (%8.3f bytes), took %.2fms\n",
		realtime, 
		host_tickcount,
		GetClientName(), 
		GetPlayerSlot(), 
		GetNetChannel()->GetAddress(),
		bits, (float)bits / 8.0f,
		flElapsedMs
	);

	// Write header line to the log if we aren't writing the whole thing
	if ( ( sv_netspike_output.GetInt() & 2 ) == 0 )
		Log("netspike: %s", logData.String() );

	for ( int i = 0 ; i < m_Trace.m_Records.Count() ; ++i )
	{
		Spike_t &sp = m_Trace.m_Records[ i ];
		logData.Printf( "%64.64s : %8d bits (%8.3f bytes)\n", sp.m_szDesc, sp.m_nBits, (float)sp.m_nBits / 8.0f );
	}

	if ( sv_netspike_output.GetInt() & 1 )
		COM_LogString( SERVER_PACKETS_LOG, logData.String() );
	if ( sv_netspike_output.GetInt() & 2 )
		Log( "%s", logData.String() );
	ETWMark1S( "netspike", logData.String() );
	m_Trace.m_Records.RemoveAll();
	m_iTracing = 0;
}

void CBaseClient::TraceNetworkData( bf_write &msg, char const *fmt, ... )
{
	if ( !IsTracing() )
		return;
	VPROF_BUDGET( "CBaseClient::TraceNetworkData", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	char buf[ 64 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	Spike_t t;
	Q_strncpy( t.m_szDesc, buf, sizeof( t.m_szDesc ) );
	t.m_nBits = msg.GetNumBitsWritten() - m_Trace.m_nCurBit;
	m_Trace.m_Records.AddToTail( t );
	m_Trace.m_nCurBit = msg.GetNumBitsWritten();
}

void CBaseClient::TraceNetworkMsg( int nBits, char const *fmt, ... )
{
	if ( !IsTracing() )
		return;
	VPROF_BUDGET( "CBaseClient::TraceNetworkMsg", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	char buf[ 64 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	Spike_t t;
	Q_strncpy( t.m_szDesc, buf, sizeof( t.m_szDesc ) );
	t.m_nBits = nBits;
	m_Trace.m_Records.AddToTail( t );
}

void CBaseClient::SendSnapshot( CClientFrame *pFrame )
{
	// never send the same snapshot twice
	if ( m_pLastSnapshot == pFrame->GetSnapshot() )
	{
		m_NetChannel->Transmit();	
		return;
	}

	// if we send a full snapshot (no delta-compression) before, wait until client
	// received and acknowledge that update. don't spam client with full updates
	if ( m_nForceWaitForTick > 0 )
	{
		// just continue transmitting reliable data
		m_NetChannel->Transmit();	
		return;
	}

	VPROF_BUDGET( "SendSnapshot", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	bool bFailedOnce = false;
write_again:
	bf_write msg( "CBaseClient::SendSnapshot", m_SnapshotScratchBuffer, sizeof( m_SnapshotScratchBuffer ) );

	TRACE_PACKET( ( "SendSnapshot(%d)\n", pFrame->tick_count ) );

	// now create client snapshot packet
	CClientFrame *deltaFrame = GetDeltaFrame( m_nDeltaTick ); // NULL if delta_tick is not found
	if ( !deltaFrame )
	{
		// We need to send a full update and reset the instanced baselines
		OnRequestFullUpdate();
	}

	// send tick time
	NET_Tick tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );

	StartTrace( msg );

	tickmsg.WriteToBuffer( msg );

	if ( IsTracing() )
	{
		TraceNetworkData( msg, "NET_Tick" );
	}

#ifndef SHARED_NET_STRING_TABLES
	// in LocalNetworkBackdoor mode we updated the stringtables already in SV_ComputeClientPacks()
	if ( !g_pLocalNetworkBackdoor )
	{
		// Update shared client/server string tables. Must be done before sending entities
		m_Server->m_StringTables->WriteUpdateMessage( this, GetMaxAckTickCount(), msg );
	}
#endif

	int nDeltaStartBit = 0;
	if ( IsTracing() )
	{
		nDeltaStartBit = msg.GetNumBitsWritten();
	}

	// send entity update, delta compressed if deltaFrame != NULL
	m_Server->WriteDeltaEntities( this, pFrame, deltaFrame, msg );

	if ( IsTracing() )
	{
		int nBits = msg.GetNumBitsWritten() - nDeltaStartBit;
		TraceNetworkMsg( nBits, "Total Delta" );
	}
			
	// send all unreliable temp entities between last and current frame
	// send max 64 events in multi player, 255 in SP
	int nMaxTempEnts = m_Server->IsMultiplayer() ? 64 : 255;
	m_Server->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), msg, nMaxTempEnts );

	if ( IsTracing() )
	{
		TraceNetworkData( msg, "Temp Entities" );
	}

	WriteGameSounds( msg );
	
	// write message to packet and check for overflow
	if ( msg.IsOverflowed() )
	{
		bool bWasTracing = IsTracing();
		if ( bWasTracing )
		{
			TraceNetworkMsg( 0, "Finished [delta %s]", deltaFrame ? "yes" : "no" );
			EndTrace( msg );
		}

		if ( !deltaFrame )
		{

			if ( !bWasTracing )
			{

				// Check for debugging by dumping a snapshot
				if ( sv_netspike_on_reliable_snapshot_overflow.GetBool() )
				{
					if ( !bFailedOnce ) // shouldn't be necessary, but just in case
					{
						Warning(" RELIABLE SNAPSHOT OVERFLOW!  Triggering trace to see what is so large\n" );
						bFailedOnce = true;
						m_iTracing = 2;
						goto write_again;
					}
					m_iTracing = 0;
				}
			}

			// if this is a reliable snapshot, drop the client
			Disconnect( "ERROR! Reliable snapshot overflow." );
			return;
		}
		else
		{
			// unreliable snapshots may be dropped
			ConMsg ("WARNING: msg overflowed for %s\n", m_Name);
			msg.Reset();
		}
	}

	// remember this snapshot
	m_pLastSnapshot = pFrame->GetSnapshot();

	// Don't send the datagram to fakeplayers unless sv_stressbots is on (which will make m_NetChannel non-null).
	if ( m_bFakePlayer && !m_NetChannel )
	{
		m_nDeltaTick = pFrame->tick_count;
		m_nStringTableAckTick = m_nDeltaTick;
		return;
	}

	bool bSendOK;

	// is this is a full entity update (no delta) ?
	if ( !deltaFrame )
	{
		VPROF_BUDGET( "SendSnapshot Transmit Full", VPROF_BUDGETGROUP_OTHER_NETWORKING );

		// transmit snapshot as reliable data chunk
		bSendOK = m_NetChannel->SendData( msg );
		bSendOK = bSendOK && m_NetChannel->Transmit();

		// remember this tickcount we send the reliable snapshot
		// so we can continue sending other updates if this has been acknowledged
		m_nForceWaitForTick = pFrame->tick_count;
	}
	else
	{
		VPROF_BUDGET( "SendSnapshot Transmit Delta", VPROF_BUDGETGROUP_OTHER_NETWORKING );

		// just send it as unreliable snapshot
		bSendOK = m_NetChannel->SendDatagram( &msg ) > 0;
	}
		
	if ( bSendOK )
	{
		if ( IsTracing() )
		{
			TraceNetworkMsg( 0, "Finished [delta %s]", deltaFrame ? "yes" : "no" );
			EndTrace( msg );
		}
	}
	else
	{
		Disconnect( "ERROR! Couldn't send snapshot." );
	}
}

bool CBaseClient::ExecuteStringCommand( const char *pCommand )
{
	if ( !pCommand || !pCommand[0] )
		return false;

	if ( !Q_stricmp( pCommand, "demorestart" ) )
	{
		DemoRestart();
		// trick, dont return true, so serverGameClients gets this command too
		return false; 
	}

	return false;
}

void CBaseClient::DemoRestart()
{
	
}

bool CBaseClient::ShouldSendMessages( void )
{
	if ( !IsConnected() )
		return false;

	// if the reliable message overflowed, drop the client
	if ( m_NetChannel && m_NetChannel->IsOverflowed() )
	{
		m_NetChannel->Reset();
		Disconnect ("%s overflowed reliable buffer\n", m_Name );
		return false;
	}

	// check, if it's time to send the next packet
	bool bSendMessage = m_fNextMessageTime <= net_time ;

	if ( !bSendMessage && !IsActive() )
	{
		// if we are in signon modem instantly reply if
		// we got a answer and have reliable data waiting
		if ( m_bReceivedPacket && m_NetChannel && m_NetChannel->HasPendingReliableData() )
		{
			bSendMessage = true;
		}
	}

	if ( bSendMessage && m_NetChannel && !m_NetChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// tell netchannel that we are choking a packet
		m_NetChannel->SetChoked();	
		// Record an ETW event to indicate that we are throttling.
		ETWThrottled();
		bSendMessage = false;
	}

	return bSendMessage;
}

void CBaseClient::UpdateSendState( void )
{
	// wait for next incoming packet
	m_bReceivedPacket = false;

	// in single player mode always send messages
	if ( !m_Server->IsMultiplayer() && !host_limitlocal.GetFloat() )
	{
		m_fNextMessageTime = net_time; // send ASAP and 
		m_bReceivedPacket = true;	// don't wait for incoming packets
	}
	else if ( IsActive() )	// multiplayer mode
	{
		// snapshot mode: send snapshots frequently
		float maxDelta = min ( m_Server->GetTickInterval(), m_fSnapshotInterval );
		float delta = clamp( (float)( net_time - m_fNextMessageTime ), 0.0f, maxDelta );
		m_fNextMessageTime = net_time + m_fSnapshotInterval - delta;
	}
	else // multiplayer signon mode
	{
		if ( m_NetChannel && m_NetChannel->HasPendingReliableData() && 
			m_NetChannel->GetTimeSinceLastReceived() < 1.0f )
		{
			// if we have pending reliable data send as fast as possible
			m_fNextMessageTime = net_time;
		}
		else
		{
			// signon mode: only respond on request or after 1 second
			m_fNextMessageTime = net_time + 1.0f;
		}
	}
}

void CBaseClient::UpdateUserSettings()
{
	int rate = m_ConVars->GetInt( "rate", DEFAULT_RATE );

	if ( sv.IsActive() )
	{
		// If we're running a local listen server then set the rate very high
		// in order to avoid delays due to network throttling. This allows for
		// easier profiling of other issues (it removes most of the frame-render
		// time which can otherwise dominate profiles) and saves developer time
		// by making maps and models load much faster.
		if ( rate == DEFAULT_RATE )
		{
			// Only override the rate if the user hasn't customized it.
			// The max rate should be a million or so in order to truly
			// eliminate networking delays.
			rate = MAX_RATE;
		}
	}

	// set server to client network rate
	SetRate( rate, false );

	// set server to client update rate
	SetUpdateRate( m_ConVars->GetInt( "cl_updaterate", 20), false );

	SetMaxRoutablePayloadSize( m_ConVars->GetInt( "net_maxroutable", MAX_ROUTABLE_PAYLOAD ) );

	m_Server->UserInfoChanged( m_nClientSlot );

	m_bConVarsChanged = false;
}

void CBaseClient::ClientRequestNameChange( const char *pszNewName )
{
	// This is called several times.  Only show a status message the first time.
	bool bShowStatusMessage = ( m_szPendingNameChange[0] == '\0' );
	
	V_strcpy_safe( m_szPendingNameChange, pszNewName );
	CheckFlushNameChange( bShowStatusMessage );
}

void CBaseClient::CheckFlushNameChange( bool bShowStatusMessage /*= false*/ )
{
	if ( !IsConnected() )
		return;
	
	if ( m_szPendingNameChange[0] == '\0' )
		return;
	
	if ( m_bPlayerNameLocked )
		return;

	// Did they change it back to the original?
	if ( !Q_strcmp( m_szPendingNameChange, m_Name ) )
	{

		// Nothing really pending, they already changed it back
		// we had a chance to apply the other one!
		m_szPendingNameChange[0] = '\0';
		return;
	}

	// Check for throttling name changes
	// Don't do it on bots
	if ( !IsFakeClient() && IsNameChangeOnCooldown( bShowStatusMessage ) )
	{
		return;
	}

	// Set the new name
	m_fTimeLastNameChange = Plat_FloatTime();
	SetName( m_szPendingNameChange );
}

bool CBaseClient::IsNameChangeOnCooldown( bool bShowStatusMessage /*= false*/ )
{
	// Check cooldown.  The first name change is free
	if ( m_fTimeLastNameChange > 0.0 )
	{
		// Too recent?
		double timeNow = Plat_FloatTime();
		double dNextChangeTime = m_fTimeLastNameChange + sv_namechange_cooldown_seconds.GetFloat();
		if ( timeNow < dNextChangeTime )
		{
			// Cooldown period still active; throttle the name change
			if ( bShowStatusMessage )
			{
				ClientPrintf( "You have changed your name recently, and must wait %i seconds.\n", (int)abs( timeNow - dNextChangeTime ) );
			}
			return true;
		}
	}

	return false;
}

void CBaseClient::OnRequestFullUpdate()
{
	VPROF_BUDGET( "CBaseClient::OnRequestFullUpdate", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// client requests a full update 
	m_pLastSnapshot = NULL;

	// free old baseline snapshot
	FreeBaselines();

	// and create new baseline snapshot
	m_pBaseline = framesnapshotmanager->CreateEmptySnapshot( 0, MAX_EDICTS );

	DevMsg("Sending full update to Client %s\n", GetClientName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//-----------------------------------------------------------------------------
bool CBaseClient::UpdateAcknowledgedFramecount(int tick)
{
	if ( IsFakeClient() )
	{
		// fake clients are always fine
		m_nDeltaTick = tick; 
		m_nStringTableAckTick = tick;
		return true;
	}

	// are we waiting for full reliable update acknowledge
	if ( m_nForceWaitForTick > 0 )
	{
		if ( tick > m_nForceWaitForTick )
		{
			// we should never get here since full updates are transmitted as reliable data now
			// Disconnect("Acknowledging reliable snapshot failed.\n");
			return true;
		}
		else if ( tick == -1 )
		{
			if( !m_NetChannel->HasPendingReliableData() )
			{
				// that's strange: we sent the client a full update, and it was fully received ( no reliable data in waiting buffers )
				// but the client is requesting another full update.
				//
				// This can happen if they request full updates in succession really quickly (using cl_fullupdate or "record X;stop" quickly).
				// There was a bug here where if we just return out, the client will have nuked its entities and we'd send it
				// a supposedly uncompressed update but m_nDeltaTick was not -1, so it was delta'd and it'd miss lots of stuff.
				// Led to clients getting full spectator mode radar while their player was not a spectator.
				ConDMsg("Client forced immediate full update.\n");
				m_nForceWaitForTick = m_nDeltaTick = -1;
				OnRequestFullUpdate();
				return true;
			}
		}
		else if ( tick < m_nForceWaitForTick )
		{
			// keep on waiting, do nothing
			return true;
		}
		else // ( tick == m_nForceWaitForTick )
		{
			// great, the client acknowledge the tick we send the full update
			m_nForceWaitForTick = -1; 
			// continue sending snapshots...
		}	 
	}
	else
	{
		if ( m_nDeltaTick == -1 )
		{
			// we still want to send a full update, don't change delta_tick from -1
			return true;
		}

		if ( tick == -1 )
		{
			OnRequestFullUpdate();
		}
		else
		{
			if ( m_nDeltaTick > tick )
			{
				// client already acknowledged new tick and now switch back to older
				// thats not allowed since we always delete older frames
				Disconnect("Client delta ticks out of order.\n");
				return false;
			}
		}
	}

	// get acknowledged client frame
	m_nDeltaTick = tick; 

	if ( m_nDeltaTick > -1 )
	{
		m_nStringTableAckTick = m_nDeltaTick;
	}

	if ( (m_nBaselineUpdateTick > -1) && (m_nDeltaTick > m_nBaselineUpdateTick) )
	{
		// server sent a baseline update, but it wasn't acknowledged yet so it was probably lost. 
		m_nBaselineUpdateTick = -1;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: return a string version of the userid
//-----------------------------------------------------------------------------
const char *GetUserIDString( const USERID_t& id )
{
	static char idstr[ MAX_NETWORKID_LENGTH ];

	idstr[ 0 ] = 0;

	switch ( id.idtype )
	{
	case IDTYPE_STEAM:
		{
			CSteamID nullID;

			if ( Steam3Server().BLanOnly() && nullID == id.steamid ) 
			{
				V_strcpy_safe( idstr, "STEAM_ID_LAN" );
			}
			else if ( nullID == id.steamid )
			{
				V_strcpy_safe( idstr, "STEAM_ID_PENDING" );
			}
			else
			{
				V_sprintf_safe( idstr, "%s", id.steamid.Render() );
			}
		}
		break;		
	case IDTYPE_HLTV:
		{
			V_strcpy_safe( idstr, "HLTV" );
		}
		break;
	case IDTYPE_REPLAY:
		{
			V_strcpy_safe( idstr, "REPLAY" );
		}
		break;
	default:
		{
			V_strcpy_safe( idstr, "UNKNOWN" );
		}
		break;
	}

	return idstr;
}

//-----------------------------------------------------------------------------
// Purpose: return a string version of the userid
//-----------------------------------------------------------------------------
const char *CBaseClient::GetNetworkIDString() const
{
	if ( IsFakeClient() )
	{
		return "BOT";
	}

	return ( GetUserIDString( GetNetworkID() ) );
}

bool CBaseClient::IgnoreTempEntity( CEventInfo *event )
{
	int iPlayerIndex = GetPlayerSlot()+1;

	return !event->filter.IncludesPlayer( iPlayerIndex );
}

const USERID_t CBaseClient::GetNetworkID() const
{
	USERID_t userID;

	userID.steamid = m_SteamID;
	userID.idtype = IDTYPE_STEAM; 

	return userID;
}

void CBaseClient::SetSteamID( const CSteamID &steamID )
{
	m_SteamID = steamID;
}

void CBaseClient::SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize )
{
	if ( m_NetChannel )
	{
		m_NetChannel->SetMaxRoutablePayloadSize( nMaxRoutablePayloadSize );
	}
}

int CBaseClient::GetMaxAckTickCount() const
{
	int nMaxTick = m_nSignonTick;
	if ( m_nDeltaTick > nMaxTick )
	{
		nMaxTick = m_nDeltaTick;
	}
	if ( m_nStringTableAckTick > nMaxTick )
	{
		nMaxTick = m_nStringTableAckTick;
	}
	return nMaxTick;
}

bool CBaseClient::ProcessCmdKeyValues( CLC_CmdKeyValues *msg )
{
	return true;
}

void CBaseClient::OnSignonStateFull()
{
#if defined( REPLAY_ENABLED )
	if ( g_pReplay && g_pServerReplayContext )
	{
		g_pServerReplayContext->CreateSessionOnClient( m_nClientSlot );
	}
#endif
}
