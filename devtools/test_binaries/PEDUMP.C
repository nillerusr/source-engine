//--------------------
// PROGRAM: PEDUMP
// FILE:    PEDUMP.C
// AUTHOR:  Matt Pietrek - 1993
//--------------------
#include <windows.h>
#include <stdio.h>
#include "objdump.h"
#include "exedump.h"
#include "extrnvar.h"

// Global variables set here, and used in EXEDUMP.C and OBJDUMP.C
BOOL fShowRelocations = FALSE;
BOOL fShowRawSectionData = FALSE;
BOOL fShowSymbolTable = FALSE;
BOOL fShowLineNumbers = FALSE;

char HelpText[] = 
"PEDUMP - Win32/COFF .EXE/.OBJ file dumper - 1993 Matt Pietrek\n\n"
"Syntax: PEDUMP [switches] filename\n\n"
"  /A    include everything in dump\n"
"  /H    include hex dump of sections\n"
"  /L    include line number information\n"
"  /R    show base relocations\n"
"  /S    show symbol table\n";

//
// Open up a file, memory map it, and call the appropriate dumping routine
//
void DumpFile(LPSTR filename)
{
	HANDLE hFile;
	HANDLE hFileMapping;
	LPVOID lpFileBase;
	PIMAGE_DOS_HEADER dosHeader;
	
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
						OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
					
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		printf("Couldn't open file with CreateFile()\n");
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

	printf("Dump of file %s\n\n", filename);
	
	dosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
	if ( dosHeader->e_magic == IMAGE_DOS_SIGNATURE )
	{
		DumpExeFile( dosHeader );
	}
	else if ( (dosHeader->e_magic == 0x014C)	// Does it look like a i386
		      && (dosHeader->e_sp == 0) )		// COFF OBJ file???
	{
		// The two tests above aren't what they look like.  They're
		// really checking for IMAGE_FILE_HEADER.Machine == i386 (0x14C)
		// and IMAGE_FILE_HEADER.SizeOfOptionalHeader == 0;
			
		DumpObjFile( (PIMAGE_FILE_HEADER)lpFileBase );
	}
	else
		printf("unrecognized file format\n");
	UnmapViewOfFile(lpFileBase);
	CloseHandle(hFileMapping);
	CloseHandle(hFile);
}

//
// process all the command line arguments and return a pointer to
// the filename argument.
//
PSTR ProcessCommandLine(int argc, char *argv[])
{
	int i;
	
	for ( i=1; i < argc; i++ )
	{
		strupr(argv[i]);
		
		// Is it a switch character?
		if ( (argv[i][0] == '-') || (argv[i][0] == '/') )
		{
			if ( argv[i][1] == 'A' )
			{
				fShowRelocations = TRUE;
				fShowRawSectionData = TRUE;
				fShowSymbolTable = TRUE;
				fShowLineNumbers = TRUE;
			}
			else if ( argv[i][1] == 'H' )
				fShowRawSectionData = TRUE;
			else if ( argv[i][1] == 'L' )
				fShowLineNumbers = TRUE;
			else if ( argv[i][1] == 'R' )
				fShowRelocations = TRUE;
			else if ( argv[i][1] == 'S' )
				fShowSymbolTable = TRUE;
		}
		else	// Not a switch character.  Must be the filename
		{
			return argv[i];
		}
	}
}

int main(int argc, char *argv[])
{
	PSTR filename;
	
	if ( argc == 1 )
	{
		printf(	HelpText );
		return 1;
	}
	
	filename = ProcessCommandLine(argc, argv);
	if ( filename )
		DumpFile( filename );

	return 0;
}
