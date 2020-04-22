//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "baserecordingsessionblockmanager.h"
#include "baserecordingsessionblock.h"
#include "replay/replayutils.h"
#include "replay/ireplaycontext.h"
#include "replay/shared_defs.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define RECORDINGSESSIONBLOCKMANAGER_VERSION		0

//----------------------------------------------------------------------------------------

CBaseRecordingSessionBlockManager::CBaseRecordingSessionBlockManager( IReplayContext *pContext )
:	m_pContext( pContext )
{
}

bool CBaseRecordingSessionBlockManager::Init()
{
	// Call CGenericPersistentManager::Init() to do setup, but don't actually load any blocks on the server.
	return BaseClass::Init( ShouldLoadBlocks() );
}

const char *CBaseRecordingSessionBlockManager::GetRelativeIndexPath() const
{
	return Replay_va( "%s%c", SUBDIR_BLOCKS, CORRECT_PATH_SEPARATOR );
}

float CBaseRecordingSessionBlockManager::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.1f;
}

int CBaseRecordingSessionBlockManager::GetVersion() const
{
	return RECORDINGSESSIONBLOCKMANAGER_VERSION;
}

CBaseRecordingSessionBlock *CBaseRecordingSessionBlockManager::GetBlock( ReplayHandle_t hBlock )
{
	return Find( hBlock );
}

void CBaseRecordingSessionBlockManager::DeleteBlock( CBaseRecordingSessionBlock *pBlock )
{
	Remove( pBlock );
}

void CBaseRecordingSessionBlockManager::UnloadBlock( CBaseRecordingSessionBlock *pBlock )
{
	FlagForUnload( pBlock );
}

CBaseRecordingSessionBlock *CBaseRecordingSessionBlockManager::FindBlockForSession( ReplayHandle_t hSession, int iReconstruction )
{
	FOR_EACH_OBJ( this, i )
	{
		CBaseRecordingSessionBlock *pCurBlock = m_vecObjs[ i ];
		if ( pCurBlock->m_hSession == hSession && pCurBlock->m_iReconstruction == iReconstruction )
		{
			return pCurBlock;
		}
	}

	return NULL;
}

const char *CBaseRecordingSessionBlockManager::GetSavePath() const
{
	return Replay_va(
		"%s%c%s%c%s%c",
		SUBDIR_REPLAY, CORRECT_PATH_SEPARATOR,
		m_pContext->GetReplaySubDir(), CORRECT_PATH_SEPARATOR,
		SUBDIR_BLOCKS, CORRECT_PATH_SEPARATOR
	);
}

const char *CBaseRecordingSessionBlockManager::GetBlockPath() const
{
	return GetSavePath();
}

void CBaseRecordingSessionBlockManager::LoadBlockFromFileName( const char *pFilename, IRecordingSession *pSession )
{
	CBaseRecordingSessionBlock *pBlock;
	if ( ReadObjFromFile( pFilename, pBlock, true ) )
	{
		pSession->AddBlock( pBlock );
	}
}

//----------------------------------------------------------------------------------------
