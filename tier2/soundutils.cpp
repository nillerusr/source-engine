//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper methods + classes for sound
//
//===========================================================================//

#include "tier2/soundutils.h"
#include "tier2/riff.h"
#include "tier2/tier2.h"
#include "filesystem.h"

#ifdef IS_WINDOWS_PC

#include <windows.h> // WAVEFORMATEX, WAVEFORMAT and ADPCM WAVEFORMAT!!!
#include <mmreg.h>

#else

#ifdef _X360
#include "xbox/xbox_win32stubs.h" // WAVEFORMATEX, WAVEFORMAT and ADPCM WAVEFORMAT!!!
#endif

#endif

//-----------------------------------------------------------------------------
// RIFF reader/writers that use the file system
//-----------------------------------------------------------------------------
class CFSIOReadBinary : public IFileReadBinary
{
public:
	// inherited from IFileReadBinary
	virtual int open( const char *pFileName );
	virtual int read( void *pOutput, int size, int file );
	virtual void seek( int file, int pos );
	virtual unsigned int tell( int file );
	virtual unsigned int size( int file );
	virtual void close( int file );
};

class CFSIOWriteBinary : public IFileWriteBinary
{
public:
	virtual int create( const char *pFileName );
	virtual int write( void *pData, int size, int file );
	virtual void close( int file );
	virtual void seek( int file, int pos );
	virtual unsigned int tell( int file );
};


//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CFSIOReadBinary s_FSIoIn;
static CFSIOWriteBinary s_FSIoOut;

IFileReadBinary *g_pFSIOReadBinary = &s_FSIoIn;
IFileWriteBinary *g_pFSIOWriteBinary = &s_FSIoOut;


//-----------------------------------------------------------------------------
// RIFF reader that use the file system
//-----------------------------------------------------------------------------
int CFSIOReadBinary::open( const char *pFileName )
{
	return (int)g_pFullFileSystem->Open( pFileName, "rb" );
}

int CFSIOReadBinary::read( void *pOutput, int size, int file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Read( pOutput, size, (FileHandle_t)file );
}

void CFSIOReadBinary::seek( int file, int pos )
{
	if ( !file )
		return;

	g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int CFSIOReadBinary::tell( int file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Tell( (FileHandle_t)file );
}

unsigned int CFSIOReadBinary::size( int file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Size( (FileHandle_t)file );
}

void CFSIOReadBinary::close( int file )
{
	if ( !file )
		return;

	g_pFullFileSystem->Close( (FileHandle_t)file );
}


//-----------------------------------------------------------------------------
// RIFF writer that use the file system
//-----------------------------------------------------------------------------
int CFSIOWriteBinary::create( const char *pFileName )
{
	g_pFullFileSystem->SetFileWritable( pFileName, true );
	return (int)g_pFullFileSystem->Open( pFileName, "wb" );
}

int CFSIOWriteBinary::write( void *pData, int size, int file )
{
	return g_pFullFileSystem->Write( pData, size, (FileHandle_t)file );
}

void CFSIOWriteBinary::close( int file )
{
	g_pFullFileSystem->Close( (FileHandle_t)file );
}

void CFSIOWriteBinary::seek( int file, int pos )
{
	g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int CFSIOWriteBinary::tell( int file )
{
	return g_pFullFileSystem->Tell( (FileHandle_t)file );
}


#ifndef POSIX
//-----------------------------------------------------------------------------
// Returns the duration of a wav file
//-----------------------------------------------------------------------------
float GetWavSoundDuration( const char *pWavFile )
{
	InFileRIFF riff( pWavFile, *g_pFSIOReadBinary );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
		return 0.0f;

	int nDataSize = 0;

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	char pFormatBuffer[ 1024 ];
	int nFormatSize;
	bool bFound = false;
	for ( ; walk.ChunkAvailable( ); walk.ChunkNext() )
	{
		switch ( walk.ChunkName() )
		{
		case WAVE_FMT:
			bFound = true;
			if ( walk.ChunkSize() > sizeof(pFormatBuffer) )
			{
				Warning( "oops, format tag too big!!!" );
				return 0.0f;
			}

			nFormatSize = walk.ChunkSize();
			walk.ChunkRead( pFormatBuffer );
			break;

		case WAVE_DATA:
			nDataSize += walk.ChunkSize();
			break;
		}
	}

	if ( !bFound )
		return 0.0f;

	const WAVEFORMATEX *pHeader = (const WAVEFORMATEX *)pFormatBuffer;

	int format = pHeader->wFormatTag;

	int nBits = pHeader->wBitsPerSample;
	int nRate = pHeader->nSamplesPerSec;
	int nChannels = pHeader->nChannels;
	int nSampleSize = ( nBits * nChannels ) / 8;

	// this can never be zero -- other functions divide by this. 
	// This should never happen, but avoid crashing
	if ( nSampleSize <= 0 )
	{
		nSampleSize = 1;
	}

	int nSampleCount = 0;
	float flTrueSampleSize = nSampleSize;

	if ( format == WAVE_FORMAT_ADPCM )
	{
		nSampleSize = 1;

		ADPCMWAVEFORMAT *pFormat = (ADPCMWAVEFORMAT *)pFormatBuffer;
		int blockSize = ((pFormat->wSamplesPerBlock - 2) * pFormat->wfx.nChannels ) / 2;
		blockSize += 7 * pFormat->wfx.nChannels;

		int blockCount = nSampleCount / blockSize;
		int blockRem = nSampleCount % blockSize;

		// total samples in complete blocks
		nSampleCount = blockCount * pFormat->wSamplesPerBlock;

		// add remaining in a short block
		if ( blockRem )
		{
			nSampleCount += pFormat->wSamplesPerBlock - (((blockSize - blockRem) * 2) / nChannels);
		}

		flTrueSampleSize = 0.5f;

	}
	else
	{
		nSampleCount = nDataSize / nSampleSize;
	}

	float flDuration = (float)nSampleCount / (float)nRate;
	return flDuration;
}
#endif
