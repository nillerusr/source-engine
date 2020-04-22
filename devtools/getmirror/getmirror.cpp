//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "stdafx.h"
#include <string>
#include <ctype.h>
#include <wchar.h>
#include <assert.h>
#include <direct.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

void rmkdir( const char *pszPath )
{
	char *pszScan = const_cast<char*>(pszPath);

	if ( *pszScan == '\\' && *(pszScan + 1) == '\\' )
	{
		assert( 0 );
	}
	else if ( *pszScan && *(pszScan + 1) == ':' && *(pszScan + 2) == '\\' )
	{
		pszScan += 3;
	}

	char *pszLimit = pszScan + strlen( pszScan ) + 1;

	while ( pszScan < pszLimit )
	{
		if ( *pszScan == '\\' || *pszScan == 0 )
		{
			char temp = *pszScan;
			*pszScan = 0;
			_mkdir( pszPath );
			*pszScan = temp;
		}
		pszScan++;
	}
}


int main(int argc, char* argv[])
{
	char input[1024*16];

	string notCopied;

	if ( argc != 3 )
	{
		printf( "wrong arguments\n");
		return 1;
	}

	string sourceRoot(argv[1]);
	string workingFolder(argv[2]);

	if ( !workingFolder.length() )
		return 1;

	if ( workingFolder[workingFolder.length()] != '\\' )
		workingFolder += "\\";

	if ( !sourceRoot.length() )
		return 1;

	if ( sourceRoot[sourceRoot.length()] != '\\' )
		sourceRoot += "\\";

	int lenRoot = sourceRoot.length();

	int count = 0;
	unsigned nKBytesCopied = 0;
	time_t startTime = time(NULL);

	while ( gets(input) )
	{
		char *pszName = strstr(input, argv[1] );
		if ( !pszName )
			continue;

		if ( strlen(pszName) - lenRoot <= 0 )
			continue;

		string dest = workingFolder + ( pszName + lenRoot );
		string destDir = dest;

		destDir.erase( destDir.rfind( '\\' ) );
		rmkdir( destDir.c_str() );

		DWORD attributes = GetFileAttributes( dest.c_str() );
		if ( attributes != -1 && !(attributes & FILE_ATTRIBUTE_READONLY) )
		{
			notCopied += '\n';
			notCopied += dest;
		}
		else
		{
			if ( attributes != -1 )
				SetFileAttributes( dest.c_str(), (attributes & ~FILE_ATTRIBUTE_READONLY ) );

			printf("%s\n", dest.c_str() );
			fflush(NULL);
			if ( !CopyFile( pszName, dest.c_str(), false ) )
			{
				printf( "    Failed to copy %s!\n", dest.c_str() );
			}
			else
			{
				struct _stat fileStat;
				_stat( dest.c_str(), &fileStat );
				nKBytesCopied += fileStat.st_size / 1024;
			}
			
			attributes = GetFileAttributes( dest.c_str() );
			SetFileAttributes( dest.c_str(), (attributes | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE) );
			count++;
		}
	}

	printf("\n");
	if ( count )
	{
		printf( "%d files copied\n", count );
		printf( "%dk copied\n", nKBytesCopied );
	}

	if ( notCopied.length() )
	{
		printf( "** The following files were not copied because they are writable **\n" );
		printf( notCopied.c_str() );
		printf( "\n" );
	}

	printf("%d seconds\n", time(NULL) - startTime);

	return 0;
}

