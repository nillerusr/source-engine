//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Uses mp3 code from:  http://www.codeproject.com/audio/MPEGAudioInfo.asp
//
// There don't appear to be any licensing restrictions for using this code:
//
/*
- Readme - MPEG Audio Info Tool V2.0 - 2004-11-01

Description:
This tool can display information about MPEG audio files. It supports
MPEG1, MPEG2, MPEG2.5 in all three layers. You can get all the fields
from the MPEG audio frame in each frame of the file. Additionally you
can check the whole file for inconsistencies.


This tool was written as an example on how to use the classes:
CMPAFile, CMPAHeader, CVBRHeader and CMPAException.

The article MPEG Audio Frame Header on Sourceproject
[http://www.codeproject.com/audio/MPEGAudioInfo.asp] 
provides additional information about these classes and the frame header
in general.

This tool was written with MS Visual C++ 7.1. The MFC library is
statically linked.
*/
//=============================================================================

#ifndef MPAFILE_H
#define MPAFILE_H
#ifdef _WIN32
#pragma once
#endif

#pragma once

#include "VBRHeader.h"
#include "MPAHeader.h"
#include "filesystem.h"

// exception class
class CMPAException
{
public:
	
	enum ErrorIDs
	{
		ErrOpenFile,
		ErrSetPosition,
		ErrReadFile,
		EndOfBuffer,
		NoVBRHeader,
		IncompleteVBRHeader,
		NoFrameInTolerance,
		NoFrame
	};

	CMPAException( ErrorIDs ErrorID, const char *szFile, const char *szFunction = NULL, bool bGetLastError=false );
	// copy constructor (necessary because of LPSTR members)
	CMPAException(const CMPAException& Source);
	~CMPAException(void);

	ErrorIDs GetErrorID() { return m_ErrorID; }

	void ShowError();

private:
	ErrorIDs m_ErrorID;
	bool m_bGetLastError;
	const char *m_szFunction;
	const char *m_szFile;
};


class CMPAFile
{
public:
	CMPAFile( const char *szFile, uint32 dwFileOffset, FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE );
	~CMPAFile(void);

	uint32 ExtractBytes( uint32 &dwOffset, uint32 dwNumBytes, bool bMoveOffset = true );
	const char *GetFilename() const { return m_szFile; };

	bool GetNextFrame();
	bool GetPrevFrame();
	bool GetFirstFrame();
	bool GetLastFrame();

private:
	static const uint32 m_dwInitBufferSize;

	// methods for file access
	void Open( const char *szFilename );
	void SetPosition( int offset );
	uint32 Read( void *pData, uint32 dwSize, uint32 dwOffset );

	void FillBuffer( uint32 dwOffsetToRead );

	static uint32 m_dwBufferSizes[MAXTIMESREAD];

	// concerning file itself
	FileHandle_t m_hFile;
	const char *m_szFile;
	bool m_bMustReleaseFile;

public:	
	uint32 m_dwBegin;	// offset of first MPEG Audio frame
	uint32 m_dwEnd;		// offset of last MPEG Audio frame (estimated)
	bool m_bVBRFile;

	uint32 m_dwBytesPerSec;

	CMPAHeader* m_pMPAHeader;
	uint32 m_dwFrameNo;

	CVBRHeader* m_pVBRHeader;		// XING or VBRI

	// concerning read-buffer
	uint32 m_dwNumTimesRead;
	char *m_pBuffer;
	uint32 m_dwBufferSize;
};

#endif // MPAFILE_H
