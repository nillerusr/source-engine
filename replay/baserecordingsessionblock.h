//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef BASERECORDINGSESSIONBLOCK_H
#define BASERECORDINGSESSIONBLOCK_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/replayhandle.h"
#include "replay/basereplayserializeable.h"
#include "genericpersistentmanager.h"
#include "baserecordingsession.h"
#include "utlstring.h"
#include "compression.h"

//----------------------------------------------------------------------------------------

class KeyValues;
class CBaseReplayContext;
class IReplayContext;

//----------------------------------------------------------------------------------------

class CBaseRecordingSessionBlock : public CBaseReplaySerializeable
{
	typedef CBaseReplaySerializeable BaseClass;

public:
	CBaseRecordingSessionBlock( IReplayContext *pContext );

	//
	// IReplaySerializeable
	//
	virtual const char	*GetSubKeyTitle() const;
	virtual const char	*GetPath() const;
	virtual bool		Read( KeyValues *pIn );
	virtual void		Write( KeyValues *pOut );
	virtual void		OnDelete();

	enum RemoteStatus_t
	{
		STATUS_INVALID = -2,
		STATUS_ERROR = -1,
		STATUS_WRITING,
		STATUS_READYFORDOWNLOAD,

		MAX_STATUS
	};

	static const char *GetRemoteStatusStringSafe( RemoteStatus_t nStatus );

	enum Error_t
	{
		ERROR_NONE,
		ERROR_NOTONDISK,	// The replay was lost somehow - eg a server crash before the replay had a chance to write to disk
		ERROR_WRITEFAILED,	// The disk write somehow failed
	};

	bool		ReadHash( KeyValues *pIn, const char *pHashName );
	void		WriteHash( KeyValues *pOut, const char *pHashName ) const;

	bool		HasValidHash() const;

	// Get a filled out sub key specifically for writing to the session info file
	void		WriteSessionInfoDataToBuffer( CUtlBuffer &buf ) const;

	RemoteStatus_t		m_nRemoteStatus;			// This represents the block's status on the server
	Error_t				m_nHttpError;
	int32				m_iReconstruction;			// For client-side reconstruction of sessions, this represents the index of the given block
	ReplayHandle_t		m_hSession;					// What session is this partial replay a part of?
	uint32				m_uFileSize;				// Size in bytes of the binary block file (if compressed, this represents the compressed file size)
	uint32				m_uUncompressedSize;		// If compressed, this represents the uncompressed file size
	char				m_szFullFilename[512];		// Filename for the .block file itself.
													// NOTE: On the server, full path info is written - on client, only filename is written
	CompressorType_t	m_nCompressorType;			// What type of compressor/decompressor was/should be used, if any?  Can be COMPRESSORTYPE_INVALID if not compressed.
	uint8				m_aHash[16];				// Server sets this and client compares to validate downloaded block data
	bool				m_bHashValid;				// Do we have a valid hash?
	IReplayContext		*m_pContext;
};

//----------------------------------------------------------------------------------------

//
// For serializing blocks - format version implied in header.
//
// In case this format changes, we have some legroom (m_aUnused).
//
struct RecordingSessionBlockSpec_t
{
	int32				m_iReconstruction;
	uint8				m_uRemoteStatus;
	uint8				m_aHash[16];
	int8				m_nCompressorType;
	uint32				m_uFileSize;
	uint32				m_uUncompressedSize;

	uint8				m_aUnused[8];
};

#define MIN_SESSION_INFO_PAYLOAD_SIZE	sizeof( RecordingSessionBlockSpec_t )

//----------------------------------------------------------------------------------------

#endif // BASERECORDINGSESSIONBLOCK_H
