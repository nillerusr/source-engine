//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_RECORDINGSESSIONBLOCKMANAGER_H
#define SV_RECORDINGSESSIONBLOCKMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionblockmanager.h"

//----------------------------------------------------------------------------------------

//
// Maintains a persistent list of session blocks in a keyvalues file
//
class CServerRecordingSessionBlockManager : public CBaseRecordingSessionBlockManager
{
	typedef CBaseRecordingSessionBlockManager BaseClass;

public:
	CServerRecordingSessionBlockManager( IReplayContext *pContext );

	virtual CBaseRecordingSessionBlock	*Create();
	virtual IReplayContext				*GetReplayContext() const;

private:
	virtual bool						ShouldLoadBlocks() const	{ return false; }
	virtual void						PreLoad();
};

//----------------------------------------------------------------------------------------

#endif // SV_RECORDINGSESSIONBLOCKMANAGER_H