//========= Copyright Valve Corporation, All rights reserved. ============//
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <assert.h>
#include "../../../public/jcalg1.h"

#define DEFAULT_BLOCK_READ_SIZE (512*1024)
#define BLOCK_SIZE (16*1024)
#define WINDOW_SIZE (16*1024)

static unsigned g_BlockReadSize = DEFAULT_BLOCK_READ_SIZE;


static void * __stdcall jcalgAlloc(DWORD size)
{
	return malloc(size);
}

static bool __stdcall jcalgDealloc(void* pointer)
{
	free(pointer);
	return true;
}

void decompress( char* filenameIn, char* filenameOut )
{
	FILE* hIn = fopen( filenameIn, "rb" );
	if( !hIn )
	{
		printf("Failed to open input file: %s\n",filenameIn);
		return;
	}

	FILE* hOut = fopen( filenameOut, "wb+" );
	if( !hIn )
	{
		printf("Failed to open output file: %s\n",filenameOut); 
		fclose(hIn);
		return;
	}

	char* inputBuffer = (char*)malloc( g_BlockReadSize );
	
	xCompressHeader header;
	fread(&header,1,sizeof(header), hIn );
	fseek(hIn, 0, SEEK_SET);

	char* outputBuffer = (char *)malloc( header.nDecompressionBufferSize );
	

	for(;;)
	{

		// Read in a buffer full of compressed data.
		unsigned bytesRead = (unsigned)fread(inputBuffer,1,g_BlockReadSize, hIn );
		if( !bytesRead )
			break;

		unsigned outputBufferLength;

		xCompressHeader* header = (xCompressHeader*)inputBuffer;
		if( header->nMagic == xCompressHeader::MAGIC
			&& header->VERSION == xCompressHeader::VERSION )
		{
			
			printf("Found header:\n"
				   "\t%i Version\n" 
				   "\t%i Uncompressed Size\n" 
				   "\t%i ReadBlockSize\n"
				   "\t%i DecompressionBufferSize\n",header->nVersion, header->nUncompressedFileSize,header->nReadBlockSize,header->nDecompressionBufferSize );

			outputBufferLength = JCALG1_Decompress_Formatted_Buffer( bytesRead - sizeof(*header), inputBuffer + sizeof(*header), g_BlockReadSize * 8, outputBuffer );

		}
		else
		{
			outputBufferLength = JCALG1_Decompress_Formatted_Buffer( bytesRead, inputBuffer, g_BlockReadSize * 8, outputBuffer );
		}

		assert(0xFFFFFFFF != outputBufferLength );
		fwrite( outputBuffer,1,outputBufferLength, hOut);

		printf("block:%u\n", outputBufferLength);
	}

	free(inputBuffer);
	free(outputBuffer);

	fclose(hIn);
	fclose(hOut);
}

void compressSimple(char* filenameIn, char* filenameOut )
{
	FILE* hIn = fopen( filenameIn, "rb" );
	if( !hIn )
	{
		printf("Failed to open input file: %s\n",filenameIn);
		return;
	}

	fseek(hIn, 0, SEEK_END);
	unsigned uncompressedSize = ftell(hIn);
	fseek(hIn, 0, SEEK_SET);

	char* uncompressedData = (char*)malloc( uncompressedSize );
	fread( uncompressedData,1,uncompressedSize, hIn );
	fclose(hIn);

	char* compressedData = (char*)malloc( uncompressedSize * 4 + sizeof( xCompressSimpleHeader ));
	int compressedSize = JCALG1_Compress(
		uncompressedData, 
		uncompressedSize,
		compressedData + sizeof( xCompressSimpleHeader ),
		uncompressedSize,
		jcalgAlloc,
		jcalgDealloc,
		NULL,
		true);

	xCompressSimpleHeader* header = (xCompressSimpleHeader*)compressedData;
	header->nMagic = xCompressSimpleHeader::MAGIC;
	header->nUncompressedSize = uncompressedSize;
	compressedSize += sizeof( xCompressSimpleHeader );

	
	printf("uncompressed size: %uk, compressedSize = %uk\n",uncompressedSize/1024, compressedSize / 1024);
	
	FILE* hOut = fopen( filenameOut, "wb+" );
	if( !hIn )
	{
		printf("Failed to open output file: %s\n",filenameOut); 
		fclose(hIn);
		return;
	}
	fwrite( compressedData, 1, compressedSize, hOut );
	fclose( hOut );
}

void decompressSimple(char* filenameIn, char* filenameOut )
{
	FILE* hIn = fopen( filenameIn, "rb" );
	if( !hIn )
	{
		printf("Failed to open input file: %s\n",filenameIn);
		return;
	}

	fseek(hIn, 0, SEEK_END);
	unsigned compressedSize = ftell(hIn);
	fseek(hIn, 0, SEEK_SET);

	char* compressedData = (char*)malloc( compressedSize );
	fread( compressedData,1,compressedSize, hIn );
	fclose(hIn);

	char* decompressedData = (char*)malloc( ((xCompressSimpleHeader*)compressedData)->nUncompressedSize );

	unsigned decompressedSize = JCALG1_Decompress_Simple_Buffer( compressedData, decompressedData );
	FILE* hOut = fopen( filenameOut, "wb+" );
	if( !hIn )
	{
		printf("Failed to open output file: %s\n",filenameOut); 
		fclose(hIn);
		return;
	}
	fwrite( decompressedData, 1, decompressedSize, hOut );
	fclose( hOut );
}


int main( int argc, char* argv[] )
{
	if( argc < 4 || argc > 5)
	{
		puts("USAGE: xcompress [c|d|cs|ds] <inputfile> <outputfile> [readsizekb]");
		puts("\nIf cs is specified, a 'simple' archive is created.  (unaligned and decompresses the entire thing to a buffer)");
		return EXIT_FAILURE;
	}

	if( !strcmpi(argv[1],"d") )
	{
		decompress( argv[2], argv[3] );
		return EXIT_SUCCESS;

	}

	if( !strcmpi(argv[1],"cs") )
	{
		compressSimple(argv[2], argv[3]);
		return EXIT_SUCCESS;
	}

	if( !strcmpi(argv[1],"ds") )
	{
		decompressSimple(argv[2], argv[3]);
		return EXIT_SUCCESS;
	}


	FILE* hIn = fopen( argv[2], "rb" );
	if( !hIn )
	{
		printf("Failed to open input file: %s\n",argv[2]);
		return EXIT_FAILURE;
	}
	fseek( hIn, 0, SEEK_END );
	unsigned nInputLength = ftell( hIn );
	fseek( hIn, 0, SEEK_SET );

	// Grab
	if( argc >= 5 )
	{
		g_BlockReadSize = atoi(argv[4]) * 1024;
		if( g_BlockReadSize <= 0 )
		{
			printf("Invalid block read size! %s\n", argv[4]);
			return EXIT_FAILURE;
		}
	}

	printf("\nOptimized for read block size: %u\n",g_BlockReadSize);

	FILE* hOut = fopen( argv[3], "wb+" );
	if( !hIn )
	{
		printf("Failed to open output file: %s\n",argv[3]);
		fclose(hIn);
		return EXIT_FAILURE;
	}

	unsigned char inputBuffer[BLOCK_SIZE];
	unsigned char outputBuffer[BLOCK_SIZE * 2];
	unsigned bytesThisBlock = 0,			// Total output bytes this block
			 inputBytesThisBlock = 0,		// Total input bytes this block
			 totalBytes = 0,
			 inputBytes = 0;

	// Set up and write out the header;
	xCompressHeader header;
	header.nMagic = xCompressHeader::MAGIC;
	header.nVersion = xCompressHeader::VERSION;
	header.nUncompressedFileSize = nInputLength;
	header.nReadBlockSize = g_BlockReadSize;
	header.nDecompressionBufferSize = 0;
	header.nWindowSize = WINDOW_SIZE;

	totalBytes = bytesThisBlock = inputBytesThisBlock = sizeof(header);
	fwrite(&header,1,sizeof(header),hOut);
	

	

	while(1)
	{
		// Read an input buffer full of data:
		size_t bytesRead = fread(inputBuffer,1,sizeof(inputBuffer),hIn);
		if( !bytesRead )
			break;

		inputBytes += (unsigned)bytesRead;
		
		unsigned compressedSize = JCALG1_Compress(
									inputBuffer,
									(unsigned)bytesRead,
									outputBuffer + sizeof(short),
									16384,
									jcalgAlloc,
									jcalgDealloc,
									NULL,
									true);

		unsigned outputBufferSize;

		// If we couldn't compress this block, just write it out:
		if( compressedSize == 0 )
		{
			outputBufferSize = (unsigned)(bytesRead + sizeof(unsigned short));
			*((unsigned short*)outputBuffer) = ((unsigned short)bytesRead) | 0x8000;
			memcpy(outputBuffer+2,inputBuffer,bytesRead);
		}
		// Tag the compression header onto this block:
		else
		{
			outputBufferSize = compressedSize + sizeof(unsigned short);
			*((unsigned short*)outputBuffer) = compressedSize;
		}

		// Do we have enough room in this chunk to fit this buffer?
		if( bytesThisBlock + outputBufferSize > g_BlockReadSize )
		{
			// no, first align it:
			while( bytesThisBlock < g_BlockReadSize )
			{
				char b = 0;
				fwrite( &b, 1, sizeof(b), hOut );
				bytesThisBlock++;
				totalBytes++;
			}

			// Compute the minimum size of the decompression buffer:
			if( inputBytesThisBlock > header.nDecompressionBufferSize )
			{
				header.nDecompressionBufferSize = inputBytesThisBlock;
			}

			// Start a new block:
			bytesThisBlock = 0;
			inputBytesThisBlock = 0;
		}

		// Write the chunk out:
		fwrite(outputBuffer,1, outputBufferSize, hOut);
		inputBytesThisBlock += bytesRead;
		bytesThisBlock+=outputBufferSize;
		totalBytes+=outputBufferSize;

		static int counter =0;
		counter++;

		if( counter % 4 == 0 )
		{
			printf("\r                                                                       \rInput:%uk Output:%uk (%0.1f%%)",inputBytes / 1024,totalBytes / 1024, ( (double)totalBytes / (double)inputBytes ) * 100);
			fflush(stdout);
		}
	}

	// Grab the last block (may be the only block)Compute the minimum size of the decompression buffer:
	if( inputBytesThisBlock > header.nDecompressionBufferSize )
	{
		header.nDecompressionBufferSize = inputBytesThisBlock;
		inputBytesThisBlock = 0;
	}

	unsigned short terminator = 0;
	fwrite(&terminator, 1, sizeof(terminator), hOut );
	
	// Align the file to a 2k boundary.
	while( ( ftell(hOut) % 2048 ) != 0)
	{
		fwrite(&terminator,1,1,hOut);
	}

	// Write the header out again, this time with ideal decompression size:
	header.nDecompressionBufferSize += 128;

	fseek( hOut, 0, SEEK_SET );
	fwrite(&header,1,sizeof(header),hOut);


	printf("\n");
	fclose(hIn);
	fclose(hOut);

}