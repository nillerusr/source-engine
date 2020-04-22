//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 Creation for all studiomdl generated files (mdl, vvd, vtx, ani, phy)
//
//=====================================================================================//

#include "MakeGameData.h"
#include "filesystem.h"
#include "../../common/bsplib.h"
#include "ibsppack.h"
#include "vtf/vtf.h"
#include "../../game/server/ai_hull.h"
#include "zip_utils.h"

#define AINET_VERSION_NUMBER	37
#define MAX_NODES				1500

bool ReadBSPHeader( const char *pFilename, dheader_t *pHeader )
{
	V_memset( pHeader, 0, sizeof( dheader_t ) );

	int handle = _open( pFilename, _O_RDONLY|_O_BINARY );
	if ( handle == -1 )
	{
		return false;
	}

	_read( handle, pHeader, sizeof( dheader_t ) );
	close( handle );

	return true;
}

//-----------------------------------------------------------------------------
// Run possible lod culling fixup
//-----------------------------------------------------------------------------
bool ConvertVHV( const char *pVhvFilename, const char *pModelName, CUtlBuffer &sourceBuffer, CUtlBuffer &targetBuffer )
{
	// find strip info from model
	char vsiFilename[MAX_PATH];
	V_strncpy( vsiFilename, pModelName, sizeof( vsiFilename ) );
	V_SetExtension( vsiFilename, ".vsi", sizeof( vsiFilename ) );

	CUtlBuffer vsiBuffer;
	if ( !g_pFullFileSystem->ReadFile( vsiFilename, NULL, vsiBuffer ) )
	{
		// cannot convert bsp's without model converions
		Msg( "Error! Missing expected model conversion file '%s'. Cannot perform VHV fixup.\n", vsiFilename );
		return false;
	}

	IMdlStripInfo *pMdlStripInfo = NULL;
	if ( !mdllib->CreateNewStripInfo( &pMdlStripInfo ) )
	{
		Msg( "Error! Failed to allocate strip info object\n" );
		return false;
	}

	if ( !pMdlStripInfo->UnSerialize( vsiBuffer ) )
	{
		Msg( "Error! Failed to unserialize strip info object '%s'\n", vsiFilename );
		pMdlStripInfo->DeleteThis();
		return false;
	}

	long originalChecksum = 0;
	long newChecksum = 0;
	if ( !pMdlStripInfo->GetCheckSum( &originalChecksum, &newChecksum ) )
	{
		Msg( "Error! Failed to get checksums from '%s'\n", vsiFilename );
		pMdlStripInfo->DeleteThis();
		return false;
	}
	
	HardwareVerts::FileHeader_t *pVHVhdr = (HardwareVerts::FileHeader_t*)sourceBuffer.Base();
	if ( pVHVhdr->m_nChecksum != originalChecksum )
	{
		// vhv file should have matching original checksums
		Msg( "Error! Mismatched checksums from '%s' and '%s'\n", vsiFilename, pVhvFilename );
		pMdlStripInfo->DeleteThis();
		return false;
	}

	targetBuffer.EnsureCapacity( sourceBuffer.TellMaxPut() );
	targetBuffer.Put( sourceBuffer.Base(), sourceBuffer.TellMaxPut() );
	if ( !pMdlStripInfo->StripHardwareVertsBuffer( targetBuffer ) )
	{
		pMdlStripInfo->DeleteThis();
		return false;
	}

	// success
	pMdlStripInfo->DeleteThis();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Generate .360 bsp
//-----------------------------------------------------------------------------
bool CreateTargetFile_BSP( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	CUtlBuffer targetBuffer;
	CUtlBuffer zipBuffer;
	CUtlBuffer fileBuffer;
	CUtlBuffer tempBuffer;
	char tempZipName[MAX_PATH];
	char tempSwapName[MAX_PATH];

	tempZipName[0] = '\0';
	tempSwapName[0] = '\0';
	void *pPakData = NULL;
	int pakSize = 0;
	CXZipTool *pNewXZip = NULL;

	if ( !g_bModPathIsValid )
	{
		Msg( "Indeterminate mod path, Cannot perform BSP conversion\n" );
		return false;
	}

	// Load bsppack.dll
	void *pBSPPack;
	CSysModule *pBSPModule;
	if ( !Sys_LoadInterface( "bsppack.dll", IBSPPACK_VERSION_STRING, &pBSPModule, &pBSPPack ) )
	{
		Msg( "Failed to load bsppack interface\n" );
		return false;
	}

	scriptlib->MakeTemporaryFilename( g_szModPath, tempSwapName, sizeof( tempSwapName ) );	

	// Swaps the bsp directly to disk
	bool bOK = ((IBSPPack*)pBSPPack)->SwapBSPFile( g_pFullFileSystem, pSourceName, tempSwapName, false, ConvertVTFTo360Format, ConvertVHV, CompressCallback );
	if ( !bOK )
	{
		goto cleanUp;
	}

	// get the pak file from the swapped bsp
	bOK = ((IBSPPack*)pBSPPack)->GetPakFileLump( g_pFullFileSystem, tempSwapName, &pPakData, &pakSize );
	if ( !bOK )
	{
		goto cleanUp;
	}

	// build an xzip version of the pak file
	if ( pPakData && pakSize )
	{
		// mount current pak file
		IZip *pOldZip = IZip::CreateZip( false, true );
		pOldZip->ParseFromBuffer( pPakData, pakSize );

		// start a new xzip version
		scriptlib->MakeTemporaryFilename( g_szModPath, tempZipName, sizeof( tempZipName ) );		

		pNewXZip = new CXZipTool;
		pNewXZip->Begin( tempZipName, XBOX_DVD_SECTORSIZE );

		// iterate each file in existing zip, add to new zip
		int zipIndex = -1;
		for ( ;; ) 
		{
			char filename[MAX_PATH];
			filename[0] = '\0';
			int fileSize = 0;
			zipIndex = pOldZip->GetNextFilename( zipIndex, filename, sizeof( filename ), fileSize );
			if ( zipIndex == -1 )
			{
				break;
			}

			fileBuffer.Purge();
			bOK = pOldZip->ReadFileFromZip( filename, false, fileBuffer );
			if ( !bOK )
			{
				goto cleanUp;
			}

			bOK = pNewXZip->AddBuffer( filename, fileBuffer, true );
			if ( !bOK )
			{
				goto cleanUp;
			}
		}

		IZip::ReleaseZip( pOldZip );
		pNewXZip->End();

		// read the new zip into memory
		bOK = scriptlib->ReadFileToBuffer( tempZipName, zipBuffer );
		if ( !bOK )
		{
			goto cleanUp;
		}

		// replace old pak lump with new zip
		bOK = ((IBSPPack*)pBSPPack)->SetPakFileLump( g_pFullFileSystem, tempSwapName, tempSwapName, zipBuffer.Base(), zipBuffer.TellMaxPut() );
		if ( !bOK )
		{
			goto cleanUp;
		}
	}

	bOK = scriptlib->ReadFileToBuffer( tempSwapName, targetBuffer );
	if ( !bOK )
	{
		goto cleanUp;
	}
 
	// never zip, always write local file
	bOK = WriteBufferToFile( pTargetName, targetBuffer, false, WRITE_TO_DISK_ALWAYS );

cleanUp:
	if ( tempZipName[0] )
	{
		_unlink( tempZipName );
	}
	if ( tempSwapName[0] )
	{
		_unlink( tempSwapName );
	}

	Sys_UnloadModule( pBSPModule );

	if ( pPakData )
	{
		free( pPakData );
	}
	
	delete pNewXZip;

	return bOK;
}


//-----------------------------------------------------------------------------
// Purpose: Generate .360 node graphs
//-----------------------------------------------------------------------------
bool CreateTargetFile_AIN( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	CUtlBuffer sourceBuf;
	if ( !scriptlib->ReadFileToBuffer( pSourceName, sourceBuf ) )
	{
		return false;
	}

	// the pc ain is tied to the pc bsp and should have been generated after the bsp
	char szBspName[MAX_PATH];
	char szBspPath[MAX_PATH];
	V_FileBase( pSourceName, szBspName, sizeof( szBspName ) );
	V_ExtractFilePath( pSourceName, szBspPath, sizeof( szBspPath ) );
	V_AppendSlash( szBspPath, sizeof( szBspPath ) );
	V_strncat( szBspPath, "..\\", sizeof( szBspPath ) );
	V_strncat( szBspPath, szBspName, sizeof( szBspPath ) );
	V_strncat( szBspPath, ".bsp", sizeof( szBspPath ) );
	if ( scriptlib->CompareFileTime( pSourceName, szBspPath ) < 0 )
	{
		// ain has a smaller filetime, thus older than bsp
		Msg( "%s: Need to regenerate PC nodegraph (stale)\n", pSourceName );
		if ( !g_bForce )
		{
			return false;
		}
	}

	// Check the version
	if ( sourceBuf.GetChar() == 'V' && sourceBuf.GetChar() == 'e' && sourceBuf.GetChar() == 'r' )
	{
		Msg( "%s: Need to regenerate PC nodegraph (bad format)\n", pSourceName );
		return false;
	}

	// reset
	sourceBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	// Version number
	int version = sourceBuf.GetInt();
	if ( version != AINET_VERSION_NUMBER )
	{
		Msg( "%s: Need to regenerate PC nodegraph (got version '%d', expected '%d')\n", pSourceName, version, AINET_VERSION_NUMBER );
		return false;
	}

	// check the map revision
	int mapVersion = sourceBuf.GetInt();
	dheader_t bspHeader;
	if ( ReadBSPHeader( szBspPath, &bspHeader ) )
	{
		if ( mapVersion != bspHeader.mapRevision )
		{
			Msg( "%s: Need to regenerate PC nodegraph (ai revision '%d' does not match bsp revision '%d')\n", pSourceName, mapVersion, bspHeader.mapRevision  );
			return false;
		}
	}
	else
	{
		Msg( "%s: Could not find expected bsp '%s'\n", pSourceName, szBspPath );
	}

	// Nodes
	int nodeCt = sourceBuf.GetInt();
	if ( nodeCt > MAX_NODES || nodeCt < 0 )
	{
		Msg( "%s: Need to regenerate PC nodegraph (corrupt)\n", pSourceName );
		return false;
	}

	CUtlBuffer targetBuf;
	targetBuf.ActivateByteSwapping( true );

	CByteswap swap;
	swap.ActivateByteSwapping( true );

	targetBuf.PutInt( version );
	targetBuf.PutInt( mapVersion );
	targetBuf.PutInt( nodeCt );

	int numFloats = NUM_HULLS + 4;
	for ( int node = 0; node < nodeCt; ++node )
	{
		targetBuf.EnsureCapacity( targetBuf.TellPut() + numFloats * sizeof( float ) );
		swap.SwapBufferToTargetEndian<float>( (float*)targetBuf.PeekPut(), (float*)sourceBuf.PeekGet(), numFloats );
		sourceBuf.SeekGet( CUtlBuffer::SEEK_CURRENT, numFloats * sizeof( float ) );
		targetBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, numFloats * sizeof( float ) );

		targetBuf.PutChar( sourceBuf.GetChar() );

		// Align the remaining data
		targetBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, 3 );

		targetBuf.PutUnsignedShort( sourceBuf.GetUnsignedShort() );
		targetBuf.PutShort( sourceBuf.GetShort() );
	}

	// Node links
	int totalNumLinks = sourceBuf.GetInt();
	targetBuf.PutInt( totalNumLinks );

	for ( int link = 0; link < totalNumLinks; ++link )
	{
		targetBuf.PutShort( sourceBuf.GetShort() );
		targetBuf.PutShort( sourceBuf.GetShort() );
		targetBuf.Put( sourceBuf.PeekGet(), NUM_HULLS );
		sourceBuf.SeekGet( CUtlBuffer::SEEK_CURRENT, NUM_HULLS );
	}

	// WC lookup table
	targetBuf.EnsureCapacity( targetBuf.TellPut() + nodeCt * sizeof( int ) );
	swap.SwapBufferToTargetEndian<int>( (int*)targetBuf.PeekPut(), (int*)sourceBuf.PeekGet(), nodeCt );
	targetBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, nodeCt * sizeof( int ) );

	// Write the file out
	return WriteBufferToFile( pTargetName, targetBuf, bWriteToZip, WRITE_TO_DISK_ALWAYS );
}

bool GetDependants_BSP( const char *pBspName, CUtlVector< CUtlString > *pList )
{
	if ( !g_bModPathIsValid )
	{
		Msg( "Indeterminate mod path, Cannot perform BSP conversion\n" );
		return false;
	}

	// Load bsppack.dll
	void *pBSPPack;
	CSysModule *pBSPModule;
	if ( !Sys_LoadInterface( "bsppack.dll", IBSPPACK_VERSION_STRING, &pBSPModule, &pBSPPack ) )
	{
		Msg( "Failed to load bsppack interface\n" );
		return false;
	}

	// 360 builds a more complete reslist that includes bsp internal files
	// build full path to bsp file
	char szBspFilename[MAX_PATH];
	V_ComposeFileName( g_szGamePath, pBspName, szBspFilename, sizeof( szBspFilename ) );

	bool bOK = ((IBSPPack*)pBSPPack)->GetBSPDependants( g_pFullFileSystem, szBspFilename, pList );

	Sys_UnloadModule( pBSPModule );

	return bOK;
}
