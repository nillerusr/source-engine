//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MEM_FGETS_H )
#define MEM_FGETS_H
#ifdef _WIN32
#pragma once
#endif

char *memfgets( unsigned char *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );

#endif // MEM_FGETS_H