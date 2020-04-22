//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <utlbuffer.h>
#include "tooldemofile.h"
#include "filesystem.h"
#include "demofile/demoformat.h"

extern IBaseFileSystem *g_pFileSystem;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CToolDemoFile::CToolDemoFile()
{
	m_hDemoFile = FILESYSTEM_INVALID_HANDLE;
}

CToolDemoFile::~CToolDemoFile()
{
	if ( IsOpen() )
	{
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolDemoFile::ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOut)
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );

	g_pFileSystem->Read( &nSeqNrIn, sizeof(int), m_hDemoFile );
	g_pFileSystem->Read( &nSeqNrOut, sizeof(int), m_hDemoFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolDemoFile::ReadCmdInfo( democmdinfo_t& info )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );
	g_pFileSystem->Read( &info, sizeof( democmdinfo_t ), m_hDemoFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cmd - 
//			dt - 
//			frame - 
//-----------------------------------------------------------------------------
void CToolDemoFile::ReadCmdHeader( unsigned char& cmd, int& tick )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );

	// Read the command
	int r = g_pFileSystem->Read ( &cmd, sizeof(byte), m_hDemoFile );
	
	if ( r <=0 )
	{
		Warning("Missing end tag in demo file.\n");
		cmd = dem_stop;
		return;
	}

	Assert( cmd >= 1 && cmd <= dem_lastcmd );

	// Read the timestamp
	g_pFileSystem->Read ( &tick, sizeof(int), m_hDemoFile );
	
	tick = LittleDWord( tick );
}

const char *CToolDemoFile::ReadConsoleCommand()
{
	static char cmdstring[1024];
	
	ReadRawData( cmdstring, sizeof(cmdstring) );

	return cmdstring;
}

unsigned int CToolDemoFile::GetCurPos()
{
	if ( m_hDemoFile == FILESYSTEM_INVALID_HANDLE )
		return 0;

	return g_pFileSystem->Tell( m_hDemoFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : expected_length - 
//			&demofile - 
//-----------------------------------------------------------------------------
int CToolDemoFile::ReadNetworkDataTables( CUtlBuffer *buf )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );
	char data[ 1024 ];
	int length;

	g_pFileSystem->Read( &length, sizeof( int ), m_hDemoFile );

	while( length > 0 )
	{
		int chunk = min( length, 1024 );
		g_pFileSystem->Read( data, chunk, m_hDemoFile );
		length -= chunk;

		if ( buf )
		{
			buf->Put( data, chunk );
		}
	}

	return length;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : discard - 
//-----------------------------------------------------------------------------
int CToolDemoFile::ReadUserCmd( char *buffer, int &size )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );

	int outgoing_sequence;
	
	g_pFileSystem->Read( &outgoing_sequence, sizeof( int ), m_hDemoFile );
	
	size = ReadRawData( buffer, size );

	return outgoing_sequence;
}

//-----------------------------------------------------------------------------
// Purpose: Rewind from the current spot by the time stamp, byte code and frame counter offsets
//-----------------------------------------------------------------------------
void CToolDemoFile::SeekTo( int position )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );
	g_pFileSystem->Seek( m_hDemoFile, position, FILESYSTEM_SEEK_HEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CToolDemoFile::ReadRawData( char *buffer, int length )
{
	Assert( m_hDemoFile != FILESYSTEM_INVALID_HANDLE );
	
	int size;
	
	// read length of data block
	g_pFileSystem->Read( &size, sizeof( int ), m_hDemoFile );

	if ( buffer && (length < size) )
	{
		DevMsg("CToolDemoFile::ReadRawData: buffe overflow (%i).\n", size );
		return -1;
	}

	if ( buffer )
	{
		if ( length < size )
		{
			// given buffer is too small
			DevMsg("CToolDemoFile::ReadRawData: buffe overflow (%i).\n", size );
			g_pFileSystem->Seek( m_hDemoFile, size, FILESYSTEM_SEEK_CURRENT );
			size = -1;
		}
		else
		{
			// read data into buffer
			int r = g_pFileSystem->Read( buffer, size, m_hDemoFile );
			if ( r != size )
			{
				Warning( "Error reading demo message data.\n");
				return -1;
			}
		}
	}
	else
	{
		// just skip it
		g_pFileSystem->Seek( m_hDemoFile, size, FILESYSTEM_SEEK_CURRENT );
	}

	return size;
}

demoheader_t *CToolDemoFile::ReadDemoHeader()
{
	if ( m_hDemoFile == FILESYSTEM_INVALID_HANDLE )
		return NULL;	// file not open
	
	// goto file start
	g_pFileSystem->Seek(m_hDemoFile, 0, FILESYSTEM_SEEK_HEAD);

	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) );

	int r = g_pFileSystem->Read( &m_DemoHeader, sizeof(demoheader_t), m_hDemoFile );

	if ( r != sizeof(demoheader_t) )
		return NULL;  // reading failed

	if ( Q_strcmp ( m_DemoHeader.demofilestamp, DEMO_HEADER_ID ) )
	{
		Warning( "%s has invalid demo header ID.\n", m_szFileName );
		return NULL;
	}

	/*
// The current network protocol version.  Changing this makes clients and servers incompatible
#define PROTOCOL_VERSION    7

	if ( m_DemoHeader.networkprotocol != PROTOCOL_VERSION )
	{
		Warning ("ERROR: demo network protocol %i outdated, engine version is %i \n", 
			m_DemoHeader.networkprotocol, PROTOCOL_VERSION );

		return NULL;
	}
	*/

	if ( m_DemoHeader.demoprotocol != DEMO_PROTOCOL )
	{
		Warning ("ERROR: demo file protocol %i outdated, engine version is %i \n", 
			m_DemoHeader.demoprotocol, DEMO_PROTOCOL );

		return NULL;
	}

	return &m_DemoHeader;
}

bool CToolDemoFile::Open(const char *name, bool bReadOnly)
{
	if ( m_hDemoFile != FILESYSTEM_INVALID_HANDLE )
	{
		Warning ("CToolDemoFile::Open: file already open.\n");
		return false;
	}

	m_szFileName[0] = 0;  // clear name
	Q_memset( &m_DemoHeader, 0, sizeof(m_DemoHeader) ); // and demo header

	if ( bReadOnly )
	{
		// open existing file for reading only
		m_hDemoFile = g_pFileSystem->Open (name, "rb", "GAME" );
	}
	else
	{
		// create new file for writing only
		m_hDemoFile = g_pFileSystem->Open (name, "wb", "GAME" );
	}

	if ( m_hDemoFile == FILESYSTEM_INVALID_HANDLE )
	{
		Warning ("CToolDemoFile::Open: couldn't open file %s for %s.\n", 
			name, bReadOnly?"reading":"writing" );
		return false;
	}

	Q_strncpy( m_szFileName, name, sizeof(m_szFileName) );

	return true;
}

bool CToolDemoFile::IsOpen()
{
	return m_hDemoFile != FILESYSTEM_INVALID_HANDLE;
}

void CToolDemoFile::Close()
{
	if ( m_hDemoFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close(m_hDemoFile);
		m_hDemoFile = FILESYSTEM_INVALID_HANDLE;
	}
}

int CToolDemoFile::GetSize()
{
	return g_pFileSystem->Size( m_hDemoFile );
}

