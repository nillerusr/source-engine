//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SESSIONINFOHEADER_H
#define SESSIONINFOHEADER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/shared_defs.h"
#include "compression.h"
#include "strtools.h"

//----------------------------------------------------------------------------------------

#define SESSION_INFO_VERSION	1

//----------------------------------------------------------------------------------------

struct SessionInfoHeader_t
{
	inline SessionInfoHeader_t()
	{
		V_memset( this, 0, sizeof( SessionInfoHeader_t ) );
		m_nCompressorType = COMPRESSORTYPE_INVALID;
		m_uVersion = SESSION_INFO_VERSION;
	}

	//
	// Session info files may be around for days, during which this format may change - so
	// we need to be careful not to break it.
	//
	// Therefore, any changes to data here should be reflected in the size of m_aUnused.
	//
	uint8				m_uVersion;
	char				m_szSessionName[MAX_SESSIONNAME_LENGTH];	// Name of session
	bool				m_bRecording;		// Is this session currenty recording?
	int32				m_nNumBlocks;		// # blocks in the session so far if recording, or total if not recording
	CompressorType_t	m_nCompressorType;	// COMPRESSORTYPE_INVALID if header is not compressed
	uint8				m_aHash[16];		// MD5 digest on payload
	uint32				m_uPayloadSize;		// Size of the payload - the compressed payload if it's compressed
	uint32				m_uPayloadSizeUC;	// Size of the uncompressed payload, if its compressed, otherwise 0

	uint8				m_aUnused[128];
};

//----------------------------------------------------------------------------------------

bool ReadSessionInfoHeader( const void *pBuf, int nBufSize, SessionInfoHeader_t &outHeader );

//----------------------------------------------------------------------------------------

#endif	// SESSIONINFOHEADER_H
