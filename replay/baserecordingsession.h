//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef BASERECORDINGSESSION_H
#define BASERECORDINGSESSION_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "tier0/platform.h"
#include "utlstring.h"
#include "utllinkedlist.h"
#include "replay/replayhandle.h"
#include "replay/basereplayserializeable.h"
#include "replay/irecordingsession.h"
#include "UtlSortVector.h"

//----------------------------------------------------------------------------------------

class CBaseRecordingSessionBlock;
class KeyValues;
class IReplayContext;

//----------------------------------------------------------------------------------------

// A recording session (e.g. round), including a list of blocks
class CBaseRecordingSession : public CBaseReplaySerializeable,
							  public IRecordingSession
{
	typedef CBaseReplaySerializeable BaseClass;

public:
	CBaseRecordingSession( IReplayContext *pContext );
	~CBaseRecordingSession();

	//
	// IRecordingSession
	//
	virtual void		AddBlock( CBaseRecordingSessionBlock *pBlock );

	virtual bool		Read( KeyValues *pIn );
	virtual void		Write( KeyValues *pOut );
	virtual const char	*GetSubKeyTitle() const;
	virtual const char	*GetPath() const;
	virtual void		OnDelete();
	virtual void		OnUnload();

	const char			*GetSessionInfoURL() const;

	virtual void		PopulateWithRecordingData( int nCurrentRecordingStartTick );

	void				OnStopRecording()	{ m_bRecording = false; }

	class CLessFunctor
	{
	public:
        bool Less( const CBaseRecordingSessionBlock *pSrc1, const CBaseRecordingSessionBlock *pSrc2, void *pContext );
	};

	typedef CUtlSortVector< CBaseRecordingSessionBlock *, CLessFunctor > BlockContainer_t;

	inline int			GetNumBlocks() const		{ return m_vecBlocks.Count(); }
	void				AddBlock( CBaseRecordingSessionBlock *pBlock, bool bFlagForFlush );
	int					FindBlock( CBaseRecordingSessionBlock *pBlock ) const;	// Returns -1 on fail

	const BlockContainer_t	&GetBlocks() const	{ return m_vecBlocks; }

	// Determines whether or not a session gets nuked at the end of a round or not.
	virtual bool		ShouldDitchSession() const;

	// Dynamically load blocks
	void				LoadBlocksForSession();
	void				DeleteBlocks();

	// Persistent:
	bool				m_bRecording;	// Is this session currently recording?
	CUtlString			m_strName;	// A unique session name, given by the server at the start of recording based on time/date
	CUtlString			m_strBaseDownloadURL;	// The download URL, with no filename, e.g., "http://someserver:80/somepath/"
	int					m_nServerStartRecordTick;	// The tick at which the server began recording the given session (based on g_ServerGlobalVariables.tickcount) -
													// which is the tick client spawn/death ticks are relative to
	float				m_flStartTime;

	// Non-persistent:
	IReplayContext		*m_pContext;
	bool				m_bAutoDelete;	// Set to true if a session is removed while it's recording - if flagged, session will auto-delete once recording ends
	bool				m_bBlocksLoaded;

protected:
	BlockContainer_t	m_vecBlocks;	// A list of session blocks for the given session - NOTE: Blocks should not be free'd
};

//----------------------------------------------------------------------------------------

#endif // BASERECORDINGSESSION_H
