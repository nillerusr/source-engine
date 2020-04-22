//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FONTAMALGAM_H
#define FONTAMALGAM_H
#ifdef _WIN32
#pragma once
#endif

#include "vguifont.h"
#include "BitmapFont.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: object that holds a set of fonts in specified ranges
//-----------------------------------------------------------------------------
class CFontAmalgam
{
public:
	CFontAmalgam();
	~CFontAmalgam();

	const char *Name();
	void SetName(const char *name);

	// adds a font to the amalgam
	void AddFont( font_t *font, int lowRange, int highRange);

	// returns the font for the given character
	font_t *GetFontForChar(int ch);

	// returns the max height of the font set
	int GetFontHeight();

	// returns height requested
	int GetFontHeightRequested();

	// returns the maximum width of a character in a font
	int GetFontMaxWidth();

	// returns the flags used to make the first font
	int GetFlags(int i);

	// returns the windows name for the font
	const char *GetFontName(int i);
	const char *GetFontFamilyName(int i);

	// returns the number of fonts in this amalgam
	int GetCount();

	// returns true if this font is underlined
	bool GetUnderlined();

	// sets the scale of a bitmap font
	void SetFontScale(float sx, float sy);

	// clears the fonts
	void RemoveAll();

private:
	struct TFontRange
	{
		int lowRange;
		int highRange;
		font_t *font;
	};

	CUtlVector<TFontRange> m_Fonts;
	char m_szName[32];
	int m_iMaxWidth;
	int m_iMaxHeight;
};


#endif // FONTAMALGAM_H
