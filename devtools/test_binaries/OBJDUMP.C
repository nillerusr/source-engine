//--------------------
// PROGRAM: PEDUMP
// FILE:    OBJDUMP.C
// AUTHOR:  Matt Pietrek - 1993
//--------------------
#include <windows.h>
#include <stdio.h>
#include "common.h"
#include "extrnvar.h"

typedef struct _i386RelocTypes
{
	WORD type;
	PSTR name;
} i386RelocTypes;

// ASCII names for the various relocations used in i386 COFF OBJs
i386RelocTypes i386Relocations[] = 
{
{ IMAGE_REL_I386_ABSOLUTE, "ABSOLUTE" },
{ IMAGE_REL_I386_DIR16, "DIR16" },
{ IMAGE_REL_I386_REL16, "REL16" },
{ IMAGE_REL_I386_DIR32, "DIR32" },
{ IMAGE_REL_I386_DIR32NB, "DIR32NB" },
{ IMAGE_REL_I386_SEG12, "SEG12" },
{ IMAGE_REL_I386_SECTION, "SECTION" },
{ IMAGE_REL_I386_SECREL, "SECREL" },
{ IMAGE_REL_I386_REL32, "REL32" }
};
#define I386RELOCTYPECOUNT (sizeof(i386Relocations) / sizeof(i386RelocTypes))

//
// Given an i386 OBJ relocation type, return its ASCII name in a buffer
//
void GetObjRelocationName(WORD type, PSTR buffer, DWORD cBytes)
{
	DWORD i;
	
	for ( i=0; i < I386RELOCTYPECOUNT; i++ )
		if ( type == i386Relocations[i].type )
		{
			strncpy(buffer, i386Relocations[i].name, cBytes);
			return;
		}
		
	wsprintf( buffer, "???_%X", type);
}

//
// Dump the relocation table for one COFF section
//
void DumpObjRelocations(PIMAGE_RELOCATION pRelocs, DWORD count)
{
	DWORD i;
	char szTypeName[32];
	
	for ( i=0; i < count; i++ )
	{
		GetObjRelocationName(pRelocs->Type, szTypeName, sizeof(szTypeName));
		printf("  Address: %08X  SymIndex: %08X  Type: %s\n",
				pRelocs->VirtualAddress, pRelocs->SymbolTableIndex,
				szTypeName);
		pRelocs++;
	}
}

//
// top level routine called from PEDUMP.C to dump the components of a
// COFF OBJ file.
//
void DumpObjFile( PIMAGE_FILE_HEADER pImageFileHeader )
{
	unsigned i;
	PIMAGE_SECTION_HEADER pSections;
	
	DumpHeader(pImageFileHeader);
	printf("\n");

	pSections = (PIMAGE_SECTION_HEADER)(pImageFileHeader+1);

	DumpSectionTable(pSections, pImageFileHeader->NumberOfSections, FALSE);
	printf("\n");

	if ( fShowRelocations )
	{
		for ( i=0; i < pImageFileHeader->NumberOfSections; i++ )
		{
			if ( pSections[i].PointerToRelocations == 0 )
				continue;
		
			printf("Section %02X (%.8s) relocations\n", i, pSections[i].Name);
			DumpObjRelocations( MakePtr(PIMAGE_RELOCATION, pImageFileHeader,
									pSections[i].PointerToRelocations),
								pSections[i].NumberOfRelocations );
			printf("\n");
		}
	}
	
	PCOFFSymbolTable = MakePtr(PIMAGE_SYMBOL, pImageFileHeader,
								pImageFileHeader->PointerToSymbolTable);
	COFFSymbolCount = pImageFileHeader->NumberOfSymbols;

	if ( fShowLineNumbers )
	{
		// Walk through the section table...
		for (i=0; i < pImageFileHeader->NumberOfSections; i++)
		{
			// if there's any line numbers for this section, dump'em
			if ( pSections->NumberOfLinenumbers )
			{
				DumpLineNumbers( MakePtr(PIMAGE_LINENUMBER, pImageFileHeader,
										 pSections->PointerToLinenumbers),
								 pSections->NumberOfLinenumbers );
				printf("\n");
			}
			pSections++;
		}
	}
	
	if ( fShowSymbolTable )
	{
		DumpSymbolTable(PCOFFSymbolTable, COFFSymbolCount);
		printf("\n");
	}
	
	if ( fShowRawSectionData )
	{
		DumpRawSectionData( (PIMAGE_SECTION_HEADER)(pImageFileHeader+1),
							pImageFileHeader,
							pImageFileHeader->NumberOfSections);
	}
}
