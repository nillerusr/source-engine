//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef COMPRESSION_H
#define COMPRESSION_H

//----------------------------------------------------------------------------------------

#include "platform.h"

//----------------------------------------------------------------------------------------

class ICompressor
{
public:
	virtual ~ICompressor() {}
	virtual bool	Compress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen ) = 0;
	virtual bool	Decompress( char *pDest, unsigned int *pDestLen, const char *pSource, unsigned int nSourceLen ) = 0;

	virtual int		GetEstimatedCompressionSize( unsigned int nSourceLen ) = 0;
};

//----------------------------------------------------------------------------------------

enum CompressorType_t
{
	COMPRESSORTYPE_INVALID = -1,

	COMPRESSORTYPE_LZSS,
	COMPRESSORTYPE_BZ2,

	NUM_COMPRESSOR_TYPES
};

//----------------------------------------------------------------------------------------

extern const char *g_pCompressorTypes[ NUM_COMPRESSOR_TYPES ];

//----------------------------------------------------------------------------------------

ICompressor *CreateCompressor( CompressorType_t nType );
const char *GetCompressorNameSafe( CompressorType_t nType );

//----------------------------------------------------------------------------------------

#endif // COMPRESSION_H
