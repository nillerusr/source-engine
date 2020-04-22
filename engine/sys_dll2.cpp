//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#define DISABLE_PROTECTED_THINGS

#if defined( USE_SDL )
#include "appframework/ilaunchermgr.h"
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#include "winlite.h"
#include <Psapi.h>
#endif

#if defined( OSX )
#include <sys/sysctl.h>
#endif

#if defined( POSIX )
#include <setjmp.h>
#include <signal.h>
#endif

#include <stdarg.h>
#include "quakedef.h"
#include "idedicatedexports.h"
#include "engine_launcher_api.h"
#include "ivideomode.h"
#include "common.h"
#include "iregistry.h"
#include "keys.h"
#include "cdll_engine_int.h"
#include "traceinit.h"
#include "iengine.h"
#include "igame.h"
#include "tier0/etwprof.h"
#include "tier0/vcrmode.h"
#include "tier0/icommandline.h"
#include "tier0/minidump.h"
#include "engine_hlds_api.h"
#include "filesystem_engine.h"
#include "cl_main.h"
#include "client.h"
#include "tier3/tier3.h"
#include "MapReslistGenerator.h"
#include "toolframework/itoolframework.h"
#include "sourcevr/isourcevirtualreality.h"
#include "DevShotGenerator.h"
#include "gl_shader.h"
#include "l_studio.h"
#include "IHammer.h"
#include "sys_dll.h"
#include "materialsystem/materialsystem_config.h"
#include "server.h"
#include "video/ivideoservices.h"
#include "datacache/idatacache.h"
#include "vphysics_interface.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/IAppSystemGroup.h"
#include "tier0/systeminformation.h"
#include "host_cmd.h"
#ifdef _WIN32
#include "VGuiMatSurface/IMatSystemSurface.h"
#endif

#ifdef GPROFILER
#include "gperftools/profiler.h"
#endif

// This is here just for legacy support of older .dlls!!!
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "eiface.h"
#include "tier1/fmtstr.h"
#include "steam/steam_api.h"

#ifndef SWDS
#include "sys_mainwind.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#include "IGameUIFuncs.h"
#include "cl_steamauth.h"
#endif // SWDS

#if defined(_WIN32)
#include <eh.h>
#endif

#if POSIX
#include <dlfcn.h>
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#else
#include "xbox/xboxstubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
IDedicatedExports *dedicated = NULL;
extern CreateInterfaceFn g_AppSystemFactory;
IHammer *g_pHammer = NULL;
IPhysics *g_pPhysics = NULL;
ISourceVirtualReality *g_pSourceVR = NULL;
#if defined( USE_SDL )
ILauncherMgr *g_pLauncherMgr = NULL;
#endif

#ifndef SWDS
extern CreateInterfaceFn g_ClientFactory;
#endif

static SteamInfVersionInfo_t g_SteamInfIDVersionInfo;
const SteamInfVersionInfo_t& GetSteamInfIDVersionInfo()
{
	Assert( g_SteamInfIDVersionInfo.AppID != k_uAppIdInvalid );
	return g_SteamInfIDVersionInfo;
}

int build_number( void )
{
	return GetSteamInfIDVersionInfo().ServerVersion;
}

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Host_GetHostInfo(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
const char *Key_BindingForKey( int keynum );
void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );
void Host_ReadPreStartupConfiguration();

//-----------------------------------------------------------------------------
// ConVars and console commands
//-----------------------------------------------------------------------------
#ifndef SWDS
//-----------------------------------------------------------------------------
// Purpose: exports an interface that can be used by the launcher to run the engine
//			this is the exported function when compiled as a blob
//-----------------------------------------------------------------------------
void EXPORT F( IEngineAPI **api )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary to prevent the LTCG compiler from crashing.
	*api = ( IEngineAPI * )(factory(VENGINE_LAUNCHER_API_VERSION, NULL));
}
#endif // SWDS

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClearIOStates( void )
{
#ifndef SWDS
	if ( g_ClientDLL ) 
	{
		g_ClientDLL->IN_ClearStates();
	}
#endif
}

//-----------------------------------------------------------------------------
// The SDK launches the game with the full path to gameinfo.txt, so we need
// to strip off the path.
//-----------------------------------------------------------------------------
const char *GetModDirFromPath( const char *pszPath )
{
	char *pszSlash = Q_strrchr( pszPath, '\\' );
	if ( pszSlash )
	{
		return pszSlash + 1;
	}
	else if ( ( pszSlash  = Q_strrchr( pszPath, '/' ) ) != NULL )
	{
		return pszSlash + 1;
	}

	// Must just be a mod directory already.
	return pszPath;
}

//-----------------------------------------------------------------------------
// Purpose: Main entry
//-----------------------------------------------------------------------------
#ifndef SWDS
#include "gl_matsysiface.h"
#endif

//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to 
//-----------------------------------------------------------------------------
class CModAppSystemGroup : public CAppSystemGroup
{
	typedef CAppSystemGroup BaseClass;
public:
	// constructor
	CModAppSystemGroup( bool bServerOnly, CAppSystemGroup *pParentAppSystem = NULL )
		: BaseClass( pParentAppSystem ),
		m_bServerOnly( bServerOnly )
	{
	}

	CreateInterfaceFn GetFactory()
	{
		return CAppSystemGroup::GetFactory();
	}

	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:

	bool IsServerOnly() const
	{
		return m_bServerOnly;
	}
	bool ModuleAlreadyInList( CUtlVector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName );

	bool AddLegacySystems();
	bool	m_bServerOnly;
};

#if defined( STAGING_ONLY )
CON_COMMAND( bigalloc, "huge alloc crash" )
{
	Msg( "pre-crash %d\n", MemAlloc_MemoryAllocFailed() );
	// Alloc a bit less than UINT_MAX so there is room for heap headers in the malloc functions.
	void *buf = malloc( UINT_MAX - 0x4000 );
	Msg( "post-alloc %d. buf: %p\n", MemAlloc_MemoryAllocFailed(), buf );
	*(int *)buf = 0;
}
#endif

extern void S_ClearBuffer();
extern char g_minidumpinfo[ 4096 ];
extern PAGED_POOL_INFO_t g_pagedpoolinfo;
extern bool g_bUpdateMinidumpComment;
void GetSpew( char *buf, size_t buflen );

extern int gHostSpawnCount;
extern int g_nMapLoadCount;
extern int g_HostServerAbortCount;
extern int g_HostErrorCount;
extern int g_HostEndDemo;

// Turn this to 1 to allow for expanded spew in minidump comments.
static ConVar sys_minidumpexpandedspew( "sys_minidumpexpandedspew", "1" );

#ifdef IS_WINDOWS_PC

extern "C" void __cdecl FailSafe( unsigned int uStructuredExceptionCode, struct _EXCEPTION_POINTERS * pExceptionInfo	)
{
	// Nothing, this just catches a crash when creating the comment block
}

#endif

#if defined( POSIX )

static sigjmp_buf g_mark;
static void posix_signal_handler( int i )
{
	siglongjmp( g_mark, -1 );
}

#define DO_TRY		if ( sigsetjmp( g_mark, 1 ) == 0 )
#define DO_CATCH	else

#if defined( OSX )
#define __sighandler_t sig_t
#endif

#else

#define DO_TRY		try
#define DO_CATCH	catch ( ... )

#endif // POSIX

//-----------------------------------------------------------------------------
// Purpose: Check whether any mods are loaded.
//  Currently looks for metamod and sourcemod.
//-----------------------------------------------------------------------------
static bool IsSourceModLoaded()
{
#if defined( _WIN32 )
	static const char *s_pFileNames[] = { "metamod.2.tf2.dll", "sourcemod.2.tf2.dll", "sdkhooks.ext.2.ep2v.dll", "sdkhooks.ext.2.tf2.dll" };

	for ( size_t i = 0; i < Q_ARRAYSIZE( s_pFileNames ); i++ )
	{
		// GetModuleHandle function returns a handle to a mapped module
		//  without incrementing its reference count.
		if ( GetModuleHandleA( s_pFileNames[ i ] ) )
			return true;
	}
#else
	FILE *fh = fopen( "/proc/self/maps", "r" );

	if ( fh )
	{
		char buf[ 1024 ];
		static const char *s_pFileNames[] = { "metamod.2.tf2.so", "sourcemod.2.tf2.so", "sdkhooks.ext.2.ep2v.so", "sdkhooks.ext.2.tf2.so" };

		while ( fgets( buf, sizeof( buf ), fh ) )
		{
			for ( size_t i = 0; i < Q_ARRAYSIZE( s_pFileNames ); i++ )
			{
				if ( strstr( buf, s_pFileNames[ i ] ) )
				{
					fclose( fh );
					return true;
				}
			}
		}

		fclose( fh );
	}
#endif

	return false;
}

template< int _SIZE >
class CErrorText
{
public:
	CErrorText() : m_bIsDedicatedServer( false ) {}
	~CErrorText() {}

	void Steam_SetMiniDumpComment()
	{
#if !defined( NO_STEAM )
		SteamAPI_SetMiniDumpComment( m_errorText );
#endif
	}

	void CommentCat( const char * str )
	{
		V_strcat_safe( m_errorText, str );
	}

	void CommentPrintf( const char *fmt, ... )
	{
		va_list args;
		va_start( args, fmt );
		
		size_t len = strlen( m_errorText );
		vsnprintf( m_errorText + len, sizeof( m_errorText ) - len - 1, fmt, args );
		m_errorText[ sizeof( m_errorText ) - 1 ] = 0;

		va_end( args );
	}

	void BuildComment( char const *pchSysErrorText, bool bRealCrash )
	{
		// Try and detect whether this
		bool bSourceModLoaded = false;
		if ( m_bIsDedicatedServer )
		{
			bSourceModLoaded = IsSourceModLoaded();
			if ( bSourceModLoaded )
			{
				AppId_t AppId = GetSteamInfIDVersionInfo().ServerAppID;
				// Bump up the number and report the crash. This should be something
				//  like 232251 (instead of 232250). 232251 is for the TF2 Windows client,
				//  but we actually report those crashes under ID 440, so this should be ok.
				SteamAPI_SetBreakpadAppID( AppId + 1 );
			}
		}

#ifdef IS_WINDOWS_PC
		// This warning only applies if you want to catch structured exceptions (crashes)
		// using C++ exceptions. We do not want to do that so we can build with C++ exceptions
		// completely disabled, and just suppress this warning.
		// warning C4535: calling _set_se_translator() requires /EHa
		#pragma warning( suppress : 4535 )
		_se_translator_function curfilter = _set_se_translator( &FailSafe );
#elif defined( POSIX )
		// Only need to worry about this function crashing when we're dealing with a real crash.
		__sighandler_t curfilter = bRealCrash ? signal( SIGSEGV, posix_signal_handler ) : 0;
#endif

		DO_TRY
		{
			Q_memset( m_errorText, 0x00, sizeof( m_errorText ) );

			if ( pchSysErrorText )
			{
				CommentCat( "Sys_Error( " );
				CommentCat( pchSysErrorText );

				// Trim trailing \n.
				int len = V_strlen( m_errorText );
				if ( len > 0 && m_errorText[ len - 1 ] == '\n' )
					m_errorText[ len - 1 ] = 0;

				CommentCat( " )\n" );
			}
			else
			{
				CommentCat( "Crash\n" );
			}
			CommentPrintf( "Uptime( %f )\n", Plat_FloatTime() );
			CommentPrintf( "SourceMod:%d,DS:%d,Crash:%d\n\n", bSourceModLoaded, m_bIsDedicatedServer, bRealCrash );

			// Add g_minidumpinfo from CL_SetSteamCrashComment().
			CommentCat( g_minidumpinfo );

			// Latch in case extended stuff below crashes
			Steam_SetMiniDumpComment();

			// Add Memory Status
			BuildCommentMemStatus();

			// Spew out paged pool stuff, etc.
			PAGED_POOL_INFO_t ppi_info;
			if ( Plat_GetPagedPoolInfo( &ppi_info ) != SYSCALL_UNSUPPORTED )
			{
				CommentPrintf( "\nPaged Pool\nprev PP PAGES: used: %lu, free %lu\nfinal PP PAGES: used: %lu, free %lu\n",
				            g_pagedpoolinfo.numPagesUsed, g_pagedpoolinfo.numPagesFree, 
				            ppi_info.numPagesUsed, ppi_info.numPagesFree );
			}

			CommentPrintf( "memallocfail? = %u\nActive: %s\nSpawnCount %d MapLoad Count %d\nError count %d, end demo %d, abort count %d\n",
			            MemAlloc_MemoryAllocFailed(),
			            ( game && game->IsActiveApp() ) ? "active" : "inactive", 
			            gHostSpawnCount, 
			            g_nMapLoadCount,
			            g_HostErrorCount,
			            g_HostEndDemo,
			            g_HostServerAbortCount );

			// Latch in case extended stuff below crashes
			Steam_SetMiniDumpComment();

			// Add user comment strings. 4096 is just a large sanity number we should
			//  never ever reach (currently our minidump supports 32 of these.)
			for( int i = 0; i < 4096; i++ )
			{
				const char *pUserStreamInfo = MinidumpUserStreamInfoGet( i );
				if( !pUserStreamInfo )
					break;

				if ( pUserStreamInfo[ 0 ] )
					CommentPrintf( "%s", pUserStreamInfo );
			}

			bool bExtendedSpew = sys_minidumpexpandedspew.GetBool();
			if ( bExtendedSpew )
			{
				BuildCommentExtended();
				Steam_SetMiniDumpComment();

#if defined( LINUX )
				if ( bRealCrash )
				{
					// bRealCrash is set when we're actually making a comment for a dump or error.
					AddFileToComment( "/proc/meminfo" );
					AddFileToComment( "/proc/self/status" );
					Steam_SetMiniDumpComment();

					// Useful, but really big, so disable for now.
					//$ AddFileToComment( "/proc/self/maps" );
				}
#endif
			}
		}
		DO_CATCH
		{
			// Oh oh
		}
		
#ifdef IS_WINDOWS_PC
		_set_se_translator( curfilter );
#elif defined( POSIX )
		if ( bRealCrash )
			signal( SIGSEGV, curfilter );
#endif
	}

	void BuildCommentMemStatus()
	{
#ifdef _WIN32
		const double MbDiv = 1024.0 * 1024.0;

		MEMORYSTATUSEX	memStat;
		ZeroMemory( &memStat, sizeof( MEMORYSTATUSEX ) );
		memStat.dwLength = sizeof( MEMORYSTATUSEX );

		if ( GlobalMemoryStatusEx( &memStat ) )
		{
			CommentPrintf( "\nMemory\nmemusage( %d %% )\ntotalPhysical Mb(%.2f)\nfreePhysical Mb(%.2f)\ntotalPaging Mb(%.2f)\nfreePaging Mb(%.2f)\ntotalVirtualMem Mb(%.2f)\nfreeVirtualMem Mb(%.2f)\nextendedVirtualFree Mb(%.2f)\n",
				memStat.dwMemoryLoad,
				(double)memStat.ullTotalPhys / MbDiv,
				(double)memStat.ullAvailPhys / MbDiv,
				(double)memStat.ullTotalPageFile / MbDiv,
				(double)memStat.ullAvailPageFile / MbDiv,
				(double)memStat.ullTotalVirtual / MbDiv,
				(double)memStat.ullAvailVirtual / MbDiv,
				(double)memStat.ullAvailExtendedVirtual / MbDiv);
		}

		HINSTANCE hInst = LoadLibrary( "Psapi.dll" );
		if ( hInst )
		{
			typedef BOOL (WINAPI *GetProcessMemoryInfoFn)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
			GetProcessMemoryInfoFn fn = (GetProcessMemoryInfoFn)GetProcAddress( hInst, "GetProcessMemoryInfo" );
			if ( fn )
			{
				PROCESS_MEMORY_COUNTERS counters;

				ZeroMemory( &counters, sizeof( PROCESS_MEMORY_COUNTERS ) );
				counters.cb = sizeof( PROCESS_MEMORY_COUNTERS );

				if ( fn( GetCurrentProcess(), &counters, sizeof( PROCESS_MEMORY_COUNTERS ) ) )
				{
					CommentPrintf( "\nProcess Memory\nWorkingSetSize Mb(%.2f)\nQuotaPagedPoolUsage Mb(%.2f)\nQuotaNonPagedPoolUsage: Mb(%.2f)\nPagefileUsage: Mb(%.2f)\n",
						(double)counters.WorkingSetSize / MbDiv,
						(double)counters.QuotaPagedPoolUsage / MbDiv,
						(double)counters.QuotaNonPagedPoolUsage / MbDiv,
						(double)counters.PagefileUsage / MbDiv );
				}
			}

			FreeLibrary( hInst );
		}

#elif defined( OSX )

		static const struct
		{
			int ctl;
			const char *name;
		} s_ctl_names[] =
		{
		#define _XTAG( _x ) { _x, #_x }
			_XTAG( HW_PHYSMEM ),
			_XTAG( HW_USERMEM ),
			_XTAG( HW_MEMSIZE ),
			_XTAG( HW_AVAILCPU ),
		#undef _XTAG
		};

		for ( size_t i = 0; i < Q_ARRAYSIZE( s_ctl_names ); i++ )
		{
			uint64_t val = 0;
			size_t len = sizeof( val );
			int mib[] = { CTL_HW, s_ctl_names[ i ].ctl };

			if ( sysctl( mib, Q_ARRAYSIZE( mib ), &val, &len, NULL, 0 ) == 0 )
			{
				CommentPrintf( " %s: %" PRIu64 "\n", s_ctl_names[ i ].name, val );
			}
		}

#endif
	}

	void BuildCommentExtended()
	{
		try
		{
			CommentCat( "\nConVars (non-default)\n\n" );
			CommentPrintf( "%s %s %s\n", "var", "value", "default" );

			for ( const ConCommandBase *var = g_pCVar->GetCommands() ; var ; var = var->GetNext())
			{
				if ( var->IsCommand() )
					continue;

				ConVar *pCvar = ( ConVar * )var;
				if ( pCvar->IsFlagSet( FCVAR_SERVER_CANNOT_QUERY | FCVAR_PROTECTED ) )
					continue;

				if ( !(pCvar->IsFlagSet( FCVAR_NEVER_AS_STRING ) ) )
				{
					char var1[ MAX_OSPATH ];
					char var2[ MAX_OSPATH ];

					Q_strncpy( var1, Host_CleanupConVarStringValue( pCvar->GetString() ), sizeof( var1 ) );
					Q_strncpy( var2, Host_CleanupConVarStringValue( pCvar->GetDefault() ), sizeof( var2 ) );

					if ( !Q_stricmp( var1, var2 ) )
						continue;
				}
				else
				{
					if ( pCvar->GetFloat() == Q_atof( pCvar->GetDefault() ) )
						continue;
				}

				if ( !(pCvar->IsFlagSet( FCVAR_NEVER_AS_STRING ) ) )
					CommentPrintf( "%s '%s' '%s'\n", pCvar->GetName(), Host_CleanupConVarStringValue( pCvar->GetString() ), pCvar->GetDefault() );
				else
					CommentPrintf( "%s '%f' '%f'\n", pCvar->GetName(), pCvar->GetFloat(), Q_atof( pCvar->GetDefault() ) );
			}

			CommentCat( "\nConsole History (reversed)\n\n" );

			// Get console
			int len = V_strlen( m_errorText );
			if ( len < sizeof( m_errorText ) )
			{
				GetSpew( m_errorText + len, sizeof( m_errorText ) - len - 1 );
				m_errorText[ sizeof( m_errorText ) - 1 ] = 0;
			}
		}
		catch ( ... )
		{
			CommentCat( "Exception thrown building console/convar history.\n" );
		}
	}

#if defined( LINUX )

	void AddFileToComment( const char *filename )
	{
		CommentPrintf( "\n%s:\n", filename );

		int nStart = Q_strlen( m_errorText );
		int nMaxLen = sizeof( m_errorText ) - nStart - 1;

		if ( nMaxLen > 0 )
		{
			FILE *fh = fopen( filename, "r" );

			if ( fh )
			{
				size_t ret = fread( m_errorText + nStart, 1, nMaxLen, fh );
				fclose( fh );

				// Replace tab characters with spaces.
				for ( size_t i = 0; i < ret; i++ )
				{
					if ( m_errorText[ nStart + i ] == '\t' )
						m_errorText[ nStart + i ] = ' ';
				}
			}

			// Entire buffer should have been zeroed out, but just super sure...
			m_errorText[ sizeof( m_errorText ) - 1 ] = 0;
		}
	}

#endif // LINUX

public:
	char m_errorText[ _SIZE ];
	bool m_bIsDedicatedServer;
};

#if defined( _X360 )
static CErrorText<3500> errorText;
#else
static CErrorText<95000> errorText;
#endif

void BuildMinidumpComment( char const *pchSysErrorText, bool bRealCrash )
{
#if !defined(NO_STEAM)
	/*
	// Uncomment this code if you are testing max minidump comment length issues
	// It allows you to asked for a dummy comment of a certain length
	int nCommentLength = CommandLine()->ParmValue( "-commentlen", 0 );
	if ( nCommentLength > 0 )
	{
		nCommentLength = MIN( nCommentLength, 128*1024 );
		char *cbuf = new char[ nCommentLength + 1 ];
		for ( int i = 0; i < nCommentLength; ++i )
		{
			cbuf[ i ] = (char)('0' + (i % 10));
		}
		cbuf[ nCommentLength ] = 0;
		SteamAPI_SetMiniDumpComment( cbuf );
		delete[] cbuf;
		return;
	}
	*/
	errorText.BuildComment( pchSysErrorText, bRealCrash );
#endif
}

#if defined( POSIX )

static void PosixPreMinidumpCallback( void *context )
{
	BuildMinidumpComment( NULL, true );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Attempt to initialize appid/steam.inf/minidump information. May only return partial information if called
//          before Filesystem is ready.
//
//          The desire is to be able to call this ASAP to init basic minidump and AppID info, then re-call later on when
//          the filesystem is setup, so full version # information and such can be propagated to the minidump system.
//          (Currently, SDK mods will generally only have partial information prior to filesystem init)
//-----------------------------------------------------------------------------
// steam.inf keys.
#define VERSION_KEY				"PatchVersion="
#define PRODUCT_KEY				"ProductName="
#define SERVER_VERSION_KEY		"ServerVersion="
#define APPID_KEY				"AppID="
#define SERVER_APPID_KEY		"ServerAppID="
enum eSteamInfoInit
{
	eSteamInfo_Uninitialized,
	eSteamInfo_Partial,
	eSteamInfo_Initialized
};
static eSteamInfoInit Sys_TryInitSteamInfo( void *pvAPI, SteamInfVersionInfo_t& VerInfo, const char *pchMod, const char *pchBaseDir, bool bDedicated )
{
	static eSteamInfoInit initState = eSteamInfo_Uninitialized;

	eSteamInfoInit previousInitState = initState;

	//
	//
	// Initialize with some defaults.
	VerInfo.ClientVersion = 0;
	VerInfo.ServerVersion = 0;
	V_strcpy_safe( VerInfo.szVersionString, "valve" );
	V_strcpy_safe( VerInfo.szProductString, "1.0.1.0" );
	VerInfo.AppID = k_uAppIdInvalid;
	VerInfo.ServerAppID = k_uAppIdInvalid;

	// Filesystem may or may not be up
	CUtlBuffer infBuf;
	bool bFoundInf = false;
	if ( g_pFileSystem )
	{
		FileHandle_t fh;
		fh = g_pFileSystem->Open( "steam.inf", "rb", "GAME" );
		bFoundInf = fh && g_pFileSystem->ReadToBuffer( fh, infBuf );
	}

	if ( !bFoundInf )
	{
		// We may try to load the steam.inf BEFORE we turn on the filesystem, so use raw filesystem API's here.
		char szFullPath[ MAX_PATH ] = { 0 };
		char szModSteamInfPath[ MAX_PATH ] = { 0 };
		V_ComposeFileName( pchMod, "steam.inf", szModSteamInfPath, sizeof( szModSteamInfPath ) );
		V_MakeAbsolutePath( szFullPath, sizeof( szFullPath ), szModSteamInfPath, pchBaseDir );

		// Try opening steam.inf
		FILE *fp = fopen( szFullPath, "rb" );
		if ( fp )
		{
			// Read steam.inf data.
			fseek( fp, 0, SEEK_END );
			size_t bufsize = ftell( fp );
			fseek( fp, 0, SEEK_SET );

			infBuf.EnsureCapacity( bufsize + 1 );

			size_t iBytesRead = fread( infBuf.Base(), 1, bufsize, fp );
			((char *)infBuf.Base())[iBytesRead] = 0;
			infBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, iBytesRead + 1 );
			fclose( fp );

			bFoundInf = ( iBytesRead == bufsize );
		}
	}

	if ( bFoundInf )
	{
		const char *pbuf = (const char*)infBuf.Base();
		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf || !com_token[ 0 ] )
				break;

			if ( !Q_strnicmp( com_token, VERSION_KEY, Q_strlen( VERSION_KEY ) ) )
			{
				V_strcpy_safe( VerInfo.szVersionString, com_token + Q_strlen( VERSION_KEY ) );
				VerInfo.ClientVersion = atoi( VerInfo.szVersionString );
			}
			else if ( !Q_strnicmp( com_token, PRODUCT_KEY, Q_strlen( PRODUCT_KEY ) ) )
			{
				V_strcpy_safe( VerInfo.szProductString, com_token + Q_strlen( PRODUCT_KEY ) );
			}
			else if ( !Q_strnicmp( com_token, SERVER_VERSION_KEY, Q_strlen( SERVER_VERSION_KEY ) ) )
			{
				VerInfo.ServerVersion = atoi( com_token + Q_strlen( SERVER_VERSION_KEY ) );
			}
			else if ( !Q_strnicmp( com_token, APPID_KEY, Q_strlen( APPID_KEY ) ) )
			{
				VerInfo.AppID = atoi( com_token + Q_strlen( APPID_KEY ) );
			}
			else if ( !Q_strnicmp( com_token, SERVER_APPID_KEY, Q_strlen( SERVER_APPID_KEY ) ) )
			{
				VerInfo.ServerAppID = atoi( com_token + Q_strlen( SERVER_APPID_KEY ) );
			}
		}

		// If we found a steam.inf we're as good as we're going to get, but don't tell callers we're fully initialized
		// if it doesn't at least have an AppID
		initState = ( VerInfo.AppID != k_uAppIdInvalid ) ? eSteamInfo_Initialized : eSteamInfo_Partial;
	}
	else if ( !bDedicated )
	{
		// Opening steam.inf failed - try to open gameinfo.txt and read in just SteamAppId from that.
		// (gameinfo.txt lacks the dedicated server steamid, so we'll just have to live until filesystem init to setup
		// breakpad there when we hit this case)
		char szModGameinfoPath[ MAX_PATH ] = { 0 };
		char szFullPath[ MAX_PATH ] = { 0 };
		V_ComposeFileName( pchMod, "gameinfo.txt", szModGameinfoPath, sizeof( szModGameinfoPath ) );
		V_MakeAbsolutePath( szFullPath, sizeof( szFullPath ), szModGameinfoPath, pchBaseDir );

		// Try opening gameinfo.txt
		FILE *fp = fopen( szFullPath, "rb" );
		if( fp )
		{
			fseek( fp, 0, SEEK_END );
			size_t bufsize = ftell( fp );
			fseek( fp, 0, SEEK_SET );

			char *buffer = ( char * )_alloca( bufsize + 1 );

			size_t iBytesRead = fread( buffer, 1, bufsize, fp );
			buffer[ iBytesRead ] = 0;
			fclose( fp );

			KeyValuesAD pkvGameInfo( "gameinfo" );
			if ( pkvGameInfo->LoadFromBuffer( "gameinfo.txt", buffer ) )
			{
				VerInfo.AppID = (AppId_t)pkvGameInfo->GetInt( "FileSystem/SteamAppId", k_uAppIdInvalid );
			}
		}

		initState = eSteamInfo_Partial;
	}

	// In partial state the ServerAppID might be unknown, but if we found the full steam.inf and it's not set, it shares AppID.
	if ( initState == eSteamInfo_Initialized && VerInfo.ServerAppID == k_uAppIdInvalid )
		VerInfo.ServerAppID = VerInfo.AppID;

#if !defined(_X360)
	if ( VerInfo.AppID )
	{
		// steamclient.dll doesn't know about steam.inf files in mod folder,
		// it accepts a steam_appid.txt in the root directory if the game is
		// not started through Steam. So we create one there containing the
		// current AppID
		FILE *fh = fopen( "steam_appid.txt", "wb" );
		if ( fh  )
		{
			CFmtStrN< 128 > strAppID( "%u\n", VerInfo.AppID );

			fwrite( strAppID.Get(), strAppID.Length() + 1, 1, fh );
			fclose( fh );
		}
	}
#endif // !_X360

	//
	// Update minidump info if we have more information than before
	//

#ifndef NO_STEAM
	// If -nobreakpad was specified or we found metamod or sourcemod, don't register breakpad.
	bool bUseBreakpad = !CommandLine()->FindParm( "-nobreakpad" ) && ( !bDedicated || !IsSourceModLoaded() );
	AppId_t BreakpadAppId = bDedicated ? VerInfo.ServerAppID : VerInfo.AppID;
	Assert( BreakpadAppId != k_uAppIdInvalid || initState < eSteamInfo_Initialized );
	if ( BreakpadAppId != k_uAppIdInvalid && initState > previousInitState && bUseBreakpad )
	{
		void *pvMiniDumpContext = NULL;
		PFNPreMinidumpCallback pfnPreMinidumpCallback = NULL;
		bool bFullMemoryDump = !bDedicated && IsWindows() && CommandLine()->FindParm( "-full_memory_dumps" );

#if defined( POSIX )
		// On Windows we're relying on the try/except to build the minidump comment. On Linux, we don't have that
		//	so we need to register the minidumpcallback handler here.
		pvMiniDumpContext = pvAPI;
		pfnPreMinidumpCallback = PosixPreMinidumpCallback;
#endif

		CFmtStrN<128> pchVersion( "%d", build_number() );
		Msg( "Using Breakpad minidump system. Version: %s AppID: %u\n", pchVersion.Get(), BreakpadAppId );

		// We can filter various crash dumps differently in the Socorro backend code:
		//    Steam/min/web/crash_reporter/socorro/scripts/config/collectorconfig.py
		SteamAPI_SetBreakpadAppID( BreakpadAppId );
		SteamAPI_UseBreakpadCrashHandler( pchVersion, __DATE__, __TIME__, bFullMemoryDump, pvMiniDumpContext, pfnPreMinidumpCallback );

		// Tell errorText class if this is dedicated server.
		errorText.m_bIsDedicatedServer = bDedicated;
	}
#endif // NO_STEAM

	MinidumpUserStreamInfoSetHeader( "%sLaunching \"%s\"\n", ( bDedicated ? "DedicatedServerAPI " : "" ), CommandLine()->GetCmdLine() );


	return initState;
}

#ifndef SWDS

//-----------------------------------------------------------------------------
//
// Main engine interface exposed to launcher
//
//-----------------------------------------------------------------------------
class CEngineAPI : public CTier3AppSystem< IEngineAPI >
{
	typedef CTier3AppSystem< IEngineAPI > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// This function must be called before init
	virtual void SetStartupInfo( StartupInfo_t &info );

	virtual int Run( );

	// Sets the engine to run in a particular editor window
	virtual void SetEngineWindow( void *hWnd );

	// Posts a console command
	virtual void PostConsoleCommand( const char *pConsoleCommand );

	// Are we running the simulation?
	virtual bool IsRunningSimulation( ) const;

	// Start/stop running the simulation
	virtual void ActivateSimulation( bool bActive );

	// Reset the map we're on
	virtual void SetMap( const char *pMapName );

	bool MainLoop();

	int RunListenServer();

private:

	// Hooks a particular mod up to the registry
	void SetRegistryMod( const char *pModName );

	// One-time setup, based on the initially selected mod
	// FIXME: This should move into the launcher!
	bool OnStartup( void *pInstance, const char *pStartupModName );
	void OnShutdown();

	// Initialization, shutdown of a mod.
	bool ModInit( const char *pModName, const char *pGameDir );
	void ModShutdown();

	// Initializes, shuts down the registry
	bool InitRegistry( const char *pModName );
	void ShutdownRegistry();

	// Handles there being an error setting up the video mode
	InitReturnVal_t HandleSetModeError();

	// Initializes, shuts down VR
	bool InitVR();
	void ShutdownVR();

	// Purpose: Message pump when running stand-alone
	void PumpMessages();

	// Purpose: Message pump when running with the editor
	void PumpMessagesEditMode( bool &bIdle, long &lIdleCount );

	// Activate/deactivates edit mode shaders
	void ActivateEditModeShaders( bool bActive );

private:
	void *m_hEditorHWnd;
	bool m_bRunningSimulation;
	bool m_bSupportsVR;
	StartupInfo_t m_StartupInfo;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CEngineAPI s_EngineAPI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineAPI, IEngineAPI, VENGINE_LAUNCHER_API_VERSION, s_EngineAPI );


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CEngineAPI::Connect( CreateInterfaceFn factory ) 
{ 
	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pFileSystem = g_pFullFileSystem;
	if ( !g_pFileSystem )
		return false;

	g_pFileSystem->SetWarningFunc( Warning );

	if ( !Shader_Connect( true ) )
		return false;

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, NULL );

	if ( !g_pStudioRender || !g_pDataCache || !g_pPhysics || !g_pMDLCache || !g_pMatSystemSurface || !g_pInputSystem /* || !g_pVideo */ )
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	if (!g_pStudioRender)
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pHammer = (IHammer*)factory( INTERFACEVERSION_HAMMER, NULL );

#if defined( USE_SDL )
	g_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL );
#endif
	
	ConnectMDLCacheNotify();

	return true; 
}

void CEngineAPI::Disconnect() 
{
	DisconnectMDLCacheNotify();

#if !defined( SWDS )
	TRACESHUTDOWN( Steam3Client().Shutdown() );
#endif

	g_pHammer = NULL;
	g_pPhysics = NULL;

	Shader_Disconnect();

	g_pFileSystem = NULL;

	BaseClass::Disconnect();

	g_AppSystemFactory = NULL;
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CEngineAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Sets startup info
//-----------------------------------------------------------------------------
void CEngineAPI::SetStartupInfo( StartupInfo_t &info )
{
	// Setup and write out steam_appid.txt before we launch
	bool bDedicated = false; // Dedicated comes through CDedicatedServerAPI
	eSteamInfoInit steamInfo = Sys_TryInitSteamInfo( this, g_SteamInfIDVersionInfo, info.m_pInitialMod, info.m_pBaseDirectory, bDedicated );

	g_bTextMode = info.m_bTextMode;

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>( info.m_pBaseDirectory );

	// Copy off all the startup info
	m_StartupInfo = info;

#if !defined( SWDS )
	// turn on the Steam3 API early so we can query app data up front
	TRACEINIT( Steam3Client().Activate(), Steam3Client().Shutdown() );
#endif

	// Needs to be done prior to init material system config
	TRACEINIT( COM_InitFilesystem( m_StartupInfo.m_pInitialMod ), COM_ShutdownFileSystem() );

	if ( steamInfo != eSteamInfo_Initialized )
	{
		// Try again with filesystem available. This is commonly needed for SDK mods which need the filesystem to find
		// their steam.inf, due to mounting SDK search paths.
		steamInfo = Sys_TryInitSteamInfo( this, g_SteamInfIDVersionInfo, info.m_pInitialMod, info.m_pBaseDirectory, bDedicated );
		Assert( steamInfo == eSteamInfo_Initialized );
		if ( steamInfo != eSteamInfo_Initialized )
		{
			Warning( "Failed to find steam.inf or equivalent steam info. May not have proper information to connect to Steam.\n" );
		}
	}

	m_bSupportsVR = false;
	if ( IsPC() )
	{
		KeyValues *modinfo = new KeyValues("ModInfo");
		if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
		{
			// Enable file tracking - client always does this in case it connects to a pure server.
			// server only does this if sv_pure is set
			// If it's not singleplayer_only
			if ( V_stricmp( modinfo->GetString("type", "singleplayer_only"), "singleplayer_only") == 0 )
			{
				DevMsg( "Disabling whitelist file tracking in filesystem...\n" );
				g_pFileSystem->EnableWhitelistFileTracking( false, false, false );
			}
			else
			{
				DevMsg( "Enabling whitelist file tracking in filesystem...\n" );
				g_pFileSystem->EnableWhitelistFileTracking( true, false, false );
			}

			m_bSupportsVR = modinfo->GetInt( "supportsvr" ) > 0 && CommandLine()->CheckParm( "-vr" );
			if ( m_bSupportsVR )
			{
				// This also has to happen before CreateGameWindow to know where to put
				// the window and how big to make it
				if ( InitVR() )
				{
					if ( Steam3Client().SteamUtils() )
					{
						if ( Steam3Client().SteamUtils()->IsSteamRunningInVR() && g_pSourceVR->IsHmdConnected() )
						{
							int nForceVRAdapterIndex = g_pSourceVR->GetVRModeAdapter();
							materials->SetAdapter( nForceVRAdapterIndex, 0 );

							g_pSourceVR->SetShouldForceVRMode();
						}
					}
				}
			}

		}
		modinfo->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::Init() 
{
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bRunningSimulation = false;

	// Initialize the FPU control word
#if defined(WIN32) && !defined( SWDS ) && !defined( _X360 )
	_asm
	{
		fninit
	}
#endif

	SetupFPUControlWord();

	// This creates the videomode singleton object, it doesn't depend on the registry
	VideoMode_Create();

	// Initialize the editor hwnd to render into
	m_hEditorHWnd = NULL;

	// One-time setup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	if ( !OnStartup( m_StartupInfo.m_pInstance, m_StartupInfo.m_pInitialMod ) )
	{
		return HandleSetModeError();
	}

	return INIT_OK; 
}

void CEngineAPI::Shutdown() 
{
	VideoMode_Destroy();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the engine to run in a particular editor window
//-----------------------------------------------------------------------------
void CEngineAPI::SetEngineWindow( void *hWnd )
{
	if ( !InEditMode() )
		return;

	// Detach input from the previous editor window
	game->InputDetachFromGameWindow();

	m_hEditorHWnd = hWnd;
	videomode->SetGameWindow( m_hEditorHWnd );
}


//-----------------------------------------------------------------------------
// Posts a console command
//-----------------------------------------------------------------------------
void CEngineAPI::PostConsoleCommand( const char *pCommand )
{
	Cbuf_AddText( pCommand );
}

	
//-----------------------------------------------------------------------------
// Is the engine currently rinning?
//-----------------------------------------------------------------------------
bool CEngineAPI::IsRunningSimulation() const
{
	return (eng->GetState() == IEngine::DLL_ACTIVE);
}


//-----------------------------------------------------------------------------
// Reset the map we're on
//-----------------------------------------------------------------------------
void CEngineAPI::SetMap( const char *pMapName )
{
//	if ( !Q_stricmp( sv.mapname, pMapName ) )
//		return;

	char buf[MAX_PATH];
	Q_snprintf( buf, MAX_PATH, "map %s", pMapName );
	Cbuf_AddText( buf );
}


//-----------------------------------------------------------------------------
// Start/stop running the simulation
//-----------------------------------------------------------------------------
void CEngineAPI::ActivateSimulation( bool bActive )
{
	// FIXME: Not sure what will happen in this case
	if ( ( eng->GetState() != IEngine::DLL_ACTIVE )	&&
		 ( eng->GetState() != IEngine::DLL_PAUSED ) )
	{
		return;
	}

	bool bCurrentlyActive = (eng->GetState() != IEngine::DLL_PAUSED);
	if ( bActive == bCurrentlyActive )
		return;

	// FIXME: Should attachment/detachment be part of the state machine in IEngine?
	if ( !bActive )
	{
		eng->SetNextState( IEngine::DLL_PAUSED );

		// Detach input from the previous editor window
		game->InputDetachFromGameWindow();
	}
	else
	{
		eng->SetNextState( IEngine::DLL_ACTIVE );

		// Start accepting input from the new window
		// FIXME: What if the attachment fails?
		game->InputAttachToGameWindow();
	}
}

static void MoveConsoleWindowToFront()
{
#ifdef _WIN32
	// Move the window to the front.
	HINSTANCE hInst = LoadLibrary( "kernel32.dll" );
	if ( hInst )
	{
		typedef HWND (*GetConsoleWindowFn)();
		GetConsoleWindowFn fn = (GetConsoleWindowFn)GetProcAddress( hInst, "GetConsoleWindow" );
		if ( fn )
		{
			HWND hwnd = fn();
			ShowWindow( hwnd, SW_SHOW );
			UpdateWindow( hwnd );
			SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
		}
		FreeLibrary( hInst );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Message pump when running stand-alone
//-----------------------------------------------------------------------------
void CEngineAPI::PumpMessages()
{
	// This message pumping happens in SDL if SDL is enabled.
#if defined( PLATFORM_WINDOWS ) && !defined( USE_SDL )
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
#endif

#if defined( USE_SDL )
	g_pLauncherMgr->PumpWindowsMessageLoop();
#endif

	// Get input from attached devices
	g_pInputSystem->PollInputState();

	if ( IsX360() )
	{
		// handle Xbox system messages
		XBX_ProcessEvents();
	}

	// NOTE: Under some implementations of Win9x, 
	// dispatching messages can cause the FPU control word to change
	if ( IsPC() )
	{
		SetupFPUControlWord();
	}

	game->DispatchAllStoredGameMessages();

	if ( IsPC() )
	{
		static bool s_bFirstRun = true;
		if ( s_bFirstRun )
		{
			s_bFirstRun = false;
			MoveConsoleWindowToFront();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message pump when running stand-alone
//-----------------------------------------------------------------------------
void CEngineAPI::PumpMessagesEditMode( bool &bIdle, long &lIdleCount )
{

	if ( bIdle && !g_pHammer->HammerOnIdle( lIdleCount++ ) )
	{
		bIdle = false;
	}

	// Get input from attached devices
	g_pInputSystem->PollInputState();

#ifdef WIN32
	MSG msg;
	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
	{
		if ( msg.message == WM_QUIT )
		{
			eng->SetQuitting( IEngine::QUIT_TODESKTOP );
			break;
		}

		if ( !g_pHammer->HammerPreTranslateMessage(&msg) )
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Reset idle state after pumping idle message.
		if ( g_pHammer->HammerIsIdleMessage(&msg) )
		{
			bIdle = true;
			lIdleCount = 0;
		}
	}
#elif defined( USE_SDL )
	Error( "Not supported" );
#else
#error
#endif


	// NOTE: Under some implementations of Win9x, 
	// dispatching messages can cause the FPU control word to change
	SetupFPUControlWord();

	game->DispatchAllStoredGameMessages();
}

//-----------------------------------------------------------------------------
// Activate/deactivates edit mode shaders
//-----------------------------------------------------------------------------
void CEngineAPI::ActivateEditModeShaders( bool bActive )
{
	if ( InEditMode() && ( g_pMaterialSystemConfig->bEditMode != bActive ) )
	{
		MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
		config.bEditMode = bActive;
		OverrideMaterialSystemConfig( config );
	}
}


#ifdef GPROFILER
static bool g_gprofiling = false;

CON_COMMAND( gprofilerstart, "Starts the gperftools profiler recording to the specified file." )
{
	if ( g_gprofiling )
	{
		Msg( "Profiling is already started.\n" );
		return;
	}

	char buffer[500];
	const char* profname = buffer;
	if ( args.ArgC() < 2 )
	{
		static const char *s_pszHomeDir = getenv("HOME");
		if ( !s_pszHomeDir )
		{
			Msg( "Syntax: gprofile <outputfilename>\n" );
			return;
		}

		// Use the current date and time to create a unique file name.time_t t = time(NULL);
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);

		V_sprintf_safe( buffer, "%s/valveprofile_%4d_%02d_%02d_%02d.%02d.%02d.prof", s_pszHomeDir,
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec );
		// profname already points to buffer.
	}
	else
	{
		profname = args[1];
	}

	int result = ProfilerStart( profname );
	if ( result )
	{
		Msg( "Profiling started successfully. Recording to %s. Stop profiling with gprofilerstop.\n", profname );
		g_gprofiling = true;
	}
	else
	{
		Msg( "Profiling to %s failed to start - errno = %d.\n", profname, errno );
	}
}

CON_COMMAND( gprofilerstop, "Stops the gperftools profiler." )
{
	if ( g_gprofiling )
	{
		ProfilerStop();
		Msg( "Stopped profiling.\n" );
		g_gprofiling = false;
	}
}
#endif


void StopGProfiler()
{
#ifdef GPROFILER
	gprofilerstop( CCommand() );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Message pump
//-----------------------------------------------------------------------------
bool CEngineAPI::MainLoop()
{
	bool bIdle = true;
	long lIdleCount = 0;

	// Main message pump
	while ( true )
	{
		// Pump messages unless someone wants to quit
		if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		{
			// We have to explicitly stop the profiler since otherwise symbol
			// resolution doesn't work correctly.
			StopGProfiler();
			if ( eng->GetQuitting() != IEngine::QUIT_TODESKTOP )
				return true;
			return false;
		}

		// Pump the message loop
		if ( !InEditMode() )
		{
			PumpMessages();
		}
		else
		{
			PumpMessagesEditMode( bIdle, lIdleCount );
		}

		// Run engine frame + hammer frame
		if ( !InEditMode() || m_hEditorHWnd )
		{
			VCRSyncToken( "Frame" );

		// Deactivate edit mode shaders
		ActivateEditModeShaders( false );

		eng->Frame();

		// Reactivate edit mode shaders (in Edit mode only...)
		ActivateEditModeShaders( true );
		}

		if ( InEditMode() )
		{
			g_pHammer->RunFrame();
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the registry
//-----------------------------------------------------------------------------
bool CEngineAPI::InitRegistry( const char *pModName )
{
	if ( IsPC() )
	{
		char szRegSubPath[MAX_PATH];
		Q_snprintf( szRegSubPath, sizeof(szRegSubPath), "%s\\%s", "Source", pModName );
		return registry->Init( szRegSubPath );
	}
	return true;
}

void CEngineAPI::ShutdownRegistry( )
{
	if ( IsPC() )
	{
		registry->Shutdown( );
	}
}


//-----------------------------------------------------------------------------
// Initializes, shuts down VR (via sourcevr.dll)
//-----------------------------------------------------------------------------
bool CEngineAPI::InitVR()
{
	if ( m_bSupportsVR )
	{
		g_pSourceVR = (ISourceVirtualReality *)g_AppSystemFactory( SOURCE_VIRTUAL_REALITY_INTERFACE_VERSION, NULL );
		if ( g_pSourceVR )
		{
			// make sure that the sourcevr DLL we loaded is secure. If not, don't 
			// let this client connect to secure servers.
			if ( !Host_AllowLoadModule( "sourcevr" DLL_EXT_STRING, "EXECUTABLE_PATH", false ) )
			{
				Warning( "Preventing connections to secure servers because sourcevr.dll is not signed.\n" );
				Host_DisallowSecureServers();
			}
		}
	}
	return true;
}


void CEngineAPI::ShutdownVR()
{
}


//-----------------------------------------------------------------------------
// One-time setup, based on the initially selected mod
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
bool CEngineAPI::OnStartup( void *pInstance, const char *pStartupModName )
{
	// This fixes a bug on certain machines where the input will 
	// stop coming in for about 1 second when someone hits a key.
	// (true means to disable priority boost)
#ifdef WIN32
	if ( IsPC() )
	{
		SetThreadPriorityBoost( GetCurrentThread(), true ); 
	}
#endif

	// FIXME: Turn videomode + game into IAppSystems?

	// Try to create the window
	COM_TimestampedLog( "game->Init" );

	// This has to happen before CreateGameWindow to set up the instance
	// for use by the code that creates the window
	if ( !game->Init( pInstance ) )
	{
		goto onStartupError;
	}

	// Try to create the window
	COM_TimestampedLog( "videomode->Init" );

	// This needs to be after Shader_Init and registry->Init
	// This way mods can have different default video settings
	if ( !videomode->Init( ) )
	{
		goto onStartupShutdownGame;
	}

	// We need to access the registry to get various settings (specifically,
	// InitMaterialSystemConfig requires it).
	if ( !InitRegistry( pStartupModName ) )
	{
		goto onStartupShutdownVideoMode;
	}

	materials->ModInit();

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
	InitMaterialSystemConfig( InEditMode() );

#if defined( _X360 )
	XBX_NotifyCreateListener( XNOTIFY_SYSTEM|XNOTIFY_LIVE|XNOTIFY_XMP );
#endif

	ShutdownRegistry();
	return true;

	// Various error conditions
onStartupShutdownVideoMode:
	videomode->Shutdown();

onStartupShutdownGame:
	game->Shutdown();

onStartupError:
	return false;
}


//-----------------------------------------------------------------------------
// One-time shutdown (shuts down stuff set up in OnStartup)
// FIXME: This should move into the launcher!
//-----------------------------------------------------------------------------
void CEngineAPI::OnShutdown()
{
	if ( videomode )
	{
		videomode->Shutdown();
	}

	ShutdownVR();

	// Shut down the game
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );
}

static bool IsValveMod( const char *pModName )
{
	// Figure out if we're running a Valve mod or not.
	return ( Q_stricmp( GetCurrentMod(), "cstrike" ) == 0 ||
		Q_stricmp( GetCurrentMod(), "dod" ) == 0 ||
		Q_stricmp( GetCurrentMod(), "hl1mp" ) == 0 ||
		Q_stricmp( GetCurrentMod(), "tf" ) == 0 || 
		Q_stricmp( GetCurrentMod(), "tf_beta" ) == 0 ||
		Q_stricmp( GetCurrentMod(), "hl2mp" ) == 0 );
}

//-----------------------------------------------------------------------------
// Initialization, shutdown of a mod.
//-----------------------------------------------------------------------------
bool CEngineAPI::ModInit( const char *pModName, const char *pGameDir )
{
	// Set up the engineparms_t which contains global information about the mod
	host_parms.mod = COM_StringCopy( GetModDirFromPath( pModName ) );
	host_parms.game = COM_StringCopy( pGameDir );

	// By default, restrict server commands in Valve games and don't restrict them in mods.
	cl.m_bRestrictServerCommands = IsValveMod( host_parms.mod );
	cl.m_bRestrictClientCommands = cl.m_bRestrictServerCommands;

	// build the registry path we're going to use for this mod
	InitRegistry( pModName );

	// This sets up the game search path, depends on host_parms
	TRACEINIT( MapReslistGenerator_Init(), MapReslistGenerator_Shutdown() );
#if !defined( _X360 )
	TRACEINIT( DevShotGenerator_Init(), DevShotGenerator_Shutdown() );
#endif

	// Slam cvars based on mod/config.cfg
	Host_ReadPreStartupConfiguration();

	bool bWindowed = g_pMaterialSystemConfig->Windowed();
	if( g_pMaterialSystemConfig->m_nVRModeAdapter != -1 )
	{
		// at init time we never want to start up full screen
		bWindowed = true;
	}

	// Create the game window now that we have a search path
	// FIXME: Deal with initial window width + height better
	if ( !videomode || !videomode->CreateGameWindow( g_pMaterialSystemConfig->m_VideoMode.m_Width, g_pMaterialSystemConfig->m_VideoMode.m_Height, bWindowed ) )
	{
		return false;
	}

	return true;
}

void CEngineAPI::ModShutdown()
{
	COM_StringFree(host_parms.mod);
	COM_StringFree(host_parms.game);
	
	// Stop accepting input from the window
	game->InputDetachFromGameWindow();

#if !defined( _X360 )
	TRACESHUTDOWN( DevShotGenerator_Shutdown() );
#endif
	TRACESHUTDOWN( MapReslistGenerator_Shutdown() );

	ShutdownRegistry();
}


//-----------------------------------------------------------------------------
// Purpose: Handles there being an error setting up the video mode
// Output : Returns true on if the engine should restart, false if it should quit
//-----------------------------------------------------------------------------
InitReturnVal_t CEngineAPI::HandleSetModeError()
{
	// show an error, see if the user wants to restart
	if ( CommandLine()->FindParm( "-safe" ) )
	{
		Sys_MessageBox( "Failed to set video mode.\n\nThis game has a minimum requirement of DirectX 7.0 compatible hardware.\n", "Video mode error", false );
		return INIT_FAILED;
	}
	
	if ( CommandLine()->FindParm( "-autoconfig" ) )
	{
		if ( Sys_MessageBox( "Failed to set video mode - falling back to safe mode settings.\n\nGame will now restart with the new video settings.", "Video - safe mode fallback", true ))
		{
			CommandLine()->AppendParm( "-safe", NULL );
			return (InitReturnVal_t)INIT_RESTART;
		}
		return INIT_FAILED;
	}

	if ( Sys_MessageBox( "Failed to set video mode - resetting to defaults.\n\nGame will now restart with the new video settings.", "Video mode warning", true ) )
	{
		CommandLine()->AppendParm( "-autoconfig", NULL );
		return (InitReturnVal_t)INIT_RESTART;
	}

	return INIT_FAILED;
}


//-----------------------------------------------------------------------------
// Purpose: Main loop for non-dedicated servers
//-----------------------------------------------------------------------------
int CEngineAPI::RunListenServer()
{
	//
	// NOTE: Systems set up here should depend on the mod 
	// Systems which are mod-independent should be set up in the launcher or Init()
	//

	// Innocent until proven guilty
	int nRunResult = RUN_OK;

	// Happens every time we start up and shut down a mod
	if ( ModInit( m_StartupInfo.m_pInitialMod, m_StartupInfo.m_pInitialGame ) )
	{
		CModAppSystemGroup modAppSystemGroup( false, m_StartupInfo.m_pParentAppSystemGroup );

		// Store off the app system factory...
		g_AppSystemFactory = modAppSystemGroup.GetFactory();

		nRunResult = modAppSystemGroup.Run();

		g_AppSystemFactory = NULL;

		// Shuts down the mod
		ModShutdown();

		// Disconnects from the editor window
		videomode->SetGameWindow( NULL );
	}

	// Closes down things that were set up in OnStartup
	// FIXME: OnStartup + OnShutdown should be removed + moved into the launcher
	// or the launcher code should be merged into the engine into the code in OnStartup/OnShutdown
	OnShutdown();

	return nRunResult;
}

static void StaticRunListenServer( void *arg )
{
	*(int *)arg = s_EngineAPI.RunListenServer();
}



// This function is set as the crash handler for unhandled exceptions and as the minidump
// handler for to be used by all of tier0's crash recording. This function
// adds a game-specific minidump comment and ensures that the SteamAPI function is
// used to save the minidump so that crashes are uploaded. SteamAPI has previously
// been configured to use breakpad by calling SteamAPI_UseBreakpadCrashHandler.
extern "C" void __cdecl WriteSteamMiniDumpWithComment( unsigned int uStructuredExceptionCode,
			struct _EXCEPTION_POINTERS * pExceptionInfo,
			const char *pszFilenameSuffix )
{
	// TODO: dynamically set the minidump comment from contextual info about the crash (i.e current VPROF node)?
#if !defined( NO_STEAM )

	if ( g_bUpdateMinidumpComment )
	{
		BuildMinidumpComment( NULL, true );
	}

	SteamAPI_WriteMiniDump( uStructuredExceptionCode, pExceptionInfo, build_number() );
	// Clear DSound Buffers so the sound doesn't loop while the game shuts down
	try
	{
		S_ClearBuffer();
	}
	catch ( ... )
	{
	}
#endif
} 

 
//-----------------------------------------------------------------------------
// Purpose: Main 
//-----------------------------------------------------------------------------
int CEngineAPI::Run()
{
	if ( CommandLine()->FindParm( "-insecure" ) || CommandLine()->FindParm( "-textmode" ) )
	{
		Host_DisallowSecureServers();
	}

#ifdef _X360
	return RunListenServer(); // don't handle exceptions on 360 (because if we do then minidumps won't work at all)
#elif defined ( _WIN32 )
	// Ensure that we crash when we do something naughty in a callback
	// such as a window proc. Otherwise on a 64-bit OS the crashes will be
	// silently swallowed.
	EnableCrashingOnCrashes();

	// Set the default minidump handling function. This is necessary so that Steam
	// will upload crashes, with comments.
	SetMiniDumpFunction( WriteSteamMiniDumpWithComment );

	// Catch unhandled crashes. A normal __try/__except block will not work across
	// the kernel callback boundary, but this does. To be clear, __try/__except
	// and try/catch will usually not catch exceptions in a WindowProc or other
	// callback that is called from kernel mode because 64-bit Windows cannot handle
	// throwing exceptions across that boundary. See this article for details:
	// http://blog.paulbetts.org/index.php/2010/07/20/the-case-of-the-disappearing-onload-exception-user-mode-callback-exceptions-in-x64/
	// Note that the unhandled exception function is not called when running
	// under a debugger, but that's fine because in that case we don't care about
	// recording minidumps.
	// The try/catch block still makes sense because it is a more reliable way
	// of catching exceptions that aren't in callbacks.
	// The unhandled exception filter will also catch crashes in threads that
	// don't have a try/catch or __try/__except block.
	bool noMinidumps = CommandLine()->FindParm( "-nominidumps");
	if ( !noMinidumps )
		MinidumpSetUnhandledExceptionFunction( WriteSteamMiniDumpWithComment );

	if ( !Plat_IsInDebugSession() && !noMinidumps )
	{
		int nRetVal = RUN_OK;
		CatchAndWriteMiniDumpForVoidPtrFn( StaticRunListenServer, &nRetVal, true );
		return nRetVal;
	}
	else
	{
		return RunListenServer();
	}
#else
	return RunListenServer();
#endif
}
#endif // SWDS

bool g_bUsingLegacyAppSystems = false;

bool CModAppSystemGroup::AddLegacySystems()
{
	g_bUsingLegacyAppSystems = true;

	AppSystemInfo_t appSystems[] = 
	{
		{ "soundemittersystem", SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }					// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

#if !defined( DEDICATED )
//	if ( CommandLine()->FindParm( "-tools" ) )
	{
		AppModule_t toolFrameworkModule = LoadModule( "engine" DLL_EXT_STRING );

		if ( !AddSystem( toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION ) )
			return false;
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Instantiate all main libraries
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::Create()
{
#ifndef SWDS
	if ( !IsServerOnly() )
{
		if ( !ClientDLL_Load() )
	return false;
}
#endif 

	if ( !ServerDLL_Load( IsServerOnly() ) )
		return false;

	IClientDLLSharedAppSystems *clientSharedSystems = 0;

#ifndef SWDS
	if ( !IsServerOnly() )
	{
		clientSharedSystems = ( IClientDLLSharedAppSystems * )g_ClientFactory( CLIENT_DLL_SHARED_APPSYSTEMS, NULL );
		if ( !clientSharedSystems )
			return AddLegacySystems();
	}
#endif

		IServerDLLSharedAppSystems *serverSharedSystems = ( IServerDLLSharedAppSystems * )g_ServerFactory( SERVER_DLL_SHARED_APPSYSTEMS, NULL );
		if ( !serverSharedSystems )
		{
			Assert( !"Expected both game and client .dlls to have or not have shared app systems interfaces!!!" );
			return AddLegacySystems();
		}
	
	// Load game and client .dlls and build list then
	CUtlVector< AppSystemInfo_t >	systems;

	int i;
		int serverCount = serverSharedSystems->Count();
	for ( i = 0 ; i < serverCount; ++i )
		{
			const char *dllName = serverSharedSystems->GetDllName( i );
			const char *interfaceName = serverSharedSystems->GetInterfaceName( i );
	
			AppSystemInfo_t info;
			info.m_pModuleName = dllName;
			info.m_pInterfaceName = interfaceName;
	
			systems.AddToTail( info );
		}

	if ( !IsServerOnly() )
	{
		int clientCount = clientSharedSystems->Count();
		for ( i = 0 ; i < clientCount; ++i )
		{
			const char *dllName = clientSharedSystems->GetDllName( i );
			const char *interfaceName = clientSharedSystems->GetInterfaceName( i );

			if ( ModuleAlreadyInList( systems, dllName, interfaceName ) )
				continue;

			AppSystemInfo_t info;
			info.m_pModuleName = dllName;
			info.m_pInterfaceName = interfaceName;

			systems.AddToTail( info );
		}
	}

	AppSystemInfo_t info;
	info.m_pModuleName = "";
	info.m_pInterfaceName = "";
	systems.AddToTail( info );

	if ( !AddSystems( systems.Base() ) ) 
		return false;

#if !defined( DEDICATED )
//	if ( CommandLine()->FindParm( "-tools" ) )
	{
		AppModule_t toolFrameworkModule = LoadModule( "engine" DLL_EXT_STRING );

		if ( !AddSystem( toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION ) )
			return false;
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, we might need to verify if the interface names differ for the client versus the server
// Input  : list - 
//			*moduleName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModAppSystemGroup::ModuleAlreadyInList( CUtlVector< AppSystemInfo_t >& list, const char *moduleName, const char *interfaceName )
{
	for ( int i = 0; i < list.Count(); ++i )
	{
		if ( !Q_stricmp( list[ i ].m_pModuleName, moduleName ) )
		{
			if ( Q_stricmp( list[ i ].m_pInterfaceName, interfaceName ) )
			{
				Error( "Game and client .dlls requesting different versions '%s' vs. '%s' from '%s'\n",
					list[ i ].m_pInterfaceName, interfaceName, moduleName );
			}
			return true;
		}
	}

	return false;
}

bool CModAppSystemGroup::PreInit()
{
	return true;
}

void SV_ShutdownGameDLL();
int CModAppSystemGroup::Main()
{
	int nRunResult = RUN_OK;

	if ( IsServerOnly() )
	{
		// Start up the game engine
		if ( eng->Load( true, host_parms.basedir ) )
		{
			// If we're using STEAM, pass the map cycle list as resource hints...
			// Dedicated server drives frame loop manually
			dedicated->RunServer();

			SV_ShutdownGameDLL();
		}
	}
	else
	{
		eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

		COM_TimestampedLog( "eng->Load" );

		// Start up the game engine
		static const char engineLoadMessage[] = "Calling CEngine::Load";
		int64 nStartTime = ETWBegin( engineLoadMessage );
		if ( eng->Load( false, host_parms.basedir ) )					
		{
#if !defined(SWDS)
			ETWEnd( engineLoadMessage, nStartTime );
			toolframework->ServerInit( g_ServerFactory );

			if ( s_EngineAPI.MainLoop() )
			{
				nRunResult = RUN_RESTART;
			}

			// unload systems
			eng->Unload();

			toolframework->ServerShutdown();
#endif
			SV_ShutdownGameDLL();
		}
	}
	
	return nRunResult;
}

void CModAppSystemGroup::PostShutdown()
{
}

void CModAppSystemGroup::Destroy() 
{
	// unload game and client .dlls
	ServerDLL_Unload();
#ifndef SWDS
	if ( !IsServerOnly() )
	{
		ClientDLL_Unload();
	}
#endif
}

//-----------------------------------------------------------------------------
//
// Purpose: Expose engine interface to launcher	for dedicated servers
//
//-----------------------------------------------------------------------------
class CDedicatedServerAPI : public CTier3AppSystem< IDedicatedServerAPI >
{
	typedef CTier3AppSystem< IDedicatedServerAPI > BaseClass;

public:
	CDedicatedServerAPI() :
	  m_pDedicatedServer( 0 )
	{
	}
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

	virtual bool ModInit( ModInfo_t &info );
	virtual void ModShutdown( void );

	virtual bool RunFrame( void );

	virtual void AddConsoleText( char *text );
	virtual void UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen );
	virtual void UpdateHostname(char *pszHostname, int maxlen);

	CModAppSystemGroup *m_pDedicatedServer;
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
EXPOSE_SINGLE_INTERFACE( CDedicatedServerAPI, IDedicatedServerAPI, VENGINE_HLDS_API_VERSION );

#define LONG_TICK_TIME					0.12f // about 8/66ths of a second
#define MIN_TIME_BETWEEN_DUMPED_TICKS	5.0f;
#define MAX_DUMPS_PER_LONG_TICK			10
void Sys_Sleep ( int msec );

bool g_bLongTickWatcherThreadEnabled = false;
bool g_bQuitLongTickWatcherThread = false;
int g_bTotalDumps = 0;

DWORD __stdcall LongTickWatcherThread( void *voidPtr )
{
	int nLastTick = 0;
	double flWarnTickTime = 0.0f;
	double flNextPossibleDumpTime = Plat_FloatTime() + MIN_TIME_BETWEEN_DUMPED_TICKS;
	int nNumDumpsThisTick = 0;

	while ( eng->GetQuitting() == IEngine::QUIT_NOTQUITTING && !g_bQuitLongTickWatcherThread )
	{
		if ( sv.m_State == ss_active && sv.m_bSimulatingTicks )
		{
			int curTick = sv.m_nTickCount;
			double curTime = Plat_FloatTime();
			if ( nLastTick > 0 && nLastTick == curTick )
			{
				if ( curTime > flNextPossibleDumpTime && curTime > flWarnTickTime && nNumDumpsThisTick < MAX_DUMPS_PER_LONG_TICK )
				{
					nNumDumpsThisTick++;
					g_bTotalDumps++;
					Warning( "Long tick after tick %i. Writing minidump #%i (%i total).\n", nLastTick, nNumDumpsThisTick, g_bTotalDumps );

					if ( nNumDumpsThisTick == MAX_DUMPS_PER_LONG_TICK )
					{
						Msg( "Not writing any more minidumps for this tick.\n" );
					}

					// If you're debugging a minidump and you ended up here, you probably want to switch to the main thread.
					WriteMiniDump( "longtick" );
				}
			}

			if ( nLastTick != curTick )
			{
				if ( nNumDumpsThisTick )
				{
					Msg( "Long tick lasted about %.1f seconds.\n", curTime - (flWarnTickTime - LONG_TICK_TIME) );
					nNumDumpsThisTick = 0;
					flNextPossibleDumpTime = curTime + MIN_TIME_BETWEEN_DUMPED_TICKS;
				}

				nLastTick = curTick;
				flWarnTickTime = curTime + LONG_TICK_TIME;
			}
		}
		else
		{
			nLastTick = 0;
		}

		if ( nNumDumpsThisTick )
		{
			// We'll write the next minidump 0.06 seconds from now.
			Sys_Sleep( 60 );
		}
		else
		{
			// Check tick progress every 1/100th of a second.
			Sys_Sleep( 10 );
		}
	}

	g_bLongTickWatcherThreadEnabled = false;
	g_bQuitLongTickWatcherThread = false;

	return 0;
}

bool EnableLongTickWatcher()
{
	bool bRet = false;
	if ( !g_bLongTickWatcherThreadEnabled )
	{
		g_bQuitLongTickWatcherThread = false;
		g_bLongTickWatcherThreadEnabled = true;

		DWORD nThreadID;
		VCRHook_CreateThread(NULL, 0,
#ifdef POSIX
			(void*)
#endif
			LongTickWatcherThread, NULL, 0, (unsigned long int *)&nThreadID );

		bRet = true;
	}
	else if ( g_bQuitLongTickWatcherThread )
	{
		Msg( "Cannot create a new long tick watcher while waiting for an old one to terminate.\n" );
	}
	else
	{
		Msg( "The long tick watcher thread is already running.\n" );
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Dedicated server entrypoint
//-----------------------------------------------------------------------------
bool CDedicatedServerAPI::Connect( CreateInterfaceFn factory ) 
{ 
	if ( CommandLine()->FindParm( "-sv_benchmark" ) != 0 )
	{
		Plat_SetBenchmarkMode( true );
	}

	if ( CommandLine()->FindParm( "-dumplongticks" ) )
	{
		Msg( "-dumplongticks found on command line. Activating long tick watcher thread.\n" );
		EnableLongTickWatcher();
	}

	// Store off the app system factory...
	g_AppSystemFactory = factory;

	if ( !BaseClass::Connect( factory ) )
		return false;

	dedicated = ( IDedicatedExports * )factory( VENGINE_DEDICATEDEXPORTS_API_VERSION, NULL );
	if ( !dedicated )
		return false;

	g_pFileSystem = g_pFullFileSystem;
	g_pFileSystem->SetWarningFunc( Warning );

	if ( !Shader_Connect( false ) )
		return false;

	if ( !g_pStudioRender )
	{
		Sys_Error( "Unable to init studio render system version %s\n", STUDIO_RENDER_INTERFACE_VERSION );
		return false;
	}

	g_pPhysics = (IPhysics*)factory( VPHYSICS_INTERFACE_VERSION, NULL );

	if ( !g_pDataCache || !g_pPhysics || !g_pMDLCache )
	{
		Warning( "Engine wasn't able to acquire required interfaces!\n" );
		return false;
	}

	ConnectMDLCacheNotify();
	return true; 
}

void CDedicatedServerAPI::Disconnect() 
{
	DisconnectMDLCacheNotify();

	g_pPhysics = NULL;

	Shader_Disconnect();

	g_pFileSystem = NULL;

	ConVar_Unregister();

	dedicated = NULL;

	BaseClass::Disconnect();

	g_AppSystemFactory = NULL;
}

//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CDedicatedServerAPI::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* engine interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 0 == normal, 1 == dedicated server
//			*instance - 
//			*basedir - 
//			*cmdline - 
//			launcherFactory - 
//-----------------------------------------------------------------------------
bool CDedicatedServerAPI::ModInit( ModInfo_t &info )
{
	// Setup and write out steam_appid.txt before we launch
	bool bDedicated = true;
	eSteamInfoInit steamInfo = Sys_TryInitSteamInfo( this, g_SteamInfIDVersionInfo, info.m_pInitialMod, info.m_pBaseDirectory, bDedicated );

	eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

	// Set up the engineparms_t which contains global information about the mod
	host_parms.basedir = const_cast<char*>(info.m_pBaseDirectory);
	host_parms.mod = const_cast<char*>(GetModDirFromPath(info.m_pInitialMod));
	host_parms.game = const_cast<char*>(info.m_pInitialGame);

	g_bTextMode = info.m_bTextMode;

	TRACEINIT( COM_InitFilesystem( info.m_pInitialMod ), COM_ShutdownFileSystem() );

	if ( steamInfo != eSteamInfo_Initialized )
	{
		// Try again with filesystem available. This is commonly needed for SDK mods which need the filesystem to find
		// their steam.inf, due to mounting SDK search paths.
		steamInfo = Sys_TryInitSteamInfo( this, g_SteamInfIDVersionInfo, info.m_pInitialMod, info.m_pBaseDirectory, bDedicated );
		Assert( steamInfo == eSteamInfo_Initialized );
		if ( steamInfo != eSteamInfo_Initialized )
		{
			Warning( "Failed to find steam.inf or equivalent steam info. May not have proper information to connect to Steam.\n" );
		}
	}

	// set this up as early as possible, if the server isn't going to run pure, stop CRCing bits as we load them
	// this happens even before the ConCommand's are processed, but we need to be sure to either CRC every file
	// that is loaded, or not bother doing any
	// Note that this mirrors g_sv_pure_mode from sv_main.cpp
	int pure_mode = 1; // default to on, +sv_pure 0 or -sv_pure 0 will turn it off
	if ( CommandLine()->CheckParm("+sv_pure") )
		pure_mode = CommandLine()->ParmValue( "+sv_pure", 1 );
	else if ( CommandLine()->CheckParm("-sv_pure") )
		pure_mode = CommandLine()->ParmValue( "-sv_pure", 1 );
	if ( pure_mode )
		g_pFullFileSystem->EnableWhitelistFileTracking( true, true, CommandLine()->FindParm( "-sv_pure_verify_hashes" ) ? true : false );
	else
		g_pFullFileSystem->EnableWhitelistFileTracking( false, false, false );

	materials->ModInit();

	// Setup the material system config record, CreateGameWindow depends on it
	// (when we're running stand-alone)
#ifndef SWDS
	InitMaterialSystemConfig( true );						// !!should this be called standalone or not?
#endif

	// Initialize general game stuff and create the main window
	if ( game->Init( NULL ) )
	{
		m_pDedicatedServer = new CModAppSystemGroup( true, info.m_pParentAppSystemGroup );

		// Store off the app system factory...
		g_AppSystemFactory = m_pDedicatedServer->GetFactory();

		m_pDedicatedServer->Run();
		return true;
	}

	return false;
}

void CDedicatedServerAPI::ModShutdown( void )
{
	if ( m_pDedicatedServer )
	{
		delete m_pDedicatedServer;
		m_pDedicatedServer = NULL;
	}

	g_AppSystemFactory = NULL;

	// Unload GL, Sound, etc.
	eng->Unload();

	// Shut down memory, etc.
	game->Shutdown();

	materials->ModShutdown();
	TRACESHUTDOWN( COM_ShutdownFileSystem() );
}

bool CDedicatedServerAPI::RunFrame( void )
{
	// Bail if someone wants to quit.
	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
	{
		return false;
	}

	// Run engine frame
	eng->Frame();
	return true;
}

void CDedicatedServerAPI::AddConsoleText( char *text )
{
	Cbuf_AddText( text );
}

void CDedicatedServerAPI::UpdateStatus(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen )
{
	Host_GetHostInfo( fps, nActive, nMaxPlayers, pszMap, maxlen );
}

void CDedicatedServerAPI::UpdateHostname(char *pszHostname, int maxlen)
{
	if ( pszHostname && ( maxlen > 0 ) )
	{
		Q_strncpy( pszHostname, sv.GetName(), maxlen );
	}
}

#ifndef SWDS

class CGameUIFuncs : public IGameUIFuncs
{
public:
	bool IsKeyDown( const char *keyname, bool& isdown )
	{
		isdown = false;
		if ( !g_ClientDLL )
			return false;

		return g_ClientDLL->IN_IsKeyDown( keyname, isdown );
	}

	const char	*GetBindingForButtonCode( ButtonCode_t code )
	{
		return ::Key_BindingForKey( code );
	}

	virtual ButtonCode_t GetButtonCodeForBind( const char *bind )
	{
		const char *pKeyName = Key_NameForBinding( bind );
		if ( !pKeyName )
			return KEY_NONE;
		return g_pInputSystem->StringToButtonCode( pKeyName ) ;
	}

	void GetVideoModes( struct vmode_s **ppListStart, int *pCount )
	{
		if ( videomode )
		{
			*pCount = videomode->GetModeCount();
			*ppListStart = videomode->GetMode( 0 );
		}
		else
		{
			*pCount = 0;
			*ppListStart = NULL;
		}
	}

	void GetDesktopResolution( int &width, int &height )
	{
		int refreshrate;
		game->GetDesktopInfo( width, height, refreshrate );
	}

	virtual void SetFriendsID( uint friendsID, const char *friendsName )
	{
		cl.SetFriendsID( friendsID, friendsName );
	}

	bool IsConnectedToVACSecureServer()
	{
		if ( cl.IsConnected() )
			return Steam3Client().BGSSecure();
		return false;
	}
};

EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, VENGINE_GAMEUIFUNCS_VERSION );

#endif

CON_COMMAND( dumplongticks, "Enables generating minidumps on long ticks." )
{
	int enable = atoi( args[1] );
	if ( args.ArgC() == 1 || enable )
	{
		if ( EnableLongTickWatcher() )
		{
			Msg( "Long tick watcher thread created. Use \"dumplongticks 0\" to disable.\n" );
		}
	}
	else
	{
		// disable watcher thread if enabled
		if ( g_bLongTickWatcherThreadEnabled && !g_bQuitLongTickWatcherThread )
		{
			Msg( "Disabling the long tick watcher.\n" );
			g_bQuitLongTickWatcherThread = true;
		}
		else
		{
			Msg( "The long tick watcher is already disabled.\n" );
		}
	}
}

