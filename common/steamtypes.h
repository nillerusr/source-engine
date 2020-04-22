//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef STEAMTYPES_H
#define STEAMTYPES_H
#ifdef _WIN32
#pragma once
#endif

// Steam-specific types. Defined here so this header file can be included in other code bases.
#ifndef WCHARTYPES_H
typedef unsigned char uint8;
#endif
#ifndef uint16
typedef unsigned short uint16;
#endif
#ifndef int32
typedef signed long int32;
#endif
#ifndef uint32
typedef unsigned long uint32;
#endif
#ifndef int64
#ifdef _WIN32
typedef __int64 int64;
#elif _LINUX
typedef long long int64;
#endif
#endif
#ifndef uint64
#ifdef _WIN32
typedef unsigned __int64 uint64;
#elif _LINUX
typedef unsigned long long uint64;
#endif
#endif

#ifndef NETADR_H
class netadr_t;
#endif

#endif // STEAMTYPES_H
