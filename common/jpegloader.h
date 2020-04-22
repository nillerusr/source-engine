//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: jpeg functions
//
//=============================================================================

#ifndef JPEGLOADER_H
#define JPEGLOADER_H

#include "tier0/platform.h"
#include "tier1/utlbuffer.h"
#include "Color.h"


// Get dimensions out of jpeg header
bool GetJpegDimensions( const byte *pubData, int cubData, uint32 &width, uint32 &height );

// Convert Jpeg data to raw RGBA, pubData is the jpeg data, cubData is it's size, buf is for output, and width/height are output as well.
// pcubUsed is an optional output param for how many bytes of data were used in the image
bool ConvertJpegToRGBA( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed = NULL );

// Convert Jpeg data to raw RGB, pubData is the jpeg data, cubData is it's size, buf is for output, and width/height are output as well. 
// pcubUsed is an optional output param for how many bytes of data were used in the image
bool ConvertJpegToRGB( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed = NULL );

// Write a Jpeg to disk, quality is 0-100
bool ConvertRGBToJpeg( const char *pchFileOut, int quality, int width, int height, CUtlBuffer &bufRGB );

// Write a Jpeg to a buffer, quality is 0-100
bool ConvertRGBToJpeg( CUtlBuffer &bufOutput, int quality, int width, int height, CUtlBuffer &bufRGB );

// Resize an RGB image using linear interpolation
bool BResizeImageRGB( CUtlBuffer &bufRGB, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight );

// Resize an RGBA image using linear interpolation
bool BResizeImageRGBA( CUtlBuffer &bufRGBA, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight );

// Convert an RGB image to RGBA with 100% opacity
bool BConvertRGBToRGBA( CUtlBuffer &bufRGB, int nWidth, int nHeight );

// Convert an RGBA image to RGB using opacity against a given solid background
bool BConvertRGBAToRGB( CUtlBuffer &bufRGB, int nWidth, int nHeight, Color colorBG = Color(255,255,255) );

#ifdef _PS3
// PS3 PSN Avatars are PNG, so we have to support some similar functions on them.  Should move
// this to it's own pngloader.h someday when we really support PNG on all platforms.

// Get dimensions out of PNG header
bool GetPNGDimensions( const byte *pubData, int cubData, uint32 &width, uint32 &height );

// Convert PNG data to raw RGBA, pubData is the PNG data, cubData is it's size, buf is for output, and width/height are output as well.
bool ConvertPNGToRGBA( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height );

#endif // _PS3

#endif // JPEGLOADER_H