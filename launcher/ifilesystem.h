//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#ifndef IFILESYSTEM_H
#define IFILESYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"


class IFileSystem;

//-----------------------------------------------------------------------------
// Loads, unloads the file system DLL
//-----------------------------------------------------------------------------
bool FileSystem_LoadFileSystemModule( void );
void FileSystem_UnloadFileSystemModule( void );

CSysModule * FileSystem_LoadModule( const char* path );
void FileSystem_UnloadModule( CSysModule *pModule );

bool FileSystem_Init( );
void FileSystem_Shutdown( void );

// Sets the file system search path based on the game directory
bool FileSystem_SetGameDirectory( char const* pGameDir );

extern IFileSystem *g_pFileSystem;

#endif // IFILESYSTEM_H