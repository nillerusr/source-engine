//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"
#include "enginestats.h"
#include "iprediction.h"
#include "cl_demo.h"
#include "cl_demoactionmanager.h"
#include "cl_pred.h"

#include "baseautocompletefilelist.h"
#include "demofile/demoformat.h"
#include "gl_matsysiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/etwprof.h"
#include "tier0/icommandline.h"
#include "vengineserver_impl.h"
#include "console.h"
#include "dt_common_eng.h"
#include "net_chan.h"
#include "gl_model_private.h"
#include "decal.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "cl_demouipanel.h"
#include "materialsystem/materialsystem_config.h"
#include "tier2/tier2.h"
#include "vgui_baseui_interface.h"
#include "con_nprint.h"
#include "networkstringtableclient.h"

#ifdef SWDS
#include "server.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar demo_recordcommands( "demo_recordcommands", "1", FCVAR_CHEAT, "Record commands typed at console into .dem files." );
static ConVar demo_quitafterplayback( "demo_quitafterplayback", "0", 0, "Quits game after demo playback." );
static ConVar demo_debug( "demo_debug", "0", 0, "Demo debug info." );
static ConVar demo_interpolateview( "demo_interpolateview", "1", 0, "Do view interpolation during dem playback." );
static ConVar demo_pauseatservertick( "demo_pauseatservertick", "0", 0, "Pauses demo playback at server tick" );
static ConVar timedemo_runcount( "timedemo_runcount", "0", 0, "Runs time demo X number of times." );

// singeltons:
static char g_pStatsFile[MAX_OSPATH] = { 0 };
static bool s_bBenchframe = false;

static CDemoRecorder s_ClientDemoRecorder;
CDemoRecorder *g_pClientDemoRecorder = &s_ClientDemoRecorder;
IDemoRecorder *demorecorder = g_pClientDemoRecorder;

static CDemoPlayer s_ClientDemoPlayer;
CDemoPlayer *g_pClientDemoPlayer = &s_ClientDemoPlayer;
IDemoPlayer *demoplayer = g_pClientDemoPlayer;

extern CNetworkStringTableContainer *networkStringTableContainerClient;

// This is the number of units under which we are allowed to interpolate, otherwise pop.
// This fixes problems with in-level transitions.
static ConVar demo_interplimit( "demo_interplimit", "4000", 0, "How much origin velocity before it's considered to have 'teleported' causing interpolation to reset." );
static ConVar demo_avellimit( "demo_avellimit", "2000", 0, "Angular velocity limit before eyes considered snapped for demo playback." );

#define DEMO_HEADER_FILE	"demoheader.tmp"

// Fast forward convars
static ConVar demo_fastforwardstartspeed( "demo_fastforwardstartspeed", "2", 0, "Go this fast when starting to hold FF button." );
static ConVar demo_fastforwardfinalspeed( "demo_fastforwardfinalspeed", "20", 0, "Go this fast when starting to hold FF button." );
static ConVar demo_fastforwardramptime( "demo_fastforwardramptime", "5", 0, "How many seconds it takes to get to full FF speed." );

float scr_demo_override_fov = 0.0f;

//-----------------------------------------------------------------------------
// Purpose: Implements IDemo and handles demo file i/o
// Demos are more or less driven off of network traffic, but there are a few
//  other kinds of data items that are also included in the demo file:  specifically
//  commands that the client .dll itself issued to the engine are recorded, though they
//  probably were not the result of network traffic.
// At the start of a connection to a map/server, all of the signon, etc. network packets
//  are buffered.  This allows us to actually decide to start recording the demo at a later
//  time.  Once we actually issue the recording command, we don't actually start recording 
//  network traffic, but instead we ask the server for an "uncompressed" packet (otherwise
//  we wouldn't be able to deal with the incoming packets during playback because we'd be missing the
//  data to delta from ) and go into a waiting state.  Once an uncompressed packet is received, 
//  we unset the waiting state and start recording network packets from that point forward.
// Demo's record the elapsed time based on the current client clock minus the time the demo was started
// During playback, the elapsed time for playback ( based on the host_time, which is subject to the
//  host_frametime cvar ) is compared with the elapsed time on the message from the demo file.  
// If it's not quite time for the message yet, the demo input stream is rewound
// The demo system sits at the point where the client is checking to see if any network messages
//  have arrived from the server.  If the message isn't ready for processing, the demo system
//  just responds that there are no messages waiting and the client continues on
// Once a true network message with entity data is read from the demo stream, a couple of other
//  actions occur.  First, the timestamp in the demo file and the view origin/angles corresponding
//  to the message are cached off.  Then, we search ahead (into the future) to find out the next true network message
//  we are going to read from the demo file.  We store of it's elapsed time and view origin/angles
// Every frame that the client is rendering, even if there is no data from the demo system,
//  the engine asks the demo system to compute an interpolated origin and view angles.  This
//  is done by taking the current time on the host and figuring out how far that puts us between
//  the last read origin from the demo file and the time when we'll actually read out and use the next origin
// We use Quaternions to avoid gimbel lock on interpolating the view angles
// To make a movie recorded at a fixed frame rate you would simply set the host_framerate to the
//  desired playback fps ( e.g., 0.02 == 50 fps ), then issue the startmovie command, and then
//  play the demo.  The demo system will think that the engine is running at 50 fps and will pull
//  messages accordingly, even though movie recording kills the actually framerate.
// It will also render frames with render origin/angles interpolated in-between the previous and next origins
//  even if the recording framerate was not 50 fps or greater.  The interpolation provides a smooth visual playback 
//  of the demo information to the client without actually adding latency to the view position (because we are
//  looking into the future for the position, not buffering the past data ).
//-----------------------------------------------------------------------------

static bool IsControlCommand( unsigned char cmd )
{
	return ( (cmd == dem_signon) || (cmd == dem_stop) ||
		     (cmd == dem_synctick) || (cmd == dem_datatables ) ||
			 (cmd == dem_stringtables) );
}


// Puts a flashing overlay on the screen during demo recording/playback
static ConVar cl_showdemooverlay( "cl_showdemooverlay", "0", 0, "How often to flash demo recording/playback overlay (0 - disable overlay, -1 - show always)" );

class DemoOverlay
{
public:
	DemoOverlay();
	~DemoOverlay();

public:
	void Tick();
	void DrawOverlay( float fSetting );

protected:
	float m_fLastTickTime;
	float m_fLastTickOverlay;
	enum Overlay { OVR_NONE = 0, OVR_REC = 1 << 1, OVR_PLAY = 1 << 2 };
	bool m_bTick;
	int m_maskDrawnOverlay;
} g_DemoOverlay;

DemoOverlay::DemoOverlay() :
	m_fLastTickTime( 0.f ), m_fLastTickOverlay( 0.f ), m_bTick( false ), m_maskDrawnOverlay( OVR_NONE )
{
}

DemoOverlay::~DemoOverlay()
{
}

void DemoOverlay::Tick()
{
	if ( !m_bTick )
	{
		m_bTick = true;

		float const fRealTime = Sys_FloatTime();
		if ( m_fLastTickTime != fRealTime )
		{
			m_fLastTickTime = fRealTime;

			float const fDelta = m_fLastTickTime - m_fLastTickOverlay;
			float const fSettingDelta = cl_showdemooverlay.GetFloat();

			if ( fSettingDelta <= 0.f ||
				fDelta >= fSettingDelta )
			{
				m_fLastTickOverlay = m_fLastTickTime;
				DrawOverlay( fSettingDelta );
			}
		}

		m_bTick = false;
	}
}

void DemoOverlay::DrawOverlay( float fSetting )
{
	int maskDrawnOverlay = OVR_NONE;

	if ( fSetting < 0.f )
	{
		// Keep drawing
		maskDrawnOverlay =
			( demorecorder->IsRecording() ? OVR_REC : 0 ) |
			( demoplayer->IsPlayingBack() ? OVR_PLAY : 0 );
	}
	else if ( fSetting == 0.f )
	{
		// None
		maskDrawnOverlay = OVR_NONE;
	}
	else
	{
		// Flash
		maskDrawnOverlay = ( !m_maskDrawnOverlay ) ? (
			( demorecorder->IsRecording() ? OVR_REC : 0 ) |
			( demoplayer->IsPlayingBack() ? OVR_PLAY : 0 )
			) : OVR_NONE;
	}

	int const idx = 1;

	if ( OVR_NONE == maskDrawnOverlay &&
		 OVR_NONE != m_maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.time_to_live = -1;
		Con_NXPrintf( &xprn, "" );
	}
	
	if ( OVR_PLAY & maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.color[0] = 0.f;
		xprn.color[1] = 1.f;
		xprn.color[2] = 0.f;
		xprn.fixed_width_font = true;
		xprn.time_to_live = ( fSetting > 0.f ) ? fSetting : 1.f;
		Con_NXPrintf( &xprn, "  PLAY   " );
	}
	
	if ( OVR_REC & maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.color[0] = 1.f;
		xprn.color[1] = 0.f;
		xprn.color[2] = 0.f;
		xprn.fixed_width_font = true;
		xprn.time_to_live = ( fSetting > 0.f ) ? fSetting : 1.f;
		Con_NXPrintf( &xprn, "   REC   " );
	}

	m_maskDrawnOverlay = maskDrawnOverlay;
}
			

//-----------------------------------------------------------------------------
// Purpose: Mark whether we are waiting for the first uncompressed update packet
// Input  : waiting - 
//-----------------------------------------------------------------------------
void CDemoRecorder::SetSignonState(int state)
{
	if ( demoplayer->IsPlayingBack() )
		return;

	if ( state == SIGNONSTATE_NEW )
	{
		if ( m_DemoFile.IsOpen() )
		{
			// we are already recording a demo file
			CloseDemoFile();

			// prepare for recording next demo
			m_nDemoNumber++; 
		}

		StartupDemoHeader();
	}
	else if ( state == SIGNONSTATE_SPAWN )
	{
		// close demo file header when this packet is finished
		m_bCloseDemoFile = true;
	}
	else if ( state == SIGNONSTATE_FULL )
	{
		if ( m_bRecording )
		{
			StartupDemoFile();
		}
	}
}

int CDemoRecorder::GetRecordingTick( void )
{
	if ( cl.m_nMaxClients > 1 )
	{
		return TIME_TO_TICKS( net_time ) - m_nStartTick;
	}
	else
	{
		return cl.GetClientTickCount() - m_nStartTick;
	}
}

void CDemoRecorder::ResyncDemoClock()
{
	if ( cl.m_nMaxClients > 1 )
	{
		m_nStartTick = TIME_TO_TICKS( net_time );
	}
	else
	{
		m_nStartTick = cl.GetClientTickCount();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : info - 
//-----------------------------------------------------------------------------
void CDemoRecorder::GetClientCmdInfo( democmdinfo_t& info )
{
	info.flags		= FDEMO_NORMAL;

	if( m_bResetInterpolation )
	{
		info.flags |= FDEMO_NOINTERP;
		m_bResetInterpolation = false;
	}

	g_pClientSidePrediction->GetViewOrigin( info.viewOrigin );
#ifndef SWDS
	info.viewAngles = cl.viewangles;
#endif
	g_pClientSidePrediction->GetLocalViewAngles( info.localViewAngles );

	// Nothing by default
	info.viewOrigin2.Init();
	info.viewAngles2.Init();
	info.localViewAngles2.Init();
}

void CDemoRecorder::WriteBSPDecals()
{
	decallist_t	*decalList = (decallist_t*)malloc( sizeof(decallist_t) * Draw_DecalMax() );
	
	int decalcount = DecalListCreate( decalList );

	char		data[NET_MAX_PAYLOAD];
	bf_write	msg;

	msg.StartWriting( data, NET_MAX_PAYLOAD );
	msg.SetDebugName( "DemoFileWriteBSPDecals" );

	for ( int i = 0; i < decalcount; i++ )
	{
		decallist_t *entry = &decalList[ i ];

		SVC_BSPDecal decal;

		bool found = false;

		IClientEntity *clientEntity = entitylist->GetClientEntity( entry->entityIndex );

		if ( !clientEntity )
			continue;

		
		const model_t * pModel = clientEntity->GetModel();

		decal.m_Pos = entry->position;
		decal.m_nEntityIndex = entry->entityIndex;
		decal.m_nDecalTextureIndex = Draw_DecalIndexFromName( entry->name, &found );
		decal.m_nModelIndex = 0;

		if ( pModel )
		{
			decal.m_nModelIndex = cl.LookupModelIndex( modelloader->GetName( pModel ) );
		}

		decal.WriteToBuffer( msg );
	}

	WriteMessages( msg );
	
	free( decalList );
}

void CDemoRecorder::RecordServerClasses( ServerClass *pClasses )
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

void CDemoRecorder::RecordStringTables()
{
	MEM_ALLOC_CREDIT();

	// !KLUDGE! It would be nice if the bit buffer could write into a stream
	// with the power to grow itself.  But it can't.  Hence this really bad
	// kludge
	void *data = NULL;
	int dataLen = 512 * 1024;
	while ( dataLen <= DEMO_FILE_MAX_STRINGTABLE_SIZE )
	{
		data = realloc( data, dataLen );
		bf_write buf( data, dataLen );
		buf.SetDebugName("CDemoRecorder::RecordStringTables");
		buf.SetAssertOnOverflow( false ); // Doesn't turn off all the spew / asserts, but turns off one
		networkStringTableContainerClient->WriteStringTables( buf );

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

void CDemoRecorder::RecordUserInput( int cmdnumber )
{
	char buffer[256];
	bf_write msg( "CDemo::WriteUserCmd", buffer, sizeof(buffer) );

	g_ClientDLL->EncodeUserCmdToBuffer( msg, cmdnumber );

	m_DemoFile.WriteUserCmd( cmdnumber, buffer, msg.GetNumBytesWritten(), GetRecordingTick() );
}

void CDemoRecorder::ResetDemoInterpolation( void )
{
	m_bResetInterpolation = true;
}


//-----------------------------------------------------------------------------
// Purpose: saves all cvars falgged with FVAR_DEMO to demo file
//-----------------------------------------------------------------------------
void CDemoRecorder::WriteDemoCvars()
{
	const ConCommandBase *var;
	
	for ( var= g_pCVar->GetCommands() ; var ; var=var->GetNext() )
	{
		if ( var->IsCommand() )
			continue;

		const ConVar *pCvar = ( const ConVar * )var;

		if ( !pCvar->IsFlagSet( FCVAR_DEMO ) )
			continue;

		char cvarcmd[MAX_OSPATH];

		Q_snprintf( cvarcmd, sizeof(cvarcmd),"%s \"%s\"",
			pCvar->GetName(), Host_CleanupConVarStringValue( pCvar->GetString() ) );

		m_DemoFile.WriteConsoleCommand( cvarcmd, GetRecordingTick() );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cmdname - 
//-----------------------------------------------------------------------------
void CDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	if ( !demo_recordcommands.GetInt() )
		return;

	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartupDemoHeader( void )
{
	CloseDemoFile();	// make sure it's closed

	// Note: this is replacing tmpfile()
	if ( !m_DemoFile.Open( DEMO_HEADER_FILE, false ) )
	{
		ConDMsg ("ERROR: couldn't open temporary header file.\n");
		return;
	}

	m_bIsDemoHeader = true;

	Assert( m_MessageData.GetBasePointer() == NULL );

	// setup writing data buffer
	m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	m_MessageData.SetDebugName( "DemoHeaderWriteBuffer" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartupDemoFile( void )
{
	if ( !m_bRecording )
		return;

	// Already recording!!!
	if ( m_DemoFile.IsOpen() )
		return;

	char demoFileName[MAX_OSPATH];

	if ( m_nDemoNumber <= 1 )
	{
		V_sprintf_safe( demoFileName, "%s.dem", m_szDemoBaseName );
	}
	else
	{
		V_sprintf_safe( demoFileName, "%s_%i.dem", m_szDemoBaseName, m_nDemoNumber );
	}

	// strip any trailing whitespace
	Q_StripPrecedingAndTrailingWhitespace( demoFileName );

	// make sure the .dem extension is still present
	char ext[10];
	Q_ExtractFileExtension( demoFileName, ext, sizeof( ext ) );
	if ( Q_strcasecmp( ext, "dem" ) )
	{
		ConMsg( "StartupDemoFile: invalid filename.\n" );
		return;
	}

	if ( !m_DemoFile.Open( demoFileName, false ) )
		return;

	// open demo header file containing sigondata
	FileHandle_t hDemoHeader = g_pFileSystem->Open( DEMO_HEADER_FILE, "rb"	);
	if ( hDemoHeader == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg ("StartupDemoFile: couldn't open demo file header.\n");
		return;
	}

	Assert( m_MessageData.GetBasePointer() == NULL );

	// setup writing data buffer
	m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	m_MessageData.SetDebugName( "DemoFileWriteBuffer" );

	// fill demo header info
	demoheader_t *dh = &m_DemoFile.m_DemoHeader;
	Q_memset(dh, 0, sizeof(demoheader_t));

	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = PROTOCOL_VERSION;
	Q_strncpy(dh->demofilestamp, DEMO_HEADER_ID, sizeof(dh->demofilestamp) );

	Q_FileBase( modelloader->GetName( host_state.worldmodel ), dh->mapname, sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	Q_strncpy(szGameDir, com_gamedir, sizeof( szGameDir ) );
	Q_FileBase ( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	Q_strncpy( dh->servername, cl.m_szRetryAddress, sizeof( dh->servername ) );
	Q_strncpy( dh->clientname, cl_name.GetString(), sizeof( dh->clientname ) );

	
	// get size	signon data size
	dh->signonlength = g_pFileSystem->Size(hDemoHeader);
	
	// write demo file header info
	m_DemoFile.WriteDemoHeader();
	
	// copy signon data from header file to demo file
	m_DemoFile.WriteFileBytes( hDemoHeader, dh->signonlength );

	// close but keep header file, we might need it for a second record
	g_pFileSystem->Close( hDemoHeader );
	
	m_nFrameCount = 0;
	m_bIsDemoHeader = false;
		
	ResyncDemoClock(); // reset demo clock
		
	// tell client to sync demo clock too 
	m_DemoFile.WriteCmdHeader( dem_synctick, 0 );
	
	RecordStringTables();

	// Demo playback should read this as an incoming message.
	WriteDemoCvars(); // save all cvars marked with FCVAR_DEMO

	WriteBSPDecals();

	g_ClientDLL->HudReset();

	//  tell server that we started recording a demo
	cl.SendStringCmd( "demorestart" );

	ConMsg ("Recording to %s...\n", demoFileName);

	g_ClientDLL->OnDemoRecordStart( m_szDemoBaseName );
}

CDemoRecorder::CDemoRecorder()
{
}

CDemoRecorder::~CDemoRecorder()
{
	CloseDemoFile();	
}

CDemoFile *CDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}

void CDemoRecorder::ResumeRecording()
{

}

void CDemoRecorder::PauseRecording()
{

}


void CDemoRecorder::CloseDemoFile()
{
	if ( m_DemoFile.IsOpen())
	{
		if ( !m_bIsDemoHeader )
		{
			// Demo playback should read this as an incoming message.
			m_DemoFile.WriteCmdHeader( dem_stop, GetRecordingTick() );

			// update demo header infos
			m_DemoFile.m_DemoHeader.playback_ticks	= GetRecordingTick();
			m_DemoFile.m_DemoHeader.playback_time	= host_state.interval_per_tick * GetRecordingTick();
			m_DemoFile.m_DemoHeader.playback_frames = m_nFrameCount;

			// go back to header and write demoHeader with correct time and #frame again
			m_DemoFile.WriteDemoHeader();

			ConMsg ("Completed demo, recording time %.1f, game frames %i.\n", 
				m_DemoFile.m_DemoHeader.playback_time, m_DemoFile.m_DemoHeader.playback_frames );
		}

		if ( demo_debug.GetInt() )
		{
			ConMsg ("Closed demo file, %i bytes.\n", m_DemoFile.GetSize() );
		}

		m_DemoFile.Close();

		g_ClientDLL->OnDemoRecordStop();
	}

	m_bCloseDemoFile = false;
	m_bIsDemoHeader = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0 );
	}
}

void CDemoRecorder::RecordMessages(bf_read &data, int bits)
{
	if ( m_MessageData.GetBasePointer() && (bits>0) )
	{
		m_MessageData.WriteBitsFromBuffer( &data, bits );

		Assert( !m_MessageData.IsOverflowed() );
	}
}

void CDemoRecorder::RecordPacket()
{
	WriteMessages( m_MessageData );

	m_MessageData.Reset(); // clear message buffer
	
	if ( m_bCloseDemoFile )
	{
		CloseDemoFile();
	}
}

void CDemoRecorder::WriteMessages( bf_write &message )
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
	unsigned char cmd = m_bIsDemoHeader ? dem_signon : dem_packet;

	if ( cmd == dem_packet )
	{
		m_nFrameCount++;
	}

	// write command & time
	m_DemoFile.WriteCmdHeader( cmd, GetRecordingTick() ); 
	
	democmdinfo_t info;
	// Snag current info
	GetClientCmdInfo( info );
		
	// Store it
	m_DemoFile.WriteCmdInfo( info );
		
	// write network channel sequencing infos
	int nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck;
	cl.m_NetChannel->GetSequenceData( nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck );
	m_DemoFile.WriteSequenceInfo( nInSequenceNr, nOutSequenceNrAck );
	
	// Output the messge buffer.
	m_DemoFile.WriteRawData( (char*) message.GetBasePointer(), len );

	if ( demo_debug.GetInt() >= 1 )
	{
		Msg( "Writing demo message %i bytes at file pos %i\n", len, m_DemoFile.GetCurPos( false ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: stop recording a demo
//-----------------------------------------------------------------------------
void CDemoRecorder::StopRecording( void )
{
	if ( !IsRecording() )
	{
		return;
	}

	if ( m_MessageData.GetBasePointer() )
	{
		delete[] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0);
	}

	CloseDemoFile();
	
	m_bRecording = false;
	m_nDemoNumber = 0;

	g_DemoOverlay.Tick();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			track - 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartRecording( const char *name, bool bContinuously )
{
	Q_strncpy( m_szDemoBaseName, name, sizeof(m_szDemoBaseName));
	
	m_bRecording		 = true;
	m_nDemoNumber		 = 1;
	m_bResetInterpolation = false;


	g_DemoOverlay.Tick();

	// request a full game update from server 
	cl.ForceFullUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoRecorder::IsRecording( void )
{
	g_DemoOverlay.Tick();

	return m_bRecording;
}

//-----------------------------------------------------------------------------
// Purpose: Called when a demo file runs out, or the user starts a game
// Output : void CDemo::StopPlayback
//-----------------------------------------------------------------------------
void CDemoPlayer::StopPlayback( void )
{
	if ( !IsPlayingBack() )
		return;

	demoaction->StopPlaying();

	m_DemoFile.Close();
	m_bPlayingBack = false;
	m_bLoading = false;
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;
	m_nEndTick = 0;

	if ( m_bTimeDemo )
	{
		g_EngineStats.EndRun();

		if ( !s_bBenchframe )
		{
			WriteTimeDemoResults();
		}
		else
		{
			mat_norendering.SetValue( 0 );
		}

		m_bTimeDemo = false;
	}
	else
	{
		int framecount = host_framecount - m_nTimeDemoStartFrame;
		float demotime = Sys_FloatTime() - m_flTimeDemoStartTime;

		if ( demotime > 0.0f )
		{
			DevMsg( "Demo playback finished ( %.1f seconds, %i render frames, %.2f fps).\n", demotime, framecount, framecount/demotime);
		}

	}
	
	m_flPlaybackRateModifier = 1.0f;

	delete[] m_DemoPacket.data;
	m_DemoPacket.data = NULL;

	scr_demo_override_fov = 0.0f;

	if ( timedemo_runcount.GetInt() > 1 )
	{
		timedemo_runcount.SetValue( timedemo_runcount.GetInt() - 1 );

		Cbuf_AddText( va( "timedemo %s", m_DemoFile.m_szFileName ) );
	}
	else if ( demo_quitafterplayback.GetBool() )
	{
		Cbuf_AddText( "quit\n" );
	}

	g_ClientDLL->OnDemoPlaybackStop();
}

CDemoFile *CDemoPlayer::GetDemoFile( void )
{
	return &m_DemoFile;
}

#define SKIP_TO_TICK_FLAG uint32( uint32( 0x88 ) << 24 )

bool CDemoPlayer::IsSkipping( void )
{
	return m_bPlayingBack && ( m_nSkipToTick != -1 );
}

bool CDemoPlayer::IsLoading( void )
{
	return m_bLoading;
}

int CDemoPlayer::GetTotalTicks(void)
{
	return m_DemoFile.m_DemoHeader.playback_ticks;	
}

void CDemoPlayer::SkipToTick( int tick, bool bRelative, bool bPause )
{
	if ( bRelative )
	{
		tick = GetPlaybackTick() + tick;
	}

	if ( tick < 0 )
		return;

	if ( tick < GetPlaybackTick() )
	{
		// we have to reload the whole demo file
		// we need to create a temp copy of the filename
		char fileName[MAX_OSPATH];
		Q_strncpy( fileName, m_DemoFile.m_szFileName, sizeof(fileName) );

		// reload current demo file
		ETWMarkPrintf( "DemoPlayer: Reloading demo file '%s'", fileName );
		StartPlayback( fileName, m_bTimeDemo );

		// Make sure the proper skipping occurs after reload
		if ( tick > 0 )
			tick |= SKIP_TO_TICK_FLAG;
	}

	m_nSkipToTick = tick;
	ETWMark1I( "DemoPlayer: SkipToTick", tick );

	if ( bPause )
		PausePlayback( -1 );
}

void CDemoPlayer::SetEndTick( int tick )
{
	if ( tick < 0 )
		return;

	m_nEndTick = tick;
}

//-----------------------------------------------------------------------------
// Purpose: Read in next demo message and send to local client over network channel, if it's time.
// Output : bool 
//-----------------------------------------------------------------------------
bool CDemoPlayer::ParseAheadForInterval( int curtick, int intervalticks )
{
	int			tick = 0;
	int			dummy;
	byte		cmd = dem_stop;

	democmdinfo_t	nextinfo;

	long		starting_position = m_DemoFile.GetCurPos( true );

	// remove all entrys older than 32 ticks
	while ( m_DestCmdInfo.Count() > 0 )
	{
		DemoCommandQueue& entry = m_DestCmdInfo[ 0 ];

		if ( entry.tick >= (curtick - 32)  )
			break;

		if ( entry.filepos >= starting_position )
			break;

		 m_DestCmdInfo.Remove( 0 );
	}

	if ( m_bTimeDemo ) 
		return false;

	while ( true )
	{
		// skip forward to the next dem_packet or dem_signon
		bool swallowmessages = true;
		do
		{
			m_DemoFile.ReadCmdHeader( cmd, tick );

			// COMMAND HANDLERS
			switch ( cmd )
			{
			case dem_synctick:
			case dem_stop:
				{
					m_DemoFile.SeekTo( starting_position, true );
					return false;
				}
				break;
			case dem_consolecmd:
				{
					m_DemoFile.ReadConsoleCommand();
				}
				break;
			case dem_datatables:
				{
					m_DemoFile.ReadNetworkDataTables( NULL );
				}
				break;
			case dem_usercmd:
				{
					m_DemoFile.ReadUserCmd( NULL, dummy );
				}
				break;
			case dem_stringtables:
				{
					m_DemoFile.ReadStringTables( NULL );
				}
				break;
			default:
				{
					swallowmessages = false;
				}
				break;
			}
		}
		while ( swallowmessages );

		int curpos = m_DemoFile.GetCurPos( true );

		// we read now a dem_packet
		m_DemoFile.ReadCmdInfo( nextinfo );
		m_DemoFile.ReadSequenceInfo( dummy, dummy ); 
		m_DemoFile.ReadRawData( NULL, 0 );

		DemoCommandQueue entry;
		entry.info = nextinfo;
		entry.tick = tick;
		entry.filepos = curpos;

		int i = 0;
		int c =  m_DestCmdInfo.Count();
		for ( ; i < c; ++i )
		{
			if (  m_DestCmdInfo[ i ].filepos == entry.filepos )
				break; // cmdinfo is already in list
		}

		if ( i >= c )
		{
			// add cmdinfo to list
			if ( c > 0 )
			{
				if (  m_DestCmdInfo[ c - 1 ].tick > tick )
				{
					 m_DestCmdInfo.RemoveAll();
				}
			}

			m_DestCmdInfo.AddToTail( entry );
		}

		if ( ( tick - curtick ) > intervalticks )
			break;
	}

	m_DemoFile.SeekTo( starting_position, true );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Read in next demo message and send to local client over network channel, if it's time.
// Output : netpacket_t* -- NULL if there is no packet available at this time.
//-----------------------------------------------------------------------------
netpacket_t *CDemoPlayer::ReadPacket( void )
{
	int			tick = 0;
	byte		cmd = dem_signon;
	long		curpos = 0;

	if ( ! m_DemoFile.IsOpen() )
	{
		m_bPlayingBack = false;
		Host_EndGame( true, "Tried to read a demo message with no demo file\n" );
		return NULL;
	}

	// If game is still shutting down, then don't read any demo messages from file quite yet
	if ( HostState_IsGameShuttingDown() )
	{
		return NULL;
	}

	Assert( IsPlayingBack() );

	if ( IsSkipping() )
	{
		// Every nMaxConsecutiveSkipPackets frames return NULL so that we don't build up an
		// endless supply of unprocessed packets. This avoids causing overflows and excessive
		// "highwater marks" in various Dota subsystems.
		++m_nSkipPacketsPlayed;
		if ( m_nSkipPacketsPlayed >= nMaxConsecutiveSkipPackets )
		{
			m_nSkipPacketsPlayed = 0;
			return NULL;
		}
	}
	else
	{
		m_nSkipPacketsPlayed = 0;
	}

	// External editor has paused playback
	if ( CheckPausedPlayback() )
		return NULL;

	bool bStopReading = false;
	
	while ( !bStopReading )
	{
		curpos = m_DemoFile.GetCurPos( true );

		m_DemoFile.ReadCmdHeader( cmd, tick );

		// always read control commands 
		if ( !IsControlCommand( cmd ) )
		{
			int playbacktick = GetPlaybackTick();

#if defined( RAD_TELEMETRY_ENABLED )
			g_Telemetry.playbacktick = playbacktick;
#endif

			// If the end tick is set, check to see if we should bail
			if ( m_nEndTick > 0 && playbacktick >= m_nEndTick )
			{
				m_nEndTick = 0;
				return NULL;
			}

			if ( !m_bTimeDemo )
			{
				// Time demo ignores clocks and tries to synchronize frames to what was recorded
				//  I.e., while frame is the same, read messages, otherwise, skip out.
				// If we're still signing on, then just parse messages until fully connected no matter what
				if ( cl.IsActive() &&
					(tick > playbacktick) && !IsSkipping() )
				{
					// is not time yet
					bStopReading = true;
				}
			}
			else
			{
				if ( m_nTimeDemoCurrentFrame == host_framecount )
				{
					// If we are playing back a timedemo, and we've already passed on a 
					//  frame update for this host_frame tag, then we'll just skip this mess
					bStopReading = true;
				}
			}

			if ( bStopReading )
			{
				demoaction->Update( false, playbacktick, TICKS_TO_TIME( playbacktick )  );
				m_DemoFile.SeekTo( curpos, true ); // go back to start of current demo command
				return NULL;   // Not time yet, dont return packet data.
			}
		}

		// COMMAND HANDLERS
		switch ( cmd )
		{
		case dem_synctick:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_synctick\n", tick );
				}

				ResyncDemoClock();

				// Once demo clock got resync-ed we can go ahead and
				// perform skipping logic normally
				if ( ( m_nSkipToTick != -1 ) &&
					 ( ( m_nSkipToTick & SKIP_TO_TICK_FLAG ) == SKIP_TO_TICK_FLAG ) )
				{
					m_nSkipToTick &= ~SKIP_TO_TICK_FLAG;
				}
			}
			break;
		case dem_stop:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_stop\n", tick );
				}

				OnStopCommand();

				return NULL;
			}
			break;
		case dem_consolecmd:
			{
				const char * command = m_DemoFile.ReadConsoleCommand();

				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_consolecmd [%s]\n", tick, command );
				}

				Cbuf_AddText( command );
				Cbuf_Execute();
			}
			break;
		case dem_datatables:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_datatables\n", tick );
				}

				void *data = malloc( 256*1024 ); // X360TBD: How much memory is really needed here?
				bf_read buf( "dem_datatables", data, 256*1024 );
				m_DemoFile.ReadNetworkDataTables( &buf );
				buf.Seek( 0 );								// re-read data

				// support for older engine demos
				if ( !DataTable_LoadDataTablesFromBuffer( &buf, m_DemoFile.m_DemoHeader.demoprotocol ) )
				{
					Host_Error( "Error parsing network data tables during demo playback." );
				}
				free( data );
			}
			break;
		case dem_stringtables:
			{
				void *data = NULL;
				int dataLen = 512 * 1024;
				while ( dataLen <= DEMO_FILE_MAX_STRINGTABLE_SIZE )
				{
					data = realloc( data, dataLen );
					bf_read buf( "dem_stringtables", data, dataLen );
					// did we successfully read
					if ( m_DemoFile.ReadStringTables( &buf ) > 0 )
					{
						buf.Seek( 0 );
						if ( !networkStringTableContainerClient->ReadStringTables( buf ) )
						{
							Host_Error( "Error parsing string tables during demo playback." );
						}
						break;
					}

					// Didn't fit.  Try doubling the size of the buffer
					dataLen *= 2;
				}

				if ( dataLen > DEMO_FILE_MAX_STRINGTABLE_SIZE )
				{
					Warning( "ReadPacket failed to read string tables. Trying to read string tables that's bigger than max string table size\n" );
				}

				free( data );
			}
			break;
		case dem_usercmd:
			{

				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_usercmd\n", tick );
				}

				char buffer[256];
				int  length = sizeof(buffer);
				int outgoing_sequence = m_DemoFile.ReadUserCmd( buffer, length );

				// put it into a bitbuffer 
				bf_read msg( "CDemo::ReadUserCmd", buffer, length );

				g_ClientDLL->DecodeUserCmdFromBuffer( msg, outgoing_sequence );

				// Note, we need to have the current outgoing sequence correct so we can do prediction
				//  correctly during playback
				cl.lastoutgoingcommand = outgoing_sequence;
				
			}
			break;
		default:
			{
				bStopReading = true;

				if ( IsSkipping() )
				{
					// adjust playback host_tickcount when skipping
					m_nStartTick = host_tickcount - tick;
				}
			}
			break;
		}
	}

	if ( cmd == dem_packet )
	{
		// remember last frame we read a dem_packet update
		m_nTimeDemoCurrentFrame = host_framecount;
	}
	
	int inseq, outseqack, outseq = 0;

	m_DemoFile.ReadCmdInfo( m_LastCmdInfo );

	m_DemoFile.ReadSequenceInfo( inseq, outseqack );
	cl.m_NetChannel->SetSequenceData( outseq, inseq, outseqack );

	int length = m_DemoFile.ReadRawData( (char*)m_DemoPacket.data,  NET_MAX_PAYLOAD );

	if ( demo_debug.GetBool() )
	{
		Msg( "%d network packet [%d]\n", tick, length );
	}

	if ( length > 0 )
	{
		// succsessfully read new demopacket
		m_DemoPacket.received = realtime;
		m_DemoPacket.size = length;
		m_DemoPacket.message.StartReading( m_DemoPacket.data,  m_DemoPacket.size );
	
		if ( demo_debug.GetInt() >= 1 )
		{
			Msg( "Demo message, tick %i, %i bytes\n", GetPlaybackTick(), length );
		}
	}

	// Try and jump ahead one frame
	m_bInterpolateView = ParseAheadForInterval( tick, 8 );

	// ConMsg( "Reading message for %i : %f skip %i\n", m_nFrameCount, fElapsedTime, forceskip ? 1 : 0 );

	// Skip a few ticks before doing any timing
	if ( (m_nTimeDemoStartFrame < 0) && GetPlaybackTick() > 100 )
	{
		m_nTimeDemoStartFrame = host_framecount;
		m_flTimeDemoStartTime = Sys_FloatTime();
		m_flTotalFPSVariability = 0.0f;

		if ( m_bTimeDemo )
		{
			g_EngineStats.BeginRun();
		}
	}

	if ( m_nSnapshotTick > 0 && m_nSnapshotTick <= GetPlaybackTick() )
	{
		const char *filename = "benchframe";

		if ( m_SnapshotFilename[0] )
			filename = m_SnapshotFilename;

		CL_TakeScreenshot( filename ); // take a screenshot
		m_nSnapshotTick = 0;

		if ( s_bBenchframe )
		{
			Cbuf_AddText( "stopdemo\n" );
		}
	}

	return &m_DemoPacket;
}

void CDemoPlayer::InterpolateDemoCommand( int targettick, DemoCommandQueue& prev, DemoCommandQueue& next )
{
	CUtlVector< DemoCommandQueue >& list = m_DestCmdInfo;
	int c = list.Count();

	prev.info.Reset();
	next.info.Reset();

	if ( c < 2 )
	{
		// we need at least two entries to interpolate
		return; 
	}

	int i = 0;
	int savedI = -1;

	DemoCommandQueue *entry1 = &list[ i ];
	DemoCommandQueue *entry2 = &list[ i+1 ];

	while ( true )
	{
		if ( (entry1->tick <= targettick) && (entry2->tick > targettick) )
		{
			// Means we hit a FDEMO_NOINTERP along the way to now
			if ( savedI != -1 )
			{
				prev = list[ savedI ];
				next = list[ savedI + 1 ];
			}
			else
			{
				prev = *entry1;
				next = *entry2;
			}
			return;
		}

		// If any command between the previous target and now has the FDEMO_NOINTERP, we need to stop at the command just before that (entry), so we save off the I
		// We can't just return since we need to see if we actually get to a spanning pair (though we always should).  Also, we only latch this final interp spot on
		///  the first FDEMO_NOINTERP we see
		if ( savedI == -1 &&
			 entry2->tick > m_nPreviousTick &&
			 entry2->tick <= targettick &&
			 entry2->info.flags & FDEMO_NOINTERP )
		{
			savedI = i;
		}

		if ( i+2 == c )
			break;

		i++;
		entry1 = &list[ i ];
		entry2 = &list[ i+1 ];
	}	

	Assert( 0 );
}

static ConVar demo_legacy_rollback( "demo_legacy_rollback", "1", 0, "Use legacy view interpolation rollback amount in demo playback." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoPlayer::InterpolateViewpoint( void )
{
	if ( !IsPlayingBack() )
		return;

	democmdinfo_t outinfo;
	outinfo.Reset();

	bool bHasValidData =
		 m_LastCmdInfo.viewOrigin != vec3_origin ||
		 m_LastCmdInfo.viewAngles != vec3_angle ||
		 m_LastCmdInfo.localViewAngles != vec3_angle ||
		 m_LastCmdInfo.flags != 0;

	int nTargetTick = GetPlaybackTick();

	// Player view needs to be one tick interval in the past like the client DLL entities
	if ( cl.m_nMaxClients == 1 )
	{
		if ( demo_legacy_rollback.GetBool() )
		{
			nTargetTick -= TIME_TO_TICKS( cl.GetClientInterpAmount() ) + 1;
		}
		else
		{
			nTargetTick -= 1;
		}
	}

	float vel = 0.0f;
	float angVel = 0.0f; 
	if ( m_bInterpolateView && demo_interpolateview.GetBool() && bHasValidData )
	{
		DemoCommandQueue prev, next;
		float frac = 0.0f;

		prev.info = m_LastCmdInfo;
		prev.tick = -1;
		next.info = m_LastCmdInfo;
		next.tick = -1;

		// Determine current time slice
		
		InterpolateDemoCommand( nTargetTick, prev, next );

		float dt = TICKS_TO_TIME(next.tick-prev.tick);

		frac = (TICKS_TO_TIME(nTargetTick-prev.tick)+cl.m_tickRemainder)/dt;

		frac = clamp( frac, 0.0f, 1.0f );

		// Now interpolate
		Vector delta;

		Vector startorigin = prev.info.GetViewOrigin();
		Vector destorigin = next.info.GetViewOrigin();

		// check for teleporting - since there can be multiple cmd packets between a game frame,
		// we need to check from the last actually ran command to see if there was a teleport
		VectorSubtract( destorigin, m_LastCmdInfo.GetViewOrigin(), delta );
		float distmoved = delta.Length();
		
		if ( dt > 0.0f )
		{
			vel = distmoved / dt;
		}

		if ( dt > 0.0f )
		{
			QAngle startang = prev.info.GetLocalViewAngles();
			QAngle destang = next.info.GetLocalViewAngles();
	
			for ( int i = 0; i < 3; ++i )
			{
				float dAng = AngleNormalizePositive( destang[ i ] ) - AngleNormalizePositive( startang[ i ] );
				dAng = AngleNormalize( dAng );
				float aVel = fabs( dAng ) / dt;
				if ( aVel > angVel )
				{
					angVel = aVel;
				}
			}
		}

		// FIXME: This should be velocity based maybe?
		if ( (vel > demo_interplimit.GetFloat()) || 
			 (angVel > demo_avellimit.GetFloat() ) ||
			m_bResetInterpolation )
		{
			m_bResetInterpolation = false;

			// it's a teleport, just let it happen naturally next frame
			// setting frac to 1.0 (like it was previously) would just mean that we
			// are teleporting a frame ahead of when we should
			outinfo.viewOrigin = m_LastCmdInfo.GetViewOrigin();
			outinfo.viewAngles = m_LastCmdInfo.GetViewAngles();
			outinfo.localViewAngles = m_LastCmdInfo.GetLocalViewAngles();
		}
		else
		{
			outinfo.viewOrigin = startorigin + frac * ( destorigin - startorigin );

			Quaternion src, dest;
			Quaternion result;

			AngleQuaternion( prev.info.GetViewAngles(), src );
			AngleQuaternion( next.info.GetViewAngles(), dest );
			QuaternionSlerp( src, dest, frac, result );

			QuaternionAngles( result, outinfo.viewAngles );

			AngleQuaternion( prev.info.GetLocalViewAngles(), src );
			AngleQuaternion( next.info.GetLocalViewAngles(), dest );
			QuaternionSlerp( src, dest, frac, result );

			QuaternionAngles( result, outinfo.localViewAngles );
		}
	}
	else if ( bHasValidData )
	{
		// don't interpolate, just copy values
		outinfo.viewOrigin = m_LastCmdInfo.GetViewOrigin();
		outinfo.viewAngles = m_LastCmdInfo.GetViewAngles();
		outinfo.localViewAngles = m_LastCmdInfo.GetLocalViewAngles();
	}

	m_nPreviousTick = nTargetTick;

	// let any demo system override view ( drive, editor, smoother etc)
	bHasValidData |= OverrideView( outinfo );

	if ( !bHasValidData )
		return; // no validate data & no override, exit

	g_pClientSidePrediction->SetViewOrigin( outinfo.viewOrigin );
	g_pClientSidePrediction->SetViewAngles( outinfo.viewAngles );
	g_pClientSidePrediction->SetLocalViewAngles( outinfo.localViewAngles );
#ifndef SWDS
	VectorCopy( outinfo.viewAngles, cl.viewangles );
#endif

	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoPlayer::IsPlayingTimeDemo( void )
{
	return m_bTimeDemo && m_bPlayingBack;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoPlayer::IsPlayingBack( void )
{
	return m_bPlayingBack;
}

CDemoPlayer::CDemoPlayer()
{
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;
	m_bTimeDemo = false;	
	m_nTimeDemoStartFrame = -1;	
	m_flTimeDemoStartTime = 0.0f;	
	m_flTotalFPSVariability = 0.0f;
	m_nTimeDemoCurrentFrame = -1; 
	m_bPlayingBack = false;
	m_bLoading = false;
	m_bPlaybackPaused = false;
	m_nSkipToTick = -1;
	m_nSkipPacketsPlayed = 0;
	m_nSnapshotTick = 0;
	m_SnapshotFilename[0] = 0;
	m_bResetInterpolation = false;
	m_nPreviousTick = 0;
	m_nEndTick = 0;
}

CDemoPlayer::~CDemoPlayer()
{
	StopPlayback();
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnDemoPlaybackStop();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Start's demo playback
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CDemoPlayer::StartPlayback( const char *filename, bool bAsTimeDemo )
{
	m_bLoading = true;

	SCR_BeginLoadingPlaque();

	// Disconnect from server or stop running one
	int oldn = cl.demonum;
	cl.demonum = -1;
	Host_Disconnect(false);
	cl.demonum = oldn;

	if ( !m_DemoFile.Open( filename, true )  )
	{
		cl.demonum = -1;		// stop demo loop
		return false;
	}

	// Read in the m_DemoHeader
	demoheader_t *dh = m_DemoFile.ReadDemoHeader();

	if ( !dh )
	{
		ConMsg( "Failed to read demo header.\n" );
		m_DemoFile.Close();
		cl.demonum = -1;
		return false;
	}
	
	ConMsg ("Playing demo from %s.\n", filename);

	// Now read in the directory structure.
	m_bPlayingBack = true;
	cl.m_nSignonState= SIGNONSTATE_CONNECTED;

	ResyncDemoClock(); 

	// create a fake channel with a NULL address
	cl.m_NetChannel = NET_CreateNetChannel( NS_CLIENT, NULL, "DEMO", &cl, false, dh->networkprotocol );

	if ( !cl.m_NetChannel )
	{
		ConMsg ("CDemo::Play: failed to create demo net channel\n" );
		m_DemoFile.Close();
		cl.demonum = -1;		// stop demo loop
		Host_Disconnect(true);
	}
	
	cl.m_NetChannel->SetTimeout( -1.0f );	// never timeout
	
	Q_memset( &m_DemoPacket, 0, sizeof(m_DemoPacket) );

	// setup demo packet data buffer
	m_DemoPacket.data = new unsigned char[NET_MAX_PAYLOAD];
	m_DemoPacket.from.SetType( NA_LOOPBACK);
		
	cl.chokedcommands = 0;
	cl.lastoutgoingcommand = -1;
 	cl.m_flNextCmdTime = net_time;

	m_bTimeDemo = bAsTimeDemo;
	m_nTimeDemoCurrentFrame = -1;
	m_nTimeDemoStartFrame = -1;

	if ( m_bTimeDemo )
	{
		SeedRandomNumberGenerator( true );
	}

	demoaction->StartPlaying( filename );

	// m_bFastForwarding = false;
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;

	scr_demo_override_fov = 0.0f;

	m_bLoading = false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCurTime - 
//-----------------------------------------------------------------------------
void CDemoPlayer::MarkFrame( float flFPSVariability )
{
	m_flTotalFPSVariability += flFPSVariability;
}

void CDemoPlayer::WriteTimeDemoResults( void )
{
	int		frames;
	float	time;
	frames = (host_framecount - m_nTimeDemoStartFrame) - 1;
	time = Sys_FloatTime() - m_flTimeDemoStartTime;
	if (!time)
	{
		time = 1;
	}
	float flVariability = (m_flTotalFPSVariability / (float)frames);
	ConMsg ("%i frames %5.3f seconds %5.2f fps (%5.2f ms/f) %5.3f fps variability\n", frames, time, frames/time, 1000*time/frames, flVariability );
	bool bFileExists = g_pFileSystem->FileExists( "SourceBench.csv" );
	FileHandle_t fileHandle = g_pFileSystem->Open( "SourceBench.csv", "a+" );
	int width, height;
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetWindowSize( width, height );

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	if( !bFileExists )
	{
		g_pFileSystem->FPrintf( fileHandle, "demofile," );
		g_pFileSystem->FPrintf( fileHandle, "fps," );
		g_pFileSystem->FPrintf( fileHandle, "framerate variability," );
		g_pFileSystem->FPrintf( fileHandle, "totaltime," );
		g_pFileSystem->FPrintf( fileHandle, "numframes," );
		g_pFileSystem->FPrintf( fileHandle, "width," );
		g_pFileSystem->FPrintf( fileHandle, "height," );
		g_pFileSystem->FPrintf( fileHandle, "windowed," );
		g_pFileSystem->FPrintf( fileHandle, "vsync," );
		g_pFileSystem->FPrintf( fileHandle, "MSAA," );
		g_pFileSystem->FPrintf( fileHandle, "Aniso," );
		g_pFileSystem->FPrintf( fileHandle, "dxlevel," );
		g_pFileSystem->FPrintf( fileHandle, "cmdline," );
		g_pFileSystem->FPrintf( fileHandle, "driver name," );
		g_pFileSystem->FPrintf( fileHandle, "vendor id," );
		g_pFileSystem->FPrintf( fileHandle, "device id," );

//		g_pFileSystem->FPrintf( fileHandle, "sound," );
		g_pFileSystem->FPrintf( fileHandle, "Reduce fillrate," );
		g_pFileSystem->FPrintf( fileHandle, "reflect entities," );
		g_pFileSystem->FPrintf( fileHandle, "motion blur," );
		g_pFileSystem->FPrintf( fileHandle, "flashlight shadows," );
		g_pFileSystem->FPrintf( fileHandle, "mat_reduceparticles," );
		g_pFileSystem->FPrintf( fileHandle, "r_dopixelvisibility," );
		g_pFileSystem->FPrintf( fileHandle, "nulldevice," );
		g_pFileSystem->FPrintf( fileHandle, "timedemo_comment," );
		g_pFileSystem->FPrintf( fileHandle, "\n" );
	}

	ConVarRef mat_vsync( "mat_vsync" );
	ConVarRef mat_antialias( "mat_antialias" );
	ConVarRef mat_forceaniso( "mat_forceaniso" );
	ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );
	ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
	ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );
	ConVarRef mat_reducefillrate( "mat_reducefillrate" );
	ConVarRef mat_reduceparticles( "mat_reduceparticles" );
	ConVarRef r_dopixelvisibility( "r_dopixelvisibility" );

	g_pFileSystem->Seek( fileHandle, 0, FILESYSTEM_SEEK_TAIL );
	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );
	g_pFileSystem->FPrintf( fileHandle, "%s,", m_DemoFile.m_szFileName );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", frames/time );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", flVariability );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", time );
	g_pFileSystem->FPrintf( fileHandle, "%i,", frames );
	g_pFileSystem->FPrintf( fileHandle, "%i,", width );
	g_pFileSystem->FPrintf( fileHandle, "%i,", height );
	g_pFileSystem->FPrintf( fileHandle, "%s,", config.Windowed() ? "windowed" : "fullscreen");
	g_pFileSystem->FPrintf( fileHandle, "%s,", mat_vsync.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%d,", mat_antialias.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%d,", mat_forceaniso.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%s,", COM_DXLevelToString( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() ) );
	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->GetCmdLine() );
	g_pFileSystem->FPrintf( fileHandle, "%s,", info.m_pDriverName );
	g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_VendorID );
	g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_DeviceID );

//	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->CheckParm( "-nosound" ) ? "off" : "on" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", mat_reducefillrate.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", r_waterforcereflectentities.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", mat_motion_blur_enabled.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", r_flashlightdepthtexture.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", mat_reduceparticles.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", r_dopixelvisibility.GetBool() ? "on" : "off" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->CheckParm( "-nulldevice" ) ? "yes" : "no" );

	int itimedemo_comment = CommandLine()->FindParm( "-timedemo_comment" );
	const char *timedemo_comment = itimedemo_comment ? CommandLine()->GetParm( itimedemo_comment + 1 ) : "";
	g_pFileSystem->FPrintf( fileHandle, "%s,", timedemo_comment );
	g_pFileSystem->FPrintf( fileHandle, "\n" );
	g_pFileSystem->Close( fileHandle );
}


void CDemoPlayer::PausePlayback( float seconds  )
{
	m_bPlaybackPaused = true;
	
	if ( seconds > 0.0f )
	{
		// Use true clock since everything else is frozen
		m_flAutoResumeTime = Sys_FloatTime() + seconds;
	}
	else
	{
		m_flAutoResumeTime = 0.0f;
	}
}

void CDemoPlayer::ResumePlayback()
{
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;
}

bool CDemoPlayer::CheckPausedPlayback()
{
	if ( demo_pauseatservertick.GetInt() > 0 )
	{
		if ( cl.GetServerTickCount() >= demo_pauseatservertick.GetInt() )
		{
			PausePlayback( -1 );
			ETWMark1I( "DemoPlayer: Reached pause tick", cl.GetServerTickCount() );
			m_nSkipToTick = -1;
			demo_pauseatservertick.SetValue( 0 );
			Msg( "Demo paused at server tick %i\n", cl.GetServerTickCount() );
		}
	}
	
	if ( IsSkipping() )
	{
		if ( ( m_nSkipToTick > GetPlaybackTick() ) ||
			 ( ( m_nSkipToTick & SKIP_TO_TICK_FLAG ) == SKIP_TO_TICK_FLAG ) )
		{
			// we are skipping
			return false;
		}
		else
		{
			// we can't skip back (or finished skipping), so disable skipping
			ETWMark1I( "DemoPlayer: SkipToTick done", GetPlaybackTick() );
			m_nSkipToTick = -1;
		}
	}

	if ( !IsPlaybackPaused() )
		return false;

	if ( m_bPlaybackPaused )
	{
		if ( (m_flAutoResumeTime > 0.0f) &&
			 (Sys_FloatTime() >= m_flAutoResumeTime) )
		{
			// it's time to unpause replay
			ResumePlayback();
		}
	}

	return m_bPlaybackPaused;
}

bool CDemoPlayer::IsPlaybackPaused()
{
	if ( !IsPlayingBack() )
		return false;

	// never pause while reading signon data
	if ( m_nTimeDemoCurrentFrame < 0 )
		return false;

	// If skipping then do not pretend paused
	if ( IsSkipping() )
		return false;
	
	return m_bPlaybackPaused;
}

int CDemoPlayer::GetPlaybackStartTick( void )
{
	return m_nStartTick;
}

int CDemoPlayer::GetPlaybackTick( void )
{
	return host_tickcount - m_nStartTick;
}

void CDemoPlayer::ResyncDemoClock()
{
	m_nStartTick = host_tickcount;
	m_nPreviousTick = m_nStartTick;
}

float CDemoPlayer::GetPlaybackTimeScale()
{
	return m_flPlaybackRateModifier;
}

void CDemoPlayer::SetPlaybackTimeScale(float timescale)
{
	m_flPlaybackRateModifier = timescale;
}

void CDemoPlayer::SetBenchframe( int tick, const char *filename )
{
	m_nSnapshotTick = tick;

	if ( filename )
	{
		Q_strncpy( m_SnapshotFilename, filename, sizeof(m_SnapshotFilename) );
	}
}

static bool ComputeNextIncrementalDemoFilename( char *name, int namesize )
{
	FileHandle_t test;
	
	test = g_pFileSystem->Open( name, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == test )
	{
		// file doesn't exist, so we can use that 
		return true;
	}
	g_pFileSystem->Close( test );

	char basename[ MAX_OSPATH ];

	Q_StripExtension( name, basename, sizeof( basename ) );

	// Start looking for a valid name
	int i = 0;
	for ( i = 0; i < 1000; i++ )
	{
		char newname[ MAX_OSPATH ];
		Q_snprintf( newname, sizeof( newname ), "%s%03i.dem", basename, i );

		test = g_pFileSystem->Open( newname, "rb" );
		if ( FILESYSTEM_INVALID_HANDLE == test )
		{
			Q_strncpy( name, newname, namesize );
			return true;
		}
		g_pFileSystem->Close( test );
	}

	ConMsg( "Unable to find a valid incremental demo filename for %s, try clearing the directory of %snnn.dem\n", name, basename );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: List the contents of a demo file.
//-----------------------------------------------------------------------------
void CL_ListDemo_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	// Find the file
	char name[MAX_OSPATH];

	Q_snprintf (name, sizeof(name), "%s", args[1]);
	
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	ConMsg ("Demo contents for %s:\n", name);

	CDemoFile demofile;

	if ( !demofile.Open( name, true ) )
	{
		ConMsg ("ERROR: couldn't open.\n");
		return;
	}

	demofile.ReadDemoHeader();

	demoheader_t *header = &demofile.m_DemoHeader;

	if ( !header )
	{
		ConMsg( "Failed reading demo header.\n" );
		demofile.Close();
		return;
	}
	
	if ( Q_strcmp ( header->demofilestamp, DEMO_HEADER_ID ) )
	{
		ConMsg( "%s is not a valid demo file\n", name);
		return;
	}

	ConMsg("Network protocol: %i\n", header->networkprotocol);
	ConMsg("Demo version    : %i\n", header->demoprotocol);
	ConMsg("Server name     : %s\n", header->servername);
	ConMsg("Map name        : %s\n", header->mapname);
	ConMsg("Game            : %s\n", header->gamedirectory);
	ConMsg("Player name     : %s\n", header->clientname);
	ConMsg("Time            : %.1f\n", header->playback_time);
	ConMsg("Ticks           : %i\n", header->playback_ticks);
	ConMsg("Frames          : %i\n", header->playback_frames);
	ConMsg("Signon size     : %i\n", header->signonlength);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( stop, "Finish recording demo." )
{
	if ( cmd_source != src_command )
		return;

	if ( !demorecorder->IsRecording() )
	{
		ConDMsg ("Not recording a demo.\n");
		return;
	}

	demorecorder->StopRecording();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND_F( record, "Record a demo.", FCVAR_DONTRECORD )
{
	if ( g_ClientDLL == NULL )
	{
		ConMsg ("Can't record on dedicated server.\n");
		return;
	}	

	if ( args.ArgC() != 2 && args.ArgC() != 3 )
	{
		ConMsg ("record <demoname> [incremental]\n");
		return;
	}

	bool incremental = false;
	if ( args.ArgC() == 3 )
	{
		if ( !Q_stricmp( args[2], "incremental" ) )
		{
			incremental = true;
		}
	}
	
	if ( demorecorder->IsRecording() )
	{
		ConMsg ("Already recording.\n");
		return;
	}

	if ( demoplayer->IsPlayingBack() )
	{
		ConMsg ("Can't record during demo playback.\n");
		return;
	}

	// check path first
	if ( !COM_IsValidPath( args[1] ) )
	{
		ConMsg( "record %s: invalid path.\n", args[1] );
		return;
	}

	char	name[ MAX_OSPATH ];

	if ( !g_ClientDLL->CanRecordDemo( name, sizeof( name ) ) )
	{
		ConMsg( "%s\n", name );	// re-use name as the error string if the client prevents us from starting a demo
		return;
	}

	// remove .dem extension if user added it
	Q_StripExtension( args[1], name, sizeof( name ) );
	
	if ( incremental )
	{
		// If file exists, construct a better name
		if ( !ComputeNextIncrementalDemoFilename( name, sizeof( name ) ) )
		{
			return;
		}
	}
	// Record it
	demorecorder->StartRecording( name, incremental );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_PlayDemo_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	if ( args.ArgC() != 2 )
	{
		ConMsg ("playdemo <demoname> : plays a demo file\n");
		return;
	}

	// Get the demo filename
	char name[ MAX_OSPATH ];
	Q_strncpy( name, args[1], sizeof( name ) );
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	// set current demo player to replay demo player?
	demoplayer = g_pClientDemoPlayer;

	//
	// open the demo file
	//
	if ( demoplayer->StartPlayback( name, false ) )
	{
		// Remove extension
		char basename[ MAX_OSPATH ];
		V_StripExtension( name, basename, sizeof( basename ) );
		g_ClientDLL->OnDemoPlaybackStart( basename );
	}
	else
	{
		SCR_EndLoadingPlaque();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_TimeDemo_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	if ( args.ArgC() < 2 || args.ArgC() > 4 )
	{
		ConMsg ("timedemo <demoname> <optional stats.txt> : gets demo speeds, starting from optional frame\n");
		return;
	}

	if( args.ArgC() >= 3 )
	{
		Q_strncpy( g_pStatsFile, args[ 2 ], sizeof( g_pStatsFile ) );
	}
	else
	{
		Q_strncpy( g_pStatsFile, "UNKNOWN", sizeof( g_pStatsFile ) );
	}

	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;
	
	// open the demo file
	char name[ MAX_OSPATH ];
	Q_strncpy (name, args[1], sizeof( name ) );
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	if ( !demoplayer->StartPlayback( name, true ) )
	{
		SCR_EndLoadingPlaque();
	}
}

void CL_TimeDemoQuit_f( const CCommand &args )
{
	demo_quitafterplayback.SetValue( 1 );
	CL_TimeDemo_f( args );
}

void CL_BenchFrame_f( const CCommand &args )
{
	if ( cmd_source != src_command )
		return;

	if ( args.ArgC() != 4 )
	{
		ConMsg ("benchframe <demoname> <frame> <tgafilename>: takes a snapshot of a particular frame in a demo\n");
		return;
	}

	g_pClientDemoPlayer->SetBenchframe( max( 0, atoi( args[2] ) ), args[3] );

	s_bBenchframe = true;

	mat_norendering.SetValue( 1 );
	
	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;
	
	// open the demo file
	char name[ MAX_OSPATH ];
	Q_strncpy (name, args[1], sizeof( name ) );
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	if ( !demoplayer->StartPlayback( name, true ) )
	{
		SCR_EndLoadingPlaque();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( vtune, "Controls VTune's sampling." )
{
 	if ( args.ArgC() != 2 )
	{
		ConMsg ("vtune \"pause\" | \"resume\" : Suspend or resume VTune's sampling.\n");
		return;
	}
	
	if( !Q_strcasecmp( args[1], "pause" ) )
	{
		if(!vtune(false))
		{
			ConMsg("Failed to find \"VTPause()\" in \"vtuneapi.dll\".\n");
			return;
		}

		ConMsg("VTune sampling paused.\n");
	}

	else if( !Q_strcasecmp( args[1], "resume" ) )
	{
		if(!vtune(true))
		{
			ConMsg("Failed to find \"VTResume()\" in \"vtuneapi.dll\".\n");
			return;
		}
		
		ConMsg("VTune sampling resumed.\n");
	}

	else
	{
		ConMsg("Unknown vtune option.\n");
	}

}



CON_COMMAND_AUTOCOMPLETEFILE( playdemo, CL_PlayDemo_f, "Play a recorded demo file (.dem ).", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( timedemo, CL_TimeDemo_f, "Play a demo and report performance info.", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( timedemoquit, CL_TimeDemoQuit_f, "Play a demo, report performance info, and then exit", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( listdemo, CL_ListDemo_f, "List demo file contents.", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( benchframe, CL_BenchFrame_f, "Takes a snapshot of a particular frame in a time demo.", NULL, dem );

CON_COMMAND( demo_pause, "Pauses demo playback." )
{
	float seconds = -1.0;

	if ( args.ArgC() == 2 )
	{
		seconds = atof( args[1] );
	}

	demoplayer->PausePlayback( seconds );
}

CON_COMMAND( demo_resume, "Resumes demo playback." )
{
	demoplayer->ResumePlayback();
}

CON_COMMAND( demo_togglepause, "Toggles demo playback." )
{
	if ( !demoplayer->IsPlayingBack() )
		return;
	
	if ( demoplayer->IsPlaybackPaused() )
	{
		demoplayer->ResumePlayback();
	}
	else
	{
		demoplayer->PausePlayback( -1 );
	}
}

CON_COMMAND( demo_gototick, "Skips to a tick in demo." )
{
	bool bRelative = false;
	bool bPause = false;

	if ( args.ArgC() < 2 )
	{
		Msg("Syntax: demo_gototick <tick> [relative] [pause]\n");
		return;
	}
	
	int nTick = atoi( args[1] );
	
	if ( args.ArgC() >= 3 )
	{
		bRelative = Q_atoi( args[2] ) != 0;
	}

	if ( args.ArgC() >= 4 )
	{
		bPause = Q_atoi( args[3] ) != 0;
	}

	demoplayer->SkipToTick( nTick, bRelative, bPause );
}

CON_COMMAND( demo_setendtick, "Sets end demo playback tick. Set to 0 to disable." )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Syntax: demo_setendtick <tick>\n" );
		return;
	}

	int nTick = atoi( args[1] );

	demoplayer->SetEndTick( nTick );
}

CON_COMMAND( demo_timescale, "Sets demo replay speed." )
{
	float fScale = 1.0f;

	if ( args.ArgC() == 2 )
	{
		fScale = atof( args[1] );
		fScale = clamp( fScale, 0.0f, 100.0f );
	}

	demoplayer->SetPlaybackTimeScale( fScale );
}

bool CDemoPlayer::OverrideView( democmdinfo_t& info )
{
	if ( g_pDemoUI && g_pDemoUI->OverrideView( info, GetPlaybackTick() ) )
		return true;

	if ( g_pDemoUI2 && g_pDemoUI2->OverrideView( info, GetPlaybackTick() ) )
		return true;

	if ( demoaction && demoaction->OverrideView( info, GetPlaybackTick() ) )
		return true;

	return false;
}

void CDemoPlayer::OnStopCommand()
{
	cl.Disconnect( "Demo stopped", true);
}

void CDemoPlayer::ResetDemoInterpolation( void )
{
	m_bResetInterpolation = true;
}

int CDemoPlayer::GetProtocolVersion()
{
	Assert( IsPlayingBack() );
	if ( !IsPlayingBack() )
		return PROTOCOL_VERSION;

	return m_DemoFile.GetProtocolVersion();
}
