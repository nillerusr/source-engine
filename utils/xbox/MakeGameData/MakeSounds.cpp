//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360.WAV Creation
//
//=====================================================================================//

#include "MakeGameData.h"
#ifndef NO_X360_XDK
#include <XMAEncoder.h>
#endif
#include "datamap.h"
#include "sentence.h"
#include "tier2/riff.h"
#include "resample.h"
#include "xwvfile.h"

// all files are built for streaming compliance
// allows for fastest runtime loading path
// actual streaming or static state is determined by engine
#define XBOX_DVD_SECTORSIZE			2048
#define XMA_BLOCK_SIZE				2048	// must be aligned to 1024
#define MAX_CHUNKS					256

// [0,100]
#define XMA_HIGH_QUALITY			90
#define XMA_DEFAULT_QUALITY			75
#define XMA_MEDIUM_QUALITY			50
#define XMA_LOW_QUALITY				25

typedef struct 
{
	unsigned int	id;
	int				size;
	byte			*pData;
} chunk_t;

struct conversion_t
{
	const char *pSubDir;
	int			quality;
	bool		bForceTo22K;
};

// default conversion rules
conversion_t g_defaultConversionRules[] =
{
	// subdir		quality					22Khz
	{ "",			XMA_DEFAULT_QUALITY,	false },	// default settings
	{ "weapons",	XMA_DEFAULT_QUALITY,	false },
	{ "music",		XMA_DEFAULT_QUALITY,	false },
	{ "vo",			XMA_MEDIUM_QUALITY,		false },
	{ "npc",		XMA_MEDIUM_QUALITY,		false },
	{ "ambient",	XMA_DEFAULT_QUALITY,	false },
	{ "commentary",	XMA_LOW_QUALITY,		true },
	{ NULL },
};

// portal conversion rules
conversion_t g_portalConversionRules[] =
{
	// subdir		quality					22Khz
	{ "",			XMA_DEFAULT_QUALITY,	false },	// default settings
	{ "commentary",	XMA_LOW_QUALITY,		true },
	{ NULL },
};

chunk_t g_chunks[MAX_CHUNKS];
int		g_numChunks;

extern IFileReadBinary *g_pSndIO;

//-----------------------------------------------------------------------------
// Purpose: chunk printer
//-----------------------------------------------------------------------------
void PrintChunk( unsigned int chunkName, int size )
{
	char	c[4];

	for ( int i=0; i<4; i++ )
	{
		c[i] = ( chunkName >> i*8 ) & 0xFF;
		if ( !c[i] )
			c[i] = ' ';
	}

	Msg( "%c%c%c%c: %d bytes\n", c[0], c[1], c[2], c[3], size );
}

//-----------------------------------------------------------------------------
// Purpose: which chunks are supported, false to ignore
//-----------------------------------------------------------------------------
bool IsValidChunk( unsigned int chunkName )
{
	switch ( chunkName )
	{
		case WAVE_DATA:
		case WAVE_CUE:
		case WAVE_SAMPLER:
		case WAVE_VALVEDATA:
		case WAVE_FMT:
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: align buffer
//-----------------------------------------------------------------------------
int AlignToBoundary( CUtlBuffer &buf, int alignment )
{
	int		curPosition;
	int		newPosition;
	byte	padByte = 0;

	buf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );
	curPosition = buf.TellPut();

	if ( alignment <= 1 )
		return curPosition;

	// advance to aligned position
	newPosition = AlignValue( curPosition, alignment );
	buf.EnsureCapacity( newPosition );

	// write empty
	for ( int i=0; i<newPosition-curPosition; i++ )
	{
		buf.Put( &padByte, 1 );
	}

	return newPosition;
}

//--------------------------------------------------------------------------------------
// SampleToXMABlockOffset
//
// Description: converts from a sample index to a block index + the number of samples
//              to offset from the beginning of the block.
//
// Parameters:
//      dwSampleIndex:      sample index to convert
//      pdwSeekTable:       pointer to the file's XMA2 seek table
//      nEntries:           number of DWORD entries in the seek table
//      out_pBlockIndex:    index of block where the desired sample lives
//      out_pOffset:        number of samples in the block before the desired sample
//--------------------------------------------------------------------------------------
bool SampleToXMABlockOffset( DWORD dwSampleIndex, const DWORD *pdwSeekTable, DWORD nEntries, DWORD *out_pBlockIndex, DWORD *out_pOffset )
{
    // Run through the seek table to find the block closest to the desired sample. 
    // Each seek table entry is the index (counting from the beginning of the file) 
    // of the first sample in the corresponding block, but there's no entry for the 
    // first block (since the index would always be zero).
    bool bFound = false;
    for ( DWORD i = 0; !bFound && i < nEntries; ++i )
    {
        if ( dwSampleIndex < BigLong( pdwSeekTable[i] ) )
        {
            *out_pBlockIndex = i;
            bFound = true;
        }
    }

    // Calculate the sample offset by figuring out what the sample index of the first sample
    // in the block is, then subtracting that from dwSampleIndex.
    if ( bFound )
    {
        DWORD dwStartOfBlock = (*out_pBlockIndex == 0) ? 0 : BigLong( pdwSeekTable[*out_pBlockIndex - 1] );
        *out_pOffset = dwSampleIndex - dwStartOfBlock;
    }

    return bFound;
}

//-----------------------------------------------------------------------------
// Compile and compress vdat
//-----------------------------------------------------------------------------
bool CompressVDAT( chunk_t *pChunk )
{
	CSentence	*pSentence;

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( pChunk->size );
	memcpy( buf.Base(), pChunk->pData, pChunk->size );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, pChunk->size );

	pSentence = new CSentence();

	// Make binary version of VDAT
	// Throws all phonemes into one word, discards sentence memory, etc.
	pSentence->InitFromDataChunk( buf.Base(), buf.TellPut() );
	pSentence->MakeRuntimeOnly();
	CUtlBuffer binaryBuffer( 0, 0, 0 );
	binaryBuffer.SetBigEndian( true );
	pSentence->CacheSaveToBuffer( binaryBuffer, CACHED_SENTENCE_VERSION_ALIGNED );
	delete pSentence;

	unsigned int compressedSize = 0;
	unsigned char *pCompressedOutput = LZMA_OpportunisticCompress( (unsigned char *)binaryBuffer.Base(),
	                                                               binaryBuffer.TellPut(), &compressedSize );
	if ( pCompressedOutput )
	{
		if ( !g_bQuiet )
		{
			Msg( "CompressVDAT: Compressed %d to %d\n", binaryBuffer.TellPut(), compressedSize );
		}

		free( pChunk->pData );
		pChunk->size = compressedSize;
		pChunk->pData = pCompressedOutput;
	}
	else
	{
		// save binary VDAT as-is
		free( pChunk->pData );
		pChunk->size = binaryBuffer.TellPut();
		pChunk->pData = (byte *)malloc( pChunk->size );
		memcpy( pChunk->pData, binaryBuffer.Base(), pChunk->size );
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: read chunks into provided array
//-----------------------------------------------------------------------------
bool ReadChunks( const char *pFileName, int &numChunks, chunk_t chunks[MAX_CHUNKS] )
{
	numChunks = 0;

	InFileRIFF riff( pFileName, *g_pSndIO );
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	IterateRIFF walk( riff, riff.RIFFSize() );

	while ( walk.ChunkAvailable() )
	{
		chunks[numChunks].id = walk.ChunkName();
		chunks[numChunks].size = walk.ChunkSize();

		int size = chunks[numChunks].size;
		if ( walk.ChunkName() == WAVE_FMT && size < sizeof( WAVEFORMATEXTENSIBLE ) )
		{
			// format chunks are variable and cast to different structures
			// ensure the data footprint is at least the structure we want to manipulate
			size = sizeof( WAVEFORMATEXTENSIBLE );
		}

		chunks[numChunks].pData = (byte *)malloc( size );
		memset( chunks[numChunks].pData, 0, size );

		walk.ChunkRead( chunks[numChunks].pData );

		numChunks++;
		if ( numChunks >= MAX_CHUNKS )
			return false;

		walk.ChunkNext();
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: promote pcm 8 bit to 16 bit pcm
//-----------------------------------------------------------------------------
void ConvertPCMDataChunk8To16( chunk_t *pFormatChunk, chunk_t *pDataChunk )
{
	WAVEFORMATEX *pFormat = (WAVEFORMATEX*)pFormatChunk->pData;

	int sampleSize = ( pFormat->nChannels * pFormat->wBitsPerSample ) >> 3;
	int sampleCount = pDataChunk->size / sampleSize;
	int outputSize = sizeof( short ) * ( sampleCount * pFormat->nChannels );
	short *pOut = (short *)malloc( outputSize );

	// in-place convert data from 8-bits to 16-bits
	Convert8To16( pDataChunk->pData, pOut, sampleCount, pFormat->nChannels );

	free( pDataChunk->pData );
	pDataChunk->pData = (byte *)pOut;
	pDataChunk->size = outputSize;

	pFormat->wFormatTag = WAVE_FORMAT_PCM;
	pFormat->nBlockAlign = 2 * pFormat->nChannels;
	pFormat->wBitsPerSample = 16;
	pFormat->nAvgBytesPerSec = 2 * pFormat->nSamplesPerSec * pFormat->nChannels;
}

//-----------------------------------------------------------------------------
// Purpose: convert adpcm to 16 bit pcm
//-----------------------------------------------------------------------------
void ConvertADPCMDataChunkTo16( chunk_t *pFormatChunk, chunk_t *pDataChunk )
{
	WAVEFORMATEX *pFormat = (WAVEFORMATEX*)pFormatChunk->pData;

	int sampleCount = ADPCMSampleCount( (byte *)pFormat, pDataChunk->pData, pDataChunk->size );
	int outputSize = sizeof( short ) * sampleCount * pFormat->nChannels;
	short *pOut = (short *)malloc( outputSize );

	// convert to PCM 16bit format
	DecompressADPCMSamples( (byte*)pFormat, (byte*)pDataChunk->pData, pDataChunk->size, pOut );

	free( pDataChunk->pData );
	pDataChunk->pData = (byte *)pOut;
	pDataChunk->size = outputSize;

	pFormat->wFormatTag = WAVE_FORMAT_PCM;
	pFormat->nBlockAlign = 2 * pFormat->nChannels;
	pFormat->wBitsPerSample = 16;
	pFormat->nAvgBytesPerSec = 2 * pFormat->nSamplesPerSec * pFormat->nChannels;

	pFormatChunk->size = 16;
}

//-----------------------------------------------------------------------------
// Purpose: Decimate to 22K
//-----------------------------------------------------------------------------
void ConvertPCMDataChunk16To22K( chunk_t *pFormatChunk, chunk_t *pDataChunk )
{
	WAVEFORMATEX *pFormat = (WAVEFORMATEX*)pFormatChunk->pData;

	if ( pFormat->nSamplesPerSec != 44100 || pFormat->wBitsPerSample != 16 || pFormat->wFormatTag != WAVE_FORMAT_PCM )
	{
		// not in expected format
		return;
	}

	int sampleSize = ( pFormat->nChannels * pFormat->wBitsPerSample ) >> 3;
	int sampleCount = pDataChunk->size / sampleSize;
	short *pOut = (short *)malloc( sizeof( short ) * ( sampleCount * pFormat->nChannels ) );

	DecimateSampleRateBy2_16( (short *)pDataChunk->pData, pOut, sampleCount, pFormat->nChannels );

	free( pDataChunk->pData );
	pDataChunk->pData = (byte *)pOut;
	pDataChunk->size = sizeof( short ) * ( sampleCount/2 * pFormat->nChannels );

	pFormat->nSamplesPerSec = 22050;
	pFormat->nBlockAlign = 2 * pFormat->nChannels;
	pFormat->nAvgBytesPerSec = 2 * pFormat->nSamplesPerSec * pFormat->nChannels;
}

//-----------------------------------------------------------------------------
// Purpose: determine loop start
//-----------------------------------------------------------------------------
int FindLoopStart( int samplerChunk, int cueChunk )
{
	int loopStartFromCue = -1;
	int loopStartFromSampler = -1;

	if ( cueChunk != -1 )
	{
		struct cuechunk_t
		{
			unsigned int dwName; 
			unsigned int dwPosition;
			unsigned int fccChunk;
			unsigned int dwChunkStart;
			unsigned int dwBlockStart; 
			unsigned int dwSampleOffset;
		};
		struct cueRIFF_t
		{
			int			cueCount;
			cuechunk_t	cues[1];
		};

		cueRIFF_t *pCue = (cueRIFF_t *)g_chunks[cueChunk].pData;
		if ( pCue->cueCount > 0 )
		{
			loopStartFromCue = pCue->cues[0].dwSampleOffset;
		}
	}
	
	if ( samplerChunk != -1 )
	{
		struct SampleLoop
		{
			unsigned int	dwIdentifier;
			unsigned int	dwType;
			unsigned int	dwStart;
			unsigned int	dwEnd;
			unsigned int	dwFraction;
			unsigned int	dwPlayCount;
		};

		struct samplerchunk_t
		{
			unsigned int		dwManufacturer;
			unsigned int		dwProduct;
			unsigned int		dwSamplePeriod;
			unsigned int		dwMIDIUnityNote;
			unsigned int		dwMIDIPitchFraction;
			unsigned int		dwSMPTEFormat;
			unsigned int		dwSMPTEOffset;
			unsigned int		cSampleLoops;
			unsigned int		cbSamplerData;
			struct SampleLoop	Loops[1];
		};

		// assume that the loop end is the sample end
		// assume that only the first loop is relevant
		samplerchunk_t *pSampler = (samplerchunk_t *)g_chunks[samplerChunk].pData;
		if ( pSampler->cSampleLoops > 0 )
		{
			// only support normal forward loops
			if ( pSampler->Loops[0].dwType == 0 )
			{
				loopStartFromSampler = pSampler->Loops[0].dwStart;
			}
		}
	}

	return ( max( loopStartFromCue, loopStartFromSampler ) );
}

//-----------------------------------------------------------------------------
// Purpose: returns chunk, -1 if not found
//-----------------------------------------------------------------------------
int FindChunk( unsigned int id )
{
	int i;
	for ( i=0; i<g_numChunks; i++ )
	{
		if ( g_chunks[i].id == id )
		{
			return i;
		}
	}

	// not found
	return - 1;
}

bool EncodeAsXMA( const char *pDebugName, CUtlBuffer &targetBuff, int quality, bool bIsVoiceOver )
{
#ifdef NO_X360_XDK
	return false;
#else
	int formatChunk = FindChunk( WAVE_FMT );
	int dataChunk = FindChunk( WAVE_DATA );
	if ( formatChunk == -1 || dataChunk == -1 )
	{
		// huh? these should have been pre-validated
		return false;
	}

	int vdatSize = 0;
	int vdatChunk = FindChunk( WAVE_VALVEDATA );
	if ( vdatChunk != -1 )
	{
		vdatSize = g_chunks[vdatChunk].size;
	}

	int loopStart = FindLoopStart( FindChunk( WAVE_SAMPLER ), FindChunk( WAVE_CUE ) );

	// format structure must be expected 16 bit PCM, otherwise encoder crashes
	WAVEFORMATEX *pFormat = (WAVEFORMATEX *)g_chunks[formatChunk].pData;
	pFormat->nAvgBytesPerSec = pFormat->nSamplesPerSec * pFormat->nChannels * 2;
	pFormat->nBlockAlign = 2 * pFormat->nChannels;
	pFormat->cbSize = 0;

	XMAENCODERSTREAM inputStream = { 0 };

	WAVEFORMATEXTENSIBLE wfx;
	Assert( g_chunks[formatChunk].size <= sizeof( WAVEFORMATEXTENSIBLE ) );
	memcpy( &wfx, g_chunks[formatChunk].pData, g_chunks[formatChunk].size );
	if ( g_chunks[formatChunk].size < sizeof( WAVEFORMATEXTENSIBLE ) )
	{
		memset( (unsigned char*)&wfx + g_chunks[formatChunk].size, 0, sizeof( WAVEFORMATEXTENSIBLE ) - g_chunks[formatChunk].size );
	}

	memcpy( &inputStream.Format, &wfx, sizeof( WAVEFORMATEX ) );
	inputStream.pBuffer = g_chunks[dataChunk].pData;
	inputStream.BufferSize = g_chunks[dataChunk].size;
	if ( loopStart != -1 )
	{
		// can only support a single loop point until end of file
		inputStream.LoopStart = loopStart;
		inputStream.LoopLength = inputStream.BufferSize / ( pFormat->nChannels * sizeof( short ) ) - loopStart;
	}

	void *pXMAData = NULL;
	DWORD XMADataSize = 0;
	XMA2WAVEFORMAT *pXMA2Format = NULL;
	DWORD XMA2FormatSize = 0;
	DWORD *pXMASeekTable = NULL;
	DWORD XMASeekTableSize = 0;
	HRESULT hr = S_OK;

	DWORD xmaFlags = XMAENCODER_NOFILTER;
	if ( loopStart != -1 )
	{
		xmaFlags |= XMAENCODER_LOOP;
	}

	int numAttempts = 1;
	while ( numAttempts < 10 )
	{
		hr = XMA2InMemoryEncoder( 1, &inputStream, quality, xmaFlags, XMA_BLOCK_SIZE/1024, &pXMAData, &XMADataSize, &pXMA2Format, &XMA2FormatSize, &pXMASeekTable, &XMASeekTableSize );
		if ( !FAILED( hr ) )
			break;

		// make small jumps
		quality += 5;
		if ( quality > 100 )
			quality = 100;
		if ( !g_bQuiet )
		{
			Msg( "XMA Encoding Error on '%s', Attempting increasing quality to %d\n", pDebugName, quality );
		}

		numAttempts++;

		pXMAData = NULL;
		XMADataSize = 0;
		pXMA2Format = NULL;
		XMA2FormatSize = 0;
		pXMASeekTable = NULL;
		XMASeekTableSize = 0;
	}

	if ( FAILED( hr ) )
	{
		// unrecoverable
		return false;
	}
	else if ( numAttempts > 1 )
	{
		if ( !g_bQuiet )
		{
			Msg( "XMA Encoding Success on '%s' at quality %d\n", pDebugName, quality );
		}
	}

	DWORD loopBlock = 0;
	DWORD numLeadingSamples = 0;
	DWORD numTrailingSamples = 0;

	if ( loopStart != -1 )
	{
		// calculate start block/offset
		DWORD loopBlockStartIndex = 0;
		DWORD loopBlockStartOffset = 0;

		if ( !SampleToXMABlockOffset( BigLong( pXMA2Format->LoopBegin ), pXMASeekTable, XMASeekTableSize/sizeof( DWORD ), &loopBlockStartIndex, &loopBlockStartOffset ) )
		{
			// could not determine loop point, out of range of encoded samples
			Msg( "XMA Loop Encoding Error on '%s', loop %d\n", pDebugName, loopStart );
			return false;
		}

		loopBlock = loopBlockStartIndex;
		numLeadingSamples = loopBlockStartOffset;

		if ( BigLong( pXMA2Format->LoopEnd ) < BigLong( pXMA2Format->SamplesEncoded ) )
		{
			// calculate end block/offset
			DWORD loopBlockEndIndex = 0;
			DWORD loopBlockEndOffset = 0;

			if ( !SampleToXMABlockOffset( BigLong( pXMA2Format->LoopEnd ), pXMASeekTable, XMASeekTableSize/sizeof( DWORD ), &loopBlockEndIndex, &loopBlockEndOffset ) )
			{
				// could not determine loop point, out of range of encoded samples
				Msg( "XMA Loop Encoding Error on '%s', loop %d\n", pDebugName, loopStart );
				return false;
			}

			if ( loopBlockEndIndex != BigLong( pXMA2Format->BlockCount ) - 1 )
			{
				// end block MUST be last block
				Msg( "XMA Loop Encoding Error on '%s', block end is %d/%d\n", pDebugName, loopBlockEndOffset, BigLong( pXMA2Format->BlockCount ) );
				return false;
			}

			numTrailingSamples = BigLong( pXMA2Format->SamplesEncoded ) - BigLong( pXMA2Format->LoopEnd );
		}

		// check for proper encoding range
		if ( loopBlock > 32767 )
		{
			Msg( "XMA Loop Encoding Error on '%s', loop block exceeds 16 bits %d\n", pDebugName, loopBlock );
			return false;
		}
		if ( numLeadingSamples > 32767 )
		{
			Msg( "XMA Loop Encoding Error on '%s', leading samples exceeds 16 bits %d\n", pDebugName, numLeadingSamples );
			return false;
		}
		if ( numTrailingSamples > 32767 )
		{
			Msg( "XMA Loop Encoding Error on '%s', trailing samples exceeds 16 bits %d\n", pDebugName, numTrailingSamples );
			return false;
		}
	}

	xwvHeader_t header;
	memset( &header, 0, sizeof( xwvHeader_t ) );

	int seekTableSize = 0;
	if ( vdatSize || bIsVoiceOver )
	{
		// save the optional seek table only for vdat or vo
		// the seek table size is expected to be derived by this calculation
		seekTableSize = ( XMADataSize / XMA_BYTES_PER_PACKET ) * sizeof( int );
		if ( seekTableSize != XMASeekTableSize )
		{
			Msg( "XMA Error: Unexpected seek table calculation in '%s'!", pDebugName );
			return false;
		}
	}

	if ( loopStart != -1 && ( vdatSize || bIsVoiceOver ) )
	{
		Msg( "XMA Warning: Unexpected loop in vo data '%s'!", pDebugName );

		// do not write the seek table for looping sounds
		seekTableSize = 0;
	}

	header.id = BigLong( XWV_ID );
	header.version = BigLong( XWV_VERSION );
	header.headerSize = BigLong( sizeof( xwvHeader_t ) );
	header.staticDataSize = BigLong( seekTableSize + vdatSize );
	header.dataOffset = BigLong( AlignValue( sizeof( xwvHeader_t) + seekTableSize + vdatSize, XBOX_DVD_SECTORSIZE ) );
	header.dataSize = BigLong( XMADataSize );

	// track the XMA number of samples that will get decoded
	// which is NOT the same as what the source actually encoded
	header.numDecodedSamples = pXMA2Format->SamplesEncoded;

	if ( loopStart != -1 )
	{
		// the loop start is in source space (now meaningless), need the loop in XMA decoding sample space
		header.loopStart = pXMA2Format->LoopBegin;
	}
	else
	{
		header.loopStart = BigLong( -1 );
	}
	header.loopBlock = BigShort( (unsigned short)loopBlock );
	header.numLeadingSamples = BigShort( (unsigned short)numLeadingSamples );
	header.numTrailingSamples = BigShort( (unsigned short)numTrailingSamples );

	header.vdatSize = BigShort( (short)vdatSize );
	header.format = XWV_FORMAT_XMA;
	header.bitsPerSample = 16;
	header.SetSampleRate( pFormat->nSamplesPerSec );
	header.SetChannels( pFormat->nChannels );
	header.quality = quality;
	header.bHasSeekTable = ( seekTableSize != 0 );

	// output header
	targetBuff.Put( &header, sizeof( xwvHeader_t ) );

	// output optional seek table
	if ( seekTableSize )
	{
		// seek table is already in big-endian format
		targetBuff.Put(	pXMASeekTable, seekTableSize );
	}

	// output vdat
	if ( vdatSize )
	{
		targetBuff.Put( g_chunks[vdatChunk].pData, g_chunks[vdatChunk].size );
	}

	AlignToBoundary( targetBuff, XBOX_DVD_SECTORSIZE );

	// write data
	targetBuff.Put( pXMAData, XMADataSize );

	// pad to EOF
	AlignToBoundary( targetBuff, XBOX_DVD_SECTORSIZE );

	free( pXMAData );
	free( pXMA2Format );
	free( pXMASeekTable );

	// xma encoder leaves its temporary files, we'll delete
	scriptlib->DeleteTemporaryFiles( "LoopStrm*" );
	scriptlib->DeleteTemporaryFiles( "EncStrm*" );

	return true;
#endif
}

bool EncodeAsPCM( const char *pTargetName, CUtlBuffer &targetBuff )
{
	int formatChunk = FindChunk( WAVE_FMT );
	int dataChunk = FindChunk( WAVE_DATA );
	if ( formatChunk == -1 || dataChunk == -1 )
	{
		// huh? these should have been pre-validated
		return false;
	}

	WAVEFORMATEX *pFormat = (WAVEFORMATEX *)g_chunks[formatChunk].pData;
	if ( pFormat->wBitsPerSample != 16 )
	{
		// huh? the input is expeted to be 16 bit PCM
		return false;
	}

	int vdatSize = 0;
	int vdatChunk = FindChunk( WAVE_VALVEDATA );
	if ( vdatChunk != -1 )
	{
		vdatSize = g_chunks[vdatChunk].size;
	}

	chunk_t *pDataChunk = &g_chunks[dataChunk];

	xwvHeader_t header;
	memset( &header, 0, sizeof( xwvHeader_t ) );

	int sampleSize = pFormat->nChannels * sizeof( short );
	int sampleCount = pDataChunk->size / sampleSize;

	header.id = BigLong( XWV_ID );
	header.version = BigLong( XWV_VERSION );
	header.headerSize = BigLong( sizeof( xwvHeader_t ) );
	header.staticDataSize = BigLong( vdatSize );
	header.dataOffset = BigLong( AlignValue( sizeof( xwvHeader_t) + vdatSize, XBOX_DVD_SECTORSIZE ) );
	header.dataSize = BigLong( pDataChunk->size );
	header.numDecodedSamples = BigLong( sampleCount );
	header.loopStart = BigLong( -1 );
	header.loopBlock = 0;
	header.numLeadingSamples = 0;
	header.numTrailingSamples = 0;
	header.vdatSize = BigShort( (short)vdatSize );
	header.format = XWV_FORMAT_PCM;
	header.bitsPerSample = 16;
	header.SetSampleRate( pFormat->nSamplesPerSec );
	header.SetChannels( pFormat->nChannels );
	header.quality = 100;

	// output header
	targetBuff.Put( &header, sizeof( xwvHeader_t ) );

	// output vdat
	if ( vdatSize )
	{
		targetBuff.Put( g_chunks[vdatChunk].pData, g_chunks[vdatChunk].size );
	}

	AlignToBoundary( targetBuff, XBOX_DVD_SECTORSIZE );

	for ( int i = 0; i < sampleCount * pFormat->nChannels; i++ )
	{
		((short *)pDataChunk->pData)[i] = BigShort( ((short *)pDataChunk->pData)[i] );
	}

	// write data
	targetBuff.Put( pDataChunk->pData, pDataChunk->size );

	// pad to EOF
	AlignToBoundary( targetBuff, XBOX_DVD_SECTORSIZE );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: read source, do work, and write to target
//-----------------------------------------------------------------------------
bool CreateTargetFile_WAV( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	g_numChunks = 0;

	// resolve relative source to absolute path
	char fullSourcePath[MAX_PATH];
	if ( _fullpath( fullSourcePath, pSourceName, sizeof( fullSourcePath ) ) )
	{
		pSourceName = fullSourcePath;
	}

	if ( !ReadChunks( pSourceName, g_numChunks, g_chunks ) )
	{
		Msg( "No RIFF Chunks on '%s'\n", pSourceName );
		return false;
	}

	int formatChunk = FindChunk( WAVE_FMT );
	if ( formatChunk == -1 )
	{
		Msg( "RIFF Format Chunk not found on '%s'\n", pSourceName );
		return false;
	}

	int dataChunk = FindChunk( WAVE_DATA );
	if ( dataChunk == -1 )
	{
		Msg( "RIFF Data Chunk not found on '%s'\n", pSourceName );
		return false;
	}

	// get the conversion rules
	conversion_t *pConversion = g_defaultConversionRules;
	if ( V_stristr( g_szModPath, "\\portal" ) )
	{
		pConversion = g_portalConversionRules;
	}

	// conversion rules are based on matching subdir
	for ( int i=1; ;i++ )
	{
		char subString[MAX_PATH];
		if ( !pConversion[i].pSubDir )
		{
			// end of list
			break;
		}

		sprintf( subString, "\\%s\\", pConversion[i].pSubDir );
		if ( V_stristr( pSourceName, subString ) )
		{
			// use matched conversion rules
			pConversion = &pConversion[i];
			break;
		}
	}

	bool bForceTo22K = pConversion->bForceTo22K;
	int quality = pConversion->quality;

	// cannot trust the localization depots to have matched their sources
	// cannot allow 44K
	if ( IsLocalizedFile( pSourceName ) )
	{
		bForceTo22K = true;
	}

	// classify strict vo  from /sound/vo only
	bool bIsVoiceOver = V_stristr( pSourceName, "\\sound\\vo\\" ) != NULL;

	// can override default settings
	quality = CommandLine()->ParmValue( "-xmaquality", quality );
	if ( quality < 0 )
		quality = 0;
	else if ( quality > 100 )
		quality = 100;
	if ( !g_bQuiet )
	{
		Msg( "Encoding quality: %d on '%s'\n", quality, pSourceName );
	}

	int vdatSize = 0;
	int vdatChunk = FindChunk( WAVE_VALVEDATA );
	if ( vdatChunk != -1 )
	{
		// compile to optimal block
		if ( !CompressVDAT( &g_chunks[vdatChunk] ) )
		{
			Msg( "Compress VDAT Error on '%s'\n", pSourceName );
			return false;
		}
		vdatSize = g_chunks[vdatChunk].size;
	}

	// for safety (not trusting their decoding) and simplicity convert all data to 16 bit PCM before encoding
	WAVEFORMATEX *pFormat = (WAVEFORMATEX *)g_chunks[formatChunk].pData;
	if ( ( pFormat->wFormatTag == WAVE_FORMAT_PCM ) )
	{
		if ( pFormat->wBitsPerSample == 8 )
		{
			ConvertPCMDataChunk8To16( &g_chunks[formatChunk], &g_chunks[dataChunk] );
		}
	}
	else if ( pFormat->wFormatTag == WAVE_FORMAT_ADPCM )
	{
		ConvertADPCMDataChunkTo16( &g_chunks[formatChunk], &g_chunks[dataChunk] );
	}
	else
	{
		Msg( "Unknown RIFF Format on '%s'\n", pSourceName );
		return false;
	}

	// optionally decimate to 22K
	if ( pFormat->nSamplesPerSec == 44100 && bForceTo22K )
	{
		if ( !g_bQuiet )
		{
			Msg( "Converting to 22K '%s'\n", pSourceName );
		}
		ConvertPCMDataChunk16To22K( &g_chunks[formatChunk], &g_chunks[dataChunk] );
	}

	CUtlBuffer targetBuff;
	bool bSuccess;

	bSuccess = EncodeAsXMA( pSourceName, targetBuff, quality, bIsVoiceOver );
	if ( bSuccess )
	{
		WriteBufferToFile( pTargetName, targetBuff, bWriteToZip, g_WriteModeForConversions );
	}

	// release data
	for ( int i = 0; i < g_numChunks; i++ )
	{
		free( g_chunks[i].pData );
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: MP3's are already pre-converted into .360.wav
//-----------------------------------------------------------------------------
bool CreateTargetFile_MP3( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	CUtlBuffer	targetBuffer;

	// ignore the .mp3 source, the .360.wav target should have been pre-converted, checked in, and exist
	// use the expected target as the source
	if ( !scriptlib->ReadFileToBuffer( pTargetName, targetBuffer ) )
	{
		// the .360.wav target does not exist
		// try again using a .wav version and convert from that
		char wavFilename[MAX_PATH];
		V_StripExtension( pSourceName, wavFilename, sizeof( wavFilename ) ); 
		V_SetExtension( wavFilename, ".wav", sizeof( wavFilename ) );
		if ( scriptlib->DoesFileExist( wavFilename ) )
		{
			if ( CreateTargetFile_WAV( wavFilename, pTargetName, bWriteToZip ) )
			{
				return true;
			}
		}

		return false;
	}

	// no conversion to write, but possibly zipped
	bool bSuccess = WriteBufferToFile( pTargetName, targetBuffer, bWriteToZip, WRITE_TO_DISK_NEVER );
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Get the preload data for a wav file
//-----------------------------------------------------------------------------
bool GetPreloadData_WAV( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	xwvHeader_t *pHeader = ( xwvHeader_t * )fileBufferIn.Base();
	if ( pHeader->id != ( unsigned int )BigLong( XWV_ID ) ||
		pHeader->version != ( unsigned int )BigLong( XWV_VERSION ) ||
		pHeader->headerSize != BigLong( sizeof( xwvHeader_t ) ) )
	{
		// bad version
		Msg( "Can't preload: '%s', has bad version\n", pFilename );
		return false;
	}

	// ensure caller's buffer is clean
	// caller determines preload size, via TellMaxPut()
	preloadBufferOut.Purge();
	unsigned int preloadSize = BigLong( pHeader->headerSize ) + BigLong( pHeader->staticDataSize );
	preloadBufferOut.Put( fileBufferIn.Base(), preloadSize );

	return true;
}
