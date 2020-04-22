//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_replaycontext.h"
#include "replay/shared_defs.h"	// BUILD_CURL defined here
#include "sv_sessionrecorder.h"
#include "sv_fileservercleanup.h"
#include "sv_recordingsession.h"
#include "sv_publishtest.h"
#include "replaysystem.h"
#include "icommandline.h"

#if BUILD_CURL
#include "curl/curl.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#undef CreateEvent

//----------------------------------------------------------------------------------------

CServerReplayContext::CServerReplayContext()
:	m_pSessionRecorder( NULL ),
	m_pFileserverCleaner( NULL ),
	m_bShouldAbortRecording( false ),
	m_flConVarSanityCheckTime( 0.0f )
{
}

CServerReplayContext::~CServerReplayContext()
{
	delete m_pSessionRecorder;
	delete m_pFileserverCleaner;
}

bool CServerReplayContext::Init( CreateInterfaceFn fnFactory )
{
#if BUILD_CURL
	// Initialize cURL - in windows, this will init the winsock stuff.
	curl_global_init( CURL_GLOBAL_ALL );
#endif

	m_pShared = new CSharedReplayContext( this );

	m_pShared->m_strSubDir = GetServerSubDirName();
	m_pShared->m_pRecordingSessionManager = new CServerRecordingSessionManager( this );
	m_pShared->m_pRecordingSessionBlockManager = new CServerRecordingSessionBlockManager( this );
	m_pShared->m_pErrorSystem = new CErrorSystem( this );

	m_pShared->Init( fnFactory );

	// Create directory for temp files
	CFmtStr fmtTmpDir( "%s%s", SV_GetBasePath(), SUBDIR_TMP );
	g_pFullFileSystem->CreateDirHierarchy( fmtTmpDir.Access() );

	// Remove any extraneous files from the temp directory
	CleanTmpDir();

	m_pSessionRecorder = new CSessionRecorder();
	m_pSessionRecorder->Init();

	m_pFileserverCleaner = new CFileserverCleaner();

	return true;
}

void CServerReplayContext::CleanTmpDir()
{
	int nFilesRemoved = 0;

	Log( "Cleaning files from temp dir, \"%s\" ...", SV_GetTmpDir() );

	FileFindHandle_t hFind;
	CFmtStr fmtPath( "%s*", SV_GetTmpDir() );
	const char *pFilename = g_pFullFileSystem->FindFirst( fmtPath.Access(), &hFind );
	while ( pFilename )
	{
		if ( pFilename[0] != '.' )
		{
			// Remove the file
			CFmtStr fmtFullFilename( "%s%s", SV_GetTmpDir(), pFilename );
			g_pFullFileSystem->RemoveFile( fmtFullFilename.Access() );

			++nFilesRemoved;
		}

		// Get next file
		pFilename = g_pFullFileSystem->FindNext( hFind );
	}

	if ( nFilesRemoved )
	{
		Log( "%i %s removed.\n", nFilesRemoved, nFilesRemoved == 1 ? "file" : "files" );
	}
	else
	{
		Log( "no files removed.\n" );
	}
}

void CServerReplayContext::Shutdown()
{
	m_pShared->Shutdown();

#if BUILD_CURL
	// Shutdown cURL
	curl_global_cleanup();
#endif
}

void CServerReplayContext::Think()
{
	ConVarSanityThink();

	if ( !g_pReplay->IsReplayEnabled() )
		return;

	if ( m_bShouldAbortRecording )
	{
		g_pBlockSpewer->PrintBlockStart();
		g_pBlockSpewer->PrintMsg( "Replay recording shutting down due to publishing error!  Recording will begin" );
		g_pBlockSpewer->PrintMsg( "at the beginning of the next round, but may fail again." );
		g_pBlockSpewer->PrintBlockEnd();

		// Shutdown recording
		m_pSessionRecorder->AbortCurrentSessionRecording();

		m_bShouldAbortRecording = false;
	}

	m_pShared->Think();
}

void CServerReplayContext::ConVarSanityThink()
{
	if ( m_flConVarSanityCheckTime == 0.0f )
		return;

	DoSanityCheckNow();
}

void CServerReplayContext::UpdateFileserverIPFromHostname( const char *pHostname )
{
	if ( !g_pEngine->NET_GetHostnameAsIP( pHostname, m_szFileserverIP, sizeof( m_szFileserverIP ) ) )
	{
		V_strcpy( m_szFileserverIP, "0.0.0.0" );
		Log( "ERROR: Could not resolve fileserver hostname \"%s\" !\n", pHostname );
		return;
	}

	Log( "Cached resolved fileserver hostname to IP address: \"%s\" -> \"%s\"\n", pHostname, m_szFileserverIP );
}

void CServerReplayContext::UpdateFileserverProxyIPFromHostname( const char *pHostname )
{
	if ( !g_pEngine->NET_GetHostnameAsIP( pHostname, m_szFileserverProxyIP, sizeof( m_szFileserverProxyIP ) ) )
	{
		V_strcpy( m_szFileserverProxyIP, "0.0.0.0" );
		Log( "ERROR: Could not resolve fileserver proxy hostname \"%s\" !\n", pHostname );
		return;
	}

	Log( "Cached resolved fileserver proxy hostname to IP address: \"%s\" -> \"%s\"\n", pHostname, m_szFileserverProxyIP );
}

void CServerReplayContext::DoSanityCheckNow()
{
	// Check now?
	if ( m_flConVarSanityCheckTime <= g_pEngine->GetHostTime() )
	{
		// Reset
		m_flConVarSanityCheckTime = 0.0f;

		g_pBlockSpewer->PrintBlockStart();

		extern ConVar replay_enable;
		if ( replay_enable.GetBool() )
		{
			// Test publish
			const bool bPublishResult = SV_DoTestPublish();

			g_pBlockSpewer->PrintEmptyLine();

			if ( bPublishResult )
			{
				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintMsg( "SUCCESS - REPLAY IS ENABLED!" );
				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintMsg( "A 'changelevel' or 'map' is required - recording will" );
				g_pBlockSpewer->PrintMsg( "begin at the start of the next round." );
				g_pBlockSpewer->PrintEmptyLine();
			}
			else
			{
				replay_enable.SetValue( 0 );

				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintMsg( "FAILURE - REPLAY DISABLED! \"replay_enable\" is now 0." );
				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintEmptyLine();
				g_pBlockSpewer->PrintMsg( "Address any failures above and re-exec replay.cfg." );
			}
		}

		g_pBlockSpewer->PrintBlockEnd();
	}
}

void CServerReplayContext::FlagForConVarSanityCheck()
{
	m_flConVarSanityCheckTime = g_pEngine->GetHostTime() + 0.2f;
}

IGameEvent *CServerReplayContext::CreateReplaySessionInfoEvent()
{
	IGameEvent *pEvent = g_pGameEventManager->CreateEvent( "replay_sessioninfo", true );
	if ( !pEvent )
		return NULL;

	extern ConVar replay_block_dump_interval;

	// Fill event
	pEvent->SetString( "sn", m_pShared->m_pRecordingSessionManager->GetCurrentSessionName() );
	pEvent->SetInt( "di", replay_block_dump_interval.GetInt() );
	pEvent->SetInt( "cb", m_pShared->m_pRecordingSessionManager->GetCurrentSessionBlockIndex() );
	pEvent->SetInt( "st", m_pSessionRecorder->GetCurrentRecordingStartTick() );

	return pEvent;
}

IReplaySessionRecorder *CServerReplayContext::GetSessionRecorder()
{
	return g_pServerReplayContext->m_pSessionRecorder;
}

const char *CServerReplayContext::GetLocalFileServerPath() const
{
	static char s_szBuf[MAX_OSPATH];
	extern ConVar replay_local_fileserver_path;

	// Fix up the path name - NOTE: We intentionally avoid calling V_FixupPathName(), which
	// pushes the entire output string to lower case.
	V_strncpy( s_szBuf, replay_local_fileserver_path.GetString(), sizeof( s_szBuf ) );
	V_FixSlashes( s_szBuf );
	V_RemoveDotSlashes( s_szBuf );
	V_FixDoubleSlashes( s_szBuf );

	V_StripTrailingSlash( s_szBuf );
	V_AppendSlash( s_szBuf, sizeof( s_szBuf ) );
	return s_szBuf;
}

void CServerReplayContext::CreateSessionOnClient( int nClientSlot )
{
	// If we have a session (i.e. if we're recording)
	if ( SV_GetRecordingSessionInProgress() )
	{
		// Create the session on the client
		IGameEvent *pSessionInfoEvent = CreateReplaySessionInfoEvent();
		g_pReplay->SV_SendReplayEvent( pSessionInfoEvent, nClientSlot );
	}
}

const char *CServerReplayContext::GetServerSubDirName() const
{
	const char *pSubDirName = CommandLine()->ParmValue( "-replayserverdir" );
	if ( !pSubDirName || !pSubDirName[0] )
	{
		Msg( "No '-replayserverdir' parameter found - using default replay folder.\n" );
		return SUBDIR_SERVER;
	}

	Msg( "\n** Using custom replay dir name: \"%s%c%s\"\n\n", SUBDIR_REPLAY, CORRECT_PATH_SEPARATOR, pSubDirName );

	return pSubDirName;
}

void CServerReplayContext::ReportErrorsToUser( wchar_t *pErrorText )
{
	char szErrorText[4096];
	g_pVGuiLocalize->ConvertUnicodeToANSI( pErrorText, szErrorText, sizeof( szErrorText ) );

	static Color s_clrRed( 255, 0, 0 );
	Warning( "\n-----------------------------------------------\n" );
	Warning( "%s", szErrorText );
	Warning( "-----------------------------------------------\n\n" );
}

void CServerReplayContext::OnPublishFailed()
{
	// Don't report publish failure and shutdown publishing more than once per session.
	if ( !m_pSessionRecorder->RecordingAborted() )
	{
		m_bShouldAbortRecording = true;
	}
}

//----------------------------------------------------------------------------------------

CServerRecordingSession *SV_GetRecordingSessionInProgress()
{
	return SV_CastSession( SV_GetRecordingSessionManager()->GetRecordingSessionInProgress() );
}

const char *SV_GetTmpDir()
{
	return Replay_va( "%s%s%c", SV_GetBasePath(), SUBDIR_TMP, CORRECT_PATH_SEPARATOR );
}

bool SV_IsOffloadingEnabled()
{
	return false;
}

bool SV_RunJobToCompletion( CJob *pJob )
{
	return RunJobToCompletion( SV_GetThreadPool(), pJob );
}

//----------------------------------------------------------------------------------------
