//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

// Let's make sure aserts, etc are enabled for this tool
#define RELEASEASSERTS
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "vpklib/packedstore.h"
#include "mathlib/mathlib.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"
#include "tier2/fileutils.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#ifdef VPK_ENABLE_SIGNING
#include "crypto.h"
#endif

static bool s_bBeVerbose = false;
static bool s_bMakeMultiChunk = false;
static bool s_bUseSteamPipeFriendlyBuilder = false;
static int s_iMultichunkSize = k_nVPKDefaultChunkSize / ( 1024 * 1024 );
const int k_nVPKDefaultChunkAlign = 1;
static int s_iChunkAlign = k_nVPKDefaultChunkAlign;
static CUtlString s_sPrivateKeyFile;
static CUtlString s_sPublicKeyFile;

static void PrintArgSummaryAndExit( int iReturnCode = 1 )
{
	fflush(stderr);
	printf(
		"Usage: vpk [options] <command> <command arguments ...>\n"
		"       vpk [options] <directory>\n"
		"       vpk [options] <vpkfile>\n"
		"\n"
		"CREATE VPK / ADD FILES:\n"
		"  vpk <dirname>\n"
		"         Creates a pack file named <dirname>.vpk located\n"
		"         in the parent of the specified directory.\n"
		"  vpk a <vpkfile> <filename1> <filename2> ...\n"
		"         Add file(s).\n"
		"  vpk a <vpkfile> @<filename>\n"
		"         Add files listed in a response file.\n"
		"  vpk k <vpkfile> <keyvalues_filename>\n"
		"         Add files listed in a keyvalues control file.\n"
		"  vpk <directory>\n"
		"         Create VPK from directory structure.  (This is invoked when\n"
		"         a directory is dragged onto the VPK tool.)\n"
		"\n"
		"EXTRACT FILES:\n"
		"  vpk x <vpkfile> <filename1> <filename2> ...\n"
		"         Extract file(s).\n"
		"  vpk <vpkfile>\n"
		"         Extract all files from VPK.  (This is invoked when\n"
		"         a .VPK file is dragged onto the VPK tool.)\n"
		"\n"
		"DISPLAY VPK INFO:\n"
		"  vpk l <vpkfile>\n"
		"         List contents of VPK.\n"
		"  vpk L <vpkfile>\n"
		"         List contents (detailed) of VPK.\n"
#ifdef VPK_ENABLE_SIGNING
		"  vpk dumpsig <vpkfile>\n"
		"         Display signature information of VPK file\n"
		"\n"
		"VPK INTEGRITY / SECURITY:\n"
		"  vpk checkhash <vpkfile>\n"
		"            Check all VPK chunk MD5's and file CRC's.\n"
		"  vpk checksig <vpkfile>\n"
		"            Verify signature of specified VPK file.\n"
		"            Requires -k to specify key file to use.\n"
//		"  vpk rehash <vpkfile>\n"
//		"            Recalculate chunk MD5's.  (Does not recalculate file CRC's)\n"
//		"            Can be used with -k to sign an existing unsigned VPK.\n"
		"\n"
		"MISC:\n"
		"  vpk generate_keypair <keybasename>\n"
		"            Generate public/private key file.  Output files\n"
		"            will be named <keybasemame>.publickey.vdf\n"
		"            and <keybasemame>.privatekey.vdf\n"
		"            Remember: your private key should be kept private.\n"
#endif
		"\n"
		"\n"
		"Options:\n"
		"  -v     Verbose.\n"
		"  -M     Produce a multi-chunk pack file\n"
		"  -P     Use SteamPipe-friendly incremental build algorithm.\n"
		"         Use with 'k' command.\n"
		"         For optimal incremental build performance, the control file used\n"
		"         for the previous build should exist and be named the same as the\n"
		"         input control file, with '.bak' appended, and each file entry\n"
		"         should have an 'md5' value.  The 'md5' field need not be the\n"
		"         actual MD5 of the file contents, it is just a unique identifier\n"
		"         that will be compared to determine if the file contents has changed\n"
		"         between builds.\n"
		"         This option implies -M\n" );
	printf(
		"  -c <size>\n"
		"         Use specified chunk size (in MB).  Default is %d.\n", k_nVPKDefaultChunkSize / ( 1024 * 1024 ) );
	printf(
		"  -a <align>\n"
		"         Align files within chunk on n-byte boundary.  Default is %d.\n", k_nVPKDefaultChunkAlign );
#ifdef VPK_ENABLE_SIGNING
	printf(
		"  -K <private keyfile>\n"
		"         With commands 'a' or 'k': Sign VPK with specified private key.\n"
		"  -k <public keyfile>\n"
		"         With commands 'a' or 'k': Public key that will be distributed\n"
		"              and used by third parties to verify signatures.\n"
		"         With command 'checksig': Check signature using specified key file.\n" );
#endif
	exit( iReturnCode );
}

bool IsRestrictedFileType( const char *pcFileName )
{
	return ( V_stristr( pcFileName, ".bat" ) || V_stristr( pcFileName, ".cmd" ) || V_stristr( pcFileName, ".com" ) || V_stristr( pcFileName, ".dll" ) ||
			 V_stristr( pcFileName, ".exe" ) || V_stristr( pcFileName, ".msi" ) || V_stristr( pcFileName, ".rar" ) || V_stristr( pcFileName, ".reg" ) ||
			 V_stristr( pcFileName, ".zip" ) );
}

void ReadFile( char const *pName )
{
	FileHandle_t f = g_pFullFileSystem->Open( pName, "rb" );
	if ( f )
	{
		int fileSize = g_pFullFileSystem->Size( f );
		unsigned bufSize = ((IFileSystem *)g_pFullFileSystem)->GetOptimalReadSize( f, fileSize );
		void *buffer = ((IFileSystem *)g_pFullFileSystem)->AllocOptimalReadBuffer( f, bufSize );
		// read into local buffer
		( ((IFileSystem *)g_pFullFileSystem)->ReadEx( buffer, bufSize, fileSize, f ) != 0 );
		g_pFullFileSystem->Close( f );	// close file after reading
		((IFileSystem *)g_pFullFileSystem)->FreeOptimalReadBuffer( buffer );
	}
}


void BenchMark( CUtlVector<char *> &names )
{
	for( int i = 0; i < names.Count(); i++ )
		ReadFile( names[i] );
}

static void AddFileToPack( CPackedStore &mypack, char const *pSrcName, int nPreloadSize = 0, char const *pDestName = NULL )
{
	// Check to make sure that no restricted file types are being added to the VPK
	if ( IsRestrictedFileType( pSrcName ) )
	{
		printf( "Ignoring %s: unsupported file type.\n", pSrcName );
		return;
	}

	// !FIXME! Make sure they didn't request alignment, because we aren't doing it.
	if ( s_iChunkAlign != 1 )
		Error( "-a is only supported with -P" );

	if ( (! pDestName ) || ( pDestName[0] == 0 ) )
	{
		pDestName = pSrcName;
	}
	CRequiredInputFile f( pSrcName );
	int fileSize = f.Size();
	uint8 *pData = new uint8[fileSize];
	f.MustRead( pData, fileSize );

	ePackedStoreAddResultCode rslt = mypack.AddFile( pDestName, Min( fileSize, nPreloadSize ), pData, fileSize, s_bMakeMultiChunk );

	if ( rslt == EPADD_ERROR )
	{
		Error( "Error adding %s\n", pSrcName );
	}
	if ( s_bBeVerbose )
	{
		switch( rslt )
		{
			case EPADD_ADDSAMEFILE:
			{
				if ( s_bBeVerbose )
				{
					printf( "File %s is already in the archive with the same contents\n", pSrcName );
				}
			}
			break;

			case EPADD_UPDATEFILE:
			{
				if ( s_bBeVerbose )
				{
					printf( "File %s is already in the archive and has been updated\n", pSrcName );
				}
			}
			break;

			case EPADD_NEWFILE:
			{
				if ( s_bBeVerbose )
				{
					printf( "Add new file %s\n", pSrcName );
				}
			}
			break;
		}
	}

	delete[] pData;
}

#ifdef VPK_ENABLE_SIGNING
static void LoadKeyFile( const char *pszFilename, const char *pszTag, CUtlVector<uint8> &outBytes )
{
	KeyValuesAD kv("key");
	if ( !kv->LoadFromFile( g_pFullFileSystem, pszFilename ) )
		Error( "Failed to load key file %s", pszFilename );
	const char *pszType = kv->GetString( "type", NULL );
	if ( pszType == NULL )
		Error( "Key file %s is missing 'type'", pszFilename );
	if ( V_stricmp( pszType, "rsa" ) != 0 )
		Error( "Key type '%s' is not supported", pszType );
	const char *pszEncodedBytes = kv->GetString( pszTag, NULL );
	if ( pszEncodedBytes == NULL )
		Error( "Key file is missing '%s'", pszTag );

	uint8 rgubDecodedData[k_nRSAKeyLenMax*2];
	uint cubDecodedData = Q_ARRAYSIZE( rgubDecodedData );
	if( !CCrypto::HexDecode( pszEncodedBytes, rgubDecodedData, &cubDecodedData ) || cubDecodedData <= 0 )
		Error( "Key file contains invalid '%s' value", pszTag );

	outBytes.SetSize( cubDecodedData );
	V_memcpy( outBytes.Base(), rgubDecodedData, cubDecodedData );
}
#endif

static void CheckLoadKeyFilesForSigning( CPackedStore &mypack )
{

	// Not signing?
	if ( s_sPrivateKeyFile.IsEmpty() && s_sPublicKeyFile.IsEmpty() )
		return;

	// Signatures only supported if creating multi-chunk file
	if ( !s_bMakeMultiChunk )
	{
		Error( "Multichunk not specified. Only multi-chunk VPK's support signatures.\n" );
	}

	// If they specified one, they must specify both
	if ( s_sPrivateKeyFile.IsEmpty() || s_sPublicKeyFile.IsEmpty() )
		Error( "Must specify both public and private key files in order to sign VPK" );

	#ifdef VPK_ENABLE_SIGNING

		CUtlVector<uint8> bytesPrivateKey;
		LoadKeyFile( s_sPrivateKeyFile, "rsa_private_key", bytesPrivateKey );
		printf( "Loaded private key file %s\n", s_sPrivateKeyFile.String() );

		CUtlVector<uint8> bytesPublicKey;
		LoadKeyFile( s_sPublicKeyFile, "rsa_public_key", bytesPublicKey );
		printf( "Loaded public key file %s\n", s_sPublicKeyFile.String() );

		mypack.SetKeysForSigning( bytesPrivateKey.Count(), bytesPrivateKey.Base(), bytesPublicKey.Count(), bytesPublicKey.Base() );
	#else
		Error( "VPK signing not implemented" );
	#endif
}

class VPKBuilder
{
public:
	VPKBuilder( CPackedStore &packfile );
	~VPKBuilder();
	void BuildFromInputKeys()
	{
		if ( s_bUseSteamPipeFriendlyBuilder )
			BuildSteamPipeFriendlyFromInputKeys();
		else
			BuildOldSchoolFromInputKeys();
	}
	void SetInputKeys( KeyValues *pInputKeys, const char *pszControlFilename );
	void LoadInputKeys( const char *pszControlFilename );

private:

	CPackedStore &m_packfile;

	struct VPKBuildFile_t
	{
		VPKBuildFile_t()
		{
			m_pOld = NULL;
			m_pNew = NULL;
			m_iOldSortIndex = -1;
			m_iNewSortIndex = -1;
			m_md5Old.Zero();
			m_md5New.Zero();
			m_pOldKey = NULL;
			m_pNewKey = NULL;
		}

		VPKContentFileInfo_t *m_pOld;
		VPKContentFileInfo_t *m_pNew;
		int m_iOldSortIndex;
		int m_iNewSortIndex;

		KeyValues *m_pOldKey;
		KeyValues *m_pNewKey;
		MD5Value_t m_md5Old;
		MD5Value_t m_md5New;
		CUtlString m_sNameOnDisk;
	};

	static int CompareBuildFileByOldPhysicalPosition( VPKBuildFile_t* const *pa, VPKBuildFile_t* const *pb )
	{
		const VPKContentFileInfo_t *a = (*pa)->m_pOld;
		const VPKContentFileInfo_t *b = (*pb)->m_pOld;
		if ( a->m_idxChunk < b->m_idxChunk ) return -1;
		if ( a->m_idxChunk > b->m_idxChunk ) return +1;
		if ( a->m_iOffsetInChunk < b->m_iOffsetInChunk ) return -1;
		if ( a->m_iOffsetInChunk > b->m_iOffsetInChunk ) return +1;
		return 0;
	}

	CUtlString m_sControlFilename;

	/// List of all files, past and present, keyed by the name in the VPK.
	CUtlDict<VPKBuildFile_t> m_dictFiles;

	/// All files as they existed in the old VPK.  (Empty if we are building from scratch.)
	CUtlVector<VPKContentFileInfo_t> m_vecOldFiles;

	/// List of all new files.
	CUtlVector<VPKContentFileInfo_t *> m_vecNewFiles;

	/// List of new files, in the requested order, only counting those
	/// that will actually go into a chunk
	CUtlVector<VPKContentFileInfo_t *> m_vecNewFilesInChunkOrder;

	/// List of old files that have some content in a chunk file,
	/// in the order they currently appear
	CUtlVector<VPKBuildFile_t *> m_vecOldFilesInChunkOrder;

	int64 m_iNewTotalFileSize;
	int64 m_iNewTotalFileSizeInChunkFiles;

	/// A group of files that are contiguous in the logical linear
	/// file list.
	struct VPKInputFileRange_t
	{
		int m_iFirstInputFile; // index of first input file in the chunk
		int m_iLastInputFile; // index of last input file in the chunk
		int m_iChunkFilenameIndex;
		bool m_bKeepExistingFile;
		int64 m_nTotalSizeInChunkFile;

		int FileCount() const
		{
			int iResult = m_iLastInputFile - m_iFirstInputFile + 1;
			Assert( iResult > 0 );
			return iResult;
		}

	};

	KeyValues *m_pInputKeys;
	KeyValues *m_pOldInputKeys;

	CUtlLinkedList<VPKInputFileRange_t,int> m_llFileRanges;
	CUtlVector<int> m_vecRangeForChunk;
	CUtlString m_sReasonToForceWriteDirFile;

	void BuildOldSchoolFromInputKeys();
	void BuildSteamPipeFriendlyFromInputKeys();
	void SanityCheckRanges();
	void SplitRangeAt( int iFirstInputFile );
	void AddRange( VPKInputFileRange_t range );
	void MapRangeToChunk( int idxRange, int iChunkFilenameIndex, bool bKeepExistingFile );
	void CalculateRangeTotalSizeInChunkFile( VPKInputFileRange_t &range ) const;
	void UnmapAllRangesForChangedChunks();
	void CoaleseAllUnmappedRanges();
	void PrintRangeDebug();
	void MapAllRangesToChunks();
};

VPKBuilder::VPKBuilder( CPackedStore &packfile )
: m_packfile( packfile )
{
	CUtlVector<uint8> savePublicKey;
	savePublicKey = m_packfile.GetSignaturePublicKey();
	CheckLoadKeyFilesForSigning( m_packfile );
	if ( savePublicKey.Count() != m_packfile.GetSignaturePublicKey().Count()
		|| V_memcmp( savePublicKey.Base(), m_packfile.GetSignaturePublicKey().Base(), savePublicKey.Count() ) != 0 )
	{
		if ( m_packfile.GetSignaturePublicKey().Count() == 0 )
		{
			m_sReasonToForceWriteDirFile = "Signature removed.";
		}
		else if ( savePublicKey.Count() == 0 )
		{
			m_sReasonToForceWriteDirFile = "Signature added.";
		}
		else
		{
			m_sReasonToForceWriteDirFile = "Public key used for signing changed.";
		}
	}
	m_pInputKeys = NULL;
	m_pOldInputKeys = NULL;

	// !FIXME! Check if public key is changing so we know if we need to re-sign!
}

VPKBuilder::~VPKBuilder()
{
	if ( m_pInputKeys )
		m_pInputKeys->deleteThis();
	if ( m_pOldInputKeys )
		m_pOldInputKeys->deleteThis();
}

void VPKBuilder::BuildOldSchoolFromInputKeys()
{

	// Just add them in order
	FOR_EACH_VEC( m_vecNewFiles, i )
	{
		VPKContentFileInfo_t *f = m_vecNewFiles[ i ];
		int idxInDict = m_dictFiles.Find( f->m_sName.String() );
		Assert( idxInDict >= 0 );
		VPKBuildFile_t *bf = &m_dictFiles[ idxInDict ];
		Assert( bf->m_pNew == f );
		AddFileToPack( m_packfile, bf->m_sNameOnDisk, f->m_iPreloadSize, f->m_sName );
	}

	if ( s_bBeVerbose )
		printf( "Hashing metadata.\n" );
	m_packfile.HashMetadata();

	if ( s_bBeVerbose )
		printf( "Writing directory file.\n" );
	m_packfile.Write();
}

void VPKBuilder::BuildSteamPipeFriendlyFromInputKeys()
{

	// Get list of all files already in the VPK
	m_packfile.GetFileList( NULL, m_vecOldFiles );
	FOR_EACH_VEC( m_vecOldFiles, i )
	{
		VPKContentFileInfo_t *f = &m_vecOldFiles[i];
		char szNameInVPK[ MAX_PATH ];
		V_strcpy_safe( szNameInVPK, f->m_sName );
		V_FixSlashes( szNameInVPK, '\\' ); // always use Windows slashes in VPK
		f->m_sName = szNameInVPK;

		// Add it to the dictionary
		int idxInDict = m_dictFiles.Find( szNameInVPK );
		if ( idxInDict == m_dictFiles.InvalidIndex() )
			idxInDict = m_dictFiles.Insert( szNameInVPK );

		// Each logical file should only be in a VPK file once
		if ( m_dictFiles[ idxInDict ].m_pOld )
			Error( "File '%s' is listed in VPK directory multiple times?!  Cannot build incrementally.\n", szNameInVPK );
		m_dictFiles[ idxInDict ].m_pOld = f;
	}

	// See if we should build incrementally
	bool bIncremental = ( m_vecOldFiles.Count() > 0 ) && !m_sControlFilename.IsEmpty();
	if ( bIncremental )
	{
		printf( "Building incrementally in SteamPipe-friendly manner.\n" );
		printf( "Existing pack file contains %d files\n", m_vecOldFiles.Count() );

		CUtlString sControlFilenameBak = m_sControlFilename;
		sControlFilenameBak += ".bak";
		m_pOldInputKeys = new KeyValues( "oldkeys" );
		if ( m_pOldInputKeys->LoadFromFile( g_pFullFileSystem, sControlFilenameBak ) )
		{
			printf( "Loaded %s OK\n", sControlFilenameBak.String() );
			printf( "Fetching MD5's and checking that it matches the pack file\n" );
			for ( KeyValues *i = m_pOldInputKeys; i; i = i->GetNextKey() )
			{
				const char *pszNameOnDisk = i->GetString( "srcpath", i->GetName() );
				char szNameInVPK[ MAX_PATH ];
				V_strcpy_safe( szNameInVPK, i->GetString( "destpath", "" ) );
				if ( szNameInVPK[0] == '\0' )
					Error( "File '%s' is missing 'destpath' in old KeyValues control file", pszNameOnDisk );
				V_FixSlashes( szNameInVPK, '\\' ); // always use Windows slashes in VPK

				// Locate file build entry.  We should have one in the VPK
				int idxInDict = m_dictFiles.Find( szNameInVPK );
				if ( idxInDict == m_dictFiles.InvalidIndex() || m_dictFiles[ idxInDict ].m_pOld == NULL )
					Error( "File '%s' in old KeyValues control file not found in pack file.\nThat control file was probably not used to build the pack file\n", szNameInVPK );
				VPKBuildFile_t &bf = m_dictFiles[ idxInDict ];

				if ( bf.m_pOldKey )
					Error( "File '%s' appears multiple times in old KeyValues control file.\nThat control file was probably not used to build the pack file\n", szNameInVPK );
				bf.m_pOldKey = i;

				// Fetch preload size from old KV, clamp to actual file size.
				int iPreloadSizeFromControlFile = i->GetInt( "preloadsize", 0 );
				iPreloadSizeFromControlFile = Min( iPreloadSizeFromControlFile, (int)bf.m_pOld->m_iTotalSize );

				if ( iPreloadSizeFromControlFile != (int)bf.m_pOld->m_iPreloadSize )
					Error( "File '%s' preload size mismatch in old KeyValues control file and pack file.\nThat control file was probably not used to build the pack file\n", szNameInVPK );

				const char *pszMD5 = i->GetString( "md5", "" );
				if ( *pszMD5 )
				{
					if ( V_strlen( pszMD5 ) != MD5_DIGEST_LENGTH*2 )
						Error( "File '%s' has invalid MD5 '%s'", pszNameOnDisk, pszMD5 );
					V_hextobinary( pszMD5, MD5_DIGEST_LENGTH*2, bf.m_md5Old.bits, MD5_DIGEST_LENGTH );
				}
				else
				{
					printf( "WARNING: Old control file entry '%s' does not have an MD5; we will have to compare file contents for this file.\n", pszNameOnDisk );
				}
			}

			// Now many sure every file in the pack was found in the control file.  If not, then
			// they probably don't match and we should not trust the MD5's.
			FOR_EACH_DICT_FAST( m_dictFiles, idxInDict )
			{
				VPKBuildFile_t &bf = m_dictFiles[ idxInDict ];
				if ( bf.m_pOld && bf.m_pOldKey == NULL )
					Error( "File '%s' is in pack but not in old control file %s.\n"
						"That control file was probably not used to build the pack file", bf.m_pOld->m_sName.String(), sControlFilenameBak.String() );
			}

			printf( "%s appears to match VPK file.\nUsing MD5s for incremental building\n", sControlFilenameBak.String() );
		}
		else
		{
			printf( "WARNING: %s not present; incremental building will be slow.\n", sControlFilenameBak.String() );
			printf( "         For best results, provide the control file previously used for building.\n" );
			m_pOldInputKeys->deleteThis();
			m_pOldInputKeys = NULL;
		}
	}
	else
	{
		printf( "Building pack file from scratch.\n" );
	}

	// Dictionary is now complete.  Gather up list of files in order
	// sorted by where they were in the old pack set
	FOR_EACH_DICT_FAST( m_dictFiles, i )
	{
		VPKBuildFile_t *f = &m_dictFiles[i];
		if ( f->m_pOld && f->m_pOld->GetSizeInChunkFile() > 0 )
			m_vecOldFilesInChunkOrder.AddToTail( f );
	}
	m_vecOldFilesInChunkOrder.Sort( CompareBuildFileByOldPhysicalPosition );
	FOR_EACH_VEC( m_vecOldFilesInChunkOrder, i )
	{
		m_vecOldFilesInChunkOrder[i]->m_iOldSortIndex = i;
	}

	// How many chunks are currently in the VPK.  (Might be zero)
	int nOldChunkCount = 0;
	if ( m_vecOldFilesInChunkOrder.Count() > 0 )
		nOldChunkCount = m_vecOldFilesInChunkOrder[ m_vecOldFilesInChunkOrder.Count()-1 ]->m_pOld->m_idxChunk + 1;

	// For each chunk filename (_nnn.vpk), remember which block
	// of files maps will be used to create it.
	// None of the chunks have been assigned a block of files yet
	for ( int i = 0 ; i < nOldChunkCount ; ++i )
		m_vecRangeForChunk.AddToTail( m_llFileRanges.InvalidIndex() );

	// Start by putting all the files into a single range
	// with no corresponding chunk
	VPKInputFileRange_t rangeAllFiles;
	rangeAllFiles.m_iChunkFilenameIndex = -1;
	rangeAllFiles.m_iFirstInputFile = 0;
	rangeAllFiles.m_iLastInputFile = m_vecNewFilesInChunkOrder.Count()-1;
	rangeAllFiles.m_bKeepExistingFile = false;
	CalculateRangeTotalSizeInChunkFile( rangeAllFiles );
	m_llFileRanges.AddToTail( rangeAllFiles );
	SanityCheckRanges();

	// Building incrementally?
	if ( bIncremental && nOldChunkCount > 0 )
	{
		printf( "Scanning for unchanged chunk files...\n" );

		// For each existing chunk, see if it's totally modified or not.
		// In our case, since SteamPipe rewrites an entire file from scratch
		// anytime a single byte changes, we don't care how much a chunk
		// file changes, we only need to detect if we can carry it forward
		// exactly as is or not.
		int idxOldFile = 0;
		while ( idxOldFile < m_vecOldFilesInChunkOrder.Count() )
		{

			// What chunk are we in?
			VPKBuildFile_t const &firstFile = *m_vecOldFilesInChunkOrder[ idxOldFile ];
			int idxChunk = firstFile.m_pOld->m_idxChunk;

			char szDataFilename[ MAX_PATH ];
			m_packfile.GetDataFileName( szDataFilename, sizeof(szDataFilename), idxChunk );
			const char *pszShortDataFilename = V_GetFileName( szDataFilename );

			int idxInChunk = 0;
			CUtlVector<int> vecFilesToCompareContents;

			// Scan to the end of files in this chunk.
			CUtlString sReasonCannotReuse;
			while ( idxOldFile < m_vecOldFilesInChunkOrder.Count() )
			{
				VPKBuildFile_t const &f = *m_vecOldFilesInChunkOrder[ idxOldFile ];
				Assert( f.m_iOldSortIndex == idxOldFile );

				// End of this old chunk?
				VPKContentFileInfo_t const *pOld = f.m_pOld;
				Assert( pOld );
				if ( idxChunk != pOld->m_idxChunk )
					break;

				Assert( f.m_iOldSortIndex == firstFile.m_iOldSortIndex + idxInChunk );

				if ( sReasonCannotReuse.IsEmpty() )
				{
					VPKContentFileInfo_t const *pNew = f.m_pNew;
					int iExpectedSortIndex = firstFile.m_iNewSortIndex + idxInChunk;
					const char *pszFilename = pOld->m_sName.String();
					if ( pNew == NULL )
					{
						sReasonCannotReuse.Format( "File '%s' was removed.", pszFilename );
					}
					else if ( pOld->m_iTotalSize != pNew->m_iTotalSize )
					{
						sReasonCannotReuse.Format( "File '%s' changed size.", pszFilename );
					}
					else if ( pOld->m_iPreloadSize != pNew->m_iPreloadSize )
					{
						sReasonCannotReuse.Format( "File '%s' changed preload size.", pszFilename );
					}
					else if ( f.m_iNewSortIndex != iExpectedSortIndex )
					{
						// Files reordered in some way.  Try to give an appropriate message
						if ( f.m_iNewSortIndex > iExpectedSortIndex && iExpectedSortIndex < m_vecNewFilesInChunkOrder.Count() )
						{
							VPKContentFileInfo_t const *pInsertedFile = m_vecNewFilesInChunkOrder[ iExpectedSortIndex ];
							const char *pszInsertedFilename = pInsertedFile->m_sName.String();
							int idxDictInserted = m_dictFiles.Find( pszInsertedFilename );
							Assert( idxDictInserted != m_dictFiles.InvalidIndex() );
							if ( m_dictFiles[idxDictInserted].m_pOld == NULL )
								sReasonCannotReuse.Format( "File '%s' was inserted\n", pszInsertedFilename );
							else
								sReasonCannotReuse.Format( "Chunk reordered.  '%s' listed where '%s' used to be.", pszInsertedFilename, pszFilename );
						}
						else
						{
							sReasonCannotReuse.Format( "Chunk was reordered.  File '%s' was moved.", pszFilename );
						}
					}
					else if ( f.m_md5Old.IsZero() || f.m_md5New.IsZero() )
					{
						vecFilesToCompareContents.AddToTail( idxOldFile );
					}
					else if ( f.m_md5Old != f.m_md5New )
					{
						sReasonCannotReuse.Format( "File '%s' changed.  (Based on MD5s in control file.)", pszFilename );
					}
				}

				++idxOldFile;
				++idxInChunk;
			}

			// Check if we need to actually compare any file contents
			if ( sReasonCannotReuse.IsEmpty() && vecFilesToCompareContents.Count() > 0 )
			{

				// We'll have to actually load the source file
				// and compare the CRC
				printf( "%s: Checking for differences using file CRCs...\n", pszShortDataFilename );
				FOR_EACH_VEC( vecFilesToCompareContents, i )
				{
					VPKBuildFile_t const &f = *m_vecOldFilesInChunkOrder[ vecFilesToCompareContents[i] ];
					Assert( f.m_pOld );

					// Load the input file
					CUtlBuffer buf;
					if ( !g_pFullFileSystem->ReadFile( f.m_sNameOnDisk, NULL, buf )
						|| buf.TellPut() != (int)f.m_pOld->m_iTotalSize )
					{
						Error( "Error reading %s", f.m_sNameOnDisk.String() );
					}

					// Calculate the CRC
					uint32 crc = CRC32_ProcessSingleBuffer( buf.Base(), f.m_pOld->m_iTotalSize );

					// Mismatch?
					if ( crc != f.m_pOld->m_crc )
					{
						sReasonCannotReuse.Format( "File '%s' changed.  (CRCs differs from %s.)", f.m_pOld->m_sName.String(), f.m_sNameOnDisk.String() );
						break;
					}
				}
			}

			// Can we take this file as is?
			if ( sReasonCannotReuse.IsEmpty() )
			{
				printf( "%s could be reused.\n", pszShortDataFilename );

				// Map the chunk
				VPKInputFileRange_t chunkRange;
				chunkRange.m_iChunkFilenameIndex = idxChunk;
				chunkRange.m_iFirstInputFile = firstFile.m_iNewSortIndex;
				chunkRange.m_iLastInputFile = firstFile.m_iNewSortIndex + idxInChunk - 1;
				chunkRange.m_bKeepExistingFile = true;
				AddRange( chunkRange );
			}
			else
			{
				printf( "%s cannot be reused.  %s\n", pszShortDataFilename, sReasonCannotReuse.String() );
			}
		}
	}

	// Take file ranges that are not mapped to a chunk, and map them.
	MapAllRangesToChunks();

	int nNewChunkCount = m_llFileRanges.Count();
	printf( "Pack file will contain %d chunk files\n", nNewChunkCount );

	// Remove files from directory that have been deleted
	int iFilesRemoved = 0;
	FOR_EACH_DICT_FAST( m_dictFiles, i )
	{
		const VPKBuildFile_t &bf = m_dictFiles[i];
		if ( bf.m_pOld && !bf.m_pNew )
			m_packfile.RemoveFileFromDirectory( bf.m_pOld->m_sName.String() );
	}
	printf( "Removing %d files from the directory\n", iFilesRemoved );

	// Make sure ranges are cool
	SanityCheckRanges();

	// Grow chunk -> range table as necessary
	while ( m_vecRangeForChunk.Count() < nNewChunkCount )
		m_vecRangeForChunk.AddToTail( m_llFileRanges.InvalidIndex() );

	// OK, at this point, we're ready to assign any ranges that have
	// not yet been assigned a range an appropriate range index
	int idxChunk = 0;
	int iChunksToKeep = 0;
	int iFilesToKeep = 0;
	int64 iChunkSizeToKeep = 0;
	int iChunksToWrite = 0;
	int iFilesToWrite = 0;
	int64 iChunkSizeToWrite = 0;
	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];
		if ( r.m_iChunkFilenameIndex >= 0 )
		{
			Assert( r.m_bKeepExistingFile );
			iChunksToKeep += 1;
			iChunkSizeToKeep += r.m_nTotalSizeInChunkFile;
			iFilesToKeep += r.FileCount();
			continue;
		}

		// Range has not been assigned a chunk.
		// Locate the next chunk index
		// that has not been assigned to a range
		while ( m_vecRangeForChunk[idxChunk] != m_llFileRanges.InvalidIndex() )
		{
			++idxChunk;
			Assert( idxChunk < nNewChunkCount );
		}

		// Map the range
		MapRangeToChunk( idxRange, idxChunk, false );
		++idxChunk;
		Assert( idxChunk <= nNewChunkCount );

		iChunksToWrite += 1;
		iChunkSizeToWrite += r.m_nTotalSizeInChunkFile;
		iFilesToWrite += r.FileCount();
	}

	// Now scan chunks in order, and write and chunks that changed.
	bool bNeedToWriteDir = false;
	for ( int idxChunk = 0 ; idxChunk < nNewChunkCount ; ++idxChunk )
	{
		int idxRange = m_vecRangeForChunk[ idxChunk ];
		VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];

		char szDataFilename[ MAX_PATH ];
		m_packfile.GetDataFileName( szDataFilename, sizeof(szDataFilename), idxChunk );
		const char *pszShortDataFilename = V_GetFileName( szDataFilename );

		// Dump info about the chunk and what we're doing with it
		printf(
			"%s %s (%d files, %lld bytes)\n",
			r.m_bKeepExistingFile ? "Keeping" : "Writing",
			pszShortDataFilename,
			r.FileCount(),
			(long long)r.m_nTotalSizeInChunkFile
		);
		if ( s_bBeVerbose )
		{
			printf( "  First file: %s\n", m_vecNewFilesInChunkOrder[ r.m_iFirstInputFile ]->m_sName.String() );
			printf( "  Last file : %s\n", m_vecNewFilesInChunkOrder[ r.m_iLastInputFile ]->m_sName.String() );
		}

		// Retaining the existing file?
		if ( r.m_bKeepExistingFile )
		{
			// Mark the input files in this chunk as having been assigned to this chunk.
			for ( int idxFile = r.m_iFirstInputFile ; idxFile <= r.m_iLastInputFile ; ++idxFile )
			{
				VPKContentFileInfo_t *f = m_vecNewFilesInChunkOrder[ idxFile ];
				f->m_idxChunk = idxChunk;
			}
			continue;
		}

		// Create the output file.
		FileHandle_t fChunkWrite = g_pFullFileSystem->Open( szDataFilename, "wb" );
		if ( !fChunkWrite )
			Error( "Can't create %s\n", szDataFilename );

		// Scan input files in order.
		uint32 iOffsetInChunk = 0;
		for ( int idxFile = r.m_iFirstInputFile ; idxFile <= r.m_iLastInputFile ; ++idxFile )
		{
			VPKContentFileInfo_t *f = m_vecNewFilesInChunkOrder[ idxFile ];
			int idxInDict = m_dictFiles.Find( f->m_sName.String() );
			Assert( idxInDict >= 0 );
			VPKBuildFile_t *bf = &m_dictFiles[ idxInDict ];
			Assert( bf->m_pNew == f );

			// Load the input file
			CUtlBuffer buf;
			if ( !g_pFullFileSystem->ReadFile( bf->m_sNameOnDisk, NULL, buf )
				|| buf.TellPut() != (int)f->m_iTotalSize )
			{
				Error( "Error reading %s", bf->m_sNameOnDisk.String() );
			}
			Assert( iOffsetInChunk == g_pFullFileSystem->Tell( fChunkWrite ) );

			// Calculate the CRC
			f->m_crc = CRC32_ProcessSingleBuffer( buf.Base(), f->m_iTotalSize );

			// Finish filling in all of the header
			f->m_iOffsetInChunk = iOffsetInChunk;
			f->m_idxChunk = idxChunk;
			f->m_pPreloadData = buf.Base();

			// Update the directory
			m_packfile.AddFileToDirectory( *f );

			// Write the data
			int nBytesToWrite = f->GetSizeInChunkFile();
			int nBytesWritten = g_pFullFileSystem->Write( (byte*)buf.Base() + f->m_iPreloadSize, nBytesToWrite, fChunkWrite );
			if ( nBytesWritten != nBytesToWrite )
				Error( "Error writing %s", szDataFilename );
			iOffsetInChunk += nBytesToWrite;
			Assert( iOffsetInChunk == g_pFullFileSystem->Tell( fChunkWrite ) );

			// Align
			Assert( s_iChunkAlign > 0 );
			while ( iOffsetInChunk % s_iChunkAlign )
			{
				unsigned char zero = 0;
				g_pFullFileSystem->Write( &zero, 1, fChunkWrite );
				++iOffsetInChunk;
			}

			// Let's clear this pointer just for grins
			f->m_pPreloadData = NULL;
		}
		g_pFullFileSystem->Close( fChunkWrite );

		// While we know the data is sitting in the OS file cache,
		// let's immediately re-calc the chunk hashes
		m_packfile.HashChunkFile( idxChunk );

		// We'll need to re-save the directory
		bNeedToWriteDir = true;
	}

	// Delete any extra chunks that aren't needed anymore
	for ( int iChunkToDelete = nNewChunkCount ; iChunkToDelete < nOldChunkCount ; ++iChunkToDelete )
	{
		char szDataFilename[ MAX_PATH ];
		m_packfile.GetDataFileName( szDataFilename, sizeof(szDataFilename), iChunkToDelete );
		printf( "Deleting %s.\n", szDataFilename );
		g_pFullFileSystem->RemoveFile( szDataFilename );
		if ( g_pFullFileSystem->FileExists( szDataFilename ) )
			Error( "Failed to delete %s\n", szDataFilename );
		m_packfile.DiscardChunkHashes( iChunkToDelete );

		// We'll need to re-save the directory
		bNeedToWriteDir = true;
	}

	if ( s_bBeVerbose )
	{
		printf( "Chunk files:         %12s%12s\n", "Retained", "Written" );
		printf( "  Pack file chunks:  %12d%12d\n", iChunksToKeep, iChunksToWrite );
		printf( "  Data files:        %12d%12d\n", iFilesToKeep, iFilesToWrite );
		printf( "  Bytes in chunk:    %12lld%12lld\n", (long long)iChunkSizeToKeep, (long long)iChunkSizeToWrite );
	}

	// Finally, scan for any files that need to go in the directory,
	// but don't have any data in a chunk.  (Zero byte files, or all
	// data is in the preload area.)
	FOR_EACH_DICT( m_dictFiles, idxInDict )
	{
		VPKBuildFile_t *bf = &m_dictFiles[ idxInDict ];
		VPKContentFileInfo_t *pNew = bf->m_pNew;
		if ( pNew == NULL || pNew->m_idxChunk >= 0 )
			continue;
		Assert( pNew->GetSizeInChunkFile() == 0 );

		// Check if the file has changed and we need to update the directory

		VPKContentFileInfo_t *pOld = bf->m_pOld;
		int iNeedToUpdateFile = 1;
		if ( pOld )
		{
			if ( pOld->m_iTotalSize != pNew->m_iTotalSize
				|| pOld->m_iPreloadSize != pNew->m_iPreloadSize )
			{
				iNeedToUpdateFile = 1;
			}
			else if ( !bf->m_md5Old.IsZero() && !bf->m_md5New.IsZero() )
			{
				// We have hashes and can make the determination purely from the hashes
				if ( bf->m_md5Old == bf->m_md5New )
					iNeedToUpdateFile = 0;
				else
					iNeedToUpdateFile = 1;
			}
			else
			{
				// Not able to make a determination without loading the file
				iNeedToUpdateFile = -1;
			}
		}

		// Might we need to update the file?
		if ( iNeedToUpdateFile == 0 )
		{
			// We were able to determine that the files match, and
			// we know that there's no need to load the input file or
			// check CRC's
			continue;
		}

		// If we get here, we might need to update the header.
		// Load the file
		CUtlBuffer buf;
		if ( !g_pFullFileSystem->ReadFile( bf->m_sNameOnDisk, NULL, buf )
			|| buf.TellPut() != (int)pNew->m_iTotalSize )
		{
			Error( "Error reading %s", bf->m_sNameOnDisk.String() );
		}

		// Calculate the CRC
		pNew->m_crc = CRC32_ProcessSingleBuffer( buf.Base(), pNew->m_iTotalSize );

		// Compare CRC's
		if ( iNeedToUpdateFile < 0 )
		{
			Assert( pOld );
			if ( pOld->m_crc == pNew->m_crc )
				continue;
		}

		// We need to add the file to the header
		if ( pNew->m_iPreloadSize > 0 )
			pNew->m_pPreloadData = buf.Base();
		else
			Assert( pNew->m_iTotalSize == 0 );

		// Write the directory entry.  This will make a copy of any preload data
		m_packfile.AddFileToDirectory( *pNew );

		// Let's clear this pointer just for grins
		pNew->m_pPreloadData = NULL;

		// We'll need to re-save the directory
		bNeedToWriteDir = true;
	}

	// Nothing changed?
	if ( !bNeedToWriteDir )
	{
		if ( m_sReasonToForceWriteDirFile.IsEmpty() )
		{
			printf( "Nothing changed; not writing directory file.\n" );
			return;
		}
		printf( "VPK contents not changed, but directory needs to be resaved.  %s.\n", m_sReasonToForceWriteDirFile.String() );
	}

	if ( s_bBeVerbose )
		printf( "Hashing metadata.\n" );
	m_packfile.HashMetadata();

	if ( s_bBeVerbose )
		printf( "Writing directory file.\n" );
	m_packfile.Write();
}

void VPKBuilder::LoadInputKeys( const char *pszControlFilename )
{
	KeyValues *pInputKeys = new KeyValues( "packkeys" );
	if ( !pInputKeys->LoadFromFile( g_pFullFileSystem, pszControlFilename ) )
		Error( "Failed to load %s", pszControlFilename );

	SetInputKeys( pInputKeys, pszControlFilename );
}

void VPKBuilder::SetInputKeys( KeyValues *pInputKeys, const char *pszControlFilename )
{
	m_pInputKeys = pInputKeys;
	m_sControlFilename = pszControlFilename;
	m_iNewTotalFileSize = 0;
	m_iNewTotalFileSizeInChunkFiles = 0;
	int iSortIndex = 0;
    for ( KeyValues *i = m_pInputKeys; i; i = i->GetNextKey() )
	{
		const char *pszNameOnDisk = i->GetString( "srcpath", i->GetName() );
		char szNameInVPK[ MAX_PATH ];
		V_strcpy_safe( szNameInVPK, i->GetString( "destpath", "" ) );
		if ( szNameInVPK[0] == '\0' )
			Error( "File '%s' is missing 'destpath' in KeyValues control file", pszNameOnDisk );
		V_FixSlashes( szNameInVPK, '\\' ); // always use Windows slashes in VPK

		// Fail if passed an absolute path.
		if ( szNameInVPK[0] == '\\' )
			Error( "destpath '%s' is an absolute path; only relative paths should be used", szNameInVPK );

		// Check to make sure that no restricted file types are being added to the VPK.
		if ( IsRestrictedFileType( szNameInVPK ) )
		{
			printf( "WARNING: Control file lists '%s'.  We cannot put that type of file in the pack.\n", szNameInVPK );
			continue;
		}

		// Make sure we have a dictionary entry
		int idxInDict = m_dictFiles.Find( szNameInVPK );
		if ( idxInDict == m_dictFiles.InvalidIndex() )
			idxInDict = m_dictFiles.Insert( szNameInVPK );

		VPKBuildFile_t &bf = m_dictFiles[ idxInDict ];
		if ( !bf.m_sNameOnDisk.IsEmpty() || bf.m_pNew || bf.m_iNewSortIndex >= 0 )
			Error( "destpath '%s' in VPK appears multiple times in the KV control file.\n  (Source files '%s' and '%s')", szNameInVPK, bf.m_sNameOnDisk.String(), pszNameOnDisk );
		bf.m_sNameOnDisk = pszNameOnDisk;

		VPKContentFileInfo_t *f = new VPKContentFileInfo_t;
		f->m_sName = szNameInVPK;
		f->m_iTotalSize = g_pFullFileSystem->Size( pszNameOnDisk );
		f->m_iPreloadSize = Min( (uint32)i->GetInt( "preloadsize", 0 ), f->m_iTotalSize );
		const char *pszMD5 = i->GetString( "md5", "" );
		if ( *pszMD5 )
		{
			if ( V_strlen( pszMD5 ) != MD5_DIGEST_LENGTH*2 )
				Error( "File '%s' has invalid MD5 '%s'", pszNameOnDisk, pszMD5 );
			V_hextobinary( pszMD5, MD5_DIGEST_LENGTH*2, bf.m_md5New.bits, MD5_DIGEST_LENGTH );
		}

		m_vecNewFiles.AddToTail( f );
		bf.m_pNew = f;
		if ( f->GetSizeInChunkFile() > 0 )
		{
			bf.m_iNewSortIndex = iSortIndex++;
			m_vecNewFilesInChunkOrder.AddToTail( f );
		}

		m_iNewTotalFileSize += f->m_iTotalSize;
		m_iNewTotalFileSizeInChunkFiles += f->GetSizeInChunkFile();
	}
	printf( "Control file lists %d files\n", m_vecNewFiles.Count() );
	printf( "  Total file size . . . . : %12lld bytes\n", (long long)m_iNewTotalFileSize );
	printf( "  Size in preload area  . : %12lld bytes\n", (long long)(m_iNewTotalFileSize - m_iNewTotalFileSizeInChunkFiles ) );
	printf( "  Size in data area . . . : %12lld bytes\n", (long long)m_iNewTotalFileSizeInChunkFiles );
}

void VPKBuilder::MapAllRangesToChunks()
{

	//PrintRangeDebug();

	// If a range is NOT at least as big as one chunk file, then we will have to merge it
	// with an adjacent range --- that is, we will need to unmap an adjacent range.
	// So the first step will be to identify which of the currently mapped ranges
	// to unmap in order to get rid of any ranges that cannot get mapped to a chunk.
	// We might have a choice in the matter, and each range that we unmap means
	// another file that will have to be rewritten.  So the goal here is to minimize
	// the number/size of chunks that we unmap and force to rewrite.
	//
	// The current state of affairs should be that all mapped regions correspond to chunk
	// files that do not need to be rewritten, and there are no two unmapped chunk files in a row.

	int64 iSizeTooSmallForAChunk = (int64)m_packfile.GetWriteChunkSize() * 95 / 100;
	for (;;)
	{
		for (;;)
		{

			// Make sure the problem of small chunks can be solved by unmapping
			// a mapped chunk
			UnmapAllRangesForChangedChunks();
			CoaleseAllUnmappedRanges();

			// Find a mapped region next to a region that's too
			// small to get its own chunk.  If there are multiple,
			// we'll choose the "best" one to coalesce according to
			// a greedy algorithm.
			int idxBestRangeToUnmap = m_llFileRanges.InvalidIndex();
			int iBestScore = -1;
			int64 iBestSize = -1;
			FOR_EACH_LL( m_llFileRanges, idxRange )
			{
				VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];
				if ( r.m_iChunkFilenameIndex < 0 )
					continue;

				// Check if neighbors exist and are too small
				// for their own chunk.  Calculate score heuristic
				// based on how good of a candidate we are to
				// be the one to get combined with our neighbors
				int iScore = 0;
				int idxPrev = m_llFileRanges.Previous( idxRange );
				if ( idxPrev != m_llFileRanges.InvalidIndex() )
				{
					VPKInputFileRange_t &p = m_llFileRanges[ idxPrev ];
					if ( p.m_iChunkFilenameIndex < 0 && p.m_nTotalSizeInChunkFile < iSizeTooSmallForAChunk )
					{
						++iScore;
						if ( idxPrev == m_llFileRanges.Head() )
							iScore += 3; // Nobody else could fix this, so we need to do it
					}
				}
				int idxNext = m_llFileRanges.Next( idxRange );
				if ( idxNext != m_llFileRanges.InvalidIndex() )
				{
					VPKInputFileRange_t &n = m_llFileRanges[ idxNext ];
					if ( n.m_iChunkFilenameIndex < 0 && n.m_nTotalSizeInChunkFile < iSizeTooSmallForAChunk )
					{
						++iScore;
						if ( idxNext == m_llFileRanges.Tail() )
							iScore += 3; // Nobody else could fix this, so we need to do it
					}
				}

				// Do we have any reason at all to absorb our neighbors?
				if ( iScore == 0 )
					continue;

				// Check if we're the best one so far to absorb our neighbor
				if ( iScore < iBestScore )
					continue;

				// When choosing which of two neighbors should absorb a new gap, add it to the smaller one.
				// (That will be less to rewrite and also keep the chunk size at a more desirable level.)
				if ( iScore == iBestScore && r.m_nTotalSizeInChunkFile > iBestSize )
					continue;

				// We're the new best
				iBestScore = iScore;
				idxBestRangeToUnmap = idxRange;
				iBestSize = r.m_nTotalSizeInChunkFile;
			}

			// Did we find a range that needed to absorb its neighbor?
			if ( idxBestRangeToUnmap == m_llFileRanges.InvalidIndex() )
				break;

			// Unmap it
			MapRangeToChunk( idxBestRangeToUnmap, -1, false );

			// We'll coalesce the unmapped region with its neighbor(s) and
			// start the whole process over
		}

		// OK, at this point, if there were any ranges that were too small to hold their
		// own chunks, then we should have merged them.  (Unless there is exactly one range.)
		// The next step is to split up ranges that are too large for a single chunk.
		SanityCheckRanges();
		FOR_EACH_LL( m_llFileRanges, idxRange )
		{
			VPKInputFileRange_t *r = &m_llFileRanges[ idxRange ];

			// Check how many chunks this
			int iChunks = r->m_nTotalSizeInChunkFile / m_packfile.GetWriteChunkSize();
			if ( iChunks <= 1 )
				continue;

			// If they consistently build with the same chunk size, then
			// we should only hit this for ranges that are going to be rewritten.
			// However, if this chunk is already fine as it, let's leave it alone.
			// There's no reason to split it.
			if ( r->m_iChunkFilenameIndex >= 0 )
			{
				Assert( r->m_bKeepExistingFile );
				printf( "Chunk %d is currently bigger than desired chunk size of %d bytes, but we're not splitting it because the contents have not changed.\n", r->m_iChunkFilenameIndex, m_packfile.GetWriteChunkSize() );
				continue;
			}

			// Try to split off approximately 1 N/th of the data into this chunk.
			// Note that if we have big files inside, we might not have enough granularity to
			// do exactly what they desire and could get caught in a bad state
			int64 iDesiredSize = r->m_nTotalSizeInChunkFile / iChunks;
			Assert( iDesiredSize >= m_packfile.GetWriteChunkSize() );
			int iNewLastInputFile = r->m_iFirstInputFile;
			int64 iNewSize = m_vecNewFilesInChunkOrder[ iNewLastInputFile ]->GetSizeInChunkFile();
			while ( iNewSize < iDesiredSize && iNewLastInputFile < r->m_iLastInputFile )
			{
				++iNewLastInputFile;
				iNewSize += m_vecNewFilesInChunkOrder[ iNewLastInputFile ]->GetSizeInChunkFile();
			}

			// Do the split
			int iSaveFirstInputFile = r->m_iFirstInputFile;
			SplitRangeAt( iNewLastInputFile+1 );
			r = &m_llFileRanges[ idxRange ]; // ranges may have moved in memory!

			// Here we make an assumption that SplitRangeAt will keep range idxRange
			// modified and link the new range AFTER this range.  Verify that assumption.
			Assert( r->m_iFirstInputFile == iSaveFirstInputFile );
			Assert( r->m_iLastInputFile == iNewLastInputFile );
			Assert( r->m_nTotalSizeInChunkFile == iNewSize );

			// We've got this range approximately to the desired size.
			// The next range should be approximately (N-1)/N as big as the original
			// size of this range, and if N>2, then it wil need to be split, too
		}

		// OK, all ranges should now be the appropriate size, and should
		// map to exactly one chunk.  We just haven't assigned the chunk
		// numbers yet.  The important thing to realize is that the numbers
		// are essentially arbitrary, and if we're going to rewrite a file,
		// it doesn't matter if data moves from one chunk to another with
		// a totally different number.  However....leaving a gap is probably
		// a bad idea.  We don't know what assumptions existing tools make,
		// and this could be confusing and look like a missing file.  So
		// if we have N chunks, we will always number them 0...N-1.
		int nNewChunkCount = m_llFileRanges.Count();

		// Check if the number of chunks has been reduced, and a chunk file
		// that we previously thought we would be able to retain has
		// a file index that won't exist any more, then let's unmap those ranges
		// and start over.
		bool bNeedToStartOver = false;
		for ( int i = m_vecRangeForChunk.Count()-1 ; i >= nNewChunkCount ; --i )
		{
			int idxRange = m_vecRangeForChunk[i];
			if ( idxRange == m_llFileRanges.InvalidIndex() )
				continue;
			Assert( m_llFileRanges[ idxRange ].m_iChunkFilenameIndex == i );
			Assert( m_llFileRanges[ idxRange ].m_bKeepExistingFile );
			MapRangeToChunk( idxRange, -1, false );
			bNeedToStartOver = true;
		}
		if ( !bNeedToStartOver )
			break;

		// We unmapped a chunk because the chunk file is going to
		// get deleted.  Start all over!
	}
}

void VPKBuilder::UnmapAllRangesForChangedChunks()
{
	SanityCheckRanges();

	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];

		// If range was assigned a chunk, but the chunk file will have to be rewitten,
		// then unmap it
		if ( r.m_iChunkFilenameIndex >= 0 && !r.m_bKeepExistingFile )
			MapRangeToChunk( idxRange, -1, false );
	}

	SanityCheckRanges();
}

void VPKBuilder::CoaleseAllUnmappedRanges()
{
	SanityCheckRanges();

	int idxRange = m_llFileRanges.Head();
	for (;;)
	{
		int idxNext = m_llFileRanges.Next( idxRange );
		if ( idxNext == m_llFileRanges.InvalidIndex() )
			break;

		// Grab shortcuts
		VPKInputFileRange_t &ri = m_llFileRanges[ idxRange ];
		VPKInputFileRange_t &rn = m_llFileRanges[ idxNext ];

		// Both chunks unassigned?
		if ( ri.m_iChunkFilenameIndex < 0 && rn.m_iChunkFilenameIndex < 0 )
		{
			// Merge current with next
			ri.m_iLastInputFile = rn.m_iLastInputFile;
			CalculateRangeTotalSizeInChunkFile( ri );
			m_llFileRanges.Remove( idxNext );

			// List should be valid at this point
			SanityCheckRanges();
		}
		else
		{
			// Keep it, advance to the next one
			idxRange = idxNext;
		}
	}
}

void VPKBuilder::CalculateRangeTotalSizeInChunkFile( VPKInputFileRange_t &range ) const
{
	range.m_nTotalSizeInChunkFile = 0;
	for ( int i = range.m_iFirstInputFile ; i <= range.m_iLastInputFile ; ++i )
	{
		range.m_nTotalSizeInChunkFile += m_vecNewFilesInChunkOrder[ i ]->GetSizeInChunkFile();
	}
}

void VPKBuilder::SanityCheckRanges()
{
	int iFileIndex = 0;
	int64 iTotalSizeInChunks = 0;
	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];
		Assert( r.m_iFirstInputFile == iFileIndex );
		Assert( r.m_iLastInputFile >= r.m_iFirstInputFile );
		iFileIndex = r.m_iLastInputFile + 1;
		iTotalSizeInChunks += r.m_nTotalSizeInChunkFile;
	}
	Assert( iFileIndex == m_vecNewFilesInChunkOrder.Count() );
	Assert( iTotalSizeInChunks == m_iNewTotalFileSizeInChunkFiles );
}

void VPKBuilder::PrintRangeDebug()
{
	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t &r = m_llFileRanges[ idxRange ];
		printf( "Range handle %d:\n", idxRange );
		printf( "  File range %d .. %d\n", r.m_iFirstInputFile, r.m_iLastInputFile );
		printf( "  Chunk %d%s\n", r.m_iChunkFilenameIndex, r.m_bKeepExistingFile ? "  (keep existing file)" : "" );
		printf( "  Size %lld\n", (long long)r.m_nTotalSizeInChunkFile );
	}
}

void VPKBuilder::AddRange( VPKInputFileRange_t range )
{
	// Sanity check that ranges are in a valid order
	SanityCheckRanges();

	// Split up the range(s) we overlap so that we will match exactly one range
	SplitRangeAt( range.m_iFirstInputFile );
	SplitRangeAt( range.m_iLastInputFile+1 );

	// Locate the range
	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t *p = &m_llFileRanges[ idxRange ];
		if ( p->m_iLastInputFile < range.m_iFirstInputFile )
			continue;

		// Range should now match exactly
		Assert( p->m_iFirstInputFile == range.m_iFirstInputFile );
		Assert( p->m_iLastInputFile == range.m_iLastInputFile );

		// Assign it to the proper chunk
		MapRangeToChunk( idxRange, range.m_iChunkFilenameIndex, range.m_bKeepExistingFile );
		return;
	}

	// We should have found it
	Assert( false );
}

void VPKBuilder::SplitRangeAt( int iFirstInputFile )
{
	// Sanity check that ranges are in a valid order
	SanityCheckRanges();

	// Now Locate any ranges that we overlap, and split them as appropriate
	FOR_EACH_LL( m_llFileRanges, idxRange )
	{
		VPKInputFileRange_t *p = &m_llFileRanges[ idxRange ];

		// No need to make any changes if split already exists at requested location
		if ( p->m_iFirstInputFile == iFirstInputFile || p->m_iLastInputFile+1 == iFirstInputFile)
			return;

		// Found the range to split?
		Assert( p->m_iFirstInputFile < iFirstInputFile );
		if ( p->m_iLastInputFile >= iFirstInputFile )
		{
			// We should only be spliting up unallocated space
			Assert( p->m_iChunkFilenameIndex < 0 );

			VPKInputFileRange_t newRange = *p;
			p->m_iLastInputFile = iFirstInputFile-1;
			newRange.m_iFirstInputFile = iFirstInputFile;
			CalculateRangeTotalSizeInChunkFile( newRange );
			CalculateRangeTotalSizeInChunkFile( *p );
			m_llFileRanges.InsertAfter( idxRange, newRange );

			// Make sure we didn't screw anything up
			SanityCheckRanges();
			return;
		}

	}

	// We should have found something
	Assert( false );
}

void VPKBuilder::MapRangeToChunk( int idxRange, int iChunkFilenameIndex, bool bKeepExistingFile )
{
	VPKInputFileRange_t *p = &m_llFileRanges[ idxRange ];

	// If range was already mapped to a chunk, unmap it.
	if ( p->m_iChunkFilenameIndex >= 0 )
	{
		Assert( m_vecRangeForChunk[ p->m_iChunkFilenameIndex ] == idxRange );
		m_vecRangeForChunk[ p->m_iChunkFilenameIndex ] = m_llFileRanges.InvalidIndex();
		p->m_iChunkFilenameIndex = -1;
		p->m_bKeepExistingFile = false;
	}

	// Map range to a chunk?
	if ( iChunkFilenameIndex >= 0 )
	{
		Assert( m_vecRangeForChunk[ iChunkFilenameIndex ] == m_llFileRanges.InvalidIndex() );
		p->m_iChunkFilenameIndex = iChunkFilenameIndex;
		p->m_bKeepExistingFile = bKeepExistingFile;
		m_vecRangeForChunk[ iChunkFilenameIndex ] = idxRange;
	}
	else
	{
		Assert( !bKeepExistingFile );
	}
}

#ifdef VPK_ENABLE_SIGNING
void GenerateKeyPair( const char *pszBaseKeyName )
{
	printf( "Generating RSA public/private keypair...\n" );

	//
	// This code pretty much copied from vsign.cpp
	//

	uint8 rgubPublicKey[k_nRSAKeyLenMax]={0};
	uint cubPublicKey = Q_ARRAYSIZE( rgubPublicKey );

	uint8 rgubPrivateKey[k_nRSAKeyLenMax]={0};
	uint cubPrivateKey = Q_ARRAYSIZE( rgubPrivateKey );

	if( !CCrypto::RSAGenerateKeys( rgubPublicKey, &cubPublicKey, rgubPrivateKey, &cubPrivateKey ) )
	{
		Error( "Failed to generate RSA keypair.\n" );
	}

	char rgchEncodedPublicKey[k_nRSAKeyLenMax*4];
	uint cubEncodedPublicKey = Q_ARRAYSIZE( rgchEncodedPublicKey );

	if( !CCrypto::HexEncode( rgubPublicKey, cubPublicKey, rgchEncodedPublicKey, cubEncodedPublicKey ) )
	{
		Error( "Failed to encode public key.\n" );
	}

// Don't encrypt
//	uint8 rgubEncryptedPrivateKey[Q_ARRAYSIZE( rgubPrivateKey )*2];
//	uint cubEncryptedPrivateKey = Q_ARRAYSIZE( rgubEncryptedPrivateKey );
//
//	if( !CCrypto::SymmetricEncrypt( rgubPrivateKey, cubPrivateKey, rgubEncryptedPrivateKey, &cubEncryptedPrivateKey, (uint8 *)rgchPassphrase, k_nSymmetricKeyLen ) )
//	{
//		printf( "ERROR! Failed to encrypt private key.\n" );
//		return false;
//	}

	char rgchEncodedEncryptedPrivateKey[Q_ARRAYSIZE( rgubPrivateKey )*8];
	if( !CCrypto::HexEncode( rgubPrivateKey, cubPrivateKey, rgchEncodedEncryptedPrivateKey, Q_ARRAYSIZE(rgchEncodedEncryptedPrivateKey) ) )
	{
		Error( "Failed to encode private key.\n" );
	}

	// Good Lord.  Use fopen, because it will work without any surprising crap or hidden limitations.
	// I just wasted an hour trying to get CUtlBuffer and our filesystem to print a block of text to a file.

	// Save public keyfile
	{
		CUtlString sPubFilename( pszBaseKeyName );
		sPubFilename += ".publickey.vdf";
		FILE *f = fopen( sPubFilename, "wt" );
		if ( f == NULL )
			Error( "Cannot create %s.", sPubFilename.String() );

		// Write public keyfile
		fprintf( f,
			"// Public key file.  You can publish this key file and share it with the world.\n"
			"// It can be used by third parties to verify any signatures made with the corresponding private key.\n"
			"public_key\n"
			"{\n"
			"\ttype \"rsa\"\n"
			"\trsa_public_key \"%s\"\n"
			"}\n",
			rgchEncodedPublicKey );
		fclose(f);
		printf( "  Saved %s\n", sPubFilename.String() );
	}

	// Save private keyfile
	{
		CUtlString sPrivFilename( pszBaseKeyName );
		sPrivFilename += ".privatekey.vdf";
		FILE *f = fopen( sPrivFilename, "wt" );
		if ( f == NULL )
			Error( "Cannot create %s.", sPrivFilename.String() );
		fprintf( f,
			"// Private key file.\n"
			"// This key can be used to sign files.  Third parties can verify your signature by using your public key.\n"
			"//\n"
			"// THIS KEY SHOULD BE KEPT SECRET\n"
			"//\n"
			"// You should share your public key freely, but anyone who has your private key will be able to impersonate you.\n"
			"private_key\n"
			"{\n"
			"\ttype \"rsa\"\n"
			"\trsa_private_key \"%s\"\n"
			"\n"
			"\t// Note: the private key is stored in plaintext.  It is not encrypted or protected by a password.\n"
			"\t//       Anyone who obtains this key can use it to sign files.\n"
			"\tprivate_key_encrypted 0\n"
			"\n"
			"\t// The public key that corresponds to this private key.  The public keyfile you can share with others is\n"
			"\t// saved in another file, but the key data is duplicated here to help you confirm which public key matches\n"
			"\t// with this private key.\n"
			"\tpublic_key\n"
			"\t{\n"
			"\t\ttype \"rsa\"\n"
			"\t\trsa_public_key \"%s\"\n"
			"\t}\n"
			"}\n",
			rgchEncodedEncryptedPrivateKey, rgchEncodedPublicKey );
		fclose(f);
		printf( "  Saved %s\n", sPrivFilename.String() );
	}

	printf( "\n" );
	printf( "REMEMBER: Your private key should be kept secret.  Don't share it!\n" );
}

static void CheckSignature( const char *pszFilename )
{
	char szActualFileName[MAX_PATH];
	CPackedStore pack( pszFilename, szActualFileName, g_pFullFileSystem );

	// Make sure they didn't make a mistake
	CUtlVector<uint8> bytesPublicKey;
	if ( s_sPublicKeyFile.IsEmpty() )
	{
		if ( !s_sPrivateKeyFile.IsEmpty() )
			Error( "Private keys are not used to verify signatures.  Did you mean to use -k instead?" );

		printf(
			"Checking signature using public key in VPK.\n"
			"\n"
			"NOTE: This just confirms that the VPK has a valid signature,\n"
			"      not that signature was made by any particular party.  Use -k\n"
			"      and provide a public key in order to verify that a file was\n"
			"      signed by a particular trusted party.\n" );
	}
	else
	{
		LoadKeyFile( s_sPublicKeyFile, "rsa_public_key", bytesPublicKey );
		printf( "Loaded public key file %s\n", s_sPublicKeyFile.String() );
	}

	printf( "\n" );
	fflush( stdout );
	CPackedStore::ESignatureCheckResult result = pack.CheckSignature( bytesPublicKey.Count(), bytesPublicKey.Base() );
	switch (result )
	{
		default:
		case CPackedStore::eSignatureCheckResult_Failed:
			fprintf( stderr, "ERROR: FAILED\n" );
			fflush( stderr );
			printf( "IO error or other generic failure." );
			exit(-1);

		case CPackedStore::eSignatureCheckResult_NotSigned:
			fprintf( stderr, "ERROR: NOT SIGNED\n" );
			fflush( stderr );
			printf( "The VPK does not contain a signature." );
			exit(1);

		case CPackedStore::eSignatureCheckResult_WrongKey:
			fprintf( stderr, "ERROR: KEY MISMATCH\n" );
			fflush( stderr );
			printf(
				"The public key provided does not match the public\n"
				"key contained in the VPK file.  The VPK was not\n"
				"signed using the private key corresponding to your\n"
				"public key.\n" );
			exit(2);

		case CPackedStore::eSignatureCheckResult_InvalidSignature:
			fprintf( stderr, "ERROR: INVALID SIGNATURE\n" );
			fflush( stderr );
			printf( "The VPK contains a signature, but it isn't valid." );
			exit(3);

		case CPackedStore::eSignatureCheckResult_ValidSignature:
			printf( "SUCCESS\n" );
			if ( s_sPublicKeyFile.IsEmpty() )
			{
				printf( "VPK contains a valid signature." );
			}
			else
			{
				printf( "VPK signature validated using the specified public key." );
			}
			exit(0);
	}
}

static void CheckHashes( const char *pszFilename )
{
	char szActualFileName[MAX_PATH];
	CPackedStore pack( pszFilename, szActualFileName, g_pFullFileSystem );

	char szChunkFilename[ 256 ];

	printf( "Checking cache line hashes:\n" );
	CUtlSortVector<ChunkHashFraction_t, ChunkHashFractionLess_t > &vecHashes = pack.AccessPackFileHashes();
	CPackedStoreFileHandle handle = pack.GetHandleForHashingFiles();
	handle.m_nFileNumber = -1;
	int nCheckedFractionsOK = 0;
	int nTotalCheckedCacheLines = 0;
	int nTotalErrorCacheLines = 0;
	FOR_EACH_VEC( vecHashes, idx )
	{
		ChunkHashFraction_t frac = vecHashes[idx];
		if ( idx == 0 || frac.m_nPackFileNumber != handle.m_nFileNumber )
		{
			if ( nCheckedFractionsOK > 0 )
				printf( "OK.  (%d caches lines)\n", nCheckedFractionsOK );
			handle.m_nFileNumber = frac.m_nPackFileNumber;
			pack.GetPackFileName( handle, szChunkFilename, sizeof(szChunkFilename) );
			printf("  %s: ", szChunkFilename );
			fflush( stdout );
			nCheckedFractionsOK = 0;
		}

		FileHash_t filehash;

		// VPKHandle.m_nFileNumber;
		// nFileFraction;
		int64 fileSize = 0;
		// if we have never hashed this before - do it now
		pack.HashEntirePackFile( handle, fileSize, frac.m_nFileFraction, frac.m_cbChunkLen, filehash );

		++nTotalCheckedCacheLines;

		if ( filehash.m_cbFileLen != frac.m_cbChunkLen )
		{
			if ( nCheckedFractionsOK >= 0 )
			{
				printf( "\n" );
				fflush( stdout );
				nCheckedFractionsOK = -1;
			}
			fprintf( stderr, "    @%d: size mismatch.  Stored: %d  Computed: %d\n", frac.m_nFileFraction, frac.m_cbChunkLen, filehash.m_cbFileLen );
			fflush( stderr );
			++nTotalErrorCacheLines;
		}
		else if ( filehash.m_md5contents != frac.m_md5contents )
		{
			if ( nCheckedFractionsOK >= 0 )
			{
				printf( "\n" );
				fflush( stdout );
				nCheckedFractionsOK = -1;
			}

			char szCalculated[ MD5_DIGEST_LENGTH*2 + 4 ];
			char szExpected[ MD5_DIGEST_LENGTH*2 + 4 ];
			V_binarytohex( filehash.m_md5contents.bits, MD5_DIGEST_LENGTH, szCalculated, sizeof(szCalculated) );
			V_binarytohex( frac.m_md5contents.bits, MD5_DIGEST_LENGTH, szExpected, sizeof(szExpected) );

			fprintf( stderr, "    @%d: hash mismatch: Got %s, expected %s.\n", frac.m_nFileFraction, szCalculated, szExpected );
			fflush( stderr );
			++nTotalErrorCacheLines;
		}
		else
		{
			if ( nCheckedFractionsOK >= 0 )
				++nCheckedFractionsOK;
		}
	}

	if ( nCheckedFractionsOK > 0 )
		printf( "OK.  (%d caches lines)\n", nCheckedFractionsOK );

	if ( nTotalErrorCacheLines == 0 )
	{
		printf( "All %d cache lines hashes matched OK\n", nTotalCheckedCacheLines );
		exit(0);
	}

	fprintf( stderr, "%d cache lines failed validation out of %d checked \n", nTotalErrorCacheLines, nTotalCheckedCacheLines );
	exit(1);
}

static void PrintBinaryBlob( const CUtlVector<uint8> &blob )
{
	const int kRowLen = 32;
	for ( int i = 0 ; i < blob.Count() ; i += kRowLen )
	{
		int iEnd = Min( i+kRowLen, blob.Count() );
		const char *pszSep = "  ";
		for ( int j = i ; j < iEnd ; ++j )
		{
			printf( "%s%02X", pszSep, blob[j] );
			pszSep = "";
		}
		printf( "\n" );
	}
}

static void DumpSignatureInfo( const char *pszFilename )
{
	char szActualFileName[MAX_PATH];
	CPackedStore pack( pszFilename, szActualFileName, g_pFullFileSystem );
	if ( pack.GetSignature().Count() == 0 )
	{
		printf( "VPK is not signed\n" );
		return;
	}
	printf( "Public key:\n" );
	PrintBinaryBlob( pack.GetSignaturePublicKey() );

	printf( "Signature:\n" );
	PrintBinaryBlob( pack.GetSignature() );
}

#endif

void BuildRecursiveFileList( const char *pcDirName, CUtlStringList &fileList )
{
	char szDirWildcard[MAX_PATH];
	FileFindHandle_t findHandle;

	V_snprintf( szDirWildcard, sizeof( szDirWildcard ), "%s%c%s", pcDirName, CORRECT_PATH_SEPARATOR, "*.*" );

	char const *pcResult = g_pFullFileSystem->FindFirst( szDirWildcard, &findHandle );

	if ( pcResult )
	{
		do
		{
			char szFullResultPath[MAX_PATH];

			if ( '.' == pcResult[0] )
			{
				pcResult = g_pFullFileSystem->FindNext( findHandle );
				continue;
			}
			
			// Make a full path to the result
			V_snprintf( szFullResultPath, sizeof( szFullResultPath ), "%s%c%s", pcDirName, CORRECT_PATH_SEPARATOR, pcResult );

			if ( g_pFullFileSystem->IsDirectory( szFullResultPath ) )
			{
				// Recurse
				BuildRecursiveFileList( szFullResultPath, fileList );
			}
			else
			{
				// Add file to the file list
				fileList.CopyAndAddToTail( szFullResultPath );
			}

			pcResult = g_pFullFileSystem->FindNext( findHandle );

		} while ( pcResult );

		g_pFullFileSystem->FindClose( findHandle );
	}
}

static void DroppedVpk( const char *pszVpkFilename )
{
	char szActualFileName[MAX_PATH];
	CPackedStore mypack( pszVpkFilename, szActualFileName, g_pFullFileSystem );
	CUtlStringList fileNames;
	char szVPKParentDir[MAX_PATH];

	V_strncpy( szVPKParentDir, pszVpkFilename, sizeof( szVPKParentDir ) );
	V_SetExtension( szVPKParentDir, "", sizeof( szVPKParentDir ) );
	mypack.GetFileList( fileNames, false, true );

	for( int i = 0 ; i < fileNames.Count(); i++ )
	{
		char szDestFilePath[MAX_PATH];
		CPackedStoreFileHandle pData = mypack.OpenFile( fileNames[i] );

		V_snprintf( szDestFilePath, sizeof( szDestFilePath ), "%s%c%s", szVPKParentDir, CORRECT_PATH_SEPARATOR, fileNames[i] );

		if ( pData )
		{
			char szParentDirectory[MAX_PATH];

			V_ExtractFilePath( szDestFilePath, szParentDirectory, sizeof( szParentDirectory ) );
			V_FixSlashes( szParentDirectory );
				
			if ( !g_pFullFileSystem->IsDirectory( szParentDirectory ) )
			{
				g_pFullFileSystem->CreateDirHierarchy( szParentDirectory );
			}
				
			printf( "extracting %s\n", fileNames[i] );
			COutputFile outF( szDestFilePath );

			if ( outF.IsOk() )
			{
				int nBytes = pData.m_nFileSize;
				while( nBytes )
				{
					char cpBuf[65535];
					int nReadSize = MIN( sizeof( cpBuf ), nBytes );
					mypack.ReadData( pData, cpBuf, nReadSize );
					outF.Write( cpBuf, nReadSize );
					nBytes -= nReadSize;
				}
				outF.Close();
			}
		}
	}
}

static void DroppedDirectory( const char *pszDirectoryArg )
{
	// Strip trailing slash, if any
	char szDirectory[MAX_PATH];
	V_strcpy_safe( szDirectory, pszDirectoryArg );
	V_StripTrailingSlash( szDirectory );

	char szVPKPath[MAX_PATH];

	// Construct path to VPK
	V_snprintf( szVPKPath, sizeof( szVPKPath ), "%s.vpk", szDirectory ); 

	// Delete any existing one at that location
	if ( g_pFullFileSystem->FileExists( szVPKPath ) )
	{
		if ( g_pFullFileSystem->IsFileWritable( szVPKPath ) )
		{
			g_pFullFileSystem->RemoveFile( szVPKPath );
		}
		else
		{
			fprintf( stderr, "Cannot delete file: %s\n", szVPKPath );
			exit(1);
		}
	}

	// Make the VPK
	char szActualFileName[MAX_PATH];
	CPackedStore mypack( szVPKPath, szActualFileName, g_pFullFileSystem, true );
	mypack.SetWriteChunkSize( s_iMultichunkSize * 1024*1024 );

	// !KLUDGE! Create keyvalues object, since that's what the builder uses
	printf( "Finding files and creating temporary control file...\n" );
	CUtlStringList fileList;
	BuildRecursiveFileList( szDirectory, fileList );
	KeyValues *pInputKeys = new KeyValues("packkeys");
	const int nBaseDirLength = V_strlen( szDirectory );
	for( int i = 0 ; i < fileList.Count(); i++ )
	{
		// .... Ug O(n^2)
		KeyValues *pFileKey = pInputKeys->CreateNewKey();
		const char *pszFilename = fileList[i];
		pFileKey->SetString( "srcpath", pszFilename );
		const char *pszDestPath = pszFilename + nBaseDirLength;
		if ( *pszDestPath == '/' || *pszDestPath == '\\' )
			++pszDestPath;
		pFileKey->SetString( "destpath", pszDestPath );
	}

	VPKBuilder builder( mypack );
	builder.SetInputKeys( pInputKeys->GetFirstSubKey(), "" );
	builder.BuildFromInputKeys();
}

int main(int argc, char **argv)
{
	InitCommandLineProgram( argc, argv );
	int nCurArg = 1;

	//
	// Check for standard usage syntax
	//
	while ( ( nCurArg < argc ) && ( argv[nCurArg][0] == '-' ) )
	{
		switch( argv[nCurArg][1] )
		{
			case '?':										// args
			{
				PrintArgSummaryAndExit( 0 ); // return success in this case.
			}
			break;

			case 'M':
			{
				s_bMakeMultiChunk = true;
			}
			break;

			case 'P':
			{
				s_bUseSteamPipeFriendlyBuilder = true;
				s_bMakeMultiChunk = true;
			}
			break;

			case 'v':										// verbose
			{
				s_bBeVerbose = true;
			}
			break;

			case 'a':
			{
				nCurArg++;
				if ( nCurArg >= argc )
				{
					fprintf( stderr, "Expected argument after %s\n", argv[nCurArg-1] );
					exit( 1 );
				}
				s_iChunkAlign = V_atoi( argv[nCurArg] );
				if ( s_iChunkAlign <= 0 || s_iChunkAlign > 32*1024 )
				{
					fprintf( stderr, "Invalid alignment value %s\n", argv[nCurArg] );
					exit( 1 );
				}
			}
			break;

			case 'c':
			{
				nCurArg++;
				if ( nCurArg >= argc )
				{
					fprintf( stderr, "Expected argument after %s\n", argv[nCurArg-1] );
					exit( 1 );
				}
				s_iMultichunkSize = V_atoi( argv[nCurArg] );
				if ( s_iMultichunkSize <= 0 || s_iMultichunkSize > 1*1024 )
				{
					fprintf( stderr, "Invalid chunk size %s\n", argv[nCurArg] );
					exit( 1 );
				}
			}
			break;

			case 'K':
				nCurArg++;
				if ( nCurArg >= argc )
				{
					fprintf( stderr, "Expected argument after %s\n", argv[nCurArg-1] );
					exit( 1 );
				}
				s_sPrivateKeyFile = argv[nCurArg];
				break;

			case 'k':
				nCurArg++;
				if ( nCurArg >= argc )
				{
					fprintf( stderr, "Expected argument after %s\n", argv[nCurArg-1] );
					exit( 1 );
				}
				s_sPublicKeyFile = argv[nCurArg];
				break;

			default:
				Error( "Unrecognized option '%s'\n", argv[nCurArg] );
		}
		nCurArg++;
		
	}
	argc -= ( nCurArg - 1 );
	argv += ( nCurArg - 1 );

	if ( argc < 2 )
	{
		Error( "No command specified.  Try 'vpk -?' for info.\n" );
	}

	const char *pszCommand = argv[1];
	if ( V_stricmp( pszCommand, "l" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		// list a file
		char szActualFileName[MAX_PATH];
		CPackedStore mypack( argv[2], szActualFileName, g_pFullFileSystem );
		CUtlStringList fileNames;
		mypack.GetFileList( fileNames, pszCommand[0] == 'L', true );
		for( int i = 0 ; i < fileNames.Count(); i++ )
		{
			printf( "%s\n", fileNames[i] );
		}
	}
	else if ( V_strcmp( pszCommand, "a" ) == 0 )
	{
		if ( argc < 3 )
		{
			fprintf( stderr, "Not enough arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		char szActualFileName[MAX_PATH];
		CPackedStore mypack( argv[2], szActualFileName, g_pFullFileSystem, true );
		CheckLoadKeyFilesForSigning( mypack );
		for( int i = 3; i < argc; i++ )
		{
			if ( argv[i][0] == '@' )
			{
				// response file?
				CRequiredInputTextFile hResponseFile( argv[i] + 1  );
				CUtlStringList fileList;
				hResponseFile.ReadLines( fileList );
				for( int i = 0 ; i < fileList.Count(); i++ )
				{
					AddFileToPack( mypack, fileList[i] );
				}
			}
			else
			{
				AddFileToPack( mypack, argv[i] );
			}
		}
		mypack.HashEverything();
		mypack.Write();
	}
	else if ( V_strcmp( pszCommand, "k" ) == 0 )
	{
		if ( argc != 4 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		char szActualFileName[MAX_PATH];
		CPackedStore mypack( argv[2], szActualFileName, g_pFullFileSystem, true );
		mypack.SetWriteChunkSize( s_iMultichunkSize * 1024*1024 );

		VPKBuilder builder( mypack );
		builder.LoadInputKeys( argv[3] );
		builder.BuildFromInputKeys();
	}
	else if ( V_strcmp( pszCommand, "x" ) == 0 )
	{
		if ( argc < 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		// extract a file
		char szActualFileName[MAX_PATH];
		CPackedStore mypack( argv[2], szActualFileName, g_pFullFileSystem );
		for( int i = 3; i < argc; i++ )
		{
			CPackedStoreFileHandle pData = mypack.OpenFile( argv[i] );
			if ( pData )
			{
				printf( "extracting %s\n", argv[i] );
				COutputFile outF( argv[i] );
				if ( !outF.IsOk() )
				{
					fprintf( stderr, "Unable to create '%s'.\n", argv[i] );
					exit(1);
				}
				int nBytes = pData.m_nFileSize;
				while( nBytes )
				{
					char cpBuf[65535];
					int nReadSize = MIN( sizeof( cpBuf ), nBytes );
					mypack.ReadData( pData, cpBuf, nReadSize );
					outF.Write( cpBuf, nReadSize );
					nBytes -= nReadSize;
				}
				outF.Close();
			}
			else
			{
				printf( "couldn't find file %s\n", argv[i] );
				break;
			}
		}
	}
	else if ( V_strcmp( pszCommand, "B" ) == 0 )
	{
		if ( argc != 4 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		// benchmark
		CRequiredInputTextFile hResponseFile( argv[3] );
		CUtlStringList files;
		hResponseFile.ReadLines( files );
		printf("%d files\n", files.Count() );
		float stime = Plat_FloatTime();
		BenchMark( files );
		printf( " time no pack = %f\n", Plat_FloatTime() - stime );
		//g_pFullFileSystem->AddVPKFile( argv[2] );
		//stime = Plat_FloatTime();
		//BenchMark( files );
		//printf( " time pack = %f\n", Plat_FloatTime() - stime );
	}
	else if ( V_strcmp( pszCommand, "rehash" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		char szActualFileName[MAX_PATH];
		CPackedStore mypack( argv[2], szActualFileName, g_pFullFileSystem, true );
		CheckLoadKeyFilesForSigning( mypack );
		mypack.HashEverything();
		mypack.Write();
	}
	else if ( V_strcmp( pszCommand, "checkhash" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		CheckHashes( argv[2] );
	}
#ifdef VPK_ENABLE_SIGNING
	else if ( V_strcmp( pszCommand, "generate_keypair" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		GenerateKeyPair( argv[2] );
	}
	else if ( V_strcmp( pszCommand, "checksig" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		CheckSignature( argv[2] );
	}
	else if ( V_strcmp( pszCommand, "dumpsig" ) == 0 )
	{
		if ( argc != 3 )
		{
			fprintf( stderr, "Incorrect number of arguments for '%s' command.\n", pszCommand );
			exit(1);
		}

		DumpSignatureInfo( argv[2] );
	}
#endif
	else if ( argc == 2 && g_pFullFileSystem->IsDirectory( argv[1] ) )
	{
		DroppedDirectory( argv[1] );
	}
	else if ( argc == 2 && V_GetFileExtension( argv[1] ) && V_stristr( V_GetFileExtension( argv[1] ), "vpk") )
	{
		DroppedVpk( argv[1] );
	}
	else
	{
		Error( "Unknown command '%s'.  Try 'vpk -?' for info.\n", pszCommand );
	}

	return 0;
}
