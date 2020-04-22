//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif

#if !defined( DONT_PROTECT_FILEIO_FUNCTIONS )
#define DONT_PROTECT_FILEIO_FUNCTIONS // for protected_things.h
#endif

#if defined( PROTECTED_THINGS_ENABLE )
#undef PROTECTED_THINGS_ENABLE // from protected_things.h
#endif

#include <stdio.h>
#include "tier1/interface.h"
#include "basetypes.h"
#include "tier0/dbg.h"
#include <string.h>
#include <stdlib.h>
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier0/stacktools.h"
#include "tier0/threadtools.h"
#ifdef _WIN32
#include <direct.h> // getcwd
#endif
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#ifdef _PS3
#include "sys/prx.h"
#include "tier1/utlvector.h"
#include "ps3/ps3_platform.h"
#include "ps3/ps3_win32stubs.h"
#include "ps3/ps3_helpers.h"
#include "ps3_pathinfo.h"
#elif defined(POSIX)
#include "tier0/platform.h"
#endif // _PS3

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------------------ //
// InterfaceReg.
// ------------------------------------------------------------------------------------ //
#ifdef POSIX
DLL_GLOBAL_EXPORT
#endif
InterfaceReg *s_pInterfaceRegs;

InterfaceReg::InterfaceReg( InstantiateInterfaceFn fn, const char *pName ) :
	m_pName(pName)
{
	m_CreateFn = fn;
	m_pNext = s_pInterfaceRegs;
	s_pInterfaceRegs = this;
}

// ------------------------------------------------------------------------------------ //
// CreateInterface.
// This is the primary exported function by a dll, referenced by name via dynamic binding
// that exposes an opqaue function pointer to the interface.
//
// We have the Internal variant so Sys_GetFactoryThis() returns the correct internal 
// symbol under GCC/Linux/Mac as CreateInterface is DLL_EXPORT so its global so the loaders
// on those OS's pick exactly 1 of the CreateInterface symbols to be the one that is process wide and 
// all Sys_GetFactoryThis() calls find that one, which doesn't work. Using the internal walkthrough here
// makes sure Sys_GetFactoryThis() has the dll specific symbol and GetProcAddress() returns the module specific
// function for CreateInterface again getting the dll specific symbol we need.
// ------------------------------------------------------------------------------------ //
void* CreateInterfaceInternal( const char *pName, int *pReturnCode )
{
	InterfaceReg *pCur;
	
	for (pCur=s_pInterfaceRegs; pCur; pCur=pCur->m_pNext)
	{
		if (strcmp(pCur->m_pName, pName) == 0)
		{
			if (pReturnCode)
			{
				*pReturnCode = IFACE_OK;
			}
			return pCur->m_CreateFn();
		}
	}
	
	if (pReturnCode)
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}

void* CreateInterface( const char *pName, int *pReturnCode )
{
    return CreateInterfaceInternal( pName, pReturnCode );
}



#if defined( POSIX ) && !defined( _PS3 )
// Linux doesn't have this function so this emulates its functionality
void *GetModuleHandle(const char *name)
{
	void *handle;

	if( name == NULL )
	{
		// hmm, how can this be handled under linux....
		// is it even needed?
		return NULL;
	}

    if( (handle=dlopen(name, RTLD_NOW))==NULL)
    {
            printf("DLOPEN Error:%s\n",dlerror());
            // couldn't open this file
            return NULL;
    }

	// read "man dlopen" for details
	// in short dlopen() inc a ref count
	// so dec the ref count by performing the close
	dlclose(handle);
	return handle;
}
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
static void *Sys_GetProcAddress( const char *pModuleName, const char *pName )
{
#if defined( _PS3 )
	Assert( !"Unsupported, use HMODULE" );
	return NULL;
#else // !_PS3
	HMODULE hModule = (HMODULE)GetModuleHandle( pModuleName );
#if defined( WIN32 )
	return (void *)GetProcAddress( hModule, pName );
#else // !WIN32
	return (void *)dlsym( (void *)hModule, pName );
#endif // WIN32
#endif // _PS3
}

static void *Sys_GetProcAddress( HMODULE hModule, const char *pName )
{
#if defined( WIN32 )
	return (void *)GetProcAddress( hModule, pName );
#elif defined( _PS3 )
	PS3_LoadAppSystemInterface_Parameters_t *pPRX = reinterpret_cast< PS3_LoadAppSystemInterface_Parameters_t * >( hModule );
	if ( !pPRX )
		return NULL;
	if ( !strcmp( pName, CREATEINTERFACE_PROCNAME ) )
		return reinterpret_cast< void * >( pPRX->pfnCreateInterface );
	Assert( !"Unknown PRX function requested!" );
	return NULL;
#else
	return (void *)dlsym( (void *)hModule, pName );
#endif
}

bool Sys_IsDebuggerPresent()
{
	return Plat_IsInDebugSession();
}

struct ThreadedLoadLibaryContext_t
{
	const char *m_pLibraryName;
	HMODULE m_hLibrary;
	DWORD m_nError;
	ThreadedLoadLibaryContext_t() : m_pLibraryName(NULL), m_hLibrary(0), m_nError(0) {}
};

#ifdef _WIN32

// wraps LoadLibraryEx() since 360 doesn't support that
static HMODULE InternalLoadLibrary( const char *pName )
{
#if defined(_X360)
	return LoadLibrary( pName );
#else
	return LoadLibraryEx( pName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#endif
}
unsigned ThreadedLoadLibraryFunc( void *pParam )
{
	ThreadedLoadLibaryContext_t *pContext = (ThreadedLoadLibaryContext_t*)pParam;
	pContext->m_hLibrary = InternalLoadLibrary(pContext->m_pLibraryName);
	return 0;
}
#endif


// global to propagate a library load error from thread into Sys_LoadModule
static DWORD g_nLoadLibraryError = 0;

static HMODULE Sys_LoadLibraryGuts( const char *pLibraryName )
{
#ifdef PLATFORM_PS3

	PS3_LoadAppSystemInterface_Parameters_t *pPRX = new PS3_LoadAppSystemInterface_Parameters_t;
	V_memset( pPRX, 0, sizeof( PS3_LoadAppSystemInterface_Parameters_t ) );
	pPRX->cbSize = sizeof( PS3_LoadAppSystemInterface_Parameters_t );
	int iResult = PS3_PrxLoad( pLibraryName, pPRX );
	if ( iResult < CELL_OK )
	{
		delete pPRX;
		return NULL;
	}
	return reinterpret_cast< HMODULE >( pPRX );

#else

	char str[1024];

	// How to get a string out of a #define on the command line.
	const char *pModuleExtension = DLL_EXT_STRING;	
	const char *pModuleAddition = pModuleExtension;

	V_strncpy( str, pLibraryName, sizeof(str) );
	if ( !V_stristr( str, pModuleExtension ) )
	{
		if ( IsX360() )
		{
			V_StripExtension( str, str, sizeof(str) );
		}
		V_strncat( str, pModuleAddition, sizeof(str) );
	}
	V_FixSlashes( str );

#ifdef _WIN32
	ThreadedLoadLibraryFunc_t threadFunc = GetThreadedLoadLibraryFunc();
	if ( !threadFunc )
	{
		HMODULE retVal = InternalLoadLibrary( str );
		if( retVal )
		{
			StackToolsNotify_LoadedLibrary( str );
		}
#if 0	// you can enable this block to help track down why a module isn't loading:
		else
		{
#ifdef  _WINDOWS
			char buf[1024];
			FormatMessage( 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				0, // Default language
				(LPTSTR) buf,
				1023,
				NULL  // no insert arguments
				);
			Warning( "Could not load %s: %s\n", str, buf );
#endif
		}
#endif

		return retVal;
	}

	ThreadedLoadLibaryContext_t context;
	context.m_pLibraryName = str;
	context.m_hLibrary = 0;

	ThreadHandle_t h = CreateSimpleThread( ThreadedLoadLibraryFunc, &context );

#ifdef _X360
	ThreadSetAffinity( h, XBOX_PROCESSOR_3 );
#endif

	unsigned int nTimeout = 0;
	while( WaitForSingleObject( (HANDLE)h, nTimeout ) == WAIT_TIMEOUT )
	{
		nTimeout = threadFunc();
	}

	ReleaseThreadHandle( h );

	if( context.m_hLibrary )
	{
		g_nLoadLibraryError = 0;
		StackToolsNotify_LoadedLibrary( str );
	}
	else
	{
		g_nLoadLibraryError = context.m_nError;
	}

	return context.m_hLibrary;

#elif defined( POSIX ) && !defined( _PS3 )
	HMODULE ret = (HMODULE)dlopen( str, RTLD_NOW );
	if ( ! ret )
	{
		const char *pError = dlerror();
		if ( pError && ( strstr( pError, "No such file" ) == 0 ) && ( strstr( pError, "image not found") == 0 ) )
		{
			Msg( " failed to dlopen %s error=%s\n", str, pError );

		}
	}

// 	if( ret )
// 		StackToolsNotify_LoadedLibrary( str );
	
	return ret;
#endif

#endif
}

static HMODULE Sys_LoadLibrary( const char *pLibraryName )
{
	// load a library. If a library suffix is set, look for the library first with that name
	char *pSuffix = NULL;
	
	if ( CommandLine()->FindParm( "-xlsp" ) )
	{
		pSuffix = "_xlsp";
	}
#ifdef POSIX
	else if ( CommandLine()->FindParm( "-valveinternal" ) )
	{
		pSuffix = "_valveinternal";
	}
#endif
#ifdef IS_WINDOWS_PC
	else if ( CommandLine()->FindParm( "-ds" ) )			// windows DS bins
	{
		pSuffix = "_ds";
	}
#endif
	if ( pSuffix )
	{
		char nameBuf[MAX_PATH];
		strcpy( nameBuf, pLibraryName );
		char *pDot = strchr( nameBuf, '.' );
		if ( pDot )
			*pDot = 0;
		V_strncat( nameBuf, pSuffix, sizeof( nameBuf ), COPY_ALL_CHARACTERS );
		HMODULE hRet = Sys_LoadLibraryGuts( nameBuf );
		if ( hRet )
			return hRet;
	}
	return Sys_LoadLibraryGuts( pLibraryName );
}



//-----------------------------------------------------------------------------
// Purpose: Keeps a flag if the current dll/exe loaded any debug modules
// This flag can also get set if the current process discovers any other debug
// modules loaded by other dlls
//-----------------------------------------------------------------------------
static bool s_bRunningWithDebugModules = false;

#ifdef IS_WINDOWS_PC
//-----------------------------------------------------------------------------
// Purpose: Construct a process-specific name for kernel object to track
// if any debug modules were loaded
//-----------------------------------------------------------------------------
static void DebugKernelMemoryObjectName( char *pszNameBuffer )
{
	sprintf( pszNameBuffer, "VALVE-MODULE-DEBUG-%08X", GetCurrentProcessId() );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule *Sys_LoadModule( const char *pModuleName )
{
	// If using the Steam filesystem, either the DLL must be a minimum footprint
	// file in the depot (MFP) or a filesystem GetLocalCopy() call must be made
	// prior to the call to this routine.
	HMODULE hDLL = NULL;

	char alteredFilename[ MAX_PATH ];
	if ( IsPS3() )
	{
		// PS3's load module *must* be fed extensions. If the extension is missing, add it. 
		if (!( strstr(pModuleName, ".sprx") || strstr(pModuleName, ".prx") ))
		{
			strncpy( alteredFilename, pModuleName, MAX_PATH );
			strncat( alteredFilename, DLL_EXT_STRING, MAX_PATH );
			pModuleName = alteredFilename;
		}
	}
	else
	{
		alteredFilename; // just to quash the warning
	}

	if ( !V_IsAbsolutePath( pModuleName ) )
	{
		// full path wasn't passed in, using the current working dir
		char szAbsoluteModuleName[1024];
#if defined( _PS3 ) 
		// getcwd not supported on ps3; use PRX PATCH path if patched
		if ( g_pPS3PathInfo->IsPatched() )
		{
			V_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/bin/%s",
				g_pPS3PathInfo->GamePatchBasePath(), pModuleName );
			hDLL = Sys_LoadLibrary( szAbsoluteModuleName );
		}
		if ( !hDLL ) // use base PRX path
		{
			V_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s",
				g_pPS3PathInfo->PrxPath(), pModuleName );
			hDLL = Sys_LoadLibrary( szAbsoluteModuleName );
		}
#else // !_PS3
		char szCwd[1024];
		_getcwd( szCwd, sizeof( szCwd ) );
		if ( IsX360() )
		{
			int i = CommandLine()->FindParm( "-basedir" );
			if ( i )
			{
				strcpy( szCwd, CommandLine()->GetParm( i+1 ) );
			}
		}
		if (szCwd[strlen(szCwd) - 1] == '/' || szCwd[strlen(szCwd) - 1] == '\\' )
		{
			szCwd[strlen(szCwd) - 1] = 0;
		}

		size_t cCwd = strlen( szCwd );
		if ( strstr( pModuleName, "bin/") == pModuleName || ( szCwd[ cCwd - 1 ] == 'n'  && szCwd[ cCwd - 2 ] == 'i' && szCwd[ cCwd - 3 ] == 'b' )  )
		{
			// don't make bin/bin path
			V_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s", szCwd, pModuleName );
		}
		else
		{
			V_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/bin/%s", szCwd, pModuleName );
		}
		hDLL = Sys_LoadLibrary( szAbsoluteModuleName );
#endif // _PS3
	}

	if ( !hDLL )
	{
		// full path failed, let LoadLibrary() try to search the PATH now
		hDLL = Sys_LoadLibrary( pModuleName );
#if defined( _DEBUG )
		if ( !hDLL )
		{
// So you can see what the error is in the debugger...
#if defined( _WIN32 ) && !defined( _X360 )
			char *lpMsgBuf;
			
			FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL 
			);

			LocalFree( (HLOCAL)lpMsgBuf );
#elif defined( _X360 )
			DWORD error = g_nLoadLibraryError ? g_nLoadLibraryError : GetLastError();
			Msg( "Error(%d) - Failed to load %s:\n", error, pModuleName );
#elif defined( _PS3 )
			Msg( "Failed to load %s:\n", pModuleName );
#else
			Msg( "Failed to load %s: %s\n", pModuleName, dlerror() );
#endif // _WIN32
		}
#endif // DEBUG
	}

	// If running in the debugger, assume debug binaries are okay, otherwise they must run with -allowdebug
	if ( !IsGameConsole() && Sys_GetProcAddress( hDLL, "BuiltDebug" ) )
	{
		if ( hDLL && !CommandLine()->FindParm( "-allowdebug" ) && 
			 !Sys_IsDebuggerPresent() )
		{
			Error( "Module %s is a debug build\n", pModuleName );
		}

		DevWarning( "Module %s is a debug build\n", pModuleName );

		if ( !s_bRunningWithDebugModules )
		{
			s_bRunningWithDebugModules = true;
			
#ifdef IS_WINDOWS_PC
			char chMemoryName[ MAX_PATH ];
			DebugKernelMemoryObjectName( chMemoryName );
			
			(void) CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, chMemoryName );
			// Created a shared memory kernel object specific to process id
			// Existence of this object indicates that we have debug modules loaded
#endif
		}
	}

	return reinterpret_cast<CSysModule *>(hDLL);
}

//-----------------------------------------------------------------------------
// Purpose: Determine if any debug modules were loaded
//-----------------------------------------------------------------------------
bool Sys_RunningWithDebugModules()
{
	if ( !s_bRunningWithDebugModules )
	{
#ifdef IS_WINDOWS_PC
		char chMemoryName[ MAX_PATH ];
		DebugKernelMemoryObjectName( chMemoryName );

		HANDLE hObject = OpenFileMapping( FILE_MAP_READ, FALSE, chMemoryName );
		if ( hObject && hObject != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hObject );
			s_bRunningWithDebugModules = true;
		}
#endif
	}
	return s_bRunningWithDebugModules;
}


//-----------------------------------------------------------------------------
// Purpose: Unloads a DLL/component from
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
void Sys_UnloadModule( CSysModule *pModule )
{
	if ( !pModule )
		return;

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);

#ifdef _WIN32
	FreeLibrary( hDLL );
#elif defined( _PS3 )
	PS3_PrxUnload( ( ( PS3_PrxLoadParametersBase_t *)pModule )->sysPrxId );
	delete ( PS3_PrxLoadParametersBase_t *)pModule;
#elif defined( POSIX )
	dlclose((void *)hDLL);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : module - windows HMODULE from Sys_LoadModule() 
//			*pName - proc name
// Output : factory for this module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory( CSysModule *pModule )
{
	if ( !pModule )
		return NULL;

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);
#ifdef _WIN32
	return reinterpret_cast<CreateInterfaceFn>(GetProcAddress( hDLL, CREATEINTERFACE_PROCNAME ));
#elif defined( _PS3 )
	return reinterpret_cast<CreateInterfaceFn>(Sys_GetProcAddress( hDLL, CREATEINTERFACE_PROCNAME ));
#elif defined( POSIX )
	// Linux gives this error:
	//../public/interface.cpp: In function `IBaseInterface *(*Sys_GetFactory
	//(CSysModule *)) (const char *, int *)':
	//../public/interface.cpp:154: ISO C++ forbids casting between
	//pointer-to-function and pointer-to-object
	//
	// so lets get around it :)
	return (CreateInterfaceFn)(GetProcAddress( (void *)hDLL, CREATEINTERFACE_PROCNAME ));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the instance of this module
// Output : interface_instance_t
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactoryThis( void )
{
	return &CreateInterfaceInternal;
}

//-----------------------------------------------------------------------------
// Purpose: returns the instance of the named module
// Input  : *pModuleName - name of the module
// Output : interface_instance_t - instance of that module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory( const char *pModuleName )
{
#ifdef _WIN32
	return static_cast<CreateInterfaceFn>( Sys_GetProcAddress( pModuleName, CREATEINTERFACE_PROCNAME ) );
#elif defined( _PS3 )
	Assert( 0 );
	return NULL;
#elif defined(POSIX)
	// see Sys_GetFactory( CSysModule *pModule ) for an explanation
	return (CreateInterfaceFn)( Sys_GetProcAddress( pModuleName, CREATEINTERFACE_PROCNAME ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: get the interface for the specified module and version
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool Sys_LoadInterface(
	const char *pModuleName,
	const char *pInterfaceVersionName,
	CSysModule **pOutModule,
	void **pOutInterface )
{
	CSysModule *pMod = Sys_LoadModule( pModuleName );
	if ( !pMod )
		return false;

	CreateInterfaceFn fn = Sys_GetFactory( pMod );
	if ( !fn )
	{
		Sys_UnloadModule( pMod );
		return false;
	}

	*pOutInterface = fn( pInterfaceVersionName, NULL );
	if ( !( *pOutInterface ) )
	{
		Sys_UnloadModule( pMod );
		return false;
	}

	if ( pOutModule )
		*pOutModule = pMod;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Place this as a singleton at module scope (e.g.) and use it to get the factory from the specified module name.  
// 
// When the singleton goes out of scope (.dll unload if at module scope),
//  then it'll call Sys_UnloadModule on the module so that the refcount is decremented 
//  and the .dll actually can unload from memory.
//-----------------------------------------------------------------------------
CDllDemandLoader::CDllDemandLoader( char const *pchModuleName ) : 
	m_pchModuleName( pchModuleName ), 
	m_hModule( 0 ),
	m_bLoadAttempted( false )
{
}

CDllDemandLoader::~CDllDemandLoader()
{
	Unload();
}

CreateInterfaceFn CDllDemandLoader::GetFactory()
{
	if ( !m_hModule && !m_bLoadAttempted )
	{
		m_bLoadAttempted = true;
		m_hModule = Sys_LoadModule( m_pchModuleName );
	}

	if ( !m_hModule )
	{
		return NULL;
	}

	return Sys_GetFactory( m_hModule );
}

void CDllDemandLoader::Unload()
{
	if ( m_hModule )
	{
		Sys_UnloadModule( m_hModule );
		m_hModule = 0;
	}
}
