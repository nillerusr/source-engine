//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUIFONT_H
#define VGUIFONT_H
#ifdef _WIN32
#pragma once
#endif

// Structure passed to CWin32Font::GetCharsRGBA
struct newChar_t
{
	wchar_t	wch;		// A new character to generate texture data for
	int		fontWide;	// Texel width of the character
	int		fontTall;	// Texel height of the character
	int     offset;		// Offset into the buffer given to GetCharsRGBA
};

#ifdef WIN32
#include "Win32Font.h"
typedef CWin32Font font_t;
#elif defined(OSX)
#include "osxfont.h"
typedef COSXFont font_t;
#elif defined(LINUX)
#include "linuxfont.h"
typedef CLinuxFont font_t;
#else
#error
#endif


#endif //VGUIFONT_H 