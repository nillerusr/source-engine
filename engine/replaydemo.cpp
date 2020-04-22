//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if defined( REPLAY_ENABLED )

#include <tier1/strtools.h>
#include <eiface.h>
#include <bitbuf.h>
#include <time.h>
#include "replaydemo.h"
#include "replayserver.h"
#include "demo.h"
#include "host_cmd.h"
#include "proto_version.h"
#include "demofile/demoformat.h"
#include "filesystem_engine.h"
#include "net.h"
#include "networkstringtable.h"
#include "dt_common_eng.h"
#include "host.h"
#include "server.h"
#include "networkstringtableclient.h"
#include "replay_internal.h"
#include "GameEventManager.h"
#include "replay/ireplaysystem.h"
#include "replay/ireplaysessionrecorder.h"
#include "replay/shared_defs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CNetworkStringTableContainer *networkStringTableContainerServer;
extern CGlobalVars g_ServerGlobalVariables;
extern IServerReplayContext *g_pServerReplayContext;

static ConVar *replay_record_voice = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplayDemoRecorder::CReplayDemoRecorder( CReplayServer* pServer )
{
	m_bIsRecording = false;

	Assert( pServer );
	m_pReplayServer = pServer;

	m_nStartTick = -1;
}

CReplayDemoRecorder::~CReplayDemoRecorder()
{
	StopRecording();
}

void CReplayDemoRecorder::GetUniqueDemoFilename( char *pOut, int nLength )
{
	Assert( pOut );
	tm today; VCRHook_LocalTime( &today );
	Q_snprintf( pOut, nLength, "%04i%02i%02i-%02i%02i%02i-%s.dem", 
		1900 + today.tm_year, today.tm_mon+1, today.tm_mday, 
		today.tm_hour, today.tm_min, today.tm_sec, m_pReplayServer->GetMapName() ); 
}

void CReplayDemoRecorder::StartRecording() 
{
	// Get a proper filename and cache it for later
	GetUniqueDemoFilename( m_szDumpFilename, sizeof( m_szDumpFilename ) );

	// Start recording to the temporary location in the game dir
	StartRecording( TMP_REPLAY_FILENAME, false );
}

const char *CReplayDemoRecorder::GetDemoFilename()
{
	static char s_szDemoFilename[ MAX_OSPATH ];
	const char *pFilename = replay->m_DemoRecorder.GetRecordingFilename();		Assert( pFilename && pFilename[0] );
	V_strcpy( s_szDemoFilename, pFilename );
	return s_szDemoFilename;
}

void CReplayDemoRecorder::StartRecording( const char *pFilename, bool bContinuously ) 
{
	SETUP_CVAR_REF( replay_recording );

	StopRecording();	// stop if we're already recording

	// Attempt to "open" the demo file
	ConVarRef replay_buffersize( "replay_buffersize" );
	const int nBufferSize = 1024 * 1024 * ( replay_buffersize.IsValid() ? replay_buffersize.GetInt() : 16 );
	if ( !m_DemoFile.Open( NULL, false, true, nBufferSize, false ) )
	{
		Warning( "Failed to start recording - couldn't open demo file %s.\n", pFilename );
		return;
	}
	
	// Using this tickcount allows us to sync up client-side recorded ragdolls later with replay demos on clients
	m_nStartTick = g_ServerGlobalVariables.tickcount;

	demoheader_t *dh = &m_DemoFile.m_DemoHeader;

	// open demo header file containing sigon data
	Q_memset( dh, 0, sizeof(demoheader_t) );

	Q_strncpy( dh->demofilestamp, DEMO_HEADER_ID, sizeof(dh->demofilestamp) );
	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = PROTOCOL_VERSION;

	Q_strncpy( dh->mapname, m_pReplayServer->GetMapName(), sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	Q_strncpy(szGameDir, com_gamedir, sizeof( szGameDir ) );
	Q_FileBase ( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	Q_strncpy( dh->servername, host_name.GetString(), sizeof( dh->servername ) );

	Q_strncpy( dh->clientname, "Replay Demo", sizeof( dh->servername ) );

	// write demo file header info
	m_DemoFile.WriteDemoHeader();
	
	dh->signonlength = WriteSignonData(); // demoheader will be written when demo is closed

	m_nFrameCount = 0;

	// Demo playback should read this as an incoming message.
	// Write the client's realtime value out so we can synchronize the reads.
	m_DemoFile.WriteCmdHeader( dem_synctick, 0 );

	m_bIsRecording = true;

	m_SequenceInfo = 1;
	m_nDeltaTick = -1;

	replay_recording.SetValue( 1 );

	extern ConVar replay_debug;
	if ( replay_debug.GetBool() ) ConMsg( "%f: Recording Replay...\n", host_time );

	g_pServerReplayContext->GetSessionRecorder()->SetCurrentRecordingStartTick( m_nStartTick );
}

bool CReplayDemoRecorder::IsRecording()
{
	return m_bIsRecording;
}

void CReplayDemoRecorder::StopRecording()
{
	if ( !IsRecording() )
		return;

	// Wipe the demo (does not write to disk)
	m_DemoFile.Close();

	// Set recording flag
	m_bIsRecording = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0 );
	}
	
	// replay_stoprecording gets set to 0 from within the replay session recorder, but only if we aren't starting to record a new round
}

CDemoFile *CReplayDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}

int CReplayDemoRecorder::GetRecordingTick()
{
	return g_ServerGlobalVariables.tickcount - m_nStartTick;
}

void CReplayDemoRecorder::WriteServerInfo()
{
	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CReplayDemoRecorder::WriteServerInfo", buffer, sizeof( buffer ) );

	SVC_ServerInfo serverinfo;	// create serverinfo message

	// on the master demos are using sv object, on relays replay
	CBaseServer *pServer = (CBaseServer*)&sv;
	
	m_pReplayServer->FillServerInfo( serverinfo ); // fill rest of info message
	
	serverinfo.WriteToBuffer( msg );

	// send first tick
	NET_Tick signonTick( m_nSignonTick, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// Write replicated ConVars to non-listen server clients only
	NET_SetConVar convars;
	// build a list of all replicated convars
	Host_BuildConVarUpdateMessage( &convars, FCVAR_REPLICATED, true );

	// write convars to demo
	convars.WriteToBuffer( msg );

	// write stringtable baselines
#ifndef SHARED_NET_STRING_TABLES
	m_pReplayServer->m_StringTables->WriteBaselines( msg );
#endif

	// send signon state
	NET_SignonState signonMsg( SIGNONSTATE_NEW, pServer->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg );
}

void CReplayDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	GET_REPLAY_DBG_REF();
	if ( replay_debug.GetBool() ) Msg( "recording command, \"%s\"\n", cmdstring );

	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick() );
}

void CReplayDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
	MEM_ALLOC_CREDIT();

	char *pBigBuffer;
	CUtlBuffer bigBuff;

	int buffSize = 256*1024;
	if ( !IsX360() )
	{
		pBigBuffer = (char*)stackalloc( buffSize );
	}
	else
	{
		// keep temp large allocations off of stack
		bigBuff.EnsureCapacity( buffSize );
		pBigBuffer = (char*)bigBuff.Base();
	}

	bf_write buf( pBigBuffer, buffSize );

	// Send SendTable info.
	DataTable_WriteSendTablesBuffer( pClasses, &buf );

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer( pClasses, &buf );

	// Now write the buffer into the demo file
	m_DemoFile.WriteNetworkDataTables( &buf, GetRecordingTick() );
}

void CReplayDemoRecorder::RecordStringTables()
{
	// !KLUDGE! It would be nice if the bit buffer could write into a stream
	// with the power to grow itself.  But it can't.  Hence this really bad
	// kludge
	void *data = NULL;
	int dataLen = 512 * 1024;
	while ( dataLen <= DEMO_FILE_MAX_STRINGTABLE_SIZE )
	{
		data = realloc( data, dataLen );
		bf_write buf( data, dataLen );
		buf.SetDebugName("CReplayDemoRecorder::RecordStringTables");
		buf.SetAssertOnOverflow( false ); // Doesn't turn off all the spew / asserts, but turns off one
		networkStringTableContainerServer->WriteStringTables( buf );

		// Did we fit?
		if ( !buf.IsOverflowed() )
		{

			// Now write the buffer into the demo file
			m_DemoFile.WriteStringTables( &buf, GetRecordingTick() );
			break;
		}

		// Didn't fit.  Try doubling the size of the buffer
		dataLen *= 2;
	}
	
	if ( dataLen > DEMO_FILE_MAX_STRINGTABLE_SIZE )
	{
		Warning( "Failed to RecordStringTables. Trying to record string table that's bigger than max string table size\n" );
	}

	free(data);
}

int CReplayDemoRecorder::WriteSignonData()
{
	int start = m_DemoFile.GetCurPos( false );

	// on the master demos are using sv object, on relays replay
	CBaseServer *pServer = (CBaseServer*)&sv;

	m_nSignonTick = pServer->m_nTickCount;		

	WriteServerInfo();

	RecordServerClasses( serverGameDLL->GetAllServerClasses() );
	RecordStringTables();

	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CReplayDemo::WriteSignonData", buffer, sizeof( buffer ) );

	// use your class infos, CRC is correct
	SVC_ClassInfo classmsg( true, pServer->serverclasses );
	classmsg.WriteToBuffer( msg );

	// Write the regular signon now
	msg.WriteBits( m_pReplayServer->m_Signon.GetData(), m_pReplayServer->m_Signon.GetNumBitsWritten() );

	// write new state
	NET_SignonState signonMsg1( SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount() );
	signonMsg1.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	msg.Reset();

	// set view entity
	SVC_SetView viewent( m_pReplayServer->m_nViewEntity );
	viewent.WriteToBuffer( msg );

	// Spawned into server, not fully active, though
	NET_SignonState signonMsg2( SIGNONSTATE_SPAWN, pServer->GetSpawnCount() );
	signonMsg2.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	
	return m_DemoFile.GetCurPos( false ) - start;
}


void CReplayDemoRecorder::WriteFrame( CReplayFrame *pFrame )
{
	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CReplayDemo::RecordFrame", buffer, sizeof( buffer ) );

	//first write reliable data
	bf_write *data = &pFrame->m_Messages[REPLAY_BUFFER_RELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	//now send snapshot data

	// send tick time
	NET_Tick tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );
	tickmsg.WriteToBuffer( msg );


#ifndef SHARED_NET_STRING_TABLES
	// Update shared client/server string tables. Must be done before sending entities
	sv.m_StringTables->WriteUpdateMessage( NULL, MAX( m_nSignonTick, m_nDeltaTick ), msg );
#endif

	// get delta frame
	CClientFrame *deltaFrame = m_pReplayServer->GetClientFrame( m_nDeltaTick ); // NULL if m_nDeltaTick is not found or -1

	// send entity update, delta compressed if deltaFrame != NULL
	sv.WriteDeltaEntities( m_pReplayServer->m_MasterClient, pFrame, deltaFrame, msg );

	// send all unreliable temp ents between last and current frame
	CFrameSnapshot * fromSnapshot = deltaFrame?deltaFrame->GetSnapshot():NULL;
	sv.WriteTempEntities( m_pReplayServer->m_MasterClient, pFrame->GetSnapshot(), fromSnapshot, msg, 255 );

	// write sound data
	data = &pFrame->m_Messages[REPLAY_BUFFER_SOUNDS];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// write voice data
	if ( replay_record_voice == NULL )
	{
		replay_record_voice = g_pCVar->FindVar( "replay_record_voice" );
		Assert( replay_record_voice != NULL );
	}

	if ( replay_record_voice && replay_record_voice->GetBool() )
	{
		data = &pFrame->m_Messages[REPLAY_BUFFER_VOICE];
		if ( data->GetNumBitsWritten() )
			msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );
	}

	// last write unreliable data
	data = &pFrame->m_Messages[REPLAY_BUFFER_UNRELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// update delta tick just like fake clients do
	m_nDeltaTick = pFrame->tick_count;

	// write packet to demo file
	WriteMessages( dem_packet, msg ); 
}

void CReplayDemoRecorder::WriteMessages( unsigned char cmd, bf_write &message )
{
	int len = message.GetNumBytesWritten();

	if (len <= 0)
		return;

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = message.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		message.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	Assert( len < NET_MAX_MESSAGE );

	// if signondata read as fast as possible, no rewind
	// and wait for packet time
	// byte cmd = (m_pDemoFileHeader != NULL)  ? dem_signon : dem_packet;

	if ( cmd == dem_packet )
	{
		m_nFrameCount++;
	}

	// write command & time
	m_DemoFile.WriteCmdHeader( cmd, GetRecordingTick() ); 
	
	// write NULL democmdinfo just to keep same format as client demos
	democmdinfo_t info;
	Q_memset( &info, 0, sizeof( info ) );
	m_DemoFile.WriteCmdInfo( info );

	// write continously increasing sequence numbers
	m_DemoFile.WriteSequenceInfo( m_SequenceInfo, m_SequenceInfo );
	m_SequenceInfo++;
	
	// Output the buffer.  Skip the network packet stuff.
	m_DemoFile.WriteRawData( (char*)message.GetBasePointer(), len );
}

void CReplayDemoRecorder::RecordMessages(bf_read &data, int bits)
{
	// create buffer if not there yet
	if ( m_MessageData.GetBasePointer() == NULL )
	{
		m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	}
	
	if ( bits>0 )
	{
		m_MessageData.WriteBitsFromBuffer( &data, bits );
		Assert( !m_MessageData.IsOverflowed() );
	}
}

void CReplayDemoRecorder::RecordPacket()
{
	Assert( !"Does this ever get called?  I can't find anywhere where it does." );
	if( m_MessageData.GetBasePointer() )
	{
		WriteMessages( dem_packet, m_MessageData );
		m_MessageData.Reset(); // clear message buffer
	}
}

const char *CReplayDemoRecorder::GetRecordingFilename()
{
	AssertMsg( 0, "Do we ever call this? " );
	if ( !IsRecording() )
	{
		Assert( 0 );
		return NULL;
	}

	return m_szDumpFilename;
}

#endif
