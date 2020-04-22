//========= Copyright Valve Corporation, All rights reserved. ============//
#include <stdlib.h>
#include <stdio.h>
#include "UtlBuffer.h"
#include "filesystem.h"
#include "filesystem_tools.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "cmdlib.h"

void Usage( void )
{
	printf( "Usage: splitskybox blah.pfm\n" );
	exit( -1 );
}

static int ReadIntFromUtlBuffer( CUtlBuffer &buf )
{
	int val = 0;
	int c;
	while( 1 )
	{
		c = buf.GetChar();
		if( c >= '0' && c <= '9' )
		{
			val = val * 10 + ( c - '0' );
		}
		else
		{
			return val;
		}
	}
}

//-----------------------------------------------------------------------------
// Loads a file into a UTLBuffer
//-----------------------------------------------------------------------------
static bool LoadFile( const char *pFileName, CUtlBuffer &buf, bool bFailOnError )
{
	FILE *fp = fopen( pFileName, "rb" );
	if (!fp)
	{
		if (!bFailOnError)
			return false;

		Error( "Can't open: \"%s\"\n", pFileName );
	}

	fseek( fp, 0, SEEK_END );
	int nFileLength = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buf.EnsureCapacity( nFileLength );
	fread( buf.Base(), 1, nFileLength, fp );
	fclose( fp );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nFileLength );
	return true;
}

static bool PFMRead( CUtlBuffer &fileBuffer, int &nWidth, int &nHeight, float **ppImage )
{
	fileBuffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if( fileBuffer.GetChar() != 'P' )
	{
		Assert( 0 );
		return false;
	}
	if( fileBuffer.GetChar() != 'F' )
	{
		Assert( 0 );
		return false;
	}
	if( fileBuffer.GetChar() != 0xa )
	{
		Assert( 0 );
		return false;
	}
	nWidth = ReadIntFromUtlBuffer( fileBuffer );
	nHeight = ReadIntFromUtlBuffer( fileBuffer );

	// eat crap until the next newline
	while( fileBuffer.GetChar() != 0xa )
	{
	}

	*ppImage = new float[nWidth * nHeight * 3];

	int y;
	for( y = nHeight-1; y >= 0; y-- )
	{
		fileBuffer.Get( *ppImage + nWidth * y * 3, nWidth * 3 * sizeof( float ) );
	}
	return true;
}

static void PFMWrite( float *pFloatImage, const char *pFilename, int width, int height )
{
	printf( "filename: %s\n", pFilename );
	FileHandle_t fp;
	fp = g_pFullFileSystem->Open( pFilename, "wb" );
	if( fp == FILESYSTEM_INVALID_HANDLE )
	{
		fprintf( stderr, "error opening \"%s\"\n", pFilename );
		exit( -1 );
	}
	g_pFullFileSystem->FPrintf( fp, "PF\n%d %d\n-1.000000\n", width, height );
	int i;
	for( i = height-1; i >= 0; i-- )
//	for( i = 0; i < height; i++ )
	{
		float *pRow = &pFloatImage[3 * width * i];
		g_pFullFileSystem->Write( pRow, width * sizeof( float ) * 3, fp );
	}
	g_pFullFileSystem->Close( fp );
}

void WriteSubRect( int nSrcWidth, int nSrcHeight, const float *pSrcImage, 
				  int nSrcOffsetX, int nSrcOffsetY, int nDstWidth, int nDstHeight, 
				  const char *pSrcFileName, const char *pDstFileNameExtension )
{
	float *pDstImage = new float[nDstWidth * nDstHeight * 3];

	int x, y;
	for( y = 0; y < nDstHeight; y++ )
	{
		for( x = 0; x < nDstWidth; x++ )
		{
			pDstImage[(x+(nDstWidth*y))*3+0] = pSrcImage[((nSrcOffsetX+x)+(nSrcOffsetY+y)*nSrcWidth)*3+0];
			pDstImage[(x+(nDstWidth*y))*3+1] = pSrcImage[((nSrcOffsetX+x)+(nSrcOffsetY+y)*nSrcWidth)*3+1];
			pDstImage[(x+(nDstWidth*y))*3+2] = pSrcImage[((nSrcOffsetX+x)+(nSrcOffsetY+y)*nSrcWidth)*3+2];
		}
	}

	char dstFileName[MAX_PATH];
	Q_strcpy( dstFileName, pSrcFileName );
	Q_StripExtension( pSrcFileName, dstFileName, MAX_PATH );
	Q_strcat( dstFileName, pDstFileNameExtension, sizeof(dstFileName) );
	Q_strcat( dstFileName, ".pfm", sizeof(dstFileName) );

	PFMWrite( pDstImage, dstFileName, nDstWidth, nDstHeight );

	delete [] pDstImage;
}

int main( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );
	if( argc != 2 )
	{
		Usage();
	}
	CmdLib_InitFileSystem( argv[ argc-1 ] );
	CUtlBuffer srcFileData;

	// Expand the path so that it can run correctly from a shortcut
	char		source[1024];
	strcpy (source, ExpandPath(argv[1]));

	LoadFile( argv[1], srcFileData, true );

	float *pSrcImage = NULL;
	int nSrcWidth, nSrcHeight;
	if( !PFMRead( srcFileData, nSrcWidth, nSrcHeight, &pSrcImage ) )
	{
		printf( "Couldn't read %s\n", argv[1] );
	}

	int nDstWidth = nSrcWidth / 4;
	int nDstHeight = nSrcHeight / 3;
	Assert( nDstWidth == nDstHeight );

	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 0, nDstHeight * 1, nDstWidth, nDstHeight, argv[1], "ft" );
	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 1, nDstHeight * 1, nDstWidth, nDstHeight, argv[1], "lf" );
	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 2, nDstHeight * 1, nDstWidth, nDstHeight, argv[1], "bk" );
	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 3, nDstHeight * 1, nDstWidth, nDstHeight, argv[1], "rt" );
	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 3, nDstHeight * 0, nDstWidth, nDstHeight, argv[1], "up" );
	WriteSubRect( nSrcWidth, nSrcHeight, pSrcImage, nDstWidth * 3, nDstHeight * 2, nDstWidth, nDstHeight, argv[1], "dn" );

	CmdLib_Cleanup();
	CmdLib_Exit( 1 );

	return 0;
}

