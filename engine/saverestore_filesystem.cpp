//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Filesystem abstraction for CSaveRestore - allows for storing temp save files
//			either in memory or on disk.
//
//===========================================================================//

#ifdef _WIN32
#include "winerror.h"
#endif
#include "filesystem_engine.h"
#include "saverestore_filesystem.h"
#include "host_saverestore.h"
#include "host.h"
#include "sys.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzss.h"
#include "tier1/convar.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar save_spew;
extern IXboxSystem *g_pXboxSystem;

#define SaveMsg if ( !save_spew.GetBool() ) ; else Msg

void SaveInMemoryCallback( IConVar *var, const char *pOldString, float flOldValue );

#ifdef _X360
ConVar save_in_memory( "save_in_memory", "1", 1, "Set to 1 to save to memory instead of disk (Xbox 360)", SaveInMemoryCallback );
#else
ConVar save_in_memory( "save_in_memory", "0", 0, "Set to 1 to save to memory instead of disk (Xbox 360)", SaveInMemoryCallback );
#endif // _X360

#define INVALID_INDEX	(GetDirectory().InvalidIndex())
enum { READ_ONLY, WRITE_ONLY };

static float g_fPrevSaveInMemoryValue;

static bool SaveFileLessFunc( const CUtlSymbol &lhs, const CUtlSymbol &rhs )
{
	return lhs < rhs;
}

//----------------------------------------------------------------------------
//	Simulates the save directory in RAM.
//----------------------------------------------------------------------------
class CSaveDirectory
{
public:
	CSaveDirectory()
	{
		m_Files.SetLessFunc( SaveFileLessFunc );
		file_t dummy;
		dummy.name = m_SymbolTable.AddString( "dummy" );
		m_Files.Insert( dummy.name, dummy );
	}

	~CSaveDirectory()
	{
		int i = m_Files.FirstInorder();
		while ( m_Files.IsValidIndex( i ) )
		{
			int idx = i;
			i = m_Files.NextInorder( i );

			delete m_Files[idx].pBuffer;
			delete m_Files[idx].pCompressedBuffer;
			m_Files.RemoveAt( idx );
		}
	}

	struct file_t
	{
		file_t() 
		{ 
			pBuffer = NULL; 
			pCompressedBuffer = NULL;
			nSize = 0;
			nCompressedSize = NULL;
		}


		int				eType;
		CUtlSymbol		name;
		unsigned int	nSize;
		unsigned int	nCompressedSize;
		CUtlBuffer		*pBuffer;
		CUtlBuffer		*pCompressedBuffer;
	};

	CUtlSymbolTable		m_SymbolTable;
	CUtlMap<CUtlSymbol, file_t>	m_Files;
};

typedef CSaveDirectory::file_t SaveFile_t;

//----------------------------------------------------------------------------
//	CSaveRestoreFileSystem: Manipulates files in the CSaveDirectory
//----------------------------------------------------------------------------
class CSaveRestoreFileSystem : public ISaveRestoreFileSystem
{
public:
	CSaveRestoreFileSystem()
	{
		m_pSaveDirectory = new CSaveDirectory();
		m_iContainerOpens = 0;
	}

	~CSaveRestoreFileSystem()
	{
		delete m_pSaveDirectory;
	}

	bool			FileExists( const char *pFileName, const char *pPathID = NULL );
	void			RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID = NULL );
	void			RemoveFile( char const* pRelativePath, const char *pathID = NULL );

	FileHandle_t	Open( const char *pFileName, const char *pOptions, const char *pathID = NULL );
	void			Close( FileHandle_t );
	int				Read( void *pOutput, int size, FileHandle_t file );
	int				Write( void const* pInput, int size, FileHandle_t file );
	void			Seek( FileHandle_t file, int pos, FileSystemSeek_t method );
	unsigned int	Tell( FileHandle_t file );
	unsigned int	Size( FileHandle_t file );
	unsigned int	Size( const char *pFileName, const char *pPathID = NULL );

	void			AsyncFinishAllWrites( void );
	void			AsyncRelease( FSAsyncControl_t hControl );
	FSAsyncStatus_t	AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl = NULL );
	FSAsyncStatus_t	AsyncFinish( FSAsyncControl_t hControl, bool wait = false );
	FSAsyncStatus_t	AsyncAppend( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl = NULL );
	FSAsyncStatus_t	AsyncAppendFile( const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl = NULL );

	void			DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave );
	void			DirectorCopyToMemory( const char *pPath, const char *pDestFileName );
	bool			DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave );
	int				DirectoryCount( const char *pPath );
	void			DirectoryClear( const char *pPath, bool bIsXSave );

	void			WriteSaveDirectoryToDisk( void );
	void			LoadSaveDirectoryFromDisk( const char *pPath );
	void			DumpSaveDirectory( void );

	void			Compress( SaveFile_t *pFile );
	void			Uncompress( SaveFile_t *pFile );
	
	void			AuditFiles( void );
	bool			LoadFileFromDisk( const char *pFilename );

private:
	CSaveDirectory	*m_pSaveDirectory;
	CUtlMap<CUtlSymbol, SaveFile_t> &GetDirectory( void ) { return m_pSaveDirectory->m_Files; }
	SaveFile_t &GetFile( const int idx ) { return m_pSaveDirectory->m_Files[idx]; }
	SaveFile_t &GetFile( const FileHandle_t hFile ) { return GetFile( (unsigned int)hFile ); }

	FileHandle_t	GetFileHandle( const char *pFileName );
	int				GetFileIndex( const char *pFileName );

	bool			HandleIsValid( FileHandle_t hFile );

	unsigned int	CompressedSize( const char *pFileName );

	CUtlSymbol		AddString( const char *str ) 
	{
		char szString[ MAX_PATH ];
		Q_strncpy( szString, str, sizeof( szString ) );
		return m_pSaveDirectory->m_SymbolTable.AddString( Q_strlower( szString ) ); 
	}
	const char		*GetString( CUtlSymbol &id ) { return m_pSaveDirectory->m_SymbolTable.String( id ); }


	int m_iContainerOpens;
};

//#define TEST_LZSS_WINDOW_SIZES

//----------------------------------------------------------------------------
//	Compress the file data
//----------------------------------------------------------------------------
void CSaveRestoreFileSystem::Compress( SaveFile_t *pFile )
{
	pFile->pCompressedBuffer->Purge();

#ifdef TEST_LZSS_WINDOW_SIZES
	// Compress the data here
	CLZSS compressor_test;
	CLZSS newcompressor_test( 2048 );
	pFile->nCompressedSize = 0;
	float start = Plat_FloatTime();
	for(int i=0;i<10;i++)
	{
		uint32 sz;
		unsigned char *pCompressedBuffer = compressor_test.Compress(
			(unsigned char *) pFile->pBuffer->Base(), pFile->nSize, &sz );
		delete[] pCompressedBuffer;
	}
	Warning(" old compressor_test %f", Plat_FloatTime() - start );
	start = Plat_FloatTime();
	for(int i=0;i<10;i++)
	{
		uint32 sz;
		unsigned char *pCompressedBuffer = newcompressor_test.Compress( 
			(unsigned char *) pFile->pBuffer->Base(), pFile->nSize, &sz );
		delete[] pCompressedBuffer;
	}
	Warning(" new compressor_test %f", Plat_FloatTime() - start );
    if ( 1)
	{
		uint32 sz;
		uint32 sz1;
		unsigned char *pNewCompressedBuffer = newcompressor_test.Compress(
			(unsigned char *) pFile->pBuffer->Base(), pFile->nSize, &sz );
		unsigned char *pOldCompressedBuffer = compressor_test.Compress( 
			(unsigned char *) pFile->pBuffer->Base(), pFile->nSize, &sz1 );
		if ( ! pNewCompressedBuffer )
			Warning("new no comp");
		if ( ! pOldCompressedBuffer )
			Warning("old no comp");
		if ( pNewCompressedBuffer && pOldCompressedBuffer )
		{
			if ( sz != sz1 )
				Warning(" new size = %d old = %d", sz, sz1 );
			if ( memcmp( pNewCompressedBuffer, pOldCompressedBuffer, sz ) )
				Warning("data mismatch");
		}
		delete[] pOldCompressedBuffer;
		delete[] pNewCompressedBuffer;
	}
#endif

	CLZSS compressor( 2048 );
	
	unsigned char *pCompressedBuffer = compressor.Compress( (unsigned char *) pFile->pBuffer->Base(), pFile->nSize, &pFile->nCompressedSize );
	if ( pCompressedBuffer == NULL )
	{
		// Just copy the buffer uncompressed
		pFile->pCompressedBuffer->Put( pFile->pBuffer->Base(), pFile->nSize );
		pFile->nCompressedSize = pFile->nSize;
	}
	else
	{
		// Take the compressed buffer as our own
		pFile->pCompressedBuffer->AssumeMemory( pCompressedBuffer, pFile->nCompressedSize, pFile->nCompressedSize ); // ?
	}
	// end compression

	pFile->pCompressedBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	pFile->pCompressedBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, pFile->nCompressedSize );

	// Don't want the uncompressed memory hanging around
	pFile->pBuffer->Purge();

	unsigned int srcBytes = pFile->nSize;

	pFile->nSize = 0;

	unsigned int destBytes = pFile->nCompressedSize;

	float percent = 0.f;
	if ( srcBytes )
		percent = 100.0f * (1.0f - (float)destBytes/(float)srcBytes);
	
	SaveMsg( "SIM: SaveDir: (%s) Compressed %d bytes to %d bytes. (%.0f%%)\n", GetString( pFile->name ), srcBytes, destBytes, percent );
}

//----------------------------------------------------------------------------
//	Uncompress the file data
//----------------------------------------------------------------------------
void CSaveRestoreFileSystem::Uncompress( SaveFile_t *pFile )
{
	pFile->pBuffer->Purge();

	// Uncompress the data here
	CLZSS compressor;
	unsigned int nUncompressedSize = compressor.GetActualSize( (unsigned char *) pFile->pCompressedBuffer->Base() );
	if ( nUncompressedSize != 0 )
	{
		unsigned char *pUncompressBuffer = (unsigned char *) malloc( nUncompressedSize );
		nUncompressedSize = compressor.Uncompress( (unsigned char *) pFile->pCompressedBuffer->Base(), pUncompressBuffer );
		pFile->pBuffer->AssumeMemory( pUncompressBuffer, nUncompressedSize, nUncompressedSize ); // ?
	}
	else
	{
		// Put it directly into our target
		pFile->pBuffer->Put( (unsigned char *) pFile->pCompressedBuffer->Base(), pFile->nCompressedSize );
	}
	// end decompression

	pFile->nSize = pFile->pBuffer->TellMaxPut();
	pFile->pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	pFile->pBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, pFile->nSize );

	unsigned int srcBytes = pFile->nCompressedSize;
	unsigned int destBytes = pFile->nSize;
	
	SaveMsg( "SIM: SaveDir: (%s) Uncompressed %d bytes to %d bytes.\n", GetString( pFile->name ), srcBytes, destBytes );
}

//----------------------------------------------------------------------------
//	Access the save files
//----------------------------------------------------------------------------
int CSaveRestoreFileSystem::GetFileIndex( const char *filename )
{
	CUtlSymbol id = AddString( Q_UnqualifiedFileName( filename ) );
	return GetDirectory().Find( id );
}

FileHandle_t CSaveRestoreFileSystem::GetFileHandle( const char *filename )
{
	int idx = GetFileIndex( filename );
	if ( idx == INVALID_INDEX )
	{
		idx = 0;
	}
	return (void*)idx;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the named memory block exists
//-----------------------------------------------------------------------------
bool CSaveRestoreFileSystem::FileExists( const char *pFileName, const char *pPathID )
{
	return ( GetFileHandle( pFileName ) != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Validates a file handle
//-----------------------------------------------------------------------------
bool CSaveRestoreFileSystem::HandleIsValid( FileHandle_t hFile )
{
	return hFile && GetDirectory().IsValidIndex( (unsigned int)hFile );
}

//-----------------------------------------------------------------------------
// Purpose: Renames a block of memory
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
{
	int idx = GetFileIndex( pOldPath );
	if ( idx != INVALID_INDEX )
	{
		CUtlSymbol newID = AddString( Q_UnqualifiedFileName( pNewPath ) );
		GetFile( idx ).name = newID;
		GetDirectory().Reinsert( newID, idx );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes a memory block from CSaveDirectory and frees it.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::RemoveFile( char const* pRelativePath, const char *pathID )
{
	int idx = GetFileIndex( pRelativePath );
	if ( idx != INVALID_INDEX )
	{
		delete GetFile( idx ).pBuffer;
		delete GetFile( idx ).pCompressedBuffer;
		GetDirectory().RemoveAt( idx );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Access an existing memory block if it exists, else allocate one.
//-----------------------------------------------------------------------------
FileHandle_t CSaveRestoreFileSystem::Open( const char *pFullName, const char *pOptions, const char *pathID )
{
	SaveFile_t *pFile = NULL;
	CUtlSymbol id = AddString( Q_UnqualifiedFileName( pFullName ) );
	int idx = GetDirectory().Find( id );
	if ( idx == INVALID_INDEX )
	{
		// Don't create a read-only file
		if ( Q_stricmp( pOptions, "rb" ) )
		{
			// Create new file
			SaveFile_t newFile;
			newFile.name = id;
			newFile.pBuffer = new CUtlBuffer();
			newFile.pCompressedBuffer = new CUtlBuffer();
			idx = GetDirectory().Insert( id, newFile );
		}
		else
		{
			return (void*)0;
		}
	}

	pFile = &GetFile( idx );

	if ( !Q_stricmp( pOptions, "rb" ) )
	{
		Uncompress( pFile );
		pFile->eType = READ_ONLY;
	}
	else if ( !Q_stricmp( pOptions, "wb" ) )
	{
		pFile->pBuffer->Clear();
		pFile->eType = WRITE_ONLY;
	}
	else if ( !Q_stricmp( pOptions, "a" ) )
	{
		Uncompress( pFile );
		pFile->eType = WRITE_ONLY;
	}
	else if ( !Q_stricmp( pOptions, "ab+" ) )
	{
		Uncompress( pFile );
		pFile->eType = WRITE_ONLY;
		pFile->pBuffer->SeekPut( CUtlBuffer::SEEK_TAIL, 0 );
	}
	else
	{
		Assert( 0 );
		Warning( "CSaveRestoreFileSystem: Attempted to open %s with unsupported option %s\n", pFullName, pOptions );
		return (void*)0;
	}

	return (void*)idx;
}

//-----------------------------------------------------------------------------
// Purpose: No need to close files in memory. Could perform post processing here.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::Close( FileHandle_t hFile )
{
	// Compress the file
	if ( HandleIsValid( hFile ) )
	{
		SaveFile_t &file = GetFile( hFile );
		
		// Files opened for read don't need to be recompressed
		if ( file.eType == READ_ONLY )
		{
			SaveMsg("SIM: Closed file: %s\n", GetString( file.name ) );
			file.pBuffer->Purge();
			file.nSize = 0;
		}
		else
		{
			Compress( &file );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads data from memory.
//-----------------------------------------------------------------------------
int CSaveRestoreFileSystem::Read( void *pOutput, int size, FileHandle_t hFile )
{
	int readSize = 0;
	if ( HandleIsValid( hFile ) )
	{
		SaveFile_t &file = GetFile( hFile );

		if( file.eType == READ_ONLY )
		{
			readSize = file.pBuffer->GetUpTo( pOutput, size );
		}
		else
		{
			Warning( "Read: Attempted to read from a write-only file" );
			readSize = 0;
			Assert( 0 );
		}
	}
	return readSize;
}

//-----------------------------------------------------------------------------
// Purpose: Writes data to memory.
//-----------------------------------------------------------------------------
int CSaveRestoreFileSystem::Write( void const* pInput, int size, FileHandle_t hFile )
{
	int writeSize = 0;
	if ( HandleIsValid( hFile ) )
	{
		SaveFile_t &file = GetFile( hFile );

		if( file.eType == WRITE_ONLY )
		{
			file.pBuffer->Put( pInput, size );
			file.nSize = file.pBuffer->TellMaxPut();
			writeSize = size;
		}
		else
		{
			Warning( "Write: Attempted to write to a read-only file" );
			writeSize = 0;
			Assert( 0 );
		}
	}
	return writeSize;
}

//-----------------------------------------------------------------------------
// Purpose: Seek in memory. Seeks the UtlBuffer put or get pos depending
//			on whether the file was opened for read or write.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::Seek( FileHandle_t hFile, int pos, FileSystemSeek_t method )
{
	if ( HandleIsValid( hFile ) )
	{
		SaveFile_t &file = GetFile( hFile );
		if ( file.eType == READ_ONLY )
		{
			file.pBuffer->SeekGet( (CUtlBuffer::SeekType_t)method, pos );
		}
		else if ( file.eType == WRITE_ONLY )
		{
			file.pBuffer->SeekPut( (CUtlBuffer::SeekType_t)method, pos );
		}
		else
		{
			Assert( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return position in memory. Returns UtlBuffer put or get pos depending
//			on whether the file was opened for read or write.
//-----------------------------------------------------------------------------
unsigned int CSaveRestoreFileSystem::Tell( FileHandle_t hFile )
{
	unsigned int pos = 0;
	if ( HandleIsValid( hFile ) )
	{
		SaveFile_t &file = GetFile( hFile );
		if ( file.eType == READ_ONLY )
		{
			pos = file.pBuffer->TellGet();
		}
		else if ( file.eType == WRITE_ONLY )
		{
			pos = file.pBuffer->TellPut();
		}
		else
		{
			Assert( 0 );
		}
	}
	return pos;
}

//-----------------------------------------------------------------------------
// Purpose: Return uncompressed memory size
//-----------------------------------------------------------------------------
unsigned int CSaveRestoreFileSystem::Size( FileHandle_t hFile )
{
	if ( HandleIsValid( hFile ) )
	{
		return GetFile( hFile ).nSize;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return uncompressed size of data in memory
//-----------------------------------------------------------------------------
unsigned int CSaveRestoreFileSystem::Size( const char *pFileName, const char *pPathID )
{
	return Size( GetFileHandle( pFileName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Return compressed size of data in memory
//-----------------------------------------------------------------------------
unsigned int CSaveRestoreFileSystem::CompressedSize( const char *pFileName )
{
	FileHandle_t hFile = GetFileHandle( pFileName );
	if ( hFile )
	{
		return GetFile( hFile ).nCompressedSize;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Writes data to memory. Function is NOT async.
//-----------------------------------------------------------------------------
FSAsyncStatus_t CSaveRestoreFileSystem::AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
{
	FSAsyncStatus_t retval = FSASYNC_ERR_FAILURE;

	FileHandle_t hFile = Open( pFileName, "wb" );
	if ( hFile )
	{
		SaveFile_t &file = GetFile( (unsigned int)hFile );

		if( file.eType == WRITE_ONLY )
		{
			file.pBuffer->Put( pSrc, nSrcBytes );
			file.nSize = file.pBuffer->TellMaxPut();
			Compress( &file );
			retval = FSASYNC_OK;
		}
		else
		{
			Warning( "AsyncWrite: Attempted to write to a read-only file" );
			Assert( 0 );
		}
	}

	if ( bFreeMemory )
		free( const_cast< void * >( pSrc ) );

	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Appends data to a memory block. Function is NOT async.
//-----------------------------------------------------------------------------
FSAsyncStatus_t CSaveRestoreFileSystem::AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl )
{
	FSAsyncStatus_t retval = FSASYNC_ERR_FAILURE;

	FileHandle_t hFile = Open( pFileName, "a" );
	if ( hFile )
	{
		SaveFile_t &file = GetFile( hFile );
		if( file.eType == WRITE_ONLY )
		{
			file.pBuffer->Put( pSrc, nSrcBytes );
			file.nSize = file.pBuffer->TellMaxPut();
			Compress( &file );
			retval = FSASYNC_OK;
		}
		else
		{
			Warning( "AsyncAppend: Attempted to write to a read-only file" );
			Assert( 0 );
		}
	}

	if ( bFreeMemory )
		free( const_cast< void * >( pSrc ) );

	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Appends a memory block to another memory block. Function is NOT async.
//-----------------------------------------------------------------------------
FSAsyncStatus_t CSaveRestoreFileSystem::AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl )
{
	FileHandle_t hFile = Open( pSrcFileName, "rb" );
	if ( hFile )
	{
		SaveFile_t &file = GetFile( hFile );
		return AsyncAppend( pDestFileName, file.pBuffer->Base(), file.nSize, false );
	}
	return FSASYNC_ERR_FILEOPEN;
}

//-----------------------------------------------------------------------------
// Purpose: All operations in memory are synchronous
//-----------------------------------------------------------------------------
FSAsyncStatus_t CSaveRestoreFileSystem::AsyncFinish( FSAsyncControl_t hControl, bool wait )
{
	// do nothing
	return FSASYNC_OK;
}
void CSaveRestoreFileSystem::AsyncRelease( FSAsyncControl_t hControl )
{
	// do nothing
}
void CSaveRestoreFileSystem::AsyncFinishAllWrites( void )
{
	// Do nothing
}

//-----------------------------------------------------------------------------
// Purpose: Package up all intermediate files to a save game as per usual, but keep them in memory instead of commiting them to disk
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::DirectorCopyToMemory( const char *pPath, const char *pDestFileName )
{
	// Write the save file
	FileHandle_t hSaveFile = Open( pDestFileName, "ab+", pPath );
	if ( !hSaveFile )
		return;

	SaveFile_t &saveFile = GetFile( hSaveFile );

	// At this point, we're going to be sneaky and spoof the uncompressed buffer back into the compressed one
	// We need to do this because the file goes out to disk as a mixture of an uncompressed header and tags, and compressed
	// intermediate files, so this emulates that in memory
	saveFile.pCompressedBuffer->Purge();
	saveFile.nCompressedSize = 0;
	saveFile.pCompressedBuffer->Put( saveFile.pBuffer->Base(), saveFile.nSize );

	unsigned int nNumFilesPacked = 0;
	for ( int i = GetDirectory().FirstInorder(); GetDirectory().IsValidIndex( i ); i = GetDirectory().NextInorder( i ) )
	{
		SaveFile_t &file = GetFile( i );
		const char *pName = GetString( file.name );
		char szFileName[MAX_PATH];
		if ( Q_stristr( pName, ".hl" ) )
		{
			int fileSize = CompressedSize( pName );
			if ( fileSize )
			{
				Assert( Q_strlen( pName ) <= MAX_PATH );

				memset( szFileName, 0, sizeof( szFileName ) );
				Q_strncpy( szFileName, pName, sizeof( szFileName ) );
				saveFile.pCompressedBuffer->Put( szFileName, sizeof( szFileName ) );
				saveFile.pCompressedBuffer->Put( &fileSize, sizeof(fileSize) );
				saveFile.pCompressedBuffer->Put( file.pCompressedBuffer->Base(), file.nCompressedSize );
				
				SaveMsg("SIM: Packed: %s [Size: %.02f KB]\n", GetString( file.name ), (float)file.nCompressedSize / 1024.0f );
				nNumFilesPacked++;
			}
		}
	}		

	// Set the final, complete size of the file
	saveFile.nCompressedSize = saveFile.pCompressedBuffer->TellMaxPut();
	SaveMsg("SIM: (%s) Total Files Packed: %d [Size: %.02f KB]\n", GetString( saveFile.name ), nNumFilesPacked, (float) saveFile.nCompressedSize / 1024.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Copies the compressed contents of the CSaveDirectory into the save file on disk.
//			Note: This expects standard saverestore behavior, and does NOT
//			currently use pPath to filter the filename search.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave )
{
	if ( !Q_stristr( pPath, "*.hl?" ) )
	{
		// Function depends on known behavior
		Assert( 0 );
		return;
	}

	// If we don't have a valid storage device, save this to memory instead
	if ( saverestore->StorageDeviceValid() == false )
	{
		DirectorCopyToMemory( pPath, pDestFileName );
		return;
	}

	// Write the save file
	FileHandle_t hSaveFile = Open( pDestFileName, "rb", pPath );
	if ( !hSaveFile )
		return;

	SaveFile_t &saveFile = GetFile( hSaveFile );

	// Find out how large this is going to be 
	unsigned int nWriteSize = saveFile.nSize;
	for ( int i = GetDirectory().FirstInorder(); GetDirectory().IsValidIndex( i ); i = GetDirectory().NextInorder( i ) )
	{
		SaveFile_t &file = GetFile( i );
		const char *pName = GetString( file.name );
		if ( Q_stristr( pName, ".hl" ) )
		{
			// Account for the lump header size
			nWriteSize += MAX_PATH + sizeof(int) + file.nCompressedSize;
		}
	}

	// Fail to write
#if defined( _X360 )
	if ( nWriteSize > XBX_SAVEGAME_BYTES )
	{
		// FIXME: This error is now lost in the ether!
		return;
	}
#endif

	g_pFileSystem->AsyncWriteFile( pDestFileName, saveFile.pBuffer, saveFile.nSize, true, false );

	// AsyncWriteFile() takes control of the utlbuffer, so don't let RemoveFile() delete it.
	saveFile.pBuffer = NULL;
	RemoveFile( pDestFileName );

	// write the list of files to the save file
	for ( int i = GetDirectory().FirstInorder(); GetDirectory().IsValidIndex( i ); i = GetDirectory().NextInorder( i ) )
	{
		SaveFile_t &file = GetFile( i );
		const char *pName = GetString( file.name );
		if ( Q_stristr( pName, ".hl" ) )
		{
			int fileSize = CompressedSize( pName );
			if ( fileSize )
			{
				Assert( Q_strlen( pName ) <= MAX_PATH );

				g_pFileSystem->AsyncAppend( pDestFileName, memcpy( new char[MAX_PATH], pName, MAX_PATH), MAX_PATH, true );
				g_pFileSystem->AsyncAppend( pDestFileName, new int(fileSize), sizeof(int), true );

				// This behaves like AsyncAppendFile (due to 5th param) but gets src file from memory instead of off disk.
				g_pFileSystem->AsyncWriteFile( pDestFileName, file.pCompressedBuffer, file.nCompressedSize, false, true );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Extracts all the files contained within pFile. 
//			Does not Uncompress the extracted data.
//-----------------------------------------------------------------------------
bool CSaveRestoreFileSystem::DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave )
{
	int				fileSize;
	FileHandle_t	pCopy;
	char			fileName[ MAX_PATH ];

	// Read compressed files from disk into the virtual directory
	for ( int i = 0; i < fileCount; i++ )
	{
		Read( fileName, MAX_PATH, pFile );
		Read( &fileSize, sizeof(int), pFile );
		if ( !fileSize )
			return false;

		pCopy = Open( fileName, "wb" );
		if ( !pCopy )
			return false;
		
		SaveFile_t &destFile = GetFile( pCopy );
		destFile.pCompressedBuffer->EnsureCapacity( fileSize );

		// Must read in the correct amount of data
		if ( Read( destFile.pCompressedBuffer->PeekPut(), fileSize, pFile ) != fileSize )
			return false;

		destFile.pCompressedBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, fileSize );
		destFile.nCompressedSize = fileSize;	

		SaveMsg("SIM: Extracted: %s [Size: %d KB]\n", GetString( destFile.name ), destFile.nCompressedSize / 1024 );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFilename - 
//-----------------------------------------------------------------------------
bool CSaveRestoreFileSystem::LoadFileFromDisk( const char *pFilename )
{
	// Open a new file for writing in memory
	FileHandle_t hMemoryFile = Open( pFilename, "wb" );
	if ( !hMemoryFile )
		return false;

	// Open the file off the disk
	FileHandle_t hDiskFile = g_pFileSystem->OpenEx( pFilename, "rb", ( IsX360() ) ? FSOPEN_NEVERINPACK : 0 );
	if ( !hDiskFile )
		return false;

	// Read it in off of the disk to memory
	SaveFile_t &memoryFile = GetFile( hMemoryFile );
	if ( g_pFileSystem->ReadToBuffer( hDiskFile, *memoryFile.pCompressedBuffer ) == false )
		return false;

	// Hold the compressed size
	memoryFile.nCompressedSize = memoryFile.pCompressedBuffer->TellMaxPut();	
	
	// Close the disk file
	g_pFileSystem->Close( hDiskFile );

	SaveMsg("SIM: Loaded %s into memory\n", pFilename );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of save files in the specified filter
//			Note: This expects standard saverestore behavior, and does NOT
//			currently use pPath to filter file search.
//-----------------------------------------------------------------------------
int CSaveRestoreFileSystem::DirectoryCount( const char *pPath )
{
	if ( !Q_stristr( pPath, "*.hl?" ) )
	{
		// Function depends on known behavior
		Assert( 0 );
		return 0;
	}

	int count = 0;
	for ( int i = GetDirectory().FirstInorder(); GetDirectory().IsValidIndex( i ); i = GetDirectory().NextInorder( i ) )
	{
		SaveFile_t &file = GetFile( i );
		if ( Q_stristr( GetString( file.name ), ".hl" ) )
		{
			++count;
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Clears the save directory of temporary save files (*.hl)
//			Note: This expects standard saverestore behavior, and does NOT
//			currently use pPath to filter file search.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::DirectoryClear( const char *pPath, bool bIsXSave )
{
	if ( !Q_stristr( pPath, "*.hl?" ) )
	{
		// Function depends on known behavior
		Assert( 0 );
		return;
	}

	int i = GetDirectory().FirstInorder();
	while ( GetDirectory().IsValidIndex( i ) )
	{
		SaveFile_t &file = GetFile( i );
		i = GetDirectory().NextInorder( i );
		if ( Q_stristr( GetString( file.name ), ".hl" ) )
		{
			SaveMsg("SIM: Cleared: %s\n", GetString( file.name ) );

			// Delete the temporary save file
			RemoveFile( GetString( file.name ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Transfer all save files from memory to disk.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::WriteSaveDirectoryToDisk( void )
{
	char szPath[ MAX_PATH ];

	int i = GetDirectory().FirstInorder();
	while ( GetDirectory().IsValidIndex( i ) )
	{
		SaveFile_t &file = GetFile( i );
		i = GetDirectory().NextInorder( i );
		const char *pName = GetString( file.name );
		if ( Q_stristr( pName, ".hl" ) )
		{
			// Write the temporary save file to disk
			Open( pName, "rb" );
			Q_snprintf( szPath, sizeof( szPath ), "%s%s", saverestore->GetSaveDir(), pName );
			g_pFileSystem->WriteFile( szPath, "GAME", *file.pBuffer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Transfer all save files from disk to memory.
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::LoadSaveDirectoryFromDisk( const char *pPath )
{
	char const	*findfn;
	char		szPath[ MAX_PATH ];

	findfn = Sys_FindFirstEx( pPath, "DEFAULT_WRITE_PATH", NULL, 0 );

	while ( findfn != NULL )
	{
		Q_snprintf( szPath, sizeof( szPath ), "%s%s", saverestore->GetSaveDir(), findfn );

		// Add the temporary save file
		FileHandle_t hFile = Open( findfn, "wb" );
		if ( hFile )
		{
			SaveFile_t &file = GetFile( hFile );
			g_pFileSystem->ReadFile( szPath, "GAME", *file.pBuffer );
			file.nSize = file.pBuffer->TellMaxPut();
			Close( hFile );
		}

		// Any more save files
		findfn = Sys_FindNext( NULL, 0 );
	}
	Sys_FindClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::AuditFiles( void )
{
	unsigned int nTotalFiles = 0;
	unsigned int nTotalCompressed = 0;
	unsigned int nTotalUncompressed = 0;

	int i = GetDirectory().FirstInorder();
	while ( GetDirectory().IsValidIndex( i ) )
	{
		SaveFile_t &file = GetFile( i );
		i = GetDirectory().NextInorder( i );
		
		nTotalFiles++;
		nTotalCompressed += file.nCompressedSize;
		nTotalUncompressed += file.nSize;
		Msg("SIM: File: %s [c: %.02f KB / u: %.02f KB]\n", GetString( file.name ), (float)file.nCompressedSize/1024.0f, (float)file.nSize/1024.0f );
	}

	Msg("SIM: ------------------------------------------------------------");
	Msg("SIM: Total files: %d [c: %.02f KB / c: %.02f KB] : Total Size: %.02f KB\n", nTotalFiles, (float)nTotalCompressed/1024.0f, (float)nTotalUncompressed/1024.0f, (float)(nTotalCompressed+nTotalUncompressed)/1024.0f );
}

CON_COMMAND( audit_save_in_memory, "Audit the memory usage and files in the save-to-memory system" )
{
	if ( !IsX360() )
		return;

	g_pSaveRestoreFileSystem->AuditFiles();
}

CON_COMMAND( dump_x360_saves, "Dump X360 save games to disk" )
{
	if ( !IsX360() )
	{
		Warning("dump_x360 only available on X360 platform!\n");
		return;
	}

	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
	{
		Warning( "No storage device attached!\n" );
		return;
	}

	char szInName[MAX_PATH]; // Read path from the container
	char szOutName[MAX_PATH]; // Output path to the disk
	char szFileNameBase[MAX_PATH]; // Name of the file minus directories or extensions
	FileFindHandle_t findHandle;
	
	char szSearchPath[MAX_PATH];
	Q_snprintf( szSearchPath, sizeof( szSearchPath ), "%s:\\*.*", GetCurrentMod() );
	
	const char *pFileName = g_pFileSystem->FindFirst( szSearchPath, &findHandle );
	while (pFileName)
	{		
		// Create the proper read path
		Q_snprintf( szInName, sizeof( szInName ), "%s:\\%s", GetCurrentMod(), pFileName );
		// Read the file and blat it out
		CUtlBuffer buf( 0, 0, 0 );
		if ( g_pFileSystem->ReadFile( szInName, NULL, buf ) )
		{
			// Strip us down to just our filename
			Q_FileBase( pFileName, szFileNameBase, sizeof ( szFileNameBase ) );
			Q_snprintf( szOutName, sizeof( szOutName ), "save/%s.sav", szFileNameBase );
			g_pFileSystem->WriteFile( szOutName, NULL, buf );

			Msg("Copied file: %s to %s\n", szInName, szOutName );
		}
		
		// Clean up
		buf.Clear();

		// Any more save files
		pFileName = g_pFileSystem->FindNext( findHandle );
	}
	
	g_pFileSystem->FindClose( findHandle );
}

CON_COMMAND( dump_x360_cfg, "Dump X360 config files to disk" )
{
	if ( !IsX360() )
	{
		Warning("dump_x360 only available on X360 platform!\n");
		return;
	}

	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
	{
		Warning( "No storage device attached!\n" );
		return;
	}

	char szInName[MAX_PATH]; // Read path from the container
	char szOutName[MAX_PATH]; // Output path to the disk
	char szFileNameBase[MAX_PATH]; // Name of the file minus directories or extensions
	FileFindHandle_t findHandle;

	char szSearchPath[MAX_PATH];
	Q_snprintf( szSearchPath, sizeof( szSearchPath ), "cfg:\\*.*" );

	const char *pFileName = g_pFileSystem->FindFirst( szSearchPath, &findHandle );
	while (pFileName)
	{		
		// Create the proper read path
		Q_snprintf( szInName, sizeof( szInName ), "cfg:\\%s", pFileName );
		// Read the file and blat it out
		CUtlBuffer buf( 0, 0, 0 );
		if ( g_pFileSystem->ReadFile( szInName, NULL, buf ) )
		{
			// Strip us down to just our filename
			Q_FileBase( pFileName, szFileNameBase, sizeof ( szFileNameBase ) );
			Q_snprintf( szOutName, sizeof( szOutName ), "%s.cfg", szFileNameBase );
			g_pFileSystem->WriteFile( szOutName, NULL, buf );

			Msg("Copied file: %s to %s\n", szInName, szOutName );
		}

		// Clean up
		buf.Clear();

		// Any more save files
		pFileName = g_pFileSystem->FindNext( findHandle );
	}

	g_pFileSystem->FindClose( findHandle );
}

#define FILECOPYBUFSIZE (1024 * 1024)
//-----------------------------------------------------------------------------
// Purpose: Copy one file to another file
//-----------------------------------------------------------------------------
static bool FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )
{
	// allocate a reasonably large file copy buffer, since otherwise write performance under steam suffers
	char	*buf = (char *)malloc(FILECOPYBUFSIZE);
	int		size;
	int		readSize;
	bool	success = true;

	while ( fileSize > 0 )
	{
		if ( fileSize > FILECOPYBUFSIZE )
			size = FILECOPYBUFSIZE;
		else
			size = fileSize;
		if ( ( readSize = g_pSaveRestoreFileSystem->Read( buf, size, pInput ) ) < size )
		{
			Warning( "Unexpected end of file expanding save game\n" );
			fileSize = 0;
			success = false;
			break;
		}
		g_pSaveRestoreFileSystem->Write( buf, readSize, pOutput );
		
		fileSize -= size;
	}

	free(buf);
	return success;
}

struct filelistelem_t
{
	char szFileName[MAX_PATH];
};

//-----------------------------------------------------------------------------
// Purpose: Implementation to execute traditional save to disk behavior
//-----------------------------------------------------------------------------
class CSaveRestoreFileSystemPassthrough : public ISaveRestoreFileSystem
{
public:
	CSaveRestoreFileSystemPassthrough() :  m_iContainerOpens( 0 ) {}
	
	bool FileExists( const char *pFileName, const char *pPathID )
	{
		return g_pFileSystem->FileExists( pFileName, pPathID );
	}

	void RemoveFile( char const* pRelativePath, const char *pathID )
	{
		g_pFileSystem->RemoveFile( pRelativePath, pathID );
	}

	void RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
	{
		g_pFileSystem->RenameFile( pOldPath, pNewPath, pathID );
	}

	void AsyncFinishAllWrites( void )
	{
		g_pFileSystem->AsyncFinishAllWrites();
	}

	FileHandle_t Open( const char *pFullName, const char *pOptions, const char *pathID )
	{
		return g_pFileSystem->OpenEx( pFullName, pOptions, FSOPEN_NEVERINPACK, pathID );
	}

	void Close( FileHandle_t hSaveFile )
	{
		g_pFileSystem->Close( hSaveFile );
	}

	int Read( void *pOutput, int size, FileHandle_t hFile )
	{
		return g_pFileSystem->Read( pOutput, size, hFile );
	}

	int Write( void const* pInput, int size, FileHandle_t hFile )
	{
		return g_pFileSystem->Write( pInput, size, hFile );
	}

	FSAsyncStatus_t AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
	{
		SaveMsg( "AsyncWrite (%s/%d)...\n", pFileName, nSrcBytes );
		return g_pFileSystem->AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, bAppend, pControl );
	}

	void Seek( FileHandle_t hFile, int pos, FileSystemSeek_t method )
	{
		g_pFileSystem->Seek( hFile, pos, method );
	}

	unsigned int Tell( FileHandle_t hFile )
	{
		return g_pFileSystem->Tell( hFile );
	}

	unsigned int Size( FileHandle_t hFile )
	{
		return g_pFileSystem->Size( hFile );
	}

	unsigned int Size( const char *pFileName, const char *pPathID )
	{
		return g_pFileSystem->Size( pFileName, pPathID );
	}

	FSAsyncStatus_t AsyncFinish( FSAsyncControl_t hControl, bool wait )
	{
		return g_pFileSystem->AsyncFinish( hControl, wait );
	}

	void AsyncRelease( FSAsyncControl_t hControl )
	{
		g_pFileSystem->AsyncRelease( hControl );
	}

	FSAsyncStatus_t AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl )
	{
		return g_pFileSystem->AsyncAppend( pFileName, pSrc, nSrcBytes, bFreeMemory, pControl );
	}

	FSAsyncStatus_t AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl )
	{
		return g_pFileSystem->AsyncAppendFile( pDestFileName, pSrcFileName, pControl );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Copies the contents of the save directory into a single file
	//-----------------------------------------------------------------------------
	void DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave )
	{
		SaveMsg( "DirectoryCopy....\n");

		CUtlVector<filelistelem_t> list;

		// force the writes to finish before trying to get the size/existence of a file
		// @TODO: don't need this if retain sizes for files written earlier in process
		SaveMsg( "DirectoryCopy: AsyncFinishAllWrites\n");
		g_pFileSystem->AsyncFinishAllWrites();

		// build the directory list
		char basefindfn[ MAX_PATH ];
		const char *findfn = Sys_FindFirstEx(pPath, "DEFAULT_WRITE_PATH", basefindfn, sizeof( basefindfn ) );
		while ( findfn )
		{
			int index = list.AddToTail();
			memset( list[index].szFileName, 0, sizeof(list[index].szFileName) );
			Q_strncpy( list[index].szFileName, findfn, sizeof(list[index].szFileName) );

			findfn = Sys_FindNext( basefindfn, sizeof( basefindfn ) );
		}
		Sys_FindClose();

		// write the list of files to the save file
		char szName[MAX_PATH];
		for ( int i = 0; i < list.Count(); i++ )
		{
			if ( !bIsXSave )
			{
				Q_snprintf( szName, sizeof( szName ), "%s%s", saverestore->GetSaveDir(), list[i].szFileName );
			}
			else
			{
				Q_snprintf( szName, sizeof( szName ), "%s:\\%s", GetCurrentMod(), list[i].szFileName );
			}

			Q_FixSlashes( szName );

			int fileSize = g_pFileSystem->Size( szName );
			if ( fileSize )
			{
				Assert( sizeof(list[i].szFileName) == MAX_PATH );

				SaveMsg( "DirectoryCopy: AsyncAppend %s, %s\n", szName, pDestFileName );
				g_pFileSystem->AsyncAppend( pDestFileName, memcpy( new char[MAX_PATH], list[i].szFileName, MAX_PATH), MAX_PATH, true );		// Filename can only be as long as a map name + extension
				g_pFileSystem->AsyncAppend( pDestFileName, new int(fileSize), sizeof(int), true );
				g_pFileSystem->AsyncAppendFile( pDestFileName, szName );
			}
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Extracts all the files contained within pFile
	//-----------------------------------------------------------------------------
	bool DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave )
	{
		int				fileSize;
		FileHandle_t	pCopy;
		char			szName[ MAX_PATH ], fileName[ MAX_PATH ];
		bool			success = true;

		for ( int i = 0; i < fileCount && success; i++ )
		{
			// Filename can only be as long as a map name + extension
			if ( g_pSaveRestoreFileSystem->Read( fileName, MAX_PATH, pFile ) != MAX_PATH )
				return false;

			if ( g_pSaveRestoreFileSystem->Read( &fileSize, sizeof(int), pFile ) != sizeof(int) )
				return false;

			if ( !fileSize )
				return false;

			if ( !bIsXSave )
			{
				Q_snprintf( szName, sizeof( szName ), "%s%s", saverestore->GetSaveDir(), fileName );
			}
			else
			{
				Q_snprintf( szName, sizeof( szName ), "%s:\\%s", GetCurrentMod(), fileName );
			}

			Q_FixSlashes( szName );
			pCopy = g_pSaveRestoreFileSystem->Open( szName, "wb", "MOD" );
			if ( !pCopy )
				return false;
			success = FileCopy( pCopy, pFile, fileSize );
			g_pSaveRestoreFileSystem->Close( pCopy );
		}

		return success;
	}

	//-----------------------------------------------------------------------------
	// Purpose: returns the number of files in the specified filter
	//-----------------------------------------------------------------------------
	int DirectoryCount( const char *pPath )
	{
		int count = 0;
		const char *findfn = Sys_FindFirstEx( pPath, "DEFAULT_WRITE_PATH", NULL, 0 );

		while ( findfn != NULL )
		{
			count++;
			findfn = Sys_FindNext(NULL, 0 );
		}
		Sys_FindClose();

		return count;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Clears the save directory of all temporary files (*.hl)
	//-----------------------------------------------------------------------------
	void DirectoryClear( const char *pPath, bool bIsXSave )
	{
		char const	*findfn;
		char		szPath[ MAX_PATH ];
		
		findfn = Sys_FindFirstEx( pPath, "DEFAULT_WRITE_PATH", NULL, 0 );
		while ( findfn != NULL )
		{
			if ( !bIsXSave )
			{
				Q_snprintf( szPath, sizeof( szPath ), "%s%s", saverestore->GetSaveDir(), findfn );
			}
			else
			{
				Q_snprintf( szPath, sizeof( szPath ), "%s:\\%s", GetCurrentMod(), findfn );
			}

			// Delete the temporary save file
			g_pFileSystem->RemoveFile( szPath, "MOD" );

			// Any more save files
			findfn = Sys_FindNext( NULL, 0 );
		}
		Sys_FindClose();
	}

	void AuditFiles( void )
	{
		Msg("Not using save-in-memory path!\n" );
	}

	bool LoadFileFromDisk( const char *pFilename )
	{
		Msg("Not using save-in-memory path!\n" );
		return true;
	}

private:
	int m_iContainerOpens;
};

static CSaveRestoreFileSystem				s_SaveRestoreFileSystem;
static CSaveRestoreFileSystemPassthrough	s_SaveRestoreFileSystemPassthrough;

#ifdef _X360
ISaveRestoreFileSystem *g_pSaveRestoreFileSystem = &s_SaveRestoreFileSystem;
#else
ISaveRestoreFileSystem *g_pSaveRestoreFileSystem = &s_SaveRestoreFileSystemPassthrough;
#endif // _X360

//-----------------------------------------------------------------------------
// Purpose: Called when switching between saving in memory and saving to disk.
//-----------------------------------------------------------------------------
void SaveInMemoryCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( !IsX360() )
	{
		Warning( "save_in_memory is compatible with only the Xbox 360!\n" );
		return;
	}

	ConVarRef var( pConVar );
	if ( var.GetFloat() == flOldValue )
		return;

	// *.hl? files are transferred between disk and memory when this cvar changes
	char szPath[ MAX_PATH ];
	Q_snprintf( szPath, sizeof( szPath ), "%s%s", saverestore->GetSaveDir(), "*.hl?" );
	if ( var.GetBool() )
	{
		g_pSaveRestoreFileSystem = &s_SaveRestoreFileSystem;

		// Clear memory and load
		s_SaveRestoreFileSystem.DirectoryClear( "*.hl?", IsX360() );
		s_SaveRestoreFileSystem.LoadSaveDirectoryFromDisk( szPath );

		// Clear disk
		s_SaveRestoreFileSystemPassthrough.DirectoryClear( szPath, IsX360() );
	}
	else
	{
		g_pSaveRestoreFileSystem = &s_SaveRestoreFileSystemPassthrough;

		// Clear disk and write
		s_SaveRestoreFileSystemPassthrough.DirectoryClear( szPath, IsX360() );
		s_SaveRestoreFileSystem.WriteSaveDirectoryToDisk();

		// Clear memory
		s_SaveRestoreFileSystem.DirectoryClear( "*.hl?", IsX360() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dump a list of the save directory contents (in memory) to the console
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystem::DumpSaveDirectory( void )
{
	unsigned int totalCompressedSize = 0;
	unsigned int totalUncompressedSize = 0;
	for ( int i = GetDirectory().FirstInorder(); GetDirectory().IsValidIndex( i ); i = GetDirectory().NextInorder( i ) )
	{
		SaveFile_t &file = GetFile( i );
		Msg( "File %d: %s Size:%d\n", i, GetString( file.name ), file.nCompressedSize );
		totalUncompressedSize += file.nSize;
		totalCompressedSize += file.nCompressedSize;
	}
	float percent = 0.f;
	if ( totalUncompressedSize )
		percent = 100.f - (totalCompressedSize / totalUncompressedSize * 100.f);
	Msg( "Total Size: %.2f Mb (%d bytes)\n", totalCompressedSize / (1024.f*1024.f), totalCompressedSize );
	Msg( "Compression: %.2f Mb to %.2f Mb (%.0f%%)\n", totalUncompressedSize/(1024.f*1024.f), totalCompressedSize/(1024.f*1024.f), percent );
}

CON_COMMAND( dumpsavedir, "List the contents of the save directory in memory" )
{
	s_SaveRestoreFileSystem.DumpSaveDirectory();
}

