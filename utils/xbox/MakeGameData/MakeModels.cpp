//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 Creation for all studiomdl generated files (mdl, vvd, vtx, ani, phy)
//
//=====================================================================================//

#include "MakeGameData.h"
#include "studiobyteswap.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/hardwareverts.h"
#include "optimize.h"

//-----------------------------------------------------------------------------
// Models are already converted in a pre-pass.
//-----------------------------------------------------------------------------
bool CreateTargetFile_Model( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	// model component should be present
	CUtlBuffer	targetBuffer;
	if ( !scriptlib->ReadFileToBuffer( pTargetName, targetBuffer ) )
	{
		return false;
	}

	// no conversion to write, but possibly zipped
	bool bSuccess = WriteBufferToFile( pTargetName, targetBuffer, bWriteToZip, WRITE_TO_DISK_NEVER );
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Load necessary dlls
//-----------------------------------------------------------------------------
bool InitStudioByteSwap( void )
{
	StudioByteSwap::SetVerbose( false );
	StudioByteSwap::ActivateByteSwapping( true );
	StudioByteSwap::SetCollisionInterface( g_pPhysicsCollision );

	return true;
}

//----------------------------------------------------------------------
// Get list of files that a model requires.
//----------------------------------------------------------------------
bool GetDependants_MDL( const char *pModelName, CUtlVector< CUtlString > *pList )
{
	if ( !g_bModPathIsValid )
	{
		Msg( "Indeterminate mod path, Cannot perform BSP conversion\n" );
		return false;
	}

	CUtlBuffer sourceBuf;
	if ( !g_pFullFileSystem->ReadFile( pModelName, "GAME", sourceBuf ) )
	{
		Msg( "Error! Couldn't open file '%s'!\n", pModelName ); 
		return false;
	}

	studiohdr_t *pStudioHdr = (studiohdr_t *)sourceBuf.Base();
	Studio_ConvertStudioHdrToNewVersion( pStudioHdr );
	if ( pStudioHdr->version != STUDIO_VERSION )
	{
		Msg( "Error! Bad Model '%s', Expecting Version (%d), got (%d)\n", pModelName, STUDIO_VERSION, pStudioHdr->version ); 
		return false;
	}

	char szOutName[MAX_PATH];
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_OBSOLETE )
	{
		V_strncpy( szOutName, "materials/sprites/obsolete.vmt", sizeof( szOutName ) );
		V_FixSlashes( szOutName );
		pList->AddToTail( szOutName );
	}
	else if ( pStudioHdr->textureindex != 0 )
	{
		// iterate each texture
		int	i;
		int	j;
		for ( i = 0; i < pStudioHdr->numtextures; i++ )
		{
			// iterate through all directories until a valid material is found
			bool bFound = false;
			for ( j = 0; j < pStudioHdr->numcdtextures; j++ )
			{
				char szPath[MAX_PATH];
				V_ComposeFileName( "materials", pStudioHdr->pCdtexture( j ), szPath, sizeof( szPath ) );

				// should have been fixed in studiomdl
				// some mdls are ending up with double slashes, borking loads
				int len = strlen( szPath );
				if ( len > 2 && szPath[len-2] == '\\' && szPath[len-1] == '\\' )
				{
					szPath[len-1] = '\0';
				}
			
				V_ComposeFileName( szPath, pStudioHdr->pTexture( i )->pszName(), szOutName, sizeof( szOutName ) ); 
				V_SetExtension( szOutName, ".vmt", sizeof( szOutName ) );

				if ( g_pFullFileSystem->FileExists( szOutName, "GAME" ) )
				{
					bFound = true;
					break;
				}
			}

			if ( bFound )
			{
				pList->AddToTail( szOutName );
			}
		}
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Get the preload data for a vhv file
//-----------------------------------------------------------------------------
bool GetPreloadData_VHV( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	HardwareVerts::FileHeader_t *pHeader = (HardwareVerts::FileHeader_t *)fileBufferIn.Base();

	unsigned int version = BigLong( pHeader->m_nVersion );

	// ensure caller's buffer is clean
	// caller determines preload size, via TellMaxPut()
	preloadBufferOut.Purge();

	if ( version != VHV_VERSION )
	{
		// bad version
		Msg( "Can't preload: '%s', expecting version %d got version %d\n", pFilename, VHV_VERSION, version );
		return false;
	}

	unsigned int nPreloadSize = sizeof( HardwareVerts::FileHeader_t );

	preloadBufferOut.Put( fileBufferIn.Base(), nPreloadSize );

	return true;
}

//-----------------------------------------------------------------------------
// Get the preload data for a vtx file
//-----------------------------------------------------------------------------
bool GetPreloadData_VTX( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	OptimizedModel::FileHeader_t *pHeader = (OptimizedModel::FileHeader_t *)fileBufferIn.Base();

	unsigned int version = BigLong( pHeader->version );

	// ensure caller's buffer is clean
	// caller determines preload size, via TellMaxPut()
	preloadBufferOut.Purge();

	if ( version != OPTIMIZED_MODEL_FILE_VERSION )
	{
		// bad version
		Msg( "Can't preload: '%s', expecting version %d got version %d\n", pFilename, OPTIMIZED_MODEL_FILE_VERSION, version );
		return false;
	}

	unsigned int nPreloadSize = sizeof( OptimizedModel::FileHeader_t );

	preloadBufferOut.Put( fileBufferIn.Base(), nPreloadSize );

	return true;
}

//-----------------------------------------------------------------------------
// Get the preload data for a vvd file
//-----------------------------------------------------------------------------
bool GetPreloadData_VVD( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	vertexFileHeader_t *pHeader = (vertexFileHeader_t *)fileBufferIn.Base();

	unsigned int id = BigLong( pHeader->id );
	unsigned int version = BigLong( pHeader->version );

	// ensure caller's buffer is clean
	// caller determines preload size, via TellMaxPut()
	preloadBufferOut.Purge();

	if ( id != MODEL_VERTEX_FILE_ID )
	{
		// bad version
		Msg( "Can't preload: '%s', expecting id %d got id %d\n", pFilename, MODEL_VERTEX_FILE_ID, id );
		return false;
	}

	if ( version != MODEL_VERTEX_FILE_VERSION )
	{
		// bad version
		Msg( "Can't preload: '%s', expecting version %d got version %d\n", pFilename, MODEL_VERTEX_FILE_VERSION, version );
		return false;
	}

	unsigned int nPreloadSize = sizeof( vertexFileHeader_t );

	preloadBufferOut.Put( fileBufferIn.Base(), nPreloadSize );

	return true;
}

bool CompressFunc( const void *pInput, int inputSize, void **pOutput, int *pOutputSize )
{
	*pOutput = NULL;	
	*pOutputSize = 0;

	if ( !inputSize )
	{
		// nothing to do
		return false;
	}

	unsigned int compressedSize;
	unsigned char *pCompressedOutput = LZMA_OpportunisticCompress( (unsigned char *)pInput, inputSize, &compressedSize );
	if ( pCompressedOutput )
	{
		*pOutput = pCompressedOutput;
		*pOutputSize = compressedSize;
		return true;
	}

	return false;

}

//-----------------------------------------------------------------------------
// Rebuilds all of a MDL's components.
//-----------------------------------------------------------------------------
static bool GenerateModelFiles( const char *pMdlFilename )
{
	CUtlBuffer	tempBuffer;
	int			fileSize;
	int			paddedSize;
	int			swappedSize;

	// .mdl
	CUtlBuffer mdlBuffer;
	if ( !scriptlib->ReadFileToBuffer( pMdlFilename, mdlBuffer ) )
	{
		return false;
	}
	if ( !Studio_ConvertStudioHdrToNewVersion( (studiohdr_t *)mdlBuffer.Base() ))
	{
		Msg("%s needs to be recompiled\n", pMdlFilename );
	}

	// .vtx
	char szVtxFilename[MAX_PATH];
	V_StripExtension( pMdlFilename, szVtxFilename, sizeof( szVtxFilename ) );
	V_strncat( szVtxFilename, ".dx90.vtx", sizeof( szVtxFilename ) );
	CUtlBuffer vtxBuffer;
	bool bHasVtx = ReadFileToBuffer( szVtxFilename, vtxBuffer, false, true );
	
	// .vvd
	char szVvdFilename[MAX_PATH];
	V_StripExtension( pMdlFilename, szVvdFilename, sizeof( szVvdFilename ) );
	V_strncat( szVvdFilename, ".vvd", sizeof( szVvdFilename ) );
	CUtlBuffer vvdBuffer;
	bool bHasVvd = ReadFileToBuffer( szVvdFilename, vvdBuffer, false, true );

	if ( bHasVtx != bHasVvd )
	{
		// paired resources, either mandates the other
		return false;
	}

	// a .mdl file that has .vtx/.vvd gets re-processed to cull lod data
	if ( bHasVtx && bHasVvd )
	{
		// cull lod if needed
		IMdlStripInfo *pStripInfo = NULL;
		bool bResult = mdllib->StripModelBuffers( mdlBuffer, vvdBuffer, vtxBuffer, &pStripInfo );
		if ( !bResult )
		{
			return false;
		}
		if ( pStripInfo )
		{
			// .vsi
			CUtlBuffer vsiBuffer;
			pStripInfo->Serialize( vsiBuffer );
			pStripInfo->DeleteThis();

			// save strip info for later processing
			char szVsiFilename[MAX_PATH];
			V_StripExtension( pMdlFilename, szVsiFilename, sizeof( szVsiFilename ) );
			V_strncat( szVsiFilename, ".vsi", sizeof( szVsiFilename ) );
			WriteBufferToFile( szVsiFilename, vsiBuffer, false, WRITE_TO_DISK_ALWAYS );
		}
	}

	// .ani processing may further update .mdl buffer
	char szAniFilename[MAX_PATH];
	V_StripExtension( pMdlFilename, szAniFilename, sizeof( szAniFilename ) );
	V_strncat( szAniFilename, ".ani", sizeof( szAniFilename ) );
	CUtlBuffer aniBuffer;
	bool bHasAni = ReadFileToBuffer( szAniFilename, aniBuffer, false, true );
	if ( bHasAni )
	{
		// Some vestigal .ani files exist in the tree, only process valid .ani
		if ( ((studiohdr_t*)mdlBuffer.Base())->numanimblocks != 0 )
		{
			// .ani processing modifies .mdl buffer
			fileSize = aniBuffer.TellPut();
			paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;
			aniBuffer.EnsureCapacity( paddedSize );
			tempBuffer.EnsureCapacity( paddedSize );
			V_StripExtension( pMdlFilename, szAniFilename, sizeof( szAniFilename ) );
			V_strncat( szAniFilename, ".360.ani", sizeof( szAniFilename ) );
			swappedSize = StudioByteSwap::ByteswapStudioFile( szAniFilename, tempBuffer.Base(), aniBuffer.PeekGet(), fileSize, (studiohdr_t*)mdlBuffer.Base(), CompressFunc );
			if ( swappedSize > 0 )
			{
				// .ani buffer is replaced with swapped data
				aniBuffer.Purge();
				aniBuffer.Put( tempBuffer.Base(), swappedSize );
				WriteBufferToFile( szAniFilename, aniBuffer, false, WRITE_TO_DISK_ALWAYS );
			}
			else
			{
				return false;				
			}
		}
	}

	// .phy
	char szPhyFilename[MAX_PATH];
	V_StripExtension( pMdlFilename, szPhyFilename, sizeof( szPhyFilename ) );
	V_strncat( szPhyFilename, ".phy", sizeof( szPhyFilename ) );
	CUtlBuffer phyBuffer;
	bool bHasPhy = ReadFileToBuffer( szPhyFilename, phyBuffer, false, true );
	if ( bHasPhy )
	{
		fileSize = phyBuffer.TellPut();
		paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;
		phyBuffer.EnsureCapacity( paddedSize );
		tempBuffer.EnsureCapacity( paddedSize );
		V_StripExtension( pMdlFilename, szPhyFilename, sizeof( szPhyFilename ) );
		V_strncat( szPhyFilename, ".360.phy", sizeof( szPhyFilename ) );
		swappedSize = StudioByteSwap::ByteswapStudioFile( szPhyFilename, tempBuffer.Base(), phyBuffer.PeekGet(), fileSize, (studiohdr_t*)mdlBuffer.Base(), CompressFunc );
		if ( swappedSize > 0 )
		{
			// .phy buffer is replaced with swapped data
			phyBuffer.Purge();
			phyBuffer.Put( tempBuffer.Base(), swappedSize );
			WriteBufferToFile( szPhyFilename, phyBuffer, false, WRITE_TO_DISK_ALWAYS );
		}
		else
		{
			return false;				
		}
	}

	if ( bHasVtx )
	{
		fileSize = vtxBuffer.TellPut();
		paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;
		vtxBuffer.EnsureCapacity( paddedSize );
		tempBuffer.EnsureCapacity( paddedSize );
		V_StripExtension( pMdlFilename, szVtxFilename, sizeof( szVtxFilename ) );
		V_strncat( szVtxFilename, ".dx90.360.vtx", sizeof( szVtxFilename ) );
		swappedSize = StudioByteSwap::ByteswapStudioFile( szVtxFilename, tempBuffer.Base(), vtxBuffer.PeekGet(), fileSize, (studiohdr_t*)mdlBuffer.Base(), CompressFunc );
		if ( swappedSize > 0 )
		{
			// .vtx buffer is replaced with swapped data
			vtxBuffer.Purge();
			vtxBuffer.Put( tempBuffer.Base(), swappedSize );
			WriteBufferToFile( szVtxFilename, vtxBuffer, false, WRITE_TO_DISK_ALWAYS );
		}
		else
		{
			return false;				
		}
	}

	if ( bHasVvd )
	{
		fileSize = vvdBuffer.TellPut();
		paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;
		vvdBuffer.EnsureCapacity( paddedSize );
		tempBuffer.EnsureCapacity( paddedSize );
		V_StripExtension( pMdlFilename, szVvdFilename, sizeof( szVvdFilename ) );
		V_strncat( szVvdFilename, ".360.vvd", sizeof( szVvdFilename ) );
		swappedSize = StudioByteSwap::ByteswapStudioFile( szVvdFilename, tempBuffer.Base(), vvdBuffer.PeekGet(), fileSize, (studiohdr_t*)mdlBuffer.Base(), CompressFunc );
		if ( swappedSize > 0 )
		{
			// .vvd buffer is replaced with swapped data
			vvdBuffer.Purge();
			vvdBuffer.Put( tempBuffer.Base(), swappedSize );
			WriteBufferToFile( szVvdFilename, vvdBuffer, false, WRITE_TO_DISK_ALWAYS );
		}
		else
		{
			return false;				
		}
	}

	// swap and write final .mdl
	fileSize = mdlBuffer.TellPut();
	paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;
	mdlBuffer.EnsureCapacity( paddedSize );
	tempBuffer.EnsureCapacity( paddedSize );
	char szMdlFilename[MAX_PATH];
	V_StripExtension( pMdlFilename, szMdlFilename, sizeof( szMdlFilename ) );
	V_strncat( szMdlFilename, ".360.mdl", sizeof( szMdlFilename ) );
	swappedSize = StudioByteSwap::ByteswapStudioFile( szMdlFilename, tempBuffer.Base(), mdlBuffer.PeekGet(), fileSize, NULL, CompressFunc );
	if ( swappedSize > 0 )
	{
		// .mdl buffer is replaced with swapped data
		mdlBuffer.Purge();
		mdlBuffer.Put( tempBuffer.Base(), swappedSize );
		WriteBufferToFile( szMdlFilename, mdlBuffer, false, WRITE_TO_DISK_ALWAYS );
	}
	else
	{
		return false;				
	}

	return true;
}

//-----------------------------------------------------------------------------
// Returns true if specified model path has a dirty sub-component, and requires
// update.
//-----------------------------------------------------------------------------
static bool ModelNeedsUpdate( const char *pMdlSourcePath )
{
	struct ModelExtensions_t
	{
		const char *pSourceExtension;
		const char *pTargetExtension;
		bool		bSourceMustExist;	// if source exists, so must target
	};
	ModelExtensions_t pExtensions[] =
	{
		{ ".mdl",		".360.mdl",			true },
		{ ".dx90.vtx",	".dx90.360.vtx",	false },
		{ ".vvd",		".360.vvd",			false },
		{ ".phy",		".360.phy",			false },
		{ ".ani",		".360.ani",			false },
		// vtx/vvd generate a vsi, vsi must be fresher to be valid
		{ ".dx90.vtx",	".vsi",				false },
		{ ".vvd",		".vsi",				false },
	};

	if ( g_bForce )
	{
		return true;
	}

	for ( int i = 0; i < ARRAYSIZE( pExtensions ); i++ )
	{
		char szSourcePath[MAX_PATH];
		struct _stat sourceStatBuf;
		V_strncpy( szSourcePath, pMdlSourcePath, sizeof( szSourcePath ) );
		V_SetExtension( szSourcePath, pExtensions[i].pSourceExtension, sizeof( szSourcePath ) );
		int retVal = _stat( szSourcePath, &sourceStatBuf );
		if ( retVal != 0 )
		{
			// couldn't get source
			if ( pExtensions[i].bSourceMustExist )
			{
				return true;
			}
			else
			{
				// source is optional
				continue;
			}
		}

		char szTargetPath[MAX_PATH];
		struct _stat targetStatBuf;
		V_strncpy( szTargetPath, pMdlSourcePath, sizeof( szTargetPath ) );
		V_SetExtension( szTargetPath, pExtensions[i].pTargetExtension, sizeof( szTargetPath ) );
		if ( _stat( szTargetPath, &targetStatBuf ) != 0 )
		{
			// target doesn't exist
			return true;
		}

		if ( difftime( sourceStatBuf.st_mtime, targetStatBuf.st_mtime ) > 0 )
		{
			// source is older (thus newer), update required
			return true;
		}
	}

	return false;
}

static bool ModelNamesLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

//-----------------------------------------------------------------------------
// Models require specialized group handling to generate intermediate lod culled
// versions that are then used as the the source for target conversion.
//-----------------------------------------------------------------------------
bool PreprocessModelFiles( CUtlVector<fileList_t> &fileList )
{
	if ( !InitStudioByteSwap() )
	{
		return false;
	}

	CUtlVector< CUtlString > updateList;
	CUtlRBTree< CUtlString, int > visitedModels( 0, 0, ModelNamesLessFunc );

	char szSourcePath[MAX_PATH];
	strcpy( szSourcePath, g_szSourcePath );
	V_StripFilename( szSourcePath );
	if ( !szSourcePath[0] )
		strcpy( szSourcePath, "." );
	V_AppendSlash( szSourcePath, sizeof( szSourcePath ) );

	char szModelName[MAX_PATH];
	for ( int i=0; i<fileList.Count(); i++ )
	{
		V_strncpy( szModelName, fileList[i].fileName.String(), sizeof( szModelName ) );

		if ( V_stristr( szModelName, ".360." ) )
		{
			// must ignore any target files
			continue;
		}

		// want only model related files
		char *pExtension = V_stristr( szModelName, ".mdl" );
		if ( !pExtension )
		{
			pExtension = V_stristr( szModelName, ".dx90.vtx" );
			if ( !pExtension )
			{
				pExtension = V_stristr( szModelName, ".vvd" );
				if ( !pExtension )
				{
					pExtension = V_stristr( szModelName, ".ani" );
					if ( !pExtension )
					{
						pExtension = V_stristr( szModelName, ".phy" );
						if ( !pExtension )
						{
							pExtension = V_stristr( szModelName, ".vsi" );
							if ( !pExtension )
							{
								continue;
							}
						}
					}
				}
			}
		}

		*pExtension = '\0';
		V_strncat( szModelName, ".mdl", sizeof( szModelName ) );
	
		if ( visitedModels.Find( szModelName ) != visitedModels.InvalidIndex() )
		{
			// already processed
			continue;
		}
		visitedModels.Insert( szModelName );

		// resolve to full source path
		const char *ptr = szModelName;
		if ( !strnicmp( ptr, ".\\", 2 ) )
			ptr += 2;
		else if ( !strnicmp( ptr, szSourcePath, strlen( szSourcePath ) ) )
			ptr += strlen( szSourcePath );
		char szCleanName[MAX_PATH];
		strcpy( szCleanName, szSourcePath );
		strcat( szCleanName, ptr );
		char szFullSourcePath[MAX_PATH];
		_fullpath( szFullSourcePath, szCleanName, sizeof( szFullSourcePath ) );

		// any one dirty component generates the set of all expected files
		if ( ModelNeedsUpdate( szFullSourcePath ) )
		{
			int index = updateList.AddToTail();
			updateList[index].Set( szFullSourcePath );
		}
	}

	Msg( "\n" );
	Msg( "Model Pre Pass: Updating %d Models.\n", updateList.Count() );
	for ( int i = 0; i < updateList.Count(); i++ )
	{
		if ( !GenerateModelFiles( updateList[i].String() ) )
		{
			int error = g_errorList.AddToTail();
			g_errorList[error].result = false;
			g_errorList[error].fileName.Set( updateList[i].String() );
		}
	}

	// iterate error list
	if ( g_errorList.Count() )
	{
		Msg( "\n" );
		for ( int i = 0; i < g_errorList.Count(); i++ )
		{
			Msg( "ERROR: could not pre-process model: %s\n", g_errorList[i].fileName.String() );
		}
	}

	return true;
}
