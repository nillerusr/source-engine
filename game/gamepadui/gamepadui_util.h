#ifndef GAMEPADUI_UTIL_H
#define GAMEPADUI_UTIL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/ISurface.h"

const char *COM_GetModDirectory();

int DrawPrintWrappedText(vgui::HFont font, int pX, int pY, const wchar_t* pszText, int nLength, int nMaxWidth, bool bDraw);

int NextPowerOfTwo( int v );

#endif // GAMEPADUI_UTIL_H
