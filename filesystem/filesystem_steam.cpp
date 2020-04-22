//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "basefilesystem.h"
#include "steamcommon.h"
#include "SteamInterface.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "steam/steam_api.h"
#ifdef POSIX
#include <fcntl.h>
#ifdef LINUX
#include <sys/file.h>
#endif
#include <dlfcn.h>
#define _S_IWRITE S_IWRITE 
#define _S_IWRITE S_IWRITE
#define _S_IFREG S_IFREG
#define FILE_ATTRIBUTE_OFFLINE 0x1000
#endif

#ifdef _WIN32
	extern "C"
	{
		__declspec(dllimport) int __stdcall IsDebuggerPresent();
	}
#endif

ISteamInterface *steam = NULL;
static SteamHandle_t	g_pLastErrorFile; 
static TSteamError		g_tLastError;
static TSteamError g_tLastErrorNoFile;

void CheckError( SteamHandle_t fp, TSteamError & steamError)
{
	if (steamError.eSteamError == eSteamErrorContentServerConnect)
	{
		// fatal error
#ifdef WIN32		
		// kill the current window so the user can see the error
		HWND hwnd = GetForegroundWindow();
		if (hwnd)
		{
			DestroyWindow(hwnd);
		}
		
		// show the error
		MessageBox(NULL, "Could not acquire necessary game files because the connection to Steam servers was lost.", "Source - Fatal Error", MB_OK | MB_ICONEXCLAMATION);

		// get out of here immediately
		TerminateProcess(GetCurrentProcess(), 0);
#else
		fprintf( stderr, "Could not acquire necessary game files because the connection to Steam servers was lost." );
		exit(-1);
#endif
		return;
	}

	if (fp)
	{
		if (steamError.eSteamError != eSteamErrorNone || g_tLastError.eSteamError != eSteamErrorNone)
		{
			g_pLastErrorFile = fp;
			g_tLastError = steamError;
		}
	}
	else
	{
		// write to the NULL error checker
		if (steamError.eSteamError != eSteamErrorNone || g_tLastErrorNoFile.eSteamError != eSteamErrorNone)
		{
			g_tLastErrorNoFile = steamError;
		}
	}
}


#ifdef POSIX
class CSteamFile
{
public:
	explicit CSteamFile( SteamHandle_t file, bool bWriteable, const char *pchName ) : m_File( file ), m_bWriteable( bWriteable ), m_FileName(pchName) {}
	~CSteamFile() {} 
	SteamHandle_t Handle() { return m_File; }
	bool BWriteable() { return m_bWriteable; }
	CUtlSymbol GetFileName() { return m_FileName; }
private:
	SteamHandle_t m_File;
	bool m_bWriteable;
	CUtlSymbol m_FileName;
};
#endif


class CFileSystem_Steam : public CBaseFileSystem
{
public:
	CFileSystem_Steam();
	~CFileSystem_Steam();

	// Methods of IAppSystem
	virtual InitReturnVal_t Init();
	virtual void			Shutdown();
	virtual void *			QueryInterface( const char *pInterfaceName );

	// Higher level filesystem methods requiring specific behavior
	virtual void GetLocalCopy( const char *pFileName );
	virtual int	HintResourceNeed( const char *hintlist, int forgetEverything );
	virtual CSysModule * LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly );
	virtual bool IsFileImmediatelyAvailable(const char *pFileName);

	// resource waiting
	virtual WaitForResourcesHandle_t WaitForResources( const char *resourcelist );
	virtual bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ );
	virtual void CancelWaitForResources( WaitForResourcesHandle_t handle );
	virtual bool IsSteam() const { return true; }
	virtual	FilesystemMountRetval_t MountSteamContent( int nExtraAppId = -1 );

protected:
	// implementation of CBaseFileSystem virtual functions
	virtual FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo );
	virtual void FS_setbufsize( FILE *fp, unsigned nBytes );
	virtual void FS_fclose( FILE *fp );
	virtual void FS_fseek( FILE *fp, int64 pos, int seekType );
	virtual long FS_ftell( FILE *fp );
	virtual int FS_feof( FILE *fp );
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size, FILE *fp );
	virtual size_t FS_fwrite( const void *src, size_t size, FILE *fp );
	virtual size_t FS_vfprintf( FILE *fp, const char *fmt, va_list list );
	virtual int FS_ferror( FILE *fp );
	virtual int FS_fflush( FILE *fp );
	virtual char *FS_fgets( char *dest, int destSize, FILE *fp );
	virtual int FS_stat( const char *path, struct _stat *buf, bool *pbLoadedFromSteamCache=NULL );
	virtual int FS_chmod( const char *path, int pmode );
	virtual HANDLE FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat);
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat);
	virtual bool FS_FindClose(HANDLE handle);

private:
	bool IsFileInSteamCache( const char *file );
	bool IsFileInSteamCache2( const char *file );
	void ViewSteamCache( const char* szDir, bool bRecurse );
	bool m_bSteamInitialized;
	bool m_bCurrentlyLoading;
	bool m_bAssertFilesImmediatelyAvailable;
	bool m_bCanAsync;
	bool m_bSelfMounted;
	bool m_bContentLoaded;
	bool m_bSDKToolMode;

	SteamCallHandle_t m_hWaitForResourcesCallHandle;
	int m_iCurrentReturnedCallHandle;
	HMODULE m_hSteamDLL;
	void LoadAndStartSteam();
#ifdef POSIX
	static CUtlMap< int, CInterlockedInt > m_LockedFDMap;
#endif	
};

#ifdef POSIX
CUtlMap< int, CInterlockedInt> CFileSystem_Steam::m_LockedFDMap;
#endif


//-----------------------------------------------------------------------------
// singleton
//-----------------------------------------------------------------------------
static CFileSystem_Steam g_FileSystem_Steam;
#if defined(DEDICATED)
CBaseFileSystem *BaseFileSystem_Steam( void )
{
	return &g_FileSystem_Steam;
}
#endif

#ifdef DEDICATED // "hack" to allow us to not export a stdio version of the FILESYSTEM_INTERFACE_VERSION anywhere

IFileSystem *g_pFileSystemSteam = &g_FileSystem_Steam;
IBaseFileSystem *g_pBaseFileSystemSteam = &g_FileSystem_Steam;

#else

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Steam, IFileSystem, FILESYSTEM_INTERFACE_VERSION, g_FileSystem_Steam );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Steam, IBaseFileSystem, BASEFILESYSTEM_INTERFACE_VERSION, g_FileSystem_Steam );

#endif


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CFileSystem_Steam::CFileSystem_Steam()
{
	m_bSteamInitialized = false;
	m_bCurrentlyLoading = false;
	m_bAssertFilesImmediatelyAvailable = false;
	m_bCanAsync = true;
	m_bContentLoaded = false;
	m_hWaitForResourcesCallHandle = STEAM_INVALID_CALL_HANDLE;
	m_iCurrentReturnedCallHandle = 1;
	m_hSteamDLL = NULL;
	m_bSDKToolMode = false;
#ifdef POSIX
	SetDefLessFunc( m_LockedFDMap );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileSystem_Steam::~CFileSystem_Steam()
{
	m_bSteamInitialized = false;
}

bool CFileSystem_Steam::IsFileInSteamCache2( const char *file )
{
	if ( !m_bContentLoaded || m_bSDKToolMode)
	{
		return true;
	}

	// see if the file exists
	TSteamElemInfo info;
	TSteamError error;
	
	SteamHandle_t h = steam->FindFirst( file, eSteamFindRemoteOnly, &info, &error );
	if ( h == STEAM_INVALID_HANDLE )
	{
		return false;
	}
	else
	{
		steam->FindClose( h, &error );
	}

	return true;
}


void MountDependencies( int iAppId, CUtlVector<unsigned int> &depList )
{
	TSteamError steamError;

	// Setup the buffers for the TSteamApp structure.
	char buffers[4][2048];
	TSteamApp steamApp;
	steamApp.szName = buffers[0];
	steamApp.uMaxNameChars = sizeof( buffers[0] );
	steamApp.szLatestVersionLabel = buffers[1];
	steamApp.uMaxLatestVersionLabelChars = sizeof( buffers[1] );
	steamApp.szCurrentVersionLabel = buffers[2];
	steamApp.uMaxCurrentVersionLabelChars = sizeof( buffers[2] );
	steamApp.szInstallDirName = buffers[3];
	steamApp.uMaxInstallDirNameChars = sizeof( buffers[3] );
	
	// Ask how many caches depend on this app ID.
	steam->EnumerateApp( iAppId, &steamApp, &steamError );
	if ( steamError.eSteamError != eSteamErrorNone )
		Error( "EnumerateApp( %d ) failed: %s", iAppId, steamError.szDesc );

	// Mount each cache.
	for ( int i=0; i < (int)steamApp.uNumDependencies; i++ )
	{
		TSteamAppDependencyInfo appDependencyInfo;
		steam->EnumerateAppDependency( iAppId, i, &appDependencyInfo, &steamError );
		if ( steamError.eSteamError != eSteamErrorNone )
			Error( "EnumerateAppDependency( %d, %d ) failed: %s", iAppId, i, steamError.szDesc );

		if ( depList.Find( appDependencyInfo.uAppId ) == -1 )
		{
			depList.AddToTail( appDependencyInfo.uAppId );

			// Make sure that the user owns the app before attempting to mount it
			int isSubscribed = false, isPending = false;
			steam->IsAppSubscribed( appDependencyInfo.uAppId, &isSubscribed, &isPending, &steamError );
			if ( isSubscribed ) 
			{
				steam->MountFilesystem( appDependencyInfo.uAppId, "", &steamError );
				if ( steamError.eSteamError != eSteamErrorNone && steamError.eSteamError != eSteamErrorNotSubscribed )
				{
					Error( "MountFilesystem( %d ) failed: %s", appDependencyInfo.uAppId, steamError.szDesc );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// QueryInterface: 
//-----------------------------------------------------------------------------
void *CFileSystem_Steam::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, FILESYSTEM_INTERFACE_VERSION, Q_strlen(FILESYSTEM_INTERFACE_VERSION) + 1))
		return (IFileSystem*)this;

	return CBaseFileSystem::QueryInterface( pInterfaceName );
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
InitReturnVal_t CFileSystem_Steam::Init()
{
	m_bSteamInitialized = true;
	m_bSelfMounted = false;

	LoadAndStartSteam();

	return CBaseFileSystem::Init();
}

void CFileSystem_Steam::Shutdown()
{
	Assert( m_bSteamInitialized );

	if ( !steam )
		return;


	TSteamError steamError;

	// If we're not running Steam in local mode, remove all mount points from the STEAM VFS.
	if ( !CommandLine()->CheckParm("-steamlocal") && !m_bSelfMounted && !steam->UnmountAppFilesystem(&steamError) )
	{
#ifdef WIN32
		OutputDebugString(steamError.szDesc);
#endif
		Assert(!("STEAM VFS failed to unmount"));

		// just continue on as if nothing happened
		// ::MessageBox(NULL, szErrorMsg, "Half-Life FileSystem_Steam Error", MB_OK);
		// exit( -1 );
	}

	steam->Cleanup(&steamError);

	if ( m_hSteamDLL )
	{
		Sys_UnloadModule( (CSysModule *)m_hSteamDLL );
		m_hSteamDLL = NULL; 
	}
	m_bSteamInitialized = false;
}


void CFileSystem_Steam::LoadAndStartSteam()
{
	if ( !m_hSteamDLL )
	{
		const char *pchSteamInstallPath = SteamAPI_GetSteamInstallPath();
		if ( pchSteamInstallPath )
		{
			char szSteamDLLPath[ MAX_PATH ];
#ifdef WIN32
			V_ComposeFileName( pchSteamInstallPath, "steam" DLL_EXT_STRING, szSteamDLLPath, Q_ARRAYSIZE(szSteamDLLPath) );
#elif defined(POSIX)
			V_ComposeFileName( pchSteamInstallPath, "libsteam" DLL_EXT_STRING, szSteamDLLPath, Q_ARRAYSIZE(szSteamDLLPath) );			
#else
#error
#endif
			// try to load the steam.dll from the running steam process first
			m_hSteamDLL = (HMODULE)Sys_LoadModule( szSteamDLLPath );
		}

		if ( !m_hSteamDLL )
#ifdef WIN32			
			m_hSteamDLL = (HMODULE)Sys_LoadModule( "steam" DLL_EXT_STRING );
#elif defined(POSIX)
			m_hSteamDLL = (HMODULE)Sys_LoadModule( "libsteam" DLL_EXT_STRING );
#else
#error
#endif
	}

	if ( m_hSteamDLL )
	{
		typedef void *(*PFSteamCreateInterface)( const char *pchSteam );
#ifdef WIN32
		PFSteamCreateInterface pfnSteamCreateInterface = (PFSteamCreateInterface)GetProcAddress( m_hSteamDLL, "_f" );
#else
		PFSteamCreateInterface pfnSteamCreateInterface = (PFSteamCreateInterface)dlsym( (void *)m_hSteamDLL, "_f" );
#endif
		if ( pfnSteamCreateInterface )
			steam = (ISteamInterface *)pfnSteamCreateInterface( STEAM_INTERFACE_VERSION );	
	}

	if ( !steam )
	{
		Error("CFileSystem_Steam::Init() failed: failed to find steam interface\n");
#ifdef WIN32
		::DestroyWindow( GetForegroundWindow() );
		::MessageBox(NULL, "CFileSystem_Steam::Init() failed: failed to find steam interface", "Half-Life FileSystem_Steam Error", MB_OK);
#endif
		_exit( -1 );
	}

	TSteamError steamError;
	if (!steam->Startup(STEAM_USING_FILESYSTEM | STEAM_USING_LOGGING | STEAM_USING_USERID | STEAM_USING_ACCOUNT, &steamError))
	{
		Error("SteamStartup() failed: %s\n", steamError.szDesc);
#ifdef WIN32
		::DestroyWindow( GetForegroundWindow() );
		::MessageBox(NULL, steamError.szDesc, "Half-Life FileSystem_Steam Error", MB_OK);
#endif
		_exit( -1 );
	}
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
FilesystemMountRetval_t CFileSystem_Steam::MountSteamContent( int nExtraAppId )
{
	m_bContentLoaded = true;
	FilesystemMountRetval_t retval = FILESYSTEM_MOUNT_OK;

	// MWD: This is here because of Hammer's funky startup sequence that requires MountSteamContent() be called in CHammerApp::PreInit(). Once that root problem is addressed this will be removed;
	if ( NULL == steam )
	{
		LoadAndStartSteam();
	}

	// only mount if we're already logged in
	// if we're not logged in, assume the app will login & mount the cache itself
	// this enables both the game and the platform to use this same code, even though they mount caches at different times
	int loggedIn = 0;
	TSteamError steamError;
	int result = steam->IsLoggedIn(&loggedIn, &steamError);
	if (!result || loggedIn)
	{
		if ( nExtraAppId != -1 )
		{
			m_bSDKToolMode = true;

			CUtlVector<unsigned int> depList;
			if ( nExtraAppId < -1 )
			{
				// Special way to tell them to mount a specific App ID's depots.
				MountDependencies( -nExtraAppId, depList );
				return FILESYSTEM_MOUNT_OK;
			}
			else
			{
				const char *pMainAppId = NULL;

				// If they specified extra app IDs they want to mount after the main one, then we mount 
				// the caches manually here.
#ifdef _WIN32
				// Use GetEnvironmentVariable instead of getenv because getenv doesn't pick up changes
				// to the process environment after the DLL was loaded.
				char szMainAppId[128];
				if ( GetEnvironmentVariable( "steamappid", szMainAppId, sizeof( szMainAppId ) ) != 0 )
				{
					pMainAppId = szMainAppId;
				}
#else
				// LINUX BUG: see above
				pMainAppId = getenv( "SteamAppId" );
#endif // _WIN32				

				if ( !pMainAppId )
					Error( "Extra App ID set to %d, but no SteamAppId.", nExtraAppId );

				//swapping this mount order ensures the most current engine binaries are used by tools
				MountDependencies( nExtraAppId, depList );
				MountDependencies( atoi( pMainAppId ), depList );				
				return FILESYSTEM_MOUNT_OK;
			}	
		}
		else if (!steam->MountAppFilesystem(&steamError))
		{
			Error("MountAppFilesystem() failed: %s\n", steamError.szDesc);
#ifdef WIN32
			::DestroyWindow( GetForegroundWindow() );
			::MessageBox(NULL, steamError.szDesc, "Half-Life FileSystem_Steam Error", MB_OK);
#endif
			_exit( -1 );
		}

	}
	else
	{
		m_bSelfMounted = true;
	}

	return retval;
}



//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
FILE *CFileSystem_Steam::FS_fopen( const char *filenameT, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo )
{
	char filename[MAX_PATH];

	FixUpPath ( filenameT, filename, sizeof( filename ) );

	// make sure the file is immediately available
	if (m_bAssertFilesImmediatelyAvailable && !m_bCurrentlyLoading)
	{
		if (!IsFileImmediatelyAvailable(filename))
		{
			Msg("Steam FS: '%s' not immediately available when not in loading dialog", filename);
		}
	}

	if ( !steam )
	{
		AssertMsg( 0, "CFileSystem_Steam::FS_fopen used with null steam interface!" );
		return NULL;
	}

	CFileLoadInfo dummyInfo;
	if ( !pInfo )
	{
		dummyInfo.m_bSteamCacheOnly = false;
		pInfo = &dummyInfo;
	}
	
	SteamHandle_t f = 0;

#ifdef POSIX
	FILE *pFile = NULL;
	bool bWriteable = false;
	if ( strchr(options,'w') || strchr(options,'a') )
		bWriteable = true;
	
	if (  bWriteable )
	{
		pFile = fopen( filename, options );
		if (pFile && size)
		{
			// todo: replace with filelength()? 
			struct _stat buf;
			int rt = _stat( filename, &buf );
			if (rt == 0)
			{
				*size = buf.st_size;
			}
		}
		if ( pFile )
		{
			
			// Win32 has an undocumented feature that is serialized ALL writes to a file across threads (i.e only 1 thread can open a file at a time)
			// so use flock here to mimic that behavior
			
			ThreadId_t curThread = ThreadGetCurrentId();
			
			{
				CThreadFastMutex Locklock;
				AUTO_LOCK( Locklock );
				int fd = fileno_unlocked( pFile );
				int iLockID = m_LockedFDMap.Find( fd );
				int ret = flock( fd, LOCK_EX | LOCK_NB );
				if ( ret < 0 )
				{
					if ( errno == EWOULDBLOCK  )
					{
						if ( iLockID != m_LockedFDMap.InvalidIndex() && 
							m_LockedFDMap[iLockID] != -1 && 
							curThread != m_LockedFDMap[iLockID] )
						{
							ret = flock( fd, LOCK_EX );
							if ( ret < 0 )
							{
								fclose( pFile );
								return NULL;						
							}
						}
					}
					else 
					{
						fclose( pFile );
						return NULL;
					}
				}

				if ( iLockID != m_LockedFDMap.InvalidIndex() )
					m_LockedFDMap[iLockID] = curThread;
				else
					m_LockedFDMap.Insert( fd, curThread );

			}
			rewind( pFile );
		}
	}
	else 
	{
#endif
	
	TSteamError steamError;
	unsigned int fileSize;
	int bLocal = 0;
	f = steam->OpenFileEx(filename, options, pInfo->m_bSteamCacheOnly, &fileSize, &bLocal, &steamError);

	pInfo->m_bLoadedFromSteamCache = (bLocal == 0);
	if (size)
	{
		*size = fileSize;
	}

	CheckError( f, steamError );

#ifdef POSIX
	}
	
	if ( f || pFile )
	{
		CSteamFile *steamFile = new CSteamFile( pFile ? (SteamHandle_t)pFile : f, bWriteable, filename );
		f = (SteamHandle_t)steamFile;
	}
#endif
	return (FILE *)f;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Steam::FS_setbufsize( FILE *fp, unsigned nBytes )
{
}


//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
WaitForResourcesHandle_t CFileSystem_Steam::WaitForResources( const char *resourcelist )
{
	char szResourceList[MAX_PATH];
	Q_strncpy( szResourceList, resourcelist, sizeof(szResourceList) );
	Q_DefaultExtension( szResourceList, ".lst", sizeof(szResourceList) );

	// cancel any old call
	TSteamError steamError;
	m_hWaitForResourcesCallHandle = steam->WaitForResources(szResourceList, &steamError);
	if (steamError.eSteamError == eSteamErrorNone)
	{
		// return a new call handle
		return (WaitForResourcesHandle_t)(++m_iCurrentReturnedCallHandle);
	}

	Msg("SteamWaitForResources() failed: %s\n", steamError.szDesc);
	return (WaitForResourcesHandle_t)FILESYSTEM_INVALID_HANDLE;
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
bool CFileSystem_Steam::GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ )
{
	// clear the input
	*progress = 0.0f;
	*complete = true;

	// check to see if they're using an old handle
	if (m_iCurrentReturnedCallHandle != handle)
		return false;
	if (m_hWaitForResourcesCallHandle == STEAM_INVALID_CALL_HANDLE)
		return false;

	// get the progress
	TSteamError steamError;
	TSteamProgress steamProgress;
	int result = steam->ProcessCall(m_hWaitForResourcesCallHandle, &steamProgress, &steamError);
	if (result && steamError.eSteamError == eSteamErrorNone)
	{
		// we've finished successfully
		m_hWaitForResourcesCallHandle = STEAM_INVALID_CALL_HANDLE;
		*complete = true;
		*progress = 1.0f;
		return true;
	}
	else if (steamError.eSteamError != eSteamErrorNotFinishedProcessing)
	{
		// we have an error, just call it done
		m_hWaitForResourcesCallHandle = STEAM_INVALID_CALL_HANDLE;
		Msg("SteamProcessCall(SteamWaitForResources()) failed: %s\n", steamError.szDesc);
		return false;
	}

	// return the progress
	if (steamProgress.bValid)
	{
		*progress = (float)steamProgress.uPercentDone / (100.0f * STEAM_PROGRESS_PERCENT_SCALE);
	}
	else
	{
		*progress = 0;
	}
	*complete = false;

	return (steamProgress.bValid != false);
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
void CFileSystem_Steam::CancelWaitForResources( WaitForResourcesHandle_t handle )
{
	// check to see if they're using an old handle
	if (m_iCurrentReturnedCallHandle != handle)
		return;
	if (m_hWaitForResourcesCallHandle == STEAM_INVALID_CALL_HANDLE)
		return;

	TSteamError steamError;
	steam->AbortCall(m_hWaitForResourcesCallHandle, &steamError);
	m_hWaitForResourcesCallHandle = STEAM_INVALID_CALL_HANDLE;
}


//-----------------------------------------------------------------------------
// Purpose: helper for posix file handle wrapper
//-----------------------------------------------------------------------------
#ifdef POSIX
FILE *GetFileHandle( CSteamFile *steamFile )
{
	if ( !steamFile )
		return NULL;
	
	return (FILE *)steamFile->Handle();	
}
bool BWriteable( CSteamFile *steamFile )
{
	return steamFile && steamFile->BWriteable();
}
#endif


//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Steam::FS_fclose( FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		int fd = fileno_unlocked( fp );
		fflush( fp );
		flock( fd, LOCK_UN );
		int iLockID = m_LockedFDMap.Find( fd );
		if ( iLockID != m_LockedFDMap.InvalidIndex() )
			m_LockedFDMap[ iLockID ] = -1;

		fclose( fp );
	}
	else
	{
#endif
	TSteamError steamError;
	steam->CloseFile((SteamHandle_t)fp, &steamError);
	CheckError( (SteamHandle_t)fp, steamError );
#ifdef POSIX
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Steam::FS_fseek( FILE *fp, int64 pos, int seekType )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		fseek( fp, pos, seekType );
	}
	else
	{
#endif
	TSteamError steamError;
	int result;
	result = steam->SeekFile((SteamHandle_t)fp, (int32)pos, (ESteamSeekMethod)seekType, &steamError);
	CheckError((SteamHandle_t)fp, steamError);
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CFileSystem_Steam::FS_ftell( FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return ftell(fp);
	}
	else
	{
#endif
	long steam_offset;
	TSteamError steamError;

	steam_offset = steam->TellFile((SteamHandle_t)fp, &steamError);
	if ( steamError.eSteamError != eSteamErrorNone )
	{
		CheckError((SteamHandle_t)fp, steamError);
		return -1L;
	}

	return steam_offset;
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Steam::FS_feof( FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return feof(fp);		
	}
	else
	{
#endif
	long orig_pos;
	
	// Figure out where in the file we currently are...
	orig_pos = FS_ftell(fp);
	
	if ( (SteamHandle_t)fp == g_pLastErrorFile && g_tLastError.eSteamError == eSteamErrorEOF )
		return 1;

	if ( g_tLastError.eSteamError != eSteamErrorNone )
		return 0;

	// Jump to the end...
	FS_fseek(fp, 0L, SEEK_END);

	// If we were already at the end, return true
	if ( orig_pos == FS_ftell(fp) )
		return 1;

	// Otherwise, go back to the original spot and return false.
	FS_fseek(fp, orig_pos, SEEK_SET);
	return 0;
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Steam::FS_fread( void *dest, size_t destSize, size_t size, FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return fread( dest, 1, size, fp );
	}
	else
	{
#endif
	TSteamError steamError;
	int blocksRead = steam->ReadFile(dest, 1, size, (SteamHandle_t)fp, &steamError);
	CheckError((SteamHandle_t)fp, steamError);
	return blocksRead; // steam reads in atomic blocks of "size" bytes
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Steam::FS_fwrite( const void *src, size_t size, FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
#define WRITE_CHUNK		(256 * 1024)
		if ( size > WRITE_CHUNK )
		{
			size_t remaining = size;
			const byte* current = (const byte *) src;
			size_t total = 0;
			
			while ( remaining > 0 )
			{
				size_t bytesToCopy = min(remaining, WRITE_CHUNK);
				
				total += fwrite(current, 1, bytesToCopy, fp);
				
				remaining -= bytesToCopy;
				current += bytesToCopy;
			}
			
			Assert( total == size );
			return total;
		}
		
		return fwrite(src, 1, size, fp);// return number of bytes written (because we have size = 1, count = bytes, so it return bytes)		
	}
	else
	{
#endif
	TSteamError steamError;
	int result = steam->WriteFile(src, 1, size, (SteamHandle_t)fp, &steamError);
	CheckError((SteamHandle_t)fp, steamError);
	return result;
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Steam::FS_vfprintf( FILE *fp, const char *fmt, va_list list )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return vfprintf(fp, fmt, list);		
	}
	else
	{
#endif
	int blen, plen;
	char *buf;

	if ( !fp || !fmt )
		return 0;

	// Open the null device...used by vfprintf to determine the length of
	// the formatted string.
	FILE *nullDeviceFP = fopen("nul:", "w");
	if ( !nullDeviceFP )
		return 0;

	// Figure out how long the formatted string will be...dump formatted
	// string to the bit bucket.
	blen = vfprintf(nullDeviceFP, fmt, list);
	fclose(nullDeviceFP);
	if ( !blen )
	{
		return 0;
	}

	// Get buffer in which to build the formatted string.
	buf = (char *)malloc(blen+1);
	if ( !buf )
	{
		return 0;
	}

	// Build the formatted string.
	plen = _vsnprintf(buf, blen, fmt, list);
	va_end(list);
	if ( plen != blen )
	{
		free(buf);
		return 0;
	}

	buf[ blen ] = 0;

	// Write out the formatted string.
	if ( plen != (int)FS_fwrite(buf, plen, fp) )
	{
		free(buf);
		return 0;
	}

	free(buf);
	return plen;
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Steam::FS_ferror( FILE *fp )
{
	if (fp)
	{
#ifdef POSIX
		CSteamFile *steamFile = (CSteamFile *)fp;
		fp = GetFileHandle( steamFile );
		if ( BWriteable( steamFile ) )
		{
			return ferror(fp);			
		}
		else
		{
#endif
		if ((SteamHandle_t)fp != g_pLastErrorFile)
		{
			// it's asking for an error for a previous file, return no error
			return 0;
		}

		return ( g_tLastError.eSteamError != eSteamErrorNone );
#ifdef POSIX
		}
#endif
	}
	return g_tLastErrorNoFile.eSteamError != eSteamErrorNone;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Steam::FS_fflush( FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return fflush(fp);		
	}
	else
	{
#endif
	TSteamError steamError;
	int result = steam->FlushFile((SteamHandle_t)fp, &steamError);
	CheckError((SteamHandle_t)fp, steamError);
	return result;
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CFileSystem_Steam::FS_fgets( char *dest, int destSize, FILE *fp )
{
#ifdef POSIX
	CSteamFile *steamFile = (CSteamFile *)fp;
	fp = GetFileHandle( steamFile );
	if ( BWriteable( steamFile ) )
	{
		return fgets(dest, destSize, fp);
	}
	else
	{
#endif
	unsigned char c;
	int numCharRead = 0;
	
	// Read at most n chars from the file or until a newline
	*dest = c = '\0';
	while ( (numCharRead < destSize-1) && (c != '\n') )
	{
		// Read in the next char...
		if ( FS_fread(&c, 1, 1, fp) != 1 )
		{
			if ( g_tLastError.eSteamError != eSteamErrorEOF || numCharRead == 0 )
			{
				return NULL;	// If we hit an error, return NULL.
			}
			
			numCharRead = destSize;	// Hit EOF, no more to read, all done...
		}

		else
		{
			*dest++ = c;		// add the char to the string and point to the next pos
			*dest = '\0';		// append NULL
			numCharRead++;		// count the char read
		}
	}
	return dest; // Has a NULL termination...
#ifdef POSIX
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Steam::FS_stat( const char *path, struct _stat *buf, bool *pbLoadedFromSteamCache )
{
	TSteamElemInfo Info;
	TSteamError steamError;

	if ( pbLoadedFromSteamCache )
		*pbLoadedFromSteamCache = false;

	if ( !steam )
	{
		// The dedicated server gets here once at startup. When setting up the executable path before loading
		// base modules like engine.dll, the filesystem looks for zipX.zip but we haven't mounted steam content
		// yet so steam is null.
#if !defined( DEDICATED )		
		AssertMsg( 0, "CFileSystem_Steam::FS_stat used with null steam interface!" );
#endif
		return -1;
	}

	memset(buf, 0, sizeof(struct _stat));
	int returnVal= steam->Stat(path, &Info, &steamError);
	if ( returnVal == 0 )
	{
		if (Info.bIsDir )
		{
			buf->st_mode |= _S_IFDIR;
			buf->st_size = 0;
		}
		else
		{
			if ( pbLoadedFromSteamCache )
				*pbLoadedFromSteamCache = ( Info.bIsLocal == 0 );

			// Now we want to know if it's writable or not. First see if there is a local copy.
			struct _stat testBuf;
			int rt = _stat( path, &testBuf );
			if ( rt == 0 )
			{
				// Ok, there's a local copy. Now check if the copy on our HD is writable.
				if ( testBuf.st_mode & _S_IWRITE )
					buf->st_mode |= _S_IWRITE;
			}

			buf->st_mode |= _S_IFREG;
			buf->st_size = Info.uSizeOrCount;
		}

		buf->st_atime = Info.lLastAccessTime;
		buf->st_mtime = Info.lLastModificationTime;
		buf->st_ctime = Info.lCreationTime;
	}

	CheckError(NULL, steamError);
	return returnVal;
}

#ifdef _WIN32
#include <io.h>
#endif

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Steam::FS_chmod( const char *path, int pmode )
{
	return _chmod( path, pmode );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
HANDLE CFileSystem_Steam::FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat)
{
	TSteamElemInfo steamFindInfo;
	HANDLE hResult = INVALID_HANDLE_VALUE;
	SteamHandle_t steamResult;
	TSteamError steamError;

	steamResult = steam->FindFirst(findname, eSteamFindAll, &steamFindInfo, &steamError);
	CheckError(NULL, steamError);

	if ( steamResult == STEAM_INVALID_HANDLE )
	{
		hResult = INVALID_HANDLE_VALUE;
	}
	else
	{
		hResult = (HANDLE)steamResult;
		strcpy(dat->cFileName, steamFindInfo.cszName);
		
// NEED TO DEAL WITH THIS STUFF!!!  FORTUNATELY HALF-LIFE DOESN'T USE ANY OF IT
// AND ARCANUM USES _findfirst() etc.
//
//		findInfo->ftLastWriteTime = steamFindInfo.lLastModificationTime;
//		findInfo->ftCreationTime = steamFindInfo.lCreationTime;
//		findInfo->ftLastAccessTime = steamFindInfo.lLastAccessTime;
//		findInfo->nFileSizeHigh = ;
//		findInfo->nFileSizeLow = ;

		// Determine if the found object is a directory...
		if ( steamFindInfo.bIsDir )
			dat->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
		else
			dat->dwFileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;

		// Determine if the found object was local or remote.
		// ***NOTE*** we are hijacking the FILE_ATTRIBUTE_OFFLINE bit and using it in a different
		//            (but similar) manner than the WIN32 documentation indicates ***NOTE***
		if ( steamFindInfo.bIsLocal )
			dat->dwFileAttributes &= ~FILE_ATTRIBUTE_OFFLINE;
		else
			dat->dwFileAttributes |= FILE_ATTRIBUTE_OFFLINE;
	}
	
	return hResult;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Steam::FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat)
{
	TSteamElemInfo steamFindInfo;
	bool result;
	TSteamError steamError;

	result = (steam->FindNext((SteamHandle_t)handle, &steamFindInfo, &steamError) == 0);
	CheckError(NULL, steamError);

	if ( result )
	{
		strcpy(dat->cFileName, steamFindInfo.cszName);
		if ( steamFindInfo.bIsDir )
			dat->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
		else
			dat->dwFileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
	}
	return result;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Steam::FS_FindClose(HANDLE handle)
{
	TSteamError steamError;
	int result = (steam->FindClose((SteamHandle_t)handle, &steamError) == 0); 
	CheckError(NULL, steamError);
	return result != 0;
}

//-----------------------------------------------------------------------------
// Purpose: files are always immediately available on disk
//-----------------------------------------------------------------------------
bool CFileSystem_Steam::IsFileImmediatelyAvailable(const char *pFileName)
{
	TSteamError steamError;
	return (steam->IsFileImmediatelyAvailable(pFileName, &steamError) != 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileSystem_Steam::GetLocalCopy( const char *pFileName )
{
	// Now try to find the dll under Steam so we can do a GetLocalCopy() on it
	TSteamError steamError;

/*
#ifdef WIN32
	struct _stat StatBuf;
	if ( FS_stat(pFileName, &StatBuf) == -1 )
	{
		// Use the environment search path to try and find it
		char* pPath = getenv("PATH");
	
		// Use the .EXE name to determine the root directory
		char srchPath[ MAX_PATH ];
#ifdef WIN32
		HINSTANCE hInstance = ( HINSTANCE )GetModuleHandle( 0 );
		if ( !GetModuleFileName( hInstance, srchPath, MAX_PATH ) )
		{
			::MessageBox( 0, "Failed calling GetModuleFileName", "Half-Life Steam Filesystem Error", MB_OK );
			return;
		}
#else
		srchPath[0] = '.';
		srchPath[1] = '\0';
#endif
		
		// Get the length of the root directory the .exe is in
		char* pSeperator = strrchr( srchPath, CORRECT_PATH_SEPARATOR );
		int nBaseLen = 0;
		if ( pSeperator )
		{
			nBaseLen = pSeperator - srchPath;
		}
		
		// Extract each section of the path
		char* pStart = pPath;
		char* pEnd = 0;
		bool bSearch = true;
		while ( bSearch )
		{
#ifdef WIN32
#define PATH_SEP ";"
#else
#define PATH_SEP ":"
#endif			
			pEnd = strstr( pStart, PATH_SEP );
			int nSize = pEnd - pStart;
			if ( !pEnd )
			{
				bSearch = false;
				// If pEnd is NULL then nSize will be rubbish, so calculate
				// it sensibly.
				nSize = strlen( pStart );
			}
		
			// Is this path even potentially in the base directory?
			if ( nSize > nBaseLen )
			{
				// Create a new path (relative to the base directory) by stripping off
				// nBaseLen characters and therefore doing FS_stat relative to the current
				// directory, which should be the base directory.
				Assert( sizeof(srchPath) > nBaseLen + strlen(pFileName) + 2 );
				nSize -= nBaseLen+1;
				memcpy( srchPath, pStart+nBaseLen+1, nSize );
				memcpy( srchPath+nSize, pFileName, strlen(pFileName)+1 );
				// If the path starts with a directory separator then we won't get a
				// relative path, so skip the check.
				if ( srchPath[0] != CORRECT_PATH_SEPARATOR )
				{
					if ( FS_stat(srchPath, &StatBuf) == 0 )
					{
						steam->GetLocalFileCopy(srchPath, &steamError);
						break;
					}
				}
			}
			pStart = pEnd+1;
		}
	}
	else
#endif
*/
	{
		// Convert _srv.so to .so...
		const char *pDllStringExtension = V_GetFileExtension( DLL_EXT_STRING );
		const char *pModuleExtension = pDllStringExtension ? ( pDllStringExtension - 1 ) : DLL_EXT_STRING;

		// If we got an extension, and this filename has it, then check if it's loaded.
		if( pModuleExtension && V_stristr( pFileName, pModuleExtension ) )
		{
			// We can't be copying files over the top of .so files if they're already loaded
			//	in memory. mmap2( ... MAP_PRIVATE ... ) says "it is unspecified whether changes
			//	made to the file after the mmap() call are visible in the mapped region." Testing
			//	and lots of debugging (thanks Pierre-Loup!) has shown that they are, in fact,
			//	blasted right over your nicely loaded and fixed up object.
			CSysModule *module = Sys_LoadModule( pFileName, SYS_NOLOAD );

			if( module )
			{
				// Sys_LoadModule( SYS_NOLOAD ) increments the refcount, so bump that back down.
				Sys_UnloadModule( module );
				return;
			}
		}

		steam->GetLocalFileCopy(pFileName, &steamError);
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Load a DLL
// Input  : *path 
//-----------------------------------------------------------------------------
CSysModule * CFileSystem_Steam::LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly )
{
	char szNewPath[ MAX_PATH ];
	CBaseFileSystem::ParsePathID( pFileName, pPathID, szNewPath );

	// File must end in .dll
	char szExtension[] = DLL_EXT_STRING;
	Assert( Q_strlen(pFileName) < sizeof(szNewPath) );
	
	Q_strncpy( szNewPath, pFileName, sizeof( szNewPath ) );
	if ( !Q_stristr(szNewPath, szExtension) )
	{
		Assert( strlen(pFileName) + sizeof(szExtension) < sizeof(szNewPath) );
		Q_strncat( szNewPath, szExtension, sizeof( szNewPath ), COPY_ALL_CHARACTERS );
	}

	LogFileAccess( szNewPath );
	if ( !pPathID )
	{
		pPathID = "EXECUTABLE_PATH"; // default to the bin dir
	}

	CUtlSymbol lookup = g_PathIDTable.AddString( pPathID );

	// a pathID has been specified, find the first match in the path list
	int c = m_SearchPaths.Count();
	for (int i = 0; i < c; i++)
	{
		// pak files are not allowed to be written to...
		if (m_SearchPaths[i].GetPackFile())
			continue;

		if ( m_SearchPaths[i].GetPathID() == lookup )
		{
			char newPathName[MAX_PATH];
			Q_snprintf( newPathName, sizeof(newPathName), "%s%s", m_SearchPaths[i].GetPathString(), szNewPath ); // append the path to this dir.

			// make sure the file exists, and is in the Steam cache
			
			if ( bValidatedDllOnly && !IsFileInSteamCache(newPathName) )
				continue;

			// Get a local copy from Steam
			bool bGetLocalCopy = true;

			if ( m_bSDKToolMode )
				bGetLocalCopy = false;
#ifdef _WIN32
			if ( IsDebuggerPresent() )
				bGetLocalCopy = false;
#endif
			if ( bGetLocalCopy )
				GetLocalCopy( newPathName );

			CSysModule *module = Sys_LoadModule( newPathName );
			if ( module ) // we found the binary in one of our search paths
			{
				if ( bValidatedDllOnly && !IsFileInSteamCache2(newPathName) )
				{
					return NULL;
				}
				else
				{
					return module;
				}
			}
		}
	}

	if ( bValidatedDllOnly && IsFileInSteamCache(szNewPath) )
	{
		// couldn't load it from any of the search paths, let LoadLibrary try
		return Sys_LoadModule( szNewPath ); 
	}

	return NULL;
}

void CFileSystem_Steam::ViewSteamCache(const char* szDir, bool bRecurse)
{
	TSteamElemInfo info;
	TSteamError error;
	char szPath[MAX_PATH];

	V_snprintf( szPath, sizeof(szPath),"%s%c*.*", szDir, CORRECT_PATH_SEPARATOR );

	SteamHandle_t h = steam->FindFirst( szPath, eSteamFindRemoteOnly, &info, &error );
	int ret = 0;

	if ( h != STEAM_INVALID_HANDLE )
	{
		do 
		{
			Msg( "View Steam Cache: '%s%c%s' \n", szDir, CORRECT_PATH_SEPARATOR, info.cszName );

			if ( bRecurse && info.bIsDir && (0 == V_stristr( info.cszName, "." ) ) )
			{
				V_snprintf( szPath, sizeof(szPath),"%s%c%s", szDir, CORRECT_PATH_SEPARATOR, info.cszName );
				ViewSteamCache( szPath, true );
			}

			ret = steam->FindNext( h, &info, &error );

		} while( 0 == ret );

		steam->FindClose( h, &error );
	}
}


// HACK HACK - to allow IsFileInSteamCache() to use the old C exported interface
extern "C" SteamHandle_t	SteamFindFirst( const char *cszPattern, ESteamFindFilter eFilter, TSteamElemInfo *pFindInfo, TSteamError *pError );
extern "C" int				SteamFindClose( SteamHandle_t hDirectory, TSteamError *pError );

//-----------------------------------------------------------------------------
// Purpose: returns true if the file exists and is in a mounted Steam cache
//-----------------------------------------------------------------------------
bool CFileSystem_Steam::IsFileInSteamCache( const char *file )
{
	if ( !m_bContentLoaded || m_bSDKToolMode )
	{
		return true;
	}

	// see if the file exists
	TSteamElemInfo info;
	TSteamError error;
	
	SteamHandle_t h = steam->FindFirst( file, eSteamFindRemoteOnly, &info, &error );
	if ( h == STEAM_INVALID_HANDLE )
	{
		return false;
	}
	else
	{
		steam->FindClose( h, &error );
	}

	return true;
}


int CFileSystem_Steam::HintResourceNeed( const char *hintlist, int forgetEverything )
{
	TSteamError steamError;
	int result = steam->HintResourceNeed( hintlist, forgetEverything, &steamError );
	CheckError(NULL, steamError);
	return result;
}


