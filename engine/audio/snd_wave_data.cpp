//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "audio_pch.h"
#include "datacache/idatacache.h"
#include "utllinkedlist.h"
#include "utldict.h"
#include "filesystem/IQueuedLoader.h"
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;
extern IFileSystem *g_pFileSystem;
extern IDataCache *g_pDataCache;
extern double realtime;

// console streaming buffer implementation, appropriate for high latency and low memory
// shift this many buffers through the wave
#define STREAM_BUFFER_COUNT		2
// duration of audio samples per buffer, 200ms is 2x the worst frame rate (10Hz)
// the engine then has at least 400ms to deliver a new buffer or pop (assuming 2 buffers)
#define STREAM_BUFFER_TIME		0.200f
// force a single buffer when streaming waves smaller than this
#define STREAM_BUFFER_DATASIZE	XBOX_DVD_ECC_SIZE

// PC single buffering implementation
// UNDONE: Allocate this in cache instead?
#define SINGLE_BUFFER_SIZE 16384

// Force a small cache for debugging cache issues.
// #define FORCE_SMALL_MEMORY_CACHE_SIZE	( 6 * 1024 * 1024 )

#define DEFAULT_WAV_MEMORY_CACHE ( 16 * 1024 * 1024 )
#define DEFAULT_XBOX_WAV_MEMORY_CACHE ( 16 * 1024 * 1024 )
#define TF_XBOX_WAV_MEMORY_CACHE ( 24 * 1024 * 1024 ) // Team Fortress uses a larger cache

// Dev builds will be missing soundcaches and hitch sometimes, we only care if its being properly launched from steam where sound caches should be complete.
ConVar snd_async_spew_blocking( "snd_async_spew_blocking", "1", 0, "Spew message to console any time async sound loading blocks on file i/o. ( 0=Off, 1=With -steam only, 2=Always" );
ConVar snd_async_spew( "snd_async_spew", "0", 0, "Spew all async sound reads, including success" );
ConVar snd_async_fullyasync( "snd_async_fullyasync", "0", 0, "All playback is fully async (sound doesn't play until data arrives)." );
ConVar snd_async_stream_spew( "snd_async_stream_spew", "0", 0, "Spew streaming info ( 0=Off, 1=streams, 2=buffers" );

static bool SndAsyncSpewBlocking()
{
	int pref = snd_async_spew_blocking.GetInt();
	return ( pref >= 2 ) || ( pref == 1 && CommandLine()->FindParm( "-steam" ) != 0 );
}

#define SndAlignReads() 1

void MaybeReportMissingWav( char const *wav );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct asyncwaveparams_t
{
	asyncwaveparams_t() : bPrefetch( false ), bCanBeQueued( false ) {}

	FileNameHandle_t	hFilename;	// handle to sound item name (i.e. not with sound\ prefix)
	int					datasize;
	int					seekpos;
	int					alignment;
	bool				bPrefetch;
	bool				bCanBeQueued;
};

//-----------------------------------------------------------------------------
// Purpose: Builds a cache of the data bytes for a specific .wav file
//-----------------------------------------------------------------------------
class CAsyncWaveData
{
public:
	explicit CAsyncWaveData();

	// APIS required by CManagedDataCacheClient
	void DestroyResource();
	CAsyncWaveData *GetData();
	unsigned int Size();

	static void AsyncCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err );
	static void QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError );
	static CAsyncWaveData *CreateResource( const asyncwaveparams_t &params );
	static unsigned int EstimatedSize( const asyncwaveparams_t &params );

	void OnAsyncCompleted( const FileAsyncRequest_t* asyncFilePtr, int numReadBytes, FSAsyncStatus_t err );
	bool BlockingCopyData( void *destbuffer, int destbufsize, int startoffset, int count );
	bool BlockingGetDataPointer( void **ppData );
	void SetAsyncPriority( int priority );
	void StartAsyncLoading( const asyncwaveparams_t& params );
	bool GetPostProcessed();
	void SetPostProcessed( bool proc );

	bool IsCurrentlyLoading();
	char const *GetFileName();

	// Data
public:
	int					m_nDataSize;		// bytes requested
	int					m_nReadSize;		// bytes actually read
	void				*m_pvData;			// target buffer
	byte				*m_pAlloc;			// memory of buffer (base may not match)
	FileAsyncRequest_t	m_async;
	FSAsyncControl_t	m_hAsyncControl;
	float				m_start;			// time at request invocation
	float				m_arrival;			// time at data arrival
	FileNameHandle_t	m_hFileNameHandle;
	int					m_nBufferBytes;		// size of any pre-allocated target buffer
	BufferHandle_t		m_hBuffer;			// used to dequeue the buffer after lru
	unsigned int		m_bLoaded : 1;
	unsigned int		m_bMissing : 1;
	unsigned int		m_bPostProcessed : 1;
};

//-----------------------------------------------------------------------------
// Purpose: C'tor
//-----------------------------------------------------------------------------
CAsyncWaveData::CAsyncWaveData() :
	m_nDataSize( 0 ),
	m_nReadSize( 0 ),
	m_pvData( 0 ),
	m_pAlloc( 0 ),
	m_hBuffer( INVALID_BUFFER_HANDLE ),
	m_nBufferBytes( 0 ),
	m_hAsyncControl( NULL ),
	m_bLoaded( false ),
	m_bMissing( false ),
	m_start( 0.0 ),
	m_arrival( 0.0 ),
	m_bPostProcessed( false ),
	m_hFileNameHandle( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: // APIS required by CDataLRU
//-----------------------------------------------------------------------------
void CAsyncWaveData::DestroyResource()
{
	if ( IsPC() )
	{
		if ( m_hAsyncControl )
		{
			if ( !m_bLoaded && !m_bMissing )
			{
				// NOTE:  We CANNOT call AsyncAbort since if the file is actually being read we'll end 
				//  up still getting a callback, but our this ptr (deleted below) will be feeefeee and we'll trash the heap 
				//  pretty bad.  So we call AsyncFinish, which will do a blocking read and will definitely succeed	
				// Block until we are finished
				g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			}
			
			g_pFileSystem->AsyncRelease( m_hAsyncControl );
			m_hAsyncControl = NULL;
		}
	}

	if ( IsX360() )
	{
		if ( m_hAsyncControl )
		{
			if ( !m_bLoaded && !m_bMissing )
			{
				// force an abort
				int errStatus = g_pFileSystem->AsyncAbort( m_hAsyncControl );
				if ( errStatus != FSASYNC_ERR_UNKNOWNID )
				{
					// must wait for abort to finish before deallocating data
					g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
				}
			}
			g_pFileSystem->AsyncRelease( m_hAsyncControl );
			m_hAsyncControl = NULL;
		}
		if ( m_hBuffer != INVALID_BUFFER_HANDLE )
		{
			// hint the manager that this tracked buffer is invalid
			wavedatacache->MarkBufferDiscarded( m_hBuffer );
		}
	}

	// delete buffers
	if ( IsPC() || !IsX360() )
	{
		g_pFileSystem->FreeOptimalReadBuffer( m_pAlloc );
	}
	else
	{
		delete [] m_pAlloc;
	}

	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CAsyncWaveData::GetFileName()
{
	static char sz[MAX_PATH];

	if ( m_hFileNameHandle )	
	{
		if ( g_pFileSystem->String( m_hFileNameHandle, sz, sizeof( sz ) ) )
		{
			return sz;
		}
	}
	
	Assert( 0 );
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CAsyncWaveData
//-----------------------------------------------------------------------------
CAsyncWaveData *CAsyncWaveData::GetData()
{ 
	return this; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CAsyncWaveData::Size()
{ 
	int size = sizeof( *this );
	
	if ( IsPC() )
	{
		size += m_nDataSize;
	}

	if ( IsX360() )
	{
		// the data size can shrink during streaming near end of file
		// need the real contant size of this object's allocations
		size += m_nBufferBytes;
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Static method for CDataLRU
// Input  : &params - 
// Output : CAsyncWaveData
//-----------------------------------------------------------------------------
CAsyncWaveData *CAsyncWaveData::CreateResource( const asyncwaveparams_t &params )
{
	CAsyncWaveData *pData = new CAsyncWaveData;
	Assert( pData );
	if ( pData )
	{
		if ( IsX360() )
		{
			// create buffer now for re-use during streaming process
			pData->m_nBufferBytes = AlignValue( params.datasize, params.alignment );
			pData->m_pAlloc = new byte[pData->m_nBufferBytes];
			pData->m_pvData = pData->m_pAlloc;
		}
		pData->StartAsyncLoading( params );
	}

	return pData;
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : &params - 
// Output : static unsigned int
//-----------------------------------------------------------------------------
unsigned int CAsyncWaveData::EstimatedSize( const asyncwaveparams_t &params )
{
	int size = sizeof( CAsyncWaveData );

	if ( IsPC() )
	{
		size += params.datasize;
	}
	if ( IsX360() )
	{
		// the expected size of this object's allocations
		size += AlignValue( params.datasize, params.alignment );
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Static method, called by thread, don't call anything non-threadsafe from handler!!!
// Input  : asyncFilePtr - 
//			numReadBytes - 
//			err - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::AsyncCallback(const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t err )
{
	CAsyncWaveData *pObject = reinterpret_cast< CAsyncWaveData * >( asyncRequest.pContext );
	Assert( pObject );
	if ( pObject )
	{
		pObject->OnAsyncCompleted( &asyncRequest, numReadBytes, err );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method, called by thread, don't call anything non-threadsafe from handler!!!
//-----------------------------------------------------------------------------
void CAsyncWaveData::QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	CAsyncWaveData *pObject = reinterpret_cast< CAsyncWaveData * >( pContext );
	Assert( pObject );

	pObject->OnAsyncCompleted( NULL, nSize, loaderError == LOADERERROR_NONE ? FSASYNC_OK : FSASYNC_ERR_FILEOPEN );
}

//-----------------------------------------------------------------------------
// Purpose: NOTE: THIS IS CALLED FROM A THREAD SO YOU CAN'T CALL INTO ANYTHING NON-THREADSAFE
//  such as CUtlSymbolTable/CUtlDict (many of the CUtl* are non-thread safe)!!!
// Input  : asyncFilePtr - 
//			numReadBytes - 
//			err - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::OnAsyncCompleted( const FileAsyncRequest_t *asyncFilePtr, int numReadBytes, FSAsyncStatus_t err )
{
	if ( IsPC() )
	{
		// Take hold of pointer (we can just use delete[] across .dlls because we are using a shared memory allocator...)
		if ( err == FSASYNC_OK || err == FSASYNC_ERR_READING )
		{
			m_arrival = ( float )Plat_FloatTime();

			// Take over ptr
			m_pAlloc = ( byte * )asyncFilePtr->pData;
			if ( SndAlignReads() )
			{
				m_async.nOffset = ( m_async.nBytes - m_nDataSize );
				m_async.nBytes -= m_async.nOffset;
				m_pvData = ((byte *)m_pAlloc) + m_async.nOffset;
				m_nReadSize	= numReadBytes - m_async.nOffset;
			}
			else
			{
				m_pvData = m_pAlloc;
				m_nReadSize = numReadBytes;
			}

			// Needs to be post-processed
			m_bPostProcessed = false;

			// Finished loading
			m_bLoaded = true;
		}
		else if ( err == FSASYNC_ERR_FILEOPEN )
		{
			// SEE NOTE IN FUNCTION COMMENT ABOVE!!!
			// Tracker 22905, et al.
			// Because this api gets called from the other thread, don't spew warning here as it can
			//  cause a crash in searching CUtlSymbolTables since they use a global var for a LessFunc context!!!
			m_bMissing = true;
		}
	}

	if ( IsX360() )
	{
		m_arrival = (float)Plat_FloatTime();

		// possibly reading more than intended due to alignment restriction
		m_nReadSize = numReadBytes;
		if ( m_nReadSize > m_nDataSize )
		{
			// clamp to expected, extra data is unreliable
			m_nReadSize = m_nDataSize;
		}

		if ( err != FSASYNC_OK )
		{
			// track as any error
			m_bMissing = true;
		}

		if ( err != FSASYNC_ERR_FILEOPEN )
		{
			// some data got loaded
			m_bLoaded = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *destbuffer - 
//			destbufsize - 
//			startoffset - 
//			count - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::BlockingCopyData( void *destbuffer, int destbufsize, int startoffset, int count )
{
	if ( !m_bLoaded )
	{
		Assert( m_hAsyncControl );
		// Force it to finish
		// It could finish between the above line and here, but the AsyncFinish call will just have a bogus id, not a big deal
		if ( SndAsyncSpewBlocking() )
		{
			// Force it to finish
			float st = ( float )Plat_FloatTime();
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			float ed = ( float )Plat_FloatTime();
			Warning( "%f BCD:  Async I/O Force %s (%8.2f msec / %8.2f msec total)\n", realtime, GetFileName(), 1000.0f * (float)( ed - st ), 1000.0f * (float)( m_arrival - m_start ) );
		}
		else
		{
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
		}
	}

	// notify on any error
	if ( m_bMissing )
	{
		// Only warn once
		m_bMissing = false;

		char fn[MAX_PATH];
		if ( g_pFileSystem->String( m_hFileNameHandle, fn, sizeof( fn ) ) )
		{
			MaybeReportMissingWav( fn );
		}
	}

	if ( !m_bLoaded )
	{
		return false;
	}
	else if ( m_arrival != 0 && snd_async_spew.GetBool() )
	{
		DevMsg( "%f Async I/O Read successful %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( m_arrival - m_start ) );
		m_arrival = 0;
	}

	// clamp requested to available
	if ( count > m_nReadSize )
	{
		count = m_nReadSize - startoffset;
	}

	if ( count < 0 )
	{
		return false;
	}

	// Copy data from stream buffer
	Q_memcpy( destbuffer, (char *)m_pvData + ( startoffset - m_async.nOffset ), count );

	g_pFileSystem->AsyncRelease( m_hAsyncControl );
	m_hAsyncControl = NULL;
	return true;
}


bool CAsyncWaveData::IsCurrentlyLoading()
{
	if ( m_bLoaded )
		return true;
	FSAsyncStatus_t status = g_pFileSystem->AsyncStatus( m_hAsyncControl );
	if ( status == FSASYNC_STATUS_INPROGRESS || status == FSASYNC_OK )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::BlockingGetDataPointer( void **ppData )
{
	Assert( ppData );
	if ( !m_bLoaded )
	{
		// Force it to finish
		// It could finish between the above line and here, but the AsyncFinish call will just have a bogus id, not a big deal
		if ( SndAsyncSpewBlocking() )
		{
			float st = ( float )Plat_FloatTime();
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
			float ed = ( float )Plat_FloatTime();
			Warning( "%f BlockingGetDataPointer:  Async I/O Force %s (%8.2f msec / %8.2f msec total )\n", realtime, GetFileName(), 1000.0f * (float)( ed - st ), 1000.0f * (float)( m_arrival - m_start ) );
		}
		else
		{
			g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
		}
	}

	// notify on any error
	if ( m_bMissing )
	{
		// Only warn once
		m_bMissing = false;

		char fn[MAX_PATH];
		if ( g_pFileSystem->String( m_hFileNameHandle, fn, sizeof( fn ) ) )
		{
			MaybeReportMissingWav( fn );
		}
	}

	if ( !m_bLoaded )
	{
		return false;
	}
	else if ( m_arrival != 0 && snd_async_spew.GetBool() )
	{
		DevMsg( "%f Async I/O Read successful %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( m_arrival - m_start ) );
		m_arrival = 0;
	}

	*ppData = m_pvData;

	g_pFileSystem->AsyncRelease( m_hAsyncControl );
	m_hAsyncControl = NULL;

	return true;
}

void CAsyncWaveData::SetAsyncPriority( int priority )
{
	if ( m_async.priority != priority )
	{
		m_async.priority = priority;
		g_pFileSystem->AsyncSetPriority( m_hAsyncControl, m_async.priority );
		if ( snd_async_spew.GetBool() )
		{
			DevMsg( "%f Async I/O Bumped priority for %s (%8.2f msec)\n", realtime, GetFileName(), 1000.0f * (float)( Plat_FloatTime() - m_start ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::StartAsyncLoading( const asyncwaveparams_t& params )
{
	Assert( IsX360() || ( IsPC() && !m_bLoaded ) );

	// expected to be relative to the sound\ dir
	m_hFileNameHandle = params.hFilename;

	// build the real filename
	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "sound\\%s", GetFileName() );

	int nPriority = 1;
	if ( params.bPrefetch )
	{
		// lower the priority of prefetched sounds, so they don't block immediate sounds from being loaded
		nPriority = 0;
	}

	if ( !IsX360() )
	{
		m_async.pData = NULL;
		if ( SndAlignReads() )
		{
			m_async.nOffset = 0;
			m_async.nBytes = params.seekpos + params.datasize;
		}
		else
		{
			m_async.nOffset = params.seekpos;
			m_async.nBytes = params.datasize;
		}
	}
	else
	{
		Assert( params.datasize > 0 );

		// using explicit allocated buffer on xbox
		m_async.pData = m_pvData;
		m_async.nOffset = params.seekpos;
		m_async.nBytes = AlignValue( params.datasize, params.alignment ); 
	}

	m_async.pfnCallback	= AsyncCallback;	// optional completion callback
	m_async.pContext = (void *)this;		// caller's unique context
	m_async.priority = nPriority;			// inter list priority, 0=lowest
	m_async.flags = IsX360() ? 0 : FSASYNC_FLAGS_ALLOCNOFREE;
	m_async.pszPathID = "GAME";

	m_bLoaded = false;
	m_bMissing = false;
	m_nDataSize = params.datasize;
	m_start = (float)Plat_FloatTime();
	m_arrival = 0;
	m_nReadSize = 0;
	m_bPostProcessed = false;

	// The async layer creates a copy of this string, ok to send a local reference
	m_async.pszFilename	= szFilename;

	char szFullName[MAX_PATH];
	if ( IsX360() && ( g_pFileSystem->GetDVDMode() == DVDMODE_STRICT ) )
	{
		// all audio is expected be in zips
		// resolve to absolute name now, where path can be filtered to just the zips (fast find, no real i/o)
		// otherwise the dvd will do a costly seek for each zip miss due to search path fall through
		PathTypeQuery_t pathType;
		if ( !g_pFileSystem->RelativePathToFullPath( m_async.pszFilename, m_async.pszPathID, szFullName, sizeof( szFullName ), FILTER_CULLNONPACK, &pathType ) )
		{
			// not found, do callback now to handle error
			m_async.pfnCallback( m_async, 0, FSASYNC_ERR_FILEOPEN );
			return;
		}
		m_async.pszFilename	= szFullName;
	}

	if ( IsX360() && params.bCanBeQueued )
	{
		// queued loader takes over
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = m_async.pszFilename;
		loaderJob.m_pPathID = m_async.pszPathID;
		loaderJob.m_pCallback = QueuedLoaderCallback;
		loaderJob.m_pContext = (void *)this;
		loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
		loaderJob.m_pTargetData = m_async.pData;
		loaderJob.m_nBytesToRead = m_async.nBytes;
		loaderJob.m_nStartOffset = m_async.nOffset;
		g_pQueuedLoader->AddJob( &loaderJob );
		return;
	}

	MEM_ALLOC_CREDIT();
	
	// Commence async I/O
	Assert( !m_hAsyncControl );
	g_pFileSystem->AsyncRead( m_async, &m_hAsyncControl );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWaveData::GetPostProcessed()
{
	return m_bPostProcessed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : proc - 
//-----------------------------------------------------------------------------
void CAsyncWaveData::SetPostProcessed( bool proc )
{
	m_bPostProcessed = proc;
}

//-----------------------------------------------------------------------------
// Purpose: Implements a cache of .wav / .mp3 data based on filename
//-----------------------------------------------------------------------------
class CAsyncWavDataCache : public IAsyncWavDataCache, 
						   public CManagedDataCacheClient<CAsyncWaveData, asyncwaveparams_t>
{
public:
	CAsyncWavDataCache();
	~CAsyncWavDataCache() {}

	virtual bool			Init( unsigned int memSize );
	virtual void			Shutdown();

	// implementation that treats file as monolithic
	virtual memhandle_t		AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch = false );
	virtual void			PrefetchCache( char const *filename, int datasize, int startpos );
	virtual bool			CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	virtual bool			CopyDataIntoMemory( memhandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed );
	virtual void			SetPostProcessed( memhandle_t handle, bool proc );
	virtual void			Unload( memhandle_t handle );
	virtual bool			GetDataPointer( memhandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed );
	virtual bool			IsDataLoadCompleted( memhandle_t handle, bool *pIsValid );
	virtual void			RestartDataLoad( memhandle_t* handle, char const *filename, int datasize, int startpos );
	virtual bool			IsDataLoadInProgress( memhandle_t handle );

	// Xbox: alternate multi-buffer streaming implementation
	virtual StreamHandle_t	OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags );
	virtual void			CloseStreamedLoad( StreamHandle_t hStream );
	virtual int				CopyStreamedDataIntoMemory( StreamHandle_t hStream, void *pBuffer, int bufferSize, int copyStartPos, int bytesToCopy );
	virtual bool			IsStreamedDataReady( StreamHandle_t hStream );
	virtual void			MarkBufferDiscarded( BufferHandle_t hBuffer );
	virtual void			*GetStreamedDataPointer( StreamHandle_t hStream, bool bSync );

	virtual	void			Flush();
	virtual void			OnMixBegin();
	virtual void			OnMixEnd();

	void					QueueUnlock( const memhandle_t &handle );
	void					SpewMemoryUsage( int level );

	// Cache helpers
	bool					GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  );

private:
	void					Clear();

	struct CacheEntry_t
	{
		CacheEntry_t() :
			name( 0 ),
			handle( 0 )
		{
		}
		FileNameHandle_t	name;
		memhandle_t			handle;
	};

	// tags the signature of a buffer inside a rb tree for faster than linear find
	struct BufferEntry_t
	{
		FileNameHandle_t	m_hName;
		memhandle_t			m_hWaveData;
		int					m_StartPos;
		bool				m_bCanBeShared;
	};

	static bool BufferHandleLessFunc( const BufferEntry_t& lhs, const BufferEntry_t& rhs )
	{
		if ( lhs.m_hName != rhs.m_hName )
		{
			return lhs.m_hName < rhs.m_hName;
		}

		if ( lhs.m_StartPos != rhs.m_StartPos )
		{
			return lhs.m_StartPos < rhs.m_StartPos;
		}

		return lhs.m_bCanBeShared < rhs.m_bCanBeShared;
	}

	CUtlRBTree< BufferEntry_t, BufferHandle_t >	m_BufferList;

	// encapsulates (n) buffers for a streamed wave object
	struct StreamedEntry_t
	{
		FileNameHandle_t	m_hName;
		memhandle_t			m_hWaveData[STREAM_BUFFER_COUNT];
		int					m_Front;			// buffer index, forever incrementing
		int					m_NextStartPos;		// predicted offset if mixing linearly
		int					m_DataSize;			// length of the data set in bytes
		int					m_DataStart;		// file offset where data set starts
		int					m_LoopStart;		// offset in data set where loop starts
		int					m_BufferSize;		// size of the buffer in bytes
		int					m_numBuffers;		// number of buffers (1 or 2) to march through
		int					m_SectorSize;		// size of sector on stream device
		bool				m_bSinglePlay;		// hint to keep same buffers
	};
	CUtlLinkedList< StreamedEntry_t, StreamHandle_t >	m_StreamedHandles;

	static bool CacheHandleLessFunc( const CacheEntry_t& lhs, const CacheEntry_t& rhs )
	{
		return lhs.name < rhs.name;
	}
	CUtlRBTree< CacheEntry_t, int >	m_CacheHandles;

	memhandle_t				FindOrCreateBuffer( asyncwaveparams_t &params, bool bFind );		
	bool					m_bInitialized;
	bool					m_bQueueCacheUnlocks;
	CUtlVector<memhandle_t> m_unlockQueue;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAsyncWavDataCache::CAsyncWavDataCache() :  
	m_CacheHandles( 0, 0, CacheHandleLessFunc ),
	m_BufferList( 0, 0, BufferHandleLessFunc ),
	m_bInitialized( false ),
	m_bQueueCacheUnlocks( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::Init( unsigned int memSize )
{
	if ( m_bInitialized )
		return true;
	
	if ( IsX360() )
	{			
		const char *pGame = engineClient->GetGameDirectory();
		if ( !Q_stricmp( Q_UnqualifiedFileName( pGame ), "tf" ) )
		{
			memSize = TF_XBOX_WAV_MEMORY_CACHE;
		}
		else
		{
			memSize = DEFAULT_XBOX_WAV_MEMORY_CACHE;
		}
	}
	else
	{
		if ( memSize < DEFAULT_WAV_MEMORY_CACHE )
		{
			memSize = DEFAULT_WAV_MEMORY_CACHE;
		}
	}

#if FORCE_SMALL_MEMORY_CACHE_SIZE
	memSize = FORCE_SMALL_MEMORY_CACHE_SIZE;
	Msg( "WARNING CAsyncWavDataCache::Init() forcing small memory cache size: %u\n", memSize );
#endif

	CCacheClientBaseClass::Init( g_pDataCache, "WaveData", memSize );

	m_bInitialized = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Shutdown()
{
	if ( !m_bInitialized )
	{
		return;
	}

	Clear();

	CCacheClientBaseClass::Shutdown();

	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates initial cache object if it doesn't already exist, starts async loading the actual data
//  in any case.
// Input  : *filename - 
//			datasize - 
//			startpos - 
// Output : memhandle_t
//-----------------------------------------------------------------------------
memhandle_t CAsyncWavDataCache::AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch )
{
	VPROF( "CAsyncWavDataCache::AsyncLoadCache" );

	FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

	CacheEntry_t search;
	search.name = fnh;
	search.handle = 0;

	// find or create the handle
	int idx = m_CacheHandles.Find( search );
	if ( idx == m_CacheHandles.InvalidIndex() )
	{
		idx = m_CacheHandles.Insert( search );
		Assert( idx != m_CacheHandles.InvalidIndex() );
	}

	CacheEntry_t &entry = m_CacheHandles[idx];

	// Try and pull it into cache
	CAsyncWaveData *data = CacheGet( entry.handle );
	if ( !data )
	{
		// Try and reload it
		asyncwaveparams_t	params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;
		params.bPrefetch = bIsPrefetch;
		entry.handle = CacheCreate( params );
	}

	return entry.handle;
}


//-----------------------------------------------------------------------------
// Purpose: Reclaim a buffer. A reclaimed resident buffer is ready for play.
//-----------------------------------------------------------------------------
memhandle_t CAsyncWavDataCache::FindOrCreateBuffer( asyncwaveparams_t &params, bool bFind )
{
	CAsyncWaveData *pWaveData;
	BufferEntry_t	search;
	BufferHandle_t	hBuffer;

	search.m_hName = params.hFilename;
	search.m_StartPos = params.seekpos;
	search.m_bCanBeShared = bFind;
	search.m_hWaveData = 0;

	if ( bFind )
	{
		// look for an existing buffer that matches exactly (same file, offset, and share)
		int iBuffer = m_BufferList.Find( search );
		if ( iBuffer != m_BufferList.InvalidIndex() )
		{
			// found
			search.m_hWaveData = m_BufferList[iBuffer].m_hWaveData;
			if ( snd_async_stream_spew.GetInt() >= 2 )
			{
				char tempBuff[MAX_PATH];
				g_pFileSystem->String( params.hFilename, tempBuff, sizeof( tempBuff ) );
				Msg( "Found Buffer: %s, offset: %d\n", tempBuff, params.seekpos );
			}
		}
	}
	
	// each resource buffer stays locked (valid) while in use
	// a buffering stream is not subject to lru and can rely on it's buffers
	// a buffering stream may obsolete it's buffers by reducing the lock count, allowing for lru
	pWaveData = CacheLock( search.m_hWaveData );
	if ( !pWaveData )
	{
		// not in cache, create and lock
		// not found, create buffer and fill with data
		search.m_hWaveData = CacheCreate( params, DCAF_LOCK );

		// add the buffer to our managed list
		hBuffer = m_BufferList.Insert( search );
		Assert( hBuffer != m_BufferList.InvalidIndex() );

		// store the handle into our managed list
		// used during a lru discard as a means to keep the list in-sync
		pWaveData = CacheGet( search.m_hWaveData );
		pWaveData->m_hBuffer = hBuffer;
	}
	else
	{
		// still in cache
		// same as requesting it and having it arrive instantly
		pWaveData->m_start = pWaveData->m_arrival = (float)Plat_FloatTime();
	}

	return search.m_hWaveData;
}


//-----------------------------------------------------------------------------
// Purpose: Load data asynchronously via multi-buffers, returns specialized handle
//-----------------------------------------------------------------------------
StreamHandle_t CAsyncWavDataCache::OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags )
{
	VPROF( "CAsyncWavDataCache::OpenStreamedLoad" );

	StreamedEntry_t			streamedEntry;
	StreamHandle_t			hStream;
	asyncwaveparams_t		params;
	int						i;

	Assert( numBuffers > 0 && numBuffers <= STREAM_BUFFER_COUNT );

	// queued load mandates one buffer
	Assert( !( flags & STREAMED_QUEUEDLOAD ) || numBuffers == 1 );

	streamedEntry.m_hName = g_pFileSystem->FindOrAddFileName( pFileName );
	streamedEntry.m_Front = 0;
	streamedEntry.m_DataSize = dataSize;
	streamedEntry.m_DataStart = dataStart;
	streamedEntry.m_NextStartPos = startPos + numBuffers * bufferSize;
	streamedEntry.m_LoopStart = loopPos;
	streamedEntry.m_BufferSize = bufferSize;
	streamedEntry.m_numBuffers = numBuffers;
	streamedEntry.m_bSinglePlay = ( flags & STREAMED_SINGLEPLAY ) != 0;
	streamedEntry.m_SectorSize = ( IsX360() && ( flags & STREAMED_FROMDVD ) ) ? XBOX_DVD_SECTORSIZE : 1;

	// single play streams expect to uniquely own and thus recycle their buffers though the data
	// single play streams are guaranteed that their buffers are private and cannot be shared
	// a non-single play stream wants persisting buffers and attempts to reclaim a matching buffer
	bool bFindBuffer = ( streamedEntry.m_bSinglePlay == false );

	// initial load populates buffers
	// mixing starts after front buffer viable
	// buffer rotation occurs after front buffer consumed
	// there should be no blocking
	params.hFilename = streamedEntry.m_hName;
	params.datasize = bufferSize;
	params.alignment = streamedEntry.m_SectorSize;
	params.bCanBeQueued = ( flags & STREAMED_QUEUEDLOAD ) != 0;
	for ( i=0; i<numBuffers; ++i )
	{
		params.seekpos = dataStart + startPos + i * bufferSize;
		streamedEntry.m_hWaveData[i] = FindOrCreateBuffer( params, bFindBuffer );
	}

	// get a unique handle for each stream request
	hStream = m_StreamedHandles.AddToTail( streamedEntry );
	Assert( hStream != m_StreamedHandles.InvalidIndex() );

	return hStream;
}


//-----------------------------------------------------------------------------
// Purpose: Cleanup a streamed load's resources.
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::CloseStreamedLoad( StreamHandle_t hStream )
{
	VPROF( "CAsyncWavDataCache::CloseStreamedLoad" );

	if ( hStream == INVALID_STREAM_HANDLE )
	{
		return;
	}

	int	lockCount;
	StreamedEntry_t	&streamedEntry = m_StreamedHandles[hStream];
	for ( int i=0; i<streamedEntry.m_numBuffers; ++i )
	{
		// multiple streams could be using the same buffer, keeping the lock count nonzero
		lockCount = GetCacheSection()->GetLockCount( streamedEntry.m_hWaveData[i] );
		Assert( lockCount >= 1 );
		if ( lockCount > 0 )
		{
			lockCount = CacheUnlock( streamedEntry.m_hWaveData[i] );
		}

		if ( streamedEntry.m_bSinglePlay )
		{
			// a buffering single play stream has no reason to reuse its own buffers and destroys them
			Assert( lockCount == 0 );
			CacheRemove( streamedEntry.m_hWaveData[i] );
		}
	}

	m_StreamedHandles.Remove( hStream );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			datasize - 
//			startpos - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::PrefetchCache( char const *filename, int datasize, int startpos )
{
	// Just do an async load, but don't get cache handle
	AsyncLoadCache( filename, datasize, startpos, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			datasize - 
//			startpos - 
//			*buffer - 
//			bufsize - 
//			copystartpos - 
//			bytestocopy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::CopyDataIntoMemory" );

	bool bret = false;

	// Add to caching system
	AsyncLoadCache( filename, datasize, startpos );

	FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

	CacheEntry_t search;
	search.name = fnh;
	search.handle = 0;

	// Now look it up, it should be in the system
	int idx = m_CacheHandles.Find( search );
	if ( idx == m_CacheHandles.InvalidIndex() )
	{
		Assert( 0 );
		return bret;
	}

	// Now see if the handle has been paged out...
	return CopyDataIntoMemory( m_CacheHandles[ idx ].handle, filename, datasize, startpos, buffer, bufsize, copystartpos, bytestocopy, pbPostProcessed );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
//			*buffer - 
//			bufsize - 
//			copystartpos - 
//			bytestocopy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::CopyDataIntoMemory( memhandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::CopyDataIntoMemory" );

	*pbPostProcessed = false;

	bool bret = false;

	CAsyncWaveData *data = CacheLock( handle );
	if ( !data )
	{
		FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

		CacheEntry_t search;
		search.name = fnh;
		search.handle = 0;

		// Now look it up, it should be in the system
		int idx = m_CacheHandles.Find( search );
		if ( idx == m_CacheHandles.InvalidIndex() )
		{
			Assert( 0 );
			return false;
		}

		// Try and reload it
		asyncwaveparams_t params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;

		handle = m_CacheHandles[ idx ].handle = CacheCreate( params );
		data = CacheLock( handle );
		if ( !data )
		{
			return bret;
		}
	}

	// Cache entry exists, but if filesize == 0 then the file itself wasn't on disk...
	if ( data->m_nDataSize != 0 )
	{
		bret = data->BlockingCopyData( buffer, bufsize, copystartpos, bytestocopy );
	}

	*pbPostProcessed = data->GetPostProcessed();

	// Release lock
	CacheUnlock( handle );
	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: Copy from streaming buffers into target memory, never blocks.
//-----------------------------------------------------------------------------
int CAsyncWavDataCache::CopyStreamedDataIntoMemory( int hStream, void *pBuffer, int bufferSize, int copyStartPos, int bytesToCopy )
{
	VPROF( "CAsyncWavDataCache::CopyStreamedDataIntoMemory" );

	int					actualCopied;
	int					count;
	int					i;
	int					which;
	CAsyncWaveData		*pWaveData[STREAM_BUFFER_COUNT];
	CAsyncWaveData		*pFront;
	asyncwaveparams_t	params;
	int					nextStartPos;
	int					bufferPos;
	bool				bEndOfFile;
	int					index;
	bool				bWaiting;
	bool				bCompleted;
	StreamedEntry_t		&streamedEntry = m_StreamedHandles[hStream];
	
	if ( copyStartPos >= streamedEntry.m_DataStart + streamedEntry.m_DataSize )
	{
		// at or past end of file
		return 0;
	}

	for ( i=0; i<streamedEntry.m_numBuffers; ++i )
	{
		pWaveData[i] = CacheGetNoTouch( streamedEntry.m_hWaveData[i] );
		Assert( pWaveData[i] );
	}

	// drive the buffering
	index = streamedEntry.m_Front;
	bEndOfFile = 0;
	actualCopied = 0;
	bWaiting = false;
	while ( 1 )
	{
		// try to satisfy from the front
		pFront = pWaveData[index % streamedEntry.m_numBuffers];
		bufferPos = copyStartPos - pFront->m_async.nOffset;

		// cache atomic async completion signal off to avoid coherency issues
		bCompleted = pFront->m_bLoaded || pFront->m_bMissing;

		if ( snd_async_stream_spew.GetInt() >= 1 )
		{
			// interval is the audio block clock rate, the block must be available within this interval
			// a faster audio rate or smaller block size implies a smaller interval
			// latency is the actual block delivery time
			// latency must not exceed the delivery interval or stariving occurs and audio pops
			float nowTime = Plat_FloatTime();
			int interval = (int)(1000.0f*(nowTime-pFront->m_start));
			int latency;
			if ( bCompleted && pFront->m_bLoaded )
			{
				latency = (int)(1000.0f*(pFront->m_arrival-pFront->m_start));
			}
			else
			{
				// buffer has not arrived yet
				latency = -1;
			}
			DevMsg( "Stream:%2d interval:%5dms latency:%5dms offset:%d length:%d (%s)\n", hStream, interval, latency, pFront->m_async.nOffset, pFront->m_nReadSize, pFront->GetFileName() );
		}

		if ( bCompleted && pFront->m_hAsyncControl && ( pFront->m_bLoaded || pFront->m_bMissing) )
		{
			g_pFileSystem->AsyncRelease( pFront->m_hAsyncControl );
			pFront->m_hAsyncControl = NULL;
		}

		if ( bCompleted && pFront->m_bLoaded )
		{
			if ( bufferPos >= 0 && bufferPos < pFront->m_nReadSize )
			{
				count = bytesToCopy;
				if ( bufferPos + bytesToCopy > pFront->m_nReadSize )
				{
					// clamp requested to actual available
					count = pFront->m_nReadSize - bufferPos;
				}
				if ( bufferPos + count > bufferSize )
				{
					// clamp requested to caller's buffer dimension
					count = bufferSize - bufferPos;
				}

				Q_memcpy( pBuffer, (char *)pFront->m_pvData + bufferPos, count );
		
				// advance past consumed bytes
				actualCopied += count;
				copyStartPos += count;
				bufferPos += count;
			}
		}
		else if ( bCompleted && pFront->m_bMissing )
		{
			// notify on any error
			MaybeReportMissingWav( pFront->GetFileName() );
			break;
		}
		else
		{
			// data not available
			bWaiting = true;
			break;
		}

		// cycle past obsolete or consumed buffers
		if ( bufferPos < 0 || bufferPos >= pFront->m_nReadSize )
		{
			// move to next buffer
			index++;
			if ( index - streamedEntry.m_Front >= streamedEntry.m_numBuffers )
			{
				// out of buffers
				break;
			}
		}

		if ( actualCopied == bytesToCopy )
		{
			// satisfied request
			break;
		}
	}

	if ( streamedEntry.m_numBuffers > 1 )
	{
		// restart consumed buffers
		while ( streamedEntry.m_Front < index )
		{
			if ( !actualCopied && !bWaiting )
			{
				// couldn't return any data because the buffers aren't in the right location
				// oh no! caller must be skipping
				// due to latency the next buffer position has to start one full buffer ahead of the caller's desired read location
				// hopefully only 1 buffer will stutter
				nextStartPos = copyStartPos - streamedEntry.m_DataStart + streamedEntry.m_BufferSize;

				// advance past, ready for next possible iteration
				copyStartPos += streamedEntry.m_BufferSize;
			}
			else
			{
				// get the next forecasted read location
				nextStartPos = streamedEntry.m_NextStartPos;
			}

			if ( nextStartPos >= streamedEntry.m_DataSize )
			{
				// next buffer is at or past end of file 
				if ( streamedEntry.m_LoopStart >= 0 )
				{
					// wrap back around to loop position
					nextStartPos = streamedEntry.m_LoopStart;
				}
				else
				{
					// advance past consumed buffer
					streamedEntry.m_Front++;

					// start no further buffers
					break;
				}
			}

			// still valid data left to read
			// snap the buffer position to required alignment
			nextStartPos = streamedEntry.m_SectorSize * (nextStartPos/streamedEntry.m_SectorSize);

			// start loading back buffer at future location
			params.hFilename = streamedEntry.m_hName;
			params.seekpos = streamedEntry.m_DataStart + nextStartPos;
			params.datasize = streamedEntry.m_DataSize - nextStartPos;
			params.alignment = streamedEntry.m_SectorSize;
			if ( params.datasize > streamedEntry.m_BufferSize )
			{
				// clamp to buffer size
				params.datasize = streamedEntry.m_BufferSize;
			}

			// save next start position
			streamedEntry.m_NextStartPos = nextStartPos + params.datasize;

			which = streamedEntry.m_Front % streamedEntry.m_numBuffers;
			if ( streamedEntry.m_bSinglePlay )
			{
				// a single play wave has no reason to persist its buffers into the lru
				// reuse buffer and restart until finished
				pWaveData[which]->StartAsyncLoading( params );
			}
			else
			{
				// release obsolete buffer to lru management
				CacheUnlock( streamedEntry.m_hWaveData[which] );
				// reclaim or create/load the desired buffer
				streamedEntry.m_hWaveData[which] = FindOrCreateBuffer( params, true );
			}

			streamedEntry.m_Front++;
		}

		if ( bWaiting )
		{
			// oh no! data needed is not yet available in front buffer
			// caller requesting data faster than can be provided or caller skipped
			// can only return what has been copied thus far (could be 0)
			return actualCopied;
		}
	}

	return actualCopied;
}


//-----------------------------------------------------------------------------
// Purpose: Get the front buffer, optionally block.
// Intended for user of a single buffer stream.
//-----------------------------------------------------------------------------
void *CAsyncWavDataCache::GetStreamedDataPointer( StreamHandle_t hStream, bool bSync )
{
	void			*pData;
	CAsyncWaveData	*pFront;
	int				index;
	StreamedEntry_t &streamedEntry = m_StreamedHandles[hStream];

	index  = streamedEntry.m_Front % streamedEntry.m_numBuffers;
	pFront = CacheGetNoTouch( streamedEntry.m_hWaveData[index] );
	Assert( pFront );
	if ( !pFront )
	{
		// shouldn't happen
		return NULL;
	}

	if ( !pFront->m_bMissing && pFront->m_bLoaded )
	{
		return pFront->m_pvData;
	}

	if ( bSync && pFront->BlockingGetDataPointer( &pData ) )
	{
		return pData;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: The front buffer must be valid
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::IsStreamedDataReady( int hStream )
{
	VPROF( "CAsyncWavDataCache::IsStreamedDataReady" );

	if ( hStream == INVALID_STREAM_HANDLE )
	{
		return false;
	}

	StreamedEntry_t &streamedEntry = m_StreamedHandles[hStream];

	if ( streamedEntry.m_Front )
	{
		// already streaming, the buffers better be arriving as expected
		return true;
	}

	// only the first front buffer must be present
	CAsyncWaveData *pFront = CacheGetNoTouch( streamedEntry.m_hWaveData[0] );
	Assert( pFront );
	if ( !pFront )
	{
		// shouldn't happen
		// let the caller think data is ready, so stream can shutdown
		return true;
	}

	// regardless of any errors
	// errors handled during data fetch
	return pFront->m_bLoaded || pFront->m_bMissing;
}


//-----------------------------------------------------------------------------
// Purpose: Dequeue the buffer entry (backdoor for list management)
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::MarkBufferDiscarded( BufferHandle_t hBuffer )
{
	m_BufferList.RemoveAt( hBuffer );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			proc - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::SetPostProcessed( memhandle_t handle, bool proc )
{
	CAsyncWaveData *data = CacheGet( handle );
	if ( data )
	{
		data->SetPostProcessed( proc );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Unload( memhandle_t handle )
{
	// Don't actually unload, just mark it as stale
	if ( GetCacheSection() )
	{
		GetCacheSection()->Age( handle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
//			**pData - 
//			copystartpos - 
//			*pbPostProcessed - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::GetDataPointer( memhandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed )
{
	VPROF( "CAsyncWavDataCache::GetDataPointer" );

	Assert( pbPostProcessed );
	Assert( pData );

	*pbPostProcessed = false;

	bool bret = false;
	*pData = NULL;

	CAsyncWaveData *data = CacheLock( handle );
	if ( !data )
	{
		FileNameHandle_t fnh = g_pFileSystem->FindOrAddFileName( filename );

		CacheEntry_t search;
		search.name = fnh;
		search.handle = 0;

		int idx = m_CacheHandles.Find( search );
		if ( idx == m_CacheHandles.InvalidIndex() )
		{
			Assert( 0 );
			return bret;
		}

		// Try and reload it
		asyncwaveparams_t params;
		params.hFilename = fnh;
		params.datasize = datasize;
		params.seekpos = startpos;

		handle = m_CacheHandles[ idx ].handle = CacheCreate( params );
		data = CacheLock( handle );
		if ( !data )
		{
			return bret;
		}
	}

	// Cache entry exists, but if filesize == 0 then the file itself wasn't on disk...
	if ( data->m_nDataSize != 0 )
	{
		if ( datasize != data->m_nDataSize )
		{
			// We've had issues where we are called with datasize larger than what we read on disk.
			//  Ie: datasize is 277,180, data->m_nDataSize is 263,168
			// This can happen due to a corrupted audio cache, but it's more likely that somehow
			//  we wound up reading the cache data from one language and the file from another.
			DevMsg( "Cached datasize != sound datasize %d - %d.\n", datasize, data->m_nDataSize );
#ifdef STAGING_ONLY
			// Adding a STAGING_ONLY debugger break to try and help track this down. Hopefully we'll
			//  get this crash internally with full debug information instead of just minidump files.
			DebuggerBreak();
#endif
		}
		else if ( copystartpos < data->m_nDataSize )
		{
			if ( data->BlockingGetDataPointer( pData ) )
			{
				*pData = (char *)*pData + copystartpos;
				bret = true;
			}
		}
	}

	*pbPostProcessed = data->GetPostProcessed();

	// Release lock at the end of mixing
	QueueUnlock( handle );
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//			*filename - 
//			datasize - 
//			startpos - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::IsDataLoadCompleted( memhandle_t handle, bool *pIsValid )
{
	VPROF( "CAsyncWavDataCache::IsDataLoadCompleted" );

	CAsyncWaveData *data = CacheGet( handle );
	if ( !data )
	{
		*pIsValid = false;
		return false;
	}
	*pIsValid = true;
	// bump the priority
	data->SetAsyncPriority( 1 );

	return data->m_bLoaded;
}


void CAsyncWavDataCache::RestartDataLoad( memhandle_t *pHandle, const char *pFilename, int dataSize, int startpos )
{
	CAsyncWaveData *data = CacheGet( *pHandle );
	if ( !data )
	{
		*pHandle = AsyncLoadCache( pFilename, dataSize, startpos );
	}
}

bool CAsyncWavDataCache::IsDataLoadInProgress( memhandle_t handle )
{
	CAsyncWaveData *data = CacheGet( handle );
	if ( data )
	{
		return data->IsCurrentlyLoading();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Flush()
{
	GetCacheSection()->Flush();
	SpewMemoryUsage( 0 );
}

void CAsyncWavDataCache::QueueUnlock( const memhandle_t &handle )
{
	// not queuing right now, just unlock
	if ( !m_bQueueCacheUnlocks )
	{
		CacheUnlock( handle );
		return;
	}
	// queue to unlock at the end of mixing
	m_unlockQueue.AddToTail( handle );
}

void CAsyncWavDataCache::OnMixBegin()
{
	Assert( !m_bQueueCacheUnlocks );
	m_bQueueCacheUnlocks = true;
	Assert( m_unlockQueue.Count() == 0 );
}

void CAsyncWavDataCache::OnMixEnd()
{
	m_bQueueCacheUnlocks = false;
	// flush the unlock queue
	for ( int i = 0; i < m_unlockQueue.Count(); i++ )
	{
		CacheUnlock( m_unlockQueue[i] );
	}
	m_unlockQueue.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAsyncWavDataCache::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  )
{
	CAsyncWaveData *pWaveData = (CAsyncWaveData *)pItem;
	Q_strncpy( pDest, pWaveData->GetFileName(), nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Spew a cache summary to the console
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::SpewMemoryUsage( int level )
{
	DataCacheStatus_t status;
	DataCacheLimits_t limits;
	GetCacheSection()->GetStatus( &status, &limits );
	int bytesUsed = status.nBytes;
	int bytesTotal = limits.nMaxBytes;

	if ( IsPC() )
	{
		float percent = 100.0f * (float)bytesUsed / (float)bytesTotal;

		Msg( "CAsyncWavDataCache:  %i .wavs total %s, %.2f %% of capacity\n", m_CacheHandles.Count(), Q_pretifymem( bytesUsed, 2 ), percent );

		if ( level >= 1 )
		{
			for ( int i = m_CacheHandles.FirstInorder(); m_CacheHandles.IsValidIndex(i); i = m_CacheHandles.NextInorder(i) )
			{
				char name[MAX_PATH];
				if ( !g_pFileSystem->String( m_CacheHandles[ i ].name, name, sizeof( name ) ) )
				{
					Assert( 0 );
					continue;
				}
				memhandle_t &handle = m_CacheHandles[ i ].handle;
				CAsyncWaveData *data = CacheGetNoTouch( handle );
				if ( data )
				{
					Msg( "\t%16.16s : %s\n", Q_pretifymem(data->Size()),name);
				}
				else
				{
					Msg( "\t%16.16s : %s\n", "not resident",name);
				}
			}
			Msg( "CAsyncWavDataCache:  %i .wavs total %s, %.2f %% of capacity\n", m_CacheHandles.Count(), Q_pretifymem( bytesUsed, 2 ), percent );
		}
	}
	
	if ( IsX360() )
	{
		CAsyncWaveData	*pData;
		BufferEntry_t	*pBuffer;
		BufferHandle_t	h;
		float			percent;
		int				lockCount;
		
		if ( bytesTotal <= 0 )
		{
			// unbounded, indeterminate
			percent = 0;
			bytesTotal = 0;
		}
		else
		{
			percent = 100.0f*(float)bytesUsed/(float)bytesTotal;
		}

		if ( level >= 1 )
		{
			// detail buffers
			ConMsg( "Streaming Buffer List:\n" );
			for ( h = m_BufferList.FirstInorder(); h != m_BufferList.InvalidIndex(); h = m_BufferList.NextInorder( h ) )
			{
				pBuffer = &m_BufferList[h];
				pData = CacheGetNoTouch( pBuffer->m_hWaveData );
				lockCount = GetCacheSection()->GetLockCount( pBuffer->m_hWaveData );

				CacheLockMutex();
				if ( pData )
				{
					ConMsg( "Start:%7d Length:%7d Lock:%3d %s\n", pData->m_async.nOffset, pData->m_nDataSize, lockCount, pData->GetFileName() );
				}
				CacheUnlockMutex();
			}
		}

		ConMsg( "CAsyncWavDataCache: %.2f MB used of %.2f MB, %.2f%% of capacity", (float)bytesUsed/(1024.0f*1024.0f), (float)bytesTotal/(1024.0f*1024.0f), percent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAsyncWavDataCache::Clear()
{
	for ( int i = m_CacheHandles.FirstInorder(); m_CacheHandles.IsValidIndex(i); i = m_CacheHandles.NextInorder(i) )
	{
		CacheEntry_t& dat = m_CacheHandles[i];
		CacheRemove( dat.handle );
	}
	m_CacheHandles.RemoveAll();

	FOR_EACH_LL( m_StreamedHandles, i )
	{
		StreamedEntry_t &dat = m_StreamedHandles[i];
		for ( int j=0; j<dat.m_numBuffers; ++j )
		{
			GetCacheSection()->BreakLock( dat.m_hWaveData[j] );
			CacheRemove( dat.m_hWaveData[j] );
		}
	}
	m_StreamedHandles.RemoveAll();
	m_BufferList.RemoveAll();
}


static CAsyncWavDataCache g_AsyncWaveDataCache;
IAsyncWavDataCache *wavedatacache = &g_AsyncWaveDataCache;

CON_COMMAND( snd_async_flush, "Flush all unlocked async audio data" )
{
	g_AsyncWaveDataCache.Flush();
}

CON_COMMAND( snd_async_showmem, "Show async memory stats" )
{
	g_AsyncWaveDataCache.SpewMemoryUsage( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//			dataOffset - 
//			dataSize - 
//-----------------------------------------------------------------------------
void PrefetchDataStream( const char *pFileName, int dataOffset, int dataSize )
{
	if ( IsX360() )
	{
		// Xbox streaming buffer implementation does not support this "hinting"
		return;
	}

	wavedatacache->PrefetchCache( pFileName, dataSize, dataOffset );
}

//-----------------------------------------------------------------------------
// Purpose: This is an instance of a stream.
//			This contains the file handle and streaming buffer
//			The mixer doesn't know the file is streaming.  The IWaveData
//			abstracts the data access.  The mixer abstracts data encoding/format
//-----------------------------------------------------------------------------
class CWaveDataStreamAsync : public IWaveData
{
public:
	CWaveDataStreamAsync( CAudioSource &source, IWaveStreamSource *pStreamSource, const char *pFileName, int fileStart, int fileSize, CSfxTable *sfx, int startOffset );
	~CWaveDataStreamAsync( void );

	// return the source pointer (mixer needs this to determine some things like sampling rate)
	CAudioSource &Source( void ) { return m_source; }

	// Read data from the source - this is the primary function of a IWaveData subclass
	// Get the data from the buffer (or reload from disk)
	virtual int ReadSourceData( void **pData, int sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	bool IsValid() { return m_bValid; }
	virtual bool IsReadyToMix();

private:
	CWaveDataStreamAsync( const CWaveDataStreamAsync & );

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		VPROF( "CWaveDataStreamAsync::GetCachedDataPointer" );

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			Assert( !"CAudioSourceWave::GetCachedDataPointer info == NULL" );
			return NULL;
		}

		return (byte *)info->CachedData();
	}

	char const				*GetFileName();
	CAudioSource			&m_source;					// wave source
	IWaveStreamSource		*m_pStreamSource;			// streaming
	int						m_sampleSize;				// size of a sample in bytes
	int						m_waveSize;					// total number of samples in the file

	int						m_bufferSize;				// size of buffer in samples
	char					*m_buffer;
	int						m_sampleIndex;
	int						m_bufferCount;
	int						m_dataStart;
	int						m_dataSize;

	memhandle_t				m_hCache;
	StreamHandle_t			m_hStream;
	FileNameHandle_t		m_hFileName;
	
	bool					m_bValid;
	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;
	int						m_nCachedDataSize;
	CSfxTable				*m_pSfx;
};

CWaveDataStreamAsync::CWaveDataStreamAsync
	( 
		CAudioSource &source, 
		IWaveStreamSource *pStreamSource, 
		const char *pFileName, 
		int fileStart, 
		int fileSize, 
		CSfxTable *sfx,
		int startOffset
	) : 
	m_source( source ), 
	m_dataStart( fileStart ), 
	m_dataSize( fileSize ), 
	m_pStreamSource( pStreamSource ), 
	m_bValid( false ), 
	m_hCache( 0 ),
	m_hStream( INVALID_STREAM_HANDLE ),
	m_hFileName( 0 ), 
	m_pSfx( sfx )
{
	m_hFileName = g_pFileSystem->FindOrAddFileName( pFileName );

	// nothing in the buffer yet
	m_sampleIndex = 0;
	m_bufferCount = 0;

	if ( IsPC() )
	{
		m_buffer = new char[SINGLE_BUFFER_SIZE];
		Q_memset( m_buffer, 0, SINGLE_BUFFER_SIZE );
	}

	m_nCachedDataSize = 0;

	if ( m_dataSize <= 0 )
	{
		DevMsg(1, "Can't find streaming wav file: sound\\%s\n", GetFileName() );
		return;
	}

	if ( IsPC() )
	{
		m_hCache = wavedatacache->AsyncLoadCache( GetFileName(), m_dataSize, m_dataStart );

		// size of a sample
		m_sampleSize = source.SampleSize();
		// size in samples of the buffer
		m_bufferSize = SINGLE_BUFFER_SIZE / m_sampleSize;
		// size in samples (not bytes) of the wave itself
		m_waveSize = fileSize / m_sampleSize;

		m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}
	
	if ( IsX360() )
	{
		// size of a sample
		m_sampleSize = source.SampleSize();
		// size in samples (not bytes) of the wave itself
		m_waveSize = fileSize / m_sampleSize;

		streamFlags_t flags = STREAMED_FROMDVD;

		if ( !Q_strnicmp( pFileName, "music", 5 ) && ( pFileName[5] == '\\' || pFileName[5] == '/') )
		{
			// music discards and cycles its buffers
			flags |= STREAMED_SINGLEPLAY;
		}
		else if ( !Q_strnicmp( pFileName, "vo", 2 ) && ( pFileName[2] == '\\' || pFileName[2] == '/' ) && !source.IsSentenceWord() )
		{
			// vo discards and cycles its buffers, except for sentence sources, which do recur
			flags |= STREAMED_SINGLEPLAY;
		}

		int bufferSize;
		if ( source.Format() == WAVE_FORMAT_XMA )
		{
			// each xma block has its own compression rate
			// the buffer must be large enough to cover worst case delivery i/o latency
			// the xma mixer expects quantum xma blocks
			COMPILE_TIME_ASSERT( ( STREAM_BUFFER_DATASIZE % XMA_BLOCK_SIZE ) == 0 );
			bufferSize = STREAM_BUFFER_DATASIZE;
		}
		else
		{
			// calculate a worst case buffer size based on rate
			bufferSize = STREAM_BUFFER_TIME*source.SampleRate()*m_sampleSize;
			if ( source.Format() == WAVE_FORMAT_ADPCM )
			{
				// consider adpcm as 4 bit samples
				bufferSize /= 2;
			}

			if ( source.IsLooped() )
			{
				// lighten the streaming load for looping samples
				// doubling the buffer halves the buffer search/load requests
				bufferSize *= 2;
			}
		}

		// streaming buffers obey alignments
		bufferSize = AlignValue( bufferSize, XBOX_DVD_SECTORSIZE );

		// use double buffering
		int numBuffers = 2;

		if ( m_dataSize <= STREAM_BUFFER_DATASIZE || m_dataSize <= numBuffers*bufferSize )
		{
			// no gain for buffering a small file or multiple buffering
			// match the expected transfer with a single buffer
			bufferSize = m_dataSize;
			numBuffers = 1;
		}

		// size in samples of the transfer buffer
		m_bufferSize = bufferSize / m_sampleSize;

		// allocate a transfer buffer
		// matches the size of the streaming buffer exactly
		// ensures that buffers can be filled and then consumed/requeued at the same time
		m_buffer = new char[bufferSize];

		int loopStart;
		if ( source.IsLooped() )
		{
			int loopBlock;
			loopStart = m_pStreamSource->GetLoopingInfo( &loopBlock, NULL, NULL ) * m_sampleSize;
			if ( source.Format() == WAVE_FORMAT_XMA )
			{
				// xma works in blocks, mixer handles inter-block accurate loop positioning
				// block streaming will cycle from the block where the loop occurs
				loopStart = loopBlock * XMA_BLOCK_SIZE;
			}
		}
		else
		{
			// sample not looped
			loopStart = -1;
		}

		// load the file piecewise through a buffering implementation
		m_hStream = wavedatacache->OpenStreamedLoad( pFileName, m_dataSize, m_dataStart, startOffset, loopStart, bufferSize, numBuffers, flags );
	}

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWaveDataStreamAsync::~CWaveDataStreamAsync( void ) 
{
	if ( IsPC() && m_source.IsPlayOnce() && m_source.CanDelete() )
	{
		m_source.SetPlayOnce( false ); // in case it gets used again
		wavedatacache->Unload( m_hCache );
	}

	if ( IsX360() )
	{
		wavedatacache->CloseStreamedLoad( m_hStream ); 
	}

	delete [] m_buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CWaveDataStreamAsync::GetFileName()
{
	static char fn[MAX_PATH];

	if ( m_hFileName )
	{
		if ( g_pFileSystem->String( m_hFileName, fn, sizeof( fn ) ) )
		{
			return fn;
		}
	}

	Assert( 0 );
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveDataStreamAsync::IsReadyToMix()
{
	if ( IsPC() )
	{
		// If not async loaded, start mixing right away
		if ( !m_source.IsAsyncLoad() && !snd_async_fullyasync.GetBool() )
		{
			return true;
		}

		bool bCacheValid;
		bool bLoaded = wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid );
		if ( !bCacheValid )
		{
			wavedatacache->RestartDataLoad( &m_hCache, GetFileName(), m_dataSize, m_dataStart );
		}
		return bLoaded;
	}

	if ( IsX360() )
	{
		return wavedatacache->IsStreamedDataReady( m_hStream );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Read data from the source - this is the primary function of a IWaveData subclass
//  Get the data from the buffer (or reload from disk)
// Input  : **pData - 
//			sampleIndex - 
//			sampleCount - 
//			copyBuf[AUDIOSOURCE_COPYBUF_SIZE] - 
// Output : int
//-----------------------------------------------------------------------------
int CWaveDataStreamAsync::ReadSourceData( void **pData, int sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	// Current file position
	int seekpos = m_dataStart + m_sampleIndex * m_sampleSize;

	// wrap position if looping
	if ( m_source.IsLooped() )
	{
		sampleIndex = m_pStreamSource->UpdateLoopingSamplePosition( sampleIndex );
		if ( sampleIndex < m_sampleIndex )
		{
			// looped back, buffer has no samples yet
			m_sampleIndex = sampleIndex;
			m_bufferCount = 0;

			// update file position
			seekpos = m_dataStart + sampleIndex * m_sampleSize;
		}
	}

	// UNDONE: This is an error!!
	// The mixer playing back the stream tried to go backwards!?!?!
	// BUGBUG: Just play the beginning of the buffer until we get to a valid linear position
	if ( sampleIndex < m_sampleIndex )
		sampleIndex = m_sampleIndex;

	// calc sample position relative to the current buffer
	// m_sampleIndex is the sample position of the first byte of the buffer
	sampleIndex -= m_sampleIndex;
	
	// out of range? refresh buffer
	if ( sampleIndex >= m_bufferCount )
	{
		// advance one buffer (the file is positioned here)
		m_sampleIndex += m_bufferCount;
		// next sample to load
		sampleIndex -= m_bufferCount;

		// if the remainder is greated than one buffer size, seek over it.  Otherwise, read the next chunk
		// and leave the remainder as an offset.

		// number of buffers to "skip" (as in the case where we are starting a streaming sound not at the beginning)
		int skips = sampleIndex / m_bufferSize;
		
		// If we are skipping over a buffer, do it with a seek instead of a read.
		if ( skips )
		{
			// skip directly to next position
			m_sampleIndex += sampleIndex;
			sampleIndex = 0;
		}

		// move the file to the new position
		seekpos = m_dataStart + (m_sampleIndex * m_sampleSize);

		// This is the maximum number of samples we could read from the file
		m_bufferCount = m_waveSize - m_sampleIndex;
		
		// past the end of the file?  stop the wave.
		if ( m_bufferCount <= 0 )
			return 0;

		// clamp available samples to buffer size
		if ( m_bufferCount > m_bufferSize )
			m_bufferCount = m_bufferSize;

		if ( IsPC() )
		{
			// See if we can load in the intial data right out of the cached data lump instead.
			int cacheddatastartpos = ( seekpos - m_dataStart );

			// FastGet doesn't call into IsPrecachedSound if the handle appears valid...
			CAudioSourceCachedInfo *info = m_AudioCacheHandle.FastGet();
			if ( !info )
			{
				// Full recache
				info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
			}

			bool startupCacheUsed = false;

			if  ( info && 
				( m_nCachedDataSize > 0 ) && 
				( cacheddatastartpos < m_nCachedDataSize ) )
			{
				// Get a ptr to the cached data
				const byte *cacheddata = info->CachedData();
				if ( cacheddata )
				{
					// See how many samples of cached data are available (cacheddatastartpos is zero on the first read)
					int availSamples = ( m_nCachedDataSize - cacheddatastartpos ) / m_sampleSize;

					// Clamp to size of our internal buffer
					if ( availSamples > m_bufferSize )
					{
						availSamples = m_bufferSize;
					}

					// Mark how many we are returning
					m_bufferCount = availSamples;
					// Copy raw sample data directly out of cache
					Q_memcpy( m_buffer, ( char * )cacheddata + cacheddatastartpos, availSamples * m_sampleSize );

					startupCacheUsed = true;
				}
			}

			// Not in startup cache, grab data from async cache loader (will block if data hasn't arrived yet)
			if ( !startupCacheUsed )
			{
				bool postprocessed = false;
				
				// read in the max bufferable, available samples
				if ( !wavedatacache->CopyDataIntoMemory( 
					m_hCache, 
					GetFileName(), 
					m_dataSize, 
					m_dataStart,
					m_buffer, 
					sizeof( m_buffer ),
					seekpos, 
					m_bufferCount * m_sampleSize,
					&postprocessed ) )
				{
					return 0;
				}

				// do any conversion the source needs (mixer will decode/decompress)
				if ( !postprocessed )
				{
					// Note that we don't set the postprocessed flag on the underlying data, since for streaming we're copying the
					//  original data into this buffer instead.
					m_pStreamSource->UpdateSamples( m_buffer, m_bufferCount );
				}
			}
		}

		if ( IsX360() )
		{
			if ( m_hStream != INVALID_STREAM_HANDLE )
			{
				// request available data, may get less
				// drives the buffering
				m_bufferCount = wavedatacache->CopyStreamedDataIntoMemory( 
									m_hStream, 
									m_buffer, 
									m_bufferSize * m_sampleSize,
									seekpos, 
									m_bufferCount * m_sampleSize );
				// convert to number of samples in the buffer
				m_bufferCount /= m_sampleSize;
			}
			else
			{
				return 0;
			}

			// do any conversion now the source needs (mixer will decode/decompress) on this buffer
			m_pStreamSource->UpdateSamples( m_buffer, m_bufferCount );
		}
	}

	// If we have some samples in the buffer that are within range of the request
	// Use unsigned comparisons so that if sampleIndex is somehow negative that
	// will be treated as out of range.
	if ( (unsigned)sampleIndex < (unsigned)m_bufferCount )
	{
		// Get the desired starting sample
		*pData = (void *)&m_buffer[sampleIndex * m_sampleSize];

		// max available
		int available = m_bufferCount - sampleIndex;
		// clamp available to max requested
		if ( available > sampleCount )
			available = sampleCount;

		return available;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Iterator for wave data (this is to abstract streaming/buffering)
//-----------------------------------------------------------------------------
class CWaveDataMemoryAsync : public IWaveData
{
public:
	CWaveDataMemoryAsync( CAudioSource &source );
	~CWaveDataMemoryAsync( void ) {}
	CAudioSource &Source( void ) { return m_source; }
	
	virtual int ReadSourceData( void **pData, int sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	virtual bool IsReadyToMix();

private:
	CAudioSource		&m_source;	// pointer to source
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
//-----------------------------------------------------------------------------
CWaveDataMemoryAsync::CWaveDataMemoryAsync( CAudioSource &source ) : 
	m_source(source) 
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **pData - 
//			sampleIndex - 
//			sampleCount - 
//			copyBuf[AUDIOSOURCE_COPYBUF_SIZE] - 
// Output : int
//-----------------------------------------------------------------------------
int CWaveDataMemoryAsync::ReadSourceData( void **pData, int sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	return m_source.GetOutputData( pData, sampleIndex, sampleCount, copyBuf );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveDataMemoryAsync::IsReadyToMix()
{
	if ( !m_source.IsAsyncLoad() && !snd_async_fullyasync.GetBool() )
	{
		// Wait until we're pending at least
		if ( m_source.GetCacheStatus() == CAudioSource::AUDIO_NOT_LOADED )
		{
			return false;
		}
		return true;
	}

	if ( m_source.IsCached() )
	{
		return true;
	}

	if ( IsPC() )
	{
		// Msg( "Waiting for data '%s'\n", m_source.GetFileName() );
		m_source.CacheLoad();
	}

	if ( IsX360() )
	{
		// expected to be resident and valid, otherwise being called prior to load
		Assert( 0 );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
//			*pStreamSource - 
//			&io - 
//			*pFileName - 
//			dataOffset - 
//			dataSize - 
// Output : IWaveData
//-----------------------------------------------------------------------------
IWaveData *CreateWaveDataStream( CAudioSource &source, IWaveStreamSource *pStreamSource, const char *pFileName, int dataStart, int dataSize, CSfxTable *pSfx, int startOffset )
{
	CWaveDataStreamAsync *pStream = new CWaveDataStreamAsync( source, pStreamSource, pFileName, dataStart, dataSize, pSfx, startOffset );
	if ( !pStream || !pStream->IsValid() )
	{
		delete pStream;
		pStream = NULL;
	}
	return pStream;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &source - 
// Output : IWaveData
//-----------------------------------------------------------------------------
IWaveData *CreateWaveDataMemory( CAudioSource &source )
{
	CWaveDataMemoryAsync *mem = new CWaveDataMemoryAsync( source );
	return mem;
}
