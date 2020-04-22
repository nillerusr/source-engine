//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//

#include "packfile.h"
#include "zip_utils.h"
#include "tier0/basetypes.h"
#include "tier1/convar.h"
#include "tier1/lzmaDecoder.h"
#include "tier1/utlbuffer.h"
#include "tier1/generichash.h"

ConVar fs_monitor_read_from_pack( "fs_monitor_read_from_pack", "0", 0, "0:Off, 1:Any, 2:Sync only" );

// How many bytes we should decode at a time when doing pseudo-reads to seek forward in a compressed file handle,
// (affects maximum stack allocation by a forward seek)
#define COMPRESSED_SEEK_READ_CHUNK 1024

CPackFile::CPackFile()
{
	m_FileLength = 0;
	m_hPackFileHandleFS = NULL;
	m_fs = NULL;
	m_nBaseOffset = 0;
	m_bIsMapPath = false;
	m_lPackFileTime = 0L;
	m_refCount = 0;
	m_nOpenFiles = 0;
	m_PackFileID = 0;
}

CPackFile::~CPackFile()
{
	if ( m_nOpenFiles )
	{
		Error( "Closing pack file with %d open files!\n", m_nOpenFiles );
	}

	if ( m_hPackFileHandleFS )
	{
		m_fs->FS_fclose( m_hPackFileHandleFS );
		m_hPackFileHandleFS = NULL;
	}

	m_fs->m_ZipFiles.FindAndRemove( this );
}

int CPackFile::GetSectorSize()
{
	if ( m_hPackFileHandleFS )
	{
		return m_fs->FS_GetSectorSize( m_hPackFileHandleFS );
	}
#if defined( SUPPORT_PACKED_STORE )
	else if ( m_hPackFileHandleVPK )
	{
		return 2048;
	}
#endif
	else
	{
		return -1;
	}
}

// Read a bit of the file from the pack file:
int CZipPackFileHandle::Read( void* pBuffer, int nDestSize, int nBytes )
{
	// Clamp nBytes to not go past the end of the file (async is still possible due to nDestSize)
	if ( nBytes + m_nFilePointer > m_nLength )
	{
		nBytes = m_nLength - m_nFilePointer;
	}

	// Seek to the given file pointer and read
	int nBytesRead = m_pOwner->ReadFromPack( m_nIndex, pBuffer, nDestSize, nBytes, m_nBase + m_nFilePointer );

	m_nFilePointer += nBytesRead;

	return nBytesRead;
}

// Seek around inside the pack:
int CZipPackFileHandle::Seek( int nOffset, int nWhence )
{
	if ( nWhence == SEEK_SET )
	{
		m_nFilePointer = nOffset;
	}
	else if ( nWhence == SEEK_CUR )
	{
		m_nFilePointer += nOffset;
	}
	else if ( nWhence == SEEK_END )
	{
		m_nFilePointer = m_nLength + nOffset;
	}

	// Clamp the file pointer to the actual bounds of the file:
	if ( m_nFilePointer > m_nLength )
	{
		m_nFilePointer = m_nLength;
	}

	return m_nFilePointer;
}

//-----------------------------------------------------------------------------
// Open a file inside of a pack file.
//-----------------------------------------------------------------------------
CFileHandle *CZipPackFile::OpenFile( const char *pFileName, const char *pOptions )
{
	int nIndex, nOriginalSize, nCompressedSize;
	int64 nPosition;
	unsigned short nCompressionMethod;

	// find the file's location in the pack
	if ( GetFileInfo( pFileName, nIndex, nPosition, nOriginalSize, nCompressedSize, nCompressionMethod ) )
	{
		m_mutex.Lock();
#if defined( SUPPORT_PACKED_STORE )
		if ( m_nOpenFiles == 0 && m_hPackFileHandleFS == NULL && !m_hPackFileHandleVPK )
#else
		if ( m_nOpenFiles == 0 && m_hPackFileHandleFS == NULL )
#endif
		{
			// Try to open it as a regular file first
			m_hPackFileHandleFS = m_fs->Trace_FOpen( m_ZipName, "rb", 0, NULL );

			// !NOTE! Pack files inside of VPK not supported
		}
		m_nOpenFiles++;
		m_mutex.Unlock();
		CPackFileHandle* ph = NULL;
		if ( nCompressionMethod == ZIP_COMPRESSION_LZMA )
		{
			ph = new CLZMAZipPackFileHandle( this, nPosition, nOriginalSize, nCompressedSize, nIndex );
		}
		else
		{
			AssertMsg( nCompressionMethod == ZIP_COMPRESSION_NONE, "Unsupported compression type in zip pack file" );
			ph = new CZipPackFileHandle( this, nPosition, nOriginalSize, nIndex );
		}
		CFileHandle *fh = new CFileHandle( m_fs );
		fh->m_pPackFileHandle = ph;
		fh->m_nLength = nOriginalSize;

		// The default mode for fopen is text, so require 'b' for binary
		if ( strstr( pOptions, "b" ) == NULL )
		{
			fh->m_type = FT_PACK_TEXT;
		}
		else
		{
			fh->m_type = FT_PACK_BINARY;
		}

#if !defined( _RETAIL )
		fh->SetName( pFileName );
#endif
		return fh;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//	Get a directory entry from a pack's preload section
//-----------------------------------------------------------------------------
ZIP_PreloadDirectoryEntry* CZipPackFile::GetPreloadEntry( int nEntryIndex )
{
	if ( !m_pPreloadHeader )
	{
		return NULL;
	}

	// If this entry doesn't have a corresponding preload entry, fail.
	if ( m_PackFiles[nEntryIndex].m_nPreloadIdx == INVALID_PRELOAD_ENTRY )
	{
		return NULL;
	}

	return &m_pPreloadDirectory[m_PackFiles[nEntryIndex].m_nPreloadIdx];
}

//-----------------------------------------------------------------------------
//	Read a file from the pack
//-----------------------------------------------------------------------------
int CZipPackFile::ReadFromPack( int nEntryIndex, void* pBuffer, int nDestBytes, int nBytes, int64 nOffset )
{
	if ( nEntryIndex >= 0 )
	{
		if ( nBytes <= 0 )
		{
			return 0;
		}

		// X360TBD: This is screwy, it works because m_nBaseOffset is 0 for preload capable zips
		// It comes into play for files out of the embedded bsp zip,
		// this hackery is a pre-bias expecting ReadFromPack() do a symmetric post bias, yuck.

		// Attempt to satisfy request from possible preload section, otherwise fall through
		// A preload entry may be compressed
		ZIP_PreloadDirectoryEntry *pPreloadEntry = GetPreloadEntry( nEntryIndex );
		if ( pPreloadEntry )
		{
			// convert the absolute pack file position to a local file position
			int nLocalOffset = nOffset - m_PackFiles[nEntryIndex].m_nPosition;
			byte *pPreloadData = (byte*)m_pPreloadData + pPreloadEntry->DataOffset;

			if ( CLZMA::IsCompressed( pPreloadData ) )
			{
				unsigned int actualSize = CLZMA::GetActualSize( pPreloadData );
				if ( nLocalOffset + nBytes <= (int)actualSize )
				{
					// satisfy from compressed preload
					if ( fs_monitor_read_from_pack.GetInt() == 1 )
					{
						char szName[MAX_PATH];
						IndexToFilename( nEntryIndex, szName, sizeof( szName ) );
						Msg( "Read From Pack: [Preload] Requested:%d, Compressed:%d, %s\n", nBytes, pPreloadEntry->Length, szName );
					}

					if ( nLocalOffset == 0 && nDestBytes >= (int)actualSize && nBytes == (int)actualSize )
					{
						// uncompress directly into caller's buffer
						CLZMA::Uncompress( (unsigned char *)pPreloadData, (unsigned char *)pBuffer );
						return nBytes;
					}

					// uncompress into temporary memory
					CUtlMemory< byte > tempMemory;
					tempMemory.EnsureCapacity( actualSize );
					CLZMA::Uncompress( pPreloadData, tempMemory.Base() );
					// copy only what caller expects
					V_memcpy( pBuffer, (byte*)tempMemory.Base() + nLocalOffset, nBytes );
					return nBytes;
				}
			}
			else if ( nLocalOffset + nBytes <= (int)pPreloadEntry->Length )
			{
				// satisfy from uncompressed preload
				if ( fs_monitor_read_from_pack.GetInt() == 1 )
				{
					char szName[MAX_PATH];
					IndexToFilename( nEntryIndex, szName, sizeof( szName ) );
					Msg( "Read From Pack: [Preload] Requested:%d, Total:%d, %s\n", nBytes, pPreloadEntry->Length, szName );
				}

				V_memcpy( pBuffer, pPreloadData + nLocalOffset, nBytes );
				return nBytes;
			}
		}
	}

#if defined ( _X360 )
	// fell through as a direct request from within the pack
	// intercept to possible embedded section
	if ( m_pSection )
	{
		// a section is a special update zip that has no files, only preload
		// it has to be in the section
		V_memcpy( pBuffer, (byte*)m_pSection + nOffset, nBytes );
		return nBytes;
	}
#endif

	// Otherwise, do the read from the pack
	m_mutex.Lock();

	if ( fs_monitor_read_from_pack.GetInt() == 1 || ( fs_monitor_read_from_pack.GetInt() == 2 && ThreadInMainThread() ) )
	{
		// spew info about real i/o request
		char szName[MAX_PATH];
		IndexToFilename( nEntryIndex, szName, sizeof( szName ) );
		Msg( "Read From Pack: Sync I/O: Requested:%7d, Offset:0x%16.16llx, %s\n", nBytes, m_nBaseOffset + nOffset, szName );
	}

	int nBytesRead = 0;
	// Seek to the start of the read area and perform the read: TODO: CHANGE THIS INTO A CFileHandle
	if ( m_hPackFileHandleFS )
	{
		m_fs->FS_fseek( m_hPackFileHandleFS, m_nBaseOffset + nOffset, SEEK_SET );
		nBytesRead = m_fs->FS_fread( pBuffer, nDestBytes, nBytes, m_hPackFileHandleFS );
	}
#if defined( SUPPORT_PACKED_STORE )
	else
	{
		// We're a packfile embedded in a VPK
		m_hPackFileHandleVPK.Seek( m_nBaseOffset + nOffset, FILESYSTEM_SEEK_HEAD );
		nBytesRead = m_hPackFileHandleVPK.Read( pBuffer, nBytes );
	}
#endif
	m_mutex.Unlock();

	return nBytesRead;
}

//-----------------------------------------------------------------------------
//	Gets size, position, and index for a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::GetFileInfo( const char *pFileName, int &nBaseIndex, int64 &nFileOffset, int &nOriginalSize, int &nCompressedSize, unsigned short &nCompressionMethod )
{
	char szCleanName[MAX_FILEPATH];
	Q_strncpy( szCleanName, pFileName, sizeof( szCleanName ) );
#ifdef _WIN32
	Q_strlower( szCleanName );
#endif
	Q_FixSlashes( szCleanName );

	if ( !Q_RemoveDotSlashes( szCleanName, CORRECT_PATH_SEPARATOR, false ) )
	{
		return false;
	}

	CZipPackFile::CPackFileEntry lookup;

	// We may get passed non-canonicalized filenames, so we need to remove the ../ from the path
	char szFixedName[MAX_PATH] = {0};
	V_strcpy_safe( szFixedName, pFileName );
	V_RemoveDotSlashes( szFixedName );

	lookup.m_HashName = HashStringCaselessConventional( szFixedName );

	int idx = m_PackFiles.Find( lookup );
	if ( -1 != idx  )
	{
		nFileOffset = m_PackFiles[idx].m_nPosition;
		nOriginalSize = m_PackFiles[idx].m_nOriginalSize;
		nCompressedSize = m_PackFiles[idx].m_nCompressedSize;
		nBaseIndex = idx;
		nCompressionMethod = m_PackFiles[idx].m_nCompressionMethod;
		return true;
	}

	return false;
}

bool CZipPackFile::IndexToFilename( int nIndex, char *pBuffer, int nBufferSize )
{
	AssertMsg( nIndex >= 0 && nIndex < m_PackFiles.Count(), "Out of bounds vector access in IndexToFilename" );
	if ( nIndex >= 0 )
	{
		m_fs->String( m_PackFiles[nIndex].m_hFileName, pBuffer, nBufferSize );
		return true;
	}

	Q_strncpy( pBuffer, "unknown", nBufferSize );

	return false;
}

//-----------------------------------------------------------------------------
//	Find a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::ContainsFile( const char *pFileName )
{
	int nIndex, nOriginalSize, nCompressedSize;
	int64 nOffset;
	unsigned short nCompressionMethod;
	bool bFound = GetFileInfo( pFileName, nIndex, nOffset, nOriginalSize, nCompressedSize, nCompressionMethod );
	return bFound;
}

//-----------------------------------------------------------------------------
//	Build a list of matching files and directories given a FindFirst() style wildcard
//-----------------------------------------------------------------------------
void CZipPackFile::GetFileAndDirLists( const char *pRawWildCard, CUtlStringList &outDirnames, CUtlStringList &outFilenames, bool bSortedOutput )
{
	// See also: VPKlib function with same name.

	CUtlDict<int,int> AddedDirectories; // Used to remove duplicate paths

	char szWildCard[MAX_PATH] = { 0 };
	char szWildCardPath[MAX_PATH] = { 0 };
	char szWildCardBase[MAX_PATH] = { 0 };
	char szWildCardExt[MAX_PATH] = { 0 };

	size_t nLenWildcardPath = 0;
	size_t nLenWildcardBase = 0;

	bool bBaseWildcard = true;
	bool bExtWildcard = true;

	//
	// Parse the wildcard string into a base and extension used for string comparisons
	//
	V_strncpy( szWildCard, pRawWildCard, sizeof( szWildCard ) );
	V_FixSlashes( szWildCard, '/' );
	V_RemoveDotSlashes( szWildCard, '/', /* bRemoveDoubleSlashes */ true );

	// Workaround edge case in crappy path code. ExtractFilePath extracts a/b/ from a/b/c/ but FileBase would return the empty string.
	size_t nLenWildCard = V_strlen( szWildCard );
	if ( nLenWildCard && szWildCard[ nLenWildCard - 1 ] == '/' )
	{
		V_strncpy( szWildCardPath, szWildCard, sizeof( szWildCardPath ) );
	}
	else
	{
		V_ExtractFilePath( szWildCard, szWildCardPath, sizeof( szWildCardPath ) );
	}

	V_FileBase( szWildCard, szWildCardBase, sizeof( szWildCardBase ) );
	bool bWildcardHasExt = !!V_strrchr( szWildCard, '.' );
	V_ExtractFileExtension( szWildCard, szWildCardExt, sizeof( szWildCardExt ) );

	// From the pattern, we now have the directory path up to the file pattern, the filename base, and the filename
	// extension.

	// We don't support partial wildcards here (foo*bar.*). This support is massively inconsistent in our codebase and
	// there's no one point where we implement it, so rather than trying to match one of our broken implementations
	// (windows stdio is the only one I could find that was actually right), I'm going with "you shouldn't use this API
	// for that".
	bBaseWildcard = ( V_strcmp( szWildCardBase, "*" ) == 0 );
	bExtWildcard = ( V_strcmp( szWildCardExt, "*" ) == 0 );

	if ( !bWildcardHasExt && bBaseWildcard )
	{
		// For the special case of just '*' (and not, e.g., '*.') match '*.*'
		bExtWildcard = true;
	}

	nLenWildcardPath = V_strlen( szWildCardPath );
	nLenWildcardBase = V_strlen( szWildCardBase );

	// Generate the list of directories and files that match the wildcard
	//

	// For each candidate we attempt to walk up its path and consider the directories it represents as well (the
	// directories in a zip only exist in that files contain them, there are no empty directories)
	FOR_EACH_VEC( m_PackFiles, filesIdx )
	{
		char szCandidateName[MAX_PATH] = { 0 };
		IndexToFilename( filesIdx, szCandidateName, sizeof( szCandidateName ));

		if ( !szCandidateName[0] )
		{
			continue;
		}

		// Check if this file starts with the wildcard selector's path.
		// Note that we only ensure the prefix is the same. There are no specific entries for directories in a zip, they
		// only exist in that files in the zip reference them, so handle subdirectory matches from filenames as well.
		CUtlDict<int,int> ConsideredDirectories; // Will have duplicate directory matches when multiple files reside in them
		if  ( ( nLenWildcardPath && ( 0 == V_strnicmp( szCandidateName, szWildCardPath, nLenWildcardPath ) ) )
		      || ( !nLenWildcardPath && strchr( szCandidateName, '/' ) ) )
		{
			// Check if we matched because of a sub-directory, e.g. a/b/*.* would match /a/b/c/d/foo (in which case we
			// want to add /a/b/c to the matched directories list, ignoring the actual specific file)
			char szCandidateBaseName[MAX_PATH] = { 0 };
			bool bIsDir = false;
			size_t nSubDirLen = 0;
			char *pSubDirSlash = strchr( szCandidateName + nLenWildcardPath, '/' );
			if ( pSubDirSlash )
			{
				// This is a subdirectory match, drop everything after it and continue with it as the filename
				nSubDirLen = (size_t)( (ptrdiff_t)pSubDirSlash - (ptrdiff_t)( szCandidateName + nLenWildcardPath ) );
				V_strncpy( szCandidateBaseName, szCandidateName + nLenWildcardPath, nSubDirLen + 1 );
				bIsDir = true;

				// Early out if we already considered this exact directory from another file
				if ( ConsideredDirectories.Find( szCandidateBaseName ) != ConsideredDirectories.InvalidIndex() )
				{
					continue;
				}

				ConsideredDirectories.Insert( szCandidateBaseName, 0 );
			}
			else
			{
				V_strncpy( szCandidateBaseName, szCandidateName + nLenWildcardPath, sizeof( szCandidateBaseName ) );
			}

			char *pExt = strchr( szCandidateBaseName, '.' );
			if ( pExt )
			{
				// Null out the . and move to point to the extension
				*pExt = '\0';
				pExt++;
			}

			// Determine if this file matches the wildcart (*.*, *.ext, ext.*)
			bool bBaseMatch = false;
			bool bExtMatch = false;

			// If we have a base dir name, and we have a szWildCardBase to match against
			if ( bBaseWildcard )
				bBaseMatch = true;  // The base is the wildCard ("*"), so whatever we have as the base matches
			else
				bBaseMatch = ( 0 == V_stricmp( szCandidateBaseName, szWildCardBase ) );

			// If we have an extension and we have a szWildCardExtension to mach against
			if ( ( bExtWildcard && pExt ) || ( !pExt && !bWildcardHasExt ) )
				bExtMatch = true;
			else
				bExtMatch = bWildcardHasExt && pExt && ( 0 == V_stricmp( pExt, szWildCardExt ) );

			// If both parts match, then add it to the list
			if ( bBaseMatch && bExtMatch )
			{
				if ( bIsDir )
				{
					// Pull up to the subdir we considered out of szCandidateName
					size_t nMatchSize = nLenWildcardPath + nSubDirLen + 1;
					char *pszFullMatch = new char[ nMatchSize ];
					V_strncpy( pszFullMatch, szCandidateName, nMatchSize );
					outDirnames.AddToTail( pszFullMatch );
				}
				else
				{
					size_t nMatchSize = V_strlen( szCandidateName ) + 1;
					char *pszFullMatch = new char[ nMatchSize ];
					V_strncpy( pszFullMatch, szCandidateName, nMatchSize );
					outFilenames.AddToTail( pszFullMatch );
				}
			}
		}
	}

	// Sort the output if requested
	if ( bSortedOutput )
	{
		outDirnames.Sort( &CUtlStringList::SortFunc );
		outFilenames.Sort( &CUtlStringList::SortFunc );
	}
}


//-----------------------------------------------------------------------------
//	Set up the preload section
//-----------------------------------------------------------------------------
void CZipPackFile::SetupPreloadData()
{
	if ( m_pPreloadHeader || !m_nPreloadSectionSize )
	{
		// already loaded or not available
		return;
	}

	MEM_ALLOC_CREDIT_( "xZip" );

	void *pPreload;
#if defined ( _X360 )
	if ( m_pSection )
	{
		pPreload = (byte*)m_pSection + m_nPreloadSectionOffset;
	}
	else
#endif
	{
		pPreload = malloc( m_nPreloadSectionSize );
		if ( !pPreload )
		{
			return;
		}

		if ( IsX360() )
		{
			// 360 XZips are always dvd aligned
			Assert( ( m_nPreloadSectionSize % XBOX_DVD_SECTORSIZE ) == 0 );
			Assert( ( m_nPreloadSectionOffset % XBOX_DVD_SECTORSIZE ) == 0 );
		}

		// preload data is loaded as a single unbuffered i/o operation
		ReadFromPack( -1, pPreload, -1, m_nPreloadSectionSize, m_nPreloadSectionOffset );
	}

	// setup the header
	m_pPreloadHeader = (ZIP_PreloadHeader *)pPreload;

	// setup the preload directory
	m_pPreloadDirectory = (ZIP_PreloadDirectoryEntry *)((byte *)m_pPreloadHeader + sizeof( ZIP_PreloadHeader ) );

	// setup the remap table
	m_pPreloadRemapTable = (unsigned short *)((byte *)m_pPreloadDirectory + m_pPreloadHeader->PreloadDirectoryEntries * sizeof( ZIP_PreloadDirectoryEntry ) );

	// set the preload data base
	m_pPreloadData = (byte *)m_pPreloadRemapTable + m_pPreloadHeader->DirectoryEntries * sizeof( unsigned short );
}

void CZipPackFile::DiscardPreloadData()
{
	if ( !m_pPreloadHeader )
	{
		// already discarded
		return;
	}

#if defined ( _X360 )
	// a section is an alias, the header becomes an alias, not owned memory
	if ( !m_pSection )
	{
		free( m_pPreloadHeader );
	}
#else
	free( m_pPreloadHeader );
#endif
	m_pPreloadHeader = NULL;
}

//-----------------------------------------------------------------------------
//	Parse the zip file to build the file directory and preload section
//-----------------------------------------------------------------------------
bool CZipPackFile::Prepare( int64 fileLen, int64 nFileOfs )
{
	if ( !fileLen || fileLen < sizeof( ZIP_EndOfCentralDirRecord ) )
	{
		// nonsense zip
		return false;
	}

	// Pack files are always little-endian
	m_swap.ActivateByteSwapping( IsX360() );

	m_FileLength = fileLen;
	m_nBaseOffset = nFileOfs;

	ZIP_EndOfCentralDirRecord rec = { 0 };

	// Find and read the central header directory from its expected position at end of the file
	bool bCentralDirRecord = false;
	int64 offset = fileLen - sizeof( ZIP_EndOfCentralDirRecord );

	// 360 can have an incompatible format
	bool bCompatibleFormat = true;
	if ( IsX360() )
	{
		// 360 has dependable exact zips, backup to handle possible xzip format
		if ( offset - XZIP_COMMENT_LENGTH >= 0 )
		{
			offset -= XZIP_COMMENT_LENGTH;
		}

		// single i/o operation, scanning forward
		char *pTemp = (char *)_alloca( fileLen - offset );
		ReadFromPack( -1, pTemp, -1, fileLen - offset, offset );
		while ( offset <= (int64)(fileLen - sizeof( ZIP_EndOfCentralDirRecord )) )
		{
			memcpy( &rec, pTemp, sizeof( ZIP_EndOfCentralDirRecord ) );
			m_swap.SwapFieldsToTargetEndian( &rec );
			if ( rec.signature == PKID( 5, 6 ) )
			{
				bCentralDirRecord = true;
				if ( rec.commentLength >= 4 )
				{
					char *pComment = pTemp + sizeof( ZIP_EndOfCentralDirRecord );
					if ( !V_strnicmp( pComment, "XZP2", 4 ) )
					{
						bCompatibleFormat = false;
					}
				}
				break;
			}
			offset++;
			pTemp++;
		}
	}
	else
	{
		// scan entire file from expected location for central dir
		for ( ; offset >= 0; offset-- )
		{
			ReadFromPack( -1, (void*)&rec, -1, sizeof( rec ), offset );
			m_swap.SwapFieldsToTargetEndian( &rec );
			if ( rec.signature == PKID( 5, 6 ) )
			{
				bCentralDirRecord = true;
				break;
			}
		}
	}
	Assert( bCentralDirRecord );
	if ( !bCentralDirRecord )
	{
		// no zip directory, bad zip
		return false;
	}

	int numFilesInZip = rec.nCentralDirectoryEntries_Total;
	if ( numFilesInZip <= 0 )
	{
		// empty valid zip
		return true;
	}

	int firstFileIdx = 0;

	MEM_ALLOC_CREDIT();

	// read central directory into memory and parse
	CUtlBuffer zipDirBuff( 0, rec.centralDirectorySize, 0 );
	zipDirBuff.EnsureCapacity( rec.centralDirectorySize );
	zipDirBuff.ActivateByteSwapping( IsX360() );
	ReadFromPack( -1, zipDirBuff.Base(), -1, rec.centralDirectorySize, rec.startOfCentralDirOffset );
	zipDirBuff.SeekPut( CUtlBuffer::SEEK_HEAD, rec.centralDirectorySize );

	ZIP_FileHeader zipFileHeader;
	char filename[MAX_PATH] = { 0 };

	// Check for a preload section, expected to be the first file in the zip
	zipDirBuff.GetObjects( &zipFileHeader );
	zipDirBuff.Get( filename, Min( (size_t)zipFileHeader.fileNameLength, sizeof(filename) - 1 ) );
	if ( !V_stricmp( filename, PRELOAD_SECTION_NAME ) )
	{
		m_nPreloadSectionSize = zipFileHeader.uncompressedSize;
		m_nPreloadSectionOffset = zipFileHeader.relativeOffsetOfLocalHeader +
						  sizeof( ZIP_LocalFileHeader ) +
						  zipFileHeader.fileNameLength +
						  zipFileHeader.extraFieldLength;
		SetupPreloadData();

		// Set up to extract the remaining files
		int nextOffset = bCompatibleFormat ? zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength : 0;
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_CURRENT, nextOffset );
		firstFileIdx = 1;
	}
	else
	{
		if ( IsX360() )
		{
			// all 360 zip files are expected to have preload sections
			// only during development, maps are allowed to lack them, due to auto-conversion
			if ( !m_bIsMapPath || g_pFullFileSystem->GetDVDMode() == DVDMODE_STRICT )
			{
				Warning( "ZipFile '%s' missing preload section\n", m_ZipName.String() );
			}
		}

		// No preload section, reset buffer pointer
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	}

	// Parse out central directory and determine absolute file positions of data.
	// Supports uncompressed zip files, with or without preload sections
	bool bSuccess = true;
	char tmpString[MAX_PATH] = { 0 };

	m_PackFiles.EnsureCapacity( numFilesInZip );

	for ( int i = firstFileIdx; i < numFilesInZip; ++i )
	{
		CZipPackFile::CPackFileEntry lookup;
		zipDirBuff.GetObjects( &zipFileHeader );

		if ( zipFileHeader.signature != PKID( 1, 2 ) )
		{
			Warning( "Invalid pack file signature\n" );
			bSuccess = false;
			break;
		}

		if ( zipFileHeader.compressionMethod != ZIP_COMPRESSION_NONE && zipFileHeader.compressionMethod != ZIP_COMPRESSION_LZMA )
		{
			Warning( "Pack file uses unsupported compression method: %hi\n", zipFileHeader.compressionMethod );
			bSuccess = false;
			break;
		}

		Assert( zipFileHeader.fileNameLength < sizeof( tmpString ) );
		unsigned int fileNameLen = Min( (size_t)zipFileHeader.fileNameLength, sizeof( tmpString ) - 1 );
		zipDirBuff.Get( (void *)tmpString, fileNameLen );
		tmpString[fileNameLen] = '\0';
		Q_FixSlashes( tmpString );

		lookup.m_hFileName = m_fs->FindOrAddFileName( tmpString );
		lookup.m_HashName = HashStringCaselessConventional( tmpString );
		lookup.m_nOriginalSize = zipFileHeader.uncompressedSize;
		lookup.m_nCompressedSize = zipFileHeader.compressedSize;
		lookup.m_nPosition = zipFileHeader.relativeOffsetOfLocalHeader +
								sizeof( ZIP_LocalFileHeader ) +
								zipFileHeader.fileNameLength +
								zipFileHeader.extraFieldLength;
		lookup.m_nCompressionMethod = zipFileHeader.compressionMethod;

		// track the index to this file's possible preload directory entry
		if ( m_pPreloadRemapTable )
		{
			lookup.m_nPreloadIdx = m_pPreloadRemapTable[i];
		}
		else
		{
			lookup.m_nPreloadIdx = INVALID_PRELOAD_ENTRY;
		}
		m_PackFiles.InsertNoSort( lookup );

		int nextOffset = bCompatibleFormat ? zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength : 0;
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_CURRENT, nextOffset );
	}

	m_PackFiles.RedoSort();

	return bSuccess;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CZipPackFile::CZipPackFile( CBaseFileSystem* fs, void *pSection )
 : m_PackFiles()
{
	m_fs = fs;
	m_pPreloadDirectory = NULL;
	m_pPreloadData = NULL;
	m_pPreloadHeader = NULL;
	m_pPreloadRemapTable = NULL;
	m_nPreloadSectionOffset = 0;
	m_nPreloadSectionSize = 0;

#if defined( _X360 )
	m_pSection = pSection;
#endif
}

CZipPackFile::~CZipPackFile()
{
	DiscardPreloadData();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : src1 -
//			src2 -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CZipPackFile::CPackFileLessFunc::Less( CZipPackFile::CPackFileEntry const& src1, CZipPackFile::CPackFileEntry const& src2, void *pCtx )
{
	return ( src1.m_HashName < src2.m_HashName );
}

//-----------------------------------------------------------------------------
// Purpose: Zip Pack file handle implementation
//-----------------------------------------------------------------------------
CZipPackFileHandle::CZipPackFileHandle( CZipPackFile* pOwner, int64 nBase, unsigned int nLength, unsigned int nIndex, unsigned int nFilePointer )
{
	m_pOwner = pOwner;
	m_nBase = nBase;
	m_nLength = nLength;
	m_nIndex = nIndex;
	m_nFilePointer = nFilePointer;
	pOwner->AddRef();
}

CZipPackFileHandle::~CZipPackFileHandle()
{
	m_pOwner->m_mutex.Lock();
	--m_pOwner->m_nOpenFiles;
	// XXX(johns) this doesn't go here, the hell
	if ( m_pOwner->m_nOpenFiles == 0 && m_pOwner->m_bIsMapPath )
	{
		if ( m_pOwner->m_hPackFileHandleFS )
		{
			m_pOwner->FileSystem()->Trace_FClose( m_pOwner->m_hPackFileHandleFS );
			m_pOwner->m_hPackFileHandleFS = NULL;
		}
	}
	m_pOwner->Release();
	m_pOwner->m_mutex.Unlock();
}

void CZipPackFileHandle::SetBufferSize( int nBytes )
{
	if ( m_pOwner->m_hPackFileHandleFS )
	{
		m_pOwner->FileSystem()->FS_setbufsize( m_pOwner->m_hPackFileHandleFS, nBytes );
	}
}

int CZipPackFileHandle::GetSectorSize()
{
	return m_pOwner->GetSectorSize();
}

int64 CZipPackFileHandle::AbsoluteBaseOffset()
{
	return m_pOwner->GetPackFileBaseOffset() + m_nBase;
}

#if defined( _DEBUG ) && !defined( OSX )
#include <atomic>
static std::atomic<int> sLZMAPackFileHandles( 0 );
#endif // defined( _DEBUG ) && !defined( OSX )

CLZMAZipPackFileHandle::CLZMAZipPackFileHandle( CZipPackFile* pOwner, int64 nBase, unsigned int nOriginalSize, unsigned int nCompressedSize,
                                                unsigned int nIndex, unsigned int nFilePointer )
	: CZipPackFileHandle( pOwner, nBase, nCompressedSize, nIndex, nFilePointer ),
	  m_BackSeekBuffer( 0, PACKFILE_COMPRESSED_FILEHANDLE_SEEK_BUFFER ),
	  m_ReadBuffer( 0, PACKFILE_COMPRESSED_FILEHANDLE_READ_BUFFER ),
	  m_pLZMAStream( NULL ), m_nSeekPosition( 0 ), m_nOriginalSize( nOriginalSize )
{
	Reset();
#if defined( _DEBUG ) && !defined( OSX )
	if ( ++sLZMAPackFileHandles == PACKFILE_COMPRESSED_FILE_HANDLES_WARNING )
	{
		// By my count a live filehandle is currently around 270k, mostly due to the LZMA dictionary (256k) with the
		// rest being the read/seek buffers.
		Warning( "More than %u compressed file handles in use. "
		         "These carry large buffers around, and can cause high memory usage\n",
		         PACKFILE_COMPRESSED_FILE_HANDLES_WARNING );
	}
#endif // defined( _DEBUG ) && !defined( OSX )
}

CLZMAZipPackFileHandle::~CLZMAZipPackFileHandle()
{
	delete m_pLZMAStream;
	m_pLZMAStream = NULL;
#if defined( _DEBUG ) && !defined( OSX )
	sLZMAPackFileHandles--;
	Assert( sLZMAPackFileHandles >= 0 );
#endif // defined( _DEBUG ) && !defined( OSX )
}

int CLZMAZipPackFileHandle::Read( void* pBuffer, int nDestSize, int nBytes )
{
	int nMaxRead = Min( Min( nDestSize, nBytes ), Size() - Tell() );
	int nBytesRead = 0;

	// If we have seeked backwards into our buffer, read from there first
	int nBackSeek = m_BackSeekBuffer.TellPut() - m_BackSeekBuffer.TellGet();
	Assert( nBackSeek >= 0 );
	if ( nBackSeek > 0 )
	{
		int nBackSeekRead = Min( nBackSeek, nMaxRead );
		m_BackSeekBuffer.Get( pBuffer, nBackSeekRead );
		nBytesRead += nBackSeekRead;
	}

	// Done if nothing to read
	if ( nMaxRead - nBytesRead <= 0 )
	{
		m_nSeekPosition += nBytesRead;
		return nBytesRead;
	}

	// Read bytes not fulfilled by backbuffer
	Assert( m_BackSeekBuffer.TellPut() == m_BackSeekBuffer.TellGet() );
	while ( nBytesRead < nMaxRead )
	{
		// refill read buffer if empty
		int nRemainingReadBuffer = FillReadBuffer();

		// Consume from read buffer
		unsigned int nCompressedBytesRead = 0;
		unsigned int nOutputBytesWritten = 0;
		bool bSuccess = m_pLZMAStream->Read( (unsigned char *)m_ReadBuffer.PeekGet(), nRemainingReadBuffer,
		                                     (unsigned char *)pBuffer + nBytesRead, nMaxRead - nBytesRead,
		                                     nCompressedBytesRead, nOutputBytesWritten );

		if ( bSuccess )
		{
			// fixup get position
			m_ReadBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nCompressedBytesRead );

			nBytesRead += nOutputBytesWritten;

			AssertMsg( nCompressedBytesRead == (unsigned int)nRemainingReadBuffer || nBytesRead == nMaxRead,
			           "Should have consumed the readbuffer or reached nMaxRead" );

			if ( nCompressedBytesRead == 0 && nOutputBytesWritten == 0 )
			{
				AssertMsg( nCompressedBytesRead > 0 || nOutputBytesWritten > 0,
				           "Stuck progress in read loop, aborting. Stream may be defunct." );
				break;
			}
		}
		else
		{
			Warning( "Pack file: reading from LZMA stream failed\n" );
			break;
		}
	}

	// Finally, store last bytes output to the backseek buffer

	// If we read less than BackSeekBuffer.Size() bytes, shift the end of the old backseek buffer up
	int nOldBackSeek = m_BackSeekBuffer.TellPut();
	int nReuseBackSeek = Max( Min( m_BackSeekBuffer.Size() - nBytesRead, nOldBackSeek ), 0 );
	if ( nReuseBackSeek )
	{
		// Shift the reused chunk to the front
		V_memmove( m_BackSeekBuffer.Base(),
		           (unsigned char *)m_BackSeekBuffer.Base() + m_BackSeekBuffer.TellPut() - nReuseBackSeek,
		           nReuseBackSeek );
	}

	// Update get/put position
	m_BackSeekBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, nReuseBackSeek );
	m_BackSeekBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, nReuseBackSeek );

	// Fill in remainder from what we just read
	int nReadIntoBackSeek = Min( m_BackSeekBuffer.Size() - nReuseBackSeek, nBytesRead );
	m_BackSeekBuffer.Put( (unsigned char *)pBuffer + nBytesRead - nReadIntoBackSeek, nReadIntoBackSeek );
	m_BackSeekBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, nReadIntoBackSeek );

	m_nSeekPosition += nBytesRead;
	return nBytesRead;
}

int CLZMAZipPackFileHandle::Seek( int nOffset, int nWhence )
{
	int nNewPosition = m_nSeekPosition;

	if ( nWhence == SEEK_CUR )
	{
		nNewPosition = m_nSeekPosition + nOffset;
	}
	else if ( nWhence == SEEK_END )
	{
		nNewPosition = Size() + nOffset;
	}
	else if ( nWhence == SEEK_SET )
	{
		nNewPosition = nOffset;
	}
	else
	{
		AssertMsg( false, "Unknown seek type" );
	}

	nNewPosition = Min( Size(), nNewPosition );
	nNewPosition = Max( 0, nNewPosition );

	if ( nNewPosition == m_nSeekPosition )
	{
		return nNewPosition;
	}

	// Backwards seek
	if ( nNewPosition < m_nSeekPosition )
	{
		int nBackSeekAvailable = m_BackSeekBuffer.TellGet();
		int nDesiredBackSeek = m_nSeekPosition - nNewPosition;

		if ( nBackSeekAvailable >= nDesiredBackSeek )
		{
			// Move get backwards into backseek buffer to account for seek
			m_BackSeekBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, -nDesiredBackSeek );
			m_nSeekPosition = nNewPosition;
		}
		else
		{
			// Seeking backwards beyond our backseek buffer. Have to restart stream. This kills the performance.
			Warning( "LZMA file handle: seeking backwards beyond backseek buffer size ( %u ), "
			         "replaying read & decompression of %u bytes. Should avoid large back seeks in compressed files or "
			         "increase backseek buffer sizing.",
			         m_BackSeekBuffer.Size(), nNewPosition );

			// Reset to beginning of underlying stream
			Reset();

			// Fall through to performing a forward seek
		}
	}

	// Forward seek
	if ( nNewPosition > m_nSeekPosition )
	{
		// Can't actually seek forward without making decode progress. Issue fake reads until we've reached our target.
		unsigned char dummyBuffer[COMPRESSED_SEEK_READ_CHUNK];
		while ( nNewPosition > m_nSeekPosition )
		{
			int nReadSize = Min( nNewPosition - m_nSeekPosition, COMPRESSED_SEEK_READ_CHUNK );
			unsigned int nBytesRead = Read( &dummyBuffer, sizeof(dummyBuffer), nReadSize );
			m_nSeekPosition += nBytesRead;
			if ( !nBytesRead )
			{
				Warning( "LZMA file handle: failed reading forward to desired seek position\n" );
				break;
			}
		}
	}

	return m_nSeekPosition;
}

int CLZMAZipPackFileHandle::Tell()
{
	return m_nSeekPosition;
}

int CLZMAZipPackFileHandle::Size()
{
	return m_nOriginalSize;
}

int CLZMAZipPackFileHandle::FillReadBuffer()
{
	int nRemainingReadBuffer = m_ReadBuffer.TellPut() - m_ReadBuffer.TellGet();
	int nRemainingCompressedBytes = CZipPackFileHandle::Size() - CZipPackFileHandle::Tell();

	if ( nRemainingReadBuffer > 0 || nRemainingCompressedBytes <= 0 )
	{
		// No action if read buffer isn't empty
		return nRemainingReadBuffer;
	}

	// Reset empty read buffer
	m_ReadBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	m_ReadBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	int nRefillSize = Min( nRemainingCompressedBytes, m_ReadBuffer.Size() );
	int nRefillResult = CZipPackFileHandle::Read( m_ReadBuffer.PeekPut(), m_ReadBuffer.Size(), nRefillSize );
	AssertMsg( nRefillSize == nRefillResult, "Don't expect to fail to read here" );

	// Fixup put pointer after writing into buffer's memory
	m_ReadBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, nRefillResult );

	return nRefillResult;
}

void CLZMAZipPackFileHandle::Reset()
{
	// Seek underlying stream back to start
	CZipPackFileHandle::Seek( SEEK_SET, 0 );

	delete m_pLZMAStream;
	m_pLZMAStream = new CLZMAStream();
	m_pLZMAStream->InitZIPHeader( CZipPackFileHandle::Size(), m_nOriginalSize );
	m_nSeekPosition = 0;
	m_BackSeekBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_BackSeekBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	m_ReadBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_ReadBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
}
