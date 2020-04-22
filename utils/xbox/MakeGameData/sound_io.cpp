//========= Copyright Valve Corporation, All rights reserved. ============//
/*****************************************************************************
	SOUND_IO.CPP

	IO class for RIFF
*****************************************************************************/
#include "../toollib/toollib.h"
#include "tier2/riff.h"

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


int COM_IOReadBinary::open( const char *pFileName )
{
	int hFile = -1;

	_sopen_s( &hFile, pFileName, _O_RDONLY|_O_BINARY, _SH_DENYWR, _S_IREAD );

	return hFile;
}

int COM_IOReadBinary::read( void *pOutput, int size, int file )
{
	return _read( file, pOutput, size );
}

void COM_IOReadBinary::seek( int file, int pos )
{
	_lseek( file, pos, SEEK_SET );
}

unsigned int COM_IOReadBinary::tell( int file )
{
	return _lseek( file, 0, SEEK_CUR );
}

unsigned int COM_IOReadBinary::size( int file )
{
	long	pos;
	long	length;

	pos = _lseek( file, 0, SEEK_CUR );
	length = _lseek( file, 0, SEEK_END );
	_lseek( file, pos, SEEK_SET );

	return length;
}

void COM_IOReadBinary::close( int file )
{
	_close( file );
}

static COM_IOReadBinary io;
IFileReadBinary *g_pSndIO = &io;

