//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "compression.h"
#include "replay/ienginereplay.h"
#include "replay/replayutils.h"
#include "convar.h"
#include "filesystem.h"
#include "fmtstr.h"
#include "../utils/bzip2/bzlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

const char *g_pCompressorTypes[ NUM_COMPRESSOR_TYPES ] =
{
	"lzss",
	"bz2",
};

//----------------------------------------------------------------------------------------

class CCompressor_Lzss : public ICompressor
{
public:
	virtual bool Compress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return g_pEngine->LZSS_Compress( pDest, pDestLen, pSource, nSourceLen );
	}

	virtual bool Decompress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return g_pEngine->LZSS_Decompress( pDest, pDestLen, pSource, nSourceLen );
	}

	virtual int GetEstimatedCompressionSize( unsigned int nSourceLen )
	{
		return nSourceLen;
	}
};

//----------------------------------------------------------------------------------------

#define BZ2_DEFAULT_BLOCKSIZE100k	9	// Highest compression rate, but uses the most memory
#define BZ2_DEFAULT_WORKFACTOR		0	// Default work factor - same as using 30

//----------------------------------------------------------------------------------------

class CCompressor_Bz2 : public ICompressor
{
public:
	CCompressor_Bz2(
		int nBlockSize100k = BZ2_DEFAULT_BLOCKSIZE100k,
		int nWorkFactor = BZ2_DEFAULT_WORKFACTOR
	)
	:	m_nBlockSize100k( nBlockSize100k ),
		m_nWorkFactor( nWorkFactor )
	{
	}

	virtual bool Compress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return BZ_OK == BZ2_bzBuffToBuffCompress(
			pDest,
			pDestLen,
			const_cast< char *>( pSource ),
			nSourceLen,
			m_nBlockSize100k,
			0,					// Silent verbosity
			m_nWorkFactor
		);
	}

	virtual bool Decompress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen )
	{
		return BZ_OK == BZ2_bzBuffToBuffDecompress(
			pDest,
			pDestLen,
			const_cast< char * >( pSource ),
			nSourceLen,
			0,			// Don't use smaller decompressor (half as fast)
			0			// Quiet
		);
	}

	virtual int GetEstimatedCompressionSize( unsigned int nSourceLen )
	{
		return (int)( 1.1f * nSourceLen ) + 600;
	}

private:
	int		m_nBlockSize100k;
	int		m_nWorkFactor;
};

//----------------------------------------------------------------------------------------

ICompressor *CreateCompressor( CompressorType_t nType )
{
	switch ( nType )
	{
	case COMPRESSORTYPE_BZ2:	return new CCompressor_Bz2();
	case COMPRESSORTYPE_LZSS:	return new CCompressor_Lzss();
	}

	return NULL;
}

const char *GetCompressorNameSafe( CompressorType_t nType )
{
	if ( nType < 0 || nType >= NUM_COMPRESSOR_TYPES )
		return "Unknown compressor type";

	return g_pCompressorTypes[ nType ];
}

//----------------------------------------------------------------------------------------

#ifdef _DEBUG

CON_COMMAND( replay_testcompress, "Test compression" )
{
	if ( args.ArgC() < 3 )
	{
		Warning( "replay_testcompress <lzss|bz2> <file to compress>" );
		return;
	}

	const char *pInFilename = args[ 2 ];
	const char *pCompressionTypeName = args[ 1 ];

	CompressorType_t nCompressorType = COMPRESSORTYPE_INVALID;

	for ( int i = 0; i < (int)NUM_COMPRESSOR_TYPES; ++i )
	{
		if ( !V_stricmp( pCompressionTypeName, g_pCompressorTypes[ i ] ) )
		{
			nCompressorType = (CompressorType_t)i;
			break;
		}
	}

	if ( nCompressorType == COMPRESSORTYPE_INVALID )
	{
		Warning( "Invalid compression type specified.  Use \"bz2\" or \"lzss\"\n" );
		return;
	}

	const unsigned int nInFileSize = g_pFullFileSystem->Size( pInFilename );
	if ( !nInFileSize )
	{
		Warning( "Zero length file.\n" );
		return;
	}
	
	FileHandle_t hInFile = g_pFullFileSystem->Open( pInFilename, "rb" );
	if ( !hInFile )
	{
		Warning( "Failed to open file, %s\n", pInFilename );
		return;
	}

	char *pUncompressed = new char[ nInFileSize ];
	if ( !pUncompressed )
	{
		Warning( "Failed to alloc %u bytes\n", nInFileSize );
		return;
	}

	if ( g_pFullFileSystem->Read( pUncompressed, nInFileSize, hInFile ) != (int)nInFileSize )
	{
		Warning( "Failed to read file %s\n", pInFilename );
	}
	else
	{
		ICompressor *pCompressor = CreateCompressor( nCompressorType );
		unsigned int nCompressedSize = pCompressor->GetEstimatedCompressionSize( nInFileSize );
		char *pCompressed = new char[ nCompressedSize ];
		if ( !pCompressed )
		{
			Warning( "Failed to allocate %u bytes for compressed buffer.\n", nCompressedSize );
			return;
		}

		// Compress
		if ( !pCompressor->Compress( pCompressed, &nCompressedSize, pUncompressed, nInFileSize ) )
		{
			Warning( "Compression failed.\n" );
		}
		else
		{
			CFmtStr fmtOutFilename( "%s.%s", pInFilename, pCompressionTypeName );
			FileHandle_t hOutFile = g_pFullFileSystem->Open( fmtOutFilename.Access(), "wb+" );	
			if ( !hOutFile )
			{
				Warning( "Failed to open out file, %s\n", fmtOutFilename.Access() );
			}
			else
			{
				if ( g_pFullFileSystem->Write( pCompressed, nCompressedSize, hOutFile ) != (int)nCompressedSize )
				{
					Warning( "Failed to write compressed data to %s\n", fmtOutFilename.Access() );
				}
				else
				{
					const float flRatio = (float)nInFileSize / nCompressedSize;
					Warning( "Wrote compressed file to successfully (%s) - ratio: %.2f:1\n", fmtOutFilename.Access(), flRatio );
				}

				g_pFullFileSystem->Close( hOutFile );
			}
		}

		delete [] pCompressed;
	}

	g_pFullFileSystem->Close( hInFile );
}

#endif	// _DEBUG

//----------------------------------------------------------------------------------------