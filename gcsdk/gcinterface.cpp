//====== Copyright ©, Valve Corporation, All rights reserved. =================
//
// Purpose: Defines the GC interface exposed to the host
//
//=============================================================================


#include "stdafx.h"
#include "winlite.h"

#include "tier0/minidump.h"
#include "tier1/interface.h"
#include "appframework/iappsystemgroup.h"
#include "filesystem.h"
#include "vstdlib/cvar.h"
#include "signal.h"

#include "gcsdk/steamextra/rtime.h"
#include "gcsdk/directory.h"
#include "gcsdk/gcinterface.h"


#define WINDOWS_LEAN_AND_MEAN
#if !defined( _WIN32_WINNT )
#define _WIN32_WINNT 0x0403
#endif
#include <windows.h>


namespace GCSDK
{
static GCConVar cv_assert_minidump_window( "assert_minidump_window", "28800", "Size of the minidump window in seconds. Each unique assert will dump at most assert_max_minidumps_in_window times in this many seconds" );
static GCConVar cv_assert_max_minidumps_in_window( "assert_max_minidumps_in_window", "5", "The amount of times each unique assert will write a dump in assert_minidump_window seconds" );
static GCConVar enable_assert_minidumps( "enable_assert_minidumps", "1", "An emergency shutoff to prevent the recording or tracking of asserts" );
static GCConVar filter_blank_lines( "filter_blank_lines", "1", "Prevents blank lines from being written or logged" );

//-----------------------------------------------------------------------------
// Purpose: Creates a global pointer to the interface and exposes it to the host
//-----------------------------------------------------------------------------
CGCInterface g_GCInterface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGCInterface, IGameCoordinator, GAMECOORDINATOR_INTERFACE_VERSION, g_GCInterface );

int32 CGCInterface::CDisableAssertRateLimit::s_nDisabledCount = 0;

// Force the linker to include this even though we're in a static lib
void ForceIncludeGCInterface()
{
	#pragma comment( linker, "/INCLUDE:" __FUNCDNAME__ )
	void *pUnused = &__g_CreateCGCInterfaceIGameCoordinator_reg;
	pUnused = NULL;

#ifdef DEBUG
	// Adds a note for the deploy tool to not let it prop with a debug GCSDK
	printf( "is a debug binary" );
#endif	
}


//-----------------------------------------------------------------------------
// Purpose: Overrides the spew func used by Msg and DMsg to print to the console
//-----------------------------------------------------------------------------
class CConsoleLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		const char *pszFmt = ( sizeof( tchar ) == sizeof( char ) ) ? "%hs" : "%ls";
		switch ( pContext->m_Severity )
		{
			default:
			case LS_MESSAGE:
				EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, pszFmt, pMessage );
				break;

			case LS_WARNING:
				EmitWarning( SPEW_CONSOLE, SPEW_ALWAYS, pszFmt, pMessage );
				break;

			case LS_ERROR:
			case LS_HIGHEST_SEVERITY:
				EmitError( SPEW_CONSOLE, pszFmt, pMessage );
				break;

			case LS_ASSERT:
				//if this assert is in a job, display the name of the job as well
				if ( ThreadInMainThread() && ( g_pJobCur != NULL ) )
				{
					pszFmt = ( sizeof( tchar ) == sizeof( char ) ) ? "[Job %s] %hs" : "[Job %s] %ls";
					EmitAssertError( SPEW_CONSOLE, pszFmt, g_pJobCur->GetName(), pMessage );
				}
				else
				{
					EmitAssertError( SPEW_CONSOLE, pszFmt, pMessage );
				}
				break;
		}
	}
};
static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;
static CConsoleLoggingListener s_ConsoleLoggingListener;


//-----------------------------------------------------------------------------
// Purpose: Prints an assert to the console
//-----------------------------------------------------------------------------
class CGCAssertionFailureListener : public IAssertionFailureListener
{
public:
	CGCAssertionFailureListener( void )
		: IAssertionFailureListener( false )
	{
	}

	virtual void *AssertionFailure( const char *pFormattedMsg, const tchar *pchFile, int nLine, const tchar *pchFunction, const tchar *pchRawExpression, int nInstanceReportCount, AssertionType_t nType, bool bFatal ) OVERRIDE
	{
		if ( Plat_IsInDebugSession() )
			return NULL;

		bool bShouldWriteMinidump = false;
		GGCInterface()->RecordAssert( pchFile, nLine, pFormattedMsg, &bShouldWriteMinidump );

		return bShouldWriteMinidump ? this : NULL;
	}

	virtual void MiniDumpHandler( const MiniDumpHandlerData_t &HandlerData, const char *pFormattedMsg, const tchar *pchFile, int nLine, const tchar *pchFunction, const tchar *pchRawExpression, int nInstanceReportCount, AssertionType_t nType, bool bFatal ) OVERRIDE
	{
		//re-route to default minidump handler (treat it the same as a crash)
		CFmtStr minidumpNameToken( "assert_%s_%d", V_GetFileName( pchFile ), nLine );
		MiniDumpOptionalData_t optionalData( minidumpNameToken.Access() );
		
		MiniDumpHandlerData_t modifiableHandlerData( HandlerData );
		modifiableHandlerData.SetOptionalData( optionalData );

		//write to disk
		Tier0GenericMiniDumpHandlerEx( modifiableHandlerData, NULL, MINIDUMP_ADDITIONAL_FLAG_PRINT_MESSAGE );
	}
};
static CGCAssertionFailureListener sg_GCAssertionFailureHandler;


static void ProtobufLogHandler( ::google::protobuf::LogLevel level, const char* filename, int line, const std::string& message )
{
	EG_MSG( g_EGMessages, "Protobuf %s(%d): %s\n", filename, line, message.c_str() );
	AssertFatalMsg( level != google::protobuf::LOGLEVEL_FATAL, "Fatal protobuf assert %s(%d): %s", filename, line, message.c_str() );
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the underlying libraries
//-----------------------------------------------------------------------------
static class CGCAppSystemGroup : public CAppSystemGroup
{
public:
	CGCAppSystemGroup() {}
	void SetPath ( const char *pchBinaryPath ) { m_sBinaryPath = pchBinaryPath; }

	// Implementation of IAppSystemGroup
	virtual bool Create() OVERRIDE
	{
		AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
		AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

		AppSystemInfo_t appSystems[] = 
		{
			{ "filesystem_stdio.dll",				FILESYSTEM_INTERFACE_VERSION },
			{ "", "" }	// Required to terminate the list
		};

		CUtlVector<CUtlString> vecFullPaths;
		AppSystemInfo_t *pSystem = appSystems;
		while( pSystem->m_pModuleName[0] != '\0' )
		{
			CUtlString &strNewPath = vecFullPaths[ vecFullPaths.AddToTail() ];
			strNewPath.Format( "%s%s%s", m_sBinaryPath.Get(), CORRECT_PATH_SEPARATOR_S, pSystem->m_pModuleName );
			pSystem->m_pModuleName = strNewPath.Get();

			pSystem++;
		}

		return AddSystems( appSystems );
	}

	virtual bool PreInit() OVERRIDE
	{
		CreateInterfaceFn factory = GetFactory();
		ConnectTier1Libraries( &factory, 1 );
		ConnectTier2Libraries( &factory, 1 );

		if( !g_pFullFileSystem )
			return false;

		if ( !g_pCVar )
			return false;

		ConVar_Register();

		return true;
	}

	virtual void PostShutdown() OVERRIDE
	{
		ConVar_Unregister();
		DisconnectTier2Libraries();
		DisconnectTier1Libraries();
	}

	virtual void Destroy() OVERRIDE {}

	// this should never be called
	virtual int Main( ) OVERRIDE { return -1; }

private:
	CUtlString m_sBinaryPath;
} g_gcAppSystemGroup;


//-----------------------------------------------------------------------------
// Purpose: Gets the global instance
//-----------------------------------------------------------------------------
CGCInterface *GGCInterface()
{
	return &g_GCInterface;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCInterface::CGCInterface()
	: m_pGCHost( NULL )
	, m_pGC( NULL )
	, m_pGCDirProcess( NULL )
	, m_nAppID( k_uAppIdInvalid )
	, m_eUniverse( k_EUniverseInvalid )
	, m_bDevMode( false )
	, m_ullGID( 0 )
	, m_bLogCaptureEnabled( false )
	, m_nVersion( 0 )
	, m_hParentProcess( NULL )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCInterface::~CGCInterface()
{
	m_BlockEmitStrings.PurgeAndDeleteElements();
	ClearAssertInfo();
	delete m_pGC;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the actual GC referred to by the interface
//-----------------------------------------------------------------------------
IGameCoordinator *CGCInterface::GetGC()
{
	return m_pGC;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the GC is running in a dev environment
//-----------------------------------------------------------------------------
bool CGCInterface::BIsDevMode() const
{
	return m_bDevMode;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the GC's appID
//-----------------------------------------------------------------------------
AppId_t CGCInterface::GetAppID() const
{
	return m_nAppID;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the directory gc.dll is running in
//-----------------------------------------------------------------------------
const char *CGCInterface::GetGCDLLPath() const
{
	return m_sGCDLLPath;
}

//-----------------------------------------------------------------------------
// Purpose: Reads the config KV from the disk
//-----------------------------------------------------------------------------
bool CGCInterface::BReadConfigDirectory( KeyValuesAD& configValues )
{
	// Read the config file
	const char *pchBaseConfigName = NULL;

	switch( GetUniverse() )
	{
	case k_EUniversePublic:		pchBaseConfigName = "gcconfig_public.vdf"; break;
	case k_EUniverseBeta:		pchBaseConfigName = "gcconfig_beta.vdf"; break;
	case k_EUniverseInternal:	pchBaseConfigName = "gcconfig_internal.vdf"; break;
	case k_EUniverseDev:		pchBaseConfigName = "gcconfig_dev.vdf"; break;
	}

	if( !pchBaseConfigName || !configValues->LoadFromFile( g_pFullFileSystem, pchBaseConfigName, "CONFIG" ) )
	{
		GCSDK::EmitError( SPEW_GC, "Unable to read config file: %s. Aborting.\n", pchBaseConfigName ? pchBaseConfigName : "unknown universe specified" );
		return false;
	}

	//load up our directory
	if ( !GDirectory()->BInit( configValues->FindKey( "directory" ) ) )
	{
		GCSDK::EmitError( SPEW_GC, "Unable to find 'directory' key within config file %s.\n", pchBaseConfigName );
		return false;
	}
	
	return true;	
}

bool CGCInterface::BReadConvars( KeyValuesAD& configValues )
{	
	//load the standard global convars
	InitConVars( configValues->FindKey( "convars" ) );

	//we can't load more if we don't have a directory as we don't know our GC type
	if( !m_pGCDirProcess )
	{
		AssertMsg( false, "Attempted to read console variables without any GC type specified" );
		return false;
	}
	
	//get convars for this specific configuration name
	InitConVars( configValues->FindKey( CFmtStr( "%s-process-convars", m_pGCDirProcess->GetName() ) ) );

	//now load the GC type specific convars. Note that these can stomp, so we must do all of them in sequence
	for( uint32 nInstance = 0; nInstance < m_pGCDirProcess->GetTypeInstanceCount(); nInstance++ )
	{
		const char* pszTypeName = GDirectory()->GetNameForGCType( m_pGCDirProcess->GetTypeInstance( nInstance )->GetType() );
		InitConVars( configValues->FindKey( CFmtStr( "%s-type-convars", pszTypeName ) ) );
	}

	//see if they have a special config associated with this GC
	if( m_pGCDirProcess->GetConfig( ) )
	{
		const char* pszAdditionalConvars = m_pGCDirProcess->GetConfig()->GetString( "convars", NULL );
		if( pszAdditionalConvars )
		{
			//now load the convars that are specific to this instance
			InitConVars( configValues->FindKey( CFmtStr( "%s-convars", pszAdditionalConvars ) ) );
		}
	}
	

	if ( k_EUniverseDev != GetUniverse() )
	{
		// See if there's a convar override file
		KeyValuesAD pkvSavedConvars( "convars" );

		if( pkvSavedConvars->LoadFromFile( g_pFullFileSystem, CFmtStr( "%u_%s_%s_savedconvars.vdf", GetAppID(), m_pGCDirProcess->GetName(), PchNameFromEUniverse( GetUniverse() ) ), "CONFIG" ) )
		{
			InitConVars( pkvSavedConvars );
		}
		else
		{
			EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "Unable to read saved convars file. Continuing with defaults.\n" );
		}
	}

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Sets the values of convars from the given KV
//-----------------------------------------------------------------------------
void CGCInterface::InitConVars( KeyValues *pkvConvars )
{
	// init all the convars
	if( !pkvConvars )
		return;

	FOR_EACH_VALUE( pkvConvars, pkvVar )
	{
		if ( !pkvVar->GetString() )
		{
			EmitWarning( SPEW_CONSOLE, SPEW_ALWAYS, "variable %s missing value, skipping\n", pkvVar->GetName() );
		}

		ConVar *pVar = NULL;
		const char *pchSuffix = V_strrchr( pkvVar->GetName(), '_' );
		if ( NULL != pchSuffix && 0 == V_strcmp( pchSuffix, CFmtStr( "_%u", GetAppID() ) ) )
		{
			pVar = g_pCVar->FindVar( pkvVar->GetName() );
		}
		else
		{
			pVar = g_pCVar->FindVar( CFmtStr( "%s_%u", pkvVar->GetName(), GetAppID() ) );
		}

		if ( !pVar )
		{
			EmitWarning( SPEW_CONSOLE, SPEW_ALWAYS, "config file references unknown convar %s\n", pkvVar->GetName() );
		}
		else
		{
			pVar->SetValue( pkvVar->GetString() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Writes the current non-default convars to disk
//-----------------------------------------------------------------------------
bool CGCInterface::BSaveConvars()
{
	//do nothing if we haven't loaded the directory
	if( !m_pGCDirProcess )
		return false;

	// copy all the non-default convars to the config
	KeyValuesAD pkvConvars( "convars" );

	ICvar::Iterator iter( g_pCVar ); 
	for ( iter.SetFirst(); iter.IsValid(); iter.Next() )
	{  
		const ConCommandBase *pCommand = iter.Get();
		const GCConVar *pVar = dynamic_cast<const GCConVar *>( pCommand );

		if( pVar && 0 != Q_strcmp( pVar->GetString(), pVar->GetDefault() ) )
		{
			KeyValues *pkvVar = pkvConvars->FindKey( pVar->GetBaseName(), true );
			pkvVar->SetStringValue( pVar->GetString() );
		}
	}

	return pkvConvars->SaveToFile( g_pFullFileSystem, CFmtStr( "%u_%s_%s_savedconvars.vdf", GetAppID(), m_pGCDirProcess->GetName(), PchNameFromEUniverse( GetUniverse() ) ), "CONFIG" );
}

//-----------------------------------------------------------------------------
// Purpose: Construct a Steam ID for a client, given an account ID
//-----------------------------------------------------------------------------
CSteamID CGCInterface::ConstructSteamIDForClient( AccountID_t unAccountID ) const
{
	return CSteamID( unAccountID, m_eUniverse, k_EAccountTypeIndividual );
}

//-----------------------------------------------------------------------------
void CGCInterface::ClearAssertWindowCounts()
{
	FOR_EACH_DICT_FAST( m_dictAsserts, nCurrFile )
	{
		FOR_EACH_VEC( *m_dictAsserts[ nCurrFile ], nCurrAssert )
		{
			( *m_dictAsserts[ nCurrFile ] )[ nCurrAssert ]->m_nWindowFired = 0;
		}
	}
}

//-----------------------------------------------------------------------------
void CGCInterface::ClearAssertInfo()
{
	FOR_EACH_DICT_FAST( m_dictAsserts, nCurrAssert )
	{
		m_dictAsserts[ nCurrAssert ]->PurgeAndDeleteElements();
	}
	m_dictAsserts.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: Records an assert and optionally passes back if we should write
//	a minidump based on it
//-----------------------------------------------------------------------------
void CGCInterface::RecordAssert( const char *pchFile, int nLine, const char *pchMessage, bool *pbShouldWriteMinidump )
{
	//assume we are not writing a dump by default
	if( pbShouldWriteMinidump )
		*pbShouldWriteMinidump = false;

	//handle an emergency disable of asserts
	if( !enable_assert_minidumps.GetBool() )
		return;

	//get our entry in our map
	int iDict = m_dictAsserts.Find( pchFile );
	if ( !m_dictAsserts.IsValidIndex( iDict ) )
	{
		iDict = m_dictAsserts.Insert( pchFile, new CUtlVector< AssertInfo_t* > );
	}

	CUtlVector< AssertInfo_t* > &vecAsserts = *m_dictAsserts[iDict];
	//see if we have an entry for this line already
	AssertInfo_t* pAssert = NULL;
	FOR_EACH_VEC( vecAsserts, nCurrAssert )
	{
		if( ( uint32 )nLine == vecAsserts[ nCurrAssert ]->m_nLine )
		{
			pAssert = vecAsserts[ nCurrAssert ];
			break;
		}
	}

	//one wasn't already in the list, so we need to create and insert it
	if( !pAssert )
	{
		pAssert = new AssertInfo_t;
		pAssert->m_nLine = nLine;
		pAssert->m_sMsg = pchMessage;
		pAssert->m_nWindowFired = 0;
		pAssert->m_nTotalFired = 0;
		pAssert->m_nTotalRecorded = 0;
		vecAsserts.AddToTail( pAssert );

		//also, remove any newlines from the asserts. The default assert inserts them and this creates problems for a lot of the exporting of the data from SQL into Excel
		pAssert->m_sMsg = pAssert->m_sMsg.Replace( '\n', ' ' );
	}

	//update our stats
	pAssert->m_nTotalFired++;
	pAssert->m_nWindowFired++;

	//remove any recorded asserts that are older than our window, so that we can record new asserts
	int nStale = 0;
	CUtlVector< RTime32 >& vecTimes = pAssert->m_vRecordTimes;
	const RTime32 nStaleTime = CRTime::RTime32TimeCur() - (uint32)cv_assert_minidump_window.GetInt();
	while ( ( nStale < vecTimes.Count() ) && ( vecTimes[nStale] < nStaleTime ) )
	{
		nStale++;
	}
	vecTimes.RemoveMultipleFromHead( nStale );

	//see if we have room in how many asserts we want to track, if so, we want to record this assert
	if ( ( vecTimes.Count() < cv_assert_max_minidumps_in_window.GetInt() ) || ( CDisableAssertRateLimit::s_nDisabledCount > 0 ) )
	{
		vecTimes.AddToTail( CRTime::RTime32TimeCur() );
		pAssert->m_nTotalRecorded++;
		if( pbShouldWriteMinidump )
			*pbShouldWriteMinidump = true;
	}
}

//flag indicating whether or not we should force a crash if we encounter an exit
static bool g_bCrashIfExitDetected = false;

//callback handler registered to force a crash on exit conditions so we can track when/why the GC ever exits
static void GCForceCrash( bool bForceCrash )
{
	if( bForceCrash )
	{
		//we just want to initiate a crash, so that we can get a call stack
		int* pForceCrash = NULL;
		*pForceCrash = 100;
	}
}

static void ExitHandler()				{ GCForceCrash( g_bCrashIfExitDetected ); }
static void AbortHandler( int )			{ GCForceCrash( true ); }
static void PureCallHandler()			{ GCForceCrash( true ); }

static void InvalidCRTParamHandler(const wchar_t* expression,
	const wchar_t* function, 
	const wchar_t* file, 
	unsigned int line, 
	uintptr_t pReserved)	{ GCForceCrash( true ); }

static void InstallExceptionHandlers( bool bCrashOnNormalExit )
{
	//don't crash on exit while in dev universe
	g_bCrashIfExitDetected = bCrashOnNormalExit;
	Plat_CollectMiniDumpsForFatalErrors();
	//and register one with the at exit handler
	atexit( ExitHandler );	
	//and register an abort handler
	signal( SIGABRT, AbortHandler );
	//CRT invalid parameter handler
	_set_invalid_parameter_handler( InvalidCRTParamHandler );
	//Pure virtual function call handler
	_set_purecall_handler( PureCallHandler );

	MiniDumpRegisterForUnhandledExceptions();
}

//-----------------------------------------------------------------------------
// Purpose: Loads the config, figures out what GC we should be running, and
//	creates it
//-----------------------------------------------------------------------------
bool CGCInterface::BAsyncInit( uint32 unAppID, const char *pchDebugName, int iGCIndex, IGameCoordinatorHost *pHost )
{
	//called to handle registration of exception handlers so that we will always crash rather than an unexpected termination
	InstallExceptionHandlers( pHost->GetUniverse() != k_EUniverseDev );	

	// Make sure we can't deploy debug GCs outside the dev environment
#ifdef _DEBUG
	if ( pHost->GetUniverse() != k_EUniverseDev )
	{
		pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, 
			CFmtStr( "The GC for App %u is a debug binary. Shutting down.\n", unAppID ) );
		return false;
	}
#endif

	//report if we are 64 or 32 bit for easier tracking during transition
	COMPILE_TIME_ASSERT( sizeof( tchar ) == sizeof( char ) );
	pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, CFmtStr( "Initializing %d bit GC, Dir Index %d, PID:%u\n", ( uint32 )( sizeof( void* ) * 8 ), iGCIndex, GetCurrentProcessId() ).Access() );
	pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, CFmtStr1024( "Command Line: %s\n", Plat_GetCommandLine() ).Access() );

	CommandLine()->CreateCmdLine( Plat_GetCommandLine() );

	//get our machine name
	{
		char szMachineName[ MAX_COMPUTERNAME_LENGTH + 1 ];
		DWORD nBufferSize = ARRAYSIZE( szMachineName );
		GetComputerName( szMachineName, &nBufferSize );
		m_sMachineName = szMachineName;
	}

	//open our a handle to our parent
	{		
		uint32 nParentPID = MAX( 0, CommandLine()->ParmValue( "-parentpid", 0 ) );
		if( nParentPID == 0 )
		{
			pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, "Parent process ID was not specified via -parentpid, unable to get information about the launching process\n" );
		}
		else
		{
			m_hParentProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, nParentPID );
			if( !m_hParentProcess )
			{
				pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, "Unable to open the parent process with read access. Unable to get information about the launching process\n" );
			}
		}
	}

	static bool s_bInitCalled = false;
	if ( s_bInitCalled )
	{
		pHost->EmitSpew( SPEW_GC.GetName(), SPEW_ERROR, SPEW_ALWAYS, LOG_ALWAYS, "BInit called twice on the game IGameCoordinator" );
		return false;
	}
	s_bInitCalled = true;
	
	// Set basic variables
	m_pGCHost = pHost;
	m_nAppID = unAppID;
	m_sDebugName = pchDebugName;
	m_eUniverse = (EUniverse)m_pGCHost->GetUniverse();

	// Initialize core systems
	CRTime::UpdateRealTime();
	RandomSeed( CRTime::RTime32TimeCur() );
	
	// Gets the path our dll is loaded from
	HMODULE hModuleGC;
	char rgchGCModuleFile[MAX_PATH+1] = ".\\";
	char rgchGCModulePath[MAX_PATH+1] = ".\\";
	if ( ::GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&g_GCInterface, &hModuleGC ) )
	{
		::GetModuleFileNameA( hModuleGC, rgchGCModuleFile, MAX_PATH );
		V_strcpy_safe( rgchGCModulePath, rgchGCModuleFile );
		if ( char *pSlash = strrchr( rgchGCModulePath, '\\' ) )
			pSlash[1] = 0;
	}
	// Full path to GC.DLL (with final slash) is now in rgchGCModulePath
	m_sGCDLLPath = rgchGCModulePath;

	CFmtStr sContentPath = rgchGCModulePath;
	CFmtStr sBinaryPath = rgchGCModulePath;
	if( Q_stristr( rgchGCModulePath, "bin\\gc\\x64" ) != NULL )
	{
		Q_MakeAbsolutePath( sContentPath.Access(), FMTSTR_STD_LEN, "..\\..\\..", rgchGCModulePath );
		Q_MakeAbsolutePath( sBinaryPath.Access(), FMTSTR_STD_LEN, "..\\..\\..\\..\\bin\\x64", rgchGCModulePath );
		m_bDevMode = true;
		m_sDevBinaryName = rgchGCModuleFile;
	}
	else if( Q_stristr( rgchGCModulePath, "bin\\gc" ) != NULL )
	{
		Q_MakeAbsolutePath( sContentPath.Access(), FMTSTR_STD_LEN, "..\\..", rgchGCModulePath );
		Q_MakeAbsolutePath( sBinaryPath.Access(), FMTSTR_STD_LEN, "..\\..\\..\\bin", rgchGCModulePath );
		m_bDevMode = true;
		m_sDevBinaryName = rgchGCModuleFile;
	}
	else
	{
		//launch through standard GC, so try and extract the version from our path (not a great solution, should extend interface so that
		//the GCH provides us with the version it expects). The format is ....\vNNN\ so try and extract that
		CUtlString sGCPath = rgchGCModulePath;
		sGCPath.StripTrailingSlash();
		if ( const char *pSlash = strrchr( sGCPath.Get(), '\\' ) )
		{
			//skip over the slash, and verify that we have a 'v' before the version
			if( tolower( pSlash[ 1 ] ) == 'v' )
			{
				//grab the version number
				m_nVersion = ( uint32 )max( 0, atoi( pSlash + 2 ) );
			}
		}
	}

	// Starts logging
	LoggingSystem_PushLoggingState();
	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	LoggingSystem_RegisterLoggingListener( &s_ConsoleLoggingListener );

	// Select folder and prefix for dumps (using just the dir index for now, but update this later once we have the name
	Tier0GenericMiniDumpHandler_SetFilenamePrefix( CFmtStr( "dumps\\gc%d_idx%d", unAppID, iGCIndex ) );

	// Make sure dialogs don't come up and hang the process in production
	if ( !m_bDevMode )
	{
		Plat_EnableHeadlessMode();
	}
	RegisterAssertionFailureListener( &sg_GCAssertionFailureHandler );

	// Initialize GIDs
	m_ullGID = 0;
	m_ullGID = (uint64)iGCIndex << 56;						// 8 bits of process id
	m_ullGID |= (uint64)CRTime::RTime32TimeCur() << 24;     // 32 bits of UTC time in seconds
	// 24 bits/second of incremental counter space

	// This system assumes there are less than 256 GCs. Make sure of that
	AssertMsg( iGCIndex >= 0 && iGCIndex < 256, "iGCIndex out of range. There can only be 256 GC processes for an app" );
	if ( iGCIndex < 0 || iGCIndex >= 256 )
		return false;

	// Make sure the protobuf library won't exitprocess without dumping
	::google::protobuf::SetLogHandler( ProtobufLogHandler );

	g_gcAppSystemGroup.SetPath( sBinaryPath );
	if( g_gcAppSystemGroup.Startup() < 0 )
		return false;

	g_pFullFileSystem->AddSearchPath( sContentPath, "GAME" );
	g_pFullFileSystem->AddSearchPath( rgchGCModulePath, "CONFIG" ); // config files go with gc.dll

	// load the config file first thing so that we can use it for all the other startup code
	KeyValuesAD configKeys( "config" );
	if( !BReadConfigDirectory( configKeys ) )
	{		
		return false;
	}
	
	// Find this GC in the config and create it
	m_pGCDirProcess = GDirectory()->GetProcess( iGCIndex );
	if ( NULL == m_pGCDirProcess )
	{
		GCSDK::EmitError( SPEW_CONSOLE, "Failed to start GC for index %d.\n", iGCIndex );
		return false;
	}

	CDirectory::GCFactory_t pfnFactory = GDirectory()->GetFactoryForProcessType( m_pGCDirProcess->GetProcessType() );
	if ( NULL == pfnFactory )
	{
		GCSDK::EmitError( SPEW_CONSOLE, "Failed to start GC for index %d (type %s). Got a NULL factory function, likely missing registration for this type\n", iGCIndex, m_pGCDirProcess->GetProcessType() );
		return false;
	}

	//now that we have more information about which GC we are, update our minidump name to reflect this
	Tier0GenericMiniDumpHandler_SetFilenamePrefix( CFmtStr( "dumps\\gc%d_%s", unAppID, m_pGCDirProcess->GetName() ) );

	//now that we know our GC type, we can actually load up our convars (which are dependent on this info)
	if(	!BReadConvars( configKeys ) )
		return false;

	// Init the GC. Not passing along the host because the interface layer owns it
	// and chooses what to expose
	m_pGC = pfnFactory( m_pGCDirProcess );
	return m_pGC->BAsyncInit( unAppID, pchDebugName, iGCIndex, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Generates a number that's guaranteed unique across all GC processes
//	for this app. It is also guaranteed to have never been used by previous
//	processes.
//-----------------------------------------------------------------------------
GID_t CGCInterface::GenerateGID()
{
	return ++m_ullGID;
}

//we have the GC index encoded in the high bits when we init the gid, so just extract that
uint32 CGCInterface::GetGCDirIndexFromGID( GID_t gid )
{
	return ( uint32 )( gid >> 56 );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the universe the GC is currently running in
//-----------------------------------------------------------------------------
EUniverse CGCInterface::GetUniverse() const
{
	// Gets the current universe
	return m_eUniverse;
}


//-----------------------------------------------------------------------------
// Purpose: Wrappers for GCHost functions that allow GCInterface to hook the calls
//-----------------------------------------------------------------------------
bool CGCInterface::BProcessSystemMessage( uint32 unGCSysMsgType, const void *pubData, uint32 cubData )
{
	AssertMsg( unGCSysMsgType != 0, "Message sent without an ID. Did you use the wrong constructor?" );

	//track this message that we are sending (always just strip off the protobuff flag so it works with all message types)
	g_theMessageList.TallySendMessage( unGCSysMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_System );

	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - ProcessSystemMessage", VPROF_BUDGETGROUP_STEAM );
		return m_pGCHost->BProcessSystemMessage( unGCSysMsgType, pubData, cubData );
	}
}


//-----------------------------------------------------------------------------
bool CGCInterface::BSendMessageToClient( uint64 ullSteamID, uint32 unMsgType, const void *pubData, uint32 cubData )
{
	AssertMsg( unMsgType != 0, "Message sent without an ID. Did you use the wrong constructor?" );

	//sanity check on our side that we are sending with a valid steam ID. Useful to catch message failures on the GC side since otherwise it must be caught in the GCH side
	if( ullSteamID == k_steamIDNil.ConvertToUint64() )
	{
		AssertMsg( false, "Message %d sent to invalid steam ID. This message will not be processed.", unMsgType & ~k_EMsgProtoBufFlag );
		return false;
	}

	//track this message that we are sending (always just strip off the protobuff flag so it works with all message types)
	g_theMessageList.TallySendMessage( unMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_Client );

	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - SendMessageToClient", VPROF_BUDGETGROUP_STEAM );
		return m_pGCHost->BSendMessageToClient( ullSteamID, unMsgType, pubData, cubData );
	}
}


//-----------------------------------------------------------------------------
bool CGCInterface::BSendMessageToGC( int iGCServerIDTarget, uint32 unMsgType, const void *pubData, uint32 cubData )
{
	AssertMsg( unMsgType != 0, "Message sent without an ID. Did you use the wrong constructor?" );

	//track this message that we are sending (always just strip off the protobuff flag so it works with all message types)
	g_theMessageList.TallySendMessage( unMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_GC );

	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - SendMessageToGC", VPROF_BUDGETGROUP_STEAM );
		return m_pGCHost->BSendMessageToGC( iGCServerIDTarget, unMsgType, pubData, cubData );
	}
}

//-----------------------------------------------------------------------------
void CGCInterface::AddBlockEmitString( const char* pszStr, bool bBlockConsole, bool bBlockLog )
{
	BlockString_t* pStr = new BlockString_t;
	pStr->m_sStr			= pszStr;
	pStr->m_bBlockConsole	= bBlockConsole;
	pStr->m_bBlockLog		= bBlockLog;
	m_BlockEmitStrings.AddToTail( pStr );
}

//-----------------------------------------------------------------------------
void CGCInterface::ClearBlockEmitStrings()
{
	m_BlockEmitStrings.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
void CGCInterface::EmitSpew( const char *pchGroupName, SpewType_t spewType, int iSpewLevel, int iLevelLog, const char *pchMsg )
{
	//see if this output is being squelched by going through our blocked string
	FOR_EACH_VEC( m_BlockEmitStrings, nStr )
	{
		//if our output contains the blocked string, turn off the severity for that level
		const BlockString_t* pStr = m_BlockEmitStrings[ nStr ];
		if( V_stristr( pchMsg, pStr->m_sStr ) != NULL )
		{
			if( pStr->m_bBlockConsole )
				iSpewLevel = SPEW_NEVER;
			if( pStr->m_bBlockLog )
				iLevelLog = LOG_NEVER;
		}
	}

	//see if this is just a blank line
	if( filter_blank_lines.GetBool() && pchMsg )
	{
		bool bIsBlankLine = true;
		for( const char* pszCurrChar = pchMsg; *pszCurrChar; pszCurrChar++ )
		{
			if( !V_isspace( *pszCurrChar ) )
			{
				bIsBlankLine = false;
				break;
			}
		}
		if( bIsBlankLine )
		{
			return;
		}
	}

	if ( m_bLogCaptureEnabled )
	{
		m_vecLogCapture[m_vecLogCapture.AddToTail()].Set( pchMsg );
	}

	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - EmitMessage", VPROF_BUDGETGROUP_STEAM );
		m_pGCHost->EmitSpew( pchGroupName, spewType, iSpewLevel, iLevelLog, pchMsg );
	}
}


//-----------------------------------------------------------------------------
void CGCInterface::AsyncSQLQuery( IGCSQLQuery *pQuery, int eSchemaCatalog )
{
	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );
	{
		VPROF_BUDGET( "GCHost - SQLQuery", VPROF_BUDGETGROUP_STEAM );
		m_pGCHost->AsyncSQLQuery( pQuery, eSchemaCatalog );
	}
}


//-----------------------------------------------------------------------------
void CGCInterface::SetStartupComplete( bool bSuccess )
{
	m_pGCHost->StartupComplete( bSuccess );
}


//-----------------------------------------------------------------------------
void CGCInterface::SetShutdownComplete()
{
	m_pGCHost->ShutdownComplete();
}


//-----------------------------------------------------------------------------
// Purpose: Passthrough implementations of the rest of the interface
//-----------------------------------------------------------------------------
void CGCInterface::Unload()
{
	if ( m_pGC )
		m_pGC->Unload();

	g_gcAppSystemGroup.Shutdown();
}


//-----------------------------------------------------------------------------
bool CGCInterface::BAsyncShutdown()
{
	bool bResult = false;
	if ( m_pGC )
		bResult = m_pGC->BAsyncShutdown();

	//if they have requested a shutdown, go ahead and allow exit
	g_bCrashIfExitDetected = false;

	return bResult;
}


//-----------------------------------------------------------------------------
bool CGCInterface::BMainLoopOncePerFrame( uint64 ulLimitMicroseconds )
{
	if ( !m_pGC )
		return false;
	else
		return m_pGC->BMainLoopOncePerFrame( ulLimitMicroseconds );
}


//-----------------------------------------------------------------------------
bool CGCInterface::BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds )
{
	if ( !m_pGC )
		return false;
	else
		return m_pGC->BMainLoopUntilFrameCompletion( ulLimitMicroseconds );
}


//-----------------------------------------------------------------------------
void CGCInterface::HandleMessageFromClient( uint64 ullSenderID, uint32 unMsgType, void *pubData, uint32 cubData )
{
	if ( NULL == pubData || 0 == cubData )
	{
		EG_ERROR( g_EGMessages, "Received invalid message from user %s. MessageID: %u pubData: %p cubData: %u\n", CSteamID( ullSenderID ).Render(), unMsgType, pubData, cubData );
	}
	else if ( m_pGC )
	{
		g_theMessageList.TallyReceiveMessage( unMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_Client );
		m_pGC->HandleMessageFromClient( ullSenderID, unMsgType, pubData, cubData );
	}
}


//-----------------------------------------------------------------------------
void CGCInterface::HandleMessageFromSystem( uint32 unGCSysMsgType, void *pubData, uint32 cubData )
{
	if ( NULL == pubData || 0 == cubData )
	{
		EG_ERROR( g_EGMessages, "Received invalid message from system. MessageID: %u pubData: %p cubData: %u\n", unGCSysMsgType, pubData, cubData );
	}
	else if ( m_pGC )
	{
		g_theMessageList.TallyReceiveMessage( unGCSysMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_System );
		m_pGC->HandleMessageFromSystem( unGCSysMsgType, pubData, cubData );
	}
}


//-----------------------------------------------------------------------------
void CGCInterface::HandleMessageFromGC( int iGCServerIDSender, uint32 unMsgType, void *pubData, uint32 cubData )
{
	if ( NULL == pubData || 0 == cubData )
	{
		EG_ERROR( g_EGMessages, "Received invalid message from GC. MessageID: %u pubData: %p cubData: %u\n", iGCServerIDSender, pubData, cubData );
	}
	else if ( m_pGC )
	{
		g_theMessageList.TallyReceiveMessage( unMsgType & ~k_EMsgProtoBufFlag, cubData, eMsgSource_GC );
		m_pGC->HandleMessageFromGC( iGCServerIDSender, unMsgType, pubData, cubData );
	}
}


//-----------------------------------------------------------------------------
void CGCInterface::StartLogCapture()
{
	m_bLogCaptureEnabled = true;
}


//-----------------------------------------------------------------------------
void CGCInterface::EndLogCapture()
{
	m_bLogCaptureEnabled = false;
}


//-----------------------------------------------------------------------------
const CUtlVector<CUtlString> *CGCInterface::GetLogCapture()
{
	return &m_vecLogCapture;
}


//-----------------------------------------------------------------------------
void CGCInterface::ClearLogCapture()
{
	m_vecLogCapture.RemoveAll();
}


}

//For the GC, we want to force a crash when we get stack corruption errors so
// that we can analyze the dumps and fix the problem. To disable this behavior,
// comment out the function below.

// TEMP: Disabled on VS2013+ because I didn't know how to fix the linking problems
#if defined( _MSC_VER ) && ( _MSC_VER < 1800 )
extern "C"
{
	#if defined (_X86_)
	__declspec(noreturn) void __cdecl __report_gsfailure(void)
	#else  /* defined (_X86_) */
	__declspec(noreturn) void __cdecl __report_gsfailure(ULONGLONG StackCookie)
	#endif  /* defined (_X86_) */
	{
		MiniDumpOptionalData_t optionalData( _T("stack_corruption") );
		InvokeMiniDumpHandler( NULL, &optionalData );
		static uint32* pNull = NULL;
		*pNull = 0;
	}
}
#endif