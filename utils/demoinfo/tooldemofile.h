//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TOOLDEMOFILE_H
#define TOOLDEMOFILE_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "tier0/platform.h"
#include "utlvector.h"
#include "filesystem.h"
#include "demofile/demoformat.h"

class CUtlBuffer;

class CToolDemoFile  
{
public:
	CToolDemoFile();
	virtual ~CToolDemoFile();

	bool	Open(const char *name, bool bReadOnly);
	bool	IsOpen();
	void	Close();

	void	SeekTo( int position );
	unsigned int GetCurPos();
	int GetSize();

	int		ReadRawData( char *buffer, int length );

	void	ReadSequenceInfo(int &nSeqNrIn, int &nSeqNrOutAck);

	void	ReadCmdInfo( democmdinfo_t& info );

	void	ReadCmdHeader( unsigned char& cmd, int& tick );
	
	const char *ReadConsoleCommand( void );

	int		ReadNetworkDataTables( CUtlBuffer *buf ); // if buf is NULL, skip it
	
	int		ReadUserCmd( char *buffer, int &size );

	demoheader_t *ReadDemoHeader();


public:
	FileHandle_t	m_hDemoFile;	// filesystem handle
	char			m_szFileName[MAX_PATH];	//name of current demo file
	demoheader_t    m_DemoHeader;  //general demo info
};

#endif // TOOLDEMOFILE_H
