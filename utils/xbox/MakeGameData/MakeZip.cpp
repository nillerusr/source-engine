//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "MakeGameData.h"

static CUtlSymbolTable g_CriticalPreloadTable( 0, 32, true );

//-----------------------------------------------------------------------------
// Purpose: Compute preload data by file type. Calls into appropriate libraries
//			to get the preload info. Libraries would use filename to generate
//			the preload if there is a compilation step, otherwise the file buffer
//			is a buffer loaded filename.
//-----------------------------------------------------------------------------
static bool GetPreloadBuffer( const char *pFilename, CUtlBuffer &fileBuffer, CUtlBuffer &preloadBuffer )
{
	char fileExtension[MAX_PATH];
	Q_ExtractFileExtension( pFilename, fileExtension, sizeof( fileExtension ) );

	// adding an entire file IS ONLY for files that are expected to be read by the game as a single read
	// NOT for files that have any seek pattern
	bool bAddEntireFile = false;

	// trivial small files, always add
	if ( !Q_stricmp( fileExtension, "txt" ) ||
		!Q_stricmp( fileExtension, "dat" ) ||
		!Q_stricmp( fileExtension, "lst" ) ||
		!Q_stricmp( fileExtension, "res" ) ||
		!Q_stricmp( fileExtension, "vmt" ) ||
		!Q_stricmp( fileExtension, "cfg" ) ||
		!Q_stricmp( fileExtension, "bnf" ) ||
		!Q_stricmp( fileExtension, "rc" ) ||
		!Q_stricmp( fileExtension, "vbf" ) ||
		!Q_stricmp( fileExtension, "vfe" ) ||
		!Q_stricmp( fileExtension, "pcf" ) ||
		!Q_stricmp( fileExtension, "inf" ) )
	{
		bAddEntireFile = true;
	}

	// critical resources get blindly added to the preload
	if ( !bAddEntireFile && ( g_CriticalPreloadTable.Find( pFilename ) != UTL_INVAL_SYMBOL ) )
	{
		bAddEntireFile = true;
	}

	if ( bAddEntireFile && LZMA_IsCompressed( (unsigned char *)fileBuffer.Base() ) )
	{
		// sorry, not allowed to add entirely to preload if already compressed
		// breaks the run-time filesystem due to inability to deliver file as-is
		bAddEntireFile = false;
	}

	if ( bAddEntireFile )
	{
		if ( fileBuffer.TellMaxPut() >= 1*1024 )
		{
			// only compress preload files of reasonable size
			unsigned int compressedSize = 0;
			unsigned char *pCompressedOutput = LZMA_OpportunisticCompress( (unsigned char *)fileBuffer.Base(),
			                                                               fileBuffer.TellMaxPut(), &compressedSize );
			if ( pCompressedOutput )
			{
				// add as compressed
				preloadBuffer.EnsureCapacity( compressedSize );
				preloadBuffer.Put( pCompressedOutput, compressedSize );
				free( pCompressedOutput );
				return true;
			}
		}

		// add entire file to preload section
		preloadBuffer.EnsureCapacity( fileBuffer.TellMaxPut() );
		preloadBuffer.Put( fileBuffer.Base(), fileBuffer.TellMaxPut() );
		return true;
	}

	// Each library will fetch the optional preload data into caller's buffer
	if ( !Q_stricmp( fileExtension, "wav" ) )
	{
		return GetPreloadData_WAV( pFilename, fileBuffer, preloadBuffer );
	}
	else if ( !Q_stricmp( fileExtension, "vtf" ) )
	{
		return GetPreloadData_VTF( pFilename, fileBuffer, preloadBuffer );
	}
	else if ( !Q_stricmp( fileExtension, "vcs" ) )
	{
		return GetPreloadData_VCS( pFilename, fileBuffer, preloadBuffer );
	}
	else if ( !Q_stricmp( fileExtension, "vhv" ) )
	{
		return GetPreloadData_VHV( pFilename, fileBuffer, preloadBuffer );
	}
	else if ( !Q_stricmp( fileExtension, "vtx" ) )
	{
		return GetPreloadData_VTX( pFilename, fileBuffer, preloadBuffer );
	}
	else if ( !Q_stricmp( fileExtension, "vvd" ) )
	{
		return GetPreloadData_VVD( pFilename, fileBuffer, preloadBuffer );
	}

	// others...
	return false;
}

void SetupCriticalPreloadScript( const char *pModPath )
{
	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// purge any prior entries
	g_CriticalPreloadTable.RemoveAll();

	// populate table
	char szCriticaList[MAX_PATH];
	char szToken[MAX_PATH];
	V_ComposeFileName( pModPath, "scripts\\preload_xbox.xsc", szCriticaList, sizeof( szCriticaList ) );
	CUtlBuffer criticalListBuffer;
	if ( ReadFileToBuffer( szCriticaList, criticalListBuffer, true, true ) )
	{
		for ( ;; )
		{
			int nTokenSize = criticalListBuffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
			if ( nTokenSize <= 0 )
			{
				break;
			}
		
			V_strlower( szToken );
			V_FixSlashes( szToken, CORRECT_PATH_SEPARATOR );
			if ( UTL_INVAL_SYMBOL == g_CriticalPreloadTable.Find( szToken ) )
			{
				g_CriticalPreloadTable.AddString( szToken );
			}
		}
	}
}

CXZipTool::CXZipTool()
{
	m_pZip = NULL;
	m_hPreloadFile = INVALID_HANDLE_VALUE;
	m_hOutputZipFile = INVALID_HANDLE_VALUE;
	m_PreloadFilename[0] = '\0';
}

CXZipTool::~CXZipTool()
{
	Reset();
}

void CXZipTool::Reset()
{
	if ( m_hOutputZipFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( m_hOutputZipFile );
		m_hOutputZipFile = INVALID_HANDLE_VALUE;
	}

	if ( m_hPreloadFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( m_hPreloadFile );
		m_hPreloadFile = INVALID_HANDLE_VALUE;
	}

	if ( m_PreloadFilename[0] )
	{
		DeleteFile( m_PreloadFilename );
		m_PreloadFilename[0] = '\0';
	}

	if ( m_pZip )
	{
		IZip::ReleaseZip( m_pZip );
		m_pZip = NULL;
	}

	m_ZipPreloadDirectoryEntries.Purge();
	m_ZipCRCList.Purge();
	m_ZipPreloadRemapEntries.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Add a file buffer to the zip
//-----------------------------------------------------------------------------
bool CXZipTool::AddBuffer( const char *pFilename, CUtlBuffer &fileBuffer, bool bDoPreload )
{
	if ( !m_pZip )
	{
		return false;
	}

	// safely strip unecessary prefix, otherise pollutes CRC
	if ( !strnicmp( pFilename, ".\\", 2 ) )
	{
		pFilename += 2;
	}

	// scan for CRC collision now, not at runtime
	CRCEntry_t	crcEntry;
	crcEntry.fileNameCRC = HashStringCaselessConventional( pFilename );
	crcEntry.filename = pFilename;
	int idx = m_ZipCRCList.Find( crcEntry );
	if ( -1 != idx  )
	{
		if ( !V_stricmp( pFilename, m_ZipCRCList[idx].filename.String() ) )
		{
			// file has already been added, ignore as succesful
			return true;
		}

		Msg( "ERROR: CRC Collision: '%s' with '%s'\n", pFilename, m_ZipCRCList[idx].filename.String() );
		return false;
	}
	else
	{
		// add unique entry to lists
		// must track filenames in a non-sort manner
		m_ZipCRCList.Insert( crcEntry );
	}

	// default, no preload entry
	unsigned short preloadDir = INVALID_PRELOAD_ENTRY;

	if ( bDoPreload )
	{
		CUtlBuffer preloadBuffer;
		bool bHasPreload = GetPreloadBuffer( pFilename, fileBuffer, preloadBuffer );
		int preloadSize = preloadBuffer.TellMaxPut();
		if ( bHasPreload && preloadSize > 0 ) 
		{
			if ( m_ZipPreloadDirectoryEntries.Count() >= 65534 )
			{
				Msg( "ERROR: Preload section FULL!, skipping %s\n", pFilename );			
				return FALSE;
			}
		
			// Initialize the entry header
			ZIP_PreloadDirectoryEntry entry;
			memset( &entry, 0, sizeof( entry ) );

			entry.Length = preloadSize;
			entry.DataOffset = SetFilePointer( m_hPreloadFile, 0, NULL, FILE_CURRENT );

			// Add the directory entry to the preload table
			preloadDir = m_ZipPreloadDirectoryEntries.AddToTail( entry );

			// Append the preload data to the preload file
			DWORD numBytesWritten;
			BOOL bOK = WriteFile( m_hPreloadFile, preloadBuffer.Base(), preloadSize, &numBytesWritten, NULL );
			if ( !bOK || preloadSize != numBytesWritten )
			{
				Msg( "ERROR: writing %d preload bytes of '%s'\n", preloadSize, pFilename );
				return false;
			}

			if ( !g_bQuiet )
			{
				// Spew it
				if ( LZMA_IsCompressed( (unsigned char *)preloadBuffer.Base() ) )
				{
					unsigned int actualSize = LZMA_GetActualSize( (unsigned char *)preloadBuffer.Base() );
					Msg( "Preload: '%s': Compressed:%u Actual:%u\n", pFilename, preloadSize, actualSize );
				}
				else
				{
					Msg( "Preload: '%s': Length:%u\n", pFilename, preloadSize );
				}
			}
		}
	}

	unsigned int fileSize = fileBuffer.TellMaxPut();
	if ( fileSize > 0 )
	{
		// order in zip is sorted, not sequential
		m_pZip->AddBufferToZip( pFilename, fileBuffer.Base(), fileSize, false );

		// track the file in the zip directory to it's preload entry
		// order in preload is sequential as buffers are added
		preloadRemap_t	remap;
		remap.filename = pFilename;
		remap.preloadDirIndex = preloadDir;
		m_ZipPreloadRemapEntries.AddToTail( remap );

		if ( !g_bQuiet )
		{
			Msg( "File: '%s': Length:%u\n", pFilename, fileSize );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Load a file and add it to the zip
//-----------------------------------------------------------------------------
bool CXZipTool::AddFile( const char *pFilename, bool bDoPreload )
{
	if ( !m_pZip )
	{
		return false;
	}

	FILE* pFile = fopen( pFilename, "rb" );
	if( !pFile )
	{
		Msg( "ERROR: failed to open file: '%s'\n", pFilename );
		return false;
	}

	// Get the length of the file
	fseek( pFile, 0, SEEK_END );
	unsigned fileSize = ftell( pFile );
	fseek( pFile, 0, SEEK_SET);

	// read file to buffer
	CUtlBuffer fileBuffer;
	fileBuffer.EnsureCapacity( fileSize );
	fread( fileBuffer.Base(), fileSize, 1, pFile );
	fclose( pFile );
	fileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, fileSize );

	return AddBuffer( pFilename, fileBuffer, bDoPreload );
}

//-----------------------------------------------------------------------------
// Purpose: Add the preload section and save out the zip file
//-----------------------------------------------------------------------------
bool CXZipTool::End()
{
	if ( !m_pZip )
	{
		return false;
	}

	// Add the preload section to the zip
	if ( m_ZipPreloadDirectoryEntries.Count() )
	{
		CUtlBuffer sectionBuffer;
		CByteswap byteSwap;

		// pc tools write 360 native data
		byteSwap.ActivateByteSwapping( IsPC() );

		// determine the preload data footprint
		unsigned int preloadDataSize = SetFilePointer( m_hPreloadFile, 0, NULL, FILE_CURRENT );

		// finalize header
		m_ZipPreloadHeader.DirectoryEntries = m_ZipPreloadRemapEntries.Count();
		m_ZipPreloadHeader.PreloadDirectoryEntries = m_ZipPreloadDirectoryEntries.Count();

		// determine the total section size ( treated as a single file inside zip )
		unsigned int sectionSize = sizeof( ZIP_PreloadHeader ) + 
						m_ZipPreloadHeader.PreloadDirectoryEntries * sizeof( ZIP_PreloadDirectoryEntry ) +
						m_ZipPreloadHeader.DirectoryEntries * sizeof( unsigned short ) +
						preloadDataSize;
		sectionSize = AlignValue( sectionSize, m_ZipPreloadHeader.Alignment );
		sectionBuffer.EnsureCapacity( sectionSize );

		// save data in order
		// save the header
		byteSwap.SwapFieldsToTargetEndian( &m_ZipPreloadHeader );
		sectionBuffer.Put( &m_ZipPreloadHeader, sizeof( ZIP_PreloadHeader ) );

		// fixup and save the preload directory
		for ( int i=0; i<m_ZipPreloadDirectoryEntries.Count(); i++ )
		{
			ZIP_PreloadDirectoryEntry entry = m_ZipPreloadDirectoryEntries[i];
			byteSwap.SwapFieldsToTargetEndian( &entry );
			sectionBuffer.Put( &entry, sizeof( ZIP_PreloadDirectoryEntry ) );
		}

		// generate remap table
		char			fileName[MAX_PATH];
		int				fileSize;
		int				zipIndex = -1;
		unsigned short	*pRemapTable = (unsigned short *)malloc( m_ZipPreloadRemapEntries.Count() * sizeof( unsigned short ) );
		for ( int i=0; i<m_ZipPreloadRemapEntries.Count(); i++ )
		{
			// zip files get iterated in the same order they are serialized to disk
			fileName[0] = '\0';
			fileSize = 0;
			zipIndex = m_pZip->GetNextFilename( zipIndex, fileName, sizeof( fileName ), fileSize );

			// find the file in the preload remap table
			bool bFound = false;
			int j;
			for ( j=0; j<m_ZipPreloadRemapEntries.Count(); j++ )
			{
				if ( !Q_stricmp( fileName, m_ZipPreloadRemapEntries[j].filename.String() ) )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
			{
				// shouldn't happen, every file in the zip has a matching preload remap entry, that is valid or marked invalid
				Msg( "ERROR: file '%s' was expected to have an entry in preload table\n", fileName );
			}

			// the remap table is used to go find a file's preload data (if available)
			pRemapTable[i] = m_ZipPreloadRemapEntries[j].preloadDirIndex;
		}	
		for ( int i=0; i<m_ZipPreloadRemapEntries.Count(); i++ )
		{
			unsigned short s = pRemapTable[i];
			sectionBuffer.PutShort( BigShort( s ) );
		}
		free( pRemapTable );		

		// get and save preload data
		void *pPreloadData = malloc( preloadDataSize );
		SetFilePointer( m_hPreloadFile, 0, NULL, FILE_BEGIN );
		DWORD numBytesRead;
		BOOL bOK = ReadFile( m_hPreloadFile, pPreloadData, preloadDataSize, &numBytesRead, NULL );
		if ( !bOK || numBytesRead != preloadDataSize )
		{
			Msg( "ERROR: failed to read %d bytes from temporary preload file\n", preloadDataSize );		
		}
		CloseHandle( m_hPreloadFile );
		m_hPreloadFile = INVALID_HANDLE_VALUE;
		
		sectionBuffer.Put( pPreloadData, preloadDataSize );
		free( pPreloadData );

		// cannot have written more than was pre-calced, unless code was broken
		Assert( (unsigned int)sectionBuffer.TellMaxPut() <= sectionSize );
		while( (unsigned int)sectionBuffer.TellMaxPut() < sectionSize )
		{
			// pad to final alignment
			sectionBuffer.PutChar( 0 );	
		}

		m_pZip->AddBufferToZip( PRELOAD_SECTION_NAME, sectionBuffer.Base(), sectionBuffer.TellMaxPut(), false );
	}
	else
	{
		// Clear the preload section placeholder
		m_pZip->RemoveFileFromZip( PRELOAD_SECTION_NAME );
	}

	m_pZip->SaveToDisk( m_hOutputZipFile );

	Reset();
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Create the zip file
//-----------------------------------------------------------------------------
bool CXZipTool::Begin( const char *pZipFileName, unsigned int alignment )
{
	// get the volume of the target zip
	char drivePath[MAX_PATH];
	_splitpath( pZipFileName, drivePath, NULL, NULL, NULL );

	m_pZip = IZip::CreateZip( drivePath, true );

	if ( alignment )
	{
		// making an aligned zip that uses an optimized (but incompatible) format
		m_pZip->ForceAlignment( true, false, alignment );
	}

	// Open the output file
	m_hOutputZipFile = CreateFile( pZipFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( m_hOutputZipFile == INVALID_HANDLE_VALUE )
	{
		Msg( "ERROR: failed to create zip file '%s'\n", pZipFileName );
		return false;
	}
	
	// Create a temporary file for storing the preloaded data
	scriptlib->MakeTemporaryFilename( g_szModPath, m_PreloadFilename, sizeof( m_PreloadFilename ) );	
	m_hPreloadFile = CreateFile( m_PreloadFilename, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( m_hPreloadFile == INVALID_HANDLE_VALUE )
	{
		Msg( "ERROR: failed to create temporary file '%s' for preload data\n", m_PreloadFilename );
		CloseHandle( m_hOutputZipFile );
		m_hOutputZipFile = INVALID_HANDLE_VALUE;
		return false;
	}
	memset( &m_ZipPreloadHeader, 0, sizeof( ZIP_PreloadHeader ) );

	m_ZipPreloadHeader.Version = PRELOAD_HDR_VERSION;
	m_ZipPreloadHeader.Alignment = alignment;

	// Add a placeholder for the preload section
	m_pZip->AddBufferToZip( PRELOAD_SECTION_NAME, NULL, 0, false );
	preloadRemap_t remap;
	remap.filename = PRELOAD_SECTION_NAME;
	remap.preloadDirIndex = INVALID_PRELOAD_ENTRY;
	m_ZipPreloadRemapEntries.AddToTail( remap );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Dump the preload contents
//-----------------------------------------------------------------------------
void CXZipTool::SpewPreloadInfo( const char *pZipName )
{
	IZip *pZip = IZip::CreateZip( NULL, true );
	
	HANDLE hZipFile = pZip->ParseFromDisk( pZipName );
	if ( !hZipFile )
	{
		Msg( "Bad or missing zip file, failed to open '%s'\n", pZipName );
		return;
	}

	CUtlBuffer preloadBuffer;
	if ( !pZip->ReadFileFromZip( hZipFile, PRELOAD_SECTION_NAME, false, preloadBuffer ) )
	{
		Msg( "No preload info for '%s'\n", pZipName );
		return;
	}

	preloadBuffer.ActivateByteSwapping( IsPC() );

	ZIP_PreloadHeader header;
	preloadBuffer.GetObjects( &header );

	// get the dir table
	ZIP_PreloadDirectoryEntry *pDir = (ZIP_PreloadDirectoryEntry *)malloc( header.PreloadDirectoryEntries * sizeof( ZIP_PreloadDirectoryEntry ) );
	preloadBuffer.GetObjects( pDir, header.PreloadDirectoryEntries );

	// get the remap table
	unsigned short *pRemap = (unsigned short *)malloc( header.DirectoryEntries * sizeof( unsigned short ) );
	for ( unsigned int i = 0; i < header.DirectoryEntries; i++ )
	{
		pRemap[i] = preloadBuffer.GetShort();
	}

	int zipIndex = -1;
	int fileSize;
	char fileName[MAX_PATH];

	// iterate preload entries sequentially
	CUtlDict< unsigned int, int > sizes( true );
	for ( unsigned int i = 0; i < header.DirectoryEntries; i++ )
	{
		fileName[0] = '\0';
		fileSize = 0;
		zipIndex = pZip->GetNextFilename( zipIndex, fileName, sizeof( fileName ), fileSize );

		unsigned short zipPreloadDirIndex = pRemap[i];
		if ( zipPreloadDirIndex == INVALID_PRELOAD_ENTRY )
		{
			continue;
		}

		Msg( "Offset: 0x%8.8x Length: %5d %s (%d)\n", pDir[zipPreloadDirIndex].DataOffset, pDir[zipPreloadDirIndex].Length, fileName, fileSize );

		// total preload sizes by extension
		const char *pExt = V_GetFileExtension( fileName );
		if ( !pExt )
		{
			pExt = "???";
		}
		int iIndex = sizes.Find( pExt );
		if ( iIndex == sizes.InvalidIndex() )
		{
			iIndex = sizes.Insert( pExt );
			sizes[iIndex] = 0;
		}
		sizes[iIndex] += pDir[zipPreloadDirIndex].Length;
	}

	Msg( "\n" );
	Msg( "Preload Size:    %.2f MB\n", (float)preloadBuffer.TellMaxPut()/(1024.0f * 1024.0f) );
	Msg( "Zip Entries:     %d\n", header.DirectoryEntries );
	Msg( "Preload Entries: %d\n", header.PreloadDirectoryEntries );

	// dump each extension's total size, necessary for debugging who is the largest contributor
	for ( int i = 0; i < sizes.Count(); i++ )
	{
		Msg( "Extension: '%3s' %d bytes (%.2f%s)\n", sizes.GetElementName( i ), sizes[i], (float)sizes[i]/(float)preloadBuffer.TellMaxPut() * 100.0f, "%%" );
	}
	Msg( "\n" );

	free( pRemap );
	free( pDir );

	IZip::ReleaseZip( pZip );
}
