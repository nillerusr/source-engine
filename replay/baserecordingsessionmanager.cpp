//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "baserecordingsessionmanager.h"
#include "baserecordingsession.h"
#include "baserecordingsessionblock.h"
#include "replay/replayutils.h"
#include "replay/shared_defs.h"
#include "replaysystem.h"
#include "KeyValues.h"
#include "shared_replaycontext.h"
#include "filesystem.h"
#include "iserver.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

inline const char *GetSessionsFullFilename()
{
	return Replay_va( "%s" SUBDIR_SESSIONS "%c", Replay_GetBaseDir(), CORRECT_PATH_SEPARATOR );
}

//----------------------------------------------------------------------------------------

CBaseRecordingSessionManager::CBaseRecordingSessionManager( IReplayContext *pContext )
:	m_pContext( pContext ),
	m_pRecordingSession( NULL ),
	m_bLastSessionDitched( false )
{
}

CBaseRecordingSessionManager::~CBaseRecordingSessionManager()
{
}

bool CBaseRecordingSessionManager::Init()
{
	if ( !BaseClass::Init() )
		return false;

	// Go through each block handle and attempt find the block in the block manager
	typedef CGenericPersistentManager< CBaseRecordingSessionBlock > BaseBlockManager_t;
	BaseBlockManager_t *pBlockManager = dynamic_cast< BaseBlockManager_t * >( m_pContext->GetRecordingSessionBlockManager() );
	FOR_EACH_OBJ( pBlockManager, it )
	{
		CBaseRecordingSessionBlock *pCurBlock = pBlockManager->m_vecObjs[ it ];

		// Find the session for the current block
		CBaseRecordingSession *pSession = m_pContext->GetRecordingSessionManager()->FindSession( pCurBlock->m_hSession );
		if ( !pSession )
		{
			m_pContext->GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Load_CouldNotFindSession" );
			continue;
		}

		// Add the block
		pSession->AddBlock( pCurBlock, false );
	}

	return true;
}

CBaseRecordingSession *CBaseRecordingSessionManager::OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName )
{
	// Add a new session if one w/ the given name doesn't already exist.
	// This is necessary on the client, where a session may already exist if, for example,
	// the client reconnects to a server where they were already playing/saved replays.
	// On the server, NULL will always be passed in for pSessionName.
	CBaseRecordingSession *pNewSession = pSessionName ? FindSessionByName( pSessionName ) : NULL;
	if ( !pNewSession )
	{
		pNewSession = CreateAndGenerateHandle();
		Add( pNewSession );
	}

	// Initialize
	pNewSession->PopulateWithRecordingData( nCurrentRecordingStartTick );

	Save();

	// Update recording session
	m_pRecordingSession = pNewSession;

	return m_pRecordingSession;
}

void CBaseRecordingSessionManager::OnSessionEnd()
{
	if ( m_pRecordingSession )
	{
		// If we don't care about the given session, ditch it
		// NOTE: ShouldDitchSession() checks auto-delete flag!
		if ( m_pRecordingSession->ShouldDitchSession() )
		{
			m_bLastSessionDitched = true;

			DBG( "Marking session for ditch!\n" );

			MarkSessionForDelete( m_pRecordingSession->GetHandle() );
		}
		else
		{
			m_bLastSessionDitched = false;

			// Save
			FlagForFlush( m_pRecordingSession, false );

			// Unload from memory?
			if ( ShouldUnloadSessions() )
			{
				FlagForUnload( m_pRecordingSession );
			}
		}
	}
	m_pRecordingSession = NULL;
}

void CBaseRecordingSessionManager::DeleteSession( ReplayHandle_t hSession, bool bForce )
{
	CBaseRecordingSession *pSession = Find( hSession );
	if ( !pSession )
	{
		AssertMsg( 0, "Trying to delete a non-existent session - should never happen!" );
		return;
	}

	AssertMsg( !pSession->IsLocked(), "Shouldn't be free'ing a locked session!" );

	// If the given session is recording, flag for delete but don't actually remove now
	if ( pSession == m_pRecordingSession && !bForce )
	{
		pSession->m_bAutoDelete = true;
		return;
	}

	// Remove the session and save
	Remove( pSession );
	Save();
}

void CBaseRecordingSessionManager::MarkSessionForDelete( ReplayHandle_t hSession )
{
	m_lstSessionsToDelete.AddToTail( hSession );
}

const char *CBaseRecordingSessionManager::GetCurrentSessionName() const
{
	if ( !m_pRecordingSession )
	{
		AssertMsg( 0, "GetCurrentSessionName() called w/o a session context" );
		return NULL;
	}

	return m_pRecordingSession->m_strName.Get();
}

int CBaseRecordingSessionManager::GetCurrentSessionBlockIndex() const
{
	if ( !m_pRecordingSession )
	{
		AssertMsg( 0, "GetCurrentPartialIndex() called w/o a session context" );
		return -1;
	}

	// Need this MAX() here since GetNumBlocks() will return 0 until the first block is actually written.
	return MAX( 0, m_pRecordingSession->GetNumBlocks() - 1 );
}

void CBaseRecordingSessionManager::FlagSessionForFlush( CBaseRecordingSession *pSession, bool bForceImmediate )
{
	FlagForFlush( pSession, bForceImmediate );
}

int CBaseRecordingSessionManager::GetServerStartTickForSession( ReplayHandle_t hSession )
{
	CBaseRecordingSession *pSession = FindSession( hSession );
	if ( !pSession )
		return -1;
	
	return pSession->m_nServerStartRecordTick;
}

CBaseRecordingSession *CBaseRecordingSessionManager::FindSession( ReplayHandle_t hSession )
{
	return Find( hSession );
}

const CBaseRecordingSession	*CBaseRecordingSessionManager::FindSession( ReplayHandle_t hSession ) const
{
	return const_cast< CBaseRecordingSessionManager * >( this )->Find( hSession );
}

CBaseRecordingSession *CBaseRecordingSessionManager::FindSessionByName( const char *pSessionName )
{
	if ( !pSessionName || !pSessionName[0] )
		return NULL;

	FOR_EACH_OBJ( this, i )
	{
		CBaseRecordingSession *pCurSession = m_vecObjs[ i ];
		if ( !V_stricmp( pSessionName, pCurSession->m_strName.Get() ) )
			return pCurSession;
	}

	return NULL;
}

const char *CBaseRecordingSessionManager::GetRelativeIndexPath() const
{
	return Replay_va( "%s%c", SUBDIR_SESSIONS, CORRECT_PATH_SEPARATOR );
}

void CBaseRecordingSessionManager::Think()
{
	VPROF_BUDGET( "CBaseRecordingSessionManager::Think", VPROF_BUDGETGROUP_REPLAY );

	DeleteSessionThink();

	BaseClass::Think();
}

void CBaseRecordingSessionManager::DeleteSessionThink()
{
	DoSessionCleanup();
}

void CBaseRecordingSessionManager::DoSessionCleanup()
{
	bool bDeletedASession = false;

	for ( int i = m_lstSessionsToDelete.Head(); i != m_lstSessionsToDelete.InvalidIndex(); )
	{
		ReplayHandle_t hSession = m_lstSessionsToDelete[ i ];

		const int itNext = m_lstSessionsToDelete.Next( i );

		if ( CanDeleteSession( hSession ) )
		{
			DBG( "Unloading session.\n" );

			DeleteSession( hSession, true );
			m_lstSessionsToDelete.Remove( i );

			bDeletedASession = true;
		}

		i = itNext;
	}

	// If we just deleted the last session, let the derived class do any post-work
	if ( !m_lstSessionsToDelete.Count() && bDeletedASession )
	{
		OnAllSessionsDeleted();
	}
}

float CBaseRecordingSessionManager::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.1f;
}

//----------------------------------------------------------------------------------------
