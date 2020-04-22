//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "baserecordingsession.h"
#include "baserecordingsessionblock.h"
#include "replay/irecordingsessionblockmanager.h"
#include "replay/replayutils.h"
#include "replay/iclientreplaycontext.h"
#include "replay/shared_defs.h"
#include "KeyValues.h"
#include "replay/replayutils.h"
#include "replay/ireplaycontext.h"
#include "filesystem.h"
#include "iserver.h"
#include "replaysystem.h"
#include "utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CBaseRecordingSession::CBaseRecordingSession( IReplayContext *pContext )
:	m_pContext( pContext ),
	m_bRecording( false ),
	m_bAutoDelete( false ),
	m_bBlocksLoaded( false ),
	m_flStartTime( 0.0f )
{
}

CBaseRecordingSession::~CBaseRecordingSession()
{
}

void CBaseRecordingSession::AddBlock( CBaseRecordingSessionBlock *pBlock )
{
	AddBlock( pBlock, false );
}

bool CBaseRecordingSession::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_strName = pIn->GetString( "name" );
	
	if ( m_strName.IsEmpty() )
	{
		CUtlBuffer buf;
		pIn->RecursiveSaveToFile( buf, 0 );
		IF_REPLAY_DBG( Warning( "Session with no session name found - aborting load for this session.  Data:\n---\n%s\n---\n", (const char *)buf.Base() ) );
		return false;
	}

	m_bRecording = pIn->GetBool( "recording" );
	m_strBaseDownloadURL = pIn->GetString( "base_download_url" );
	m_nServerStartRecordTick = pIn->GetInt( "server_start_record_tick", -1 );

	return true;
}

void CBaseRecordingSession::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetString( "name", m_strName.Get() );
	pOut->SetInt( "recording", m_bRecording ? 1 : 0 );
	pOut->SetString( "base_download_url", m_strBaseDownloadURL.Get() );
	pOut->SetInt( "server_start_record_tick", m_nServerStartRecordTick );
}

const char *CBaseRecordingSession::GetSubKeyTitle() const
{
	return m_strName.Get();
}

const char *CBaseRecordingSession::GetPath() const
{
	return Replay_va( "%s%s%c", m_pContext->GetBaseDir(), SUBDIR_SESSIONS, CORRECT_PATH_SEPARATOR );
}

const char *CBaseRecordingSession::GetSessionInfoURL() const
{
	return Replay_va( "%s%s.%s", m_strBaseDownloadURL.Get(), m_strName.Get(), GENERIC_FILE_EXTENSION );
}

void CBaseRecordingSession::LoadBlocksForSession()
{
	if ( m_bBlocksLoaded )
		return;

	IRecordingSessionBlockManager *pBlockManager = m_pContext->GetRecordingSessionBlockManager();

	// Peek in directory and load files based on what's there
	FileFindHandle_t hFind;
	CFmtStr fmtPath( "%s%s*.%s", pBlockManager->GetBlockPath(), m_strName.Get(), GENERIC_FILE_EXTENSION );
	const char *pFilename = g_pFullFileSystem->FindFirst( fmtPath.Access(), &hFind );
	while ( pFilename )
	{
		// Load the block - this will add the block to this session
		pBlockManager->LoadBlockFromFileName( pFilename, this );	

		// Get next file
		pFilename = g_pFullFileSystem->FindNext( hFind );
	}

	// Blocks loaded
	m_bBlocksLoaded = true;
}

void CBaseRecordingSession::OnDelete()
{
	BaseClass::OnDelete();

	// Dynamically load blocks if necessary, then delete from the block manager and from disk
	DeleteBlocks();
}

void CBaseRecordingSession::DeleteBlocks()
{
	if ( !m_bBlocksLoaded )
	{
		// Load blocks now based on the session name
		LoadBlocksForSession();
	}

	// Delete all blocks associated w/ the session
	FOR_EACH_VEC( m_vecBlocks, i )
	{
		CBaseRecordingSessionBlock *pCurBlock = m_vecBlocks[ i ];
		m_pContext->GetRecordingSessionBlockManager()->DeleteBlock( pCurBlock );
	}
}

void CBaseRecordingSession::OnUnload()
{
	BaseClass::OnUnload();

	FOR_EACH_VEC( m_vecBlocks, i )
	{
		CBaseRecordingSessionBlock *pCurBlock = m_vecBlocks[ i ];
		m_pContext->GetRecordingSessionBlockManager()->UnloadBlock( pCurBlock );
	}
}

void CBaseRecordingSession::PopulateWithRecordingData( int nCurrentRecordingStartTick )
{
	Assert( nCurrentRecordingStartTick >= 0 );

	m_strBaseDownloadURL = Replay_GetDownloadURL();
	m_bRecording = true;
	m_nServerStartRecordTick = nCurrentRecordingStartTick;
}

void CBaseRecordingSession::AddBlock( CBaseRecordingSessionBlock *pBlock, bool bFlagForFlush )
{
	Assert( pBlock->m_hSession == GetHandle() );

	Assert( m_vecBlocks.Find( pBlock ) == m_vecBlocks.InvalidIndex() );
	m_vecBlocks.Insert( pBlock );

	if ( bFlagForFlush )
	{
		// Mark as dirty
		m_pContext->GetRecordingSessionManager()->FlagSessionForFlush( this, false );
	}

	m_bBlocksLoaded = true;
}

int CBaseRecordingSession::FindBlock( CBaseRecordingSessionBlock *pBlock ) const
{
	int itResult = m_vecBlocks.Find( pBlock );
	if ( itResult == m_vecBlocks.InvalidIndex() )
		return -1;

	return itResult;
}

bool CBaseRecordingSession::ShouldDitchSession() const
{
	return m_bAutoDelete;
}

//----------------------------------------------------------------------------------------

bool CBaseRecordingSession::CLessFunctor::Less( const CBaseRecordingSessionBlock *pSrc1, const CBaseRecordingSessionBlock *pSrc2, void *pContext )
{
	return pSrc1->m_iReconstruction < pSrc2->m_iReconstruction;
}

//----------------------------------------------------------------------------------------
