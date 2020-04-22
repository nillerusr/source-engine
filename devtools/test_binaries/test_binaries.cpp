//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// test_binaries.cpp : test for debug section
//
// Adapted from PEDUMP, AUTHOR:  Matt Pietrek - 1993
//--------------------
#include <windows.h>
#include <stdio.h>
#include "common.h"
#include "strtools.h"

bool HasSection( PIMAGE_SECTION_HEADER section, int numSections, const char *pSectionName )
{
	for ( int i = 0; i < numSections; i++ )
	{
		if ( !strnicmp( (char *)section[i].Name, pSectionName, 8 ) )
			return true;
	}

	return false;
}


void TestExeFile( const char *pFilename, PIMAGE_DOS_HEADER dosHeader )
{
	PIMAGE_NT_HEADERS pNTHeader;
	
	pNTHeader = MakePtr( PIMAGE_NT_HEADERS, dosHeader,
								dosHeader->e_lfanew );

	// First, verify that the e_lfanew field gave us a reasonable
	// pointer, then verify the PE signature.
	if ( IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) ||
	     pNTHeader->Signature != IMAGE_NT_SIGNATURE )
	{
		printf("Unhandled EXE type, or invalid .EXE (%s)\n", pFilename);
		return;
	}

	if ( HasSection( (PIMAGE_SECTION_HEADER)(pNTHeader+1), pNTHeader->FileHeader.NumberOfSections, "ValveDBG" ) )
	{
		printf("%s is a debug build\n", pFilename);
	}
}

//
// Open up a file, memory map it, and call the appropriate dumping routine
//
void TestFile(const char *pFilename)
{
	HANDLE hFile;
	HANDLE hFileMapping;
	LPVOID lpFileBase;
	PIMAGE_DOS_HEADER dosHeader;
	
	hFile = CreateFile(pFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
						OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
					
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		printf("Couldn't open file %s with CreateFile()\n", pFilename );
		return;
	}

	hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if ( hFileMapping == 0 )
	{
		CloseHandle(hFile);
		printf("Couldn't open file mapping with CreateFileMapping()\n");
		return;
	}

	lpFileBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if ( lpFileBase == 0 )
	{
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		printf("Couldn't map view of file with MapViewOfFile()\n");
		return;
	}

	dosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
	if ( dosHeader->e_magic == IMAGE_DOS_SIGNATURE )
	{
		TestExeFile( pFilename, dosHeader );
	}
#if 0
	else if ( (dosHeader->e_magic == 0x014C)	// Does it look like a i386
		      && (dosHeader->e_sp == 0) )		// COFF OBJ file???
	{
		// The two tests above aren't what they look like.  They're
		// really checking for IMAGE_FILE_HEADER.Machine == i386 (0x14C)
		// and IMAGE_FILE_HEADER.SizeOfOptionalHeader == 0;
			
		DumpObjFile( (PIMAGE_FILE_HEADER)lpFileBase );
	}
#endif
	else
		printf("unrecognized file format\n");
	UnmapViewOfFile(lpFileBase);
	CloseHandle(hFileMapping);
	CloseHandle(hFile);
}
int main(int argc, char* argv[])
{
	if ( argc < 2 )
	{
		printf("Usage: test_binaries <FILENAME>\n" );
	}
	else
	{
		char fileName[2048], dir[2048];
		if ( !Q_ExtractFilePath( argv[1], dir, sizeof( dir ) ) )
		{
			strcpy( dir, "" );
		}
		else
		{
			Q_FixSlashes( dir, '/' );
			int len = strlen(dir);
			if ( len && dir[len-1] !='/' )
			{
				strcat( dir, "/" );
			}
		}
		
		WIN32_FIND_DATA findData;
		HANDLE hFind = FindFirstFile( argv[1], &findData );
		if ( hFind == INVALID_HANDLE_VALUE )
		{
			printf("Can't find %s\n", argv[1] );
		}
		else
		{
			do
			{
				sprintf( fileName, "%s%s", dir, findData.cFileName );
				TestFile( fileName );
			} while ( FindNextFile( hFind, &findData ) );
			FindClose( hFind );
		}
	}
	return 0;
}
