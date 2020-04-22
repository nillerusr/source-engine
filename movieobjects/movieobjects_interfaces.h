//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MOVIEOBJECTS_INTERFACE_H
#define MOVIEOBJECTS_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// typedefs that should be in platform.h
//-----------------------------------------------------------------------------
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IGlobalFlexController;


//-----------------------------------------------------------------------------
// Global interfaces used by the movieobjects library
//-----------------------------------------------------------------------------
extern IGlobalFlexController *g_pGlobalFlexController;


#endif // MOVIEOBJECTS_INTERFACE_H