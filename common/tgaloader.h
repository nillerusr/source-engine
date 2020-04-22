//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: tga functions
//
//=============================================================================

#include "tier0/platform.h"

// parses out a tga file into an RGBA memory buffer
bool LoadTGA( int32 iBytes, char *pData, byte **rawImage, int * rawImageBytes, int * width, int *height );

// puts a simple raw 32bpp tga onto disk
void WriteTGA( const char *pchFileName, void *rgba, int wide, int tall );
// returns the image size of a tga
bool GetTGADimensions( int32 iBytes, char *pData, int * width, int *height );
