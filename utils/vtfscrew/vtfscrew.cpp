//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdlib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "vtf/vtf.h"
#include "UtlBuffer.h"

int main( int argc, char **argv )
{
	if( argc != 5 )
	{
		fprintf( stderr, "Usage: vtfscreen blah.vtf 0 1 2\n" );
		exit( -1 );
	}
	const char *pSrcFilename = argv[1];
	int rOffset = atoi( argv[2] );
	int gOffset = atoi( argv[3] );
	int bOffset = atoi( argv[4] );
	CUtlBuffer srcData;

	struct	_stat buf;
	if( _stat( pSrcFilename, &buf ) == -1 )
	{
		fprintf( stderr, "Couldn't stat %s\n", pSrcFilename );
		exit( -1 );
	}

	int srcFileSize = buf.st_size;

	srcData.EnsureCapacity( srcFileSize );

	FILE *fp = fopen( pSrcFilename, "rb" );
	if( !fp )
	{
		fprintf( stderr, "Couldn't open %s\n", pSrcFilename );
		exit( -1 );
	}

	int nBytesRead = fread( srcData.Base(), 1, srcFileSize, fp );
	fclose( fp );

	srcData.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	IVTFTexture *pSrcTexture = CreateVTFTexture();
	
	pSrcTexture->Unserialize( srcData );
	ImageFormat srcFormat = pSrcTexture->Format();
	pSrcTexture->ConvertImageFormat( IMAGE_FORMAT_DEFAULT, false );

	int totalSize = pSrcTexture->ComputeTotalSize();

	unsigned char *pImageData = pSrcTexture->ImageData();
	int i;
	for( i = 0; i < totalSize; i += 4 )
	{
#if 0
		// ATI BEFORE E32003
		int r, g, b;
		r = ( int )pImageData[i+0];
		g = ( int )pImageData[i+1];
		b = ( int )pImageData[i+2];

		int grey = r + g + b;
		grey = grey / 3;
		pImageData[i+0] = 0;
		pImageData[i+1] = grey;
		pImageData[i+2] = 0;
#endif
#if 0
		// NVIDIA - first e3 build
		pImageData[i+1] = 0;
		pImageData[i+2] = 0;
#endif
#if 0
		// ATI e3 build on . .get the green component and stomp all channels with it.
		pImageData[i+0] = pImageData[i+1];
		pImageData[i+2] = pImageData[i+1];
#endif
#if 0
		// Intel . .get the blue component and stomp all channels with it.
		pImageData[i+0] = pImageData[i+2];
		pImageData[i+1] = pImageData[i+2];
#endif
		// leave alpha alone
		int tmpR, tmpG, tmpB;
		tmpR = pImageData[i+rOffset];
		tmpG = pImageData[i+gOffset];
		tmpB = pImageData[i+bOffset];
		pImageData[i+0] = tmpR;
		pImageData[i+1] = tmpG;
		pImageData[i+2] = tmpB;
	}

	pSrcTexture->ConvertImageFormat( srcFormat, false );
	CUtlBuffer dstData;

	pSrcTexture->Serialize( dstData );

	fp = fopen( pSrcFilename, "wb" );
	fwrite( dstData.Base(), 1, dstData.TellPut(), fp );
	fclose( fp );

	return 0;
}
