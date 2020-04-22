//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// CGamePalette implementation
//

#include "stdafx.h"
#include "GamePalette.h"
#include "Hammer.h"
#include "tier1/strtools.h"
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning(pop)

#pragma warning(disable:4244)

CGamePalette::CGamePalette()
{
	fBrightness = 1.0;

	uPaletteBytes = sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * 256;

	// allocate memory
	pPalette = (LOGPALETTE*) malloc(uPaletteBytes);
	pOriginalPalette = (LOGPALETTE*) malloc(uPaletteBytes);

	memset(pPalette, 0, uPaletteBytes);
	memset(pOriginalPalette, 0, uPaletteBytes);

	if(!pPalette || !pOriginalPalette)
	{
		AfxMessageBox("I couldn't allocate memory for the palette.");
		PostQuitMessage(-1);
		return;
	}

	pPalette->palVersion = 0x300;
	pPalette->palNumEntries = 256;

	pOriginalPalette->palVersion = 0x300;
	pOriginalPalette->palNumEntries = 256;

	GDIPalette.CreatePalette(pPalette);
}

CGamePalette::~CGamePalette()
{
	if(pPalette && pOriginalPalette)
	{
		// free memory
		free(pPalette);
		free(pOriginalPalette);
	}
}

BOOL CGamePalette::Create(LPCTSTR pszFile)
{
	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, pszFile, szRootDir ); 

	strFile = szFullPath;

	if( GetFileAttributes(strFile) == 0xffffffff )
		return FALSE;	// not exist

	// open file & read palette
	std::ifstream file(strFile, std::ios::binary);

	if( !file.is_open() )
		return FALSE;

	int i;
	for(i = 0; i < 256; i++)
	{
		if(file.eof())
			break;

		pOriginalPalette->palPalEntry[i].peRed = file.get();
		pOriginalPalette->palPalEntry[i].peGreen = file.get();
		pOriginalPalette->palPalEntry[i].peBlue = file.get();
		pOriginalPalette->palPalEntry[i].peFlags = D3DRMPALETTE_READONLY |
			PC_NOCOLLAPSE;
	}

	file.close();

	if(i < 256)
		return FALSE;

	// copy  into working palette
	memcpy((void*) pPalette, (void*) pOriginalPalette, uPaletteBytes);
	GDIPalette.SetPaletteEntries(0, 256, pPalette->palPalEntry);

	return TRUE;
}

static BYTE fixbyte(float fValue)
{
	if(fValue > 255.0)
		fValue = 255.0;
	if(fValue < 0)
		fValue = 0;

	return BYTE(fValue);
}

void CGamePalette::SetBrightness(float fValue)
{
	if(fValue <= 0)
		return;

	fBrightness = fValue;

	// if fValue is 1.0, memcpy
	if(fValue == 1.0)
	{
		memcpy((void*) pPalette, (void*) pOriginalPalette, uPaletteBytes);
		GDIPalette.SetPaletteEntries(0, 256, pPalette->palPalEntry);
		return;
	}

	// copy original palette to new palette, scaling by fValue
	PALETTEENTRY * pOriginalEntry;
	PALETTEENTRY * pNewEntry;

	for(int i = 0; i < 256; i++)
	{
		pOriginalEntry = &pOriginalPalette->palPalEntry[i];
		pNewEntry = &pPalette->palPalEntry[i];

		pNewEntry->peRed = fixbyte(pOriginalEntry->peRed * fBrightness);
		pNewEntry->peGreen = fixbyte(pOriginalEntry->peGreen * fBrightness);
		pNewEntry->peBlue = fixbyte(pOriginalEntry->peBlue * fBrightness);
	}

	GDIPalette.SetPaletteEntries(0, 256, pPalette->palPalEntry);
}

float CGamePalette::GetBrightness()
{
	return fBrightness;
}
