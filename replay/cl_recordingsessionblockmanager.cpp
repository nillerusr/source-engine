//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_recordingsessionblockmanager.h"
#include "cl_recordingsessionblock.h"
#include "cl_recordingsession.h"
#include "cl_replaycontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CClientRecordingSessionBlockManager::CClientRecordingSessionBlockManager( IReplayContext *pContext )
:	CBaseRecordingSessionBlockManager( pContext )
{
}

CBaseRecordingSessionBlock *CClientRecordingSessionBlockManager::Create()
{
	return new CClientRecordingSessionBlock( m_pContext );
}

IReplayContext *CClientRecordingSessionBlockManager::GetReplayContext() const
{
	return g_pClientReplayContextInternal;
}

float CClientRecordingSessionBlockManager::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.5f;
}

void CClientRecordingSessionBlockManager::Think()
{
	BaseClass::Think();
}

//----------------------------------------------------------------------------------------
