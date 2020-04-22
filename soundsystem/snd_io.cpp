//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "soundsystem.h"
#include "tier2/riff.h"
#include "filesystem.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Implements Audio IO on the engine's COMMON filesystem
//-----------------------------------------------------------------------------
class COM_IOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName );
	int read( void *pOutput, int size, int file );
	void seek( int file, int pos );
	unsigned int tell( int file );
	unsigned int size( int file );
	void close( int file );
};


// prepend sound/ to the filename -- all sounds are loaded from the sound/ directory
int COM_IOReadBinary::open( const char *pFileName )
{
	char namebuffer[512];
	FileHandle_t hFile;

	Q_strncpy(namebuffer, "sound", sizeof( namebuffer ) );

	//HACK HACK HACK  the server is sending back sound names with slashes in front... 
	if (pFileName[0]!='/')
	{
		Q_strncat(namebuffer,"/", sizeof( namebuffer ), COPY_ALL_CHARACTERS );
	}

	Q_strncat( namebuffer, pFileName, sizeof( namebuffer ), COPY_ALL_CHARACTERS );

	hFile = g_pFullFileSystem->Open( namebuffer, "rb", "GAME" );

	return (int)hFile;
}

int COM_IOReadBinary::read( void *pOutput, int size, int file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Read( pOutput, size, (FileHandle_t)file );
}

void COM_IOReadBinary::seek( int file, int pos )
{
	if ( !file )
		return;

	g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int COM_IOReadBinary::tell( int file )
{
	if ( !file )
		return 0;
	return g_pFullFileSystem->Tell( (FileHandle_t)file );
}

unsigned int COM_IOReadBinary::size( int file )
{
	if (!file)
		return 0;
	return g_pFullFileSystem->Size( (FileHandle_t)file );
}

void COM_IOReadBinary::close( int file )
{
	if (!file)
		return;

	g_pFullFileSystem->Close( (FileHandle_t)file );
}

static COM_IOReadBinary io;
IFileReadBinary *g_pSndIO = &io;

