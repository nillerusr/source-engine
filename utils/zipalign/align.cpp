//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Processes .zip directories so every file is aligned to a user-defined sector boundary.
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zip_uncompressed.h"
#include "UtlBuffer.h"
#include "zip_utils.h"

typedef unsigned int	uint;
typedef unsigned short	ushort;

// Defined constant values
static const uint DEF_SECTOR_SIZE		=	512;		// in bytes. (512 is xbox HD sector size)
static const uint FILE_HEADER_SIG		=	0x04034b50;
static const uint DIRECTORY_HEADER_SIG	=	0x02014b50;
static const uint DIRECTORY_END_SIG		=	0x06054b50;
static const uint SIG_ERROR				=	0xffffffff;

// File scope globals
static FILE* g_fin				= 0;
static FILE* g_fout				= 0;
static uint	 g_sectorSize		= 0;
static uint  g_fileCt			= 0;
static uint  g_sizeCt			= 0;

// Local function declarations
static bool OpenInputStream		( const char* name, const char* mode );
static void CloseInputStream	();
static bool OpenOutputStream	( const char* name, const char* mode );
static void CloseOutputStream	();
static void FileError			();
static uint FindFilenameStart	( char* name );

//---------------------------------------------------------------
//	Returns the file header signature, which is used to determine
//	what type of header will follow.
//---------------------------------------------------------------
static uint GetHeaderSignature()
{
	fpos_t pos;
	fgetpos( g_fin, &pos );

	unsigned int sig;
	if( !fread( &sig, 4, 1, g_fin ) )
	{
		FileError();
		return SIG_ERROR;
	}

	fsetpos( g_fin, &pos );
	return sig;
}

//---------------------------------------------------------------
//	Reads in the header, name, and data for a zipped file
//---------------------------------------------------------------
static bool ReadFileData( ZIP_LocalFileHeader& fileHeader, char* filename, char*& data )
{
	// Read in the header
	if( !fread( &fileHeader, sizeof(ZIP_LocalFileHeader), 1, g_fin ) )
	{
		FileError();
		return false;
	}

	// buffer check
	if( fileHeader.fileNameLength > 256 )
	{
		printf("Error: filename too long\n");
		return false;
	}

	// Read in the filename
	if( !fread( filename, fileHeader.fileNameLength, 1, g_fin ) )
	{
		FileError();
		return false;
	}
	
	filename[fileHeader.fileNameLength] = '\0';
	printf("Reading file %s\n",filename);
	
	// Absorb any extra-field padding
	fseek( g_fin, fileHeader.extraFieldLength, SEEK_CUR );

	// NOTE: With zipped sub-directories, folders have their own header
	// with a data size of zero.  These can be ignored (and will be by
	// zip_utils anyway)
	if( fileHeader.compressedSize != 0 )
	{
		// Read in the file data
		data = new char[fileHeader.compressedSize];
		if( !fread( data, fileHeader.compressedSize, 1, g_fin ) )
		{
			FileError();
			delete [] data;
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------
//	Does all the work of aligning each file in the directory
//  to start on a sector boundary. (Determined by g_sectorSize)
//
//	.zip file format:
//		[local file header ][filename][extra field][data]
//		....
//		[directory header][filename][extra field][file comment]
//		....
//		[central directory header][end of central directory]
//---------------------------------------------------------------
static bool AlignZipDirectory( const char* infile, const char* outfile )
{
	// Open and validate the input file
	if( !OpenInputStream( infile, "rb" ) )
		return false;

	// Read the file header sig
	uint sig = GetHeaderSignature();
	if( sig == SIG_ERROR )
		return false;

	// Open and validate the output file
	if( !OpenOutputStream( outfile, "wb" ) )
		return false;

	ZIP_LocalFileHeader hdr;
	char filename[256]; // filename size field is 2 bytes, so to be safe the filename
						// length is checked before copying into this buffer.

	IZip *zip_utils = IZip::CreateZip();
	if ( !zip_utils )
		return false;

	// Process all the files in the directory
	while( sig == FILE_HEADER_SIG )
	{
		char * data = 0;  // the data buffer is allocated in ReadFileData.

		// Get the original file
		if( !ReadFileData( hdr, filename, data ) )
			return false;
		
		if( data )
		{
			// Add to the zip file
			zip_utils->AddBufferToZip( filename, data, hdr.compressedSize, false );

			// Update counters
			g_fileCt++;
			g_sizeCt += hdr.compressedSize;
		}

		delete data;

		// Read the next file header sig
		sig = GetHeaderSignature();
		if( sig == SIG_ERROR )
			return false;
	}

	// Set the alignment size
	zip_utils->ForceAlignment( true, true, g_sectorSize );

	// Write the padded zip out to disk
	printf("\nWriting aligned directory %s to disk\n",outfile);
	zip_utils->SaveToDisk( g_fout );

	IZip::ReleaseZip( zip_utils );

	// Alignment complete
	printf("\n%d Files successfully aligned.\n\n",g_fileCt);
	
	return true;
}

//---------------------------------------------------------------
//	Main function
//---------------------------------------------------------------
void main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		printf("\n");
		printf("  Usage: zipalign {input filename} [output filename] [sector size]\n\n");
		printf("  output filename: default = input filename prepended with \"a_\"\n");
		printf("  sector size: minimum sector size (in bytes) to align to. Default = %d\n\n",DEF_SECTOR_SIZE);
		return;
	}

	// Check the extension
	size_t ext = strlen( argv[1] ) - 4;
	if( strcmp( &argv[1][ext], ".zip" ) )
	{
		printf("Input file must be a .zip\n\n");
		return;
	}

	// Get the sector size
	g_sectorSize = DEF_SECTOR_SIZE;
	if( argc > 3 )
	{
		g_sectorSize = atoi(argv[3]);
	}

	if( g_sectorSize <= 1 || ( g_sectorSize & (g_sectorSize - 1) ) )
	{
		printf("Invalid sector size.\n\n");
		return;
	}

	// Call the align function
	bool success = false;
	if( argc > 2 )
	{
		success = AlignZipDirectory( argv[1], argv[2] );
	}
	else
	{
		// Construct an output filename by prepending "a_" (skip over path portion)
		char* outfile = new char[ strlen( argv[1] ) + 3 ];
		const uint idx = FindFilenameStart( argv[1] );
		strcpy( outfile, argv[1] );
		outfile[idx] = 'a';
		outfile[idx+1] = '_';
		strcpy( &outfile[idx+2], &argv[1][idx] );

		success = AlignZipDirectory( argv[1], outfile );

		delete [] outfile;
	}

	if( !success )
	{
		printf("\nAlignment failed.\n\n");
	}

	CloseInputStream();
	CloseOutputStream();
}

//---------------------------------------------------------------
//		File operations
//---------------------------------------------------------------
uint FindFilenameStart( char* name )
{
	int end = (int)strlen( name );
	while( end >= 0 && name[end] != '\\' && name[end] != '/' )
		--end;
	return end + 1;
}

bool OpenInputStream( const char* name, const char* mode )
{
	g_fin = fopen( name, mode );
	if( !g_fin )
	{
		printf("\nUnable to open the input file %s\n\n",name);
		return false;
	}

	return true;
}

void CloseInputStream()
{
	if( g_fin )
		fclose( g_fin );
}

bool OpenOutputStream( const char* name, const char* mode )
{
	g_fout = fopen( name, mode );
	if( !g_fout )
	{
		printf("\nUnable to open the output file %s\n\n",name);
		return false;
	}

	return true;
}

void CloseOutputStream()
{
	if( g_fout )
		fclose( g_fout );
}

static void FileError()
{
	if( ferror(g_fin) )
		printf("\nError reading the file.\n\n");
	else if( feof(g_fin) )
		printf("\nError: Unexpected end of file found.\n\n");
	else
		printf("\nUnknown file error.\n\n");
}

