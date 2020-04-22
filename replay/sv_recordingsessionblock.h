//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_RECORDINGSESSIONBLOCK_H
#define SV_RECORDINGSESSIONBLOCK_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionblock.h"

//----------------------------------------------------------------------------------------

class IFilePublisher;

//----------------------------------------------------------------------------------------

class CServerRecordingSessionBlock : public CBaseRecordingSessionBlock
{
	typedef CBaseRecordingSessionBlock BaseClass;

public:
	CServerRecordingSessionBlock( IReplayContext *pContext );

	virtual bool		Read( KeyValues *pIn );
	virtual void		Write( KeyValues *pOut );

	double				GetSecondsToExpiration();

	enum WriteStatus_t
	{
		WRITESTATUS_INVALID = -1,
		WRITESTATUS_WORKING,
		WRITESTATUS_SUCCESS,
		WRITESTATUS_FAILED
	};

	WriteStatus_t	m_nWriteStatus;				// SERVER: initially set to WRITESTATUS_INVALID, then set to STATUS_WORKING, STATUS_SUCCESS,
												// or STATUS_FAILED, depending on the state of the write process (which runs on a separate thread
	IFilePublisher	*m_pPublisher;				// Managed by session recorder

private:
	virtual void		OnDelete();
};

//----------------------------------------------------------------------------------------

inline CServerRecordingSessionBlock *SV_CastBlock( IReplaySerializeable *pBlock )
{
	return static_cast< CServerRecordingSessionBlock * >( pBlock );
}

inline const CServerRecordingSessionBlock *SV_CastBlock( const IReplaySerializeable *pBlock )
{
	return static_cast< const CServerRecordingSessionBlock * >( pBlock );
}

//----------------------------------------------------------------------------------------

#endif // SV_RECORDINGSESSIONBLOCK_H
