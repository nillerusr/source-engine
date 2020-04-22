//========= Copyright 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"
#include "tier1/utldict.h"
#include "tier1/keyvalues.h"
#include "baseprojectdatacollector.h"

#ifndef STEAM
 bool V_StrSubstInPlace( char *pchInOut, int cchInOut, const char *pMatch, const char *pReplaceWith, bool bCaseSensitive )
{
	bool bRet = false;
	char *pchT = (char *)malloc( cchInOut );
	if ( V_StrSubst( pchInOut, pMatch, pReplaceWith, pchT, cchInOut, bCaseSensitive ) )
	{
		V_strncpy( pchInOut, pchT, cchInOut );
		bRet = true;
	}
	free( pchT );
	return bRet;
}
#endif

void VPC_ParseFileSection( bool *pbHadConfigSection = NULL )
{
	if ( pbHadConfigSection )
		*pbHadConfigSection = false;

	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "}" ) )
		{
			// end of section
			break;
		}
		if ( !V_stricmp( pToken, "$configuration" ) )
		{
			VPC_Keyword_FileConfiguration( );
			if ( pbHadConfigSection )
				*pbHadConfigSection = true;
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_TrackSchemaFile
//
//-----------------------------------------------------------------------------

void VPC_TrackSchemaFile( const char *pName, bool bRemove, const char *pFileFlag )
{
#ifdef STEAM
	return;
#else
	if ( !bRemove && (!pFileFlag || !V_stristr( pFileFlag, "schema" )) )
	{
		// adding something that's not schema - ignore
		return;
	}

	for ( int i = 0; i < g_pVPC->m_SchemaFiles.Count(); i++ )
	{
		if ( !g_pVPC->m_SchemaFiles[i].String() )
			continue;

		if ( !V_stricmp( pName, g_pVPC->m_SchemaFiles[i].String() ) )
		{
			if ( bRemove )
			{
				g_pVPC->m_SchemaFiles.Remove( i );
			}
			return;
		}
	}

	if ( bRemove )
	{
		// not found, nothing to do
		return;	
	}
	
	g_pVPC->m_SchemaFiles.AddToTail( pName );

	// suppress building of schematized cpp files
	// (they get #included by an auto-generated cpp that is built instead)
	const char *pExt = V_GetFileExtension(pName);
	if ( pExt && !V_stricmp( pExt, "cpp" ) )
	{
		CUtlVector< CUtlString > configurationNames;
 		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
			g_pVPC->GetProjectGenerator()->FileIsSchema( true );
			g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
	}

#endif
}

static char *ResolveCandidate( const char *pszFile, const char *pszPlatform )
{
	char szPath[MAX_PATH];
	char szPathExpanded[MAX_PATH];

	V_ComposeFileName( g_pVPC->GetProjectPath(), pszFile, szPath, sizeof(szPath) );
	Sys_ReplaceString( szPath, "$os", pszPlatform, szPathExpanded, sizeof(szPathExpanded) );
	V_FixSlashes( szPathExpanded );
	V_RemoveDotSlashes( szPathExpanded );
	V_FixDoubleSlashes( szPathExpanded );

	if ( Sys_Exists( szPathExpanded ) )
	{
		char *pszResolvedFilename = (char*)malloc( MAX_PATH );
		Sys_ReplaceString( pszFile, "$os", pszPlatform, pszResolvedFilename, MAX_PATH );
		return pszResolvedFilename;
	}

	return NULL;
}

// No particular order, but must not change without changing the arrPlatformChains matrix
const char *g_szArrPlatforms[] = 
{
	"win32",	//  0
	"win64",	//  1
	"osx32",	//  2
	"osx64",	//  3
	"linux32",	//  4
	"linux64",	//  5
	"cygwin",	//  6
	"ps3",		//  7
	"x360",		//  8
	"win",		//  9
	"osx",		// 10
	"linux",	// 11
	"posix",	// 12
	"any",		// 13
	NULL
};

//-----------------------------------------------------------------------------
//	ResolveFilename
//		Utility to expand $OS if present
//		Returns expanded filename to a file that exists on disk, or NULL otherwise
//		vecBonusFiles will contain a list of valid OS files that are on disk but did not
//		match the current target platform.
//-----------------------------------------------------------------------------
static char *ResolveFilename( const char *pszFile, CUtlVector<CUtlString> &vecBonusFiles )
{
	static const int k_lastRealPlatform = 8; // index, not count
	static const int k_AnyPlatform = 13; // index

	static const int arrPlatformChains[][9] =
	{
		{  0,  1,  2,  3,  4,  5,  6,  7,  8 }, // the raw platforms
		{  9,  9, 10, 10, 11, 11, 12, 12,  9 }, // first fallback
		{ 13, 13, 12, 12, 12, 12, 13, 13, 13 }, // 2nd fallback
		{ 13, 13, 13, 13, 13, 13, 13, 13, 13 }, // 3rd fallback
	};

	// Don't want the actual backing store to be const since we aren't compile-time init'ing,
	// but it is single-instance initialized, so make a const ref to it that everyone except
	// the init codes uses to make sure it doesn't get mucked with.
	static bool bInited = false;
	static char _szPlatformStore[128];
	static const char (&szPlatform)[128] = _szPlatformStore;
	
	if ( !bInited )
	{
		bInited = true;
		const char *pszPlatform = g_pVPC->GetTargetPlatformName();
		V_strncpy( _szPlatformStore, pszPlatform, sizeof( szPlatform ) );
		V_strlower( _szPlatformStore );
	}

	vecBonusFiles.RemoveAll();

	int nPlatformColumn = -1;
	for ( int i = 0; i <= k_lastRealPlatform; i++ )
	{
		if ( V_strcmp( szPlatform, g_szArrPlatforms[i] ) == 0 )
		{
			nPlatformColumn = i;
			break;
		}
	}
	
	if ( nPlatformColumn < 0 )
	{
		g_pVPC->VPCWarning( "Internal Error: Target Platform: '%s' unrecognized while expanding $os!", szPlatform );
		return NULL;
	}

	// Now walk the chain of potential platform matches from strongest to weakest
	char *pszResolved = NULL;
	int nPlatformToCheck = -1;
	int nCurrentPlatformRow = -1;
	do 
	{
		nCurrentPlatformRow++;
		nPlatformToCheck = arrPlatformChains[nCurrentPlatformRow][nPlatformColumn];
		pszResolved = ResolveCandidate( pszFile, g_szArrPlatforms[ nPlatformToCheck ] );
		if ( pszResolved )
			break;
	}
	while ( arrPlatformChains[nCurrentPlatformRow][nPlatformColumn] != k_AnyPlatform );

	// Now go pickup the any files that exist, but were non-matches
	for ( int i = 0; g_szArrPlatforms[i] != NULL; i++ )
	{
		// Don't pick up the actual found platform
		if ( i != nPlatformToCheck )
		{
			CUtlString file( ResolveCandidate( pszFile, g_szArrPlatforms[ i ] ) );
			if ( !file.IsEmpty() )
				vecBonusFiles.AddToTail( file );
		}
	}
	return pszResolved;
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_AddFile
//
//-----------------------------------------------------------------------------
void VPC_Keyword_AddFilesByPattern()
{
	CUtlVector<CUtlString>			files;

	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( false );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			if ( files.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $FilePattern without any pattern preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				// we did all that work for no reason, time to bail out
				return;
			}
		}

		char szFilename[MAX_PATH];
		g_pVPC->ResolveMacrosInString( pToken, szFilename, sizeof( szFilename ) );

		V_FixSlashes( szFilename );

		CUtlVector< CUtlString > vecResults;
		Sys_ExpandFilePattern( szFilename, vecResults );

		for ( int i=0; i < vecResults.Count(); i++ )
		{
			g_pVPC->VPCStatus( false, "glob: adding '%s' to project", vecResults[i].String() );
			g_pVPC->GetProjectGenerator()->StartFile( vecResults[i].String(), true ); 
			g_pVPC->GetProjectGenerator()->EndFile();
		}
	}

}

// forward decl we need in the add file code
void VPC_Keyword_Folder( bool bUnity, const folderConfig_t *pInheritedFolderConfig = NULL );

//-----------------------------------------------------------------------------
//	VPC_Keyword_AddFile
//
//-----------------------------------------------------------------------------
void VPC_Keyword_AddFile( const char *pFileFlag = NULL, const folderConfig_t *pFolderConfig = NULL )
{
	bool							bAllowNextLine = false;
	CUtlVector<CUtlString>			files;
	CUtlVector<CUtlString> unbuiltFiles;

	bool bHasConditional = false;
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			if ( files.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $File without any file preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				// DO NOT INTEGRATE OR TAKE THIS TO STEAM
				// Steam VPC differs in conditional handling inside grouped files
				unbuiltFiles.AddToTail( files[files.Count() - 1] );
				files.Remove( files.Count() - 1 );
				//unbuiltFiles.AddMultipleToTail( files.Count(), files.Base() );
				//files.RemoveAll();
			}

			bHasConditional = true;
			continue;
		}

		char szFilename[MAX_PATH];
		g_pVPC->ResolveMacrosInString( pToken, szFilename, sizeof( szFilename ) );

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		V_FixSlashes( szFilename );

		CUtlString string = szFilename;
		files.AddToTail( string );

		// check for another optional file
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;
	}

	// check for optional section
	bool bHasSection = false;
	const char *pToken = g_pVPC->GetScript().PeekNextToken( true );
	if ( pToken && pToken[0] && !V_stricmp( pToken, "{" ) )
	{
		bHasSection = true;
	}

	// dynamic files need to opt out of strict file presence check
	bool bDynamicFile = pFileFlag && V_stristr( pFileFlag, "dynamic" );

	// need to check files early to handle possible rejected section
	if ( g_pVPC->IsCheckFiles() && !bDynamicFile )
	{
		for ( int i=0; i<files.Count(); i++ )
		{
			const char *pFilename = files[i].String();
			if ( !Sys_Exists( pFilename ) && !V_stristr( pFilename, "$os" ) )
			{
#if defined( POSIX )
				// We have a _lot_ of vpc files that contain header files with the incorrect casing.
				// So try to lowercase the filename here and if it exists, replace it and carry on.
				//	
				CUtlString FileNameLower = files[ i ];

				V_strlower( FileNameLower.Get() );
				if( Sys_Exists( FileNameLower ) )
				{
					files[ i ] = FileNameLower;
					continue;
				}
#endif

				// Hack around all the libraries which are listed as $File instead of $Lib
				const char* extension = strrchr( pFilename, '.' );
				if ( !extension || stricmp( extension, ".lib" ) )
				{
					g_pVPC->VPCWarning( "File '%s' does not exist. Not adding to project.", pFilename );
					g_pVPC->IncrementFileMissing();
					files.Remove( i );
				}
				else
				{
					// don't complain about libs that contain $(Configuration) in their path
					if ( !V_stristr( pFilename, "$(Configuration)" ) )
						g_pVPC->VPCWarning( "Library '%s' does not exist. Adding to project anyway.", pFilename );
				}
			}
		}
	}

	if ( g_pVPC->IsShowCaseIssues() && !bDynamicFile )
	{
		for ( int i = 0; i < files.Count(); i++ )
		{
			const char *pFilename = files[i].String();
			char actualFilename[MAX_PATH];
			if ( !Sys_IsFilenameCaseConsistent( pFilename, actualFilename, sizeof( actualFilename ) ) )
			{
				g_pVPC->VPCWarning( "Case Consistency Issue! File '%s' specified in '%s' is inconsistent with OS version '%s'.", pFilename, g_pVPC->GetProjectName(), actualFilename );

				// need script stack to assist in tracking down missing file
				g_pVPC->GetScript().SpewScriptStack();
			}
		}
	}

	if ( !files.Count() && bHasSection )
	{
		// optional section has been conditionally removed
		g_pVPC->GetScript().SkipBracedSection();
		return;
	}

	for ( int i=0; i < unbuiltFiles.Count(); i++ )
	{
		const char *pExcludedFilename = unbuiltFiles[i].String();

		g_pVPC->GetProjectGenerator()->StartFile( pExcludedFilename, true );
		CUtlVector< CUtlString > configurationNames;
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
			g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
		g_pVPC->GetProjectGenerator()->EndFile();
	}

	if ( bHasSection )
	{
		// found optional section, parse opening brace
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
			g_pVPC->VPCSyntaxError();
	}

	// Handle $OS expansion
	// save parser state
	CScriptSource startingScriptSource = g_pVPC->GetScript().GetCurrentScript();

	for ( int i=0; i<files.Count(); i++ )
	{
		const char *pFilename = files[i].String();

		CUtlString filename;
		if ( !g_pVPC->m_bInMkSlnPass && V_stristr( pFilename, "$os" ) )
		{
			CUtlVector<	CUtlString > vecExcludedFiles;
			filename = ResolveFilename( pFilename, vecExcludedFiles );
			char rgchRejectList[4096]; rgchRejectList[0]='\0';
			if ( vecExcludedFiles.Count() )
			{
				for ( int i=0; i<files.Count(); i++ )
				{
							V_strncat( rgchRejectList, files[i].String(), V_ARRAYSIZE( rgchRejectList ) );
							V_strncat( rgchRejectList, ",", V_ARRAYSIZE( rgchRejectList ) );
				}
			}
			g_pVPC->VPCStatus( false, "$OS: Resolved %s -> %s, rejected %s", pFilename, filename.String(), rgchRejectList );

			if ( filename.IsEmpty() )
			{
				g_pVPC->VPCWarning( "$File %s did not resolve to an existing file, skipping!", pFilename );
				continue;
			}

			for ( i=0; i < vecExcludedFiles.Count(); i++ )
			{
				const char *pExcludedFilename = vecExcludedFiles[i].String();
				const char *pExcludedExtension = V_GetFileExtension( pExcludedFilename );
				if ( !pExcludedExtension )
				{
					pExcludedExtension = "";
				}
				if ( pExcludedExtension && !V_stricmp( pExcludedExtension, "cpp" ) )
				{
					g_pVPC->VPCStatus( false, "excluding '%s' from build", pExcludedFilename );
					g_pVPC->GetProjectGenerator()->StartFile( pExcludedFilename, true ); 
					CUtlVector< CUtlString > configurationNames;
 					g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
					for ( int i=0; i < configurationNames.Count(); i++ )
					{
						g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
						g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
						g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
					}
					g_pVPC->GetProjectGenerator()->EndFile();
				}
			}

			pFilename = filename.String();
		}

		bool bAdded = g_pVPC->GetProjectGenerator()->StartFile( pFilename, true ); 
		
		if ( bDynamicFile )
		{
			CUtlVector< CUtlString > configurationNames;
			g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
			for ( int i=0; i < configurationNames.Count(); i++ )
			{
				g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
				g_pVPC->GetProjectGenerator()->FileIsDynamic( true );
				g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
			}
		}

		// Lookup extension for a custom build script
		const char *pExtension = V_GetFileExtension( pFilename );
		if ( !pExtension )
		{
			pExtension = "";
		}
		
		if ( !g_pVPC->m_sUnityCurrent.IsEmpty() && !V_stricmp( pExtension, "cpp" ) && !bHasSection && !bHasConditional ) 
		{
			bool bEmitUnityFiles = ( !g_pVPC->m_bInMkSlnPass && ( g_pVPC->IsForceGenerate() || !g_pVPC->IsProjectCurrent( g_pVPC->GetOutputFilename(), false ) ) );
			if ( bEmitUnityFiles )
			{
				// append to the unity file
				g_pVPC->VPCStatus( false, "Unity: adding '%s' to unity file '%s'", pFilename, g_pVPC->m_sUnityCurrent.String() );
				FILE *fp = fopen( g_pVPC->m_sUnityCurrent.String(), "at" );
						if ( !fp )
						{
							g_pVPC->VPCError( "Cannot open %s for appending", g_pVPC->m_sUnityCurrent.String() );
						}
						else
						{
				char pFixedFilename[MAX_PATH];
				V_strncpy( pFixedFilename, pFilename, sizeof(pFixedFilename) );
				V_FixSlashes( pFixedFilename, '/' );
				fprintf( fp, "#include \"%s\"\n", pFixedFilename );
				fclose( fp );
			}
					}

			g_pVPC->VPCStatus( false, "Unity: excluding '%s' from build", pFilename );
			CUtlVector< CUtlString > configurationNames;
 			g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
			for ( int i=0; i < configurationNames.Count(); i++ )
			{
				g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
				g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
				g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
			}
		}
		else 
		{
			bool bHadConfigSection = false;

			int index = g_pVPC->m_CustomBuildSteps.Find( pExtension );
			if ( g_pVPC->m_CustomBuildSteps.IsValidIndex( index ) )
			{
				CUtlString buildsteps = g_pVPC->m_CustomBuildSteps[index];
				const char *pBuffer = buildsteps.Get();

				CUtlString scriptName;
				scriptName = pExtension;
				scriptName += " custom build step";

				// save parser state
				g_pVPC->GetScript().PushScript( g_pVPC->GetScript().GetName(), pBuffer );

				// parse injected buildstep
				VPC_ParseFileSection( &bHadConfigSection );

				// restore parser state
				g_pVPC->GetScript().PopScript();
				g_pVPC->GetScript().RestoreScript( startingScriptSource );
			}

			// apply optional section to each file
			if ( bHasSection && bAdded )
			{
				// restore parser state
				g_pVPC->GetScript().RestoreScript( startingScriptSource );
				VPC_ParseFileSection( &bHadConfigSection );
			}

			// if this file doesn't have a file-specific config, but the folder we're in has a config
			//	apply that folder config to the file here
			if ( !bHadConfigSection && pFolderConfig )
				VPC_ApplyFolderConfigurationToFile( *pFolderConfig );

		}

		VPC_TrackSchemaFile( pFilename, false, pFileFlag );

		if ( bAdded )
			g_pVPC->GetProjectGenerator()->EndFile();
	}
}

// parse a list of filenames from the current token, handling conditionals, macro expansion, \\, fixslashes, etc
static void VPC_ParseFileList( CUtlStringList &files )
{
	bool bAllowNextLine = false;

	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
		if ( !pNextToken || !pNextToken[0] )
		{
			// current token is last token
			// last token can be optional conditional, need to identify
			// backup and reparse up to last token
			if ( pToken && pToken[0] == '[' )
			{
				if ( files.Count() == 0 )
				{
					g_pVPC->VPCSyntaxError( "Conditional specified on a file list without any file preceding it." );
				}
				// last token is an optional conditional
				bool bResult = g_pVPC->EvaluateConditionalExpression( pToken );
				if ( !bResult )							// was conditional false?
				{
					files.PurgeAndDeleteElements();
				}
				return;
			}
		}

		char szFilename[MAX_PATH];
		g_pVPC->ResolveMacrosInString( pToken, szFilename, sizeof( szFilename ) );
		V_FixSlashes( szFilename );

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		files.CopyAndAddToTail( szFilename );

		// check for another optional file
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;
	}
}

// add or remove .lib or dll import files, automatically adding prefixes and suffices to the name
static void VPC_HandleLibraryExpansion( char const *pDefaultPath, char const *pFileNamePrefix, char const *pSuffix, bool bRemove )
{
	if ( !pFileNamePrefix )
	{
		pFileNamePrefix = "";
	}
	char szResolvedFilePrefix[MAX_PATH];
	g_pVPC->ResolveMacrosInString( pFileNamePrefix, szResolvedFilePrefix, sizeof(szResolvedFilePrefix) );
	
	CUtlStringList impFiles;
	VPC_ParseFileList( impFiles );
	for( int i = 0; i < impFiles.Count(); i++ )
	{
		char szFilename[MAX_PATH];
		char const *pPathPrefixToUse = pDefaultPath;

		// do not add the path prefix if the filename contains path information already, or if null was passed.
		char impFile[MAX_PATH];
		V_strncpy( impFile, impFiles[i], sizeof( impFile ) );
		V_RemoveDotSlashes( impFile );
		
		char *pLastSlash = (char*)MAX( strrchr( impFile, '\\' ), strrchr( impFile, '/' ) );
		if ( pLastSlash )
		{
			*pLastSlash = 0;
			const char *pFilenamePart = pLastSlash + 1;
			// don't prepend the prefix if the filename already starts with it
			bool bAddPrefix = true;
			if ( V_strlen(szResolvedFilePrefix) && !V_strnicmp( pFilenamePart, szResolvedFilePrefix, V_strlen(szResolvedFilePrefix) ) )
				bAddPrefix = false;
			sprintf( szFilename, "%s/%s%s%s", impFile, bAddPrefix ? pFileNamePrefix : "", pFilenamePart, pSuffix );
		}
		else
		{
			bool bAddPrefix = true;
			if ( V_strlen(szResolvedFilePrefix) && !V_strnicmp( impFiles[i], szResolvedFilePrefix, V_strlen(szResolvedFilePrefix) ) )
				bAddPrefix = false;
			sprintf( szFilename, "%s%s%s%s", pPathPrefixToUse, bAddPrefix ? pFileNamePrefix : "", impFiles[i], pSuffix );
		}

		char szFilename1[MAX_PATH];
		g_pVPC->ResolveMacrosInString( szFilename, szFilename1, sizeof( szFilename1 ) );

		// Replace forward slashes with backslashes regardless of target platform
		V_FixSlashes( szFilename1 );
		V_RemoveDotSlashes( szFilename1 );

		if ( bRemove )
		{
			bool bSucc = g_pVPC->GetProjectGenerator()->RemoveFile( szFilename1 );
			if ( !bSucc )
			{
				g_pVPC->VPCError( "Broken $implib command. Failed to remove file %s from project.", szFilename1 );
			}
		}
		else
		{
			bool bAdded = g_pVPC->GetProjectGenerator()->StartFile( szFilename1, true );
			if ( !bAdded )
			{
				g_pVPC->VPCError( "couldn't add %s", szFilename1 );
			}
			g_pVPC->GetProjectGenerator()->EndFile();
		}
	}
}

enum EVPCKeywordFlag
{
	k_eVPCKeywordFlag_Remove = 0x00000001,
	k_eVPCKeywordFlag_External = 0x00000002,
};

static void VPC_Keyword_ImportLibrary( uint32 Flags )
{
	char const *pSuffix = "$_IMPLIB_EXT";
	bool bRemove = !!(Flags & k_eVPCKeywordFlag_Remove );

	if( ( Flags & k_eVPCKeywordFlag_External ) &&
		g_pVPC->FindOrCreateMacro( "_EXTERNAL_IMPLIB_EXT", false, NULL ) )
	{
		pSuffix = "$_EXTERNAL_IMPLIB_EXT";
	}

	VPC_HandleLibraryExpansion( "$LIBPUBLIC\\", "$_IMPLIB_PREFIX", pSuffix, bRemove );
}

static void VPC_Keyword_LinkerLibrary( uint32 Flags )
{
	char const *pSuffix = "$_STATICLIB_EXT";
	bool bRemove = !!(Flags & k_eVPCKeywordFlag_Remove );

	if( ( Flags & k_eVPCKeywordFlag_External ) &&
		g_pVPC->FindOrCreateMacro( "_EXTERNAL_STATICLIB_EXT", false, NULL ) )
	{
		pSuffix = "$_EXTERNAL_STATICLIB_EXT";
	}

	VPC_HandleLibraryExpansion( "$LIBPUBLIC\\", "$_STATICLIB_PREFIX", pSuffix, bRemove );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_RemoveFile
//
//-----------------------------------------------------------------------------
void VPC_Keyword_RemoveFile()
{
	CUtlStringList filesToRemove;
	VPC_ParseFileList( filesToRemove );
	for ( int i = 0; i < filesToRemove.Count(); i++ )
	{
		bool bSucc = g_pVPC->GetProjectGenerator()->RemoveFile( filesToRemove[i] );
		if ( !bSucc )
		{
			g_pVPC->VPCWarning( "Failed to remove file %s from project", filesToRemove[i] );
		}

		VPC_TrackSchemaFile( filesToRemove[i], true, NULL );
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Folder
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Folder( bool bUnity, const folderConfig_t *pInheritedFolderConfig /* = NULL */ )
{
	const char				*pToken;
	char				folderName[MAX_PATH];
	folderConfig_t			folderConfig;
	
	// by default, our active config is any config that we inherited from the parent.
	const folderConfig_t	*pActiveFolderConfig = pInheritedFolderConfig;

	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, folderName, sizeof( folderName ) ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

	g_pVPC->GetProjectGenerator()->StartFolder( folderName );

	bool bEmitUnityFiles = ( !g_pVPC->m_bInMkSlnPass && ( g_pVPC->IsForceGenerate() || !g_pVPC->IsProjectCurrent( g_pVPC->GetOutputFilename(), false ) ) );
	if ( bUnity )
	{
		// generate a .cpp file from the folderName
		char unityName[MAX_PATH];
				V_snprintf( unityName, sizeof(unityName), "%s_%s_unity.cpp", folderName, g_pVPC->GetProjectName() );
				V_StrSubstInPlace( unityName, sizeof(unityName), " ", "_", false );

		// make sure we have a unique file name, we don't want to tread on another projects unity
		g_pVPC->m_sUnityCurrent = unityName;
		int cAttempt = 1;
		while ( g_pVPC->m_UnityFilesSeen.Find( g_pVPC->m_sUnityCurrent ) != g_pVPC->m_UnityFilesSeen.InvalidIndex() )
		{
					V_snprintf( unityName, sizeof(unityName), "%s_%s_unity_%d.cpp", folderName, g_pVPC->GetProjectName(), ++cAttempt );
					V_StrSubstInPlace( unityName, sizeof(unityName), " ", "_", false );
			g_pVPC->m_sUnityCurrent = unityName;
		}

		g_pVPC->m_UnityStack.Push( g_pVPC->m_sUnityCurrent );

		if ( bEmitUnityFiles )
		{
			g_pVPC->VPCStatus( false, "Unity: emitting '%s' in project: '%s'", unityName, g_pVPC->GetProjectName() );

			// always add the stdafx.h at the top (if we're using a precompiled header, what if we're not?)
			FILE *fp = fopen( g_pVPC->m_sUnityCurrent.String(), "wt" );
					if ( !fp )
					{
						g_pVPC->VPCError( "Cannot open %s for writing", g_pVPC->m_sUnityCurrent.String() );
					}
					else
					{
			const char *pStdAfx = g_pVPC->GetMacroValue( "STDAFX" );
			if ( pStdAfx && *pStdAfx )
			{
				fprintf( fp, "#include \"%s\"\n", pStdAfx );
			}
			fclose( fp );
		}
				}

		g_pVPC->m_UnityFilesSeen.Insert( g_pVPC->m_sUnityCurrent );

		// Msg( "pushing unity file %s\n", g_sUnityCurrent );
		g_pVPC->GetProjectGenerator()->StartFile( g_pVPC->m_sUnityCurrent, true );
		g_pVPC->GetProjectGenerator()->EndFile();	
	}

	// Now parse all the files and subfolders..
	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "}" ) )
		{
			// pop
			break;
		}
		else if ( !V_stricmp( pToken, "$file" ) || !V_stricmp( pToken, "$schemafile" ) || !V_stricmp( pToken, "$entschemafile" ) )
		{
			// add file
			VPC_Keyword_AddFile( NULL, pActiveFolderConfig );
		}
		else if ( !V_stricmp( pToken, "$DynamicFile" ) )
		{
			// add file
			VPC_Keyword_AddFile( "dynamic", pActiveFolderConfig );
		}
		else if ( !V_stricmp( pToken, "$FilePattern" ) )
		{
			// glob the given pattern, add all files
			VPC_Keyword_AddFilesByPattern();
		}
// THIS IS A LEGACY INCORRECT IMPLEMENTATION
//		else if ( !V_stricmp( pToken, "$schemafile" ) )
//		{
//			// add file
//			VPC_Keyword_AddFile( "schema" );
//		}
		else if ( !V_stricmp( pToken, "$implib" ) ||
				  !V_stricmp( pToken, "$implibexternal" ) ||
				  !V_stricmp( pToken, "-$implib" ) ||
				  !V_stricmp( pToken, "-$implibexternal" ) )
		{
			uint32 KeywordFlags = ( pToken[ 0 ] == '-' ) ? k_eVPCKeywordFlag_Remove : 0;

			if( V_stristr( pToken, "external" ) )
				KeywordFlags |= k_eVPCKeywordFlag_External;

			VPC_Keyword_ImportLibrary( KeywordFlags );
		}
		else if ( !V_stricmp( pToken, "$lib" ) ||
				  !V_stricmp( pToken, "$libexternal" ) ||
				  !V_stricmp( pToken, "-$lib" ) ||
				  !V_stricmp( pToken, "-$libexternal" ) )
		{
			uint32 KeywordFlags = ( pToken[ 0 ] == '-' ) ? k_eVPCKeywordFlag_Remove : 0;

			if( V_stristr( pToken, "external" ) )
				KeywordFlags |= k_eVPCKeywordFlag_External;

			VPC_Keyword_LinkerLibrary( KeywordFlags );
		}
		else if ( !V_stricmp( pToken, "-$file" ) )
		{
			// remove file
			VPC_Keyword_RemoveFile();
		}
		else if ( !V_stricmp( pToken, "$folder" ) )
		{
			// descend into subdirectory
			VPC_Keyword_Folder( false, pActiveFolderConfig );
		}
		else if ( !V_stricmp( pToken, "$unity" ) )
		{
			VPC_Keyword_Folder( g_pVPC->IsUnity(), pActiveFolderConfig );
		}
		else if ( !V_stricmp( pToken, "$configuration" ) )
		{
			// read the folder-specific configuration.  
			VPC_Keyword_FolderConfiguration( &folderConfig );
			
			// if we found a new config section, make it active and apply it to subsequent files
			if ( folderConfig.BHasConfig() )
				pActiveFolderConfig = &folderConfig;
			else
				pActiveFolderConfig = NULL;
		}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}
	
	if ( bUnity )
	{
		// Msg( "popping unity file %s\n", g_sUnityCurrent );
		g_pVPC->m_UnityStack.Pop();

		if ( g_pVPC->m_UnityStack.Count() == 0 )
			g_pVPC->m_sUnityCurrent = NULL;
		else
			g_pVPC->m_sUnityCurrent = g_pVPC->m_UnityStack.Top();
	}
	g_pVPC->GetProjectGenerator()->EndFolder();
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Shaders
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Shaders( int depth )
{
	const char						*pToken;
	char						shadersName[MAX_PATH];
	CUtlBuffer					vpcBuffer;
	CUtlVector< CUtlString >	fxcList;
	CUtlVector< CUtlString >	vshList;
	CUtlVector< CUtlString >	pshList;
	CUtlVector< CUtlString >	vfxList;
	CUtlVector< CUtlString >	otherList;
	int							i;
	bool						bIgnoreRedundancyWarning;

	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, shadersName, sizeof( shadersName ) ) )
	{
		return;		
	}

	g_pVPC->VPCStatus( false, "Parsing: %s", shadersName );
	g_pVPC->GetScript().PushScript( shadersName );

	// parse the shader list file into types (fxc,vsh,psh)
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			// end of file
			break;
		}

		if ( V_stristr( pToken, ".fxc" ) )
		{
			fxcList.AddToTail( pToken );
		}
		else if ( V_stristr( pToken, ".vsh" ) )
		{
			vshList.AddToTail( pToken );
		}
		else if ( V_stristr( pToken, ".psh" ) )
		{
			pshList.AddToTail( pToken );
		}
		else if ( V_stristr( pToken, ".vfx" ) )
		{
			vfxList.AddToTail( pToken );
		}
		else
		{
			otherList.AddToTail( pToken );
		}
	}

	g_pVPC->GetScript().PopScript();

	if ( !fxcList.Count() && 
		!vshList.Count() && 
		!pshList.Count() &&
		!vfxList.Count() &&
		!otherList.Count() )
	{
		g_pVPC->VPCWarning( "No shaders found in %s", shadersName );
		return;
	}

	// generate a vpc compatible file to generate the shader file hierarchy
	vpcBuffer.SetBufferType( true, true );
	vpcBuffer.Printf( "$Folder \"Shader Source\" \n" );
	vpcBuffer.Printf( "{\n" );

	// add the shader file as a convienence
	vpcBuffer.Printf( "$file \"%s\"\n", shadersName );
	vpcBuffer.Printf( "{\n" );
	vpcBuffer.Printf( "$Configuration\n" );
	vpcBuffer.Printf( "{\n" );
	vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
	vpcBuffer.Printf( "}\n" );
	vpcBuffer.Printf( "}\n" );

	// fxc files
	if ( fxcList.Count() )
	{
		vpcBuffer.Printf( "$Folder \"fxc\" \n" );
		vpcBuffer.Printf( "{\n" );
		for ( i=0; i<fxcList.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", fxcList[i].String() );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$Configuration\n" );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
			vpcBuffer.Printf( "}\n" );
			vpcBuffer.Printf( "}\n" );
		}
		vpcBuffer.Printf( "}\n" );
	}

	// vsh files
	if ( vshList.Count() )
	{
		vpcBuffer.Printf( "$Folder \"vsh\" \n" );
		vpcBuffer.Printf( "{\n" );
		for ( i=0; i<vshList.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", vshList[i].String() );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$Configuration\n" );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
			vpcBuffer.Printf( "}\n" );
			vpcBuffer.Printf( "}\n" );
		}
		vpcBuffer.Printf( "}\n" );
	}

	// psh files
	if ( pshList.Count() )
	{
		vpcBuffer.Printf( "$Folder \"psh\" \n" );
		vpcBuffer.Printf( "{\n" );
		for ( i=0; i<pshList.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", pshList[i].String() );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$Configuration\n" );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
			vpcBuffer.Printf( "}\n" );
			vpcBuffer.Printf( "}\n" );
		}
		vpcBuffer.Printf( "}\n" );
	}

	// vfx files
	if ( vfxList.Count() )
	{
		vpcBuffer.Printf( "$Folder \"vfx\" \n" );
		vpcBuffer.Printf( "{\n" );
		for ( i=0; i<vfxList.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", vfxList[i].String() );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$Configuration\n" );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
			vpcBuffer.Printf( "}\n" );
			vpcBuffer.Printf( "}\n" );
		}
		vpcBuffer.Printf( "}\n" );
	}

	// other files
	if ( otherList.Count() )
	{
		// psh files
		vpcBuffer.Printf( "$Folder \"other\" \n" );
		vpcBuffer.Printf( "{\n" );
		for ( i=0; i<otherList.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", otherList[i].String() );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$Configuration\n" );
			vpcBuffer.Printf( "{\n" );
			vpcBuffer.Printf( "$ExcludedFromBuild \"Yes\"\n" );
			vpcBuffer.Printf( "}\n" );
			vpcBuffer.Printf( "}\n" );
		}
		vpcBuffer.Printf( "}\n" );
	}

	// end of shader folder
	vpcBuffer.Printf( "}\n" );

	// save parser
	bIgnoreRedundancyWarning = g_pVPC->IsIgnoreRedundancyWarning();
	g_pVPC->SetIgnoreRedundancyWarning( true );

	g_pVPC->GetScript().PushScript( "Internal List", (char*)vpcBuffer.Base() );

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( pToken && pToken[0] && !V_stricmp( pToken, "$folder" ) )
	{
		VPC_Keyword_Folder( false );
	}

	// restore parser
	g_pVPC->GetScript().PopScript();
	g_pVPC->SetIgnoreRedundancyWarning( bIgnoreRedundancyWarning );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Macro
//
//-----------------------------------------------------------------------------
enum MacroType_t { VPC_MACRO_VALUE, VPC_MACRO_EMPTY_STRING };
void VPC_Keyword_Macro( MacroType_t eMacroType )
{
	const char	*pToken;
	char	macro[MAX_SYSTOKENCHARS];
	char	value[MAX_SYSTOKENCHARS];

	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
		g_pVPC->VPCSyntaxError();
	strcpy( macro, pToken );
	
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, value, sizeof( value ) ) )
	{
		return;
	}

	char environmentValue[MAX_SYSTOKENCHARS];
	if ( Sys_EvaluateEnvironmentExpression( value, "", environmentValue, sizeof( environmentValue ) ) )
	{
		V_strncpy( value, environmentValue, sizeof( value ) );
	}
	
	g_pVPC->FindOrCreateMacro( macro, true, ( eMacroType == VPC_MACRO_VALUE ) ? value : "" );
}

//-----------------------------------------------------------------------------
//	$MacroRequired <Macro> [DefaultValue] [Condition]
//	$MacroRequiredAllowEmpty <Macro> [DefaultValue] [Condition]
//
//	Forces a script to error if a macro that it depends on was not set.
//	The Default will be used if the macro was not defined, otherwise error.
//	This is to allow a required macro in a base script to have a concept
//	of a default initialization value.
//-----------------------------------------------------------------------------
enum MacroRequiredType_t { VPC_MACRO_REQUIRED_NOT_EMPTY, VPC_MACRO_REQUIRED_ALLOW_EMPTY };
void VPC_Keyword_MacroRequired( MacroRequiredType_t eMacroRequiredType )
{
	char	macroName[MAX_SYSTOKENCHARS];
	char	macroDefaultValue[MAX_SYSTOKENCHARS];
	const char	*pToken;

	macroDefaultValue[0] = '\0';
	
	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
	{
		g_pVPC->VPCSyntaxError();
	}
	strcpy( macroName, pToken );

	// optional default macro value or conditional
	pToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( pToken && pToken[0] )
	{
		if ( pToken[0] == '[' )
		{
			// evaulate argument as conditional
			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				return;
			}
		}
		else
		{
			// argument is a default macro value
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, macroDefaultValue, sizeof( macroDefaultValue ) ) )
			{
				return;
			}
		}
	}

	// find macro, needs to be present and non-empty
	macro_t *pMacro = g_pVPC->FindOrCreateMacro( macroName, false, NULL );
	if ( !pMacro || ( eMacroRequiredType == VPC_MACRO_REQUIRED_NOT_EMPTY && !strlen( pMacro->value.String() ) ) )
	{
		if ( macroDefaultValue[0] || ( eMacroRequiredType == VPC_MACRO_REQUIRED_ALLOW_EMPTY ) )
		{
			g_pVPC->FindOrCreateMacro( macroName, true, macroDefaultValue );
		}
		else
		{
			// In case we're in mksln showing a pacifier of dots. Make sure to show the error on a new line.
			g_pVPC->VPCSyntaxError( "\n\nRequired Macro '%s', not defined or empty", macroName );
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacro
//
//	$LoadAddressMacro	<MacroName>
//	{
//		<ProjectName>	<BaseAddress>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacro( void )
{
	char	szProjectName[MAX_SYSTOKENCHARS];
	char	szMacroName[MAX_SYSTOKENCHARS];
	char	szBaseAddress[MAX_SYSTOKENCHARS];
	const char	*pToken;

	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, szMacroName, sizeof( szMacroName ) ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError();
	}
	
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}
		strcpy( szProjectName, pToken );

		if ( !V_stricmp( pToken, "}" ) )
		{
			break;
		}
		else
		{
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, szBaseAddress, sizeof( szBaseAddress ) ) )
			{
				continue;
			}
		
			if ( !V_stricmp( szProjectName, g_pVPC->GetLoadAddressName() ) )
			{
				// set Macro
				g_pVPC->FindOrCreateMacro( szMacroName, true, szBaseAddress );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAlias
//
//	$LoadAddressMacroAlias	<Alias>
//	{
//		<ProjectName>
//	}
//
//	When evaluating $LoadAddressMacro/$LoadAddressMacroAuto, substitute all listed <ProjectName> entries with <Alias>
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAlias( void )
{
	char	szProjectName[MAX_SYSTOKENCHARS];
	char	szAlias[MAX_SYSTOKENCHARS];
	const char	*pToken;

	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, szAlias, sizeof( szAlias ) ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError();
	}

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}
		strcpy( szProjectName, pToken );

		if ( !V_stricmp( pToken, "}" ) )
		{
			break;
		}
		else
		{
			if ( !V_stricmp( szProjectName, g_pVPC->GetProjectName() ) )
			{
				// set Macro and alias
				g_pVPC->FindOrCreateMacro( "LOADADDRESSNAME", true, szAlias );
				g_pVPC->SetLoadAddressName( szAlias );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Internal_LoadAddressMacroAuto
//
//	bPad - Differentiate between $LoadAddressMacroAuto and $LoadAddressMacroAuto_Padded
//			implementations
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void Internal_LoadAddressMacroAuto( bool bPad )
{
	char	szProjectName[MAX_SYSTOKENCHARS];
	char	szMacroName[MAX_SYSTOKENCHARS];
	char	szBaseAddress[MAX_SYSTOKENCHARS];
	char	szLength[MAX_SYSTOKENCHARS];
	const char	*pToken;

	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
	{
		g_pVPC->VPCSyntaxError();
	}
	strcpy( szMacroName, pToken );

	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, szBaseAddress, sizeof( szBaseAddress ) ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}
	unsigned int baseAddress = 0;
	sscanf( szBaseAddress, "%x", &baseAddress );
	unsigned int iInitialBaseAddress = baseAddress;

	macro_t *pMacro = NULL;
	int iSetEntryNum = 0;
	int iSetBaseAddress = 0;

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError();
	}
	
	int iEntryNum = 0;
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}
		strcpy( szProjectName, pToken );

		if ( !V_stricmp( szProjectName, g_pVPC->GetLoadAddressName() ) )
		{
			// set Macro
			char szMacroValue[MAX_SYSTOKENCHARS];
			sprintf( szMacroValue, "0x%8.8x", baseAddress );
			
			iSetEntryNum = iEntryNum;
			iSetBaseAddress = baseAddress;
			pMacro = g_pVPC->FindOrCreateMacro( szMacroName, true, szMacroValue );
		}

		if ( !V_stricmp( pToken, "}" ) )
		{
			break;
		}
		else
		{
			unsigned int dllLength = 0;
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, szLength, sizeof( szLength ) )	)
			{
				continue;
			}
			if ( strstr( szLength, "." ) )
			{
				// assume float format
				float fLength = 0;
				sscanf( szLength, "%f", &fLength );
				dllLength = fLength * 1024.0f * 1024.0f;
			}
			else
			{
				sscanf( szLength, "%d", &dllLength );
			}

			if ( !bPad )
			{
				dllLength = AlignValue( dllLength, 64 * 1024 ); //will align later when we actually set the darn thing
			}

			if ( dllLength == 0 )
			{
				g_pVPC->VPCSyntaxError( "$LoadAddressMacroAuto no longer supports 0 size dlls. Use $LoadAddressMacroAlias to have two orthogonal projects load in the same space" );
			}

			baseAddress += dllLength;
		}

		++iEntryNum;
	}

	if ( bPad && pMacro )
	{
		unsigned int iEndAddress;
		if ( (iInitialBaseAddress >= 0x82000000) && (iInitialBaseAddress < 0x8C000000) )
		{
			iEndAddress = 0x8BFFFFFF;
		}
		else
		{
			iEndAddress = 0x9BFFFFFF;
		}

		// compute leftover unused address space
		unsigned int iRemainingSpace = iEndAddress - baseAddress;

		int iPadPerEntry = iRemainingSpace / iEntryNum;
		//iPadPerEntry = iPadPerEntry & ~(64 * 1024); //align DOWN to 64k
		if ( iPadPerEntry > 0 )
		{
			// set the base address again with padding added
			iSetBaseAddress += iPadPerEntry * iSetEntryNum;
			iSetBaseAddress = AlignValue( iSetBaseAddress, 64 * 1024 );

			char szMacroValue[MAX_SYSTOKENCHARS];
			sprintf( szMacroValue, "0x%8.8x", iSetBaseAddress );
	
			pMacro->value = szMacroValue;
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAuto
//
//	$LoadAddressMacroAuto	<MacroName> <BaseAddress>
//	{
//		<ProjectName>	<Length>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAuto( void )
{
	Internal_LoadAddressMacroAuto( false );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAuto_Padded
//
//	$LoadAddressMacroAuto_Padded	<MacroName> <BaseAddress>
//	{
//		<ProjectName>	<Length>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name. Assumes the contained list is minimally packed and has free reign of 
//	space up to the limit. Finds unused space spreads it out evenly between
//	each project
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAuto_Padded( void )
{
	Internal_LoadAddressMacroAuto( true );
}

//-----------------------------------------------------------------------------
// VPC_SharedKeyword_Conditional
// $Conditional "CONDITIONNAME" "VALUE"
//
// Sets a conditional that can be used later in the script to affect later keywords.
// This works in both project and group scripts.
//-----------------------------------------------------------------------------
void VPC_SharedKeyword_Conditional()
{
	const char *pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
		g_pVPC->VPCSyntaxError();

	char name[MAX_SYSTOKENCHARS];
	if ( pToken[0] == '$' )
	{
		// being nice to users, quietly remove the unwanted conditional prefix '$'
		pToken++;
	}
	strcpy( name, pToken );
	
	char value[MAX_SYSTOKENCHARS];
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, value, sizeof( value ) ) )
	{
		return;
	}
	
	conditional_t *pConditional = g_pVPC->FindOrCreateConditional( name, true, CONDITIONAL_CUSTOM );
	if ( pConditional->type != CONDITIONAL_CUSTOM )
	{
		// scripts cannot change any platform or game conditionals
		g_pVPC->VPCSyntaxError( "$Conditional cannot be used on the reserved '$%s'", pConditional->upperCaseName.Get() );
	}

	char environmentValue[MAX_SYSTOKENCHARS];
	if ( Sys_EvaluateEnvironmentExpression( value, "0", environmentValue, sizeof( environmentValue ) ) )
	{
		V_strncpy( value, environmentValue, sizeof( value ) );
	}

	g_pVPC->SetConditional( name, Sys_StringToBool( value ) );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_IgnoreRedundancyWarning
//
//-----------------------------------------------------------------------------
void VPC_Keyword_IgnoreRedundancyWarning( void )
{
	char	value[MAX_SYSTOKENCHARS];
	
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, value, sizeof( value ) ) )
	{
		return;
	}

	bool bVal = Sys_StringToBool( value );
	g_pVPC->SetIgnoreRedundancyWarning( bVal );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Linux
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Linux( void )
{
	// always ignore everything in this block
	// parsed and processed by a different tool
	g_pVPC->GetScript().SkipBracedSection();
}

void VPC_PrepareToReadScript( const char *pInputScriptName, int depth, bool bQuiet, char* &pScriptBuffer, char szScriptName[MAX_PATH] )
{
	if ( !depth )
	{
		// startup initialization
		g_pVPC->GetProjectGenerator()->StartProject();
	}

	V_strncpy( szScriptName, pInputScriptName, MAX_PATH );
	V_FixSlashes( szScriptName );

	// always spew the root script
	if ( !bQuiet )
	{
		bool bSpew = ( depth == 0 );
		g_pVPC->VPCStatus( bSpew, "Parsing: %s", szScriptName );
	}

	// parse the text script
	if ( !Sys_Exists( szScriptName ) )
	{
		g_pVPC->VPCError( "Cannot open %s", szScriptName );	
	}

	// load it with the file expansions to compute it's CRC, so we notice if new matching files appear on disk
	// and regenerate the project correctly.
	int scriptLen = Sys_LoadTextFileWithIncludes( szScriptName, &pScriptBuffer, true );
	if ( scriptLen < 0 )
	{
		// unexpected due to existence check
		g_pVPC->VPCError( "Cannot open %s", szScriptName );	
	}

	g_pVPC->AddScriptToCRCCheck( szScriptName, CRC32_ProcessSingleBuffer( pScriptBuffer, scriptLen ) );

	delete pScriptBuffer;
	Sys_LoadTextFileWithIncludes( szScriptName, &pScriptBuffer, false );

	g_pVPC->GetScript().PushScript( szScriptName, pScriptBuffer );
}

//-----------------------------------------------------------------------------
// Adds the current VPC file to the 'VPC Script' file, setting all the
// custom builds steps needed to verify the .vcproj is up to date
//-----------------------------------------------------------------------------
void VPC_AddCurrentVPCScriptToProjectFolder( bool bDoCRCCheck ) 
{
	// skip including VPC scripts if NOVPC is defined
	if( g_pVPC->EvaluateConditionalExpression( "$NOVPC" ) )
		return;

	g_pVPC->GetProjectGenerator()->StartFolder( "VPC Scripts" );
	g_pVPC->GetProjectGenerator()->StartFile( g_pVPC->GetScript().GetName(), false );

	// only emit the extra information on windows, and only for the project vpc
	if ( bDoCRCCheck && g_pVPC->EvaluateConditionalExpression( "$WINDOWS" ) )
	{
		CUtlString sSentinel = CFmtStr( "$PROJECTDIR\\%s.sentinel", g_pVPC->GetScript().GetName() ).Access();
		bool bShouldSkip;

		CUtlVector< CUtlString > configurationNames;
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
		char rgchCRCCheckExpanded[2048]; rgchCRCCheckExpanded[0] = '\0';
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i], true );
			g_pVPC->GetProjectGenerator()->StartPropertySection( KEYWORD_CUSTOMBUILDSTEP, &bShouldSkip );
			// this will write a sentinel file so we have a clear build target so we can know the last time we checked
			g_pVPC->GetProjectGenerator()->HandleProperty( "$Description", CFmtStr("\"Running VPC CRC Check - %s\"", g_pVPC->GetScript().GetName() ) );
					g_pVPC->GetProjectGenerator()->HandleProperty( "$CommandLine", CFmtStr( "\"rem IncrediBuild_AllowOverlap\n%s\necho crc_complete > %s\"", g_pVPC->GetMacroValue( "CRCCHECK" ), sSentinel.Get() ) );
			g_pVPC->GetProjectGenerator()->HandleProperty( "$Outputs", sSentinel );
			g_pVPC->GetProjectGenerator()->EndPropertySection( KEYWORD_CUSTOMBUILDSTEP );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
	}

	g_pVPC->GetProjectGenerator()->EndFile();
	g_pVPC->GetProjectGenerator()->EndFolder();
}

void VPC_HandleIncludeStatement( int depth, bool bQuiet, void (*CallbackFn)( const char *pScriptName, int depth, bool bQuiet ) )
{
	char szBigBuffer[MAX_SYSTOKENCHARS];
	if ( g_pVPC->GetScript().ParsePropertyValue( NULL, szBigBuffer, sizeof( szBigBuffer ) ) )
	{
		// recurse into and run
		char *pScriptBuffer;
		char szFixedScriptName[MAX_PATH];
		VPC_PrepareToReadScript( szBigBuffer, depth+1, bQuiet, pScriptBuffer, szFixedScriptName );

		VPC_AddCurrentVPCScriptToProjectFolder( false );

		CallbackFn( szBigBuffer, depth+1, bQuiet );
		free( pScriptBuffer );
		
		// restore state
		g_pVPC->GetScript().PopScript();
	}
}

void VPC_HandleProjectCommands( const char *pUnusedScriptName, int depth, bool bQuiet )
{
	const char *pToken;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( !V_stricmp( pToken, "}" ) )
		{
			break;
		}
		else if ( !V_stricmp( pToken, "$include" ) )
		{
			VPC_HandleIncludeStatement( depth, bQuiet, VPC_HandleProjectCommands );
		}
		else if ( !V_stricmp( pToken, "$Folder" ) )
		{
			// root level folder
			VPC_Keyword_Folder( false );
		}
		else if ( !V_stricmp( pToken, "$Unity" ) )
		{
			// root level folder
			VPC_Keyword_Folder( g_pVPC->IsUnity() );
		}
		else if ( !V_stricmp( pToken, "$File" ) )
		{
			// add root level file
			VPC_Keyword_AddFile();
		}
		else if ( !V_stricmp( pToken, "$SchemaFile" ) )
		{
			// add root level file
			VPC_Keyword_AddFile( "schema" );
		}
		else if ( !V_stricmp( pToken, "-$File" ) )
		{
			// remove root level file
			VPC_Keyword_RemoveFile();
		}
		else if ( !V_stricmp( pToken, "$Shaders" ) )
		{
			// add root level shaders folder
			VPC_Keyword_Shaders( 0 );
		}
				else if ( !V_stricmp( pToken, "$macro" ) )
				{
					VPC_Keyword_Macro( VPC_MACRO_VALUE );
				}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}
}


void WriteCRCCheckFile( const char *pVCProjFilename )
{
	char szFilename[MAX_PATH];
	V_snprintf( szFilename, sizeof( szFilename ), "%s." VPCCRCCHECK_FILE_EXTENSION, pVCProjFilename );

	FILE *fp = fopen( szFilename, "wt" );
	if ( !fp )
	{
		g_pVPC->VPCError( "Unable to open %s to write CRCs into.", szFilename );
	}

	fprintf( fp, "%s\n", VPCCRCCHECK_FILE_VERSION_STRING );
	// add the executable crc
			char vpcExeAbsPath[MAX_PATH]; vpcExeAbsPath[0] = '\0';
	CRC32_t nCRCFromFileContents = 0;
			if ( Sys_GetExecutablePath( vpcExeAbsPath, sizeof( vpcExeAbsPath ) ) )
	{
		char *pBuffer;
				int cbVPCExe = Sys_LoadFile( vpcExeAbsPath, (void**)&pBuffer );

		// Calculate the CRC from the contents of the file.
		nCRCFromFileContents = CRC32_ProcessSingleBuffer( pBuffer, cbVPCExe );
		delete [] pBuffer;
	}

			const char *vpcExePath = vpcExeAbsPath;

			char vpcExeRelPath[MAX_PATH]; vpcExeRelPath[0] = '\0';
			if ( V_MakeRelativePath( vpcExeAbsPath, g_pVPC->GetProjectPath(), vpcExeRelPath, sizeof( vpcExeRelPath ) ) )
			{
				vpcExePath = vpcExeRelPath;
			}

	fprintf( fp, "%8.8x %s\n", ( unsigned int ) nCRCFromFileContents, vpcExePath );

	// add the supplemental string crc
	fprintf( fp, "%s\n", g_pVPC->GetCRCString() );
	
	CUtlDict<int, int> filenameDict( k_eDictCompareTypeFilenames );
	for ( int i=0; i < g_pVPC->m_ScriptList.Count(); i++ )
	{
		scriptList_t *pScript = &g_pVPC->m_ScriptList[i];

		// Use the dictionary to prevent duplicate file CRCs being written in here.
		if ( filenameDict.Find( pScript->m_scriptName.String() ) == filenameDict.InvalidIndex() )
		{
			filenameDict.Insert( pScript->m_scriptName.String(), 1 );

			// [crc] [filename]
			fprintf( fp, "%8.8x %s\n", ( unsigned int ) pScript->m_crc, pScript->m_scriptName.Get() );
		}
	}

	fclose( fp );
}


//
// This is called when it's done parsing a project. If there were any $SchemaFile
// entries in the project, then we will make this project depend on schemacompiler
// (via $AdditionalProjectDependencies).
//
// This fixes full rebuild problems where it's building a project that uses schemacompiler
// at the same time as it's building schemacompiler. This usually screws up when 
// it tries to copy the new schemacompiler.exe to game\bin but it's in use.
//
void VPC_ForceAdditionalSchemaDependencies( const char *pProjectName )
{
	if ( g_pVPC->m_SchemaFiles.Count() == 0 )
		return;

	// Add "$BASE;SchemaCompiler" to $AdditionalProjectDependencies.
	CUtlVector< CUtlString > configurationNames;
	g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
	for ( int i=0; i < configurationNames.Count(); i++ )
	{
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), false );
		g_pVPC->GetProjectGenerator()->StartPropertySection( KEYWORD_GENERAL, NULL );

		g_pVPC->GetProjectGenerator()->HandleProperty( g_pOption_AdditionalProjectDependencies, "$BASE;SchemaCompiler" );

		g_pVPC->GetProjectGenerator()->EndPropertySection( KEYWORD_GENERAL );
		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}


//-----------------------------------------------------------------------------
//	VPC_Keyword_Project
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Project( int depth, bool bQuiet )
{
	char	szProjectName[MAX_PATH];

	// check for optional project name
	const char *pToken = g_pVPC->GetScript().PeekNextToken( false );

	if ( pToken && pToken[0] && V_stricmp( pToken, "{" ) )
	{
		// get optional project name
		pToken = g_pVPC->GetScript().GetToken( false );
		if ( !pToken || !pToken[0] )
		{
			g_pVPC->VPCSyntaxError();
		}

		g_pVPC->ResolveMacrosInString( pToken, szProjectName, sizeof( szProjectName ) );

		if ( g_pVPC->IsDecorateProject() )
		{
					g_pVPC->DecorateProjectName( szProjectName );
		}
	}
	else
	{
		CUtlString strName = g_pVPC->GetProjectGenerator()->GetProjectName();
		V_strncpy( szProjectName, strName.String(), sizeof( szProjectName ) );
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	VPC_HandleProjectCommands( NULL, depth, bQuiet );
	
	// the unnamed project does not get written, once it is named it will be written on closing scope
	if ( V_stricmp( szProjectName, "UNNAMED" ) )
	{
		VPC_ForceAdditionalSchemaDependencies( szProjectName );

		// change name
		g_pVPC->GetProjectGenerator()->SetProjectName( szProjectName );

				// need to do this at the end of parsing the project, so we have all the macros needed
				VPC_AddCurrentVPCScriptToProjectFolder( true );

		g_pVPC->GetProjectGenerator()->EndProject();
		g_pVPC->m_bGeneratedProject = true;
	}
}

static const char* fileTypes[] =
{
	"c", "cpp", "cxx", "cc",
	"h", "hh", "hxx", "hpp", ""
};
bool VPC_IsBuiltInFileType( const char *extension )
{
				for (int i = 0; i < V_ARRAYSIZE(fileTypes); i++)
	{
		if ( !V_stricmp( fileTypes[i], extension ) )
			return true;
	}

	return false;
}

void VPC_Keyword_CustomBuildStep( void )
{
	bool							bAllowNextLine = false;
	CUtlVector<CUtlString>			extensions;

	const char *pToken = NULL;
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			if ( extensions.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $CustomBuildStep without any extensions preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				extensions.Remove( extensions.Count() - 1 );
			}

			continue;
		}

		if ( !V_stricmp( pToken, "\\" ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		if ( VPC_IsBuiltInFileType( pToken ) )
		{
			g_pVPC->VPCSyntaxError( "Cannot define custom build steps for built in file type: %s", pToken);
		}

		CUtlString string = pToken;
		extensions.AddToTail( string );

		// check for another optional file
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;
	}


	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_strcmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError( "Missing section for CustomBuildStep" );
	}
	else if ( extensions.Count() == 0 )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;
	}
	else
	{
		const char *pScriptSave = g_pVPC->GetScript().GetData();
		while ( 1 )
		{
			pToken = g_pVPC->GetScript().GetToken( true );
			if ( !pToken || !pToken[0] )
				break;

			if ( !V_stricmp( pToken, "}" ) )
			{
				// end of section
				break;
			}			
		}

		CUtlString buildsteps;
		if ( g_pVPC->GetScript().GetData() > pScriptSave )
		{
			CUtlString tempString;
			tempString.SetDirect( pScriptSave, int( g_pVPC->GetScript().GetData() - pScriptSave - 1 ) );
			buildsteps += "$Configuration\n{\n$CustomBuildStep\n{";
			buildsteps += tempString + "}\n}\n";
		}

		if ( !buildsteps.IsEmpty() )
		{
			FOR_EACH_VEC( extensions, i )
			{
				g_pVPC->m_CustomBuildSteps.Insert( extensions[i], buildsteps );
			}
		}
	}
}

void VPC_ParseProjectScriptParameters( const char *szScriptName, int depth, bool bQuiet )
{
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			// end of file
			break;
		}

		if ( !V_stricmp( pToken, "$include" ) )
		{
			VPC_HandleIncludeStatement( depth, bQuiet, VPC_ParseProjectScriptParameters );
		}
		else if ( !V_stricmp( pToken, "$configuration" ) )
		{
			VPC_Keyword_Configuration();
		}
		else if ( !V_stricmp( pToken, "$project" ) )
		{
			VPC_Keyword_Project( depth, bQuiet );
		}
		else if ( !V_stricmp( pToken, "$macro" ) )
		{
			VPC_Keyword_Macro( VPC_MACRO_VALUE );
		}
		else if ( !V_stricmp( pToken, "$MacroEmptyString" ) )
		{
			VPC_Keyword_Macro( VPC_MACRO_EMPTY_STRING );
		}
		else if ( !V_stricmp( pToken, "$MacroRequired" ) )
		{
			VPC_Keyword_MacroRequired( VPC_MACRO_REQUIRED_NOT_EMPTY );
		}
		else if ( !V_stricmp( pToken, "$MacroRequiredAllowEmpty" ) )
		{
			VPC_Keyword_MacroRequired( VPC_MACRO_REQUIRED_ALLOW_EMPTY );
		}
		else if ( !V_stricmp( pToken, "$LoadAddressMacro" ) )
		{
			VPC_Keyword_LoadAddressMacro();
		}
		else if ( !V_stricmp( pToken, "$LoadAddressMacroAlias" ) )
		{
			VPC_Keyword_LoadAddressMacroAlias();
		}
		else if ( !V_stricmp( pToken, "$LoadAddressMacroAuto" ) )
		{
			VPC_Keyword_LoadAddressMacroAuto();
		}
		else if ( !V_stricmp( pToken, "$LoadAddressMacroAuto_Padded" ) )
		{
			VPC_Keyword_LoadAddressMacroAuto_Padded();
		}
		else if ( !V_stricmp( pToken, "$IgnoreRedundancyWarning" ) )
		{
			VPC_Keyword_IgnoreRedundancyWarning();
		}
		else if ( !V_stricmp( pToken, "$Linux" ) )
		{
			VPC_Keyword_Linux();
		}
		else if ( !V_stricmp( pToken, "$CustomBuildStep" ) )
		{
			VPC_Keyword_CustomBuildStep();
		}
		else if ( !V_stricmp( pToken, "$Conditional" ) )
		{
			VPC_SharedKeyword_Conditional();
		}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::ParseProjectScript( const char *pScriptName, int depth, bool bQuiet, bool bWriteCRCCheckFile )
{
	char	*pScriptBuffer;
	char	szScriptName[MAX_PATH];

	VPC_PrepareToReadScript( pScriptName, depth, bQuiet, pScriptBuffer, szScriptName );
	
	int cMissingFilesPreParse = g_pVPC->GetMissingFilesCount();

	if ( !depth )
	{
		// create reserved $ROOTSCRIPT - tracks the root script
		FindOrCreateMacro( "ROOTSCRIPT", true, szScriptName );

		// create reserved $PROJECTNAME - tracks the undecorated pure project name
		// $(ProjectName) can be auto-decorated, making it unuseable by scripts expecting a pure project name
		FindOrCreateMacro( "PROJECTNAME", true, g_pVPC->GetProjectName() );

		// create reserved $LOADADDRESSNAME - defaults to project name but can be aliased with $LoadAddressMacroAlias
		FindOrCreateMacro( "LOADADDRESSNAME", true, g_pVPC->GetLoadAddressName() );

		// create reserved $CRCCHECK
		// The CRCs themselves are dumped into theproject.vcproj.vpc_crc (in VPC_WriteCRCCheckFile), so all this does 
		// is point vpccrccheck.exe at it with "vpccrccheck.exe -crc2 theproject.vcproj"
		// scripts add the terminal /n if they append, after referencing $CRCCHECK
					// needs to be quoted to work with /dp which puts " (platform) " in project names,

		CUtlString crcCheckString;
					crcCheckString = "if exist $QUOTE$SRCDIR\\devtools\\bin\\" VPCCRCCHECK_EXE_FILENAME "$QUOTE $QUOTE$SRCDIR\\devtools\\bin\\" VPCCRCCHECK_EXE_FILENAME "$QUOTE -crc2 $QUOTE";
		crcCheckString += g_pVPC->GetOutputFilename();
					crcCheckString += "$QUOTE\nif ERRORLEVEL 1 exit /b 1";
		g_pVPC->FindOrCreateMacro( "CRCCHECK", true, crcCheckString.Get() );

		// create reserved $PROJECTDIR
		char szProjectRootPath[MAX_PATH];
		V_snprintf( szProjectRootPath, sizeof( szProjectRootPath ), "%s", g_pVPC->GetProjectPath() );
		V_RemoveDotSlashes( szProjectRootPath );
		SetMacro( "PROJECTDIR", szProjectRootPath, true );

		// reset
		g_pVPC->m_SchemaFiles.Purge();

		// reset unity file tracking (it's per project)
		g_pVPC->m_UnityFilesSeen.RemoveAll();
		g_pVPC->m_UnityStack.Clear();
		g_pVPC->m_sUnityCurrent = NULL;
	}

	VPC_ParseProjectScriptParameters( szScriptName, depth, bQuiet );

	free( pScriptBuffer );

	// for safety, force callers to restore to proper state
	g_pVPC->GetScript().PopScript();

	if ( !depth )
	{
		// at end of all processing, don't write crc checks if we're missing files
		if ( bWriteCRCCheckFile && g_pVPC->GetMissingFilesCount() == cMissingFilesPreParse )
		{
			// Finally write out the file with all the CRCs in it. This is referenced by the $CRCCHECK macro in the prebuild steps.
			WriteCRCCheckFile( g_pVPC->GetOutputFilename() );
		}

		g_pVPC->m_ScriptList.Purge();
		g_pVPC->RemoveScriptCreatedMacros(); // Remove any macros that came from the script file.
	}

	return true;
}

void CVPC::AddScriptToCRCCheck( const char *pScriptName, CRC32_t crc )
{
	for ( int i = 0; i < m_ScriptList.Count(); i++ )
	{
		if ( !V_stricmp( m_ScriptList[i].m_scriptName, pScriptName ) )
		{
			// update
			g_pVPC->m_ScriptList[i].m_crc = crc;
			return;
		}
	}

	int index = g_pVPC->m_ScriptList.AddToTail();
	g_pVPC->m_ScriptList[index].m_scriptName = pScriptName;
	g_pVPC->m_ScriptList[index].m_crc = crc;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//
//							Schema Script Generation
//
// Temporary - Once the schema workflow settles down, this schema-specific code should be 
//             at minimum moved into its own file, and ideally generalized so that VPC
//             has a minimum of embedded schema-specific logic.
//
//
//
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

KeyValues *ConfigPreprocessorSettingsAsKV( CSpecificConfig *pConfig );
struct SchemaFileInfo_t
{
	bool bIsCppFile;
	char szFile[MAX_PATH];
	char szGeneratedFile[MAX_PATH];
};

void VPC_FakeKeyword_SchemaFolder( CBaseProjectDataCollector *pDataCollector )
{
	if ( g_pVPC->m_SchemaFiles.Count() == 0 )
		return;

	macro_t *pMacro = g_pVPC->FindOrCreateMacro( "NOSCHEMACOMPILER", false, NULL );
	if ( pMacro && atoi( pMacro->value.Get() ) )
		return;

	CUtlString platformName = g_pVPC->GetTargetPlatformName();
	V_strlower( platformName.Get() );
	const char *pPlatformName = platformName.String();

	CUtlVector<SchemaFileInfo_t> schemaFileInfos;
	schemaFileInfos.AddMultipleToTail( g_pVPC->m_SchemaFiles.Count() );

	char schemaCompilerPath[MAX_PATH];
	g_pVPC->ResolveMacrosInString( "$SRCDIR/devtools/bin/schemacompiler.dll", schemaCompilerPath, sizeof( schemaCompilerPath ) );

	CUtlString schemaInputs;
	CUtlString schemaOutputs;
	char szExt[MAX_PATH];

	schemaInputs += schemaCompilerPath;

	for ( int i = 0; i < g_pVPC->m_SchemaFiles.Count(); i++ )
	{
		if ( !g_pVPC->m_SchemaFiles[i].String() )
			continue;

		schemaInputs += ";";
		schemaInputs += g_pVPC->m_SchemaFiles[i];

		V_ExtractFileExtension( g_pVPC->m_SchemaFiles[i], szExt, sizeof(szExt) );
		SchemaFileInfo_t &fileInfo = schemaFileInfos[i];

		V_strncpy( fileInfo.szFile, g_pVPC->m_SchemaFiles[i], sizeof(fileInfo.szFile) );
		V_FixSlashes( fileInfo.szFile, '/' );

		char szNoExt[MAX_PATH];
		V_StripExtension( fileInfo.szFile, szNoExt, sizeof(szNoExt) );
		char szBase[MAX_PATH];
		V_FileBase( fileInfo.szFile, szBase, sizeof(szBase) );
		char szPath[MAX_PATH];
		V_ExtractFilePath( fileInfo.szFile, szPath, sizeof(szPath) );

		char szGenRelPath[MAX_PATH];

		if ( IsCFileExtension( szExt ) )
		{
			fileInfo.bIsCppFile = true;
			V_snprintf( szGenRelPath,  sizeof(szGenRelPath),
						"generated_code_%s_%s/%s_schema_cpp_gen.cpp", g_pVPC->GetProjectName(), pPlatformName, szBase );
		}
		else
		{
			fileInfo.bIsCppFile = false;
			V_snprintf( szGenRelPath,  sizeof(szGenRelPath),
						"generated_code_%s_%s/%s_schema_gen.cpp", g_pVPC->GetProjectName(), pPlatformName, szBase );
		}

		V_strncpy( fileInfo.szGeneratedFile, szGenRelPath, sizeof(fileInfo.szGeneratedFile) );
		V_FixSlashes( fileInfo.szGeneratedFile, '/' );

		schemaOutputs += fileInfo.szGeneratedFile;
		schemaOutputs += ";";
	}

	char szSchemaOutAnchorPath[MAX_PATH];
	V_snprintf( szSchemaOutAnchorPath,  sizeof( szSchemaOutAnchorPath ),
				"generated_code_%s_%s/%s_schema_anchor.cpp", g_pVPC->GetProjectName(), pPlatformName, g_pVPC->GetProjectName() );
	V_FixSlashes( szSchemaOutAnchorPath, '/' );
	schemaOutputs += szSchemaOutAnchorPath;
	schemaOutputs += ";";

	char szSchemaPath[MAX_PATH];
	V_snprintf( szSchemaPath,  sizeof( szSchemaPath ), "%s_%s.schproj", g_pVPC->GetProjectName(), pPlatformName );

	//////////////////////////////////////////////////////////////////////////

	CUtlBuffer					vpcBuffer;

	char schemaCompilerExePath[MAX_PATH];
	g_pVPC->ResolveMacrosInString( "$SRCDIR/devtools/bin/schemacompiler.exe", schemaCompilerExePath, sizeof( schemaCompilerExePath ) );
	V_FixSlashes( schemaCompilerExePath );

	vpcBuffer.SetBufferType( true, true );
	vpcBuffer.Printf( "$Folder \"Autogenerated Schema Files\" \n" );
	vpcBuffer.Printf( "{\n" );
	vpcBuffer.Printf( 
		"$File \"%s\"\n"
		"{\n"
		"	$Configuration\n"
		"	{\n"
		"		$CustomBuildStep\n"
		"		{\n"
		"			$CommandLine	\"%s -schproj $(InputPath) -config $(ConfigurationName)\"\n"
		"			$Description	\"Schema Compiler for [$(ConfigurationName)] $(InputName)\"\n"
		"			$Outputs		\"%s\"\n"
		"			$AdditionalDependencies		\"%s\"\n"
		"		}\n"
		"	}\n"
		"}\n", szSchemaPath, schemaCompilerExePath, schemaOutputs.Get(), schemaInputs.Get() );

	static const char *schemaConfiguration =
		"{\n"
		"	$Configuration\n"
		"	{\n"
		"		$Compiler\n"
		"		{\n"
		//"			$PreprocessorDefinitions			\"NO_MEMOVERRIDE_NEW_DELETE\"\n"
		"			$Create/UsePrecompiledHeader		\"Not Using Precompiled Headers\"\n"
		"		}\n"
		"	}\n"
		"}\n";
	
	vpcBuffer.Printf( "$Folder \"Cpp Schema Wrappers\" \n" );
	{
		vpcBuffer.Printf( "{\n" );

		for ( int i = 0; i < schemaFileInfos.Count(); ++i )
		{
			if ( schemaFileInfos[i].bIsCppFile )
			{
				vpcBuffer.Printf( "$File \"%s\"\n", schemaFileInfos[i].szGeneratedFile );
				vpcBuffer.Printf( schemaConfiguration );
			}
		}

		vpcBuffer.Printf( "}\n" );
	}

	vpcBuffer.Printf( "$Folder \"Header-Generated Files\" \n" );
	{
		vpcBuffer.Printf( "{\n" );

		for ( int i = 0; i < schemaFileInfos.Count(); ++i )
		{
			if ( !schemaFileInfos[i].bIsCppFile )
			{
				vpcBuffer.Printf( "$File \"%s\"\n", schemaFileInfos[i].szGeneratedFile );
				vpcBuffer.Printf( schemaConfiguration );
			}
		}

		vpcBuffer.Printf( "}\n" );
	}

	vpcBuffer.Printf( "$File \"%s\"\n", szSchemaOutAnchorPath );
	vpcBuffer.Printf( schemaConfiguration );
	vpcBuffer.Printf( "}\n" );

	// save parser
	bool bIgnoreRedundancyWarning = g_pVPC->IsIgnoreRedundancyWarning();
	g_pVPC->SetIgnoreRedundancyWarning( true );
	g_pVPC->GetScript().PushScript( "Internal List [Schema]", (char*)vpcBuffer.Base() );

	const char *pToken = g_pVPC->GetScript().GetToken( true );
	if ( pToken && pToken[0] && !V_stricmp( pToken, "$folder" ) )
	{
		VPC_Keyword_Folder( false );
	}

	// restore parser
	g_pVPC->GetScript().PopScript();
	g_pVPC->SetIgnoreRedundancyWarning( bIgnoreRedundancyWarning );

	//////////////////////////////////////////////////////////////////////////

	KeyValues *pOutKeyValues = new KeyValues("schproj");
	pOutKeyValues->SetString( "project_name", g_pVPC->GetProjectName() );
	pOutKeyValues->SetString( "platform_name", pPlatformName );
	pOutKeyValues->SetString( "anchor_path", szSchemaOutAnchorPath );
	char dmeTargetFolder[MAX_PATH];
	g_pVPC->ResolveMacrosInString( "$SRCDIR\\public", dmeTargetFolder, sizeof( dmeTargetFolder ) );
	pOutKeyValues->SetString( "dme_target_folder", dmeTargetFolder );	

	KeyValues *pOutAllConfigs = new KeyValues( "configs" );
	pOutKeyValues->AddSubKey( pOutAllConfigs );

	for ( int i = pDataCollector->m_BaseConfigData.m_Configurations.First();
		  i != pDataCollector->m_BaseConfigData.m_Configurations.InvalidIndex();
		  i = pDataCollector->m_BaseConfigData.m_Configurations.Next( i ) )
	{
		CSpecificConfig *pConfig = pDataCollector->m_BaseConfigData.m_Configurations[i];
		pOutAllConfigs->AddSubKey( ConfigPreprocessorSettingsAsKV( pConfig ) );
	}

	char szNum[64];
	{
		KeyValues *pOutInputs = new KeyValues( "inputs" );
		int nInput = 0;
		pOutKeyValues->AddSubKey( pOutInputs );
		for ( int i = 0; i < schemaFileInfos.Count(); i++ )
		{
			V_snprintf( szNum, sizeof(szNum), "%03d", nInput++ );

			SchemaFileInfo_t &fileInfo = schemaFileInfos[i];

			KeyValues *pOutFile = new KeyValues( szNum );
			pOutInputs->AddSubKey( pOutFile );

			pOutFile->SetString( "generation_target", fileInfo.szGeneratedFile );
			pOutFile->SetBool( "is_cpp", fileInfo.bIsCppFile );
			pOutFile->SetString( "input", fileInfo.szFile );
		}
	}
	CUtlBuffer tmpBuf;
	tmpBuf.SetBufferType( true, true );
	pOutKeyValues->RecursiveSaveToFile( tmpBuf, 0 );
	pOutKeyValues->deleteThis();

	FILE *fp = fopen( szSchemaPath, "wt" );
	if ( !fp )
	{
		g_pVPC->VPCStatus( true, "Error Saving File: '%s'", szSchemaPath );
	}
	else
	{
		fwrite( tmpBuf.Base(), sizeof(char), tmpBuf.TellMaxPut(), fp );
		fclose( fp );
	}
}

KeyValues *ConfigPreprocessorSettingsAsKV( CSpecificConfig * pConfig )
{
	KeyValues *pOutConfig = new KeyValues( pConfig->GetConfigName() );

	char szNum[64];
	KeyValues *pInConfigKV = pConfig->m_pKV;

	//////////////////////////////////////////////////////////////////////////
	// write defines 

	{
		KeyValues *pOutDefines = new KeyValues( "defines" );
		pOutConfig->AddSubKey( pOutDefines );

		CSplitString outStrings( pInConfigKV->GetString( g_pOption_PreprocessorDefinitions ),
						(const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );

		int nDefine = 0;

		for ( int i=0; i < outStrings.Count(); i++ )
		{
			V_snprintf( szNum, sizeof(szNum), "%03d", nDefine++ );
			pOutDefines->SetString( szNum, outStrings[i] );
		}

		// change #1001922 from source2 did the pBuf...
		char pBuf[512];
		for ( int i=0; i < g_pVPC->m_Macros.Count(); i++ )
		{
			macro_t *pMacro = &g_pVPC->m_Macros[i];

			if ( pMacro->m_bSetupDefineInProjectFile )
			{
				V_snprintf( szNum, sizeof(szNum), "%03d", nDefine++ );
				V_snprintf( pBuf, sizeof(pBuf), "%s=%s", pMacro->name.Get(), pMacro->value.Get() );
				pOutDefines->SetString( szNum, pBuf );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// write includes 

	KeyValues *pOutIncludes = new KeyValues( "includes" );
	pOutConfig->AddSubKey( pOutIncludes );

	int nInclude = 0;

					CSplitString outStrings( pInConfigKV->GetString( g_pOption_AdditionalIncludeDirectories ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
	for ( int i=0; i < outStrings.Count(); i++ )
	{
		V_snprintf( szNum, sizeof(szNum), "%03d", nInclude++ );

		char sDir[MAX_PATH];
		V_StrSubst( outStrings[i], "$(IntDir)", "$(OBJ_DIR)", sDir, sizeof( sDir ) );
		V_FixSlashes( sDir, '/' );
		pOutIncludes->SetString( szNum, sDir );
	}

	return pOutConfig;
}
