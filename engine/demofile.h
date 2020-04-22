//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef DEMOFILE_H
#define DEMOFILE_H
#ifdef _WIN32
#pragma once
#endif

#define DEMO_FILE_UTLBUFFER 1
#define DEMO_FILE_MAX_STRINGTABLE_SIZE 5000000 // 5 mb

#include "demo.h"

#ifdef DEMO_FILE_UTLBUFFER
#include "tier2/utlstreambuffer.h"
#else
#include <filesystem.h>
#endif


#include "tier1/bitbuf.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDemoBuffer;

//-----------------------------------------------------------------------------
// Demo file 
//-----------------------------------------------------------------------------
class CDemoFile  
{
public:
	CDemoFile();
	~CDemoFile();

	bool	Open(const char *name, bool bReadOnly, bool bMemoryBuffer = false, int nBufferSize = 0, bool bAllowHeaderWrite = true);
	bool	IsOpen();
	void	Close();

	void	SeekTo( int position, bool bRead );
	unsigned int GetCurPos( bool bRead );
	int		GetSize();

	void	WriteRawData( const char *buffer, int length );
	int		ReadRawData( char *buffer, int length );

	void	WriteSequenceInfo(int nSeqNrIn, int nSeqNrOutAck);
	void	ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOutAck);

	void	WriteCmdInfo( democmdinfo_t& info );
	void	ReadCmdInfo( democmdinfo_t& info );

	void	WriteCmdHeader( unsigned char cmd, int tick );
	void	ReadCmdHeader( unsigned char& cmd, int& tick );
	
	void	WriteConsoleCommand( const char *cmd, int tick );
	const char *ReadConsoleCommand( void );

	void	WriteNetworkDataTables( bf_write *buf, int tick );
	int		ReadNetworkDataTables( bf_read *buf );
	
	void	WriteStringTables( bf_write *buf, int tick );
	int		ReadStringTables( bf_read *buf );

	void	WriteUserCmd( int cmdnumber, const char *buffer, unsigned char bytes, int tick );
	int		ReadUserCmd( char *buffer, int &size );

	void	WriteDemoHeader();
	demoheader_t *ReadDemoHeader();

	void	WriteFileBytes( FileHandle_t fh, int length );

	// Returns the PROTOCOL_VERSION used when .dem was recorded
	int		GetProtocolVersion();
public:
	char			m_szFileName[MAX_PATH];	//name of current demo file
	demoheader_t    m_DemoHeader;  //general demo info
	CUtlBuffer		*m_pBuffer;
	bool			m_bAllowHeaderWrite;
	bool			m_bIsStreamBuffer;
};

#endif // DEMOFILE_H
