//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//--------------------------------------------------------------------------------------------------------------
// download.cpp
// 
// Implementation file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------------------------------

// fopen is needed for the bzip code
#undef fopen

#if defined( WIN32 ) && !defined( _X360 )
#include "winlite.h"
#include <WinInet.h>
#endif

#include <assert.h>

#include "download.h"
#include "tier0/platform.h"
#include "download_internal.h"

#include "client.h"
#include "net_chan.h"

#include <KeyValues.h>
#include "filesystem.h"
#include "filesystem_engine.h"
#include "server.h"
#include "vgui_baseui_interface.h"
#include "tier0/vcrmode.h"
#include "cdll_engine_int.h"

#include "../utils/bzip2/bzlib.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "engine/idownloadsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *g_pFileSystem;
static const char *CacheDirectory = "cache";
static const char *CacheFilename = "cache/DownloadCache.db";
Color DownloadColor			(   0, 200, 100, 255 );
Color DownloadErrorColor	( 200, 100, 100, 255 );
Color DownloadCompleteColor	( 100, 200, 100, 255 );

ConVar download_debug( "download_debug", "0", FCVAR_DONTRECORD );	// For debug printouts

const char k_szDownloadPathID[] = "download";

//--------------------------------------------------------------------------------------------------------------
// Class Definitions
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Purpose: Implements download cache manager
//--------------------------------------------------------------------------------------------------------------
class DownloadCache
{
public:
	DownloadCache();
	~DownloadCache();
	void Init();

	void GetCachedData( RequestContext_t *rc );			///< Loads cached data, if any
	void PersistToDisk( const RequestContext_t *rc );		///< Writes out a completed download to disk
	void PersistToCache( const RequestContext_t *rc );	///< Writes out a partial download (lost connection, user abort, etc) to cache

private:
	KeyValues *m_cache;

	void GetCacheFilename( const RequestContext_t *rc, char cachePath[_MAX_PATH] );
	void GenerateCacheFilename( const RequestContext_t *rc, char cachePath[_MAX_PATH] );

	void BuildKeyNames( const char *gamePath );			///< Convenience function to build the keys to index into m_cache
	char m_cachefileKey[BufferSize + 64];
	char m_timestampKey[BufferSize + 64];
};
static DownloadCache *TheDownloadCache = NULL;

//--------------------------------------------------------------------------------------------------------------
DownloadCache::DownloadCache()
{
	m_cache = NULL;
}

//--------------------------------------------------------------------------------------------------------------
DownloadCache::~DownloadCache()
{
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::BuildKeyNames( const char *gamePath )
{
	if ( !gamePath )
	{
		m_cachefileKey[0] = 0;
		m_timestampKey[0] = 0;
		return;
	}

	char *tmpGamePath = V_strdup( gamePath );
	char *tmp = tmpGamePath;
	while ( *tmp )
	{
		if ( *tmp == '/' || *tmp == '\\' )
		{
			*tmp = '_';
		}
		++tmp;
	}
	Q_snprintf( m_cachefileKey, sizeof( m_cachefileKey ), "cachefile_%s", tmpGamePath );
	Q_snprintf( m_timestampKey, sizeof( m_timestampKey ), "timestamp_%s", tmpGamePath );

	delete[] tmpGamePath;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::Init()
{
	if ( m_cache )
	{
		m_cache->deleteThis();
	}

	m_cache = new KeyValues( "DownloadCache" );
	m_cache->LoadFromFile( g_pFileSystem, CacheFilename, NULL );
	g_pFileSystem->CreateDirHierarchy( CacheDirectory, "DEFAULT_WRITE_PATH" );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GetCachedData( RequestContext_t *rc )
{
	if ( !m_cache )
		return;

	char cachePath[_MAX_PATH];
	GetCacheFilename( rc, cachePath );

	if ( !(*cachePath) )
		return;

	FileHandle_t fp = g_pFileSystem->Open( cachePath, "rb" );

	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return;

	int size = g_pFileSystem->Size(fp);
	rc->cacheData = new unsigned char[size];
	int status = g_pFileSystem->Read( rc->cacheData, size, fp );
	g_pFileSystem->Close( fp );
	if ( !status )
	{
		delete[] rc->cacheData;
		rc->cacheData = NULL;
	}
	else
	{
		BuildKeyNames( rc->gamePath );
		rc->nBytesCached = size;
		strncpy( rc->cachedTimestamp, m_cache->GetString( m_timestampKey, "" ), BufferSize );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Takes a data stream compressed with bzip2, and writes it out to disk, uncompresses it, and deletes the
 *  compressed version.
 */
static bool DecompressBZipToDisk( const char *outFilename, const char *srcFilename, char *data, int bytesTotal )
{
	if ( g_pFileSystem->FileExists( outFilename ) || !data || bytesTotal < 1 )
	{
		return false;
	}

	// Create the subdirs
	char * tmpDir = V_strdup( outFilename );
	COM_CreatePath( tmpDir );
	delete[] tmpDir;

	// open the file for writing
	char fullSrcPath[MAX_PATH];
	Q_MakeAbsolutePath( fullSrcPath, sizeof( fullSrcPath ), srcFilename, com_gamedir );

	if ( !g_pFileSystem->FileExists( fullSrcPath ) )
	{
		// Write out the .bz2 file, for simplest decompression
		FileHandle_t ifp = g_pFileSystem->Open( fullSrcPath, "wb" );
		if ( !ifp )
		{
			return false;
		}
		int bytesWritten = g_pFileSystem->Write( data, bytesTotal, ifp );
		g_pFileSystem->Close( ifp );
		if ( bytesWritten != bytesTotal )
		{
			// couldn't write out all of the .bz2 file
			g_pFileSystem->RemoveFile( srcFilename );
			return false;
		}
	}

	// Prepare the uncompressed filehandle
	FileHandle_t ofp = g_pFileSystem->Open( outFilename, "wb" );
	if ( !ofp )
	{
		g_pFileSystem->RemoveFile( srcFilename );
		return false;
	}

	// And decompress!
	const int OutBufSize = 65536;
	char    buf[ OutBufSize ];
	BZFILE *bzfp = BZ2_bzopen( fullSrcPath, "rb" );
	int totalBytes = 0;

	bool bMapFile = false;
	char szOutFilenameBase[MAX_PATH];
	Q_FileBase( outFilename, szOutFilenameBase, sizeof( szOutFilenameBase ) );
	const char *pszMapName = cl.m_szLevelBaseName;
	if ( pszMapName && pszMapName[0] )
	{
		bMapFile = ( Q_stricmp( szOutFilenameBase, pszMapName ) == 0 );
	}

	while ( 1 )
	{
		int bytesRead = BZ2_bzread( bzfp, buf, OutBufSize );
		if ( bytesRead < 0 )
		{
			break; // error out
		}

		if ( bytesRead > 0 )
		{
			int bytesWritten = g_pFileSystem->Write( buf, bytesRead, ofp );
			if ( bytesWritten != bytesRead )
			{
				break; // error out
			}
			else
			{
				totalBytes += bytesWritten;
				if ( !bMapFile ) 
				{
					if ( totalBytes > MAX_FILE_SIZE )
					{
						ConDColorMsg( DownloadErrorColor, "DecompressBZipToDisk: '%s' too big (max %i bytes).\n", srcFilename, MAX_FILE_SIZE );
						break; // error out
					}
				}
			}
		}
		else
		{
			g_pFileSystem->Close( ofp );
			BZ2_bzclose( bzfp );
			g_pFileSystem->RemoveFile( srcFilename );
			return true;
		}
	}

	// We failed somewhere, so clean up and exit
	g_pFileSystem->Close( ofp );
	BZ2_bzclose( bzfp );
	g_pFileSystem->RemoveFile( srcFilename );
	g_pFileSystem->RemoveFile( outFilename );
	return false;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::PersistToDisk( const RequestContext_t *rc )
{
	if ( !m_cache )
		return;

	if ( rc && rc->data && rc->nBytesTotal )
	{
		char absPath[MAX_PATH];
		if ( rc->bIsBZ2 )
		{
			Q_StripExtension( rc->absLocalPath, absPath, sizeof( absPath ) );
		}
		else
		{
			Q_strncpy( absPath, rc->absLocalPath, sizeof( absPath ) );
		}

		if ( !g_pFileSystem->FileExists( absPath ) )
		{
			// Create the subdirs
			char * tmpDir = V_strdup( absPath );
			COM_CreatePath( tmpDir );
			delete[] tmpDir;

			bool success = false;
			if ( rc->bIsBZ2 )
			{
				success = DecompressBZipToDisk( absPath, rc->absLocalPath, reinterpret_cast< char * >(rc->data), rc->nBytesTotal );
			}
			else
			{
				FileHandle_t fp = g_pFileSystem->Open( absPath, "wb" );
				if ( fp )
				{
					g_pFileSystem->Write( rc->data, rc->nBytesTotal, fp );
					g_pFileSystem->Close( fp );
					success = true;
				}
			}

			if ( success )
			{
				// write succeeded.  remove any old data from the cache.
				char cachePath[_MAX_PATH];
				GetCacheFilename( rc, cachePath );
				if ( cachePath[0] )
				{
					g_pFileSystem->RemoveFile( cachePath, NULL );
				}

				BuildKeyNames( rc->gamePath );
				KeyValues *kv = m_cache->FindKey( m_cachefileKey, false );
				if ( kv )
				{
					m_cache->RemoveSubKey( kv );
				}
				kv = m_cache->FindKey( m_timestampKey, false );
				if ( kv )
				{
					m_cache->RemoveSubKey( kv );
				}
			}
		}
	}

	m_cache->SaveToFile( g_pFileSystem, CacheFilename, NULL );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::PersistToCache( const RequestContext_t *rc )
{
	if ( !m_cache || !rc || !rc->data || !rc->nBytesTotal || !rc->nBytesCurrent )
		return;

	char cachePath[_MAX_PATH];
	GenerateCacheFilename( rc, cachePath );

	FileHandle_t fp = g_pFileSystem->Open( cachePath, "wb" );
	if ( fp )
	{
		g_pFileSystem->Write( rc->data, rc->nBytesCurrent, fp );
		g_pFileSystem->Close( fp );

		m_cache->SaveToFile( g_pFileSystem, CacheFilename, NULL );
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GetCacheFilename( const RequestContext_t *rc, char cachePath[_MAX_PATH] )
{
	BuildKeyNames( rc->gamePath );
	const char *path = m_cache->GetString( m_cachefileKey, NULL );
	if ( !path || strncmp( path, CacheDirectory, strlen(CacheDirectory) ) )
	{
		cachePath[0] = 0;
		return;
	}
	strncpy( cachePath, path, _MAX_PATH );
	cachePath[_MAX_PATH-1] = 0;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GenerateCacheFilename( const RequestContext_t *rc, char cachePath[_MAX_PATH] )
{
	GetCacheFilename( rc, cachePath );
	BuildKeyNames( rc->gamePath );

	m_cache->SetString( m_timestampKey, rc->cachedTimestamp );
	//ConDColorMsg( DownloadColor,"DownloadCache::GenerateCacheFilename() set %s = %s\n", m_timestampKey, rc->cachedTimestamp );

	if ( !*cachePath )
	{
		const char * lastSlash = strrchr( rc->gamePath, '/' );
		const char * lastBackslash = strrchr( rc->gamePath, '\\' );
		const char *gameFilename = rc->gamePath;
		if ( lastSlash || lastBackslash )
		{
			gameFilename = max( lastSlash, lastBackslash ) + 1;
		}
		for( int i=0; i<1000; ++i )
		{
			Q_snprintf( cachePath, _MAX_PATH, "%s/%s%4.4d", CacheDirectory, gameFilename, i );
			if ( !g_pFileSystem->FileExists( cachePath ) )
			{
				m_cache->SetString( m_cachefileKey, cachePath );
				//ConDColorMsg( DownloadColor,"DownloadCache::GenerateCacheFilename() set %s = %s\n", m_cachefileKey, cachePath );
				return;
			}
		}
		// all 1000 were invalid?!?
		Q_snprintf( cachePath, _MAX_PATH, "%s/overflow", CacheDirectory );
		//ConDColorMsg( DownloadColor,"DownloadCache::GenerateCacheFilename() set %s = %s\n", m_cachefileKey, cachePath );
		m_cache->SetString( m_cachefileKey, cachePath );
	}
}

//--------------------------------------------------------------------------------------------------------------
// Purpose: Implements download manager class
//--------------------------------------------------------------------------------------------------------------
class CDownloadManager
{
public:
	CDownloadManager();
	~CDownloadManager();

	void Queue( const char *baseURL, const char *urlPath, const char *gamePath );
	void Stop() { Reset(); }
	int GetQueueSize() { return m_queuedRequests.Count(); }

	virtual bool Update();	///< Monitors download thread, starts new downloads, and updates progress bar

	bool FileReceived( const char *filename, unsigned int requestID );
	bool FileDenied( const char *filename, unsigned int requestID );

	bool HasMapBeenDownloadedFromServer( const char *serverMapName );
	void MarkMapAsDownloadedFromServer( const char *serverMapName );

private:
	void QueueInternal( const char *pBaseURL, const char *pURLPath, const char *pGamePath, bool bAsHttp, bool bCompressed );

protected:
	virtual void UpdateProgressBar();

	virtual RequestContext_t *NewRequestContext();///< Call this to allocate a RequestContext_t - calls setup functions for derived classes
	virtual bool ShouldAttemptCompressedFileDownload()		{ return true; }
	virtual void SetupURLPath( RequestContext_t *pRequestContext, const char *pURLPath );
	virtual void SetupServerURL( RequestContext_t *pRequestContext );

	// Event handlers
	virtual void OnHttpConnecting( RequestContext_t *pRequestContext ) {}
	virtual void OnHttpFetch( RequestContext_t *pRequestContext ) {}
	virtual void OnHttpDone( RequestContext_t *pRequestContext ) {}
	virtual void OnHttpError( RequestContext_t *pRequestContext ) {}

	void Reset();						///< Cancels any active download, as well as any queued ones

	void PruneCompletedRequests();		///< Check download requests that have been completed to see if their threads have exited
	void CheckActiveDownload();			///< Checks download status, and updates progress bar
	void StartNewDownload();			///< Starts a new download if there are queued requests

	typedef CUtlVector< RequestContext_t * > RequestVector;
	RequestVector m_queuedRequests;		///< these are requests waiting to be spawned
	RequestContext_t *m_activeRequest;	///< this is the active request being downloaded in another thread
	RequestVector m_completedRequests;	///< these are waiting for the thread to exit

	int m_lastPercent;					///< last percent value the progress bar was updated with (to avoid spamming it)
	int m_totalRequests;				///< Total number of requests (used to set the top progress bar)

	int m_RequestIDCounter;				///< global increasing request ID counter

	typedef CUtlVector< char * > StrVector;
	StrVector m_downloadedMaps;			///< List of maps for which we have already tried to download assets.
};

//--------------------------------------------------------------------------------------------------------------
static CDownloadManager s_DownloadManager;

//--------------------------------------------------------------------------------------------------------------
CDownloadManager::CDownloadManager()
{
	m_activeRequest = NULL;
	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
CDownloadManager::~CDownloadManager()
{
	Reset();

	for ( int i=0; i<m_downloadedMaps.Count(); ++i )
	{
		delete[] m_downloadedMaps[i];
	}
	m_downloadedMaps.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
RequestContext_t *CDownloadManager::NewRequestContext()
{
	return new RequestContext_t;
}

//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::SetupURLPath( RequestContext_t *pRequestContext, const char *pURLPath )
{
	V_strcpy( pRequestContext->urlPath, pRequestContext->gamePath );
}
//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::SetupServerURL( RequestContext_t *pRequestContext )
{
	Q_strncpy( pRequestContext->serverURL, cl.m_NetChannel->GetRemoteAddress().ToString(), BufferSize );
}
//--------------------------------------------------------------------------------------------------------------
bool CDownloadManager::HasMapBeenDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return false;

	for ( int i=0; i<m_downloadedMaps.Count(); ++i )
	{
		const char *oldServerMapName = m_downloadedMaps[i];
		if ( oldServerMapName && !stricmp( serverMapName, oldServerMapName ) )
		{
			return true;
		}
	}
	return false;
}

bool CDownloadManager::FileDenied( const char *filename, unsigned int requestID )
{
	if ( !m_activeRequest )
		return false;

	if ( m_activeRequest->nRequestID != requestID )
		return false;

	if ( m_activeRequest->bAsHTTP )
		return false;

	if ( download_debug.GetBool() )
	{
		ConDColorMsg( DownloadErrorColor, "Error downloading %s\n", m_activeRequest->absLocalPath );
	}

	UpdateProgressBar();

	// try to download the next file
	m_completedRequests.AddToTail( m_activeRequest );
	m_activeRequest = NULL;

	return true;
}

bool CDownloadManager::FileReceived( const char *filename, unsigned int requestID )
{
	if ( !m_activeRequest )
		return false;

	if ( m_activeRequest->nRequestID != requestID )
		return false;

	if ( m_activeRequest->bAsHTTP )
		return false;

	if ( download_debug.GetBool() )
	{
		ConDColorMsg( DownloadCompleteColor, "Download finished!\n" );
	}

	UpdateProgressBar();

	m_completedRequests.AddToTail( m_activeRequest );
	m_activeRequest = NULL;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::MarkMapAsDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return;

	if ( HasMapBeenDownloadedFromServer( serverMapName ) )
		return;

	m_downloadedMaps.AddToTail( V_strdup( serverMapName ) );


	return;
}

//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::QueueInternal( const char *pBaseURL, const char *pURLPath, const char *pGamePath,
									 bool bAsHttp, bool bCompressed )
{
	// NOTE: Assumes valid game path (i.e. IsGamePathValidAndSafe() has been called already)

	++m_totalRequests;

	// Initialize the download cache if necessary
	if ( !TheDownloadCache )
	{
		TheDownloadCache = new DownloadCache;
		TheDownloadCache->Init();
	}

	// Create a new context and add queue it
	RequestContext_t *rc = NewRequestContext();
	m_queuedRequests.AddToTail( rc );

	rc->bIsBZ2 = bCompressed;
	rc->bAsHTTP = bAsHttp;
	rc->status = HTTP_CONNECTING;

	// Setup base path.  We put it in the "download" search path, if they have set one
	char szBasePath[ MAX_PATH ];
	if ( g_pFileSystem->GetSearchPath( k_szDownloadPathID, false, szBasePath, sizeof(szBasePath) ) > 0 )
	{
		char *split = V_strstr( szBasePath, ";" );
		if ( split != NULL )
		{
			Warning( "Multiple download search paths?  Check gameinfo.txt" );
			*split = '\0';
		}
	}

	// Otherwise, put it in the game dir
	if ( szBasePath[0] == '\0' )
		V_strcpy_safe( szBasePath, com_gamedir );

	// Setup game path
	V_strcpy_safe( rc->gamePath, pGamePath );
	if ( bCompressed )
	{
		V_strcat_safe( rc->gamePath, ".bz2" );
	}
	Q_FixSlashes( rc->gamePath, '/' ); // only matters for debug prints, which are full URLS, so we want forward slashes

	// NOTE: Loose files on disk must always be lowercase!  At least on Linux they HAVE to be,
	// but we do the same thing on Windows to keep things consistent.
	char szGamePathLower[MAX_PATH];
	V_strcpy_safe( szGamePathLower, rc->gamePath );
	V_strlower( szGamePathLower );

	// Now set the full absolute path.  Why does the file system not provide a convenient method to
	// do stuff like this?
	V_strcpy_safe( rc->absLocalPath, szBasePath );
	V_AppendSlash( rc->absLocalPath, sizeof(rc->absLocalPath) );
	V_strcat_safe( rc->absLocalPath, szGamePathLower );
	V_FixSlashes( rc->absLocalPath );

	// Setup base URL if necessary
	if ( bAsHttp )
	{
		V_strcpy_safe( rc->baseURL, pBaseURL );
		V_StripTrailingSlash( rc->baseURL );
		V_strcat_safe( rc->baseURL, "/" );
	}

	// Call virtual methods for setting up additional context info
	SetupURLPath( rc, pURLPath );
	SetupServerURL( rc );

	if ( download_debug.GetBool() )
	{
		ConDColorMsg( DownloadColor, "Queueing %s%s.\n", rc->baseURL, pGamePath );
	}

	// Invoke the callback if appropriate
	if ( bAsHttp )
	{
		OnHttpConnecting( rc ); 
	}
}

void CDownloadManager::Queue( const char *baseURL, const char *urlPath, const char *gamePath )
{
	if ( !CL_IsGamePathValidAndSafeForDownload( gamePath ) )
		return;

	// Don't download existing files
	if ( g_pFileSystem->FileExists( gamePath ) )
		return;

#ifndef _DEBUG
	if ( sv.IsActive() )
	{
		return;	// don't try to download things for the local server (in case a map is missing sounds etc that
				// aren't needed to play.
	}
#endif

	// HTTP or NetChan?
	bool bAsHTTP = baseURL && ( !Q_strnicmp( baseURL, "http://", 7 ) || !Q_strnicmp( baseURL, "https://", 8 ) );

	// Queue up an HTTP download of the bzipped asset, in case it exists.
	// When a bzipped download finishes, we'll uncompress the file to it's
	// original destination, and the queued download of the uncompressed
	// file will abort.
	if ( bAsHTTP && ShouldAttemptCompressedFileDownload() && !g_pFileSystem->FileExists( va( "%s.bz2", gamePath ) ) )
	{
		QueueInternal( baseURL, urlPath, gamePath, true, true );
	}

	// Queue up the straight, uncompressed version
	QueueInternal( baseURL, urlPath, gamePath, bAsHTTP, false );

	if ( download_debug.GetBool() )
	{
		ConDColorMsg( DownloadColor, "Queueing %s%s.\n", baseURL, gamePath );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::Reset()
{
	// ask the active request to bail
	if ( m_activeRequest )
	{
		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadColor, "Aborting download of %s\n", m_activeRequest->gamePath );
		}

		if ( m_activeRequest->nBytesTotal && m_activeRequest->nBytesCurrent )
		{
			// Persist partial data to cache
			TheDownloadCache->PersistToCache( m_activeRequest );
		}
		m_activeRequest->shouldStop = true;
		m_completedRequests.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		//TODO: StopLoadingProgressBar();
	}

	// clear out any queued requests
	for ( int i=0; i<m_queuedRequests.Count(); ++i )
	{
		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadColor, "Discarding queued download of %s\n", m_queuedRequests[i]->gamePath );
		}

		delete m_queuedRequests[i];
	}
	m_queuedRequests.RemoveAll();

	if ( TheDownloadCache )
	{
		delete TheDownloadCache;
		TheDownloadCache = NULL;
	}

	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
// Check download requests that have been completed to see if their threads have exited
void CDownloadManager::PruneCompletedRequests()
{
	for ( int i=m_completedRequests.Count()-1; i>=0; --i )
	{
		if ( m_completedRequests[i]->threadDone || !m_completedRequests[i]->bAsHTTP )
		{
			if ( m_completedRequests[i]->cacheData )
			{
				delete[] m_completedRequests[i]->cacheData;
			}
			delete m_completedRequests[i];
			m_completedRequests.Remove( i );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
// Checks download status, and updates progress bar
void CDownloadManager::CheckActiveDownload()
{
	if ( !m_activeRequest )
		return;

	if ( !m_activeRequest->bAsHTTP )
	{
		UpdateProgressBar();
		return;
	}
	
	// check active request for completion / error / progress update
	switch ( m_activeRequest->status )
	{
	case HTTP_DONE:
		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadCompleteColor, "Download finished!\n" );
		}

		UpdateProgressBar();
		OnHttpDone( m_activeRequest );
		if ( m_activeRequest->nBytesTotal )
		{
			// Persist complete data to disk, and remove cache entry
			//TODO: SetSecondaryProgressBarText( m_activeRequest->gamePath );
			TheDownloadCache->PersistToDisk( m_activeRequest );
			m_activeRequest->shouldStop = true;
			m_completedRequests.AddToTail( m_activeRequest );
			m_activeRequest = NULL;
			if ( !m_queuedRequests.Count() )
			{
				//TODO: StopLoadingProgressBar();
				//TODO: Cbuf_AddText("retry\n");
			}
		}
		break;
	case HTTP_ERROR:
		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadErrorColor, "Error downloading %s%s\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
		}

		UpdateProgressBar();

		// try to download the next file
		m_activeRequest->shouldStop = true;
		m_completedRequests.AddToTail( m_activeRequest );
		OnHttpError( m_activeRequest );
		m_activeRequest = NULL;
		if ( !m_queuedRequests.Count() )
		{
			//TODO: StopLoadingProgressBar();
			//TODO: Cbuf_AddText("retry\n");
		}
		break;
	case HTTP_FETCH:
		UpdateProgressBar();
		// Update progress bar
		//TODO: SetSecondaryProgressBarText( m_activeRequest->gamePath );
		if ( m_activeRequest->nBytesTotal )
		{
			int percent = ( m_activeRequest->nBytesCurrent * 100 / m_activeRequest->nBytesTotal );
			if ( percent != m_lastPercent )
			{
				if ( download_debug.GetBool() )
				{
					ConDColorMsg( DownloadColor, "Downloading %s%s: %3.3d%% - %d of %d bytes\n",
						m_activeRequest->baseURL, m_activeRequest->gamePath,
						percent, m_activeRequest->nBytesCurrent, m_activeRequest->nBytesTotal );
				}

				m_lastPercent = percent;
				//TODO: SetSecondaryProgressBar( m_lastPercent * 0.01f );
			}
		}
		OnHttpFetch( m_activeRequest );
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
// Starts a new download if there are queued requests
void CDownloadManager::StartNewDownload()
{
	if ( m_activeRequest || !m_queuedRequests.Count() )
		return;

	while ( !m_activeRequest && m_queuedRequests.Count() )
	{
		// Remove one request from the queue and make it active
		m_activeRequest = m_queuedRequests[0];
		m_queuedRequests.Remove( 0 );

		if ( g_pFileSystem->FileExists( m_activeRequest->absLocalPath ) )
		{
			if ( download_debug.GetBool() )
			{
				ConDColorMsg( DownloadColor, "Skipping existing file %s%s.\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
			}

			m_activeRequest->shouldStop = true;
			m_activeRequest->threadDone = true;
			m_completedRequests.AddToTail( m_activeRequest );
			m_activeRequest = NULL;
		}
	}

	if ( !m_activeRequest )
		return;

	if ( g_pFileSystem->FileExists( m_activeRequest->absLocalPath ) )
	{
		m_activeRequest->shouldStop = true;
		m_activeRequest->threadDone = true;
		m_completedRequests.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		return; // don't download existing files
	}

	if ( m_activeRequest->bAsHTTP )
	{
		// Check cache for partial match
		TheDownloadCache->GetCachedData( m_activeRequest );

		//TODO: ContinueLoadingProgressBar( "Http", m_totalRequests - m_queuedRequests.Count(), 0.0f );
		//TODO: SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
		//TODO: SetSecondaryProgressBarText( m_activeRequest->gamePath );
		//TODO: SetSecondaryProgressBar( 0.0f );
		UpdateProgressBar();

		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadColor, "Downloading %s%s.\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
		}

		m_lastPercent = 0;

		// Start the thread
		DWORD threadID;
		VCRHook_CreateThread(NULL, 0, 
#ifdef POSIX
			(void *)
#endif
			DownloadThread, m_activeRequest, 0, (unsigned long int *)&threadID );

		ThreadDetach( ( ThreadHandle_t )threadID );
	}
	else
	{
		UpdateProgressBar();

		if ( download_debug.GetBool() )
		{
			ConDColorMsg( DownloadColor, "Downloading %s.\n", m_activeRequest->gamePath );
		}

		m_lastPercent = 0;
		
		m_activeRequest->nRequestID = cl.m_NetChannel->RequestFile( m_activeRequest->gamePath );
	}
}

//--------------------------------------------------------------------------------------------------------------
void CDownloadManager::UpdateProgressBar()
{
	if ( !m_activeRequest )
	{
		return;
	}

	wchar_t filenameBuf[MAX_OSPATH];
	float progress = 0.0f;

	if ( m_activeRequest->bAsHTTP )
	{
		int overallPercent = (m_totalRequests - m_queuedRequests.Count() - 1) * 100 / m_totalRequests;
		int filePercent = 0;
		if ( m_activeRequest->nBytesTotal > 0 )
		{	
			filePercent = ( m_activeRequest->nBytesCurrent * 100 / m_activeRequest->nBytesTotal );
		}

		progress = (overallPercent + filePercent * 1.0f / m_totalRequests) * 0.01f;
	}
	else
	{
		int received, total;
		cl.m_NetChannel->GetStreamProgress( FLOW_INCOMING, &received, &total );
		
		progress = (float)(received)/(float)(total);
	}

#ifndef DEDICATED
	_snwprintf( filenameBuf, 256, L"Downloading %hs", m_activeRequest->gamePath );
	EngineVGui()->UpdateCustomProgressBar( progress, filenameBuf );
#endif
}

//--------------------------------------------------------------------------------------------------------------
// Monitors download thread, starts new downloads, and updates progress bar
bool CDownloadManager::Update()
{
	PruneCompletedRequests();
	CheckActiveDownload();
	StartNewDownload();

	return m_activeRequest != NULL;
}

//--------------------------------------------------------------------------------------------------------------
// Externally-visible function definitions
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
bool CL_DownloadUpdate(void)
{
	return s_DownloadManager.Update();
}

//--------------------------------------------------------------------------------------------------------------
void CL_HTTPStop_f(void)
{
	s_DownloadManager.Stop();
}

bool CL_FileReceived( const char *filename, unsigned int requestID )
{
	return s_DownloadManager.FileReceived( filename, requestID );
}

bool CL_FileDenied( const char *filename, unsigned int requestID )
{
	return s_DownloadManager.FileDenied( filename, requestID );
}

//--------------------------------------------------------------------------------------------------------------
extern ConVar sv_downloadurl;
void CL_QueueDownload( const char *filename )
{
	s_DownloadManager.Queue( sv_downloadurl.GetString(), NULL, filename );
}

//--------------------------------------------------------------------------------------------------------------

int CL_GetDownloadQueueSize(void)
{
	return s_DownloadManager.GetQueueSize();
}

//--------------------------------------------------------------------------------------------------------------

int CL_CanUseHTTPDownload(void)
{
	if ( sv_downloadurl.GetString()[0] )
	{
		const char *serverMapName = va( "%s:%s", sv_downloadurl.GetString(), cl.m_szLevelFileName );
		return !s_DownloadManager.HasMapBeenDownloadedFromServer( serverMapName );
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------------------

void CL_MarkMapAsUsingHTTPDownload(void)
{
	const char *serverMapName = va( "%s:%s", sv_downloadurl.GetString(), cl.m_szLevelFileName );
	s_DownloadManager.MarkMapAsDownloadedFromServer( serverMapName );
}

//--------------------------------------------------------------------------------------------------------------

bool CL_IsGamePathValidAndSafeForDownload( const char *pGamePath )
{
	if ( !CNetChan::IsValidFileForTransfer( pGamePath ) )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------

class CDownloadSystem : public IDownloadSystem
{
public:
	virtual DWORD CreateDownloadThread( RequestContext_t *pContext )
	{
		DWORD nThreadID;
		VCRHook_CreateThread(NULL, 0,
#ifdef POSIX
		 	(void*)
#endif
		 	DownloadThread, pContext, 0, (unsigned long int *)&nThreadID );

		ThreadDetach( ( ThreadHandle_t )nThreadID );
		return nThreadID;
	}
};

//--------------------------------------------------------------------------------------------------------------

static CDownloadSystem s_DownloadSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDownloadSystem, IDownloadSystem, INTERFACEVERSION_DOWNLOADSYSTEM, s_DownloadSystem );

//--------------------------------------------------------------------------------------------------------------

