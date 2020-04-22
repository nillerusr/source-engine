//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper methods + classes for file access
//
//===========================================================================//

#include "tier2/fileutils.h"
#include "tier2/tier2.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/utlbuffer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Builds a directory which is a subdirectory of the current mod
//-----------------------------------------------------------------------------
void GetModSubdirectory( const char *pSubDir, char *pBuf, int nBufLen )
{
	// Compute starting directory
	Assert( g_pFullFileSystem->GetSearchPath( "MOD_WRITE", false, NULL, 0 ) < nBufLen );
	if ( g_pFullFileSystem->GetSearchPath( "MOD_WRITE", false, pBuf, nBufLen ) == 0 )
	{
		// if we didn't find MOD_WRITE, back to the old MOD
		Assert( g_pFullFileSystem->GetSearchPath( "MOD", false, NULL, 0 ) < nBufLen );
		g_pFullFileSystem->GetSearchPath( "MOD", false, pBuf, nBufLen );
	}

	char *pSemi = strchr( pBuf, ';' );
	if ( pSemi )
	{
		*pSemi = 0;
	}

	Q_StripTrailingSlash( pBuf );
	if ( pSubDir )
	{
		int nLen = Q_strlen( pSubDir );
		Q_strncat( pBuf, "\\", nBufLen, 1 );
		Q_strncat( pBuf, pSubDir, nBufLen, nLen );
	}

	Q_FixSlashes( pBuf );
}


//-----------------------------------------------------------------------------
// Builds a directory which is a subdirectory of the current mod's *content*
//-----------------------------------------------------------------------------
void GetModContentSubdirectory( const char *pSubDir, char *pBuf, int nBufLen )
{
	char pTemp[ MAX_PATH ];
	GetModSubdirectory( pSubDir, pTemp, sizeof(pTemp) );
	ComputeModContentFilename( pTemp, pBuf, nBufLen );
}


//-----------------------------------------------------------------------------
// Generates a filename under the 'game' subdirectory given a subdirectory of 'content'
//-----------------------------------------------------------------------------
void ComputeModFilename( const char *pContentFileName, char *pBuf, size_t nBufLen )
{
	char pRelativePath[ MAX_PATH ];
	if ( !g_pFullFileSystem->FullPathToRelativePathEx( pContentFileName, "CONTENTROOT", pRelativePath, sizeof(pRelativePath) ) )
	{
		Q_strncpy( pBuf, pContentFileName, (int)nBufLen );
		return;
	}

	char pGameRoot[ MAX_PATH ];
	g_pFullFileSystem->GetSearchPath( "GAMEROOT", false, pGameRoot, sizeof(pGameRoot) );
	char *pSemi = strchr( pGameRoot, ';' );
	if ( pSemi )
	{
		*pSemi = 0;
	}

	Q_ComposeFileName( pGameRoot, pRelativePath, pBuf, (int)nBufLen );
}


//-----------------------------------------------------------------------------
// Generates a filename under the 'content' subdirectory given a subdirectory of 'game'
//-----------------------------------------------------------------------------
void ComputeModContentFilename( const char *pGameFileName, char *pBuf, size_t nBufLen )
{
	char pRelativePath[ MAX_PATH ];
	if ( !g_pFullFileSystem->FullPathToRelativePathEx( pGameFileName, "GAMEROOT", pRelativePath, sizeof(pRelativePath) ) )
	{
		Q_strncpy( pBuf, pGameFileName, (int)nBufLen );
		return;
	}

	char pContentRoot[ MAX_PATH ];
	g_pFullFileSystem->GetSearchPath( "CONTENTROOT", false, pContentRoot, sizeof(pContentRoot) );
	char *pSemi = strchr( pContentRoot, ';' );
	if ( pSemi )
	{
		*pSemi = 0;
	}

	Q_ComposeFileName( pContentRoot, pRelativePath, pBuf, (int)nBufLen );
}


//-----------------------------------------------------------------------------
// Purpose: Generates an Xbox 360 filename from a PC filename
//-----------------------------------------------------------------------------
char *CreateX360Filename( const char *pSourceName, char *pTargetName, int targetLen )
{
	Q_StripExtension( pSourceName, pTargetName, targetLen );
	int idx = Q_strlen( pTargetName );

	// restore extension
	Q_snprintf( pTargetName, targetLen, "%s.360%s", pTargetName, &pSourceName[idx] );

	return pTargetName;
}

//-----------------------------------------------------------------------------
// Purpose: Generates a PC filename from a possible 360 name.
// Strips the .360. from filename.360.extension.
// Filenames might have multiple '.', need to be careful and only consider the
// last true extension. Complex filenames do occur:
// d:\foo\.\foo.dat
// d:\zip0.360.zip\foo.360.dat
// Returns source if no change needs to occur, othwerwise generates and
// returns target.
//-----------------------------------------------------------------------------
char *RestoreFilename( const char *pSourceName, char *pTargetName, int targetLen )
{
	// find extension
	// scan backward for '.', but not past a seperator
	int end = V_strlen( pSourceName ) - 1;
	while ( end > 0 && pSourceName[end] != '.' && !( pSourceName[end] == '\\' || pSourceName[end] == '/' ) )
	{
		--end;
	}

	if ( end >= 4 && pSourceName[end] == '.' && !V_strncmp( pSourceName + end - 4 , ".360", 4 ) )
	{
		// cull the .360, leave the trailing extension
		end -= 4;
		int length = MIN( end + 1, targetLen );
		V_strncpy( pTargetName, pSourceName, length );
		V_strncat( pTargetName, pSourceName + end + 4, targetLen );

		return pTargetName;
	}
	
	// source filename is as expected
	return (char *)pSourceName;
}

//-----------------------------------------------------------------------------
// Generate an Xbox 360 file if it doesn't exist or is out of date. This function determines
// the source and target path and whether the file needs to be generated. The caller provides
// a callback function to do the actual creation of the 360 file. "pExtraData" is for the caller to
// pass the address of any data that the callback function may need to access. This function
// is ONLY to be called by caller's who expect to have 360 versions of their file.
//-----------------------------------------------------------------------------
int UpdateOrCreate( const char *pSourceName, char *pTargetName, int targetLen, const char *pPathID, CreateCallback_t pfnCreate, bool bForce, void *pExtraData )
{
	if ( pTargetName )
	{
		// caller could supply source as PC or 360 name, we want the PC filename
		char szFixedSourceName[MAX_PATH];
		pSourceName = RestoreFilename( pSourceName, szFixedSourceName, sizeof( szFixedSourceName ) );
		// caller wants us to provide 360 named version of source
		CreateX360Filename( pSourceName, pTargetName, targetLen );
	}

	// no conversion are performed by the game at runtime anymore
	// SMB access was removed by the XDK for Vista....
	return UOC_NOT_CREATED;
}


//-----------------------------------------------------------------------------
// Returns the search path as a list of paths
//-----------------------------------------------------------------------------
void GetSearchPath( CUtlVector< CUtlString > &path, const char *pPathID )
{
	int nMaxLen = g_pFullFileSystem->GetSearchPath( pPathID, false, NULL, 0 );
	char *pBuf = (char*)stackalloc( nMaxLen );
	g_pFullFileSystem->GetSearchPath( pPathID, false, pBuf, nMaxLen );

	char *pSemi;
	while ( NULL != ( pSemi = strchr( pBuf, ';' ) ) )
	{
		*pSemi = 0;
		path.AddToTail( pBuf );
		pBuf = pSemi + 1;
	}
	path.AddToTail( pBuf );
}

//-----------------------------------------------------------------------------
// Given file name in the current dir generate a full path to it.
//-----------------------------------------------------------------------------
bool GenerateFullPath( const char *pFileName, char const *pPathID, char *pBuf, int nBufLen )
{
	if ( V_IsAbsolutePath( pFileName ) )
	{
		V_strncpy( pBuf, pFileName, nBufLen );
		return true;
	}

	const char *pFullPath = g_pFullFileSystem->RelativePathToFullPath( pFileName, pPathID, pBuf, nBufLen );
	if ( pFullPath && Q_IsAbsolutePath( pFullPath ) )
		return true;

	char pDir[ MAX_PATH ];
	if ( !g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof( pDir ) ) )
		return false;

	V_ComposeFileName( pDir, pFileName, pBuf, nBufLen );
	V_RemoveDotSlashes( pBuf );
	return true;
}

//-----------------------------------------------------------------------------
// Builds a list of all files under a directory with a particular extension
//-----------------------------------------------------------------------------
void AddFilesToList( CUtlVector< CUtlString > &list, const char *pDirectory, const char *pPathID, const char *pExtension )
{
	char pSearchString[MAX_PATH];
	Q_snprintf( pSearchString, MAX_PATH, "%s\\*", pDirectory );

	bool bIsAbsolute = Q_IsAbsolutePath( pDirectory );

	// get the list of files
	FileFindHandle_t hFind;
	const char *pFoundFile = g_pFullFileSystem->FindFirstEx( pSearchString, pPathID, &hFind );

	// add all the items
	CUtlVector< CUtlString > subDirs;
	for ( ; pFoundFile; pFoundFile = g_pFullFileSystem->FindNext( hFind ) )
	{
		char pChildPath[MAX_PATH];
		Q_snprintf( pChildPath, MAX_PATH, "%s\\%s", pDirectory, pFoundFile );

		if ( g_pFullFileSystem->FindIsDirectory( hFind ) )
		{
			if ( Q_strnicmp( pFoundFile, ".", 2 ) && Q_strnicmp( pFoundFile, "..", 3 ) )
			{
				subDirs.AddToTail( pChildPath );
			}
			continue;
		}

		// Check the extension matches
		const char *pExt = Q_GetFileExtension( pFoundFile );
		if ( !pExt || Q_stricmp( pExt, pExtension ) != 0 )
			continue;

		char pFullPathBuf[MAX_PATH];
		char *pFullPath = pFullPathBuf;
		if ( !bIsAbsolute )
		{
			g_pFullFileSystem->RelativePathToFullPath( pChildPath, pPathID, pFullPathBuf, sizeof(pFullPathBuf) );
		}
		else
		{
			pFullPath = pChildPath;
		}

		V_strlower( pFullPath );
		Q_FixSlashes( pFullPath );
		list.AddToTail( pFullPath );
	}

	g_pFullFileSystem->FindClose( hFind );

	int nCount = subDirs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		AddFilesToList( list, subDirs[i], pPathID, pExtension );
	}
}

void CBaseFile::ReadLines( CUtlStringList &lineList, int nMaxLineLength )
{
	char *pLine = ( char * ) stackalloc( nMaxLineLength );
	while( ReadLine( pLine, nMaxLineLength ) )
	{
		char *pEOL = strchr( pLine, '\n' );					// kill the \n
		if ( pEOL )
			*pEOL = 0;
		lineList.CopyAndAddToTail( pLine );
	}
}

void CBaseFile::ReadFile( CUtlBuffer &fileData )
{
	int nFileSize = Size();
	fileData.EnsureCapacity( Size() );
	int nSize = Read( fileData.Base(), nFileSize );
	fileData.SeekPut( CUtlBuffer::SEEK_HEAD, nSize );
}


