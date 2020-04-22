//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// Defines the entry point for the application.
//
//===========================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include "shlwapi.h" // registry stuff
#include <direct.h>
#elif defined ( LINUX ) || defined( OSX )
	#define O_EXLOCK 0
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <locale.h>
#elif defined ( _X360 )
#else
#error
#endif
#include "appframework/ilaunchermgr.h"
#include <stdio.h>
#include "tier0/icommandline.h"
#include "engine_launcher_api.h"
#include "tier0/vcrmode.h"
#include "ifilesystem.h"
#include "tier1/interface.h"
#include "tier0/dbg.h"
#include "iregistry.h"
#include "appframework/IAppSystem.h"
#include "appframework/AppFramework.h"
#include <vgui/VGUI.h>
#include <vgui/ISurface.h>
#include "tier0/platform.h"
#include "tier0/memalloc.h"
#include "filesystem.h"
#include "tier1/utlrbtree.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "IHammer.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem_init.h"
#include "vstdlib/iprocessutils.h"
#include "video/ivideoservices.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "p4lib/ip4.h"
#include "inputsystem/iinputsystem.h"
#include "filesystem/IQueuedLoader.h"
#include "reslistgenerator.h"
#include "tier1/fmtstr.h"
#include "sourcevr/isourcevirtualreality.h"

#define VERSION_SAFE_STEAM_API_INTERFACES
#include "steam/steam_api.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_console.h"
#include "xbox/xbox_launch.h"
#endif

#if defined( USE_SDL )
#include "SDL.h"

#if !defined( _WIN32 )
#define MB_OK 			0x00000001
#define MB_SYSTEMMODAL	0x00000002
#define MB_ICONERROR	0x00000004
int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType );
#endif // _WIN32

#endif // USE_SDL

#if defined( POSIX )
#define RELAUNCH_FILE "/tmp/hl2_relaunch"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEFAULT_HL2_GAMEDIR	"hl2"

#if defined( USE_SDL )
extern void* CreateSDLMgr();
#endif

//-----------------------------------------------------------------------------
// Modules...
//-----------------------------------------------------------------------------
static IEngineAPI *g_pEngineAPI;
static IHammer *g_pHammer;

bool g_bTextMode = false;

static char g_szBasedir[MAX_PATH];
static char g_szGamedir[MAX_PATH];

// copied from sys.h
struct FileAssociationInfo
{
	char const  *extension;
	char const  *command_to_issue;
};

static FileAssociationInfo g_FileAssociations[] =
{
	{ ".dem", "playdemo" },
	{ ".sav", "load" },
	{ ".bsp", "map" },
};

#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg(lib)
#endif

class CLeakDump
{
public:
	CLeakDump()
	 :	m_bCheckLeaks( false )
	{
	}

	~CLeakDump()
	{
		if ( m_bCheckLeaks )
		{
			MemAlloc_DumpStats();
		}
	}

	bool m_bCheckLeaks;
} g_LeakDump;

//-----------------------------------------------------------------------------
// Spew function!
//-----------------------------------------------------------------------------
SpewRetval_t LauncherDefaultSpewFunc( SpewType_t spewType, char const *pMsg )
{
#ifndef _CERT
#ifdef WIN32
	OutputDebugStringA( pMsg );
#else
	fprintf( stderr, "%s", pMsg );
#endif
	
	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_LOG:
		return SPEW_CONTINUE;

	case SPEW_WARNING:
		if ( !stricmp( GetSpewOutputGroup(), "init" ) )
		{
#if defined( WIN32 ) || defined( USE_SDL )
			::MessageBox( NULL, pMsg, "Warning!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
#endif
		}
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
		if ( !ShouldUseNewAssertDialog() )
		{
#if defined( WIN32 ) || defined( USE_SDL )
			::MessageBox( NULL, pMsg, "Assert!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
#endif
		}
		return SPEW_DEBUGGER;
	
	case SPEW_ERROR:
	default:
#if defined( WIN32 ) || defined( USE_SDL )
		::MessageBox( NULL, pMsg, "Error!", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );
#endif
		_exit( 1 );
	}
#else
	if ( spewType != SPEW_ERROR)
		return SPEW_CONTINUE;
	_exit( 1 );
#endif
}


//-----------------------------------------------------------------------------
// Implementation of VCRHelpers.
//-----------------------------------------------------------------------------
class CVCRHelpers : public IVCRHelpers
{
public:
	virtual void ErrorMessage( const char *pMsg )
	{
#if defined( WIN32 ) || defined( LINUX )
		NOVCR( ::MessageBox( NULL, pMsg, "VCR Error", MB_OK ) );
#endif
	}

	virtual void* GetMainWindow()
	{
		return NULL;
	}
};

static CVCRHelpers g_VCRHelpers;

//-----------------------------------------------------------------------------
// Purpose: Return the game directory
// Output : char
//-----------------------------------------------------------------------------
char *GetGameDirectory( void )
{
	return g_szGamedir;
}

void SetGameDirectory( const char *game )
{
	Q_strncpy( g_szGamedir, game, sizeof(g_szGamedir) );
}

//-----------------------------------------------------------------------------
// Gets the executable name
//-----------------------------------------------------------------------------
bool GetExecutableName( char *out, int outSize )
{
#ifdef WIN32
	if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, outSize ) )
	{
		return false;
	}
	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return the base directory
// Output : char
//-----------------------------------------------------------------------------
char *GetBaseDirectory( void )
{
	return g_szBasedir;
}

//-----------------------------------------------------------------------------
// Purpose: Determine the directory where this .exe is running from
//-----------------------------------------------------------------------------
void UTIL_ComputeBaseDir()
{
	g_szBasedir[0] = 0;

	if ( IsX360() )
	{
		char const *pBaseDir = CommandLine()->ParmValue( "-basedir" );
		if ( pBaseDir )
		{
			strcpy( g_szBasedir, pBaseDir );
		}
	}

	if ( !g_szBasedir[0] && GetExecutableName( g_szBasedir, sizeof( g_szBasedir ) ) )
	{
		char *pBuffer = strrchr( g_szBasedir, '\\' );
		if ( *pBuffer )
		{
			*(pBuffer+1) = '\0';
		}

		int j = strlen( g_szBasedir );
		if (j > 0)
		{
			if ( ( g_szBasedir[j-1] == '\\' ) || 
				 ( g_szBasedir[j-1] == '/' ) )
			{
				g_szBasedir[j-1] = 0;
			}
		}
	}

	if ( IsPC() )
	{
		char const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );
		if ( pOverrideDir )
		{
			strcpy( g_szBasedir, pOverrideDir );
		}
	}

#ifdef WIN32
	Q_strlower( g_szBasedir );
#endif
	Q_FixSlashes( g_szBasedir );
}

#ifdef WIN32
BOOL WINAPI MyHandlerRoutine( DWORD dwCtrlType )
{
#if !defined( _X360 )
	TerminateProcess( GetCurrentProcess(), 2 );
#endif
	return TRUE;
}
#endif

void InitTextMode()
{
#ifdef WIN32
#if !defined( _X360 )
	AllocConsole();

	SetConsoleCtrlHandler( MyHandlerRoutine, TRUE );

	freopen( "CONIN$", "rb", stdin );		// reopen stdin handle as console window input
	freopen( "CONOUT$", "wb", stdout );		// reopen stout handle as console window output
	freopen( "CONOUT$", "wb", stderr );		// reopen stderr handle as console window output
#else
	XBX_Error( "%s %s: Not Supported", __FILE__, __LINE__ );
#endif
#endif
}

void SortResList( char const *pchFileName, char const *pchSearchPath );

#define ALL_RESLIST_FILE	"all.lst"
#define ENGINE_RESLIST_FILE  "engine.lst"

// create file to dump out to
class CLogAllFiles
{
public:
	CLogAllFiles();
	void Init();
	void Shutdown();
	void LogFile( const char *fullPathFileName, const char *options );

private:
	static void LogAllFilesFunc( const char *fullPathFileName, const char *options );
	void LogToAllReslist( char const *line );

	bool		m_bActive;
	char		m_szCurrentDir[_MAX_PATH];

	// persistent across restarts
	CUtlRBTree< CUtlString, int > m_Logged;
	CUtlString	m_sResListDir;
	CUtlString	m_sFullGamePath;
};

static CLogAllFiles g_LogFiles;

static bool AllLogLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

CLogAllFiles::CLogAllFiles() :
	m_bActive( false ),
	m_Logged( 0, 0, AllLogLessFunc )
{
	MEM_ALLOC_CREDIT();
	m_sResListDir = "reslists";
}

void CLogAllFiles::Init()
{
	if ( IsX360() )
	{
		return;
	}

	// Can't do this in edit mode
	if ( CommandLine()->CheckParm( "-edit" ) )
	{
		return;
	}

	if ( !CommandLine()->CheckParm( "-makereslists" ) )
	{
		return;
	}

	m_bActive = true;

	char const *pszDir = NULL;
	if ( CommandLine()->CheckParm( "-reslistdir", &pszDir ) && pszDir )
	{
		char szDir[ MAX_PATH ];
		Q_strncpy( szDir, pszDir, sizeof( szDir ) );
		Q_StripTrailingSlash( szDir );
#ifdef WIN32
		Q_strlower( szDir );
#endif
		Q_FixSlashes( szDir );
		if ( Q_strlen( szDir ) > 0 )
		{
			m_sResListDir = szDir;
		}
	}

	// game directory has not been established yet, must derive ourselves
	char path[MAX_PATH];
	Q_snprintf( path, sizeof(path), "%s/%s", GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );
	Q_FixSlashes( path );
#ifdef WIN32
	Q_strlower( path );
#endif
	m_sFullGamePath = path;

	// create file to dump out to
	char szDir[ MAX_PATH ];
	V_snprintf( szDir, sizeof( szDir ), "%s\\%s", m_sFullGamePath.String(), m_sResListDir.String() );
	g_pFullFileSystem->CreateDirHierarchy( szDir, "GAME" );

	g_pFullFileSystem->AddLoggingFunc( &LogAllFilesFunc );

	if ( !CommandLine()->FindParm( "-startmap" ) && !CommandLine()->FindParm( "-startstage" ) )
	{
		m_Logged.RemoveAll();
		g_pFullFileSystem->RemoveFile( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "GAME" );
	}

#ifdef WIN32
	::GetCurrentDirectory( sizeof(m_szCurrentDir), m_szCurrentDir );
	Q_strncat( m_szCurrentDir, "\\", sizeof(m_szCurrentDir), 1 );
	_strlwr( m_szCurrentDir );
#else
	getcwd( m_szCurrentDir, sizeof(m_szCurrentDir) );
	Q_strncat( m_szCurrentDir, "/", sizeof(m_szCurrentDir), 1 );
#endif
}

void CLogAllFiles::Shutdown()
{
	if ( !m_bActive )
		return;

	m_bActive = false;

	if ( CommandLine()->CheckParm( "-makereslists" ) )
	{
		g_pFullFileSystem->RemoveLoggingFunc( &LogAllFilesFunc );
	}

	// Now load and sort all.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "GAME" );
	// Now load and sort engine.lst
	SortResList( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ENGINE_RESLIST_FILE ), "GAME" );

	m_Logged.Purge();
}

void CLogAllFiles::LogToAllReslist( char const *line )
{
	// Open for append, write data, close.
	FileHandle_t fh = g_pFullFileSystem->Open( CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sResListDir.String(), ALL_RESLIST_FILE ), "at", "GAME" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write("\"", 1, fh);
		g_pFullFileSystem->Write( line, Q_strlen(line), fh );
		g_pFullFileSystem->Write("\"\n", 2, fh);
		g_pFullFileSystem->Close( fh );
	}
}

void CLogAllFiles::LogFile(const char *fullPathFileName, const char *options)
{
	if ( !m_bActive )
	{
		Assert( 0 );
		return;
	}

	// write out to log file
	Assert( fullPathFileName[1] == ':' );

	int idx = m_Logged.Find( fullPathFileName );
	if ( idx != m_Logged.InvalidIndex() )
	{
		return;
	}

	m_Logged.Insert( fullPathFileName );

	// make it relative to our root directory
	const char *relative = Q_stristr( fullPathFileName, GetBaseDirectory() );
	if ( relative )
	{
		relative += ( Q_strlen( GetBaseDirectory() ) + 1 );

		char rel[ MAX_PATH ];
		Q_strncpy( rel, relative, sizeof( rel ) );
#ifdef WIN32
		Q_strlower( rel );
#endif
		Q_FixSlashes( rel );
		
		LogToAllReslist( rel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: callback function from filesystem
//-----------------------------------------------------------------------------
void CLogAllFiles::LogAllFilesFunc(const char *fullPathFileName, const char *options)
{
	g_LogFiles.LogFile( fullPathFileName, options );
}

//-----------------------------------------------------------------------------
// Purpose: This is a bit of a hack because it appears 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static bool IsWin98OrOlder()
{
	bool retval = false;

#if defined( WIN32 ) && !defined( _X360 )
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	BOOL bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);
	if( !bOsVersionInfoEx )
	{
		// If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if ( !GetVersionEx ( (OSVERSIONINFO *) &osvi) )
		{
			Error( "IsWin98OrOlder:  Unable to get OS version information" );
		}
	}

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		// NT, XP, Win2K, etc. all OK for SSE
		break;
	case VER_PLATFORM_WIN32_WINDOWS:
		// Win95, 98, Me can't do SSE
		retval = true;
		break;
	case VER_PLATFORM_WIN32s:
		// Can't really run this way I don't think...
		retval = true;
		break;
	default:
		break;
	}
#endif

	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Figure out if Steam is running, then load the GameOverlayRenderer.dll
//-----------------------------------------------------------------------------
void TryToLoadSteamOverlayDLL()
{
#if defined( WIN32 ) && !defined( _X360 )
	// First, check if the module is already loaded, perhaps because we were run from Steam directly
	HMODULE hMod = GetModuleHandle( "GameOverlayRenderer" DLL_EXT_STRING );
	if ( hMod )
	{
		return;
	}

	if ( 0 == GetEnvironmentVariableA( "SteamGameId", NULL, 0 ) )
	{
		// Initializing the Steam client API has the side effect of setting up the AppId
		// which is immediately queried in GameOverlayRenderer.dll's DllMain entry point
		if( SteamAPI_InitSafe() )
		{
			const char *pchSteamInstallPath = SteamAPI_GetSteamInstallPath();
			if ( pchSteamInstallPath )
			{
				char rgchSteamPath[MAX_PATH];
				V_ComposeFileName( pchSteamInstallPath, "GameOverlayRenderer" DLL_EXT_STRING, rgchSteamPath, Q_ARRAYSIZE(rgchSteamPath) );
				// This could fail, but we can't fix it if it does so just ignore failures
				LoadLibrary( rgchSteamPath );
			
			}

			SteamAPI_Shutdown();
		}
	}

#endif
}

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to 
//-----------------------------------------------------------------------------
class CSourceAppSystemGroup : public CSteamAppSystemGroup
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	const char *DetermineDefaultMod();
	const char *DetermineDefaultGame();

	bool m_bEditMode;
};


//-----------------------------------------------------------------------------
// The dirty disk error report function
//-----------------------------------------------------------------------------
void ReportDirtyDiskNoMaterialSystem()
{
#ifdef _X360
	for ( int i = 0; i < 4; ++i )
	{
		if ( XUserGetSigninState( i ) != eXUserSigninState_NotSignedIn )
		{
			XShowDirtyDiscErrorUI( i );
			return;
		}
	}
	XShowDirtyDiscErrorUI( 0 );
#endif
}


//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CSourceAppSystemGroup::Create()
{
	IFileSystem *pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	pFileSystem->InstallDirtyDiskReportFunc( ReportDirtyDiskNoMaterialSystem );

#ifdef WIN32
	CoInitialize( NULL );
#endif

	// Are we running in edit mode?
	m_bEditMode = CommandLine()->CheckParm( "-edit" );

	double st = Plat_FloatTime();

	AppSystemInfo_t appSystems[] = 
	{
		{ "engine" DLL_EXT_STRING,			CVAR_QUERY_INTERFACE_VERSION },	// NOTE: This one must be first!!
		{ "inputsystem" DLL_EXT_STRING,		INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem" DLL_EXT_STRING,	MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "datacache" DLL_EXT_STRING,		DATACACHE_INTERFACE_VERSION },
		{ "datacache" DLL_EXT_STRING,		MDLCACHE_INTERFACE_VERSION },
		{ "datacache" DLL_EXT_STRING,		STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "studiorender" DLL_EXT_STRING,	STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics" DLL_EXT_STRING,		VPHYSICS_INTERFACE_VERSION },
		{ "video_services" DLL_EXT_STRING,  VIDEO_SERVICES_INTERFACE_VERSION },
  
		// NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface implementation
		{ "vguimatsurface" DLL_EXT_STRING,	VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2" DLL_EXT_STRING,			VGUI_IVGUI_INTERFACE_VERSION },
		{ "engine" DLL_EXT_STRING,			VENGINE_LAUNCHER_API_VERSION },

		{ "", "" }							// Required to terminate the list
	};

#if defined( USE_SDL )
	AddSystem( (IAppSystem *)CreateSDLMgr(), SDLMGR_INTERFACE_VERSION );
#endif

	if ( !AddSystems( appSystems ) ) 
		return false;


	// This will be NULL for games that don't support VR. That's ok. Just don't load the DLL
	AppModule_t sourceVRModule = LoadModule( "sourcevr" DLL_EXT_STRING );
	if( sourceVRModule != APP_MODULE_INVALID )
	{
		AddSystem( sourceVRModule, SOURCE_VIRTUAL_REALITY_INTERFACE_VERSION );
	}

	// pull in our filesystem dll to pull the queued loader from it, we need to do it this way due to the 
	// steam/stdio split for our steam filesystem
	char pFileSystemDLL[MAX_PATH];
	bool bSteam;
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, bSteam ) != FS_OK )
		return false;

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	AddSystem( fileSystemModule, QUEUEDLOADER_INTERFACE_VERSION );

	// Hook in datamodel and p4 control if we're running with -tools
	if ( IsPC() && ( ( CommandLine()->FindParm( "-tools" ) && !CommandLine()->FindParm( "-nop4" ) ) || CommandLine()->FindParm( "-p4" ) ) )
	{
#ifdef STAGING_ONLY
		AppModule_t p4libModule = LoadModule( "p4lib" DLL_EXT_STRING );
		IP4 *p4 = (IP4*)AddSystem( p4libModule, P4_INTERFACE_VERSION );
		
		// If we are running with -steam then that means the tools are being used by an SDK user. Don't exit in this case!
		if ( !p4 && !CommandLine()->FindParm( "-steam" ) )
		{
			return false;
		}
#endif // STAGING_ONLY

		AppModule_t vstdlibModule = LoadModule( "vstdlib" DLL_EXT_STRING );
		IProcessUtils *processUtils = ( IProcessUtils* )AddSystem( vstdlibModule, PROCESS_UTILS_INTERFACE_VERSION );
		if ( !processUtils )
			return false;
	}

	// Connect to iterfaces loaded in AddSystems that we need locally
	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
		return false;

	g_pEngineAPI = (IEngineAPI*)FindSystem( VENGINE_LAUNCHER_API_VERSION );

	// Load the hammer DLL if we're in editor mode
#if defined( _WIN32 ) && defined( STAGING_ONLY )
	if ( m_bEditMode )
	{
		AppModule_t hammerModule = LoadModule( "hammer_dll" DLL_EXT_STRING );
		g_pHammer = (IHammer*)AddSystem( hammerModule, INTERFACEVERSION_HAMMER );
		if ( !g_pHammer )
		{
			return false;
		}
	}
#endif // defined( _WIN32 ) && defined( STAGING_ONLY )

	// Load up the appropriate shader DLL
	// This has to be done before connection.
	char const* pDLLName = "shaderapidx9" DLL_EXT_STRING;
	if ( CommandLine()->FindParm( "-noshaderapi" ) )
	{
		pDLLName = "shaderapiempty" DLL_EXT_STRING;
	}

	pMaterialSystem->SetShaderAPI( pDLLName );

	double elapsed = Plat_FloatTime() - st;
	COM_TimestampedLog( "LoadAppSystems:  Took %.4f secs to load libraries and get factories.", (float)elapsed );

	return true;
}

bool CSourceAppSystemGroup::PreInit()
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConVar_Register( );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pMaterialSystem )
		return false;

	CFSSteamSetupInfo steamInfo;
	steamInfo.m_bToolsMode = false;
	steamInfo.m_bSetSteamDLLPath = false;
	steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
	steamInfo.m_bOnlyUseDirectoryName = true;
	steamInfo.m_pDirectoryName = DetermineDefaultMod();
	if ( !steamInfo.m_pDirectoryName )
	{
		steamInfo.m_pDirectoryName = DetermineDefaultGame();
		if ( !steamInfo.m_pDirectoryName )
		{
			Error( "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game specified." );
		}
	}
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFullFileSystem;
	fsInfo.m_bToolsMode = m_bEditMode;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	if ( IsPC() || !IsX360() )
	{
		fsInfo.m_pFileSystem->AddSearchPath( "platform", "PLATFORM" );
	}
	else
	{
		// 360 needs absolute paths
		FileSystem_AddSearchPath_Platform( g_pFullFileSystem, steamInfo.m_GameInfoPath );
	}

	if ( IsPC() )
	{
		// This will get called multiple times due to being here, but only the first one will do anything
		reslistgenerator->Init( GetBaseDirectory(), CommandLine()->ParmValue( "-game", "hl2" ) );

		// This will also get called each time, but will actually fix up the command line as needed
		reslistgenerator->SetupCommandLine();
	}

	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Init();

	// Required to run through the editor
	if ( m_bEditMode )
	{
		g_pMaterialSystem->EnableEditorMaterials();	
	}

	StartupInfo_t info;
	info.m_pInstance = GetAppInstance();
	info.m_pBaseDirectory = GetBaseDirectory();
	info.m_pInitialMod = DetermineDefaultMod();
	info.m_pInitialGame = DetermineDefaultGame();
	info.m_pParentAppSystemGroup = this;
	info.m_bTextMode = g_bTextMode;

	g_pEngineAPI->SetStartupInfo( info );

	return true;
}

int CSourceAppSystemGroup::Main()
{
	return g_pEngineAPI->Run();
}

void CSourceAppSystemGroup::PostShutdown()
{
	// FIXME: Logfiles is mod-specific, needs to move into the engine.
	g_LogFiles.Shutdown();

	reslistgenerator->Shutdown();

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister( );
	DisconnectTier1Libraries();
}

void CSourceAppSystemGroup::Destroy() 
{
	g_pEngineAPI = NULL;
	g_pMaterialSystem = NULL;
	g_pHammer = NULL;

#ifdef WIN32
	CoUninitialize();
#endif
}


//-----------------------------------------------------------------------------
// Determines the initial mod to use at load time.
// We eventually (hopefully) will be able to switch mods at runtime
// because the engine/hammer integration really wants this feature.
//-----------------------------------------------------------------------------
const char *CSourceAppSystemGroup::DetermineDefaultMod()
{
	if ( !m_bEditMode )
	{   		 
		return CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
	}
	return g_pHammer->GetDefaultMod();
}

const char *CSourceAppSystemGroup::DetermineDefaultGame()
{
	if ( !m_bEditMode )
	{
		return CommandLine()->ParmValue( "-defaultgamedir", DEFAULT_HL2_GAMEDIR );
	}
	return g_pHammer->GetDefaultGame();
}

//-----------------------------------------------------------------------------
// MessageBox for SDL/OSX
//-----------------------------------------------------------------------------
#if defined( USE_SDL ) && !defined( _WIN32 )

int MessageBox( HWND hWnd, const char *message, const char *header, unsigned uType )
{
	SDL_ShowSimpleMessageBox( 0, header, message, GetAssertDialogParent() );
	return 0;
}

#endif

//-----------------------------------------------------------------------------
// Allow only one windowed source app to run at a time
//-----------------------------------------------------------------------------
#ifdef WIN32
HANDLE g_hMutex = NULL;
#elif defined(POSIX)
int g_lockfd = -1;
char g_lockFilename[MAX_PATH];
#endif
bool GrabSourceMutex()
{
#ifdef WIN32
	if ( IsPC() )
	{
		// don't allow more than one instance to run
		g_hMutex = ::CreateMutex(NULL, FALSE, TEXT("hl2_singleton_mutex"));

		unsigned int waitResult = ::WaitForSingleObject(g_hMutex, 0);

		// Here, we have the mutex
		if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED)
			return true;

		// couldn't get the mutex, we must be running another instance
		::CloseHandle(g_hMutex);

		return false;
	}
#elif defined(POSIX)

	// Under OSX use flock in /tmp/source_engine_<game>.lock, create the file if it doesn't exist
	const char *pchGameParam = CommandLine()->ParmValue( "-game", DEFAULT_HL2_GAMEDIR );
	CRC32_t gameCRC;
	CRC32_Init(&gameCRC);
	CRC32_ProcessBuffer( &gameCRC, (void *)pchGameParam, Q_strlen( pchGameParam ) );
	CRC32_Final( &gameCRC );

#ifdef LINUX
	/*
	 * Linux
 	 */

	// Check TMPDIR environment variable for temp directory.
	char *tmpdir = getenv( "TMPDIR" );

	// If it's NULL, or it doesn't exist, or it isn't a directory, fallback to /tmp.
	struct stat buf;
	if( !tmpdir || stat( tmpdir, &buf ) || !S_ISDIR ( buf.st_mode ) )
		tmpdir = "/tmp";

	V_snprintf( g_lockFilename, sizeof(g_lockFilename), "%s/source_engine_%u.lock", tmpdir, gameCRC );

	g_lockfd = open( g_lockFilename, O_WRONLY | O_CREAT, 0666 );
	if ( g_lockfd == -1 )
	{
		printf( "open(%s) failed\n", g_lockFilename );
		return false;
	}

	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	if ( fcntl ( g_lockfd, F_SETLK, &fl ) == -1 )
	{
		printf( "fcntl(%d) for %s failed\n", g_lockfd, g_lockFilename );
		return false;
	}

	return true;
#else
	/*
	 * OSX
 	 */
	V_snprintf( g_lockFilename, sizeof(g_lockFilename), "/tmp/source_engine_%u.lock", gameCRC );

	g_lockfd = open( g_lockFilename, O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK | O_TRUNC, 0777 );
	if (g_lockfd >= 0)
	{
		// make sure we give full perms to the file, we only one instance per machine
		fchmod( g_lockfd, 0777 );

		// we leave the file open, under unix rules when we die we'll automatically close and remove the locks
		return true;
	}   		 

	// We were unable to open the file, it should be because we are unable to retain a lock
	if ( errno != EWOULDBLOCK)
	{
		fprintf( stderr, "unexpected error %d trying to exclusively lock %s\n", errno, g_lockFilename );
	}

	return false;
#endif // OSX

#endif // POSIX
	return true;
}

void ReleaseSourceMutex()
{
#ifdef WIN32
	if ( IsPC() && g_hMutex )
	{
		::ReleaseMutex( g_hMutex );
		::CloseHandle( g_hMutex );
		g_hMutex = NULL;
	}
#elif defined(POSIX)
	if ( g_lockfd != -1 )
	{
		close( g_lockfd );
		g_lockfd = -1;
		unlink( g_lockFilename ); 
	}
#endif
}

// Remove all but the last -game parameter.
// This is for mods based off something other than Half-Life 2 (like HL2MP mods).
// The Steam UI does 'steam -applaunch 320 -game c:\steam\steamapps\sourcemods\modname', but applaunch inserts
// its own -game parameter, which would supercede the one we really want if we didn't intercede here.
void RemoveSpuriousGameParameters()
{
	// Find the last -game parameter.
	int nGameArgs = 0;
	char lastGameArg[MAX_PATH];
	for ( int i=0; i < CommandLine()->ParmCount()-1; i++ )
	{
		if ( Q_stricmp( CommandLine()->GetParm( i ), "-game" ) == 0 )
		{
			Q_snprintf( lastGameArg, sizeof( lastGameArg ), "\"%s\"", CommandLine()->GetParm( i+1 ) );
			++nGameArgs;
			++i;
		}
	}

	// We only care if > 1 was specified.
	if ( nGameArgs > 1 )
	{
		CommandLine()->RemoveParm( "-game" );
		CommandLine()->AppendParm( "-game", lastGameArg );
	}
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
static char *va( char *format, ... )
{
	va_list		argptr;
	static char	string[8][512];
	static int	curstring = 0;

	curstring = ( curstring + 1 ) % 8;

	va_start (argptr, format);
	Q_vsnprintf( string[curstring], sizeof( string[curstring] ), format, argptr );
	va_end (argptr);

	return string[curstring];  
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *param - 
// Output : static char const
//-----------------------------------------------------------------------------
static char const *Cmd_TranslateFileAssociation(char const *param )
{
	static char sz[ 512 ];
	char *retval = NULL;

	char temp[ 512 ];
	Q_strncpy( temp, param, sizeof( temp ) );
	Q_FixSlashes( temp );
#ifdef WIN32
	Q_strlower( temp );
#endif
	const char *extension = V_GetFileExtension(temp);
	// must have an extension to map
	if (!extension)
		return retval;
	extension--; // back up so we have the . in the extension

	int c = ARRAYSIZE( g_FileAssociations );
	for ( int i = 0; i < c; i++ )
	{
		FileAssociationInfo& info = g_FileAssociations[ i ];

		if ( ! Q_strcmp( extension, info.extension ) && 
			! CommandLine()->FindParm(va( "+%s", info.command_to_issue ) ) )
		{
			// Translate if haven't already got one of these commands			
			Q_strncpy( sz, temp, sizeof( sz ) );
			Q_FileBase( sz, temp, sizeof( sz ) );

			Q_snprintf( sz, sizeof( sz ), "%s %s", info.command_to_issue, temp );
			retval = sz;
			break;
		}		
	}

	// return null if no translation, otherwise return commands
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Converts all the convar args into a convar command
// Input  : none 
// Output : const char * series of convars
//-----------------------------------------------------------------------------
static const char *BuildCommand()
{
	static CUtlBuffer build( 0, 0, CUtlBuffer::TEXT_BUFFER );
	build.Clear();

	// arg[0] is the executable name
	for ( int i=1; i < CommandLine()->ParmCount(); i++ )
	{
		const char *szParm = CommandLine()->GetParm(i);
		if (!szParm) continue;

		if (szParm[0] == '-') 
		{
			// skip -XXX options and eat their args
			const char *szValue = CommandLine()->ParmValue(szParm);
			if ( szValue ) i++;
			continue;
		}
		if (szParm[0] == '+')
		{
			// convert +XXX options and stuff them into the build buffer
			const char *szValue = CommandLine()->ParmValue(szParm);
			if (szValue)
			{
				build.PutString(va("%s %s;", szParm+1, szValue));
				i++;
			}
			else
			{
				build.PutString(szParm+1);
				build.PutChar(';');
			}
		}
		else 
		{
			// singleton values, convert to command
			char const *translated = Cmd_TranslateFileAssociation( CommandLine()->GetParm( i ) );
			if (translated)
			{
				build.PutString(translated);
				build.PutChar(';');
			}
		}
	}

	build.PutChar( '\0' );

	return (const char *)build.Base();
}

//-----------------------------------------------------------------------------
// Purpose: The real entry point for the application
// Input  : hInstance - 
//			hPrevInstance - 
//			lpCmdLine - 
//			nCmdShow - 
// Output : int APIENTRY
//-----------------------------------------------------------------------------
#ifdef WIN32
extern "C" __declspec(dllexport) int LauncherMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
#else
DLL_EXPORT int LauncherMain( int argc, char **argv )
#endif
{
#ifdef LINUX
	// Temporary fix to stop us from crashing in printf/sscanf functions that don't expect
	//  localization to mess with your "." and "," float seperators. Mac OSX also sets LANG
	//  to en_US.UTF-8 before starting up (in info.plist I believe).
	// We need to double check that localization for libcef is handled correctly
	//  when we slam things to en_US.UTF-8.
	// Also check if C.UTF-8 exists and use it? This file: /usr/lib/locale/C.UTF-8.
	// It looks like it's only installed on Debian distros right now though.
	const char en_US[] = "en_US.UTF-8";

	setenv( "LC_ALL", en_US, 1 );
	setlocale( LC_ALL, en_US );

	const char *CurrentLocale = setlocale( LC_ALL, NULL );
	if ( Q_stricmp( CurrentLocale, en_US ) )
	{
		Warning( "WARNING: setlocale('%s') failed, using locale:'%s'. International characters may not work.\n", en_US, CurrentLocale );
	}
#endif // LINUX

#ifdef WIN32
	SetAppInstance( hInstance );
#elif defined( POSIX )
	// Store off command line for argument searching
	Plat_SetCommandLine( BuildCmdLine( argc, argv, false ) );

	if( CommandLine()->CheckParm( "-sleepatstartup" ) )
	{
		// When launching from Steam, it can be difficult to get a debugger attached when you're
		//	crashing quickly at startup. So add a -sleepatstartup command line and sleep for 5
		//	seconds which should allow time to attach a debugger.
		sleep( 5 );
	}
#endif

	// Hook the debug output stuff.
	SpewOutputFunc( LauncherDefaultSpewFunc );

	if ( 0 && IsWin98OrOlder() )
	{
		Error( "This build does not currently run under Windows 98/Me." );
		return -1;
	}

	// Quickly check the hardware key, essentially a warning shot.  
	if ( !Plat_VerifyHardwareKeyPrompt() )
	{
		return -1;
	}

	const char *filename;
#ifdef WIN32
	CommandLine()->CreateCmdLine( IsPC() ? VCRHook_GetCommandLine() : lpCmdLine );
#else
	CommandLine()->CreateCmdLine( argc, argv );
#endif

	// No -dxlevel or +mat_hdr_level allowed on POSIX
#ifdef POSIX	
	CommandLine()->RemoveParm( "-dxlevel" );
	CommandLine()->RemoveParm( "+mat_hdr_level" );
	CommandLine()->RemoveParm( "+mat_dxlevel" );
#endif

	// If we're using -default command line parameters, get rid of DX8 settings. 
	if ( CommandLine()->CheckParm( "-default" ) )
	{
		CommandLine()->RemoveParm( "-dxlevel" );
		CommandLine()->RemoveParm( "-maxdxlevel" );
		CommandLine()->RemoveParm( "+mat_dxlevel" );
	}
	
	// Figure out the directory the executable is running from
	UTIL_ComputeBaseDir();

#if defined( _X360 )
	bool bSpewDllInfo = CommandLine()->CheckParm( "-dllinfo" );
	bool bWaitForConsole = CommandLine()->CheckParm( "-vxconsole" );
	XboxConsoleInit();
	XBX_InitConsoleMonitor( bWaitForConsole || bSpewDllInfo );
#endif


#if defined( _X360 )
	if ( bWaitForConsole )
		COM_TimestampedLog( "LauncherMain: Application Start - %s", CommandLine()->GetCmdLine() );
	if ( bSpewDllInfo )
	{	
		XBX_DumpDllInfo( GetBaseDirectory() );
		Error( "Stopped!\n" );
	}

	int storageID = XboxLaunch()->GetStorageID();
	if ( storageID != XBX_INVALID_STORAGE_ID && storageID != XBX_STORAGE_DECLINED )
	{
		// Validate the storage device
		XDEVICE_DATA deviceData;
		DWORD ret = XContentGetDeviceData( storageID, &deviceData );
		if ( ret != ERROR_SUCCESS )
		{
			// Device was removed
			storageID = XBX_INVALID_STORAGE_ID;
			XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, WM_SYS_STORAGEDEVICESCHANGED, 0, 0 );
		}
	}
	XBX_SetStorageDeviceId( storageID );

	int userID = XboxLaunch()->GetUserID();
	if ( !IsRetail() && userID == XBX_INVALID_USER_ID )
	{
		// didn't come from appchooser, try find a valid user id for dev purposes
		XUSER_SIGNIN_INFO info;
		for ( int i = 0; i < 4; ++i )
		{
			if ( ERROR_NO_SUCH_USER != XUserGetSigninInfo( i, 0, &info ) )
			{
				userID = i;
				break;
			}
		}
	}
	XBX_SetPrimaryUserId( userID );
#endif // defined( _X360 )
	
#ifdef POSIX
	{
		struct stat st;
		if ( stat( RELAUNCH_FILE, &st ) == 0 ) 
		{
			unlink( RELAUNCH_FILE );
		}
	}
#endif

	// This call is to emulate steam's injection of the GameOverlay DLL into our process if we
	// are running from the command line directly, this allows the same experience the user gets
	// to be present when running from perforce, the call has no effect on X360
	TryToLoadSteamOverlayDLL();

	// Start VCR mode?
	if ( CommandLine()->CheckParm( "-vcrrecord", &filename ) )
	{
		if ( !VCRStart( filename, true, &g_VCRHelpers ) )
		{
			Error( "-vcrrecord: can't open '%s' for writing.\n", filename );
			return -1;
		}
	}
	else if ( CommandLine()->CheckParm( "-vcrplayback", &filename ) )
	{
		if ( !VCRStart( filename, false, &g_VCRHelpers ) )
		{
			Error( "-vcrplayback: can't open '%s' for reading.\n", filename );
			return -1;
		}
	}

	// See the function for why we do this.
	RemoveSpuriousGameParameters();

#ifdef WIN32
	if ( IsPC() )
	{
		// initialize winsock
		WSAData wsaData;
		int	nError = ::WSAStartup( MAKEWORD(2,0), &wsaData );
		if ( nError )
		{
			Msg( "Warning! Failed to start Winsock via WSAStartup = 0x%x.\n", nError);
		}
	}
#endif

	// Run in text mode? (No graphics or sound).
	if ( CommandLine()->CheckParm( "-textmode" ) )
	{
		g_bTextMode = true;
		InitTextMode();
	}
#ifdef WIN32
	else
	{
		int retval = -1;
		// Can only run one windowed source app at a time
		if ( !GrabSourceMutex() )
		{
			// Allow the user to explicitly say they want to be able to run multiple instances of the source mutex.
			// Useful for side-by-side comparisons of different renderers.
			bool multiRun = CommandLine()->CheckParm( "-multirun" ) != NULL;

			// We're going to hijack the existing session and load a new savegame into it. This will mainly occur when users click on links in Bugzilla that will automatically copy saves and load them
			// directly from the web browser. The -hijack command prevents the launcher from objecting that there is already an instance of the game.
			if (CommandLine()->CheckParm( "-hijack" ))
			{
				HWND hwndEngine = FindWindow( "Valve001", NULL );

				// Can't find the engine
				if ( hwndEngine == NULL )
				{
					::MessageBox( NULL, "The modified entity keyvalues could not be sent to the Source Engine because the engine does not appear to be running.", "Source Engine Not Running", MB_OK | MB_ICONEXCLAMATION );
				}
				else
				{			
					const char *szCommand = BuildCommand();

					//
					// Fill out the data structure to send to the engine.
					//
					COPYDATASTRUCT copyData;
					copyData.cbData = strlen( szCommand ) + 1;
					copyData.dwData = 0;
					copyData.lpData = ( void * )szCommand;

					if ( !::SendMessage( hwndEngine, WM_COPYDATA, 0, (LPARAM)&copyData ) )
					{
						::MessageBox( NULL, "The Source Engine was found running, but did not accept the request to load a savegame. It may be an old version of the engine that does not support this functionality.", "Source Engine Declined Request", MB_OK | MB_ICONEXCLAMATION );
					}
					else
					{
						retval = 0;
					}

					free((void *)szCommand);
				}
			}
			else
			{
				if (!multiRun) {
					::MessageBox(NULL, "Only one instance of the game can be running at one time.", "Source - Warning", MB_ICONINFORMATION | MB_OK);
				}
			}

			if (!multiRun) {
				return retval;
			}
		}
	}
#elif defined( POSIX )
	else
	{
		if ( !GrabSourceMutex() )
		{
			::MessageBox(NULL, "Only one instance of the game can be running at one time.", "Source - Warning", 0 );
			return -1;
		}
	}
#endif

#ifdef WIN32
	// Make low priority?
	if ( CommandLine()->CheckParm( "-low" ) )
	{
		SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );
	}
	else if ( CommandLine()->CheckParm( "-high" ) )
	{
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
	}
#endif

	// If game is not run from Steam then add -insecure in order to avoid client timeout message
	if ( NULL == CommandLine()->CheckParm( "-steam" ) )
	{
		CommandLine()->AppendParm( "-insecure", NULL );
	}

	// Figure out the directory the executable is running from
	// and make that be the current working directory
	_chdir( GetBaseDirectory() );

	g_LeakDump.m_bCheckLeaks = CommandLine()->CheckParm( "-leakcheck" ) ? true : false;

	bool bRestart = true;
	while ( bRestart )
	{
		bRestart = false;

		CSourceAppSystemGroup sourceSystems;
		CSteamApplication steamApplication( &sourceSystems );
		int nRetval = steamApplication.Run();
		if ( steamApplication.GetErrorStage() == CSourceAppSystemGroup::INITIALIZATION )
		{
			bRestart = (nRetval == INIT_RESTART);
		}
		else if ( nRetval == RUN_RESTART )
		{
			bRestart = true;
		}

		bool bReslistCycle = false;
		if ( !bRestart )
		{
			bReslistCycle = reslistgenerator->ShouldContinue();
			bRestart = bReslistCycle;
		}
		
		if ( !bReslistCycle )
		{
			// Remove any overrides in case settings changed
			CommandLine()->RemoveParm( "-w" );
			CommandLine()->RemoveParm( "-h" );
			CommandLine()->RemoveParm( "-width" );
			CommandLine()->RemoveParm( "-height" );
			CommandLine()->RemoveParm( "-sw" );
			CommandLine()->RemoveParm( "-startwindowed" );
			CommandLine()->RemoveParm( "-windowed" );
			CommandLine()->RemoveParm( "-window" );
			CommandLine()->RemoveParm( "-full" );
			CommandLine()->RemoveParm( "-fullscreen" );
			CommandLine()->RemoveParm( "-dxlevel" );
			CommandLine()->RemoveParm( "-autoconfig" );
			CommandLine()->RemoveParm( "+mat_hdr_level" );
		}
	}

#ifdef WIN32
	if ( IsPC() )
	{
		// shutdown winsock
		int nError = ::WSACleanup();
		if ( nError )
		{
			Msg( "Warning! Failed to complete WSACleanup = 0x%x.\n", nError );
		}
	}
#endif

	// Allow other source apps to run
	ReleaseSourceMutex();

#if defined( WIN32 ) && !defined( _X360 )

	// Now that the mutex has been released, check HKEY_CURRENT_USER\Software\Valve\Source\Relaunch URL. If there is a URL here, exec it.
	// This supports the capability of immediately re-launching the the game via Steam in a different audio language 
	HKEY hKey; 
	if ( RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Source", NULL, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS )
	{
		char szValue[MAX_PATH];
		DWORD dwValueLen = MAX_PATH;

		if ( RegQueryValueEx( hKey, "Relaunch URL", NULL, NULL, (unsigned char*)szValue, &dwValueLen ) == ERROR_SUCCESS )
		{
			ShellExecute (0, "open", szValue, 0, 0, SW_SHOW);
			RegDeleteValue( hKey, "Relaunch URL" );
		}

		RegCloseKey(hKey);
	}

#elif defined( OSX ) || defined( LINUX )
	struct stat st;
	if ( stat( RELAUNCH_FILE, &st ) == 0 ) 
	{
		FILE *fp = fopen( RELAUNCH_FILE, "r" );
		if ( fp )
		{
			char szCmd[256];
			int nChars = fread( szCmd, 1, sizeof(szCmd), fp );
			if ( nChars > 0 )
			{
				if ( nChars > (sizeof(szCmd)-1) )
				{
					nChars = (sizeof(szCmd)-1);
				}
				szCmd[nChars] = 0;
				char szOpenLine[ MAX_PATH ];
				#if defined( LINUX )
					Q_snprintf( szOpenLine, sizeof(szOpenLine), "xdg-open \"%s\"", szCmd );
				#else
					Q_snprintf( szOpenLine, sizeof(szOpenLine), "open \"%s\"", szCmd );
				#endif
				system( szOpenLine );
			}
			fclose( fp );
			unlink( RELAUNCH_FILE );
		}
	}
#elif defined( _X360 )
#else
#error
#endif

	return 0;
}
