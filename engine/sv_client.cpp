//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "server_pch.h"
#include "framesnapshot.h"
#include "checksum_engine.h"
#include "sv_main.h"
#include "GameEventManager.h"
#include "networkstringtable.h"
#include "demo.h"
#include "PlayerState.h"
#include "tier0/vprof.h"
#include "sv_packedentities.h"
#include "LocalNetworkBackdoor.h"
#include "testscriptmgr.h"
#include "hltvserver.h"
#include "pr_edict.h"
#include "logofile_shared.h"
#include "dt_send_eng.h"
#include "sv_plugin.h"
#include "download.h"
#include "cmodel_engine.h"
#include "tier1/CommandBuffer.h"
#include "gl_cvars.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#include "replay_internal.h"
#endif
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CNetworkStringTableContainer *networkStringTableContainerServer;

static ConVar	sv_timeout( "sv_timeout", "65", 0, "After this many seconds without a message from a client, the client is dropped" );
static ConVar	sv_maxrate( "sv_maxrate", "0", FCVAR_REPLICATED, "Max bandwidth rate allowed on server, 0 == unlimited" );
static ConVar	sv_minrate( "sv_minrate", "3500", FCVAR_REPLICATED, "Min bandwidth rate allowed on server, 0 == unlimited" );
       
       ConVar	sv_maxupdaterate( "sv_maxupdaterate", "66", FCVAR_REPLICATED, "Maximum updates per second that the server will allow" );
	   ConVar	sv_minupdaterate( "sv_minupdaterate", "10", FCVAR_REPLICATED, "Minimum updates per second that the server will allow" );

	   ConVar	sv_stressbots("sv_stressbots", "0", FCVAR_DEVELOPMENTONLY, "If set to 1, the server calculates data and fills packets to bots. Used for perf testing.");
static ConVar	sv_allowdownload ("sv_allowdownload", "1", 0, "Allow clients to download files");
static ConVar	sv_allowupload ("sv_allowupload", "1", 0, "Allow clients to upload customizations files");
	   ConVar	sv_sendtables ( "sv_sendtables", "0", FCVAR_DEVELOPMENTONLY, "Force full sendtable sending path." );
	   

extern ConVar sv_maxreplay;
extern ConVar tv_snapshotrate;
extern ConVar tv_transmitall;
extern ConVar sv_pure_kick_clients;
extern ConVar sv_pure_trace;

// static ConVar sv_failuretime( "sv_failuretime", "0.5", 0, "After this long without a packet from client, don't send any more until client starts sending again" );

static const char * s_clcommands[] = 
{
	"status",
	"pause",
	"setpause",
	"unpause",
	"ping",
	"rpt_server_enable",
	"rpt_client_enable",
#ifndef SWDS 
	"rpt",
	"rpt_connect",
	"rpt_password",
	"rpt_screenshot",
	"rpt_download_log",
#endif
	NULL,
};


// Used on the server and on the client to bound its cl_rate cvar.
int ClampClientRate( int nRate )
{
	if ( sv_maxrate.GetInt() > 0 )
	{
		nRate = clamp( nRate, MIN_RATE, sv_maxrate.GetInt() );
	}

	if ( sv_minrate.GetInt() > 0 )
	{
		nRate = clamp( nRate, sv_minrate.GetInt(), MAX_RATE );
	}

	return nRate;
}


CGameClient::CGameClient(int slot, CBaseServer *pServer )
{
	Clear();

	m_nClientSlot = slot;
	m_nEntityIndex = slot+1;
	m_Server = pServer;
	m_pCurrentFrame = NULL;
	m_bIsInReplayMode = false;

	// NULL out data we'll never use.
	memset( &m_PrevPackInfo, 0, sizeof( m_PrevPackInfo ) );
	m_PrevPackInfo.m_pTransmitEdict = &m_PrevTransmitEdict;
}

CGameClient::~CGameClient()
{

}

bool CGameClient::ProcessClientInfo( CLC_ClientInfo *msg )
{
	CBaseClient::ProcessClientInfo( msg );

	if ( m_bIsHLTV )
	{
		// Likely spoofing, or misconfiguration. Don't let Disconnect pathway believe this is a replay bot, it will
		// asplode.
		m_bIsHLTV = false;
		Disconnect( "ProcessClientInfo: SourceTV can not connect to game directly.\n" );
		return false;
	}

#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		// Likely spoofing, or misconfiguration. Don't let Disconnect pathway believe this is an hltv bot, it will
		// asplode.
		m_bIsReplay = false;
		Disconnect( "ProcessClientInfo: Replay can not connect to game directly.\n" );
		return false;
	}
#endif

	if ( sv_allowupload.GetBool() )
	{
		// download all missing customizations files from this client;
		DownloadCustomizations();
	}
	return true;
}

bool CGameClient::ProcessMove(CLC_Move *msg)
{
	// Don't process usercmds until the client is active. If we do, there can be weird behavior
	// like the game trying to send reliable messages to the client and having those messages discarded.
	if ( !IsActive() )
		return true;	

	if ( m_LastMovementTick == sv.m_nTickCount )  
	{
		// Only one movement command per frame, someone is cheating.
		return true;
	}

	m_LastMovementTick = sv.m_nTickCount; 


	int totalcmds =msg->m_nBackupCommands + msg->m_nNewCommands;

	// Decrement drop count by held back packet count
	int netdrop = m_NetChannel->GetDropNumber();

	bool ignore = !sv.IsActive();
#ifdef SWDS
	bool paused = sv.IsPaused();
#else
	bool paused = sv.IsPaused() || ( !sv.IsMultiplayer() && Con_IsVisible() );
#endif

	// Make sure player knows of correct server time
	g_ServerGlobalVariables.curtime = sv.GetTime();
	g_ServerGlobalVariables.frametime = host_state.interval_per_tick;

//	COM_Log( "sv.log", "  executing %i move commands from client starting with command %i(%i)\n",
//		numcmds, 
//		m_Client->m_NetChan->incoming_sequence,
//		m_Client->m_NetChan->incoming_sequence & SV_UPDATE_MASK );
	
	int startbit = msg->m_DataIn.GetNumBitsRead();

	serverGameClients->ProcessUsercmds
	( 
		edict,					// Player edict
		&msg->m_DataIn,
		msg->m_nNewCommands,
		totalcmds,							// Commands in packet
		netdrop,							// Number of dropped commands
		ignore,								// Don't actually run anything
		paused								// Run, but don't actually do any movement
	);


	if ( msg->m_DataIn.IsOverflowed() )
	{
		Disconnect( "ProcessUsercmds:  Overflowed reading usercmd data (check sending and receiving code for mismatches)!\n" );
		return false;
	}

	int endbit = msg->m_DataIn.GetNumBitsRead();

	if ( msg->m_nLength != (endbit-startbit) )
	{
		Disconnect( "ProcessUsercmds:  Incorrect reading frame (check sending and receiving code for mismatches)!\n" );
		return false;
	}

	return true;
}

bool CGameClient::ProcessVoiceData( CLC_VoiceData *msg )
{
	char voiceDataBuffer[4096];
	int bitsRead = msg->m_DataIn.ReadBitsClamped( voiceDataBuffer, msg->m_nLength );

	SV_BroadcastVoiceData( this, Bits2Bytes(bitsRead), voiceDataBuffer, msg->m_xuid );

	return true;
}

bool CGameClient::ProcessCmdKeyValues( CLC_CmdKeyValues *msg )
{
	serverGameClients->ClientCommandKeyValues( edict, msg->GetKeyValues() );
	return true;
}

bool CGameClient::ProcessRespondCvarValue( CLC_RespondCvarValue *msg )
{
	if ( msg->m_iCookie > 0 )
	{
		if ( g_pServerPluginHandler )
			g_pServerPluginHandler->OnQueryCvarValueFinished( msg->m_iCookie, edict, msg->m_eStatusCode, msg->m_szCvarName, msg->m_szCvarValue );
	}
	else
	{
		// Negative cookie means the game DLL asked for the value.
		if ( serverGameDLL && g_iServerGameDLLVersion >= 6 )
		{
#ifdef REL_TO_STAGING_MERGE_TODO
			serverGameDLL->OnQueryCvarValueFinished( msg->m_iCookie, edict, msg->m_eStatusCode, msg->m_szCvarName, msg->m_szCvarValue );
#endif
		}
	}
	
	return true;
}

#include "pure_server.h"
bool CGameClient::ProcessFileCRCCheck( CLC_FileCRCCheck *msg )
{
	// Ignore this message if we're not in pure server mode...
	if ( !sv.IsInPureServerMode() )
		return true;

	char warningStr[1024] = {0};

	// The client may send us files we don't care about, so filter them here
//	if ( !sv.GetPureServerWhitelist()->GetForceMatchList()->IsFileInList( msg->m_szFilename ) )
//		return true;

	// first check against all the other files users have sent
	FileHash_t filehash;
	filehash.m_md5contents = msg->m_MD5;
	filehash.m_crcIOSequence = msg->m_CRCIOs;
	filehash.m_eFileHashType = msg->m_eFileHashType;
	filehash.m_cbFileLen = msg->m_nFileFraction;
	filehash.m_nPackFileNumber = msg->m_nPackFileNumber;
	filehash.m_PackFileID = msg->m_PackFileID;

	const char *path = msg->m_szPathID;
	const char *fileName = msg->m_szFilename;
	if ( g_PureFileTracker.DoesFileMatch( path, fileName, msg->m_nFileFraction, &filehash, GetNetworkID() ) )
	{
		// track successful file
	}
	else
	{
		V_snprintf( warningStr, sizeof( warningStr ), "Pure server: file [%s]\\%s does not match the server's file.", path, fileName );
	}

	// still ToDo:
	// 1. make sure the user sends some files
	// 2. make sure the user doesnt skip any files
	// 3. make sure the user sends the right files...

	if ( warningStr[0] )
	{
		if ( sv_pure_kick_clients.GetInt() )
		{
			Disconnect( "%s", warningStr );
		}
		else
		{
			ClientPrintf( "Warning: %s\n", warningStr );
			if ( sv_pure_trace.GetInt() >= 1 )
			{
				Msg( "[%s] %s\n", GetNetworkIDString(), warningStr );
			}
		}		
	}
	else
	{
		if ( sv_pure_trace.GetInt() >= 2 )
		{
			Msg( "Pure server CRC check: client %s passed check for [%s]\\%s\n", GetClientName(), msg->m_szPathID, msg->m_szFilename );
		}
	}

	return true;
}
bool CGameClient::ProcessFileMD5Check( CLC_FileMD5Check *msg )
{
	// Legacy message
	return true;
}


#if defined( REPLAY_ENABLED )
bool CGameClient::ProcessSaveReplay( CLC_SaveReplay *pMsg )
{
	// Don't allow on listen servers
	if ( !sv.IsDedicated() )
		return false;

	if ( !g_pReplay )
		return false;

	g_pReplay->SV_NotifyReplayRequested();

	return true;
}
#endif

void CGameClient::DownloadCustomizations()
{
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( m_nCustomFiles[i].crc == 0 )
			continue; // slot not used

		CCustomFilename hexname( m_nCustomFiles[i].crc );

		if ( g_pFileSystem->FileExists( hexname.m_Filename, "game" ) )
			continue; // we already have it

		// we don't have it, request download from client

		m_nCustomFiles[i].reqID = m_NetChannel->RequestFile( hexname.m_Filename );
	}
}

void CGameClient::Connect( const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, int clientChallenge )
{
	CBaseClient::Connect( szName, nUserID, pNetChannel, bFakePlayer, clientChallenge );

	edict = EDICT_NUM( m_nEntityIndex );
	
	// init PackInfo
	m_PackInfo.m_pClientEnt = edict;
	m_PackInfo.m_nPVSSize = sizeof( m_PackInfo.m_PVS );
				
	// fire global game event - server only
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_connect" );
	{
		event->SetInt( "userid", m_UserID );
		event->SetInt( "index", m_nClientSlot );
		event->SetString( "name", m_Name );
		event->SetString("networkid", GetNetworkIDString() ); 	
		event->SetString( "address", m_NetChannel?m_NetChannel->GetAddress():"none" );
		event->SetInt( "bot", m_bFakePlayer?1:0 );
		g_GameEventManager.FireEvent( event, true );
	}

	// the only difference here is we don't send an
	// IP to prevent hackers from doing evil things
	event = g_GameEventManager.CreateEvent( "player_connect_client" );
	if ( event )
	{
		event->SetInt( "userid", m_UserID );
		event->SetInt( "index", m_nClientSlot );
		event->SetString( "name", m_Name );
		event->SetString( "networkid", GetNetworkIDString() );
		event->SetInt( "bot", m_bFakePlayer ? 1 : 0 );
		g_GameEventManager.FireEvent( event );
	}
}

void CGameClient::SetupPackInfo( CFrameSnapshot *pSnapshot )
{
	// Compute Vis for each client
	m_PackInfo.m_nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;
	serverGameClients->ClientSetupVisibility( (edict_t *)m_pViewEntity,
		m_PackInfo.m_pClientEnt, m_PackInfo.m_PVS, m_PackInfo.m_nPVSSize );

	// This is the frame we are creating, i.e., the next
	// frame after the last one that the client acknowledged

	m_pCurrentFrame = AllocateFrame();
	m_pCurrentFrame->Init( pSnapshot );

	m_PackInfo.m_pTransmitEdict = &m_pCurrentFrame->transmit_entity;

	// if this client is the HLTV or Replay client, add the nocheck PVS bit array
	// normal clients don't need that extra array
#ifndef _XBOX
#if defined( REPLAY_ENABLED )
	if ( IsHLTV() || IsReplay() )
#else
	if ( IsHLTV() )
#endif
	{
		// the hltv client doesn't has a ClientFrame list
		m_pCurrentFrame->transmit_always = new CBitVec<MAX_EDICTS>;
		m_PackInfo.m_pTransmitAlways = m_pCurrentFrame->transmit_always;
	}
	else
#endif
	{
		m_PackInfo.m_pTransmitAlways = NULL;
	}

	// Add frame to ClientFrame list 

	int nMaxFrames = MAX_CLIENT_FRAMES;

	if ( sv_maxreplay.GetFloat() > 0 )
	{
		// if the server has replay features enabled, allow a way bigger frame buffer
		nMaxFrames = max ( (float)nMaxFrames, sv_maxreplay.GetFloat() / m_Server->GetTickInterval() );
	}
		
	if ( nMaxFrames < AddClientFrame( m_pCurrentFrame ) )
	{
		// If the client has more than 64 frames, the server will start to eat too much memory.
		RemoveOldestFrame(); 
	}
		
	// Since area to area visibility is determined by each player's PVS, copy
	//  the area network lookups into the ClientPackInfo_t
	m_PackInfo.m_AreasNetworked = 0;
	int areaCount = g_AreasNetworked.Count();
	for ( int j = 0; j < areaCount; j++ )
	{
		m_PackInfo.m_Areas[m_PackInfo.m_AreasNetworked] = g_AreasNetworked[ j ];
		m_PackInfo.m_AreasNetworked++;

		// Msg("CGameClient::SetupPackInfo: too much areas (%i)", areaCount );
		Assert( m_PackInfo.m_AreasNetworked < MAX_WORLD_AREAS );
	}
	
	CM_SetupAreaFloodNums( m_PackInfo.m_AreaFloodNums, &m_PackInfo.m_nMapAreas );
}

void CGameClient::SetupPrevPackInfo()
{
	memcpy( &m_PrevTransmitEdict, m_PackInfo.m_pTransmitEdict, sizeof( m_PrevTransmitEdict ) );
	
	// Copy the relevant fields into m_PrevPackInfo.
	m_PrevPackInfo.m_AreasNetworked = m_PackInfo.m_AreasNetworked;
	memcpy( m_PrevPackInfo.m_Areas, m_PackInfo.m_Areas, sizeof( m_PackInfo.m_Areas[0] ) * m_PackInfo.m_AreasNetworked );

	m_PrevPackInfo.m_nPVSSize = m_PackInfo.m_nPVSSize;
	memcpy( m_PrevPackInfo.m_PVS, m_PackInfo.m_PVS, m_PackInfo.m_nPVSSize );
	
	m_PrevPackInfo.m_nMapAreas = m_PackInfo.m_nMapAreas;
	memcpy( m_PrevPackInfo.m_AreaFloodNums, m_PackInfo.m_AreaFloodNums, m_PackInfo.m_nMapAreas * sizeof( m_PackInfo.m_nMapAreas ) );
}


/*
================
CheckRate

Make sure channel rate for active client is within server bounds
================
*/
void CGameClient::SetRate(int nRate, bool bForce )
{
	if ( !bForce )
	{
		nRate = ClampClientRate( nRate );
	}

	CBaseClient::SetRate( nRate, bForce );
}
void CGameClient::SetUpdateRate(int udpaterate, bool bForce)
{
	if ( !bForce )
	{
		if ( sv_maxupdaterate.GetInt() > 0 )
		{
			udpaterate = clamp( udpaterate, 1, sv_maxupdaterate.GetInt() );
		}

		if ( sv_minupdaterate.GetInt() > 0 )
		{
			udpaterate = clamp( udpaterate, sv_minupdaterate.GetInt(), 100 );
		}
	}

	CBaseClient::SetUpdateRate( udpaterate, bForce );
}


void CGameClient::UpdateUserSettings()
{
	// set voice loopback
	m_bVoiceLoopback = m_ConVars->GetInt( "voice_loopback", 0 ) != 0;

	CBaseClient::UpdateUserSettings();

	// Give entity dll a chance to look at the changes.
	// Do this after CBaseClient::UpdateUserSettings() so name changes like prepending a (1)
	// take effect before the server dll sees the name.
	g_pServerPluginHandler->ClientSettingsChanged( edict );
}



//-----------------------------------------------------------------------------
// Purpose: A File has been received, if it's a logo, send it on to any other players who need it
//  and return true, otherwise, return false
// Input  : *cl - 
//			*filename - 
// Output : Returns true on success, false on failure.
/*-----------------------------------------------------------------------------
bool CGameClient::ProcessIncomingLogo( const char *filename )
{
	char crcfilename[ 512 ];
	char logohex[ 16 ];
	Q_binarytohex( (byte *)&logo, sizeof( logo ), logohex, sizeof( logohex ) );

	Q_snprintf( crcfilename, sizeof( crcfilename ), "materials/decals/downloads/%s.vtf", logohex );

	// It's not a logo file?
	if ( Q_strcasecmp( filename, crcfilename ) )
	{
		return false;
	}

	// First, make sure crc is valid
	CRC32_t check;
	CRC_File( &check, crcfilename );
	if ( check != logo )
	{
		ConMsg( "Incoming logo file didn't match player's logo CRC, ignoring\n" );
		// Still note that it was a logo!
		return true;
	}

	// Okay, looks good, see if any other players need this logo file
	SV_SendLogo( check );
	return true;
} */


/*
===================
SV_FullClientUpdate

sends all the info about *cl to *sb
===================
*/

bool CGameClient::IsHearingClient( int index ) const
{
#if defined( REPLAY_ENABLED )
	if ( IsHLTV() || IsReplay() )
#else
	if ( IsHLTV() )
#endif
		return true;

	if ( index == GetPlayerSlot() )
		return m_bVoiceLoopback;
	
	CGameClient *pClient = sv.Client( index );
	return pClient->m_VoiceStreams.Get( GetPlayerSlot() ) != 0;
}

bool CGameClient::IsProximityHearingClient( int index ) const
{
	CGameClient *pClient = sv.Client( index );
	return pClient->m_VoiceProximity.Get( GetPlayerSlot() ) != 0;
}

void CGameClient::Inactivate( void )
{
	if ( edict && !edict->IsFree() )
	{
		m_Server->RemoveClientFromGame( this );
	}
#ifndef _XBOX
	if ( IsHLTV() )
	{	
		hltv->Changelevel();
	}

#if defined( REPLAY_ENABLED )
	if ( IsReplay() )
	{
		replay->Changelevel();
	}
#endif
#endif
	CBaseClient::Inactivate();

	m_Sounds.Purge();
	m_VoiceStreams.ClearAll();
	m_VoiceProximity.ClearAll();


	DeleteClientFrames( -1 ); // delete all
}

bool CGameClient::UpdateAcknowledgedFramecount(int tick)
{
	// free old client frames which won't be used anymore
	if ( tick != m_nDeltaTick )
	{
		// delta tick changed, free all frames smaller than tick
		int removeTick = tick;
		
		if ( sv_maxreplay.GetFloat() > 0 )
			removeTick -= (sv_maxreplay.GetFloat() / m_Server->GetTickInterval() ); // keep a replay buffer

		if ( removeTick > 0 )
		{
			DeleteClientFrames( removeTick );	
		}
	}

	return CBaseClient::UpdateAcknowledgedFramecount( tick );
}



void CGameClient::Clear()
{
#ifndef _XBOX
	if ( m_bIsHLTV && hltv )
	{
		hltv->Shutdown();
	}
	
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay && replay )
	{
		replay->Shutdown();
	}
#endif
#endif

	CBaseClient::Clear();

	// free all frames
	DeleteClientFrames( -1 );

	m_Sounds.Purge();
	m_VoiceStreams.ClearAll();
	m_VoiceProximity.ClearAll();
	edict = NULL;
	m_pViewEntity = NULL;
	m_bVoiceLoopback = false;
	m_LastMovementTick = 0;
	m_nSoundSequence = 0;
#if defined( REPLAY_ENABLED )
	m_flLastSaveReplayTime = host_time;
#endif
}

void CGameClient::Reconnect( void )
{
	// If the client was connected before, tell the game .dll to disconnect him/her.
	sv.RemoveClientFromGame( this );

	CBaseClient::Reconnect();
}

void CGameClient::Disconnect( const char *fmt, ... )
{
	va_list		argptr;
	char		reason[1024];

	if ( m_nSignonState == SIGNONSTATE_NONE )
		return;	// no recursion

	va_start (argptr,fmt);
	Q_vsnprintf (reason, sizeof( reason ), fmt,argptr);
	va_end (argptr);

	// notify other clients of player leaving the game
	// send the username and network id so we don't depend on the CBasePlayer pointer
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_disconnect" );

	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetString("reason", reason );
		event->SetString("name", GetClientName() );
		event->SetString("networkid", GetNetworkIDString() ); 
		event->SetInt( "bot", m_bFakePlayer?1:0 );
		g_GameEventManager.FireEvent( event );
	}

	m_Server->RemoveClientFromGame( this );

	CBaseClient::Disconnect( "%s", reason );
}

bool CGameClient::SetSignonState(int state, int spawncount)
{
	if ( state == SIGNONSTATE_CONNECTED )
	{
		if ( !CheckConnect() )
			return false;

		m_NetChannel->SetTimeout( SIGNON_TIME_OUT ); // allow 5 minutes to load map
		m_NetChannel->SetFileTransmissionMode( false );
		m_NetChannel->SetMaxBufferSize( true, NET_MAX_PAYLOAD );
	}
	else if ( state == SIGNONSTATE_NEW )
	{
		if ( !sv.IsMultiplayer() )
		{
			// local client as received and create string tables,
			// now link server tables to client tables
			SV_InstallClientStringTableMirrors();
		}
	}
	else if ( state == SIGNONSTATE_FULL )
	{
		if ( sv.m_bLoadgame )
		{
			// If this game was loaded from savegame, finish restoring game now
			sv.FinishRestore();
		}

		m_NetChannel->SetTimeout( sv_timeout.GetFloat() ); // use smaller timeout limit
		m_NetChannel->SetFileTransmissionMode( true );

#ifdef _XBOX
		// to save memory on the XBOX reduce reliable buffer size from 96 to 8 kB
		m_NetChannel->SetMaxBufferSize( true, 8*1024 );
#endif
	}

	return CBaseClient::SetSignonState( state, spawncount );
}

void CGameClient::SendSound( SoundInfo_t &sound, bool isReliable )
{
#if defined( REPLAY_ENABLED )
	if ( IsFakeClient() && !IsHLTV() && !IsReplay() )
#else
	if ( IsFakeClient() && !IsHLTV() )
#endif
	{
		return; // dont send sound messages to bots
	}

	// don't send sound messages while client is replay mode
	if ( m_bIsInReplayMode )
	{
		return;
	}

	// reliable sounds are send as single messages
	if ( isReliable )
	{
		SVC_Sounds	sndmsg;
		char		buffer[32];

		m_nSoundSequence = ( m_nSoundSequence + 1 ) & SOUND_SEQNUMBER_MASK;	// increase own sound sequence counter
		sound.nSequenceNumber = 0; // don't transmit nSequenceNumber for reliable sounds

		sndmsg.m_DataOut.StartWriting(buffer, sizeof(buffer) );
		sndmsg.m_nNumSounds = 1;
		sndmsg.m_bReliableSound = true;
				
		SoundInfo_t	defaultSound; defaultSound.SetDefault();

		sound.WriteDelta( &defaultSound, sndmsg.m_DataOut );

		// send reliable sound as single message
		SendNetMsg( sndmsg, true );
		return;
	}

	sound.nSequenceNumber = m_nSoundSequence;

	m_Sounds.AddToTail( sound );	// queue sounds until snapshot is send
}

void CGameClient::WriteGameSounds( bf_write &buf )
{
	if ( m_Sounds.Count() <= 0 )
		return;

	char data[NET_MAX_PAYLOAD];
	SVC_Sounds msg;
	msg.m_DataOut.StartWriting( data, sizeof(data) );
	
	int nSoundCount = FillSoundsMessage( msg );
	msg.WriteToBuffer( buf );

	if ( IsTracing() )
	{
		TraceNetworkData( buf, "Sounds [count=%d]", nSoundCount );
	}
}

int	CGameClient::FillSoundsMessage(SVC_Sounds &msg)
{
	int i, count = m_Sounds.Count();

	// send max 64 sound in multiplayer per snapshot, 255 in SP
	int max = m_Server->IsMultiplayer() ? 32 : 255;

	// Discard events if we have too many to signal with 8 bits
	if ( count > max )
		count = max;

	// Nothing to send
	if ( !count )
		return 0;

	SoundInfo_t defaultSound; defaultSound.SetDefault();
	SoundInfo_t *pDeltaSound = &defaultSound;
	
	msg.m_nNumSounds = count;
	msg.m_bReliableSound = false;
	msg.SetReliable( false );

	Assert( msg.m_DataOut.GetNumBitsLeft() > 0 );

	for ( i = 0 ; i < count; i++ )
	{
		SoundInfo_t &sound = m_Sounds[ i ];
		sound.WriteDelta( pDeltaSound, msg.m_DataOut );
		pDeltaSound = &m_Sounds[ i ];
	}

	// remove added events from list
	int remove = m_Sounds.Count() - ( count + max );

	if ( remove > 0 )
	{
		DevMsg("Warning! Dropped %i unreliable sounds for client %s.\n" , remove, m_Name );
		count+= remove;
	}
	
	if ( count > 0 )
	{
		m_Sounds.RemoveMultiple( 0, count );
	}

	Assert( m_Sounds.Count() <= max ); // keep ev_max temp ent for next update

	return msg.m_nNumSounds;
}



bool CGameClient::CheckConnect( void )
{
	// Allow the game dll to reject this client.
	char szRejectReason[128];
	Q_strncpy( szRejectReason, "Connection rejected by game\n", sizeof( szRejectReason ) );

	if ( !g_pServerPluginHandler->ClientConnect( edict, m_Name, m_NetChannel->GetAddress(), szRejectReason, sizeof( szRejectReason ) ) )
	{
		// Reject the connection and drop the client.
		Disconnect( szRejectReason, m_Name );
		return false;
	}

	return true;
}

void CGameClient::ActivatePlayer( void )
{
	CBaseClient::ActivatePlayer();

	COM_TimestampedLog( "CGameClient::ActivatePlayer -start" );

	// call the spawn function
	if ( !sv.m_bLoadgame )
	{
		g_ServerGlobalVariables.curtime = sv.GetTime();

		COM_TimestampedLog( "g_pServerPluginHandler->ClientPutInServer" );

		g_pServerPluginHandler->ClientPutInServer( edict, m_Name );
	}

    COM_TimestampedLog( "g_pServerPluginHandler->ClientActive" );

	g_pServerPluginHandler->ClientActive( edict, sv.m_bLoadgame );

	COM_TimestampedLog( "g_pServerPluginHandler->ClientSettingsChanged" );

	g_pServerPluginHandler->ClientSettingsChanged( edict );

	COM_TimestampedLog( "GetTestScriptMgr()->CheckPoint" );

	GetTestScriptMgr()->CheckPoint( "client_connected" );

	// don't send signonstate to client, client will switch to FULL as soon 
	// as the first full entity update packets has been received

	// fire a activate event
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_activate" );

	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		g_GameEventManager.FireEvent( event );
	}

	COM_TimestampedLog( "CGameClient::ActivatePlayer -end" );
}

bool CGameClient::SendSignonData( void )
{
	bool bClientHasdifferentTables = false;

	if ( sv.m_FullSendTables.IsOverflowed() )
	{
		Host_Error( "Send Table signon buffer overflowed %i bytes!!!\n", sv.m_FullSendTables.GetNumBytesWritten() );
		return false;
	}

	if ( SendTable_GetCRC() != (CRC32_t)0 )
	{
		bClientHasdifferentTables =  m_nSendtableCRC != SendTable_GetCRC();
	}

#ifdef _DEBUG
	if ( sv_sendtables.GetInt() == 2 )
	{
		// force sending class tables, for debugging
		bClientHasdifferentTables = true; 
	}
#endif

	// Write the send tables & class infos if needed
	if ( bClientHasdifferentTables )
	{
		if ( sv_sendtables.GetBool() )
		{
			// send client class table descriptions so it can rebuild tables
			ConDMsg("Client sent different SendTable CRC, sending full tables.\n" );
			m_NetChannel->SendData( sv.m_FullSendTables );
		}
		else
		{
			Disconnect( "Server uses different class tables" );
			return false;
		}
	}
	else
	{
		// use your class infos, CRC is correct
		SVC_ClassInfo classmsg( true, m_Server->serverclasses );
		m_NetChannel->SendNetMsg( classmsg );
	}

	if ( !CBaseClient::SendSignonData()	)
		return false;

	m_nSoundSequence = 1; // reset sound sequence numbers after signon block

	return true;
}


void CGameClient::SpawnPlayer( void )
{
	// run the entrance script
	if ( sv.m_bLoadgame )
	{	// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.SetPaused( false );
	}
	else
	{
		// set up the edict
		Assert( serverGameEnts );
		serverGameEnts->FreeContainingEntity( edict );
		InitializeEntityDLLFields( edict );
		
	}

	// restore default client entity and turn off replay mdoe
	m_nEntityIndex = m_nClientSlot+1;
	m_bIsInReplayMode = false;

	// set view entity
    SVC_SetView setView( m_nEntityIndex );
	SendNetMsg( setView );

	

	CBaseClient::SpawnPlayer();

	// notify that the player is spawning
	serverGameClients->ClientSpawned( edict );
}

CClientFrame *CGameClient::GetDeltaFrame( int nTick )
{
#ifndef _XBOX
	Assert ( !IsHLTV() ); // has no ClientFrames
#if defined( REPLAY_ENABLED )
	Assert ( !IsReplay() );  // has no ClientFrames
#endif
#endif	

	if ( m_bIsInReplayMode )
	{
		int followEntity; 

		serverGameClients->GetReplayDelay( edict, followEntity );

		Assert( followEntity > 0 );

		CGameClient *pFollowEntity = sv.Client( followEntity-1 );

		if ( pFollowEntity )
			return pFollowEntity->GetClientFrame( nTick );
	}

	return GetClientFrame( nTick );
}

void CGameClient::WriteViewAngleUpdate()
{
//
// send the current viewpos offset from the view entity
//
// a fixangle might get lost in a dropped packet.  Oh well.

	if ( IsFakeClient() )
		return;

	Assert( serverGameClients );
	CPlayerState *pl = serverGameClients->GetPlayerState( edict );
	Assert( pl );

	if ( pl && pl->fixangle != FIXANGLE_NONE	 )
	{
		if ( pl->fixangle == FIXANGLE_RELATIVE		 )
		{
			SVC_FixAngle fixAngle( true, pl->anglechange );
			m_NetChannel->SendNetMsg( fixAngle );
			pl->anglechange.Init(); // clear
		}
		else
		{
			SVC_FixAngle fixAngle(false, pl->v_angle );
			m_NetChannel->SendNetMsg( fixAngle );
		}
		
		pl->fixangle = FIXANGLE_NONE;
	}
}

/*
===================
SV_ValidateClientCommand

Determine if passed in user command is valid.
===================
*/
bool CGameClient::IsEngineClientCommand( const CCommand &args ) const
{
	if ( args.ArgC() == 0 )
		return false;

	for ( int i = 0; s_clcommands[i] != NULL; ++i )
	{
		if ( !Q_strcasecmp( args[0], s_clcommands[i] ) )
			return true;
	}

	return false;
}

bool CGameClient::SendNetMsg(INetMessage &msg, bool bForceReliable)
{
#ifndef _XBOX
	if ( m_bIsHLTV )
	{
		// pass this message to HLTV
		return hltv->SendNetMsg( msg, bForceReliable );
	}
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		// pass this message to replay
		return replay->SendNetMsg( msg, bForceReliable );
	}
#endif
#endif
	return CBaseClient::SendNetMsg( msg, bForceReliable);
}

bool CGameClient::ExecuteStringCommand( const char *pCommandString )
{
	// first let the baseclass handle it
	if ( CBaseClient::ExecuteStringCommand( pCommandString ) )
		return true;
	
	// Determine whether the command is appropriate
	CCommand args;
	if ( !args.Tokenize( pCommandString ) )
		return false;

	if ( args.ArgC() == 0 )
		return false;

	if ( IsEngineClientCommand( args ) )
	{
		Cmd_ExecuteCommand( args, src_client, m_nClientSlot );
		return true;
	}
	
	const ConCommandBase *pCommand = g_pCVar->FindCommandBase( args[ 0 ] );
	if ( pCommand && pCommand->IsCommand() && pCommand->IsFlagSet( FCVAR_GAMEDLL ) )
	{
		// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
		// NOTE: Don't bother with rpt stuff; commands that matter there shouldn't have FCVAR_GAMEDLL set
		if ( pCommand->IsFlagSet( FCVAR_CHEAT ) )
		{
			if ( sv.IsMultiplayer() && !CanCheat() )
				return false;
		}

		if ( pCommand->IsFlagSet( FCVAR_SPONLY ) )
		{
			if ( sv.IsMultiplayer() )
			{
				return false;
			}
		}

		// Don't allow clients to execute commands marked as development only.
		if ( pCommand->IsFlagSet( FCVAR_DEVELOPMENTONLY ) )
		{
			return false;
		}

		g_pServerPluginHandler->SetCommandClient( m_nClientSlot );
		Cmd_Dispatch( pCommand, args );
	}
	else
	{
		g_pServerPluginHandler->ClientCommand( edict, args ); // TODO pass client id and string
	}

	return true;
}

void CGameClient::SendSnapshot( CClientFrame * pFrame )
{
	if ( m_bIsHLTV )
	{
#ifndef SHARED_NET_STRING_TABLES
		// copy string updates from server to hltv stringtable
		networkStringTableContainerServer->DirectUpdate( GetMaxAckTickCount() );
#endif
		char *buf = (char *)_alloca( NET_MAX_PAYLOAD );

		// pack sounds to one message
		if ( m_Sounds.Count() > 0 )
		{
			SVC_Sounds sounds;
			sounds.m_DataOut.StartWriting( buf, NET_MAX_PAYLOAD );

			FillSoundsMessage( sounds );
			hltv->SendNetMsg( sounds );
		}

		int maxEnts = tv_transmitall.GetBool()?255:64;
		hltv->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), *hltv->GetBuffer( HLTV_BUFFER_TEMPENTS ), maxEnts );

		// add snapshot to HLTV server frame list
		hltv->AddNewFrame( pFrame );

		// remember this snapshot
		m_pLastSnapshot = pFrame->GetSnapshot(); 

		// fake acknowledgement, remove ClientFrame reference immediately 
		UpdateAcknowledgedFramecount( pFrame->tick_count );
		
		return;
	}

#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
#ifndef SHARED_NET_STRING_TABLES
		// copy string updates from server to replay stringtable
		networkStringTableContainerServer->DirectUpdate( GetMaxAckTickCount() );
#endif
		char *buf = (char *)_alloca( NET_MAX_PAYLOAD );

		// pack sounds to one message
		if ( m_Sounds.Count() > 0 )
		{
			SVC_Sounds sounds;
			sounds.m_DataOut.StartWriting( buf, NET_MAX_PAYLOAD );

			FillSoundsMessage( sounds );
			replay->SendNetMsg( sounds );
		}

		int maxEnts = 255;
		replay->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), *replay->GetBuffer( REPLAY_BUFFER_TEMPENTS ), maxEnts );

		// add snapshot to Replay server frame list
		if ( replay->AddNewFrame( pFrame ) )
		{
			// remember this snapshot
			m_pLastSnapshot = pFrame->GetSnapshot(); 

			// fake acknowledgement, remove ClientFrame reference immediately 
			UpdateAcknowledgedFramecount( pFrame->tick_count );
		}

		return;
	}
#endif

	// update client viewangles update
	WriteViewAngleUpdate();

	CBaseClient::SendSnapshot( pFrame );
}

//-----------------------------------------------------------------------------
// This function contains all the logic to determine if we should send a datagram
// to a particular client
//-----------------------------------------------------------------------------

bool CGameClient::ShouldSendMessages( void )
{
#ifndef _XBOX
	if ( m_bIsHLTV )
	{
		// calc snapshot interval
		int nSnapshotInterval = 1.0f / ( m_Server->GetTickInterval() * tv_snapshotrate.GetFloat() );

		// I am the HLTV client, record every nSnapshotInterval tick
		return ( sv.m_nTickCount >= (hltv->m_nLastTick + nSnapshotInterval) );
	}
	
#if defined( REPLAY_ENABLED )
	if ( m_bIsReplay )
	{
		const float replay_snapshotrate = 16.0f;

		// calc snapshot interval
		int nSnapshotInterval = 1.0f / ( m_Server->GetTickInterval() * replay_snapshotrate );

		// I am the Replay client, record every nSnapshotInterval tick
		return ( sv.m_nTickCount >= (replay->m_nLastTick + nSnapshotInterval) );
	}
#endif
#endif
	// If sv_stressbots is true, then treat a bot more like a regular client and do deltas and such for it.
	if( IsFakeClient() )
	{
		if ( !sv_stressbots.GetBool() )
			return false;
	}

	return CBaseClient::ShouldSendMessages();
}

void CGameClient::FileReceived( const char *fileName, unsigned int transferID )
{
	//check if file is one of our requested custom files
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( m_nCustomFiles[i].reqID == transferID )
		{
			m_nFilesDownloaded++;

			// broadcast update to other clients so they start downlaoding this file
			m_Server->UserInfoChanged( m_nClientSlot );
			return;
		}
	}

	Msg( "CGameClient::FileReceived: %s not wanted.\n", fileName );
}

void CGameClient::FileRequested(const char *fileName, unsigned int transferID )
{
	DevMsg( "File '%s' requested from client %s.\n", fileName, m_NetChannel->GetAddress() );

	if ( sv_allowdownload.GetBool() )
	{
		m_NetChannel->SendFile( fileName, transferID );
	}
	else
	{
		m_NetChannel->DenyFile( fileName, transferID );
	}
}

void CGameClient::FileDenied(const char *fileName, unsigned int transferID )
{
	ConMsg( "Downloading file '%s' from client %s failed.\n", fileName, GetClientName() );
}

void CGameClient::FileSent( const char *fileName, unsigned int transferID )
{
	ConMsg( "Sent file '%s' to client %s.\n", fileName, GetClientName() );
}

void CGameClient::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	// make sure m_LastMovementTick != sv.tickcount
	m_LastMovementTick = ( sv.m_nTickCount - 1 );

	host_client = this;

	// During connection, only respond if client sends a packet
	m_bReceivedPacket = true; 
}

void CGameClient::PacketEnd()
{
	// Fix up clock in case prediction/etc. code reset it.
	g_ServerGlobalVariables.frametime = host_state.interval_per_tick;
}

void CGameClient::ConnectionClosing(const char *reason)
{
#ifndef _XBOX
	SV_RedirectEnd ();
#endif
	// Check for printf format tokens in this reason string. Crash exploit.
	Disconnect ( (reason && !strchr( reason, '%' ) ) ? reason : "Connection closing" );	
}

void CGameClient::ConnectionCrashed(const char *reason)
{
	if ( m_Name[0] && IsConnected() )
	{
		DebuggerBreakIfDebugging_StagingOnly();

#ifndef _XBOX
		SV_RedirectEnd ();
#endif
		// Check for printf format tokens in this reason string. Crash exploit.
		Disconnect ( (reason && !strchr( reason, '%' ) ) ? reason : "Connection lost" );	
	}
}

CClientFrame *CGameClient::GetSendFrame()
{
	CClientFrame *pFrame = m_pCurrentFrame;

	// just return if replay is disabled
	if ( sv_maxreplay.GetFloat() <= 0 )
		return pFrame;
			
	int followEntity;

	int delayTicks = serverGameClients->GetReplayDelay( edict, followEntity );

	bool isInReplayMode = ( delayTicks > 0 );

	if ( isInReplayMode != m_bIsInReplayMode )
	{
		// force a full update when modes are switched
		m_nDeltaTick = -1; 

		m_bIsInReplayMode = isInReplayMode;

		if ( isInReplayMode )
		{
			m_nEntityIndex = followEntity;
		}
		else
		{
			m_nEntityIndex = m_nClientSlot+1;
		}
	}

	Assert( (m_nClientSlot+1 == m_nEntityIndex) || isInReplayMode );

	if ( isInReplayMode )
	{
		CGameClient *pFollowPlayer = sv.Client( followEntity-1 );

		if ( !pFollowPlayer )
			return NULL;

		pFrame = pFollowPlayer->GetClientFrame( sv.GetTick() - delayTicks, false );

		if ( !pFrame )
			return NULL;

		if ( m_pLastSnapshot == pFrame->GetSnapshot() )
			return NULL;
	}

	return pFrame;
}

bool CGameClient::IgnoreTempEntity( CEventInfo *event )
{
	// in replay mode replay all temp entities
	if ( m_bIsInReplayMode )
		return false;

	return CBaseClient::IgnoreTempEntity( event );
}


const CCheckTransmitInfo* CGameClient::GetPrevPackInfo()
{
	return &m_PrevPackInfo;
}

// This code is useful for verifying that the networking of soundinfo_t stuff isn't borked.
#if 0  

#include "vstdlib/random.h"

class CTestSoundInfoNetworking
{
public:

	CTestSoundInfoNetworking();

	void RunTest();

private:

	void CreateRandomSounds( int nCount );
	void CreateRandomSound( SoundInfo_t &si );
	void Compare( const SoundInfo_t &s1, const SoundInfo_t &s2 );

	CUtlVector< SoundInfo_t >	m_Sounds;

	CUtlVector< SoundInfo_t >	m_Received;
};

static CTestSoundInfoNetworking g_SoundTest;

CON_COMMAND( st, "sound test" )
{
	int nCount = 1;
	if ( args.ArgC() >= 2 )
	{
		nCount = clamp( Q_atoi( args.Arg( 1 ) ), 1, 100000 );
	}

	for ( int i = 0 ; i < nCount; ++i )
	{
		if ( !( i % 100 ) && i > 0 )
		{
			Msg( "Running test %d %f %% done\n",
				i, 100.0f * (float)i/(float)nCount );
		}
		g_SoundTest.RunTest();
	}
}
CTestSoundInfoNetworking::CTestSoundInfoNetworking()
{
}

void CTestSoundInfoNetworking::CreateRandomSound( SoundInfo_t &si )
{
	int entindex = RandomInt( 0, MAX_EDICTS - 1 );
	int channel = RandomInt( 0, 7 );
	int soundnum = RandomInt( 0, MAX_SOUNDS - 1 );
	Vector org = RandomVector( -16383, 16383 );
	Vector dir = RandomVector( -1.0f, 1.0f );
	float flVolume = RandomFloat( 0.1f, 1.0f );
	bool bLooping = RandomInt( 0, 100 ) < 5;
	int nPitch = RandomInt( 0, 100 ) < 5 ? RandomInt( 95, 105 ) : 100;
	Vector lo = RandomInt( 0, 100 ) < 5 ? RandomVector( -16383, 16383 ) : org;
	int speaker = RandomInt( 0, 100 ) < 2 ? RandomInt( 0, MAX_EDICTS - 1 ) : -1;
	soundlevel_t level = soundlevel_t(RandomInt( 70, 150 ));

	si.Set( entindex, channel, "foo.wav", org, dir, flVolume, level, bLooping, nPitch, lo, speaker );

	si.nFlags = ( 1 << RandomInt( 0, 6 ) );
	si.nSoundNum = soundnum;
	si.bIsSentence = RandomInt( 0, 1 );
	si.bIsAmbient = RandomInt( 0, 1 );
	si.fDelay = RandomInt( 0, 100 ) < 2 ? RandomFloat( -0.1, 0.1f ) : 0.0f;
}

void CTestSoundInfoNetworking::CreateRandomSounds( int nCount )
{
	m_Sounds.Purge();
	m_Sounds.EnsureCount( nCount );

	for ( int i = 0; i < nCount; ++i )
	{
		SoundInfo_t &si = m_Sounds[ i ];
		CreateRandomSound( si );
	}
}

void CTestSoundInfoNetworking::RunTest()
{
	int m_nSoundSequence = 0;

	CreateRandomSounds( 512 );

	SoundInfo_t defaultSound; defaultSound.SetDefault();
	SoundInfo_t *pDeltaSound = &defaultSound;

	SVC_Sounds	msg;

	char *buf = (char *)_alloca( NET_MAX_PAYLOAD );

	msg.m_DataOut.StartWriting( buf, NET_MAX_PAYLOAD );

	msg.m_nNumSounds = m_Sounds.Count();
	msg.m_bReliableSound = false;
	msg.SetReliable( false );

	Assert( msg.m_DataOut.GetNumBitsLeft() > 0 );

	for ( int i = 0 ; i < m_Sounds.Count(); i++ )
	{
		SoundInfo_t &sound = m_Sounds[ i ];
		sound.WriteDelta( pDeltaSound, msg.m_DataOut );
		pDeltaSound = &m_Sounds[ i ];
	}

	// Now read them out
	defaultSound.SetDefault();
	pDeltaSound = &defaultSound;

	msg.m_DataIn.StartReading( buf, msg.m_DataOut.GetNumBytesWritten(), 0, msg.m_DataOut.GetNumBitsWritten() );

	SoundInfo_t sound;

	for ( int i=0; i<msg.m_nNumSounds; i++ )
	{
		sound.ReadDelta( pDeltaSound, msg.m_DataIn );

		pDeltaSound = &sound;	// copy delta values

		if ( msg.m_bReliableSound )
		{
			// client is incrementing the reliable sequence numbers itself
			m_nSoundSequence = ( m_nSoundSequence + 1 ) & SOUND_SEQNUMBER_MASK;

			Assert ( sound.nSequenceNumber == 0 );

			sound.nSequenceNumber = m_nSoundSequence;
		}

		// Add no ambient sounds to sorted queue, will be processed after packet has been completly parsed
		// CL_AddSound( sound );
		m_Received.AddToTail( sound );
	}

	
	// Now validate them
	for ( int i = 0 ; i < msg.m_nNumSounds; ++i )
	{
		SoundInfo_t &server = m_Sounds[ i ];
		SoundInfo_t &client = m_Received[ i ];

		Compare( server, client );
	}

	m_Sounds.Purge();
	m_Received.Purge();
}

void CTestSoundInfoNetworking::Compare( const SoundInfo_t &s1, const SoundInfo_t &s2 )
{
	bool bSndStop = s2.nFlags == SND_STOP;
	
	if ( !bSndStop && s1.nSequenceNumber != s2.nSequenceNumber )
	{
		Msg( "seq number mismatch %d %d\n", s1.nSequenceNumber, s2.nSequenceNumber );
	}


	if ( s1.nEntityIndex != s2.nEntityIndex )
	{
		Msg( "ent mismatch %d %d\n", s1.nEntityIndex, s2.nEntityIndex );
	}

	if ( s1.nChannel != s2.nChannel )
	{
		Msg( "channel mismatch %d %d\n", s1.nChannel, s2.nChannel );
	}

	Vector d;

	d = s1.vOrigin - s2.vOrigin;

	if ( !bSndStop && d.Length() > 32.0f )
	{
		Msg( "origin mismatch [%f] (%f %f %f) != (%f %f %f)\n", d.Length(), s1.vOrigin.x, s1.vOrigin.y, s1.vOrigin.z, s2.vOrigin.x, s2.vOrigin.y, s2.vOrigin.z );
	}

	// Vector			vDirection;
	float delta = fabs( s1.fVolume - s2.fVolume );

	if ( !bSndStop && delta > 1.0f )
	{
		Msg( "vol mismatch %f %f\n", s1.fVolume, s2.fVolume );
	}


	if ( !bSndStop && s1.Soundlevel != s2.Soundlevel )
	{
		Msg( "sndlvl mismatch %d %d\n", s1.Soundlevel, s2.Soundlevel );
	}

	// bLooping; 

	if ( s1.bIsSentence != s2.bIsSentence )
	{
		Msg( "sentence mismatch %d %d\n", s1.bIsSentence ? 1 : 0, s2.bIsSentence ? 1 : 0 );
	}
	if ( s1.bIsAmbient != s2.bIsAmbient )
	{
		Msg( "ambient mismatch %d %d\n", s1.bIsAmbient ? 1 : 0, s2.bIsAmbient ? 1 : 0 );
	}

	if ( !bSndStop && s1.nPitch != s2.nPitch )
	{
		Msg( "pitch mismatch %d %d\n", s1.nPitch, s2.nPitch );
	}

	if ( !bSndStop && s1.nSpecialDSP != s2.nSpecialDSP )
	{
		Msg( "special dsp mismatch %d %d\n", s1.nSpecialDSP, s2.nSpecialDSP );
	}

	// Vector			vListenerOrigin;

	if ( s1.nFlags != s2.nFlags )
	{
		Msg( "flags mismatch %d %d\n", s1.nFlags, s2.nFlags );
	}

	if ( s1.nSoundNum != s2.nSoundNum )
	{
		Msg( "soundnum mismatch %d %d\n", s1.nSoundNum, s2.nSoundNum );
	}

	delta = fabs( s1.fDelay - s2.fDelay );

	if ( !bSndStop && delta > 0.020f )
	{
		Msg( "delay mismatch %f %f\n", s1.fDelay, s2.fDelay );
	}


	if ( !bSndStop && s1.nSpeakerEntity != s2.nSpeakerEntity )
	{
		Msg( "speakerentity mismatch %d %d\n", s1.nSpeakerEntity, s2.nSpeakerEntity );
	}
}

#endif
