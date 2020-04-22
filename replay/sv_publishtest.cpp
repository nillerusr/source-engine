//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_publishtest.h"
#include "spew.h"
#include "replay/replayutils.h"
#include "replaysystem.h"
#include "sv_basejob.h"
#include "sv_fileservercleanup.h"
#include "tier1/convar.h"
#include "sv_filepublish.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

const char *g_pAcceptableFileserverProtocols[] = { "http", "https", NULL };
const char *g_pAcceptableOffloadProtocols[] = { "ftp", NULL };

//----------------------------------------------------------------------------------------

class CPublishTester
{
public:
	CPublishTester();
	~CPublishTester();

	bool Go();

private:
	bool Test_Emptyness( const char *pDescription, const char *pStr, bool bPrintResult );
	bool Test_Hostname( const char *pHostname, const char *pProtocolExample );
	bool Test_Protocol( const char *pDescription, const char *pProtocol, const char **pAcceptableProtocols );
	bool Test_Port( int nPort );
	bool Test_Path( const char *pDescription, const char *pPath, bool bForwardSlashesAllowed, bool bBackslashesAllowed );
	bool Test_LocalWebServerCVars();
	bool Test_IO( const char *pFilename );
	bool Test_FilePublish( const char *pFilename, bool bOffload );
	bool Test_PublishedFileDelete( const char *pFullFilename, const char *pFilename, bool bOffload );
	bool Test_WaitingForPlayersCVar();
	void PrintBaseUrlWarning();

	char		*m_pGarbageBuffer;
	CBaseJob	*m_pJob;
	CBaseJob	*m_pCleanerJob;
};

//----------------------------------------------------------------------------------------

#define GARBAGE_BUFFER_SIZE			( 1024 * 1000 )

CPublishTester::CPublishTester()
:	m_pGarbageBuffer( NULL ),
	m_pJob( NULL ),
	m_pCleanerJob( NULL )
{
	m_pGarbageBuffer = new char[ GARBAGE_BUFFER_SIZE ];
}

CPublishTester::~CPublishTester()
{
	delete [] m_pGarbageBuffer;
	
	if ( m_pJob )
	{
		m_pJob->Release();
	}

	if ( m_pCleanerJob )
	{
		m_pCleanerJob->Release();
	}
}

bool CPublishTester::Test_Hostname( const char *pHostname, const char *pProtocolExample )
{
	if ( !Test_Emptyness( "Hostname", pHostname, false ) )
		return false;

	if ( V_strstr( pHostname, "://" ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		CFmtStr fmtError( "Should not contain a protocol (e.g: %s)!", pProtocolExample );
		g_pBlockSpewer->PrintEventError( fmtError.Access() );
		return false;
	}

	// Test IP lookup
	char szIP[16];
	if ( !g_pEngine->NET_GetHostnameAsIP( pHostname, szIP, sizeof( szIP ) ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "DNS lookup failed!" );
		return false;
	}

	g_pBlockSpewer->PrintEventResult( true );
	g_pBlockSpewer->PrintEmptyLine();

	return true;
}

bool CPublishTester::Test_Emptyness( const char *pDescription, const char *pStr, bool bPrintResult )
{
	g_pBlockSpewer->PrintValue( pDescription, pStr );
	g_pBlockSpewer->PrintEventStartMsg( "Validating" );

	if ( V_strlen( pStr ) == 0 )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Empty!" );
		return false;
	}

	if ( bPrintResult )
	{
		g_pBlockSpewer->PrintEventResult( true );
		g_pBlockSpewer->PrintEmptyLine();
	}

	return true;
}

bool CPublishTester::Test_Port( int nPort )
{
	g_pBlockSpewer->PrintValue( "Port", Replay_va( "%i", nPort ) );
	g_pBlockSpewer->PrintEventStartMsg( "Validating" );

	if ( nPort < 0 || nPort > 65535 )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Port must be between 0 and 65535." );
		return false;
	}

	g_pBlockSpewer->PrintEventResult( true );
	g_pBlockSpewer->PrintEmptyLine();

	return true;
}

bool CPublishTester::Test_Path( const char *pDescription, const char *pPath, bool bForwardSlashesAllowed, bool bBackslashesAllowed )
{
	g_pBlockSpewer->PrintValue( pDescription, pPath );
	g_pBlockSpewer->PrintEventStartMsg( "Validating" );
	if ( V_strlen( pPath ) == 0 )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Empty path not allowed." );
		return false;
	}

	if ( !bBackslashesAllowed && V_strstr( pPath, "\\" ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Backslashes not allowed!" );
		return false;
	}

	if ( !bForwardSlashesAllowed && V_strstr( pPath, "/" ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Forward slashes not allowed!" );
		return false;
	}

	if ( V_strstr( pPath, "//" ) || V_strstr( pPath, "\\\\" ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "Double slash detected!" );
		return false;
	}

	if ( V_strstr( pPath, ".." ) )
	{
		g_pBlockSpewer->PrintEventResult( false );
		g_pBlockSpewer->PrintEventError( "\"..\" not allowed!" );
		return false;
	}

	g_pBlockSpewer->PrintEventResult( true );
	g_pBlockSpewer->PrintEmptyLine();

	return true;
}

bool CPublishTester::Test_Protocol( const char *pDescription, const char *pProtocol, const char **pAcceptableProtocols )
{
	g_pBlockSpewer->PrintValue( pDescription, pProtocol );
	g_pBlockSpewer->PrintEventStartMsg( "Validating" );

	// Test to see if the input protocol is acceptable
	bool bProtocolOK = false;
	int i = 0;
	while ( pAcceptableProtocols[ i ] )
	{
		if ( V_strcmp( pAcceptableProtocols[ i++ ], pProtocol ) == 0 )
		{
			bProtocolOK = true;
			break;
		}
	}

	// Protocol allowed?
	if ( !bProtocolOK )
	{
		g_pBlockSpewer->PrintEventResult( false );
		CFmtStr fmtError( "Must be one of the following (case-sensitive): " );
		i = 0;
		while ( pAcceptableProtocols[ i ] )
			fmtError.AppendFormat( "\"%s\" ", pAcceptableProtocols[ i++ ] );
		g_pBlockSpewer->PrintEventError( fmtError.Access() );
		return false;
	}

	g_pBlockSpewer->PrintEventResult( true );
	g_pBlockSpewer->PrintEmptyLine();

	return true;
}


bool CPublishTester::Test_LocalWebServerCVars()
{
	// NOTE: We use the raw cvar here as opposed to CServerReplayContext::GetLocalFileServerPath(),
	// which actually fixes slashes.  If the cvar is using incorrect slashes here for the given OS,
	// this test will fail with a specific error message.
	extern ConVar replay_local_fileserver_path;
	if ( !Test_Path( "Path", replay_local_fileserver_path.GetString(), IsPosix(), IsWindows() ) )
		return false;

	return true;
}

bool CPublishTester::Test_IO( const char *pFilename )
{
	g_pBlockSpewer->PrintTestHeader( "Testing File I/O" );

	// Print out temp directory so the context for this section is clear
	g_pBlockSpewer->PrintValue( "Temp path", SV_GetTmpDir() );
	g_pBlockSpewer->PrintEmptyLine();

	// Open the file
	FileHandle_t hTmpFile = g_pFullFileSystem->Open( pFilename, "wb+" );
	g_pBlockSpewer->PrintEventStartMsg( "Opening temp file" );
	if ( !hTmpFile )
	{
		g_pBlockSpewer->PrintEventResult( false );
		return false;
	}
	g_pBlockSpewer->PrintEventResult( true );

	// Write the file
	g_pBlockSpewer->PrintEventStartMsg( "Allocating test buffer" );	// Lie.
	if ( !m_pGarbageBuffer )
	{
		g_pBlockSpewer->PrintEventResult( false );
		return false;
	}
	g_pBlockSpewer->PrintEventResult( true );

	g_pBlockSpewer->PrintEventStartMsg( "Writing temp file" );
	if ( g_pFullFileSystem->Write( m_pGarbageBuffer, GARBAGE_BUFFER_SIZE, hTmpFile ) != GARBAGE_BUFFER_SIZE )
	{
		g_pBlockSpewer->PrintEventResult( false );
		return false;
	}
	g_pBlockSpewer->PrintEventResult( true );

	// Close the file
	g_pFullFileSystem->Close( hTmpFile );
	
	return true;
}

bool CPublishTester::Test_FilePublish( const char *pFilename, bool bOffload )
{
	g_pBlockSpewer->PrintTestHeader( "Testing file publisher" );
	g_pBlockSpewer->PrintValue( "Fileserver type", "Local Web server" );

	if ( !Test_LocalWebServerCVars() )
		return false;

	m_pJob = SV_CreateLocalPublishJob( pFilename );

	g_pBlockSpewer->PrintEmptyLine();

	// Run publish test
	if ( !m_pJob || !SV_RunJobToCompletion( m_pJob ) )
	{
		g_pBlockSpewer->PrintEventError( m_pJob->GetErrorStr() );
		return false;
	}

	return true;
}

bool CPublishTester::Test_PublishedFileDelete( const char *pFullFilename, const char *pFilename, bool bOffload )
{
	g_pBlockSpewer->PrintTestHeader( "Testing fileserver delete" );

	if ( bOffload )
	{
		// Delete the file from the tmp dir
		g_pFullFileSystem->RemoveFile( pFullFilename );
	}

	m_pCleanerJob = SV_CreateDeleteFileJob();
	IFileserverCleanerJob *pCleanerJobImp = SV_CastJobToIFileserverCleanerJob( m_pCleanerJob );
	pCleanerJobImp->AddFileForDelete( pFilename );
	if ( !m_pCleanerJob || !SV_RunJobToCompletion( m_pCleanerJob ) )
	{
		g_pBlockSpewer->PrintEventError( m_pCleanerJob->GetErrorStr() );
		return false;
	}

	return true;
}

bool CPublishTester::Test_WaitingForPlayersCVar()
{
	ConVarRef mp_waitingforplayers_cancel( "mp_waitingforplayers_cancel" );
	if ( mp_waitingforplayers_cancel.IsValid() && mp_waitingforplayers_cancel.GetBool() )
	{
		g_pBlockSpewer->PrintEventError( "mp_waitingforplayers_cancel must be 0 in order for replay to work!" );
		return false;
	}

	return true;
}

void CPublishTester::PrintBaseUrlWarning()
{
	g_pBlockSpewer->PrintEmptyLine();
	g_pBlockSpewer->PrintMsg( "If clients can't access the following URL via a Web" );
	g_pBlockSpewer->PrintMsg( "browser, they will not be able to download Replays." );
	g_pBlockSpewer->PrintEmptyLine();
	g_pBlockSpewer->PrintValue( "URL", Replay_GetDownloadURL() );
}

bool CPublishTester::Go()
{
	const bool bOffload = false;

	// Force anyone outside of Go() to use the block spewer until we're done.
	CSpewScope SpewScope( g_pBlockSpewer );

	g_pBlockSpewer->PrintMsg( "TESTING REPLAY SYSTEM CONFIGURATION..." );

	// Fileserver convars
	g_pBlockSpewer->PrintTestHeader( "Testing Fileserver ConVars (replay_fileserver_*)" );

	// Test replay_fileserver_protocol
	extern ConVar replay_fileserver_protocol;
	if ( !Test_Protocol( "Protocol", replay_fileserver_protocol.GetString(), g_pAcceptableFileserverProtocols ) )
		return false;

	extern ConVar replay_fileserver_host;
	if ( !Test_Hostname( replay_fileserver_host.GetString(), "\"http\" or \"https\"" ) )
		return false;

	extern ConVar replay_fileserver_port;
	if ( !Test_Port( replay_fileserver_port.GetInt() ) )
		return false;

	extern ConVar replay_fileserver_path;
	if ( !Test_Path( "Path", replay_fileserver_path.GetString(), true, false ) )
		return false;

	// Print out the base URL / warning
	PrintBaseUrlWarning();

	CFmtStr fmtTmpFilename( "testpublish_%i.tmp", (int)abs( rand() % 10000 ) );
	CFmtStr fmtTmpFilenameFullPath( "%s%s", SV_GetTmpDir(), fmtTmpFilename.Access() );
	const char *pFilename = fmtTmpFilenameFullPath.Access();

	// Test file I/O
	if ( !Test_IO( pFilename ) )
		return false;

	// Get out if necessary
	if ( !Test_FilePublish( pFilename, bOffload ) )
		return false;

	// Test delete from fileserver
	if ( !Test_PublishedFileDelete( pFilename, fmtTmpFilename.Access(), bOffload ) )
		return false;

	// Make sure mp_waitingforplayers_cancel isn't on or replay will be fucked.
	if ( !Test_WaitingForPlayersCVar() )
		return false;

	return true;
}

//----------------------------------------------------------------------------------------

bool SV_DoTestPublish()
{
	CPublishTester tester;
	return tester.Go();
}

//----------------------------------------------------------------------------------------
