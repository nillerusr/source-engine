//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// MakePtr is a macro that allows you to easily add to values (including
// pointers) together without dealing with C's pointer arithmetic.  It
// essentially treats the last two parameters as DWORDs.  The first
// parameter is used to typecast the result to the appropriate pointer type.
#define MakePtr( cast, ptr, addValue ) (cast)( (DWORD)(ptr) + (addValue) )

void DumpHeader(PIMAGE_FILE_HEADER pImageFileHeader);
void DumpOptionalHeader(PIMAGE_OPTIONAL_HEADER pImageOptionalHeader);
void DumpSectionTable(PIMAGE_SECTION_HEADER section,
					  unsigned cSections,
					  BOOL IsEXE);
BOOL LookupSymbolName(DWORD index, PSTR buffer, UINT length);
void DumpSymbolTable(PIMAGE_SYMBOL pSymbolTable, unsigned cSymbols);
LPVOID GetSectionPtr(PSTR name, PIMAGE_NT_HEADERS pNTHeader, DWORD imageBase);
PIMAGE_SECTION_HEADER GetSectionHeader(PSTR name, PIMAGE_NT_HEADERS pNTHeader);
void DumpRawSectionData(PIMAGE_SECTION_HEADER section,
						PVOID base,
						unsigned cSections);
void DumpLineNumbers(PIMAGE_LINENUMBER pln, DWORD count);
void HexDump(PBYTE ptr, DWORD length);
