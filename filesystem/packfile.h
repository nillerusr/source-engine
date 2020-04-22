//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//

#ifndef PACKFILE_H
#define PACKFILE_H

#ifdef _WIN32
#pragma once
#endif

// How many bytes compressed filehandles should hold for seeking backwards. Seeks beyond this limit will require
// rewinding and restarting the compression, at significant performance penalty. (Warnings emitted when this occurs)
#define PACKFILE_COMPRESSED_FILEHANDLE_SEEK_BUFFER 4096

// How many bytes compressed filehandles should attempt to read (and cache) at a time from the underlying compressed data.
#define PACKFILE_COMPRESSED_FILEHANDLE_READ_BUFFER 4096

// Emit warnings in debug builds if we hold more than this many compressed file handles alive to alert to the poor
// memory characteristics.
#define PACKFILE_COMPRESSED_FILE_HANDLES_WARNING 20

#include "basefilesystem.h"
#include "tier1/refcount.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzmaDecoder.h"

class CPackFile;
class CZipPackFile;

// A pack file handle - essentially represents a file inside the pack file.
class CPackFileHandle
{
public:
	virtual ~CPackFileHandle() {};

	virtual int Read( void* pBuffer, int nDestSize, int nBytes ) = 0;
	virtual int Seek( int nOffset, int nWhence )                 = 0;
	virtual int Tell()                                           = 0;
	virtual int Size()                                           = 0;

	virtual void   SetBufferSize( int nBytes ) = 0;
	virtual int    GetSectorSize()             = 0;
	virtual int64  AbsoluteBaseOffset()        = 0;
};

class CZipPackFileHandle : public CPackFileHandle
{
public:
	CZipPackFileHandle( CZipPackFile* pOwner, int64 nBase, unsigned int nLength, unsigned int nIndex = -1, unsigned int nFilePointer = 0 );
	virtual ~CZipPackFileHandle();

	virtual int Read( void* pBuffer, int nDestSize, int nBytes ) OVERRIDE;
	virtual int Seek( int nOffset, int nWhence )                 OVERRIDE;

	virtual int Tell() OVERRIDE { return m_nFilePointer; };
	virtual int Size() OVERRIDE { return m_nLength; };

	virtual void   SetBufferSize( int nBytes ) OVERRIDE;
	virtual int    GetSectorSize()             OVERRIDE;
	virtual int64  AbsoluteBaseOffset()        OVERRIDE;

protected:
	int64         m_nBase;        // Base offset of the file inside the pack file.
	unsigned int  m_nFilePointer; // Current seek pointer (0 based from the beginning of the file).
	CZipPackFile* m_pOwner;       // Pack file that owns this handle
	unsigned int  m_nLength;      // Length of this file.
	unsigned int  m_nIndex;       // Index into the pack's directory table
};

class CLZMAZipPackFileHandle : public CZipPackFileHandle
{
public:
	CLZMAZipPackFileHandle( CZipPackFile* pOwner, int64 nBase, unsigned int nOriginalSize, unsigned int nCompressedSize,
	                        unsigned int nIndex = -1, unsigned int nFilePointer = 0 );
	~CLZMAZipPackFileHandle();

	virtual int Read( void* pBuffer, int nDestSize, int nBytes ) OVERRIDE;
	virtual int Seek( int nOffset, int nWhence )                 OVERRIDE;

	virtual int Tell() OVERRIDE;
	virtual int Size() OVERRIDE;

private:
	// Ensure there are bytes in the read buffer, assuming we're not at the end of the underlying data
	int FillReadBuffer();

	// Reset buffers and underlying seek position to 0
	void Reset();

	// Contains the last PACKFILE_COMPRESSED_FILEHANDLE_SEEK_BUFFER decompressed bytes. The Put and Get locations mimic our
	// filehandle -- TellPut() == TelGet() when we are not back seeking.
	CUtlBuffer    m_BackSeekBuffer;

	// The read buffer from the underlying compressed stream. We read PACKFILE_COMPRESSED_FILEHANDLE_READ_BUFFER bytes
	// into this buffer, then consume it via the buffer get position.
	CUtlBuffer    m_ReadBuffer;

	// The decompress stream we feed our base filehandle into
	CLZMAStream   *m_pLZMAStream;

	// Current seek position in uncompressed data
	int  m_nSeekPosition;

	// Size of the decompressed data
	unsigned int m_nOriginalSize;
};

//-----------------------------------------------------------------------------

// An abstract pack file
class CPackFile : public CRefCounted<CRefCountServiceMT>
{
public:
	CPackFile();
	virtual ~CPackFile();

	// The means by which you open files:
	virtual CFileHandle *OpenFile( const char *pFileName, const char *pOptions = "rb" ) = 0;

	// Check for existance in pack
	virtual bool ContainsFile( const char *pFileName ) = 0;

	// The two functions a pack file must provide
	virtual bool Prepare( int64 fileLen = -1, int64 nFileOfs = 0 ) = 0;

	// Returns the filename for a given file in the pack. Returns true if a filename is found, otherwise buffer is filled with "unknown"
	virtual bool IndexToFilename( int nIndex, char* buffer, int nBufferSize ) = 0;

	inline int GetSectorSize();

	virtual void SetupPreloadData() {}
	virtual void DiscardPreloadData() {}
	virtual int64 GetPackFileBaseOffset() = 0;

	CBaseFileSystem *FileSystem() { return m_fs; }

	// Helper for the filesystem's FindFirst/FindNext() API which mimics the old windows equivalent. pWildcard is the
	// same pattern that you would pass to FindFirst, not a true wildcard.
	// Mirrors the VPK code's similar call.
	virtual void GetFileAndDirLists( const char *pFindWildCard, CUtlStringList &outDirnames, CUtlStringList &outFilenames, bool bSortedOutput ) = 0;

	// Note: threading model for pack files assumes that data
	// is segmented into pack files that aggregate files
	// meant to be read in one thread. Performance characteristics
	// tuned for that case
	CThreadFastMutex	m_mutex;

	// Path management:
	void SetPath( const CUtlSymbol &path ) { m_Path = path; }
	const CUtlSymbol& GetPath() const	{ Assert( m_Path != UTL_INVAL_SYMBOL ); return m_Path; }
	CUtlSymbol			m_Path;

	// possibly embedded pack
	int64				m_nBaseOffset;

	CUtlString			m_ZipName;

	bool				m_bIsMapPath;
	long				m_lPackFileTime;

	int					m_refCount;
	int					m_nOpenFiles;

	FILE				*m_hPackFileHandleFS;
#if defined( SUPPORT_PACKED_STORE )
	CPackedStoreFileHandle m_hPackFileHandleVPK;
#endif
	bool				m_bIsExcluded;

	int					m_PackFileID;
protected:
	// This is the core IO routine for reading anything from a pack file, everything should go through here at some point
	virtual int ReadFromPack( int nIndex, void* buffer, int nDestBytes, int nBytes, int64 nOffset ) = 0;

	int64				m_FileLength;
	CBaseFileSystem		*m_fs;

	friend class		CPackFileHandle;
};

class CZipPackFile : public CPackFile
{
	friend class CZipPackFileHandle;
public:
	CZipPackFile( CBaseFileSystem* fs, void *pSection = NULL );
	virtual ~CZipPackFile();

	// Loads the pack file
	virtual bool Prepare( int64 fileLen = -1, int64 nFileOfs = 0 ) OVERRIDE;
	virtual bool ContainsFile( const char *pFileName ) OVERRIDE;
	virtual CFileHandle *OpenFile( const char *pFileName, const char *pOptions = "rb" ) OVERRIDE;

	virtual void GetFileAndDirLists( const char *pFindWildCard, CUtlStringList &outDirnames, CUtlStringList &outFilenames, bool bSortedOutput ) OVERRIDE;

	virtual int64 GetPackFileBaseOffset() OVERRIDE { return m_nBaseOffset; }

	virtual bool IndexToFilename( int nIndex, char *pBuffer, int nBufferSize ) OVERRIDE;

protected:
	virtual int  ReadFromPack( int nIndex, void* buffer, int nDestBytes, int nBytes, int64 nOffset  ) OVERRIDE;

	#pragma pack(1)

	typedef struct
	{
		char name[ 112 ];
		int64 filepos;
		int64 filelen;
	} packfile64_t;

	typedef struct
	{
		char id[ 4 ];
		int64 dirofs;
		int64 dirlen;
	} packheader64_t;

	typedef struct
	{
		char id[ 8 ];
		int64 packheaderpos;
		int64 originalfilesize;
	} packappenededheader_t;

	#pragma pack()

	// A Pack file directory entry:
	class CPackFileEntry
	{
	public:
		unsigned int		m_nPosition;
		unsigned int		m_nOriginalSize;
		unsigned int		m_nCompressedSize;
		unsigned int		m_HashName;
		unsigned short		m_nPreloadIdx;
		unsigned short		pad;
		unsigned short		m_nCompressionMethod;
		FileNameHandle_t	m_hFileName;
	};

	class CPackFileLessFunc
	{
	public:
		bool Less( CPackFileEntry const& src1, CPackFileEntry const& src2, void *pCtx );
	};

	// Find a file inside a pack file:
	const CPackFileEntry* FindFile( const char* pFileName );

	// Entries to the individual files stored inside the pack file.
	CUtlSortVector< CPackFileEntry, CPackFileLessFunc > m_PackFiles;

	bool						GetFileInfo( const char *pFileName, int &nBaseIndex, int64 &nFileOffset, int &nOriginalSize, int &nCompressedSize, unsigned short &nCompressionMethod );

	// Preload Support
	void						SetupPreloadData() OVERRIDE;
	void						DiscardPreloadData() OVERRIDE;
	ZIP_PreloadDirectoryEntry*	GetPreloadEntry( int nEntryIndex );

	int64						m_nPreloadSectionOffset;
	unsigned int				m_nPreloadSectionSize;
	ZIP_PreloadHeader			*m_pPreloadHeader;
	unsigned short*				m_pPreloadRemapTable;
	ZIP_PreloadDirectoryEntry	*m_pPreloadDirectory;
	void*						m_pPreloadData;
	CByteswap					m_swap;

#if defined ( _X360 )
	void						*m_pSection;
#endif
};

#endif // PACKFILE_H
