//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: png functions
//
//=============================================================================

#include "tier0/platform.h"
#include "tier1/utlbuffer.h"

// Get dimensions out of PNG header
bool GetPNGDimensions( const byte *pubPNGData, int cubPNGData, uint32 &width, uint32 &height );

// parses out a png file into an RGBA memory buffer
bool ConvertPNGToRGBA( const byte *pubPNGData, int cubPNGData, CUtlBuffer &bufOutput, int &width, int &height );


