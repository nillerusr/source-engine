//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: image functions
//
//=============================================================================

#ifndef IMAGELOADWRAPPER_H
#define IMAGELOADWRAPPER_H

#include "jpegloader.h"
#include "tgaloader.h"
#include "pngloader.h"

// Convert any supported image type (jpeg, tga) to RGB 
bool ConvertImageToRGB( const byte *pubImageData, int cubImageData, CUtlBuffer &bufOutput, int &width, int &height );

// Convert any supported image type (jpeg, tga) to RGBA
bool ConvertImageToRGBA( const byte *pubImageData, int cubImageData, CUtlBuffer &bufOutput, int &width, int &height );

#endif // IMAGELOADWRAPPER_H