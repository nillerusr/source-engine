//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <tier0/dbg.h>
#include <tier1/strtools.h>
#include <utlbuffer.h>

#include "demofile.h"
#include "filesystem_engine.h"
#include "demo.h"
#include "proto_version.h"
#include "convar.h"	// For dbg_demofile

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


void Host_EndGame (bool bShowMainMenu, const char *message, ...);

// Debug helpers - this class prints in a nested format
ConVar dbg_demofile( "dbg_demofile", "0", FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN );
//#define DEMOFILE_DBG_PRINT
#if defined( DEMOFILE_DBG_PRINT )
class CDbgPrint
{
public:
	static int s_nIndent;
	CDbgPrint( const char *pMsg )
	{
		++s_nIndent;
		if ( dbg_demofile.GetInt() )
		{
			for (int i = 0; i < 3*s_nIndent; ++i)
				DevMsg(" ");
			DevMsg( pMsg );
		}
	}
	~CDbgPrint() { --s_nIndent; }
};
int CDbgPrint::s_nIndent = 0;
#define DemoFileDbg(_txt) CDbgPrint printer( _txt )
#else
#define DemoFileDbg(_txt) (void)0
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoFile::CDemoFile() :
	m_pBuffer( NULL ),
	m_bAllowHeaderWrite( true ),
	m_bIsStreamBuffer( false )
{
}

CDemoFile::~CDemoFile()
{
	if ( IsOpen() )
	{
		Close();
	}
}

void CDemoFile::WriteSequenceInfo(int nSeqNrIn, int nSeqNrOut)
{
	DemoFileDbg( "WriteSequenceInfo()\n" );
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	m_pBuffer->PutInt( nSeqNrIn );
	m_pBuffer->PutInt( nSeqNrOut );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOut)
{
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	nSeqNrIn = m_pBuffer->GetInt( );
	nSeqNrOut = m_pBuffer->GetInt( );
}


inline void ByteSwap_democmdinfo_t( democmdinfo_t &swap )
{
	swap.flags = LittleDWord( swap.flags );

	LittleFloat( &swap.viewOrigin.x, &swap.viewOrigin.x );
	LittleFloat( &swap.viewOrigin.y, &swap.viewOrigin.y );
	LittleFloat( &swap.viewOrigin.z, &swap.viewOrigin.z );

	LittleFloat( &swap.viewAngles.x, &swap.viewAngles.x );
	LittleFloat( &swap.viewAngles.y, &swap.viewAngles.y );
	LittleFloat( &swap.viewAngles.z, &swap.viewAngles.z );

	LittleFloat( &swap.localViewAngles.x, &swap.localViewAngles.x );
	LittleFloat( &swap.localViewAngles.y, &swap.localViewAngles.y );
	LittleFloat( &swap.localViewAngles.z, &swap.localViewAngles.z );

	LittleFloat( &swap.viewOrigin2.x, &swap.viewOrigin2.x );
	LittleFloat( &swap.viewOrigin2.y, &swap.viewOrigin2.y );
	LittleFloat( &swap.viewOrigin2.z, &swap.viewOrigin2.z );

	LittleFloat( &swap.viewAngles2.x, &swap.viewAngles2.x );
	LittleFloat( &swap.viewAngles2.y, &swap.viewAngles2.y );
	LittleFloat( &swap.viewAngles2.z, &swap.viewAngles2.z );

	LittleFloat( &swap.localViewAngles2.x, &swap.localViewAngles2.x );
	LittleFloat( &swap.localViewAngles2.y, &swap.localViewAngles2.y );
	LittleFloat( &swap.localViewAngles2.z, &swap.localViewAngles2.z );
}

void CDemoFile::WriteCmdInfo( democmdinfo_t& info )
{
	DemoFileDbg( "WriteCmdInfo()\n" );
	democmdinfo_t littleEndianInfo = info;
	ByteSwap_democmdinfo_t( littleEndianInfo );

	Assert( m_pBuffer && m_pBuffer->IsValid() );
	m_pBuffer->Put( &littleEndianInfo, sizeof(democmdinfo_t) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoFile::ReadCmdInfo( democmdinfo_t& info )
{
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	m_pBuffer->Get( &info, sizeof(democmdinfo_t) );

	ByteSwap_democmdinfo_t( info );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			*fp - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteCmdHeader( unsigned char cmd, int tick )
{
	if ( dbg_demofile.GetInt() ) DevMsg( "----------------------------------------\n" );
	Assert( cmd >= dem_signon && cmd <= dem_lastcmd );

	Assert( m_pBuffer && m_pBuffer->IsValid() );
	m_pBuffer->PutUnsignedChar( cmd );
	m_pBuffer->PutInt( tick );

	static const char *cmdname[] = 
	{
		"dem_unknown",
		"dem_signon",
		"dem_packet",
		"dem_synctick",
		"dem_consolecmd",
		"dem_usercmd",
		"dem_datatables",
		"dem_stop",
		"dem_stringtables"
	};

	DemoFileDbg( "WriteCmdHeader()..." );
	if ( dbg_demofile.GetInt() ) DevMsg( "tick %i, cmd %s \n", tick, cmdname[cmd] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			dt - 
//			frame - 
//-----------------------------------------------------------------------------
void CDemoFile::ReadCmdHeader( unsigned char& cmd, int& tick )
{
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	cmd = m_pBuffer->GetUnsignedChar( );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		ConDMsg("Missing end tag in demo file.\n");
		cmd = dem_stop;
		return;
	}

	if ( cmd <= 0 || cmd > dem_lastcmd )
	{
		ConDMsg("Unexepcted command token [%d] in .demo file\n", cmd );
		cmd = dem_stop;
		return;
	}

	tick = m_pBuffer->GetInt( );
}

void CDemoFile::WriteConsoleCommand( const char *cmdstring, int tick )
{
	DemoFileDbg( "WriteConsoleCommand()\n" );
	if ( !cmdstring || !cmdstring[0] )
		return;

	if ( !m_pBuffer || !m_pBuffer->IsValid() )
		return;

	int len = Q_strlen( cmdstring ) + 1;
	if ( len >= 1024 )
	{
		DevMsg("CDemoFile::WriteConsoleCommand: command too long (>1024).\n");
		return;
	}

	WriteCmdHeader( dem_consolecmd, tick );

	WriteRawData( cmdstring, len );
}

const char *CDemoFile::ReadConsoleCommand()
{
	static char cmdstring[1024];
	
	ReadRawData( cmdstring, sizeof(cmdstring) );

	return cmdstring;
}

unsigned int CDemoFile::GetCurPos( bool bRead )
{
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
		return 0;
	if ( bRead )
		return m_pBuffer->TellGet();
	return m_pBuffer->TellPut();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteNetworkDataTables( bf_write *buf, int tick  )
{
	DemoFileDbg( "WriteNetworkDataTables()\n" );
	MEM_ALLOC_CREDIT();

	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		DevMsg("CDemoFile::WriteNetworkDataTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_datatables, tick );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : expected_length - 
//			&demofile - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadNetworkDataTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( NULL, 0 ); // skip data
}

void CDemoFile::WriteStringTables( bf_write *buf, int tick )
{
	DemoFileDbg( "WriteStringTables()\n" );
	MEM_ALLOC_CREDIT();

	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		DevMsg("CDemoFile::WriteStringTables: Haven't opened file yet!\n" );
		return;
	}

	WriteCmdHeader( dem_stringtables, tick );

	WriteRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesWritten() );
}

int CDemoFile::ReadStringTables( bf_read *buf )
{
	if ( buf )
		return ReadRawData( (char*)buf->GetBasePointer(), buf->GetNumBytesLeft() );
	return ReadRawData( NULL, 0 ); // skip data
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmdnumber - 
//-----------------------------------------------------------------------------
void CDemoFile::WriteUserCmd( int cmdnumber, const char *buffer, unsigned char bytes, int tick )
{
	DemoFileDbg( "WriteUserCmd()\n" );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
		return;

	WriteCmdHeader( dem_usercmd, tick );

	m_pBuffer->PutInt( cmdnumber );

	WriteRawData( buffer, bytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : discard - 
//-----------------------------------------------------------------------------
int CDemoFile::ReadUserCmd( char *buffer, int &size )
{
	int outgoing_sequence;
	
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	outgoing_sequence = m_pBuffer->GetInt();

	size = ReadRawData( buffer, size );
	return outgoing_sequence;
}

//
// Purpose: Rewind from the current spot by the time stamp, byte code and frame counter offsets
//-----------------------------------------------------------------------------
void CDemoFile::SeekTo( int position, bool bRead )
{
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	if ( bRead )
	{
		m_pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, position );
	}
	else
	{
		m_pBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, position );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CDemoFile::ReadRawData( char *buffer, int length )
{
	int size;
	
	Assert( m_pBuffer && m_pBuffer->IsValid() );
	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		Host_EndGame(true, "Error reading demo message data.\n");
		return -1;
	}

	size = m_pBuffer->GetInt();

	if ( !buffer )
	{
		// just skip it
		m_pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, size );
		return size;
	}

	if ( length < size )
	{
		// given buffer is too small
		DevMsg("CDemoFile::ReadRawData: buffer overflow (%i).\n", size );
		m_pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, -(int)sizeof( int ) ); // rewind our get pointer
		return -1;
	}

	// read data into buffer
	m_pBuffer->Get( buffer, size );

	return size;
}

void CDemoFile::WriteRawData( const char *buffer, int length )
{
	DemoFileDbg( "WriteRawData()\n" );
	MEM_ALLOC_CREDIT();

	Assert( m_pBuffer && m_pBuffer->IsValid() );
	m_pBuffer->PutInt( length );
	m_pBuffer->Put( buffer, length );
}

void CDemoFile::WriteDemoHeader()
{
	if ( !m_bAllowHeaderWrite )
		return;

	DemoFileDbg( "WriteDemoHeader()\n" );
	Assert( m_DemoHeader.networkprotocol == PROTOCOL_VERSION );

	if ( dbg_demofile.GetInt() )
	{
		DevMsg( "\n" );
		DevMsg( "     demofilestamp: %s\n", m_DemoHeader.demofilestamp );
		DevMsg( "     demoprotocol (should be %i): %i\n", DEMO_PROTOCOL, m_DemoHeader.demoprotocol );
		DevMsg( "     networkprotocol (should be %i): %i\n", PROTOCOL_VERSION, m_DemoHeader.networkprotocol );
		DevMsg( "     servername: %s\n", m_DemoHeader.servername );
		DevMsg( "     clientname: %s\n", m_DemoHeader.clientname );
		DevMsg( "     mapname: %s\n", m_DemoHeader.mapname );
		DevMsg( "     gamedirectory: %s\n", m_DemoHeader.gamedirectory );
		DevMsg( "     playback_time: %f\n", m_DemoHeader.playback_time );
		DevMsg( "     playback_ticks: %i\n", m_DemoHeader.playback_ticks );	
		DevMsg( "     playback_frames: %i\n", m_DemoHeader.playback_frames );
		DevMsg( "     signonlength: %i\n", m_DemoHeader.signonlength );
		DevMsg( "\n" );
	}

	// Swaps endianness, goes to file start and writes header
	demoheader_t littleEndianHeader = *((demoheader_t*)&m_DemoHeader);
	ByteSwap_demoheader_t( littleEndianHeader );

	// Goto file start
	m_pBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, 0 );

	// Write
	m_pBuffer->Put( &m_DemoHeader, sizeof( m_DemoHeader ) );
}

demoheader_t *CDemoFile::ReadDemoHeader()
{
	bool bOk;
	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) );

	if ( !m_pBuffer || !m_pBuffer->IsValid() )
		return NULL;
	m_pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_pBuffer->Get( &m_DemoHeader, sizeof(demoheader_t) );
	bOk = m_pBuffer->IsValid();

	ByteSwap_demoheader_t( m_DemoHeader );

	if ( !bOk )
		return NULL;  // reading failed

	if ( Q_strcmp( m_DemoHeader.demofilestamp, DEMO_HEADER_ID ) )
	{
		ConMsg( "%s has invalid demo header ID.\n", m_szFileName );
		return NULL;
	}

	if ( m_DemoHeader.networkprotocol != PROTOCOL_VERSION 
#if defined( DEMO_BACKWARDCOMPATABILITY )
		&&  m_DemoHeader.networkprotocol < PROTOCOL_VERSION_12 
#endif
		)
	{
		ConMsg ("ERROR: demo network protocol %i outdated, engine version is %i \n", 
			m_DemoHeader.networkprotocol, PROTOCOL_VERSION );

		return NULL;
	}

	if ( ( m_DemoHeader.demoprotocol > DEMO_PROTOCOL) ||
		 ( m_DemoHeader.demoprotocol < 2 ) )
	{
		ConMsg ("ERROR: demo file protocol %i outdated, engine vnoteersion is %i \n", 
			m_DemoHeader.demoprotocol, DEMO_PROTOCOL );

		return NULL;
	}

	return &m_DemoHeader;
}

void CDemoFile::WriteFileBytes( FileHandle_t fh, int length )
{
	DemoFileDbg( "WriteFileBytes()\n" );
	int   copysize = length;
	char  copybuf[COM_COPY_CHUNK_SIZE];

	while ( copysize > COM_COPY_CHUNK_SIZE )
	{
		g_pFileSystem->Read ( copybuf, COM_COPY_CHUNK_SIZE, fh );
		m_pBuffer->Put( copybuf, COM_COPY_CHUNK_SIZE );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	g_pFileSystem->Read ( copybuf, copysize, fh );
	m_pBuffer->Put( copybuf, copysize );
	
	g_pFileSystem->Flush ( fh );
}

bool CDemoFile::Open(const char *name, bool bReadOnly, bool bMemoryBuffer, int nBufferSize/*=0*/, bool bAllowHeaderWrite/*=true*/)
{
	if ( m_pBuffer && m_pBuffer->IsValid() )
	{
		ConMsg ("CDemoFile::Open: file already open.\n");
		return false;
	}

	m_szFileName[0] = 0;  // clear name
	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) ); // and demo header

	// This is used by replay, which manually writes a header.
	m_bAllowHeaderWrite = bAllowHeaderWrite;

	if ( bMemoryBuffer )
	{
		Assert( !bReadOnly );	// Only read from files
		Assert( nBufferSize > 0 );
		m_pBuffer = new CUtlBuffer( nBufferSize, nBufferSize, 0 );
		m_bIsStreamBuffer = false;
	}
	else
	{
		m_pBuffer = new CUtlStreamBuffer( name, NULL, bReadOnly ? CUtlBuffer::READ_ONLY : 0, false );
		m_bIsStreamBuffer = true;
	}

	// Demo files are always little endian
	m_pBuffer->SetBigEndian( false );

	if ( !m_pBuffer || !m_pBuffer->IsValid() )
	{
		ConMsg ("CDemoFile::Open: couldn't open file %s for %s.\n", 
			name, bReadOnly?"reading":"writing" );
		Close();
		return false;
	}

	if ( name )
	{
		Q_strncpy( m_szFileName, name, sizeof(m_szFileName) );
	}

	return true;
}

bool CDemoFile::IsOpen()
{
	return m_pBuffer && m_pBuffer->IsValid();
}

void CDemoFile::Close()
{
	// CUtlBuffer base class does NOT have a virtual destructor!
	if ( m_bIsStreamBuffer )
	{
		// Destructor will call Close() as needed
		delete static_cast<CUtlStreamBuffer*>(m_pBuffer);
	}
	else
	{
		delete m_pBuffer;
	}
	m_pBuffer = NULL;
}

int CDemoFile::GetSize()
{
	return m_pBuffer->TellMaxPut();
}

// Returns the PROTOCOL_VERSION used when .dem was recorded
int CDemoFile::GetProtocolVersion()
{
	return m_DemoHeader.networkprotocol;
}
