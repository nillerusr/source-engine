//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "tier2/riff.h"
#include "snd_wave_source.h"
#include "snd_wave_mixer_private.h"
#include "snd_audio_source.h"
#include <mmsystem.h>		// wave format
#include <mmreg.h>			// adpcm format
#include "hlfaceposer.h"
#include "filesystem.h"
#include "utlbuffer.h"
#include "phonemeconverter.h"

//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return filesystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		filesystem->Close( (FileHandle_t)file );
	}
};

static StdIOReadBinary io;

#define RIFF_WAVE			MAKEID('W','A','V','E')
#define WAVE_FMT			MAKEID('f','m','t',' ')
#define WAVE_DATA			MAKEID('d','a','t','a')
#define WAVE_FACT			MAKEID('f','a','c','t')
#define WAVE_CUE			MAKEID('c','u','e',' ')

void ChunkError( unsigned int id )
{
}

//-----------------------------------------------------------------------------
// Purpose: Init to empty wave
//-----------------------------------------------------------------------------
CAudioSourceWave::CAudioSourceWave( void )
{
	m_format = 0;
	m_pHeader = NULL;
	// no looping
	m_loopStart = -1;
	m_sampleSize = 1;
	m_sampleCount = 0;
}


CAudioSourceWave::~CAudioSourceWave( void )
{
	// for non-standard waves, we store a copy of the header in RAM
	delete[] m_pHeader;
	//	m_pWords points into m_pWordBuffer, no need to delete
}

//-----------------------------------------------------------------------------
// Purpose: Init the wave data.
// Input  : *pHeaderBuffer - the RIFF fmt chunk
//			headerSize - size of that chunk
//-----------------------------------------------------------------------------
void CAudioSourceWave::Init( const char *pHeaderBuffer, int headerSize )
{
	const WAVEFORMATEX *pHeader = (const WAVEFORMATEX *)pHeaderBuffer;

	// copy the relevant header data
	m_format = pHeader->wFormatTag;
	m_bits = pHeader->wBitsPerSample;
	m_rate = pHeader->nSamplesPerSec;
	m_channels = pHeader->nChannels;

	m_sampleSize = (m_bits * m_channels) / 8;
	
	// this can never be zero -- other functions divide by this. 
	// This should never happen, but avoid crashing
	if ( m_sampleSize <= 0 )
		m_sampleSize = 1;

	// For non-standard waves (like ADPCM) store the header, it has some useful data
	if ( m_format != WAVE_FORMAT_PCM )
	{
		m_pHeader = new char[headerSize];
		memcpy( m_pHeader, pHeader, headerSize );
		if ( m_format == WAVE_FORMAT_ADPCM )
		{
			// treat ADPCM sources as a file of bytes.  They are decoded by the mixer
			m_sampleSize = 1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CAudioSourceWave::TrueSampleSize( void )
{
	if ( m_format == WAVE_FORMAT_ADPCM )
	{
		return 0.5f;
	}
	return (float)m_sampleSize;
}

//-----------------------------------------------------------------------------
// Purpose: Total number of samples in this source
// Output : int
//-----------------------------------------------------------------------------
int CAudioSourceWave::SampleCount( void ) 
{
	if ( m_format == WAVE_FORMAT_ADPCM )
	{
		ADPCMWAVEFORMAT *pFormat = (ADPCMWAVEFORMAT *)m_pHeader;
		int blockSize = ((pFormat->wSamplesPerBlock - 2) * pFormat->wfx.nChannels ) / 2;
		blockSize += 7 * pFormat->wfx.nChannels;

		int blockCount = m_sampleCount / blockSize;
		int blockRem = m_sampleCount % blockSize;
		
		// total samples in complete blocks
		int sampleCount = blockCount * pFormat->wSamplesPerBlock;

		// add remaining in a short block
		if ( blockRem )
		{
			sampleCount += pFormat->wSamplesPerBlock - (((blockSize - blockRem) * 2) / m_channels);
		}
		return sampleCount;
	}
	return m_sampleCount; 
}

//-----------------------------------------------------------------------------
// Purpose: Do any sample conversion
//			For 8 bit PCM, convert to signed because the mixing routine assumes this
// Input  : *pData - pointer to sample data
//			sampleCount - number of samples
//-----------------------------------------------------------------------------
void CAudioSourceWave::ConvertSamples( char *pData, int sampleCount )
{
	if ( m_format == WAVE_FORMAT_PCM )
	{
		if ( m_bits == 8 )
		{
			for ( int i = 0; i < sampleCount; i++ )
			{
				for ( int j = 0; j < m_channels; j++ )
				{
					*pData = (unsigned char)((int)((unsigned)*pData) - 128);
					pData++;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
void CAudioSourceWave::ParseSentence( IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	m_Sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

//-----------------------------------------------------------------------------
// Purpose: Parse base chunks
// Input  : &walk - riff file to parse
//		  : chunkName - name of the chunk to parse
//-----------------------------------------------------------------------------
// UNDONE: Move parsing loop here and drop each chunk into a virtual function
//			instead of this being virtual.
void CAudioSourceWave::ParseChunk( IterateRIFF &walk, int chunkName )
{
	switch( chunkName )
	{
		case WAVE_CUE:
			{
				m_loopStart = ParseCueChunk( walk );
			}
			break;
		case WAVE_VALVEDATA:
			{
				ParseSentence( walk );
			}
			break;
			// unknown/don't care
		default:
			{
				ChunkError( walk.ChunkName() );
			}
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CSentence
//-----------------------------------------------------------------------------
CSentence *CAudioSourceWave::GetSentence( void )
{
	return &m_Sentence;
}

//-----------------------------------------------------------------------------
// Purpose: Bastardized construction routine.  This is just to avoid complex
//			constructor functions so code can be shared more easily by sub-classes
// Input  : *pFormatBuffer - RIFF header
//			formatSize - header size
//			&walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceWave::Setup( const char *pFormatBuffer, int formatSize, IterateRIFF &walk )
{
	Init( pFormatBuffer, formatSize );

	while ( walk.ChunkAvailable() )
	{
		ParseChunk( walk, walk.ChunkName() );
		walk.ChunkNext();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wave file that is completely in memory
//			UNDONE: Implement Lock/Unlock and caching
//-----------------------------------------------------------------------------
class CAudioSourceMemWave : public CAudioSourceWave
{
public:
	CAudioSourceMemWave( void );
	~CAudioSourceMemWave( void );

	// Create an instance (mixer) of this audio source
	virtual CAudioMixer		*CreateMixer( void );

	virtual void			ParseChunk( IterateRIFF &walk, int chunkName );
	void					ParseDataChunk( IterateRIFF &walk );

	virtual int				GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward = true );
	virtual float			GetRunningLength( void ) { return CAudioSourceWave::GetRunningLength(); };

	virtual int				GetNumChannels();

private:
	char	*m_pData;		// wave data
};


//-----------------------------------------------------------------------------
// Purpose: Iterator for wave data (this is to abstract streaming/buffering)
//-----------------------------------------------------------------------------
class CWaveDataMemory : public CWaveData
{
public:
	CWaveDataMemory( CAudioSourceWave &source ) : m_source(source) {}
	~CWaveDataMemory( void ) {}
	CAudioSourceWave &Source( void ) { return m_source; }
	
	// this file is in memory, simply pass along the data request to the source
	virtual int ReadSourceData( void **pData, int sampleIndex, int sampleCount, bool forward /*= true*/ )
	{
		return m_source.GetOutputData( pData, sampleIndex, sampleCount, forward );
	}
private:
	CAudioSourceWave		&m_source;	// pointer to source
};


//-----------------------------------------------------------------------------
// Purpose: NULL the wave data pointer (we haven't loaded yet)
//-----------------------------------------------------------------------------
CAudioSourceMemWave::CAudioSourceMemWave( void )
{
	m_pData = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Free any wave data we've allocated
//-----------------------------------------------------------------------------
CAudioSourceMemWave::~CAudioSourceMemWave( void )
{
	delete[] m_pData;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a mixer and initializes it with an appropriate mixer
//-----------------------------------------------------------------------------
CAudioMixer *CAudioSourceMemWave::CreateMixer( void )
{
	return CreateWaveMixer( new CWaveDataMemory(*this), m_format, m_channels, m_bits );
}

//-----------------------------------------------------------------------------
// Purpose: parse chunks with unique processing to in-memory waves
// Input  : &walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::ParseChunk( IterateRIFF &walk, int chunkName )
{
	switch( chunkName )
	{
		// this is the audio data
	case WAVE_DATA:
		{
			ParseDataChunk( walk );
		}
		return;
	}

	CAudioSourceWave::ParseChunk( walk, chunkName );
}


//-----------------------------------------------------------------------------
// Purpose: reads the actual sample data and parses it
// Input  : &walk - RIFF file
//-----------------------------------------------------------------------------
void CAudioSourceMemWave::ParseDataChunk( IterateRIFF &walk )
{
	int size = walk.ChunkSize();
	
	// create a buffer for the samples
	m_pData = new char[size];

	// load them into memory
	walk.ChunkRead( m_pData );

	if ( m_format == WAVE_FORMAT_PCM )
	{
		// number of samples loaded
		m_sampleCount = size / m_sampleSize;

		// some samples need to be converted
		ConvertSamples( m_pData, m_sampleCount );
	}
	else if ( m_format == WAVE_FORMAT_ADPCM )
	{
		// The ADPCM mixers treat the wave source as a flat file of bytes.
		m_sampleSize = 1;
		// Since each "sample" is a byte (this is a flat file), the number of samples is the file size
		m_sampleCount = size;

		// file says 4, output is 16
		m_bits = 16;
	}
}

int CAudioSourceMemWave::GetNumChannels()
{
	return m_channels;
}



//-----------------------------------------------------------------------------
// Purpose: parses loop information from a cue chunk
// Input  : &walk - RIFF iterator
// Output : int loop start position
//-----------------------------------------------------------------------------
int CAudioSourceWave::ParseCueChunk( IterateRIFF &walk )
{
	// Cue chunk as specified by RIFF format
	// see $/research/jay/sound/riffnew.htm
	struct 
	{
		unsigned int dwName; 
		unsigned int dwPosition;
		unsigned int fccChunk;
		unsigned int dwChunkStart;
		unsigned int dwBlockStart; 
		unsigned int dwSampleOffset;
	} cue_chunk;

	int cueCount;

	// assume that the cue chunk stored in the wave is the start of the loop
	// assume only one cue chunk, UNDONE: Test this assumption here?
	cueCount = walk.ChunkReadInt();

	walk.ChunkReadPartial( &cue_chunk, sizeof(cue_chunk) );
	return cue_chunk.dwSampleOffset;
}

//-----------------------------------------------------------------------------
// Purpose: get the wave header
//-----------------------------------------------------------------------------
void *CAudioSourceWave::GetHeader( void )
{
	return m_pHeader;
}


//-----------------------------------------------------------------------------
// Purpose: wrap the position wrt looping
// Input  : samplePosition - absolute position
// Output : int - looped position
//-----------------------------------------------------------------------------
int CAudioSourceWave::ConvertLoopedPosition( int samplePosition )
{
	// if the wave is looping and we're past the end of the sample
	// convert to a position within the loop
	// At the end of the loop, we return a short buffer, and subsequent call
	// will loop back and get the rest of the buffer
	if ( m_loopStart >= 0 )
	{
		if ( samplePosition >= m_sampleCount )
		{
			// size of loop
			int loopSize = m_sampleCount - m_loopStart;
			// subtract off starting bit of the wave
			samplePosition -= m_loopStart;
			
			if ( loopSize )
			{
				// "real" position in memory (mod off extra loops)
				samplePosition = m_loopStart + (samplePosition % loopSize);
			}
			// ERROR? if no loopSize
		}
	}

	return samplePosition;
}

bool CAudioSourceWave::IsStereoWav( void ) 
{ 
	return (m_channels == 2) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **pData - output pointer to samples
//			samplePosition - position (in samples not bytes) 
//			sampleCount - number of samples (not bytes)
// Output : int - number of samples available
//-----------------------------------------------------------------------------
int CAudioSourceMemWave::GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward /*= true*/ )
{
	// handle position looping
	samplePosition = ConvertLoopedPosition( samplePosition );

	// how many samples are available (linearly not counting looping)
	int availableSampleCount = m_sampleCount - samplePosition;
	if ( !forward )
	{
		if ( samplePosition >= m_sampleCount )
		{
			availableSampleCount = 0;
		}
		else
		{
			availableSampleCount = samplePosition;
		}
	}

	// may be asking for a sample out of range, clip at zero
	if ( availableSampleCount < 0 )
		availableSampleCount = 0;

	// clip max output samples to max available
	if ( sampleCount > availableSampleCount )
		sampleCount = availableSampleCount;

	// byte offset in sample database
	samplePosition *= m_sampleSize;

	// if we are returning some samples, store the pointer
	if ( sampleCount )
	{
		*pData = m_pData + samplePosition;
	}

	return sampleCount;
}

//-----------------------------------------------------------------------------
// Purpose: Create a wave audio source (streaming or in memory)
// Input  : *pName - file name
//			streaming - if true, don't load, stream each instance
// Output : CAudioSource * - a new source
//-----------------------------------------------------------------------------
// UNDONE : Pool these and check for duplicates?
CAudioSource *CreateWave( const char *pName )
{
	char formatBuffer[1024];
	InFileRIFF riff( pName, io );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		printf("Bad RIFF file type %s\n", pName );
		return NULL;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	int format = 0;
	int formatSize = 0;

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	while ( walk.ChunkAvailable() && format == 0 )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_FMT:
			{
				if ( walk.ChunkSize() <= 1024 )
				{
					walk.ChunkRead( formatBuffer );
					formatSize = walk.ChunkSize();
					format = ((WAVEFORMATEX *)formatBuffer)->wFormatTag;
				}
			}
			break;
		default:
			{
				ChunkError( walk.ChunkName() );
			}
			break;
		}
		walk.ChunkNext();
	}

	// Not really a WAVE file or no format chunk, bail
	if ( !format )
		return NULL;

	CAudioSourceWave *pWave;

	// create the source from this file
	pWave = new CAudioSourceMemWave();

	// init the wave source
	pWave->Setup( formatBuffer, formatSize, walk );

	return pWave;
}

//-----------------------------------------------------------------------------
// Purpose: Wrapper for CreateWave()
//-----------------------------------------------------------------------------
CAudioSource *Audio_CreateMemoryWave( const char *pName )
{
	return CreateWave( pName );
}
