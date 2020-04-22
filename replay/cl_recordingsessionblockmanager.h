//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef RECORDINGSESSIONBLOCKMANAGER_H
#define RECORDINGSESSIONBLOCKMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionblockmanager.h"

//----------------------------------------------------------------------------------------

class CClientRecordingSessionBlockManager : public CBaseRecordingSessionBlockManager
{
	typedef CBaseRecordingSessionBlockManager BaseClass;

public:
	CClientRecordingSessionBlockManager( IReplayContext *pContext );

	//
	// CGenericPersistentManager
	//
	virtual CBaseRecordingSessionBlock	*Create();

private:
	//
	// CGenericPersistentManager
	//
	virtual IReplayContext	*GetReplayContext() const;

	//
	// CBaseThinker
	//
	virtual float		GetNextThinkTime() const;
	virtual void		Think();

	virtual bool		ShouldLoadBlocks() const	{ return false; }
};

//----------------------------------------------------------------------------------------

#endif // RECORDINGSESSIONBLOCKMANAGER_H