//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef BASERECORDINGSESSIONBLOCKMANAGER_H
#define BASERECORDINGSESSIONBLOCKMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "genericpersistentmanager.h"
#include "replay/replayhandle.h"
#include "replay/irecordingsessionblockmanager.h"
#include "utlstring.h"
#include "baserecordingsession.h"
#include "replay/basereplayserializeable.h"
#include "baserecordingsessionblock.h"

//----------------------------------------------------------------------------------------

class KeyValues;
class CBaseReplayContext;
class IReplayContext;

//----------------------------------------------------------------------------------------

//
// Maintains a persistent list of session blocks in a keyvalues file
//
class CBaseRecordingSessionBlockManager : public CGenericPersistentManager< CBaseRecordingSessionBlock >,
										  public IRecordingSessionBlockManager
{
	typedef CGenericPersistentManager< CBaseRecordingSessionBlock > BaseClass;

public:
	CBaseRecordingSessionBlockManager( IReplayContext *pContext );

	virtual bool			Init();

	//
	// IRecordingSessionBlockManager
	//
	CBaseRecordingSessionBlock	*GetBlock( ReplayHandle_t hBlock );
	virtual void				DeleteBlock( CBaseRecordingSessionBlock *pBlock );
	virtual void				UnloadBlock( CBaseRecordingSessionBlock *pBlock );
	virtual const char			*GetBlockPath() const;
	virtual void				LoadBlockFromFileName( const char *pFilename, IRecordingSession *pSession );	// NOTE: This will not actually add the block to the session block manager - this is for loading blocks and adding them to recording sessions so they can be deleted from disk and cleaned up from the fileserver if necessary.

	// Find the block for with the given reconstruction index for the given session
	CBaseRecordingSessionBlock	*FindBlockForSession( ReplayHandle_t hSession, int iReconstruction );

	// Gets something like "replays/<client|server>/blocks/"
	const char 					*GetSavePath() const;

protected:
	virtual bool		ShouldSerializeToIndividualFiles() const { return true; }
	virtual const char	*GetRelativeIndexPath() const;

	virtual bool		ShouldLoadBlocks() const = 0;

	//
	// CBaseThinker
	//
	virtual float	GetNextThinkTime() const;

	IReplayContext		*m_pContext;

private:
	virtual const char	*GetDebugName() const			{ return "block manager"; }
	virtual int			GetVersion() const;
	virtual const char	*GetIndexFilename() const	{ return "blocks." GENERIC_FILE_EXTENSION; }
};

//----------------------------------------------------------------------------------------

#endif // BASERECORDINGSESSIONBLOCKMANAGER_H