//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "sceneimage.h"
#include "choreoscene.h"
#include "iscenetokenprocessor.h"
#include "scenefilecache/SceneImageFile.h"

#include "lzma/lzma.h"

#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlvector.h"
#include "tier1/UtlSortVector.h"

#include "scriplib.h"
#include "cmdlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSceneImage : public ISceneImage
{
public:
	virtual bool CreateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status );
	// This will update the scenes.image file, or create a new one if it doesn't exist
	virtual bool UpdateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status, CUtlString *pFilesToUpdate, int nUpdateCount );

private:

	bool WriteSceneImageFile( CUtlBuffer &targetBuffer, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus );
};

static CSceneImage g_SceneImage;
ISceneImage *g_pSceneImage = &g_SceneImage;

struct SceneFile_t
{
	SceneFile_t()
	{
		msecs = 0;
		lastspeak_msecs = 0;
		crcFileName = (CRC32_t)0u;
	}

	// This gets used when we load back in from disk
	CRC32_t		crcFileName;
	// Otherwise, it's this
	CUtlString	fileName;
	CUtlBuffer	compiledBuffer;

	unsigned int		msecs;
	unsigned int		lastspeak_msecs;
	CUtlVector< short >	soundList;
};
CUtlVector< SceneFile_t > g_SceneFiles;

//-----------------------------------------------------------------------------
// Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char *CurrentToken( void )
	{
		return token;
	}

	bool GetToken( bool crossline )
	{
		return ::GetToken( crossline ) ? true : false;
	}

	bool TokenAvailable( void )
	{
		return ::TokenAvailable() ? true : false;
	}

	void Error( const char *fmt, ... )
	{
		char string[2048];
		va_list argptr;
		va_start( argptr, fmt );
		Q_vsnprintf( string, sizeof(string), fmt, argptr );
		va_end( argptr );

		Warning( "%s", string );
		Assert( 0 );
	}
};
static CSceneTokenProcessor g_SceneTokenProcessor;
ISceneTokenProcessor *tokenprocessor = &g_SceneTokenProcessor;

// a simple case insensitive string pool
// the final pool contains all the unique strings seperated by a null
class CChoreoStringPool : public IChoreoStringPool
{
public:
	CChoreoStringPool() : m_StringMap( true )
	{
		m_nOffset = 0;
	}

	// Returns a valid id into the string table
	virtual short FindOrAddString( const char *pString )
	{
		int stringId = m_StringMap.Find( pString );
		if ( stringId != m_StringMap.InvalidIndex() )
		{
			// found in pool
			return stringId;
		}

		int &nOffset = m_StringMap[pString];
		nOffset = m_nOffset;
		// advance by string and null
		m_nOffset += strlen( pString ) + 1;

		stringId = m_StringMap.Find( pString );
		Assert( stringId >= 0 && stringId <= 32767 );

		return stringId;
	}

	virtual bool GetString( short stringId, char *buff, int buffSize )
	{
		if ( stringId < 0 || stringId >= m_StringMap.GetNumStrings() )
		{
			V_strncpy( buff, "", buffSize );
			return false;
		}
		V_strncpy( buff, m_StringMap.String( stringId ), buffSize );
		return true;
	}

	int GetNumStrings()
	{
		return m_StringMap.GetNumStrings();
	}

	unsigned int GetPoolSize()
	{
		return m_nOffset;
	}

	// build the final pool
	void GetTableAndPool( CUtlVector< unsigned int > &offsets, CUtlBuffer &buffer )
	{
		offsets.Purge();
		buffer.Purge();

		offsets.EnsureCapacity( m_StringMap.GetNumStrings() );
		buffer.EnsureCapacity( m_nOffset );

		unsigned int currentOffset = 0;
		for ( int i = 0; i < m_StringMap.GetNumStrings(); i++ )
		{
			offsets.AddToTail( currentOffset );

			const char *pString = m_StringMap.String( i );
			buffer.Put( pString, strlen( pString ) + 1 ); 

			currentOffset += strlen( pString ) + 1;
		}
		Assert( currentOffset == m_nOffset );

		// align string pool to end on dword boundary
		while ( buffer.TellPut() & 0x03 )
		{
			buffer.PutChar( '\0' );
			m_nOffset++;
		}
	}

	void DumpPool()
	{
		for ( int i = 0; i < m_StringMap.GetNumStrings(); i++ )
		{
			const char *pString = m_StringMap.String( i );
			Msg( "%s\n", pString );
		}
	}

	void Reset()
	{
		m_StringMap.Purge();
		m_nOffset = 0;
	}

private:
	CUtlStringMap< int >	m_StringMap;
	unsigned int			m_nOffset;
};
CChoreoStringPool g_ChoreoStringPool;

//-----------------------------------------------------------------------------
// Helper for crawling events to determine sounds
//-----------------------------------------------------------------------------
void FindSoundsInEvent( CChoreoEvent *pEvent, CUtlVector< short >& soundList )
{
	if ( !pEvent || pEvent->GetType() != CChoreoEvent::SPEAK )
		return;

	unsigned short stringId = g_ChoreoStringPool.FindOrAddString( pEvent->GetParameters() );
	if ( soundList.Find( stringId ) == soundList.InvalidIndex() )
	{
		soundList.AddToTail( stringId );
	}

	if ( pEvent->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( pEvent->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			stringId = g_ChoreoStringPool.FindOrAddString( tok );
			if ( soundList.Find( stringId ) == soundList.InvalidIndex() )
			{
				soundList.AddToTail( stringId );
			}
		}
	}
}

bool UpdateTargetFile_VCD( SceneFile_t *pEntry, const char *pSourceName, const char *pTargetName, bool bWriteToZip, bool bLittleEndian )
{
	pEntry->crcFileName = (CRC32_t)0u;
	pEntry->fileName.Set( pSourceName );

	CUtlBuffer sourceBuf;
	if ( !scriptlib->ReadFileToBuffer( pSourceName, sourceBuf ) )
	{
		return false;
	}

	CRC32_t crcSource;
	CRC32_Init( &crcSource );
	CRC32_ProcessBuffer( &crcSource, sourceBuf.Base(), sourceBuf.TellPut() );
	CRC32_Final( &crcSource );

	ParseFromMemory( (char *)sourceBuf.Base(), sourceBuf.TellPut() );

	CChoreoScene *pChoreoScene = ChoreoLoadScene( pSourceName, NULL, &g_SceneTokenProcessor, Msg );
	if ( !pChoreoScene )
	{
		return false;
	}

	// Walk all events looking for SPEAK events
	CChoreoEvent *pEvent;
	for ( int i = 0; i < pChoreoScene->GetNumEvents(); ++i )
	{
		pEvent = pChoreoScene->GetEvent( i );
		FindSoundsInEvent( pEvent, pEntry->soundList );
	}

	// calc duration
	pEntry->msecs = (unsigned int)( pChoreoScene->FindStopTime() * 1000.0f + 0.5f );
	pEntry->lastspeak_msecs = (unsigned int)( pChoreoScene->FindLastSpeakTime() * 1000.0f + 0.5f );

	pEntry->compiledBuffer.Clear();
	// compile to binary buffer
	pEntry->compiledBuffer.SetBigEndian( !bLittleEndian );
	pChoreoScene->SaveToBinaryBuffer( pEntry->compiledBuffer, crcSource, &g_ChoreoStringPool );

	unsigned int compressedSize;
	unsigned char *pCompressedBuffer = LZMA_Compress( (unsigned char *)pEntry->compiledBuffer.Base(), pEntry->compiledBuffer.TellPut(), &compressedSize );
	if ( pCompressedBuffer )
	{
		// replace the compiled buffer with the compressed version
		pEntry->compiledBuffer.Purge();
		pEntry->compiledBuffer.EnsureCapacity( compressedSize );
		pEntry->compiledBuffer.Put( pCompressedBuffer, compressedSize );
		free( pCompressedBuffer );
	}

	delete pChoreoScene;

	return true;
}


//-----------------------------------------------------------------------------
// Create binary compiled version of VCD. Stores to a dictionary for later
// post processing
//-----------------------------------------------------------------------------
bool CreateTargetFile_VCD( const char *pSourceName, const char *pTargetName, bool bWriteToZip, bool bLittleEndian )
{
	int iScene = g_SceneFiles.AddToTail();

	bool bRet = UpdateTargetFile_VCD( &g_SceneFiles[ iScene ], pSourceName, pTargetName, bWriteToZip, bLittleEndian );
	if ( !bRet )
	{
		g_SceneFiles.Remove( iScene );
	}
	return bRet;
}


class CSceneImageEntryLessFunc
{
public:
	bool Less( const SceneImageEntry_t &entryLHS, const SceneImageEntry_t &entryRHS, void *pCtx )
	{
		return entryLHS.crcFilename < entryRHS.crcFilename;
	}
};



//-----------------------------------------------------------------------------
// A Scene image file contains all the compiled .XCD
//-----------------------------------------------------------------------------
bool CSceneImage::CreateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus )
{
	CUtlVector<fileList_t>	vcdFileList;
	CUtlSymbolTable			vcdSymbolTable( 0, 32, true );

	Msg( "\n" );

	// get all the VCD files according to the seacrh paths
	char searchPaths[512];
	g_pFullFileSystem->GetSearchPath( "GAME", false, searchPaths, sizeof( searchPaths ) );
	char *pPath = strtok( searchPaths, ";" );
	while ( pPath )
	{
		int currentCount = vcdFileList.Count();

		char szPath[MAX_PATH];
		V_ComposeFileName( pPath, "scenes/*.vcd", szPath, sizeof( szPath ) );

		scriptlib->FindFiles( szPath, true, vcdFileList );

		Msg( "Scenes: Searching '%s' - Found %d scenes.\n", szPath, vcdFileList.Count() - currentCount );

		pPath = strtok( NULL, ";" );
	}

	if ( !vcdFileList.Count() )
	{
		Msg( "Scenes: No Scene Files found!\n" );
		return false;
	}

	// iterate and convert all the VCD files
	bool bGameIsTF = V_stristr( pchModPath, "\\tf" ) != NULL;
	for ( int i=0; i<vcdFileList.Count(); i++ )
	{
		const char *pFilename = vcdFileList[i].fileName.String();
		const char *pSceneName = V_stristr( pFilename, "scenes\\" );
		if ( !pSceneName )
		{
			continue;
		}

		if ( !bLittleEndian && bGameIsTF && V_stristr( pSceneName, "high\\" ) )
		{
			continue;
		}

		// process files in order they would be found in search paths
		// i.e. skipping later processed files that match an earlier conversion
		UtlSymId_t symbol = vcdSymbolTable.Find( pSceneName );
		if ( symbol == UTL_INVAL_SYMBOL )
		{
			vcdSymbolTable.AddString( pSceneName );

			pStatus->UpdateStatus( pFilename, bQuiet, i, vcdFileList.Count() );

			if ( !CreateTargetFile_VCD( pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' conversion!\n", pFilename );
			}
		}
	}

	if ( !g_SceneFiles.Count() )
	{
		// nothing to do
		return true;
	}

	return WriteSceneImageFile( targetBuffer, bLittleEndian, bQuiet, pStatus );
}

bool CSceneImage::WriteSceneImageFile( CUtlBuffer &targetBuffer, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus )
{
	Msg( "Scenes: Finalizing %d unique scenes.\n", g_SceneFiles.Count() );

	// get the string pool
	CUtlVector< unsigned int > stringOffsets;
	CUtlBuffer stringPool;
	g_ChoreoStringPool.GetTableAndPool( stringOffsets, stringPool );

	if ( !bQuiet )
	{
		Msg( "Scenes: String Table: %d bytes\n", stringOffsets.Count() * sizeof( int ) );
		Msg( "Scenes: String Pool: %d bytes\n", stringPool.TellPut() );
	}

	// first header, then lookup table, then string pool blob
	int stringPoolStart = sizeof( SceneImageHeader_t ) + stringOffsets.Count() * sizeof( int );
	// then directory
	int sceneEntryStart = stringPoolStart + stringPool.TellPut();
	// then variable sized summaries
	int sceneSummaryStart = sceneEntryStart + g_SceneFiles.Count() * sizeof( SceneImageEntry_t );
	// then variable sized compiled binary scene data
	int sceneDataStart = 0;

	// construct header
	SceneImageHeader_t imageHeader = { 0 };
	imageHeader.nId = SCENE_IMAGE_ID;
	imageHeader.nVersion = SCENE_IMAGE_VERSION;
	imageHeader.nNumScenes = g_SceneFiles.Count();
	imageHeader.nNumStrings = stringOffsets.Count();
	imageHeader.nSceneEntryOffset = sceneEntryStart;
	if ( !bLittleEndian )
	{
		imageHeader.nId = BigLong( imageHeader.nId );
		imageHeader.nVersion = BigLong( imageHeader.nVersion );
		imageHeader.nNumScenes = BigLong( imageHeader.nNumScenes );
		imageHeader.nNumStrings = BigLong( imageHeader.nNumStrings );
		imageHeader.nSceneEntryOffset = BigLong( imageHeader.nSceneEntryOffset );
	}
	targetBuffer.Put( &imageHeader, sizeof( imageHeader ) );

	// header is immediately followed by string table and pool
	for ( int i = 0; i < stringOffsets.Count(); i++ )
	{
		unsigned int offset = stringPoolStart + stringOffsets[i];
		if ( !bLittleEndian )
		{
			offset = BigLong( offset );
		}
		targetBuffer.PutInt( offset );
	}
	Assert( stringPoolStart == targetBuffer.TellPut() );
	targetBuffer.Put( stringPool.Base(), stringPool.TellPut() );

	// construct directory
	CUtlSortVector< SceneImageEntry_t, CSceneImageEntryLessFunc > imageDirectory;
	imageDirectory.EnsureCapacity( g_SceneFiles.Count() );

	// build directory
	// directory is linear sorted by filename checksum for later binary search
	for ( int i = 0; i < g_SceneFiles.Count(); i++ )
	{
		SceneImageEntry_t imageEntry = { 0 };

		CRC32_t crcFilename = g_SceneFiles[ i ].crcFileName;
		if ( crcFilename == 0 )
		{
			Assert( Q_strlen( g_SceneFiles[i].fileName.String() ) > 0 );

			// name needs to be normalized for determinstic later CRC name calc
			// calc crc based on scenes\anydir\anyscene.vcd
			char szCleanName[MAX_PATH];
			V_strncpy( szCleanName, g_SceneFiles[i].fileName.String(), sizeof( szCleanName ) );
			V_strlower( szCleanName );
			V_FixSlashes( szCleanName );
			char *pName = V_stristr( szCleanName, "scenes\\" );
			if ( !pName )
			{
				// must have scenes\ in filename
				Error( "CreateSceneImageFile: Unexpected lack of scenes prefix on %s\n", g_SceneFiles[i].fileName.String() );
			}

			crcFilename = CRC32_ProcessSingleBuffer( pName, strlen( pName ) );
		}

		imageEntry.crcFilename = crcFilename;

		// temp store an index to its file, fixup later, necessary to access post sort
		imageEntry.nDataOffset = i;
		if ( imageDirectory.Find( imageEntry ) != imageDirectory.InvalidIndex() )
		{
			// filename checksums must be unique or runtime binary search would be bogus
			Error( "CreateSceneImageFile: Unexpected filename checksum collision!\n" );
		}		

		imageDirectory.Insert( imageEntry );
	}

	// determine sort order and start of data after dynamic summaries
	CUtlVector< int > writeOrder;
	writeOrder.EnsureCapacity( g_SceneFiles.Count() );
	sceneDataStart = sceneSummaryStart;
	for ( int i = 0; i < imageDirectory.Count(); i++ )
	{
		// reclaim offset, indicates write order of scene file
		int iScene = imageDirectory[i].nDataOffset;
		writeOrder.AddToTail( iScene );

		// march past each variable sized summary to determine start of scene data
		int numSounds = g_SceneFiles[iScene].soundList.Count();
		sceneDataStart += sizeof( SceneImageSummary_t ) + ( numSounds - 1 ) * sizeof( int );
	}

	// finalize and write directory
	Assert( sceneEntryStart == targetBuffer.TellPut() );
	int nSummaryOffset = sceneSummaryStart;
	int nDataOffset = sceneDataStart;
	for ( int i = 0; i < imageDirectory.Count(); i++ )
	{
		int iScene = writeOrder[i];

		imageDirectory[i].nDataOffset = nDataOffset;
		imageDirectory[i].nDataLength = g_SceneFiles[iScene].compiledBuffer.TellPut();
		imageDirectory[i].nSceneSummaryOffset = nSummaryOffset;
		if ( !bLittleEndian )
		{
			imageDirectory[i].crcFilename = BigLong( imageDirectory[i].crcFilename );
			imageDirectory[i].nDataOffset = BigLong( imageDirectory[i].nDataOffset );
			imageDirectory[i].nDataLength = BigLong( imageDirectory[i].nDataLength );
			imageDirectory[i].nSceneSummaryOffset = BigLong( imageDirectory[i].nSceneSummaryOffset );
		}
		targetBuffer.Put( &imageDirectory[i], sizeof( SceneImageEntry_t ) );

		int numSounds = g_SceneFiles[iScene].soundList.Count();
		nSummaryOffset += sizeof( SceneImageSummary_t ) + (numSounds - 1) * sizeof( int );

		nDataOffset += g_SceneFiles[iScene].compiledBuffer.TellPut();
	}

	// finalize and write summaries
	Assert( sceneSummaryStart == targetBuffer.TellPut() );
	for ( int i = 0; i < imageDirectory.Count(); i++ )
	{
		int iScene = writeOrder[i];
		int msecs = g_SceneFiles[iScene].msecs;
		int soundCount = g_SceneFiles[iScene].soundList.Count();
		unsigned int lastspeak = g_SceneFiles[iScene].lastspeak_msecs;
		Assert( lastspeak < msecs );
		if ( !bLittleEndian )
		{
			msecs = BigLong( msecs );
			soundCount = BigLong( soundCount );
			lastspeak = BigLong( lastspeak );
		}
		targetBuffer.PutInt( msecs );
		targetBuffer.PutInt( lastspeak );
		targetBuffer.PutInt( soundCount );
		for ( int j = 0; j < g_SceneFiles[iScene].soundList.Count(); j++ )
		{
			int soundId = g_SceneFiles[iScene].soundList[j];
			if ( !bLittleEndian )
			{
				soundId = BigLong( soundId );
			}
			targetBuffer.PutInt( soundId );
		}
	}

	// finalize and write data
	Assert( sceneDataStart == targetBuffer.TellPut() );
	for ( int i = 0; i < imageDirectory.Count(); i++ )
	{	
		int iScene = writeOrder[i];
		targetBuffer.Put( g_SceneFiles[iScene].compiledBuffer.Base(), g_SceneFiles[iScene].compiledBuffer.TellPut() );
	}

	if ( !bQuiet )
	{
		Msg( "Scenes: Final size: %.2f MB\n", targetBuffer.TellPut() / (1024.0f * 1024.0f ) );
	}

	// cleanup
	g_SceneFiles.Purge();

	return true;
}

static int FindSceneByCRC( CRC32_t crcFilename )
{
	// use binary search, entries are sorted by ascending crc
	int nLowerIdx = 1;
	int nUpperIdx = g_SceneFiles.Count();
	for ( ;; )
	{
		if ( nUpperIdx < nLowerIdx )
		{
			return -1;
		}
		else
		{
			int nMiddleIndex = ( nLowerIdx + nUpperIdx )/2;
			CRC32_t nProbe = g_SceneFiles[nMiddleIndex-1].crcFileName;
			if ( crcFilename < nProbe )
			{
				nUpperIdx = nMiddleIndex - 1;
			}
			else
			{
				if ( crcFilename > nProbe )
				{
					nLowerIdx = nMiddleIndex + 1;
				}
				else
				{
					return nMiddleIndex - 1;
				}
			}
		}
	}

	return -1;
}

// This will update the scenes.image file, or create a new one if it doesn't exist
// The caller should pass in the existing .image file in targetBuffer!!!
bool CSceneImage::UpdateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus, CUtlString *pFilesToUpdate, int nUpdateCount )
{
	// Prime everything using existing data file
	if ( targetBuffer.TellPut() <= 0 )
	{
		return CreateSceneImageFile( targetBuffer, pchModPath, bLittleEndian, bQuiet, pStatus );
	}

	bool bSuccess = true;

	g_SceneFiles.Purge();
	g_ChoreoStringPool.Reset();

	// Rewind to start
	targetBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Load stuff
	SceneImageHeader_t imageHeader;
	targetBuffer.Get( &imageHeader, sizeof( imageHeader ) );
	if ( !bLittleEndian )
	{
		imageHeader.nId = BigLong( imageHeader.nId );
		imageHeader.nVersion = BigLong( imageHeader.nVersion );
		imageHeader.nNumScenes = BigLong( imageHeader.nNumScenes );
		imageHeader.nNumStrings = BigLong( imageHeader.nNumStrings );
		imageHeader.nSceneEntryOffset = BigLong( imageHeader.nSceneEntryOffset );
	}

	if ( imageHeader.nId != SCENE_IMAGE_ID )
	{
		bSuccess = false;
	}
	if ( imageHeader.nVersion != SCENE_IMAGE_VERSION )
	{
		bSuccess = false;
	}

	if ( bSuccess )
	{
		// Re-Build the string pool

		// first header, then lookup table, then string pool blob
		int stringPoolStart = sizeof( SceneImageHeader_t ) + imageHeader.nNumStrings * sizeof( int );
		// then directory
		int sceneEntryStart = imageHeader.nSceneEntryOffset;

		// unsigned int *pOffset = (unsigned int *)( (byte *)targetBuffer.Base() + targetBuffer.TellGet() );

		targetBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, stringPoolStart );

		char str[ 4096 ];
		for ( int i = 0; i < imageHeader.nNumStrings; ++i )
		{
			targetBuffer.GetString( str, sizeof( str ) );
			g_ChoreoStringPool.FindOrAddString( str );
		}

		// Now read in the file data

		targetBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, sceneEntryStart );

		// get scene summary
		SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)targetBuffer.Base() + sceneEntryStart );

		for ( int i = 0; i < imageHeader.nNumScenes; ++i )
		{
			SceneImageEntry_t *pEntry = &pEntries[ i ];
			if ( !bLittleEndian )
			{
				pEntry->crcFilename = BigLong( pEntry->crcFilename );
				pEntry->nDataOffset = BigLong( pEntry->nDataOffset );
				pEntry->nDataLength = BigLong( pEntry->nDataLength );
				pEntry->nSceneSummaryOffset = BigLong( pEntry->nSceneSummaryOffset );
			}

			SceneImageSummary_t * RESTRICT pSummary = (SceneImageSummary_t *)( (byte *)targetBuffer.Base() + pEntry->nSceneSummaryOffset );

			unsigned char *pData = (unsigned char *)targetBuffer.Base() + pEntry->nDataOffset;

			// Now read in the data
			int idx = g_SceneFiles.AddToTail();
			SceneFile_t &scene = g_SceneFiles[ idx ];

			// We only load the crc based filenames for appending/replacing
			scene.crcFileName = pEntry->crcFilename;
			scene.compiledBuffer.Put( pData, pEntry->nDataLength );
			scene.msecs = pSummary->msecs;
			scene.lastspeak_msecs = pSummary->lastspeech_msecs;
			// Load sounds
			for ( int j = 0 ; j < pSummary->numSounds; ++j )
			{
				scene.soundList.AddToTail( pSummary->soundStrings[ j ] );
			}
		}
	}

	Assert( g_SceneFiles.Count() == imageHeader.nNumScenes );

	// Now validate that the scenes list is sorted correctly
	CRC32_t current = (CRC32_t)0;
	for ( int i = 0 ; i < g_SceneFiles.Count(); ++i )
	{
		CRC32_t crc = g_SceneFiles[ i ].crcFileName;
		Assert( crc != (CRC32_t)0 );
		if ( crc <= current )
		{
			Error( "UpdateSceneImageFile:  Scene Files not in CRC order\n" );
		}
		current = crc;
	}

	// Now add the additional files
	bool bGameIsTF = V_stristr( pchModPath, "\\tf" ) != NULL;
	for ( int i = 0; i < nUpdateCount; ++i )
	{
		const char *pFilename = pFilesToUpdate[i].String();
		const char *pSceneName = V_stristr( pFilename, "scenes\\" );
		if ( !pSceneName )
		{
			continue;
		}

		if ( !bLittleEndian && bGameIsTF && V_stristr( pSceneName, "high\\" ) )
		{
			continue;
		}

		// name needs to be normalized for determinstic later CRC name calc
		// calc crc based on scenes\anydir\anyscene.vcd
		char szCleanName[MAX_PATH];
		V_strncpy( szCleanName, pFilename, sizeof( szCleanName ) );
		V_strlower( szCleanName );
		V_FixSlashes( szCleanName );
		char *pName = V_stristr( szCleanName, "scenes\\" );
		if ( !pName )
		{
			// must have scenes\ in filename
			Error( "UpdateSceneImageFile: Unexpected lack of scenes prefix on %s\n", pFilename );
		}

		CRC32_t crcFilename = CRC32_ProcessSingleBuffer( pName, strlen( pName ) );

		pStatus->UpdateStatus( pFilename, bQuiet, i, nUpdateCount );

		int idx = FindSceneByCRC( crcFilename );
		// Not found, append entry
		if ( idx == -1 )
		{
			if ( !CreateTargetFile_VCD( pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' conversion!\n", pFilename );
			}
		}
		else
		// Found it, let's just update entry
		{
			if ( !UpdateTargetFile_VCD( &g_SceneFiles[ idx ], pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' update!\n", pFilename );
			}
		}
	}

	// Now write the final data out
	targetBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	return WriteSceneImageFile( targetBuffer, bLittleEndian, bQuiet, pStatus );
}

