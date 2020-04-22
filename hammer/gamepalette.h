//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef _GAMEPALETTE_H
#define _GAMEPALETTE_H

#include <d3drmdef.h>

class CGamePalette
{
public:
	CGamePalette();
	~CGamePalette();

	BOOL Create(LPCTSTR pszFile);

	void SetBrightness(float fValue);
	float GetBrightness();

	operator LOGPALETTE*()
	{ return pPalette; }
	operator D3DRMPALETTEENTRY*()
	{ return (D3DRMPALETTEENTRY*) pPalette->palPalEntry; }
	operator CPalette*()
	{ return &GDIPalette; }

private:
	float fBrightness;

	// CPalette:
	CPalette GDIPalette;

	// palette working with:
	LOGPALETTE *pPalette;
	// to convert & store in pPalette:
	LOGPALETTE *pOriginalPalette;

	// file stored in:
	CString strFile;

	// sizeof each palette:
	size_t uPaletteBytes;
};

#endif