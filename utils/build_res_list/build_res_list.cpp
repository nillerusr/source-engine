//========= Copyright Valve Corporation, All rights reserved. ============//
// build_res_list.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <io.h>
#include <string.h>


int PrintUsage()
{
	printf( "build_res_list <source directory> <output filename>\n" );
	return 1;
}


void ScanDirectories_R( const char *pFullDir, const char *pRelDir, FILE *fp )
{
	char spec[512];
	_snprintf( spec, sizeof( spec ), "%s\\*.*", pFullDir );

	printf( "\"%s\"\n", pFullDir );

	_finddata_t findInfo;
	long handle = _findfirst( spec, &findInfo );
	if ( handle != -1 )
	{
		do
		{
			if ( !stricmp( findInfo.name, "." ) || !stricmp( findInfo.name, ".." )  )
				continue;

			char fullName[512], relName[512];
			_snprintf( fullName, sizeof( fullName ), "%s\\%s", pFullDir, findInfo.name );
			_snprintf( relName, sizeof( relName ), "%s%s%s", pRelDir, (pRelDir[0] == 0) ? "" : "\\", findInfo.name );

			if ( findInfo.attrib & _A_SUBDIR )
			{
				ScanDirectories_R( fullName, relName, fp );
			}
			else
			{
				fprintf( fp, "\"%s\"\n", relName );
			}
		} while ( !_findnext( handle, &findInfo ) );

		_findclose( handle );
	}
}


int main(int argc, char* argv[])
{
	if ( argc < 3 )
		return PrintUsage();

	const char *pSourceDir = argv[1];
	const char *pOutputFilename = argv[2];

	FILE *fp = fopen( pOutputFilename, "wt" );
	if ( !fp )
	{
		printf( "Can't open %s for writing.\n", pOutputFilename );
		return PrintUsage();
	}

	ScanDirectories_R( pSourceDir, "", fp );

	fclose( fp );
	return 0;
}

