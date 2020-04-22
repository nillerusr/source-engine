//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "basefilesystem.h"
#include "tier0/vprof.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if !defined( DEDICATED )

#ifdef SUPPORT_PACKED_STORE

unsigned ThreadStubProcessMD5Requests( void *pParam )
{
	return ((CFileTracker2 *)pParam)->ThreadedProcessMD5Requests();
}

//-----------------------------------------------------------------------------
// ThreadedProcessMD5Requests
// Calculate the MD5s of all the blocks submitted to us
//-----------------------------------------------------------------------------
unsigned CFileTracker2::ThreadedProcessMD5Requests()
{
	ThreadSetDebugName( "ProcessMD5Requests" );

	while ( m_bThreadShouldRun )
	{
		StuffToMD5_t stuff;

		while ( m_PendingJobs.PopItem( &stuff ) )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ ); 

			MD5Context_t ctx;
			memset( &ctx, 0, sizeof(MD5Context_t) );
			MD5Init( &ctx );
			MD5Update( &ctx, stuff.m_pubBuffer, stuff.m_cubBuffer );
			MD5Final( stuff.m_md5Value.bits, &ctx);

			{
				// update the FileTracker MD5 database
				AUTO_LOCK( m_Mutex );

				TrackedVPKFile_t &trackedVPKFile = m_treeTrackedVPKFiles[ stuff.m_idxTrackedVPKFile ];
				TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ trackedVPKFile.m_idxAllOpenedFiles ];

				memcpy( trackedfile.m_filehashFinal.m_md5contents.bits, stuff.m_md5Value.bits, sizeof( trackedfile.m_filehashFinal.m_md5contents.bits ) );
				trackedfile.m_filehashFinal.m_crcIOSequence = stuff.m_cubBuffer;
				trackedfile.m_filehashFinal.m_cbFileLen = stuff.m_cubBuffer;
				trackedfile.m_filehashFinal.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
				trackedfile.m_filehashFinal.m_nPackFileNumber = trackedVPKFile.m_nPackFileNumber;
				trackedfile.m_filehashFinal.m_PackFileID = trackedVPKFile.m_PackFileID;
			}

			m_CompletedJobs.PushItem( stuff );
			m_threadEventWorkCompleted.Set();
		}

		{
			tmZone( TELEMETRY_LEVEL0, TMZF_IDLE, "m_threadEventWorkToDo" ); 

			m_threadEventWorkToDo.Wait( 1000 );
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// SubmitThreadedMD5Request
// add pubBuffer,cubBuffer to our queue of stuff to MD5
// caller promises that the memory will remain valid
// until BlockUntilMD5RequestComplete() is called
// returns: request handle
//-----------------------------------------------------------------------------
int CFileTracker2::SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction )
{
	int idxList;
	StuffToMD5_t stuff;

	{
		AUTO_LOCK( m_Mutex );

		TrackedVPKFile_t trackedVPKFileFind;
		trackedVPKFileFind.m_nPackFileNumber = nPackFileNumber;
		trackedVPKFileFind.m_PackFileID = PackFileID;
		trackedVPKFileFind.m_nFileFraction = nPackFileFraction;

		int idxTrackedVPKFile = m_treeTrackedVPKFiles.Find( trackedVPKFileFind );
		if ( idxTrackedVPKFile != m_treeTrackedVPKFiles.InvalidIndex() )
		{
			// dont early out if we have already done the MD5, if the caller wants us
			// to do it again - then do it again
			m_cDupMD5s++;
		}
		else
		{
			// this is an error, we should already know about the file
			Assert(0);
			return 0;
		}

		SubmittedMd5Job_t submittedjob;
		submittedjob.m_bFinished = false;
		idxList = m_SubmittedJobs.AddToTail( submittedjob );

		stuff.m_pubBuffer = pubBuffer;
		stuff.m_cubBuffer = cubBuffer;
		stuff.m_idxTrackedVPKFile = idxTrackedVPKFile;
		stuff.m_idxListSubmittedJobs = idxList;
	}

	// Start thread if it wasn't already active. Do this down here due to the
	//  return 0 above us. Ie, don't start the thread unless we actually have work
	//  to do.
	if ( m_hWorkThread == NULL )
	{
		Assert( !m_bThreadShouldRun );
		m_bThreadShouldRun = true;
		m_hWorkThread = CreateSimpleThread( ThreadStubProcessMD5Requests, this );
	}

	// submit the work
	m_PendingJobs.PushItem( stuff );
	m_threadEventWorkToDo.Set();

	return idxList + 1;
}

//-----------------------------------------------------------------------------
// IsMD5RequestComplete
// is request identified by iRequest finished?
// ( the caller wants to free the memory, but now must wait until we finish
// calculating the MD5 )
//-----------------------------------------------------------------------------
bool CFileTracker2::IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{
	AUTO_LOCK( m_Mutex );
	int idxListWaiting = iRequest - 1;

	// deal with all completed jobs
	StuffToMD5_t stuff;
	while ( m_CompletedJobs.PopItem( &stuff ) )
	{
		int idxList = stuff.m_idxListSubmittedJobs;
		Q_memcpy( &m_SubmittedJobs[ idxList ].m_md5Value, &stuff.m_md5Value, sizeof( MD5Value_t ) );
		m_SubmittedJobs[ idxList ].m_bFinished = true;
	}

	// did the one we wanted finish?
	if ( m_SubmittedJobs[ idxListWaiting ].m_bFinished )
	{
		Q_memcpy( pMd5ValueOut, &m_SubmittedJobs[ idxListWaiting ].m_md5Value, sizeof( MD5Value_t ) );
		// you can not ask again, we have removed it from the list
		m_SubmittedJobs.Remove(idxListWaiting);
		return true;
	}

	// not done yet
	return false;
}

//-----------------------------------------------------------------------------
// BlockUntilMD5RequestComplete
// block until request identified by iRequest is finished
// ( the caller wants to free the memory, but now must wait until we finish
// calculating the MD5 )
//-----------------------------------------------------------------------------
bool CFileTracker2::BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut )
{
	while ( 1 )
	{
		if ( IsMD5RequestComplete( iRequest, pMd5ValueOut ) )
			return true;
		m_cThreadBlocks++;
		m_threadEventWorkCompleted.Wait( 1 );
	}
	return false;
}

#endif // SUPPORT_PACKED_STORE

CFileTracker2::CFileTracker2( CBaseFileSystem *pFileSystem ):
	m_treeAllOpenedFiles( TrackedFile_t::Less ),
	m_treeTrackedVPKFiles( TrackedVPKFile_t::Less )
{
#if defined( DEDICATED )
    Assert( 0 );
#endif

	m_pFileSystem = pFileSystem;

	m_cThreadBlocks = 0;
	m_cDupMD5s = 0;

#ifdef SUPPORT_PACKED_STORE
	m_bThreadShouldRun = false;
	m_hWorkThread = NULL;
#endif
}

CFileTracker2::~CFileTracker2()
{
#ifdef SUPPORT_PACKED_STORE
	Assert( !m_bThreadShouldRun );
	Assert( m_hWorkThread == NULL );
#endif
}

void CFileTracker2::ShutdownAsync()
{
#ifdef SUPPORT_PACKED_STORE
	m_bThreadShouldRun = false;
	m_threadEventWorkToDo.Set();
	// wait for it to die
	if ( m_hWorkThread )
	{
		ThreadJoin( m_hWorkThread );
		ReleaseThreadHandle( m_hWorkThread );
		m_hWorkThread = NULL;
	}
#endif
}

void CFileTracker2::MarkAllCRCsUnverified()
{
	// AUTO_LOCK( m_Mutex );
}

int CFileTracker2::GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles )
{
	return 0;
}

EFileCRCStatus CFileTracker2::CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
{
	Assert( ThreadInMainThread() );
	AUTO_LOCK( m_Mutex );

	TrackedFile_t trackedfileFind;
	trackedfileFind.RebuildFileName( m_stringPool, pRelativeFilename, pPathID, nFileFraction );

	int idx = m_treeAllOpenedFiles.Find( trackedfileFind );
	if ( idx != m_treeAllOpenedFiles.InvalidIndex() )
	{
		TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ idx ];

		if ( trackedfile.m_bFileInVPK )
		{
			// the FileHash is not meaningful, because the file is in a VPK, we have hashed the entire VPK
			// if the user is sending us a hash for this file, it means he has extracted it from the VPK and tricked the client into loading it
			// instead of the version in the VPK.
			return k_eFileCRCStatus_FileInVPK;
		}

		return k_eFileCRCStatus_CantOpenFile;
	}
	else
	{
		return k_eFileCRCStatus_CantOpenFile;
	}
}

void TrackedFile_t::RebuildFileName( CStringPool &stringPool, const char *pFilename, const char *pPathID, int nFileFraction )
{
	char szFixedName[ MAX_PATH ];
	char szPathName[ MAX_PATH ];

	V_strcpy_safe( szFixedName, pFilename );
	V_RemoveDotSlashes( szFixedName );
	V_FixSlashes( szFixedName );
	V_strlower( szFixedName ); // !KLUDGE!
	m_filename = stringPool.Allocate( szFixedName );

	V_strcpy_safe( szPathName, pPathID ? pPathID : "" );
	V_strupr( szPathName ); // !KLUDGE!
	m_path = stringPool.Allocate( szPathName );

	// CRC32_t crcFilename;
	// CRC32_Init( &crcFilename );
	// CRC32_ProcessBuffer( &crcFilename, m_filename, Q_strlen( m_filename ) );
	// CRC32_ProcessBuffer( &crcFilename, m_path, Q_strlen( m_path ) );
	// CRC32_Final( &crcFilename );

	// m_crcIdentifier = crcFilename;

	m_nFileFraction = nFileFraction;
}

#ifdef SUPPORT_PACKED_STORE

void CFileTracker2::NotePackFileAccess( const char *pFilename, const char *pPathID, int iSearchPathStoreId, CPackedStoreFileHandle &VPKHandle )
{
#if !defined( _GAMECONSOLE ) && !defined( DEDICATED )
	AUTO_LOCK( m_Mutex );
	Assert( iSearchPathStoreId > 0 );

	int idxFile = IdxFileFromName( pFilename, pPathID, 0, false );
	TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ idxFile ];

	// we could use the CRC data from the VPK header - and verify it
	// VPKHandle.GetFileCRCFromHeaderData();
	// for now all we are going to do is track that this file came from a VPK
	trackedfile.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedfile.m_nPackFileNumber = VPKHandle.m_nFileNumber; // this might be useful to send up
	trackedfile.m_iLoadedSearchPathStoreId = iSearchPathStoreId;
	trackedfile.m_bFileInVPK = true;
#endif // !defined( _GAMECONSOLE ) && !defined( DEDICATED )
}

#endif // SUPPORT_PACKED_STORE

struct FileListToUnloadForWhitelistChange : public IFileList
{
	virtual bool IsFileInList( const char *pFilename )
	{
		char szFixedName[ MAX_PATH ];
		GetFixedName( pFilename, szFixedName );
		return m_dictFiles.Find( szFixedName ) >= 0;
	}

	virtual void Release()
	{
		delete this;
	}

	void AddFile( const char *pszFilename )
	{
		char szFixedName[ MAX_PATH ];
		GetFixedName( pszFilename, szFixedName );
		if ( m_dictFiles.Find( szFixedName ) < 0 )
			m_dictFiles.Insert( szFixedName );
	}

	void GetFixedName( const char *pszFilename, char *pszFixedName )
	{
		V_strncpy( pszFixedName, pszFilename, MAX_PATH );
		V_strlower( pszFixedName );
		V_FixSlashes( pszFixedName );
	}

	CUtlDict<int> m_dictFiles;
};

IFileList *CFileTracker2::GetFilesToUnloadForWhitelistChange( IPureServerWhitelist *pNewWhiteList )
{
	FileListToUnloadForWhitelistChange *pResult = new FileListToUnloadForWhitelistChange;

	for ( int i = m_treeAllOpenedFiles.FirstInorder() ; i >= 0 ; i = m_treeAllOpenedFiles.NextInorder( i ) )
	{
		TrackedFile_t &f = m_treeAllOpenedFiles[i];

		// !KLUDGE! If we ignored it at all, just reload it.
		// This is more conservative than we need to be, but the set of files we are ignoring is probably
		// pretty small so it should be fine.
		if ( f.m_bIgnoredForPureServer )
		{
			f.m_bIgnoredForPureServer = false;
#ifdef PURE_SERVER_DEBUG_SPEW
			Msg( "%s was ignored for pure server purposes.  Queuing for reload\n", f.m_filename );
#endif
			pResult->AddFile( f.m_filename );
			continue;
		}

		if ( f.m_iLoadedSearchPathStoreId != 0 && pNewWhiteList && pNewWhiteList->GetFileClass( f.m_filename ) == ePureServerFileClass_AnyTrusted )
		{
			// Check if we loaded it from a path that no longer exists or is no longer trusted
			const CBaseFileSystem::CSearchPath *pSearchPath = m_pFileSystem->FindSearchPathByStoreId( f.m_iLoadedSearchPathStoreId );
			if ( pSearchPath == NULL )
			{
#ifdef PURE_SERVER_DEBUG_SPEW
				Msg( "%s was loaded from search path that's no longer mounted.  Queuing for reload\n", f.m_filename );
#endif
				pResult->AddFile( f.m_filename );
			}
			else if ( !pSearchPath->m_bIsTrustedForPureServer )
			{
#ifdef PURE_SERVER_DEBUG_SPEW
				Msg( "%s was loaded from search path that's not currently trusted.  Queuing for reload\n", f.m_filename );
#endif
				pResult->AddFile( f.m_filename );
			}
			else
			{
#if defined( _DEBUG ) && defined( PURE_SERVER_DEBUG_SPEW )
				Msg( "%s is OK.  Keeping\n", f.m_filename );
#endif
			}
		}
	}

	// Do we need to reload anything?
	if ( pResult->m_dictFiles.Count() > 0 )
		return pResult;

	// Nothing to reload, return an empty list as an optimization
	pResult->Release();
	return NULL;
}

#ifdef SUPPORT_PACKED_STORE

void CFileTracker2::AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &VPKHandle )
{
#if !defined( DEDICATED )
	AUTO_LOCK( m_Mutex );

	char szDataFileName[MAX_PATH];
	VPKHandle.m_nFileNumber = nPackFileNumber;
	VPKHandle.GetPackFileName( szDataFileName, sizeof(szDataFileName) );
	const char *pszFileName = V_GetFileName( szDataFileName );

	TrackedVPKFile_t trackedVPKFile;
	trackedVPKFile.m_nPackFileNumber = VPKHandle.m_nFileNumber;
	trackedVPKFile.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedVPKFile.m_nFileFraction = nFileFraction;
	trackedVPKFile.m_idxAllOpenedFiles = IdxFileFromName( pszFileName, "GAME", nFileFraction, true );

	m_treeTrackedVPKFiles.Insert( trackedVPKFile );

	TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ trackedVPKFile.m_idxAllOpenedFiles ];
    // These set in IdxFileFromName:
    //   trackedfile.m_crcIdentifier
    //   trackedfile.m_filename
    //   trackedfile.m_path
    //   trackedfile.m_bPackOrVPKFile
    //   trackedfile.m_nFileFraction
    // Not set:
    //   trackedfile.m_iLoadedSearchPathStoreId
    //   trackedfile.m_bIgnoredForPureServer
	trackedfile.m_bFileInVPK = false;
	trackedfile.m_bPackOrVPKFile = true;
	trackedfile.m_filehashFinal.m_cbFileLen = cbFileLen;
	trackedfile.m_filehashFinal.m_eFileHashType = FileHash_t::k_EFileHashTypeEntireFile;
	trackedfile.m_filehashFinal.m_nPackFileNumber = nPackFileNumber;
	trackedfile.m_filehashFinal.m_PackFileID = VPKHandle.m_pOwner->m_PackFileID;
	trackedfile.m_filehashFinal.m_crcIOSequence = cbFileLen;
	Q_memcpy( trackedfile.m_filehashFinal.m_md5contents.bits, md5.bits, sizeof( md5.bits) );
#endif // !DEDICATED
}

#endif // SUPPORT_PACKED_STORE

int CFileTracker2::IdxFileFromName( const char *pFilename, const char *pPathID, int nFileFraction, bool bPackOrVPKFile )
{
	TrackedFile_t trackedfile;

	trackedfile.RebuildFileName( m_stringPool, pFilename, pPathID, nFileFraction );
	trackedfile.m_bPackOrVPKFile = bPackOrVPKFile;

	int idxFile = m_treeAllOpenedFiles.Find( trackedfile );
	if ( idxFile == m_treeAllOpenedFiles.InvalidIndex() )
	{
		idxFile = m_treeAllOpenedFiles.Insert( trackedfile );
	}

	return idxFile;
}

#ifdef SUPPORT_PACKED_STORE

int CFileTracker2::NotePackFileOpened( const char *pVPKAbsPath, const char *pPathID, int64 nLength )
{
#if !defined( _GAMECONSOLE )
	AUTO_LOCK( m_Mutex );

	int idxFile = IdxFileFromName( pVPKAbsPath, pPathID, 0, true );

	TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ idxFile ];
	// we have the real name we want to use. correct the name
	trackedfile.m_bPackOrVPKFile = true;
	trackedfile.m_PackFileID = idxFile + 1;
	trackedfile.m_filehashFinal.m_PackFileID = trackedfile.m_PackFileID;
	trackedfile.m_filehashFinal.m_nPackFileNumber = -1;
	m_treeAllOpenedFiles.Reinsert( idxFile );
	return idxFile + 1;
#else
	return 0;
#endif
}

#endif // SUPPORT_PACKED_STORE

void CFileTracker2::NoteFileIgnoredForPureServer( const char *pFilename, const char *pPathID, int iSearchPathStoreId )
{
#if !defined( _GAMECONSOLE )
	AUTO_LOCK( m_Mutex );

	int idxFile = IdxFileFromName( pFilename, pPathID, 0, false );
	m_treeAllOpenedFiles[ idxFile ].m_bIgnoredForPureServer = true;
#endif
}

void CFileTracker2::NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, int iSearchPathStoreId, FILE *fp, int64 nLength )
{
#if !defined( _GAMECONSOLE ) && !defined( DEDICATED )
	AUTO_LOCK( m_Mutex );

	Assert( iSearchPathStoreId != 0 );

	int idxFile = IdxFileFromName( pFilename, pPathID, 0, false );
	TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ idxFile ];
	trackedfile.m_iLoadedSearchPathStoreId = iSearchPathStoreId;
#endif
}

void CFileTracker2::NoteFileUnloaded( const char *pFilename, const char *pPathID )
{
#if !defined( _GAMECONSOLE )
	AUTO_LOCK( m_Mutex );

	// Locate bookeeping entry, if any
	TrackedFile_t trackedfile;
	trackedfile.RebuildFileName( m_stringPool, pFilename, pPathID, 0 );

	int idxFile = m_treeAllOpenedFiles.Find( trackedfile );
	if ( idxFile >= 0 )
	{
		// Clear state
		TrackedFile_t &trackedfile = m_treeAllOpenedFiles[ idxFile ];
		trackedfile.m_iLoadedSearchPathStoreId = 0;
		trackedfile.m_bIgnoredForPureServer = false;
	}
#endif
}

int CFileTracker2::ListOpenedFiles( bool bAllOpened, const char *pchFilenameFind )
{
	AUTO_LOCK( m_Mutex );

	int i;
	int InvalidIndex;

	if ( bAllOpened )
	{
		i = m_treeAllOpenedFiles.FirstInorder();
		InvalidIndex = m_treeAllOpenedFiles.InvalidIndex();
	}
	else
	{
		i = m_treeTrackedVPKFiles.FirstInorder();
		InvalidIndex = m_treeTrackedVPKFiles.InvalidIndex();
	}

	Msg( "#, Path, FileName, (PackFileID, PackFileNumber), FileLen, FileFraction\n" );

	int count = 0;
	int cPackFiles = 0;
	while ( i != InvalidIndex )
	{
		int index = bAllOpened ? i : m_treeTrackedVPKFiles[ i ].m_idxAllOpenedFiles;
		TrackedFile_t &file = m_treeAllOpenedFiles[ index ];

		if ( file.m_PackFileID )
			cPackFiles++;
		if ( !pchFilenameFind ||
			Q_strstr( file.m_filename, pchFilenameFind ) ||
			Q_strstr( file.m_path, pchFilenameFind ) )
		{
			Msg( "%d %s %s ( %d, %d ) %d %d%s%s\n", 
				count, file.m_path, file.m_filename, file.m_PackFileID, file.m_nPackFileNumber,
				file.m_filehashFinal.m_cbFileLen, file.m_nFileFraction /*, file.m_crcIdentifier*/,
				file.m_bFileInVPK ? " (invpk)" : "",
				file.m_bPackOrVPKFile ? " (vpk)" : "");
		}

		i = bAllOpened ? m_treeAllOpenedFiles.NextInorder( i ) : m_treeTrackedVPKFiles.NextInorder( i );
		count++;
	}

	Msg( "cThreadedBlocks:%d cDupMD5s:%d\n", m_cThreadBlocks, m_cDupMD5s );
	Msg( "TrackedVPKFiles:%d AllOpenedFiles:%d files VPKfiles:%d StringPoolCount:%d\n",
		m_treeTrackedVPKFiles.Count(), m_treeAllOpenedFiles.Count(), cPackFiles, m_stringPool.Count() );
	return m_treeAllOpenedFiles.Count();
}

static void CC_TrackerListAllFiles( const CCommand &args )
{
	const char *pchFilenameFind = ( args.ArgC() >= 2 ) ? args[1] : NULL;
	BaseFileSystem()->m_FileTracker2.ListOpenedFiles( true, pchFilenameFind );
}
static ConCommand trackerlistallfiles( "trackerlistallfiles", CC_TrackerListAllFiles, "TrackerListAllFiles" );

static void CC_TrackerListVPKFiles( const CCommand &args )
{
	const char *pchFilenameFind = ( args.ArgC() >= 2 ) ? args[1] : NULL;
	BaseFileSystem()->m_FileTracker2.ListOpenedFiles( false, pchFilenameFind );
}
static ConCommand trackerlistvpkfiles( "trackerlistvpkfiles", CC_TrackerListVPKFiles, "TrackerListVPKFiles" );

#endif // !DEDICATED
