//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CLASSCHECK_UTIL_H
#define CLASSCHECK_UTIL_H
#ifdef _WIN32
#pragma once
#endif


void vprint( int depth, const char *fmt, ... );

unsigned char *COM_LoadFile( const char *name, int *len);
void COM_FreeFile( unsigned char *buffer );
bool COM_DirectoryExists( const char *dir );

#endif // CLASSCHECK_UTIL_H
