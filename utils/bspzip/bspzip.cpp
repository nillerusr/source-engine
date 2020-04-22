//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include "bsplib.h"
#include "cmdlib.h"
#include "tier0/icommandline.h"
#include "utlbuffer.h"

int CopyVariableLump( int lump, void **dest, int size );

void StripPath( const char* pPath, char* pBuf, int nBufLen )
{
	const char	*pSrc;

	// want filename only
	pSrc = pPath + Q_strlen( pPath ) - 1;
	while ((pSrc != pPath) && (*(pSrc-1) != '\\') && (*(pSrc-1) != '/') && (*(pSrc-1) != ':'))
		pSrc--;

	Q_strncpy( pBuf, pSrc, nBufLen );
}

bool RepackBSP( const char *pszMapFile, bool bCompress )
{
	Msg( "Repacking %s\n", pszMapFile );

	if ( !g_pFullFileSystem->FileExists( pszMapFile ) )
	{
		Warning( "Couldn't open input file %s - BSP recompress failed\n", pszMapFile );
		return false;
	}

	CUtlBuffer inputBuffer;
	if ( !g_pFullFileSystem->ReadFile( pszMapFile, NULL, inputBuffer ) )
	{
		Warning( "Couldn't read file %s - BSP compression failed\n", pszMapFile );
		return false;
	}

	CUtlBuffer outputBuffer;

	if ( !RepackBSP( inputBuffer, outputBuffer,
	                 bCompress ? RepackBSPCallback_LZMA : NULL,
	                 bCompress ? IZip::eCompressionType_LZMA : IZip::eCompressionType_None ) )
	{
		Warning( "Internal error compressing BSP\n" );
		return false;
	}

	g_pFullFileSystem->WriteFile( pszMapFile, NULL, outputBuffer );

	Msg( "Successfully repacked %s -- %u -> %u bytes\n",
	     pszMapFile, inputBuffer.TellPut(), outputBuffer.TellPut() );

	return true;
}

void Usage( void )
{
	fprintf( stderr, "usage: \n" );
	fprintf( stderr, "bspzip -extract <bspfile> <blah.zip>\n");
	fprintf( stderr, "bspzip -extractfiles <bspfile> <targetpath>\n");
	fprintf( stderr, "bspzip -dir <bspfile>\n");
	fprintf( stderr, "bspzip -addfile <bspfile> <relativepathname> <fullpathname> <newbspfile>\n");
	fprintf( stderr, "bspzip -addlist <bspfile> <listfile> <newbspfile>\n");
	fprintf( stderr, "bspzip -addorupdatelist <bspfile> <listfile> <newbspfile>\n");
	fprintf( stderr, "bspzip -extractcubemaps <bspfile> <targetPath>\n");
	fprintf( stderr, "  Extracts the cubemaps to <targetPath>.\n");
	fprintf( stderr, "bspzip -deletecubemaps <bspfile>\n");
	fprintf( stderr, "  Deletes the cubemaps from <bspFile>.\n");
	fprintf( stderr, "bspzip -addfiles <bspfile> <relativePathPrefix> <listfile> <newbspfile>\n");
	fprintf( stderr, "  Adds files to <newbspfile>.\n");
	fprintf( stderr, "bspzip -repack [ -compress ] <bspfile>\n");
	fprintf( stderr, "  Optimally repacks a BSP file, optionally using compressed BSP format.\n");
	fprintf( stderr, "  Using on a compressed BSP without -compress will effectively decompress\n");
	fprintf( stderr, "  a compressed BSP.\n");

	exit( -1 );
}

char* xzpFilename = NULL;

int main( int argc, char **argv )
{
	::SetHDRMode( false );

	Msg( "\nValve Software - bspzip.exe (%s)\n", __DATE__ );

	int curArg = 1;

	// Options parsing assumes
	// [ -game foo ] -<action> <action specific args>

	// Skip -game foo
	if ( argc >= curArg + 2 && stricmp( argv[curArg], "-game" ) == 0 )
	{
		// Handled by filesystem code
		curArg += 2;
	}

	// Should have at least action
	if ( curArg >= argc )
	{
		// End of args
		Usage();
		return 0;
	}

	// Pointers to the action, the file, and any action args so I can remove all the messy argc pointer math this was
	// using.
	char *pAction = argv[curArg];
	curArg++;
	char **pActionArgs = &argv[curArg];
	int nActionArgs = argc - curArg;

	CommandLine()->CreateCmdLine( argc, argv );

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	if( ( stricmp( pAction, "-extract" ) == 0 ) && nActionArgs == 2 )
	{
		// bspzip -extract <bspfile> <blah.zip>
		CmdLib_InitFileSystem( pActionArgs[0] );

		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension( bspName, ".bsp", sizeof( bspName ) );

		char zipName[MAX_PATH] = { 0 };
		V_strncpy( zipName, pActionArgs[1], sizeof( zipName ) );
		Q_DefaultExtension(zipName, ".zip", sizeof( zipName ) );

		ExtractZipFileFromBSP( bspName, zipName );
	}
	else if( ( stricmp( pAction, "-extractfiles" ) == 0 ) && nActionArgs == 2 )
	{
		// bsipzip -extractfiles <bspfile> <targetpath>
		CmdLib_InitFileSystem( pActionArgs[0] );

		// necessary for xbox process
		// only the .vtf are extracted as necessary for streaming and not the .vmt
		// the .vmt are non-streamed and therefore remain, referenced normally as part of the bsp search path
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension( bspName, ".bsp", sizeof( bspName ) );

		char targetPathName[MAX_PATH] = { 0 };
		V_strncpy( targetPathName, pActionArgs[1], sizeof( targetPathName ) );
		Q_AppendSlash( targetPathName, sizeof( targetPathName ) );

		printf( "\n" );
		printf( "Opening bsp file: %s\n", bspName );
		LoadBSPFile( bspName );

		CUtlBuffer	buf;
		char		relativeName[MAX_PATH] = { 0 };
		char		targetName[MAX_PATH] = { 0 };
		int			fileSize = 0;
		int			id = -1;
		int			numFilesExtracted = 0;
		while(true)
		{
			id = GetNextFilename( GetPakFile(), id, relativeName, sizeof(relativeName), fileSize );
			if ( id == -1)
				break;

			{
				buf.EnsureCapacity( fileSize );
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
				ReadFileFromPak( GetPakFile(), relativeName, false, buf );

				V_strncpy( targetName, targetPathName, sizeof(targetName) );
				V_strncat( targetName, relativeName, sizeof(targetName) );
				Q_FixSlashes( targetName, '\\' );

				SafeCreatePath( targetName );

				printf( "Writing file: %s\n", targetName );
				FILE *fp = fopen( targetName, "wb" );
				if ( !fp )
				{
					printf( "Error: Could not write %s\n", targetName);
					exit( -1 );
				}

				fwrite( buf.Base(), fileSize, 1, fp );
				fclose( fp );

				numFilesExtracted++;
			}
		}
		printf( "%d files extracted.\n", numFilesExtracted );
	}
	else if( ( stricmp( pAction, "-extractcubemaps" ) == 0 ) && nActionArgs == 2 )
	{
		// bspzip -extractcubemaps <bspfile> <targetPath>
		CmdLib_InitFileSystem( pActionArgs[0] );
		// necessary for xbox process
		// only the .vtf are extracted as necessary for streaming and not the .vmt
		// the .vmt are non-streamed and therefore remain, referenced normally as part of the bsp search path
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension( bspName, ".bsp", sizeof( bspName ) );

		char targetPathName[MAX_PATH] = { 0 };
		V_strncpy( targetPathName, pActionArgs[1], sizeof(targetPathName) );
		Q_AppendSlash( targetPathName, sizeof( targetPathName ) );

		printf( "\n" );
		printf( "Opening bsp file: %s\n", bspName );
		LoadBSPFile( bspName );

		CUtlBuffer	buf;
		char		relativeName[MAX_PATH] = { 0 };
		char		targetName[MAX_PATH] = { 0 };
		int			fileSize = 0;
		int			id = -1;
		int			numFilesExtracted = 0;
		while (1)
		{
			id = GetNextFilename( GetPakFile(), id, relativeName, sizeof(relativeName), fileSize );
			if ( id == -1)
				break;

			if ( Q_stristr( relativeName, ".vtf" ) )
			{
				buf.EnsureCapacity( fileSize );
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
				ReadFileFromPak( GetPakFile(), relativeName, false, buf );

				V_strncpy( targetName, targetPathName, sizeof( targetName ) );
				V_strncat( targetName, relativeName, sizeof( targetName ) );
				Q_FixSlashes( targetName, '\\' );

				SafeCreatePath( targetName );

				printf( "Writing vtf file: %s\n", targetName );
				FILE *fp = fopen( targetName, "wb" );
				if ( !fp )
				{
					printf( "Error: Could not write %s\n", targetName);
					exit( -1 );
				}

				fwrite( buf.Base(), fileSize, 1, fp );
				fclose( fp );

				numFilesExtracted++;
			}
		}
		printf( "%d cubemaps extracted.\n", numFilesExtracted );
	}
	else if( ( stricmp( pAction, "-deletecubemaps" ) == 0 ) && nActionArgs == 1 )
	{
		// bspzip -deletecubemaps <bspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );
		// necessary for xbox process
		// the cubemaps are deleted as they cannot yet be streamed out of the bsp
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension( bspName, ".bsp", sizeof( bspName ) );

		printf( "\n" );
		printf( "Opening bsp file: %s\n", bspName );
		LoadBSPFile( bspName );

		CUtlBuffer	buf;
		char		relativeName[MAX_PATH] = { 0 };
		int			fileSize = 0;
		int			id = -1;
		int			numFilesDeleted = 0;
		while (1)
		{
			id = GetNextFilename( GetPakFile(), id, relativeName, sizeof(relativeName), fileSize );
			if ( id == -1)
				break;

			if ( Q_stristr( relativeName, ".vtf" ) )
			{
				RemoveFileFromPak( GetPakFile(), relativeName );

				numFilesDeleted++;

				// start over
				id = -1;
			}
		}

		printf( "%d cubemaps deleted.\n", numFilesDeleted );
		if ( numFilesDeleted )
		{
			// save out bsp file
			printf( "Updating bsp file: %s\n", bspName );
			WriteBSPFile( bspName );
		}
	}
	else if( ( stricmp( pAction, "-addfiles" ) == 0 ) && nActionArgs == 4 )
	{
		// bspzip -addfiles <bspfile> <relativePathPrefix> <listfile> <newbspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension( bspName, ".bsp", sizeof( bspName ) );

		char relativePrefixName[MAX_PATH] = { 0 };
		V_strncpy( relativePrefixName, pActionArgs[1], sizeof( relativePrefixName ) );

		char filelistName[MAX_PATH] = { 0 };
		V_strncpy( filelistName, pActionArgs[2], sizeof( filelistName ) );

		char newbspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( newbspName, sizeof( newbspName ), pActionArgs[3] );
		Q_DefaultExtension( newbspName, ".bsp", sizeof( newbspName) );

		char fullpathName[MAX_PATH] = { 0 };
		FILE *fp = fopen(filelistName, "r");
		if ( fp )
		{
			printf("Opening bsp file: %s\n", bspName);
			LoadBSPFile(bspName);

			while ( !feof(fp) )
			{
				if ( ( fgets( fullpathName, sizeof( fullpathName ), fp ) != NULL ) )
				{
					if ( *fullpathName && fullpathName[strlen(fullpathName) - 1] == '\n' )
					{
						fullpathName[strlen(fullpathName) - 1] = '\0';
					}

					// strip the path and add relative prefix
					char fileName[MAX_PATH] = { 0 };
					char relativeName[MAX_PATH] = { 0 };
					StripPath( fullpathName, fileName, sizeof( fileName ) );
					V_strncpy( relativeName, relativePrefixName, sizeof( relativeName ) );
					V_strncat( relativeName, fileName, sizeof( relativeName ) );

					printf( "Adding file: %s as %s\n", fullpathName, relativeName );

					AddFileToPak( GetPakFile(), relativeName, fullpathName );
				}
				else if ( !feof( fp ) )
				{
					printf( "Error: Missing full path names\n");
					fclose( fp );
					return( -1 );
				}
			}
			printf("Writing new bsp file: %s\n", newbspName);
			WriteBSPFile(newbspName);
			fclose( fp );
		}
	}
	else if( ( stricmp( pAction, "-dir" ) == 0 ) && nActionArgs == 1 )
	{
		// bspzip -dir <bspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension (bspName, ".bsp", sizeof( bspName ) );
		LoadBSPFile (bspName);
		PrintBSPPackDirectory();
	}
	else if( ( stricmp( pAction, "-addfile" ) == 0 ) && nActionArgs == 4 )
	{
		// bspzip -addfile <bspfile> <relativepathname> <fullpathname> <newbspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );

		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension (bspName, ".bsp", sizeof( bspName ) );

		char relativeName[MAX_PATH] = { 0 };
		V_strncpy( relativeName, pActionArgs[1], sizeof( relativeName ) );

		char fullpathName[MAX_PATH] = { 0 };
		V_strncpy( fullpathName, pActionArgs[2], sizeof( fullpathName ) );

		char newbspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( newbspName, sizeof( newbspName ), pActionArgs[3] );
		Q_DefaultExtension (newbspName, ".bsp", sizeof( newbspName ) );

		// read it in, add pack file, write it back out
		LoadBSPFile (bspName);
		AddFileToPak( GetPakFile(), relativeName, fullpathName );
		WriteBSPFile(newbspName);
	}
	else if( ( stricmp( pAction, "-addlist" ) == 0 ) && nActionArgs == 3 )
	{
		// bspzip -addlist <bspfile> <listfile> <newbspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );

		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension (bspName, ".bsp", sizeof( bspName ) );

		char filelistName[MAX_PATH] = { 0 };
		V_strncpy( filelistName, pActionArgs[1], sizeof( filelistName ) );

		char newbspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( newbspName, sizeof( newbspName ), pActionArgs[2] );
		Q_DefaultExtension (newbspName, ".bsp", sizeof( newbspName) );

		// read it in, add pack file, write it back out

		char relativeName[MAX_PATH] = { 0 };
		char fullpathName[MAX_PATH] = { 0 };
		FILE *fp = fopen(filelistName, "r");
		if ( fp )
		{
			printf("Opening bsp file: %s\n", bspName);
			LoadBSPFile (bspName);

			while ( !feof(fp) )
			{
				relativeName[ 0 ] = 0;
				fullpathName[ 0 ] = 0;
				if ( ( fgets( relativeName, sizeof( relativeName ), fp ) != NULL ) &&
				     ( fgets( fullpathName, sizeof( fullpathName ), fp ) != NULL ) )
				{
					int l1 = strlen(relativeName);
					int l2 = strlen(fullpathName);
					if ( l1 > 0 )
					{
						if ( relativeName[ l1 - 1 ] == '\n' )
						{
							relativeName[ l1 - 1 ] = 0;
						}
					}
					if ( l2 > 0 )
					{
						if ( fullpathName[ l2 - 1 ] == '\n' )
						{
							fullpathName[ l2 - 1 ] = 0;
						}
					}
					printf("Adding file: %s\n", fullpathName);
					AddFileToPak( GetPakFile(), relativeName, fullpathName );
				}
				else if ( !feof( fp ) || ( relativeName[0] && !fullpathName[0] ) )
				{
					printf( "Error: Missing paired relative/full path names\n");
					fclose( fp );
					return( -1 );
				}
			}
			printf("Writing new bsp file: %s\n", newbspName);
			WriteBSPFile(newbspName);
			fclose( fp );
		}
	}
	else if( ( stricmp( pAction, "-addorupdatelist" ) == 0 ) && nActionArgs == 3 )
	{
		// bspzip -addorupdatelist <bspfile> <listfile> <newbspfile>
		CmdLib_InitFileSystem( pActionArgs[0] );
		char bspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( bspName, sizeof( bspName ), pActionArgs[0] );
		Q_DefaultExtension (bspName, ".bsp", sizeof( bspName ) );

		char filelistName[MAX_PATH] = { 0 };
		V_strncpy( filelistName, pActionArgs[1], sizeof( filelistName ) );

		char newbspName[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( newbspName, sizeof( newbspName ), pActionArgs[2] );
		Q_DefaultExtension (newbspName, ".bsp", sizeof( newbspName) );

		// read it in, add pack file, write it back out

		char relativeName[MAX_PATH] = { 0 };
		char fullpathName[MAX_PATH] = { 0 };
		FILE *fp = fopen(filelistName, "r");
		if ( fp )
		{
			printf("Opening bsp file: %s\n", bspName);
			LoadBSPFile (bspName);

			while ( !feof(fp) )
			{
				relativeName[ 0 ] = 0;
				fullpathName[ 0 ] = 0;
				if ( ( fgets( relativeName, sizeof( relativeName ), fp ) != NULL ) &&
				     ( fgets( fullpathName, sizeof( fullpathName ), fp ) != NULL ) )
				{
					int l1 = strlen(relativeName);
					int l2 = strlen(fullpathName);
					if ( l1 > 0 )
					{
						if ( relativeName[ l1 - 1 ] == '\n' )
						{
							relativeName[ l1 - 1 ] = 0;
						}
					}
					if ( l2 > 0 )
					{
						if ( fullpathName[ l2 - 1 ] == '\n' )
						{
							fullpathName[ l2 - 1 ] = 0;
						}
					}

					bool bUpdating = FileExistsInPak( GetPakFile(), relativeName );
					printf("%s file: %s\n", bUpdating ? "Updating" : "Adding", fullpathName);
					if ( bUpdating )
					{
						RemoveFileFromPak( GetPakFile(), relativeName );
					}
					AddFileToPak( GetPakFile(), relativeName, fullpathName );
				}
				else if ( !feof( fp ) || ( relativeName[0] && !fullpathName[0] ) )
				{
					printf( "Error: Missing paired relative/full path names\n");
					fclose( fp );
					return( -1 );
				}
			}
			printf("Writing new bsp file: %s\n", newbspName);
			WriteBSPFile(newbspName);
			fclose( fp );
		}
	}
	else if( ( stricmp( pAction, "-repack" ) == 0 ) && ( nActionArgs == 1 || nActionArgs == 2 ) )
	{
		// bspzip -repack [ -compress ] <bspfile>
		bool bCompress = false;
		const char *pFile = pActionArgs[0];
		if ( nActionArgs == 2 && stricmp( pActionArgs[0], "-compress" ) == 0 )
		{
			pFile = pActionArgs[1];
			bCompress = true;
		}
		else if ( nActionArgs == 2 )
		{
			Usage();
			return 0;
		}

		CmdLib_InitFileSystem( pFile );
		char szAbsBSPPath[MAX_PATH] = { 0 };
		Q_MakeAbsolutePath( szAbsBSPPath, sizeof( szAbsBSPPath ), pFile );
		Q_DefaultExtension( szAbsBSPPath, ".bsp", sizeof( szAbsBSPPath ) );
		return RepackBSP( szAbsBSPPath, bCompress ) ? 0 : -1;
	}
	else
	{
		Usage();
	}
	return 0;
}
