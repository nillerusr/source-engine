//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <memory.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Copied from sound.cpp in the DLL
char *memfgets( unsigned char *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize )
{
	int filePos = *pFilePos;
	int i, last, stop;

	// Bullet-proofing
	if ( !pMemFile || !pBuffer )
		return NULL;

	if ( filePos >= fileSize )
		return NULL;

	i = filePos;
	last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if ( last - filePos > (bufferSize-1) )
		last = filePos + (bufferSize-1);

	stop = 0;

	// Stop at the next newline (inclusive) or end of buffer
	while ( i < last && !stop )
	{
		if ( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}


	// If we actually advanced the pointer, copy it over
	if ( i != filePos )
	{
		// We read in size bytes
		int size = i - filePos;
		// copy it out
		memcpy( pBuffer, pMemFile + filePos, sizeof(unsigned char)*size );
		
		// If the buffer isn't full, terminate (this is always true)
		if ( size < bufferSize )
			pBuffer[size] = 0;

		// Update file pointer
		*pFilePos = i;
		return pBuffer;
	}

	// No data read, bail
	return NULL;
}