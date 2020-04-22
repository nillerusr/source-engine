//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "../utils/bzip2/bzlib.h"
#include "sv_filepublish.h"
#include "utlstring.h"
#include "strtools.h"
#include "sv_replaycontext.h"
#include "convar.h"
#include "fmtstr.h"
#include "compression.h"
#include "replay/shared_defs.h"
#include "spew.h"
#include "utlqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

ConVar replay_publish_simulate_delay_local_http( "replay_publish_simulate_delay_local_http", "0", FCVAR_DONTRECORD,
	"Simulate a delay (in seconds) when publishing replay data via local HTTP.", true, 0.0f, true, 60.0f );
ConVar replay_publish_simulate_rename_fail( "replay_publish_simulate_rename_fail", "0", FCVAR_DONTRECORD,
	"Simulate a rename failure during local HTTP publishing, which will force a manual copy & delete.", true, 0.0f, true, 1.0f );

//----------------------------------------------------------------------------------------

CBasePublishJob::CBasePublishJob( JobPriority_t nPriority/*=JP_NORMAL*/,
								  ISpewer *pSpewer/*=g_pDefaultSpewer*/ )
:	CBaseJob( nPriority, pSpewer )
{
}

void CBasePublishJob::SimulateDelay( int nDelay, const char *pThreadName )
{
	if ( nDelay > 0 )
	{
		Log( "%s thread: Simulating %i sec delay.\n", pThreadName, nDelay );
		ThreadSleep( nDelay * 1000 );
		Log( "%s thread: simulation done.\n", pThreadName );
	}
}

//----------------------------------------------------------------------------------------

CLocalPublishJob::CLocalPublishJob( const char *pLocalFilename )
{
	V_strcpy( m_szLocalFilename, pLocalFilename );
}

JobStatus_t	CLocalPublishJob::DoExecute()
{
	DBG( "Attempting to rename file to local fileserver path..." );

	PrintEventStartMsg( "Source file exists?" );
	if ( !g_pFullFileSystem->FileExists( m_szLocalFilename ) )
	{
		PrintEventResult( false );
		CFmtStr fmtError( "Source file '%s' does not exist", m_szLocalFilename );
		SetError( ERROR_SOURCE_FILE_DOES_NOT_EXIST, fmtError.Access() );
		return JOB_FAILED;
	}
	PrintEventResult( true );

	// Make sure the publish path exists
	const char *pFileserverPath = g_pServerReplayContext->GetLocalFileServerPath();
	PrintEventStartMsg( "Checking fileserver path" );
	if ( !g_pFullFileSystem->IsDirectory( pFileserverPath ) )
	{
		PrintEventResult( false );
		CFmtStr fmtError( "Fileserver path '%s' invalid (see replay_local_fileserver_path)",
			pFileserverPath );
		SetError( ERROR_INVALID_FILESERVER_PATH, fmtError.Access() );
		return JOB_FAILED;
	}
	PrintEventResult( true );

	// Format a path & filename that points to the fileserver's download directory, with <session name>.dmx on the end
	const char *pFilename = V_UnqualifiedFileName( m_szLocalFilename );
	CFmtStr fmtPublishFilename( "%s%s", pFileserverPath, pFilename );
	const char *pTargetFilename = fmtPublishFilename.Access();

	// Delete the destination file if it exists already
	if ( g_pFullFileSystem->FileExists( pTargetFilename ) )
	{
		PrintEventStartMsg( "Target file exists - deleting" );
		g_pFullFileSystem->RemoveFile( pTargetFilename );

		// Give the system a bit of time before another check
		ThreadSleep( 500 );

		if ( g_pFullFileSystem->FileExists( pTargetFilename ) )
		{
#ifdef WIN32
	        LPVOID pMsgBuf;
	        if ( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                            NULL,
	                            GetLastError(),
	                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	                            (LPTSTR) &pMsgBuf,
	                            0,
	                            NULL ))
	        {
				Log( "\n\nError: %s\n", (const char *)pMsgBuf );
	            LocalFree( pMsgBuf );
	        }
#endif
			PrintEventResult( false );
			CFmtStr fmtError( "Target already existed and could not be removed: '%s'", pTargetFilename );
			SetError( ERROR_COULD_NOT_DELETE_TARGET_FILE, fmtError.Access() );
			return JOB_FAILED;
		}
		PrintEventResult( true );
	}

	// Simulate a delay if necessary
	SimulateDelay( replay_publish_simulate_delay_local_http.GetInt(), "Local HTTP" );

	// Rename the file - RenameFile() still returns true, even if the destination pathname
	// is nonsense.  If the *source* is invalid, it fails as expected, though.  Adding a FileExists()
	// does not help.
	PrintEventStartMsg( "Renaming to target" );
	const bool bSimulateRenameFail = replay_publish_simulate_rename_fail.GetBool();
	if ( bSimulateRenameFail || !g_pFullFileSystem->RenameFile( m_szLocalFilename, pTargetFilename ) )
	{
		// Try to explicitly copy to target
		if ( g_pEngine->CopyFile( m_szLocalFilename, pTargetFilename ) )
		{
			// ...and deletion of source.
			g_pFullFileSystem->RemoveFile( m_szLocalFilename );
		}
		else
		{
			PrintEventResult( false );
			CFmtStr fmtError( "Failed to rename '%s' -> '%s'\n", m_szLocalFilename, pTargetFilename );
			SetError( ERROR_RENAME_FAILED, fmtError.Access() );
			return JOB_FAILED;
		}
	}

	PrintEventResult( true );
	DBG( "Rename succeeded.\n" );
	return JOB_OK;
}

//----------------------------------------------------------------------------------------

CLocalPublishJob *SV_CreateLocalPublishJob( const char *pLocalFilename )
{
	return new CLocalPublishJob( pLocalFilename );
}

//----------------------------------------------------------------------------------------

CCompressionJob::CCompressionJob( const uint8 *pSrcData, uint32 nSrcSize, CompressorType_t nType,
								  bool *pOutResult, uint32 *pResultSize )
:	m_pSrcData( pSrcData ),
	m_nSrcSize( nSrcSize ),
	m_pCompressionResult( pOutResult ),
	m_pResultSize( pResultSize )
{
	*m_pCompressionResult = false;
	*m_pResultSize = 0;

	m_pCompressor = CreateCompressor( nType );
}

JobStatus_t	CCompressionJob::DoExecute()
{
	IF_REPLAY_DBG2( Warning( "Attempting to compress...\n" ) );

	if ( m_nSrcSize == 0 )
	{
		SetError( ERROR_FAILED_ZERO_LENGTH_DATA, "Compression failed.  Zero length data." );
		return JOB_FAILED;
	}

	int nResult = JOB_FAILED;

	// Attempt to compress the file
	const int nMaxCompressedSize = ceil( m_nSrcSize * 1.1f ) + 600;	// see "destLen" - http://www.bzip.org/1.0.3/html/util-fns.html
	uint8 *pCompressed = new uint8[ nMaxCompressedSize ];

	// Compress
	unsigned int nCompressedSize;
	PrintEventStartMsg( "Compressing" );
	if ( !m_pCompressor->Compress( (char *)pCompressed, &nCompressedSize, (const char *)m_pSrcData, m_nSrcSize ) )
	{
		// Compression failed?
		IF_REPLAY_DBG2( Warning( "Could not compress stream.\n" ) );
		PrintEventResult( false );
		SetError( ERROR_OK_COULDNOTCOMPRESS );

		// Set result to uncompressed buffer and free compressed
		m_pResult = (uint8 *)m_pSrcData;
		delete [] pCompressed;

		*m_pCompressionResult = false;
		*m_pResultSize = m_nSrcSize;
	}
	else
	{
		PrintEventResult( true );

		// Success!
		DBG( "Compression succeeded.\n" );

		nResult = JOB_OK;
	
		// Set result to compressed buffer
		m_pResult = pCompressed;

		*m_pResultSize = nCompressedSize;
		*m_pCompressionResult = true;
	}

	// Compression would have been worse than not compressing at all
	return nResult;
}

void CCompressionJob::GetOutputData( uint8 **ppData, uint32 *pDataSize ) const
{
	*ppData = m_pResult;
	*pDataSize = *m_pResultSize;
}

//----------------------------------------------------------------------------------------

CMd5Job::CMd5Job( const void *pSrcData, int nSrcSize, bool *pOutHashed, uint8 *pOutHash,
				  unsigned int pSeed[4]/*=NULL*/ )
:	m_pSrcData( pSrcData ),
	m_nSrcSize( nSrcSize ),
	m_pHashed( pOutHashed ),
	m_pHash( pOutHash ),
	m_pSeed( pSeed )
{
	*m_pHashed = false;
	V_memset( pOutHash, 0, 16 );
}

JobStatus_t CMd5Job::DoExecute()
{
	IF_REPLAY_DBG2( Warning( "Attempting to hash...\n" ) );

	PrintEventStartMsg( "Running" );
	bool bResult = g_pEngine->MD5_HashBuffer( m_pHash, (const uint8 *)m_pSrcData, m_nSrcSize, m_pSeed );
	PrintEventResult( bResult );
	*m_pHashed = bResult;

	if ( !bResult )
		return JOB_FAILED;

	IF_REPLAY_DBG2( Warning( "Hash succeeded\n" ) );
	return JOB_OK;
}

//----------------------------------------------------------------------------------------

CDeleteLocalFileJob::CDeleteLocalFileJob( const char *pFilename )
{
	V_strncpy( m_szFilename, pFilename, sizeof( m_szFilename ) - 1 );
}

JobStatus_t CDeleteLocalFileJob::DoExecute()
{
	// File exists?
	if ( !g_pFullFileSystem->FileExists( m_szFilename ) )
	{
		SetError( ERROR_FILE_DOES_NOT_EXISTS );
		return JOB_FAILED;
	}

	// Attempt to remove the file now
	g_pFullFileSystem->RemoveFile( m_szFilename );

	// Delete succeeded?
	if ( g_pFullFileSystem->FileExists( m_szFilename ) )
	{
		SetError( ERROR_COULD_NOT_DELETE );
		return JOB_FAILED;
	}

	return JOB_OK;
}

//----------------------------------------------------------------------------------------

class CBaseFilePublisher : public IFilePublisher
{
public:
	enum Phase_t
	{
		PHASE_INVALID = -1,

		PHASE_COMPRESSION,
		PHASE_HASH,
		PHASE_ADJUSTHEADER,
		PHASE_WRITETODISK,
		PHASE_PUBLISH,
		PHASE_DELETEFILE,

		NUM_PHASES
	};

	CBaseFilePublisher()
	:	m_pCallbackHandler( NULL ),
		m_pUserData( NULL ),
		m_pCurrentJob( NULL ),
		m_pInData( NULL ),
		m_pHeaderData( NULL ),
		m_nStatus( PUBLISHSTATUS_INVALID ),
		m_nPhase( PHASE_INVALID ),
		m_bCompressedOk( false ),
		m_bHashedOk( false ),
		m_nHeaderSize( 0 ),
		m_nCompressedSize( 0 ),
		m_nInSize( 0 ),
		m_nInType( IO_INVALID )
	{
		m_szOutFilename[ 0 ] = 0;
		V_memset( m_aHash, 0, sizeof( m_aHash ) );
	}

	virtual PublishStatus_t	GetStatus() const
	{
		return m_nStatus;
	}

	void SetStatus( PublishStatus_t nStatus )
	{
		m_nStatus = nStatus;
	}

	virtual bool IsDone() const
	{
		return m_nStatus != PUBLISHSTATUS_INVALID;
	}

	virtual bool Compressed() const
	{
		return m_bCompressedOk;
	}

	virtual bool Hashed() const
	{
		return m_bHashedOk;
	}

	virtual void GetHash( uint8 *pOut ) const
	{
		V_memcpy( pOut, m_aHash, sizeof( m_aHash ) );
	}

	virtual CompressorType_t GetCompressorType() const
	{
		return m_bCompressedOk ? m_nCompressorType : COMPRESSORTYPE_INVALID;
	}

	virtual int GetCompressedSize() const
	{
		return m_nCompressedSize;
	}

	virtual void AbortAndCleanup()
	{
		if ( m_pCurrentJob )
		{
			m_pCurrentJob->Abort( true );
			m_pCurrentJob = NULL;
		}
	}

	virtual void FinishSynchronouslyAndCleanup()
	{
		if ( m_pCurrentJob )
		{
			m_pCurrentJob->WaitForFinishAndRelease();
			m_pCurrentJob = NULL;
		}

		SetStatus( PUBLISHSTATUS_ABORTED );
	}

	virtual void Publish( const PublishFileParams_t &params )
	{
		V_strcpy( m_szOutFilename, params.m_pOutFilename );

		m_pInData = params.m_pSrcData;
		m_nInSize = params.m_nSrcSize;
		m_pCallbackHandler = params.m_pCallbackHandler;
		m_pUserData = params.m_pUserData;
		m_bFreeSrcData = params.m_bFreeSrcData;
		m_pSrcData = params.m_pSrcData;		// Cache src data so we can determine whether free'ing is OK
		m_pHeaderData = params.m_pHeaderData;
		m_nHeaderSize = params.m_nHeaderSize;

		m_flStartTime = g_pEngine->GetHostTime();

		if ( params.m_nCompressorType != COMPRESSORTYPE_INVALID )
		{
			m_PhaseQueue.Insert( PHASE_COMPRESSION );
			m_nCompressorType = params.m_nCompressorType;	// Cache compressor type
		}

		if ( params.m_bHash )
		{
			m_PhaseQueue.Insert( PHASE_HASH );
		}

		if ( params.m_pHeaderData )
		{
			Assert( params.m_nHeaderSize );
			m_PhaseQueue.Insert( PHASE_ADJUSTHEADER );
		}

		m_PhaseQueue.Insert( PHASE_WRITETODISK );
		m_PhaseQueue.Insert( PHASE_PUBLISH );

		if ( params.m_bDeleteFile )
		{
			m_PhaseQueue.Insert( PHASE_DELETEFILE );
		}

		// Start off first job
		SetupNextJob( true );
	}

	void PrintErrors()
	{
		// If we don't print out any error now, it'll be lost once the job is released.  Kind of a hack.
		if ( m_pCurrentJob->GetStatus() == JOB_FAILED && !IsFailureOkForPhase() )
		{
			CBasePublishJob *pCurrentJob = dynamic_cast< CBasePublishJob * >( m_pCurrentJob );
			if ( pCurrentJob )
			{
				g_pBlockSpewer->PrintBlockStart();
				g_pBlockSpewer->PrintEventError( pCurrentJob->GetErrorStr() );
				g_pBlockSpewer->PrintBlockEnd();
			}
		}
	}

	void Abort()
	{
		// Abort the job
		if ( m_pCurrentJob )
		{
			m_pCurrentJob->Abort( true );
			m_pCurrentJob = NULL;
		}

		// Update status
		SetStatus( PUBLISHSTATUS_ABORTED );

		// Let owner know we've aborted
		if ( m_pCallbackHandler )
		{
			m_pCallbackHandler->OnPublishAborted( this );
		}
	}

	virtual void Think()
	{
		const float flCurTime = g_pEngine->GetHostTime();
		extern ConVar replay_fileserver_offload_aborttime;
		if ( flCurTime > m_flStartTime + replay_fileserver_offload_aborttime.GetFloat() )
		{
			g_pBlockSpewer->PrintMsg( Replay_va( "ERROR: Publish timed out after %i seconds.", replay_fileserver_offload_aborttime.GetInt() ) );
			Abort();
			return;
		}

		if ( !m_pCurrentJob )
			return;

		const int nJobStatus = m_pCurrentJob->GetStatus();
		if ( nJobStatus <= JOB_OK )
		{
			PrintErrors();

			// What it says
			CacheOutputsOfCurrentJobForInputsOfNextJob();

			// Job's done - clean up
			m_pCurrentJob->Release();
			m_pCurrentJob = NULL;

			// Did the current job fail?
			bool bPublishDone = false;
			if ( nJobStatus < JOB_OK && !IsFailureOkForPhase() )
			{
				// Don't process the next job
				SetStatus( PUBLISHSTATUS_FAILED );
				bPublishDone = true;
			}
			else if ( IsLastPhase() )
			{
				// nJobStatus is JOB_OK and we are in publish phase.
				SetStatus( PUBLISHSTATUS_OK );
				bPublishDone = true;
			}

			if ( bPublishDone )
			{
				InvokeCallback();
				return;
			}

			// Otherwise, publish isn't complete yet - go to next phase and spawn job thread
			SetupNextJob( false );
		}
	}

protected:
	virtual CBasePublishJob *GetPublishJob() const = 0;

	char				m_szOutFilename[MAX_OSPATH];	// Filename only
	IPublishCallbackHandler	*m_pCallbackHandler;
	void				*m_pUserData;

private:
	enum IO_t
	{
		IO_INVALID = -1,
		IO_BUFFER,
		IO_FILE,
		IO_DONTCARE,	// As an input, this means the job doesn't care about the main pipeline stream
						// (e.g. adjust header gets its inputs elsewhere) phase.  As an output, this
						// should only be used for the final phase (publish).
	};

	void CacheOutputsOfCurrentJobForInputsOfNextJob()
	{
		bool bFreeOldInData = false;
		uint8 *pOldInData = m_pInData;

		IO_t nOutputType = GetCurrentPhaseOutputType();

		// Write phase is a special case
		if ( m_nPhase == PHASE_WRITETODISK )
		{
			// Clear the in buffer
			m_pInData = NULL;
			m_nInSize = 0;

			bFreeOldInData = true;
		}
		else if ( nOutputType == IO_BUFFER )
		{
			// This should always be a CBasePublishJob
			CBasePublishJob *pCurrentJob = dynamic_cast< CBasePublishJob * >( m_pCurrentJob );
			Assert( pCurrentJob );

			// Get job output buffer
			uint8 *pJobOutData;
			uint32 nJobOutDataSize;
			pCurrentJob->GetOutputData( &pJobOutData, &nJobOutDataSize );

			// Compare output data against input data - if different, free input and replace
			// with output.  In the case of hashing, for example, the input buffer is used
			// to do some computation, but the buffer itself goes untouched.
			if ( pJobOutData && ( m_pInData != pJobOutData || m_nInSize != nJobOutDataSize ) )
			{
				m_pInData = pJobOutData;
				m_nInSize = nJobOutDataSize;
				bFreeOldInData = true;
			}
		}
		else if ( nOutputType == IO_DONTCARE )
		{
			// This should have been cleaned up in write-to-disk phase if we're in publish phase
			Assert( m_nPhase != PHASE_PUBLISH || m_pInData == NULL );
		}
#ifdef _DEBUG
		else
		{
			AssertMsg( 0, "Shouldn't reach here" );
		}
#endif

		// Free old input data?
		if ( bFreeOldInData && ( m_bFreeSrcData || pOldInData != m_pSrcData ) )
		{
			delete [] pOldInData;
		}

		// Cache output of current job for input of next job
		if ( m_nPhase != PHASE_PUBLISH )
		{
			m_nInType = nOutputType;
		}
	}

	// NOTE: This needs to return a CJob ptr (i.e. and not a CBaseJob) since the job may be an AsyncWrite
	CJob *GetJobForPhase( Phase_t nPhase )
	{
		CJob *pResult = NULL;

		switch ( nPhase )
		{
		case PHASE_COMPRESSION:
			pResult = new CCompressionJob( m_pInData, m_nInSize, m_nCompressorType, &m_bCompressedOk, &m_nCompressedSize );
			break;

		case PHASE_HASH:
			pResult = new CMd5Job( m_pInData, m_nInSize, &m_bHashedOk, m_aHash );
			break;

		case PHASE_ADJUSTHEADER:
			{
				// Let the callback handler make any adjustments to the header (add md5 digest, etc.)
				m_pCallbackHandler->AdjustHeader( this, m_pHeaderData );

				if ( m_pHeaderData && m_nHeaderSize )
				{
					// Write the header to the target file
					FSAsyncControl_t hFileJob;
					const bool bFreeMemory = false;
					g_pFullFileSystem->AsyncWrite( m_szOutFilename, m_pHeaderData, m_nHeaderSize, bFreeMemory, false, &hFileJob );
					pResult = (CJob *)hFileJob;
				}
			}
			break;

		case PHASE_WRITETODISK:
			if ( m_pInData && m_nInSize )
			{
				// Create an asynchronous write job - if a header already exists in the file, append.
				FSAsyncControl_t hFileJob;
				const bool bAppend = m_pHeaderData != NULL;
				g_pFullFileSystem->AsyncWrite( m_szOutFilename, m_pInData, m_nInSize, false, bAppend, &hFileJob );
				pResult = (CJob *)hFileJob;
			}
			break;

		case PHASE_PUBLISH:
			pResult = GetPublishJob();
			break;

		case PHASE_DELETEFILE:
			pResult = new CDeleteLocalFileJob( m_szOutFilename );
			break;

		default:
			AssertMsg( 0, "File publish phase is bad." );
		}

		// Sanity check input type with output type of previous job
		Assert(
			GetCurrentPhaseInputType() == IO_DONTCARE ||
			m_nInType == IO_DONTCARE ||
			GetCurrentPhaseInputType() == m_nInType
		);

		return pResult;
	}

	bool IsFailureOkForPhase() const
	{
		// Compression will fail (e.g. due to small buffer size), which shouldn't bring down the house.
		return m_nPhase == PHASE_COMPRESSION || m_nPhase == PHASE_DELETEFILE;
	}

	bool IsLastPhase() const
	{
		return m_PhaseQueue.Count() == 0;
	}

	IO_t GetCurrentPhaseInputType() const
	{
		return sm_aPhaseIOTypes[ m_nPhase ].m_nInputType;
	}

	IO_t GetCurrentPhaseOutputType() const
	{
		return sm_aPhaseIOTypes[ m_nPhase ].m_nOutputType;
	}

	void SetupNextJob( bool bFirstJob )
	{
		// Get next phase from queue
		Assert( m_PhaseQueue.Count() > 0 );
		m_nPhase = ( Phase_t )m_PhaseQueue.RemoveAtHead();

		// Set the input type if this is the first job
		if ( bFirstJob )
		{
			m_nInType = GetCurrentPhaseInputType();
		}

		// Create the job
		m_pCurrentJob = GetJobForPhase( m_nPhase );

		// Kick off the job now
		SV_GetThreadPool()->AddJob( m_pCurrentJob );
	}

	void InvokeCallback()
	{
		if ( m_pCallbackHandler )
		{
			m_pCallbackHandler->OnPublishComplete( this, m_pUserData );
		}
	}

	CUtlQueue< uint8 >	m_PhaseQueue;
	bool				m_bCompressedOk;
	bool				m_bHashedOk;
	CompressorType_t	m_nCompressorType;
	uint8				m_aHash[16];
	Phase_t				m_nPhase;
	PublishStatus_t		m_nStatus;
	CJob				*m_pCurrentJob;
	uint32				m_nCompressedSize;

	IO_t				m_nInType;
	uint8				*m_pInData;
	uint32				m_nInSize;

	bool				m_bFreeSrcData;
	void				*m_pSrcData;

	void				*m_pHeaderData;
	int					m_nHeaderSize;

	float				m_flStartTime;

	struct IoInfo_t
	{
		IO_t	m_nInputType;
		IO_t	m_nOutputType;
	};

	static IoInfo_t		sm_aPhaseIOTypes[ NUM_PHASES ];
};

CBaseFilePublisher::IoInfo_t CBaseFilePublisher::sm_aPhaseIOTypes[ NUM_PHASES ] =
{
	// Input		Output
	{ IO_BUFFER,	IO_BUFFER },	// PHASE_COMPRESSION
	{ IO_BUFFER,	IO_BUFFER },	// PHASE_HASH
	{ IO_DONTCARE,	IO_DONTCARE },	// PHASE_ADJUSTHEADER - this phase can operate independent of the pipeline, so
									//						long as any compression/hashing is taken care of.
	{ IO_BUFFER,	IO_FILE },		// PHASE_WRITETODISK
	{ IO_FILE,		IO_DONTCARE },	// PHASE_PUBLISH
	{ IO_DONTCARE,	IO_DONTCARE }	// PHASE_DELETEFILE
};

//----------------------------------------------------------------------------------------

class CLocalFileserverPublisher : public CBaseFilePublisher
{
	typedef CBaseFilePublisher BaseClass;
public:
	virtual CBasePublishJob *GetPublishJob() const
	{
		DBG( "Attempting to publish a file locally...\n" );

		// Destination filename is implied
		return new CLocalPublishJob( m_szOutFilename );
	}
};



//----------------------------------------------------------------------------------------

IFilePublisher *SV_PublishFile( const PublishFileParams_t &params )
{
	Assert( !params.m_pHeaderData || ( params.m_pHeaderData && params.m_pCallbackHandler ) );

	IFilePublisher *pResult;

	pResult = new CLocalFileserverPublisher();
	
	pResult->Publish( params );

	return pResult;
}

//----------------------------------------------------------------------------------------
