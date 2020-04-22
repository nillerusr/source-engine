//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
// 
// $NoKeywords: $
//=============================================================================//
#ifndef LINUX_SUPPORT_H
#define LINUX_SUPPORT_H

#include <ctype.h> // tolower()
#include <limits.h> // PATH_MAX define 
#include <string.h> //strcmp, strcpy
#include <sys/stat.h> // stat()
#include <unistd.h> 
#include <dirent.h> // scandir()
#include <stdlib.h>
#include <stdio.h>
#include "tier0/platform.h"

#define FILE_ATTRIBUTE_DIRECTORY S_IFDIR

typedef struct 
{
	// public data
	int dwFileAttributes;
	char cFileName[PATH_MAX]; // the file name returned from the call
	char cBaseDir[PATH_MAX]; // the root dir for this find

	int numMatches;
	int curMatch;
	struct dirent **namelist;  
} FIND_DATA;

#define WIN32_FIND_DATA FIND_DATA

HANDLE FindFirstFile( const char *findName, FIND_DATA *dat);
bool FindNextFile(HANDLE handle, FIND_DATA *dat);
bool FindClose(HANDLE handle);

// findFileInDirCaseInsensitive looks for the specified file. It returns
// false if no directory separator is found. Otherwise it looks for the
// requested file by scanning the specified directory and doing a case
// insensitive match. If the file exists (in any case) then the correctly cased
// filename will be returned in the user's buffer and 'true' will be returned.
// If the file does not exist then the filename will be lowercased and 'false'
// will be returned.
// This function uses a static buffer for internal purposes and is therefore
// not thread safe, so it must only be called from the main thread.
bool findFileInDirCaseInsensitive( const char *file, OUT_Z_BYTECAP(bufSize) char* output, size_t bufSize );
// The _safe version of this function should be preferred since it always infers
// the directory size correctly.
template<size_t bufSize>
bool findFileInDirCaseInsensitive_safe( const char *file, OUT_Z_ARRAY char (&output)[bufSize] )
{
	return findFileInDirCaseInsensitive( file, output, bufSize );
}

#endif // LINUX_SUPPORT_H
