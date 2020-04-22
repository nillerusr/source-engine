//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_sessionpublishmanager.h"
#include "sv_recordingsession.h"
#include "sv_sessionblockpublisher.h"
#include "sv_sessioninfopublisher.h"
#include "replay_dbg.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CSessionPublishManager::CSessionPublishManager( CServerRecordingSession *pSession )
:	m_pSession( pSession ),
	m_pBlockPublisher( NULL ),
	m_pSessionInfoPublisher( NULL )
{
	m_pSessionInfoPublisher = new CSessionInfoPublisher( pSession );
	m_pBlockPublisher = new CSessionBlockPublisher( pSession, m_pSessionInfoPublisher );
}

CSessionPublishManager::~CSessionPublishManager()
{
	delete m_pBlockPublisher;
	delete m_pSessionInfoPublisher;
}

void CSessionPublishManager::PublishAllSynchronous()
{
	Msg( "Finishing up replay publish...\n" );

	m_pBlockPublisher->PublishAllSynchronous();
	m_pSessionInfoPublisher->PublishAllSynchronous();
}

void CSessionPublishManager::OnStartRecording()
{
	// Lock the session (which will propagate the lock to all contained blocks)
	m_pSession->SetLocked( true );

	Assert( m_pSession->m_bRecording );
}

void CSessionPublishManager::OnStopRecord( bool bAborting )
{
	// Recording should be turned off on the session by this point
	Assert( !m_pSession->m_bRecording );

	m_pBlockPublisher->OnStopRecord( bAborting );
	m_pSessionInfoPublisher->OnStopRecord( bAborting );
}

ReplayHandle_t CSessionPublishManager::GetSessionHandle() const
{
	return m_pSession->GetHandle();
}

bool CSessionPublishManager::IsDone() const
{
	return !m_pSession->m_bRecording &&
		m_pBlockPublisher->IsDone() &&
		m_pSessionInfoPublisher->IsDone();
}

void CSessionPublishManager::Think()
{
	// NOTE: This gets called even if replay is disabled.  This is intentional.
	VPROF_BUDGET( "CSessionPublishManager::Think", VPROF_BUDGETGROUP_REPLAY );

	// Call publishers
	m_pBlockPublisher->Think();
	m_pSessionInfoPublisher->Think();

#ifdef _DEBUG
	m_pSession->VerifyLocks();
#endif
}

void CSessionPublishManager::UnlockSession()
{
	Assert( !m_pSession->m_bRecording );
	Assert( m_pBlockPublisher->IsDone() );
	Assert( m_pSessionInfoPublisher->IsDone() );

	IF_REPLAY_DBG( Warning( "Unlocking session %s\n", m_pSession->GetDebugName() ) );

	m_pSession->SetLocked( false );
}

void CSessionPublishManager::AbortPublish()
{
	m_pBlockPublisher->AbortPublish();
	m_pSessionInfoPublisher->AbortPublish();
}

#ifdef _DEBUG
void CSessionPublishManager::Validate()
{
	m_pBlockPublisher->Validate();
}
#endif

//----------------------------------------------------------------------------------------
