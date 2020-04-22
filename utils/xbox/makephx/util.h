//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef UTIL_H
#define UTIL_H
#ifdef _WIN32
#pragma once
#endif

extern void UTIL_StringToFloatArray( float *pVector, int count, const char *pString );
extern void UTIL_StringToVector( float *pVector, const char *pString );

#endif // UTIL_H
