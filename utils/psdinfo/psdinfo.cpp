//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Access to PSD image resources.
//
// $NoKeywords: $
//
//=============================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tier0/platform.h"
#include "tier1/utlbuffer.h"

#include "bitmap/imageformat.h"
#include "bitmap/psd.h"

int Usage()
{
	printf( "psdinfo ver. " __DATE__ " " __TIME__ "\n" );
	printf( "Usage: \n" );
	printf( "      psdinfo [OPTIONS] psdfile.psd \n" );
	printf( "Options: \n" );
	printf( "      -read         read and print the info record (default) \n" );
	printf( "      -write        update the info record with data from pipe \n" );
	printf( "psdfile.psd         the PSD file to process. \n" );
	printf( "\n" );

	return 1;
}

// Global options
static struct Options {
	Options() :
		szFilename( "" ),
		bWriteInfo( false )
		{}

	char const *szFilename;
	bool bWriteInfo;
} s_opts;


// Map a file into a UtlBuffer
bool LoadFileAndClose( FILE *fp, CUtlBuffer &buf, int numExtraBytesAlloc = 0 )
{
	if (!fp)
		return false;

	fseek( fp, 0, SEEK_END );
	int nFileLength = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buf.EnsureCapacity( nFileLength + numExtraBytesAlloc );
	int nBytesRead = fread( buf.Base(), 1, nFileLength, fp );
	
	fclose( fp );

	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	return true;
}


// Reading the info
int ReadInfo()
{
	CUtlBuffer bufFile;
	if ( !LoadFileAndClose( fopen( s_opts.szFilename, "rb" ), bufFile ) )
		Error( "%s cannot be opened for read!\n", s_opts.szFilename );

	if ( !IsPSDFile( bufFile ) )
		Error( "%s is not a valid PSD file!\n", s_opts.szFilename );

	PSDImageResources imgres = PSDGetImageResources( bufFile );
	PSDResFileInfo resFileInfo( imgres.FindElement( PSDImageResources::eResFileInfo ) );
	PSDResFileInfo::ResFileInfoElement descr = resFileInfo.FindElement( PSDResFileInfo::eDescription );
	if ( descr.m_pvData )
	{
		unsigned char const *pvData = descr.m_pvData, *pvEnd = pvData + descr.m_numBytes;
		while ( unsigned char const *pvCR = ( unsigned char const * ) memchr( pvData, '\r', pvEnd - pvData ) )
		{
			printf( "%.*s\n", pvCR - pvData, pvData );
			pvData = pvCR + 1;
		}
		(pvEnd > pvData) ? printf( "%.*s\n", pvEnd - pvData, pvData ) : 0;
	}

	return 0;
}


struct Wr_PSDImageResources : public PSDImageResources {
	friend int WriteInfo();
	Wr_PSDImageResources( PSDImageResources const &x ) : PSDImageResources( x ) { }
};
struct Wr_PSDResFileInfo : public PSDResFileInfo {
	friend int WriteInfo();
	Wr_PSDResFileInfo( PSDResFileInfo const &x ) : PSDResFileInfo( x ) { }
};

void BufferMove( void const *pvDst, void const *pvSrc, size_t numBytes )
{
	memmove( const_cast< void * >( pvDst ), pvSrc, numBytes );
}

// Writing the info
int WriteInfo()
{
	// We will have to:
	// a) patch the "numBytesImgResource",
	// b) probably insert our own section eResFileInfo
	// c) inside the section add eDescription

	int numDeltaBytes = 0;
	CUtlBuffer inBuf;
	while ( !feof( stdin ) )
	{
		char chBuffer[4096];
		if ( !fgets( chBuffer, sizeof( chBuffer ) - 1, stdin ) )
			break;
		chBuffer[ sizeof( chBuffer ) - 1 ] = 0;
		int len = strlen( chBuffer );
		if ( len && chBuffer[ len - 1 ] == '\n' )
			chBuffer[ len - 1 ] = '\r';
		inBuf.Put( chBuffer, len );
	}
	
	if ( inBuf.TellPut() &&
		 '\r' == ( ( unsigned char const * ) inBuf.Base() )[ inBuf.TellPut() - 1 ] )
		 inBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );

	// Now once we have the info load up the PSD file
	CUtlBuffer bufFile;
	if ( !LoadFileAndClose( fopen( s_opts.szFilename, "rb" ), bufFile,
			inBuf.TellPut() + 0x100 ) ) // Having extra room for insertions
		Error( "%s cannot be opened for read!\n", s_opts.szFilename );
	unsigned char const *pvBufBase = ( unsigned char const * ) bufFile.Base();

	if ( !IsPSDFile( bufFile ) )
		Error( "%s is not a valid PSD file!\n", s_opts.szFilename );

	Wr_PSDImageResources imgres( PSDGetImageResources( bufFile ) );
	if ( !imgres.m_pvBuffer )
		Error( "%s does not have image resources to write!\n", s_opts.szFilename );

	Wr_PSDResFileInfo resFileInfo( PSDResFileInfo( imgres.FindElement( PSDImageResources::eResFileInfo ) ) );

	// Maybe the file info is missing?
	if ( !resFileInfo.m_res.m_pvData )
	{
		// Insert a standard file info
		unsigned char chStdFileInfo[] = { 0x38,0x42,0x49,0x4D,   0x04,0x04,   0,0,0,0,   0x00,0x07,   0x1C,0x02,0x00,   0x00,0x02,   0x00,0x02,   0 };
		unsigned int addSize = sizeof( chStdFileInfo );

		imgres.m_numBytes += addSize;
		( ( unsigned int * ) imgres.m_pvBuffer )[ -1 ] = BigLong( imgres.m_numBytes );
		
		BufferMove( imgres.m_pvBuffer + addSize, imgres.m_pvBuffer, pvBufBase + bufFile.TellPut() - imgres.m_pvBuffer );
		BufferMove( imgres.m_pvBuffer, chStdFileInfo, addSize );

		resFileInfo.m_res.m_eType = PSDImageResources::eResFileInfo;
		resFileInfo.m_res.m_numBytes = 7;
		resFileInfo.m_res.m_pvData = imgres.m_pvBuffer + 12;

		bufFile.SeekPut( CUtlBuffer::SEEK_CURRENT, addSize );
	}

	PSDResFileInfo::ResFileInfoElement descr = resFileInfo.FindElement( PSDResFileInfo::eDescription );

	// If the description is present
	if ( descr.m_pvData )
	{
		// Fix the buffer
		numDeltaBytes = inBuf.TellPut() - descr.m_numBytes;
		if ( numDeltaBytes )
			BufferMove( descr.m_pvData + inBuf.TellPut(), descr.m_pvData + descr.m_numBytes, pvBufBase + bufFile.TellPut() - descr.m_pvData - descr.m_numBytes );

		// Copy into the buffer
		BufferMove( descr.m_pvData, inBuf.Base(), inBuf.TellPut() );
		descr.m_numBytes += numDeltaBytes;
		( ( unsigned short * ) descr.m_pvData )[ -1 ] = BigShort( descr.m_numBytes );
	}
	else
	{
		// Need to insert the description
		numDeltaBytes = 5 + inBuf.TellPut();
		unsigned char const *pvInsertPoint = resFileInfo.m_res.m_pvData + resFileInfo.m_res.m_numBytes;
		BufferMove( pvInsertPoint + numDeltaBytes, pvInsertPoint, pvBufBase + bufFile.TellPut() - pvInsertPoint );

		// Copy into the buffer
		unsigned char signature[] = { 0x1C,0x02, PSDResFileInfo::eDescription };
		BufferMove( pvInsertPoint, signature, sizeof( signature ) );

		unsigned short usDescrBytes = inBuf.TellPut();
		* ( unsigned short * ) ( pvInsertPoint + 3 ) = BigShort( usDescrBytes );

		BufferMove( pvInsertPoint + 5, inBuf.Base(), inBuf.TellPut() );
	}

	// File size changed
	bufFile.SeekPut( CUtlBuffer::SEEK_CURRENT, numDeltaBytes );

	// Still remaining to fix: the file-info size and imgres size

	// File-info
	resFileInfo.m_res.m_numBytes += numDeltaBytes;
	if ( numDeltaBytes & 1 )	// Odd number of bytes delta
	{
		if ( resFileInfo.m_res.m_numBytes & 1 )		// It was even, becomes odd
		{
			++ numDeltaBytes;
			BufferMove( resFileInfo.m_res.m_pvData + resFileInfo.m_res.m_numBytes + 1, resFileInfo.m_res.m_pvData + resFileInfo.m_res.m_numBytes, pvBufBase + bufFile.TellPut() - resFileInfo.m_res.m_pvData - resFileInfo.m_res.m_numBytes );
			const_cast< unsigned char & > ( resFileInfo.m_res.m_pvData[ resFileInfo.m_res.m_numBytes ] ) = 0x00;
			bufFile.SeekPut( CUtlBuffer::SEEK_CURRENT, +1 );
		}
		else		// It was odd, becomes even
		{
			-- numDeltaBytes;
			BufferMove( resFileInfo.m_res.m_pvData + resFileInfo.m_res.m_numBytes, resFileInfo.m_res.m_pvData + resFileInfo.m_res.m_numBytes + 1, pvBufBase + bufFile.TellPut() - resFileInfo.m_res.m_pvData - resFileInfo.m_res.m_numBytes - 1 );
			bufFile.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
		}
	}

	( ( unsigned short * ) resFileInfo.m_res.m_pvData )[ -1 ] = BigShort( resFileInfo.m_res.m_numBytes );

	// Image resources
	imgres.m_numBytes += numDeltaBytes;
	( ( unsigned int * ) imgres.m_pvBuffer )[ -1 ] = BigLong( imgres.m_numBytes );

	// Now write the file buffer
	{
		FILE *fp = fopen( s_opts.szFilename, "wb" );
		if ( !fp )
			Error( "%s cannot be opened for update!\n", s_opts.szFilename );
		
		fwrite( bufFile.Base(), 1, bufFile.TellPut(), fp );
		fclose( fp );
	}
	
	return 0;
}


// Application entry point
int main( int argc, char **argv )
{
	// Filename is the last argument
	if ( argc <= 1 )
		return Usage();
	else
		s_opts.szFilename = argv[argc - 1];

	// Read out all the options
	for ( int iArg = 1; iArg < argc - 1; ++ iArg )
	{
		if ( !stricmp( argv[iArg], "-read" ) )
		{
			s_opts.bWriteInfo = false;
		}
		else if ( !stricmp( argv[iArg], "-write" ) )
		{
			s_opts.bWriteInfo = true;
		}
		else
		{
			printf( "Unknown option \"%s\"!\n", argv[iArg] );
			return Usage();
		}
	}

	// Go ahead and perform the corresponding read or write
	return s_opts.bWriteInfo ? WriteInfo() : ReadInfo();
}

