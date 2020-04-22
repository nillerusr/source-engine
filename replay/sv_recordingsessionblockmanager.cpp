//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_recordingsessionblockmanager.h"
#include "sv_recordingsessionblock.h"
#include "sv_replaycontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CServerRecordingSessionBlockManager::CServerRecordingSessionBlockManager( IReplayContext *pContext )
:	CBaseRecordingSessionBlockManager( pContext )
{
}

CBaseRecordingSessionBlock *CServerRecordingSessionBlockManager::Create()
{
	return new CServerRecordingSessionBlock( m_pContext );
}
	
IReplayContext *CServerRecordingSessionBlockManager::GetReplayContext() const
{
	extern CServerReplayContext *g_pServerReplayContext;
	return g_pServerReplayContext;
}

void CServerRecordingSessionBlockManager::PreLoad()
{
	ConMsg( "Loading recording session blocks - this may take a minute...\n" );
}

//----------------------------------------------------------------------------------------
