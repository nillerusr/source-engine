//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include <tier1/strtools.h>
#include <eiface.h>
#include <bitbuf.h>
#include <time.h>
#include "hltvdemo.h"
#include "hltvserver.h"
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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CNetworkStringTableContainer *networkStringTableContainerServer;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVDemoRecorder::CHLTVDemoRecorder()
{
	m_bIsRecording = false;
}

CHLTVDemoRecorder::~CHLTVDemoRecorder()
{
	StopRecording();
}

void CHLTVDemoRecorder::StartAutoRecording() 
{
	char fileName[MAX_OSPATH];

	tm today; VCRHook_LocalTime( &today );

	Q_snprintf( fileName, sizeof(fileName), "auto-%04i%02i%02i-%02i%02i-%s.dem", 
		1900 + today.tm_year, today.tm_mon+1, today.tm_mday, 
		today.tm_hour, today.tm_min, hltv->GetMapName() ); 

	StartRecording( fileName, false );
}


void CHLTVDemoRecorder::StartRecording( const char *filename, bool bContinuously ) 
{
	StopRecording();	// stop if we're already recording
	
	if ( !m_DemoFile.Open( filename, false ) )
	{
		ConMsg ("StartRecording: couldn't open demo file %s.\n", filename );
		return;
	}

	ConMsg ("Recording SourceTV demo to %s...\n", filename);

	demoheader_t *dh = &m_DemoFile.m_DemoHeader;

	// open demo header file containing sigondata
	Q_memset( dh, 0, sizeof(demoheader_t));

	Q_strncpy( dh->demofilestamp, DEMO_HEADER_ID, sizeof(dh->demofilestamp) );
	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = PROTOCOL_VERSION;

	Q_strncpy( dh->mapname, hltv->GetMapName(), sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	Q_strncpy(szGameDir, com_gamedir, sizeof( szGameDir ) );
	Q_FileBase ( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	Q_strncpy( dh->servername, host_name.GetString(), sizeof( dh->servername ) );

	Q_strncpy( dh->clientname, "SourceTV Demo", sizeof( dh->servername ) );

	// write demo file header info
	m_DemoFile.WriteDemoHeader();
	
	dh->signonlength = WriteSignonData(); // demoheader will be written when demo is closed

	m_nFrameCount = 0;

	m_nStartTick = host_tickcount;

	// Demo playback should read this as an incoming message.
	// Write the client's realtime value out so we can synchronize the reads.
	m_DemoFile.WriteCmdHeader( dem_synctick, 0 );

	m_bIsRecording = true;

	m_SequenceInfo = 1;
	m_nDeltaTick = -1;
}

bool CHLTVDemoRecorder::IsRecording()
{
	return m_bIsRecording;
}

void CHLTVDemoRecorder::StopRecording()
{
	if ( !m_bIsRecording )
		return;

	// Demo playback should read this as an incoming message.
	m_DemoFile.WriteCmdHeader( dem_stop, GetRecordingTick() );

	// update demo header info
	m_DemoFile.m_DemoHeader.playback_ticks = GetRecordingTick();
	m_DemoFile.m_DemoHeader.playback_time =  host_state.interval_per_tick *	GetRecordingTick();
	m_DemoFile.m_DemoHeader.playback_frames = m_nFrameCount;

	// write updated version
	m_DemoFile.WriteDemoHeader();

	m_DemoFile.Close();

	m_bIsRecording = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0 );
	}
	
	ConMsg("Completed SourceTV demo \"%s\", recording time %.1f\n",
		m_DemoFile.m_szFileName,
		m_DemoFile.m_DemoHeader.playback_time );
}

CDemoFile *CHLTVDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}


int CHLTVDemoRecorder::GetRecordingTick( void )
{
	return host_tickcount - m_nStartTick;
}



void CHLTVDemoRecorder::WriteServerInfo()
{
	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CHLTVDemoRecorder::WriteServerInfo", buffer, sizeof( buffer ) );

	SVC_ServerInfo serverinfo;	// create serverinfo message

	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = hltv->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(hltv);
	
	hltv->FillServerInfo( serverinfo ); // fill rest of info message
	
	serverinfo.WriteToBuffer( msg );

	// send first tick
	NET_Tick signonTick( m_nSignonTick, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// write stringtable baselines

#ifndef SHARED_NET_STRING_TABLES
	pServer->m_StringTables->WriteBaselines( msg );
#endif
	
	// Write replicated ConVars to non-listen server clients only
	NET_SetConVar convars;

	// build a list of all replicated convars
	Host_BuildConVarUpdateMessage( &convars, FCVAR_REPLICATED, true );

	if ( hltv->IsMasterProxy() )
	{
		// for SourceTV server demos write set "tv_transmitall 1" even
		// if it's off for the real broadcast
		NET_SetConVar::cvar_t acvar;
		Q_strncpy( acvar.name, "tv_transmitall", MAX_OSPATH );
		Q_strncpy( acvar.value, "1", MAX_OSPATH );
		convars.m_ConVars.AddToTail( acvar );
	}

	// write convars to demo
	convars.WriteToBuffer( msg );
	
	// send signon state
	NET_SignonState signonMsg( SIGNONSTATE_NEW, pServer->GetSpawnCount() );
	signonMsg.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg );
}

void CHLTVDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick() );
}

void CHLTVDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
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

void CHLTVDemoRecorder::RecordStringTables()
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
		buf.SetDebugName("CHLTVDemoRecorder_StringTables");
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

int CHLTVDemoRecorder::WriteSignonData()
{
	int start = m_DemoFile.GetCurPos( false );

	// on the master demos are using sv object, on relays hltv
	CBaseServer *pServer = hltv->IsMasterProxy()?(CBaseServer*)(&sv):(CBaseServer*)(hltv);

	m_nSignonTick = pServer->m_nTickCount;		

	WriteServerInfo();

	RecordServerClasses( serverGameDLL->GetAllServerClasses() );
	RecordStringTables();

	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CHLTVDemo::WriteSignonData", buffer, sizeof( buffer ) );

	// use your class infos, CRC is correct
	SVC_ClassInfo classmsg( true, pServer->serverclasses );
	classmsg.WriteToBuffer( msg );

	// Write the regular signon now
	msg.WriteBits( hltv->m_Signon.GetData(), hltv->m_Signon.GetNumBitsWritten() );

	// write new state
	NET_SignonState signonMsg1( SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount() );
	signonMsg1.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	msg.Reset();

	// set view entity
	SVC_SetView viewent( hltv->m_nViewEntity );
	viewent.WriteToBuffer( msg );

	// Spawned into server, not fully active, though
	NET_SignonState signonMsg2( SIGNONSTATE_SPAWN, pServer->GetSpawnCount() );
	signonMsg2.WriteToBuffer( msg );

	WriteMessages( dem_signon, msg ); 
	
	return m_DemoFile.GetCurPos( false ) - start;
}


void CHLTVDemoRecorder::WriteFrame( CHLTVFrame *pFrame )
{
	ALIGN4 byte		buffer[ NET_MAX_PAYLOAD ] ALIGN4_POST;
	bf_write	msg( "CHLTVDemo::RecordFrame", buffer, sizeof( buffer ) );

	assert( hltv->IsMasterProxy() ); // this works only on the master since we use sv.

	//first write reliable data
	bf_write *data = &pFrame->m_Messages[HLTV_BUFFER_RELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten() );

	//now send snapshot data

	// send tick time
	NET_Tick tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );
	tickmsg.WriteToBuffer( msg );


#ifndef SHARED_NET_STRING_TABLES
	// Update shared client/server string tables. Must be done before sending entities
	sv.m_StringTables->WriteUpdateMessage( NULL, max( m_nSignonTick, m_nDeltaTick ), msg );
#endif

	// get delta frame
	CClientFrame *deltaFrame = hltv->GetClientFrame( m_nDeltaTick ); // NULL if delta_tick is not found or -1
	
	// send entity update, delta compressed if deltaFrame != NULL
	sv.WriteDeltaEntities( hltv->m_MasterClient, pFrame, deltaFrame, msg );

	// send all unreliable temp ents between last and current frame
	CFrameSnapshot * fromSnapshot = deltaFrame?deltaFrame->GetSnapshot():NULL;
	sv.WriteTempEntities( hltv->m_MasterClient, pFrame->GetSnapshot(), fromSnapshot, msg, 255 );

	// write sound data
	data = &pFrame->m_Messages[HLTV_BUFFER_SOUNDS];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// write voice data
	data = &pFrame->m_Messages[HLTV_BUFFER_VOICE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// last write unreliable data
	data = &pFrame->m_Messages[HLTV_BUFFER_UNRELIABLE];
	if ( data->GetNumBitsWritten() )
		msg.WriteBits( data->GetBasePointer(), data->GetNumBitsWritten()  );

	// update delta tick just like fakeclients do
	m_nDeltaTick = pFrame->tick_count;

	// write packet to demo file
	WriteMessages( dem_packet, msg ); 
}

void CHLTVDemoRecorder::WriteMessages( unsigned char cmd, bf_write &message )
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
	
	if ( tv_debug.GetInt() > 1 )
	{
		Msg( "Writing SourceTV demo message %i bytes at file pos %i\n", len, m_DemoFile.GetCurPos( false ) );
	}
}

void CHLTVDemoRecorder::RecordMessages(bf_read &data, int bits)
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

void CHLTVDemoRecorder::RecordPacket()
{
	if( m_MessageData.GetBasePointer() )
	{
		WriteMessages( dem_packet, m_MessageData );
		m_MessageData.Reset(); // clear message buffer
	}
}
