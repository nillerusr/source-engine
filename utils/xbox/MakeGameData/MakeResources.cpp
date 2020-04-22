//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 file creation of miscellaneous resource and data files
//
//=====================================================================================//

#include "MakeGameData.h"
#include "captioncompiler.h"

//-----------------------------------------------------------------------------
// Purpose: Generate .360 compiled caption files
//-----------------------------------------------------------------------------
bool CreateTargetFile_CCDAT( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	CUtlBuffer targetBuffer;
	bool bOk = false;

	if ( !scriptlib->ReadFileToBuffer( pSourceName, targetBuffer ) )
	{
		return false;
	}

	if ( SwapClosecaptionFile( targetBuffer.Base() ) )
	{
		bOk = WriteBufferToFile( pTargetName, targetBuffer, bWriteToZip, g_WriteModeForConversions );
	}

	return bOk;
}

static bool ReslistLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

//-----------------------------------------------------------------------------
// Find or Add, prevents duplicates. Returns TRUE if found.
//-----------------------------------------------------------------------------
bool FindOrAddFileToResourceList( const char *pFilename, CUtlRBTree< CUtlString, int > *pTree, bool bAdd = true )
{
	char szOutName[MAX_PATH];
	char *pOutName;
	V_strncpy( szOutName, pFilename, sizeof( szOutName ) );
	V_FixSlashes( szOutName );
	V_RemoveDotSlashes( szOutName );
	V_strlower( szOutName );
	pOutName = szOutName;

	// strip any prefixed game name
	for ( int i = 0; g_GameNames[i] != NULL; i++ )
	{
		size_t len = strlen( g_GameNames[i] );
		if ( !V_strnicmp( pOutName, g_GameNames[i], len ) && pOutName[len] == '\\' )
		{
			// skip past game name and slash
			pOutName += len+1;
			break;
		}
	}

	if ( pTree->Find( pOutName ) != pTree->InvalidIndex() )
	{
		// found
		return true;
	}

	if ( bAdd )
	{
		pTree->Insert( pOutName );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Remove entry from dictionary, Returns TRUE if removed.
//-----------------------------------------------------------------------------
bool RemoveFileFromResourceList( const char *pFilename, CUtlRBTree< CUtlString, int > *pTree )
{
	char szOutName[MAX_PATH];
	char *pOutName;
	V_strncpy( szOutName, pFilename, sizeof( szOutName ) );
	V_FixSlashes( szOutName );
	V_RemoveDotSlashes( szOutName );
	V_strlower( szOutName );
	pOutName = szOutName;

	// strip any prefixed game name
	for ( int i = 0; g_GameNames[i] != NULL; i++ )
	{
		size_t len = strlen( g_GameNames[i] );
		if ( !V_strnicmp( pOutName, g_GameNames[i], len ) && pOutName[len] == '\\' )
		{
			// skip past game name and slash
			pOutName += len+1;
			break;
		}
	}

	if ( pTree->Find( pOutName ) != pTree->InvalidIndex() )
	{
		pTree->Remove( pOutName );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Generate a tree containing files from a reslist.  Returns TRUE if successful.
//-----------------------------------------------------------------------------
bool LoadReslist( const char *pReslistName, CUtlRBTree< CUtlString, int > *pTree )
{
	CUtlBuffer buffer;
	if ( !scriptlib->ReadFileToBuffer( pReslistName, buffer, true ) )
	{
		return false;
	}

	char szBasename[MAX_PATH];
	V_FileBase( pReslistName, szBasename, sizeof( szBasename ) );

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// parse reslist
	char szToken[MAX_PATH];
	char szBspName[MAX_PATH];
	szBspName[0] = '\0';
	for ( ;; )
	{
		int nTokenSize = buffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		// reslists are pc built, filenames can be sloppy
		V_strlower( szToken );
		V_FixSlashes( szToken );
		V_RemoveDotSlashes( szToken );

		// can safely cull filetypes that are ignored by queued loader at runtime
		bool bKeep = false;
		const char *pExt = V_GetFileExtension( szToken );
		if ( !pExt )
		{
			// unknown
			continue;
		}
		else if ( !V_stricmp( pExt, "vmt" ) || 
				!V_stricmp( pExt, "vhv" ) || 
				!V_stricmp( pExt, "mdl" ) || 
				!V_stricmp( pExt, "raw" ) || 
				!V_stricmp( pExt, "wav" ) )
		{
			bKeep = true;
		}
		else if ( !V_stricmp( pExt, "mp3" ) )
		{
			// change to .wav
			V_SetExtension( szToken, ".wav", sizeof( szToken ) );
			bKeep = true;
		}
		else if ( !V_stricmp( pExt, "bsp" ) )
		{
			// reslists erroneously have multiple bsps
			if ( !V_stristr( szToken, szBasename ) )
			{
				// wrong one, cull it
				continue;
			}
			else
			{
				// right one, save it
				strcpy( szBspName, szToken );
				bKeep = true;
			}
		}

		if ( bKeep )
		{
			FindOrAddFileToResourceList( szToken, pTree );
		}
	}

	if ( !szBspName[0] )
	{
		// reslist is not bsp derived, nothing more to do
		return true;
	}

	CUtlVector< CUtlString > bspList;
	bool bOK = GetDependants_BSP( szBspName, &bspList );
	if ( !bOK )
	{
		return false;
	}
	
	// add all the bsp dependants to the resource list
	for ( int i=0; i<bspList.Count(); i++ )
	{
		FindOrAddFileToResourceList( bspList[i].String(), pTree );
	}

	// iterate all the models in the resource list, get all their dependents
	CUtlVector< CUtlString > modelList;
	for ( int i = pTree->FirstInorder(); i != pTree->InvalidIndex(); i = pTree->NextInorder( i ) )
	{
		const char *pExt = V_GetFileExtension( pTree->Element( i ).String() );
		if ( !pExt || V_stricmp( pExt, "mdl" ) )
		{
			continue;
		}

		if ( !GetDependants_MDL( pTree->Element( i ).String(), &modelList ) )
		{
			return false;
		}
	}

	// add all the model dependents to the resource list
	for ( int i=0; i<modelList.Count(); i++ )
	{
		FindOrAddFileToResourceList( modelList[i].String(), pTree );
	}

	// check for optional commentary, include wav dependencies
	char szCommentaryName[MAX_PATH];
	V_ComposeFileName( g_szGamePath, szBspName, szCommentaryName, sizeof( szCommentaryName ) );
	V_StripExtension( szCommentaryName, szCommentaryName, sizeof( szCommentaryName ) );
	V_strncat( szCommentaryName, "_commentary.txt", sizeof( szCommentaryName ) );
	CUtlBuffer commentaryBuffer;
	if ( ReadFileToBuffer( szCommentaryName, commentaryBuffer, true, true ) )
	{
		// any single token may be quite large to due to text
		char szCommentaryToken[8192];
		for ( ;; )
		{
			int nTokenSize = commentaryBuffer.ParseToken( &breakSet, szCommentaryToken, sizeof( szCommentaryToken ) );
			if ( nTokenSize < 0 )
			{
				break;
			}
			if ( nTokenSize > 0 && !V_stricmp( szCommentaryToken, "commentaryfile" ) )
			{
				// get the commentary file
				nTokenSize = commentaryBuffer.ParseToken( &breakSet, szCommentaryToken, sizeof( szCommentaryToken ) );
				if ( nTokenSize > 0 )
				{
					// skip past sound chars
					char *pName = szCommentaryToken;
					while ( *pName && IsSoundChar( *pName ) )
					{
						pName++;
					}
					char szWavFile[MAX_PATH];
					V_snprintf( szWavFile, sizeof( szWavFile ), "sound/%s", pName );
					FindOrAddFileToResourceList( szWavFile, pTree );
				}
			}
		}
	}

	// check for optional blacklist
	char szBlacklist[MAX_PATH];
	V_ComposeFileName( g_szGamePath, "reslistfixes_xbox.xsc", szBlacklist, sizeof( szBlacklist ) );
	CUtlBuffer blacklistBuffer;
	if ( ReadFileToBuffer( szBlacklist, blacklistBuffer, true, true ) )
	{
		for ( ;; )
		{
			int nTokenSize = blacklistBuffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
			if ( nTokenSize <= 0 )
			{
				break;
			}

			bool bAdd;
			if ( !V_stricmp( szToken, "-" ) )
			{
				bAdd = false;
			}
			else if ( !V_stricmp( szToken, "+" ) )
			{
				bAdd = true;
			}
			else
			{
				// bad syntax, skip line
				Msg( "Bad Syntax, expecting '+' or '-' as first token in reslist fixup file '%s'.\n", szBlacklist );
				continue;
			}

			// get entry
			nTokenSize = blacklistBuffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
			if ( nTokenSize <= 0 )
			{
				break;
			}

			if ( bAdd )	
			{
				FindOrAddFileToResourceList( szToken, pTree );
			}
			else
			{
				RemoveFileFromResourceList( szToken, pTree );
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Generate .360 compiled reslist files
// Reslist files are processed for the unique consumption of Queued Loading.
//-----------------------------------------------------------------------------
bool CreateTargetFile_RESLST( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	bool bOK = false;

	// parse reslist
	CUtlRBTree< CUtlString, int > rbTree( 0, 0, ReslistLessFunc );
	if ( !LoadReslist( pSourceName, &rbTree ) )
	{
		return false;
	}

	CUtlBuffer targetBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( int iIndex = rbTree.FirstInorder(); iIndex != rbTree.InvalidIndex(); iIndex = rbTree.NextInorder( iIndex ) )
	{
		targetBuffer.PutChar( '\"' );
		targetBuffer.PutString( rbTree[iIndex].String() );
		targetBuffer.PutChar( '\"' );
		targetBuffer.PutString( "\n" );
	}

	bOK = WriteBufferToFile( pTargetName, targetBuffer, bWriteToZip, g_WriteModeForConversions );

	return bOK;
}