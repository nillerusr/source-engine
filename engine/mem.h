//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined( MEM_H )
#define MEM_H
#ifdef _WIN32
#pragma once
#endif

void *Mem_Malloc( size_t size );
void *Mem_ZeroMalloc( size_t size );
void *Mem_Calloc( int num, size_t size );
void *Mem_Realloc( void *memblock, size_t size );
char *Mem_Strdup( const char *strSource );
void Mem_Free( void *p );

#endif  // MEM_H