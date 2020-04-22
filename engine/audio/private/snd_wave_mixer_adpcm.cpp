//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "audio_pch.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// max size of ADPCM block in bytes
#define MAX_BLOCK_SIZE	4096


//-----------------------------------------------------------------------------
// Purpose: Mixer for ADPCM encoded audio
//-----------------------------------------------------------------------------
class CAudioMixerWaveADPCM : public CAudioMixerWave
{
public:
	CAudioMixerWaveADPCM( IWaveData *data );
	~CAudioMixerWaveADPCM( void );
	
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	virtual int	 GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	// need to override this to fixup blocks
	void SetSampleStart( int newPosition );
	virtual int GetMixSampleSize() { return CalcSampleSize( 16, NumChannels() ); }

private:
	bool					DecodeBlock( void );
	int						NumChannels( void );
	void					DecompressBlockMono( short *pOut, const char *pIn, int count );
	void					DecompressBlockStereo( short *pOut, const char *pIn, int count );

	const ADPCMWAVEFORMAT	*m_pFormat;
	const ADPCMCOEFSET		*m_pCoefficients;

	short					*m_pSamples;
	int						m_sampleCount;
	int						m_samplePosition;

	int						m_blockSize;
	int						m_offset;

	int						m_totalBytes;
};


CAudioMixerWaveADPCM::CAudioMixerWaveADPCM( IWaveData *data ) : CAudioMixerWave( data ) 
{
	m_pSamples = NULL;
	m_sampleCount = 0;
	m_samplePosition = 0;
	m_offset = 0;

	CAudioSourceWave &source = reinterpret_cast<CAudioSourceWave &>(m_pData->Source());

#ifdef _DEBUG
	CAudioSource *pSource = NULL;
	pSource = &m_pData->Source();
	Assert( dynamic_cast<CAudioSourceWave *>(pSource) != NULL );
#endif

	m_pFormat = (const ADPCMWAVEFORMAT *)source.GetHeader();
	if ( m_pFormat )
	{
		m_pCoefficients = (ADPCMCOEFSET *)((char *)m_pFormat + sizeof(WAVEFORMATEX) + 4);

		// create the decode buffer
		m_pSamples = new short[m_pFormat->wSamplesPerBlock * m_pFormat->wfx.nChannels];

		// number of bytes for samples
		m_blockSize = ((m_pFormat->wSamplesPerBlock - 2) * m_pFormat->wfx.nChannels ) / 2;
		// size of channel header
		m_blockSize += 7 * m_pFormat->wfx.nChannels;
		Assert( m_blockSize < MAX_BLOCK_SIZE );

		m_totalBytes = source.DataSize();
	}
}


CAudioMixerWaveADPCM::~CAudioMixerWaveADPCM( void )
{
	delete[] m_pSamples;
}


int	CAudioMixerWaveADPCM::NumChannels( void )
{
	if ( m_pFormat )
	{
		return m_pFormat->wfx.nChannels;
	}
	return 0;
}

void CAudioMixerWaveADPCM::Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
{
	if ( NumChannels() == 1 )
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	else
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
}


static int error_sign_lut[] =		  { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };
static int error_coefficients_lut[] = { 230, 230, 230, 230, 307, 409, 512, 614,
										768, 614, 512, 409, 307, 230, 230, 230 };

//-----------------------------------------------------------------------------
// Purpose: ADPCM decompress a single block of 1-channel audio
// Input  : *pOut - output buffer 16-bit
//			*pIn - input block
//			count - number of samples to decode (to support partial blocks)
//-----------------------------------------------------------------------------
void CAudioMixerWaveADPCM::DecompressBlockMono( short *pOut, const char *pIn, int count )
{
	int pred = *pIn++;
	int co1 = m_pCoefficients[pred].iCoef1;
	int co2 = m_pCoefficients[pred].iCoef2;

	// read initial delta
	int delta = *((short *)pIn);
	pIn += 2;

	// read initial samples for prediction
	int samp1 = *((short *)pIn);
	pIn += 2;

	int samp2 = *((short *)pIn);
	pIn += 2;

	// write out the initial samples (stored in reverse order)
	*pOut++ = (short)samp2;
	*pOut++ = (short)samp1;

	// subtract the 2 samples in the header
	count -= 2;

	// this is a toggle to read nibbles, first nibble is high
	int high = 1;

	int error, sample=0;

	// now process the block
	while ( count )
	{
		// read the error nibble from the input stream
		if ( high )
		{
			sample = (unsigned char) (*pIn++);
			// high nibble
			error = sample >> 4;
			// cache low nibble for next read
			sample = sample & 0xf;
			// Next read is from cache, not stream
			high = 0;
		}
		else
		{
			// stored in previous read (low nibble)
			error = sample;
			// next read is from stream
			high = 1;
		}
		// convert to signed with LUT
		int errorSign = error_sign_lut[error];

		// interpolate the new sample
		int predSample = (samp1 * co1) + (samp2 * co2);
		// coefficients are fixed point 8-bit, so shift back to 16-bit integer
		predSample >>= 8;

		// Add in current error estimate
		predSample += (errorSign * delta);

		// Correct error estimate
		delta = (delta * error_coefficients_lut[error]) >> 8;
		// Clamp error estimate
		if ( delta < 16 )
			delta = 16;

		// clamp
		if ( predSample > 32767L )
			predSample = 32767L;
		else if ( predSample < -32768L )
			predSample = -32768L;
		
		// output
		*pOut++ = (short)predSample;
		// move samples over
		samp2 = samp1;
		samp1 = predSample;

		count--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Decode a single block of stereo ADPCM audio
// Input  : *pOut - 16-bit output buffer
//			*pIn - ADPCM encoded block data
//			count - number of sample pairs to decode
//-----------------------------------------------------------------------------
void CAudioMixerWaveADPCM::DecompressBlockStereo( short *pOut, const char *pIn, int count )
{
	int pred[2], co1[2], co2[2];
	int i;

	for ( i = 0; i < 2; i++ )
	{
		pred[i] = *pIn++;
		co1[i] = m_pCoefficients[pred[i]].iCoef1;
		co2[i] = m_pCoefficients[pred[i]].iCoef2;
	}

	int delta[2], samp1[2], samp2[2];

	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		// read initial delta
		delta[i] = *((short *)pIn);
	}

	// read initial samples for prediction
	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		samp1[i] = *((short *)pIn);
	}
	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		samp2[i] = *((short *)pIn);
	}

	// write out the initial samples (stored in reverse order)
	*pOut++ = (short)samp2[0];	// left
	*pOut++ = (short)samp2[1];	// right
	*pOut++ = (short)samp1[0];	// left
	*pOut++ = (short)samp1[1];	// right

	// subtract the 2 samples in the header
	count -= 2;

	// this is a toggle to read nibbles, first nibble is high
	int high = 1;

	int error, sample=0;

	// now process the block
	while ( count )
	{
		for ( i = 0; i < 2; i++ )
		{
			// read the error nibble from the input stream
			if ( high )
			{
				sample = (unsigned char) (*pIn++);
				// high nibble
				error = sample >> 4;
				// cache low nibble for next read
				sample = sample & 0xf;
				// Next read is from cache, not stream
				high = 0;
			}
			else
			{
				// stored in previous read (low nibble)
				error = sample;
				// next read is from stream
				high = 1;
			}
			// convert to signed with LUT
			int errorSign = error_sign_lut[error];

			// interpolate the new sample
			int predSample = (samp1[i] * co1[i]) + (samp2[i] * co2[i]);
			// coefficients are fixed point 8-bit, so shift back to 16-bit integer
			predSample >>= 8;

			// Add in current error estimate
			predSample += (errorSign * delta[i]);

			// Correct error estimate
			delta[i] = (delta[i] * error_coefficients_lut[error]) >> 8;
			// Clamp error estimate
			if ( delta[i] < 16 )
				delta[i] = 16;

			// clamp
			if ( predSample > 32767L )
				predSample = 32767L;
			else if ( predSample < -32768L )
				predSample = -32768L;
			
			// output
			*pOut++ = (short)predSample;
			// move samples over
			samp2[i] = samp1[i];
			samp1[i] = predSample;
		}
		count--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Read data from the source and pass it to the appropriate decompress
//			routine.
// Output : Returns true if data was decoded, false if none.
//-----------------------------------------------------------------------------
bool CAudioMixerWaveADPCM::DecodeBlock( void )
{
	char	tmpBlock[MAX_BLOCK_SIZE];
	char	*pData;
	int		blockSize;
	int		firstSample;

	// fixup position with possible loop
	CAudioSourceWave &source = reinterpret_cast<CAudioSourceWave &>(m_pData->Source());
	m_offset = source.ConvertLoopedPosition( m_offset );

	if ( m_offset >= m_totalBytes )
	{
		// no more data
		return false;
	}

	// can only decode in block sized chunks
	firstSample = m_offset % m_blockSize;
	m_offset    = m_offset - firstSample;

	// adpcm must calculate and request correct block size for proper decoding
	// last block size may be truncated
	blockSize = m_totalBytes - m_offset;
	if ( blockSize > m_blockSize )
	{
		blockSize = m_blockSize;
	}

	// get requested data
	int available = m_pData->ReadSourceData( (void **)(&pData), m_offset, blockSize, NULL );
	if ( available < blockSize )
	{
		// pump to get all of requested data
		int total = 0;
		while ( available && total < blockSize )
		{
			memcpy( tmpBlock + total, pData, available );
			total += available;
			available = m_pData->ReadSourceData( (void **)(&pData), m_offset + total, blockSize - total, NULL );
		}
		pData     = tmpBlock;
		available = total;
	}

	if ( !available )
	{
		// no more data
		return false;
	}

	// advance the file pointer
	m_offset += available;

	int channelCount = NumChannels();

	// this is sample pairs for stereo, samples for mono
	m_sampleCount = m_pFormat->wSamplesPerBlock;

	// short block?, fixup sample count (2 samples per byte, divided by number of channels per sample set)
	m_sampleCount -= ((m_blockSize - available) * 2) / channelCount;

	// new block, start at the first sample
	m_samplePosition = firstSample;

	// no need to subclass for different channel counts...
	if ( channelCount == 1 )
	{
		DecompressBlockMono( m_pSamples, pData, m_sampleCount );
	}
	else
	{
		DecompressBlockStereo( m_pSamples, pData, m_sampleCount );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Read existing buffer or decompress a new block when necessary
// Input  : **pData - output data pointer
//			sampleCount - number of samples (or pairs)
// Output : int - available samples (zero to stop decoding)
//-----------------------------------------------------------------------------
int CAudioMixerWaveADPCM::GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	if ( m_samplePosition >= m_sampleCount )
	{
		if ( !DecodeBlock() )
			return 0;
	}

	if ( m_pSamples && m_samplePosition < m_sampleCount )
	{
		*pData = (void *)(m_pSamples + m_samplePosition * NumChannels());
		int available = m_sampleCount - m_samplePosition;
		if ( available > sampleCount )
			available = sampleCount;

		m_samplePosition += available;

		// update count of max samples loaded in CAudioMixerWave
		CAudioMixerWave::m_sample_max_loaded += available;
	
		// update index of last sample loaded
		CAudioMixerWave::m_sample_loaded_index += available;

		return available;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Seek to a new position in the file
//			NOTE: In most cases, only call this once, and call it before playing
//			any data.
// Input  : newPosition - new position in the sample clocks of this sample
//-----------------------------------------------------------------------------
void CAudioMixerWaveADPCM::SetSampleStart( int newPosition )
{
	// cascade to base wave to update sample counter
	CAudioMixerWave::SetSampleStart( newPosition );
	
	// which block is the desired starting sample in?
	int blockStart = newPosition / m_pFormat->wSamplesPerBlock;
	// how far into the block is the sample
	int blockOffset = newPosition % m_pFormat->wSamplesPerBlock;

	// set the file position
	m_offset = blockStart * m_blockSize;
	
	// NOTE: Must decode a block here to properly position the sample Index
	// THIS MEANS YOU DON'T WANT TO CALL THIS ROUTINE OFTEN FOR ADPCM SOUNDS
	DecodeBlock();
	
	// limit to the samples decoded
	if ( blockOffset < m_sampleCount )
		blockOffset = m_sampleCount;
	
	// set the new current position
	m_samplePosition = blockOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Abstract factory function for ADPCM mixers
// Input  : *data - wave data access object
//			channels - 
// Output : CAudioMixer
//-----------------------------------------------------------------------------
CAudioMixer *CreateADPCMMixer( IWaveData *data )
{
	return new CAudioMixerWaveADPCM( data );
}
