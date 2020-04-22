//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef FILETRACKER_H
#define FILETRACKER_H
#ifdef _WIN32
#pragma once
#endif

class CBaseFileSystem;
class CPackedStoreFileHandle;

#if !defined( DEDICATED )

// Comments from Fletcher:
// 
// TF isn’t sending any hashes to the server. (That’s probably what CSGO is doing,
// but not TF.) Comparing hashes doesn’t work when there are optional updates. (We
// release a new client without updating the server.) Also on TF, we don’t ship
// any textures or audio to the dedicated server.
//  
// On TF, the client just confirms that the files were loaded from a trusted source.
// 
// When a client connects to a “pure” server, the client is supposed to limit
// which modified files are allowed.)

#include "ifilelist.h"
#include "tier1/utldict.h"
#include "tier0/tslist.h"
#include "tier1/stringpool.h"

struct TrackedFile_t
{
	TrackedFile_t()
	{
		m_nFileFraction = 0;
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
		m_bPackOrVPKFile = false;
		m_bFileInVPK = false;
		m_iLoadedSearchPathStoreId = 0;
		m_bIgnoredForPureServer = false;
	}

	FileHash_t m_filehashFinal;

	const char *m_filename;
	const char *m_path;
	int m_nFileFraction;
	int m_iLoadedSearchPathStoreId; // ID of the search path that we loaded from.  Zero if we are not currently loaded.

	int m_PackFileID;
	int m_nPackFileNumber;
	bool m_bPackOrVPKFile;
	bool m_bFileInVPK;
	bool m_bIgnoredForPureServer; // Did we ignore a file by this name as a result of pure server rules, the last time it was opened?

	// The crcIdentifier is a CRC32 of the filename and path. It could be used for quick comparisons of
	//  path+filename, but since we don't do that at the moment we don't need this.
	// CRC32_t m_crcIdentifier;

	void RebuildFileName( CStringPool &stringPool, const char *pFilename, const char *pPathID, int nFileFraction );

	static bool Less( const TrackedFile_t& lhs, const TrackedFile_t& rhs )
	{
		int nCmp = Q_strcmp( lhs.m_path, rhs.m_path );
		if ( nCmp < 0 )
			return true;
		else if ( nCmp > 0 )
			return false;

		nCmp = Q_strcmp( lhs.m_filename, rhs.m_filename );
		if ( nCmp < 0 )
			return true;

		return false;
	}
};

struct TrackedVPKFile_t
{
	TrackedVPKFile_t()
	{
		m_PackFileID = 0;
		m_nPackFileNumber = 0;
		m_nFileFraction = 0;
	}
	int m_PackFileID;
	int m_nPackFileNumber;
	int m_nFileFraction;
	int m_idxAllOpenedFiles; // Index into m_treeAllOpenedFiles

	static bool Less( const TrackedVPKFile_t& lhs, const TrackedVPKFile_t& rhs )
	{
		if ( lhs.m_nPackFileNumber < rhs.m_nPackFileNumber )
			return true;
		else if ( lhs.m_nPackFileNumber > rhs.m_nPackFileNumber )
			return false;

		if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
			return true;
		else if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
			return false;

		if ( lhs.m_PackFileID < rhs.m_PackFileID )
			return true;

		return false;
	}
};

class StuffToMD5_t
{
public:
	uint8 *m_pubBuffer;
	int m_cubBuffer;
	MD5Value_t m_md5Value;
    int m_idxTrackedVPKFile;
	int m_idxListSubmittedJobs;
};

class SubmittedMd5Job_t
{
public:
	bool m_bFinished;
	MD5Value_t m_md5Value;
};

class CFileTracker2
#ifdef SUPPORT_PACKED_STORE
	: IThreadedFileMD5Processor
#endif
{
public:
	CFileTracker2( CBaseFileSystem *pFileSystem );
	~CFileTracker2();

	void ShutdownAsync();

	void MarkAllCRCsUnverified();
	int	GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles );
	EFileCRCStatus CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash );

#ifdef SUPPORT_PACKED_STORE
	unsigned ThreadedProcessMD5Requests();
	virtual int SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction );
	virtual bool BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );
	virtual bool IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut );

	int NotePackFileOpened( const char *pVPKAbsPath, const char *pPathID, int64 nLength );
	void NotePackFileAccess( const char *pFilename, const char *pPathID, int iSearchPathStoreId, CPackedStoreFileHandle &VPKHandle );
	void AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &fhandle );
#endif

	void NoteFileIgnoredForPureServer( const char *pFilename, const char *pPathID, int iSearchPathStoreId );
	void NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, int iSearchPathStoreId, FILE *fp, int64 nLength );
	void NoteFileUnloaded( const char *pFilename, const char *pPathID );
	int ListOpenedFiles( bool bAllOpened, const char *pchFilenameFind  );

	IFileList *GetFilesToUnloadForWhitelistChange( IPureServerWhitelist *pNewWhiteList );

private:
	int IdxFileFromName( const char *pFilename, const char *pPathID, int nFileFraction, bool bPackOrVPKFile );

	CStringPool m_stringPool;
	CUtlRBTree< TrackedFile_t, int > m_treeAllOpenedFiles;
	CUtlRBTree< TrackedVPKFile_t, int > m_treeTrackedVPKFiles;

	CBaseFileSystem					*m_pFileSystem;
	CThreadMutex					m_Mutex;	// Threads call into here, so we need to be safe.

#ifdef SUPPORT_PACKED_STORE
	CThreadEvent m_threadEventWorkToDo;
	CThreadEvent m_threadEventWorkCompleted;
	volatile bool m_bThreadShouldRun;
	ThreadHandle_t m_hWorkThread;

	CTSQueue< StuffToMD5_t >				m_PendingJobs;
	CTSQueue< StuffToMD5_t >				m_CompletedJobs;
	CUtlLinkedList< SubmittedMd5Job_t >		m_SubmittedJobs;
#endif // SUPPORT_PACKED_STORE

	// Stats
	int m_cThreadBlocks;
    int m_cDupMD5s;
};

#else

//
// Dedicated server NULL filetracker. Pretty much does nothing.
//
class CFileTracker2
#ifdef SUPPORT_PACKED_STORE
	: IThreadedFileMD5Processor
#endif
{
public:
	CFileTracker2( CBaseFileSystem *pFileSystem ) {}
	~CFileTracker2() {}

	void ShutdownAsync() {}

	void MarkAllCRCsUnverified() {}
	int	GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles ) { return 0; }
	EFileCRCStatus CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
		{ return k_eFileCRCStatus_CantOpenFile; }

#ifdef SUPPORT_PACKED_STORE
	virtual int SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction )
		{ return 0; }
	virtual bool BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut ) { Assert(0); return true; }
	virtual bool IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut ) { Assert(0); return true; }

	int NotePackFileOpened( const char *pVPKAbsPath, const char *pPathID, int64 nLength ) { return 0; }
	void NotePackFileAccess( const char *pFilename, const char *pPathID, int iSearchPathStoreId, CPackedStoreFileHandle &VPKHandle ) {}
	void AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &fhandle ) {}
#endif

	void NoteFileIgnoredForPureServer( const char *pFilename, const char *pPathID, int iSearchPathStoreId ) {}
	void NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, int iSearchPathStoreId, FILE *fp, int64 nLength ) {}
	void NoteFileUnloaded( const char *pFilename, const char *pPathID ) {}

	IFileList *GetFilesToUnloadForWhitelistChange( IPureServerWhitelist *pNewWhiteList ) { return NULL; }
};

#endif // DEDICATED

#endif // FILETRACKER_H
