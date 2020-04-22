//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma warning( disable: 4201 )
#include <mmsystem.h>
#pragma warning( default: 4201 )

#include <mmreg.h>
#include "snd_wave_source.h"
#include "snd_wave_mixer_adpcm.h"
#include "snd_wave_mixer_private.h"
#include "soundsystem.h"

// max size of ADPCM block in bytes
#define MAX_BLOCK_SIZE	4096


//-----------------------------------------------------------------------------
// Purpose: Mixer for ADPCM encoded audio
//-----------------------------------------------------------------------------
class CAudioMixerWaveADPCM : public CAudioMixerWave
{
public:
	CAudioMixerWaveADPCM( CWaveData *data );
	~CAudioMixerWaveADPCM( void );
	
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward = true );
	virtual int	 GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward = true );

	virtual bool SetSamplePosition( int position, bool scrubbing = false );

private:
	bool					DecodeBlock( void );
	int						NumChannels( void );
	void					DecompressBlockMono( short *pOut, const char *pIn, int count );
	void					DecompressBlockStereo( short *pOut, const char *pIn, int count );

	void					SetCurrentBlock( int block );
	int						GetCurrentBlock( void ) const;
	int						GetBlockNumberForSample( int samplePosition );
	bool					IsSampleInCurrentBlock( int samplePosition );
	int						GetFirstSampleForBlock( int blocknum ) const;

	const ADPCMWAVEFORMAT	*m_pFormat;
	const ADPCMCOEFSET		*m_pCoefficients;

	short					*m_pSamples;
	int						m_sampleCount;
	int						m_samplePosition;

	int						m_blockSize;
	int						m_offset;

	int						m_currentBlock;
};


CAudioMixerWaveADPCM::CAudioMixerWaveADPCM( CWaveData *data ) : CAudioMixerWave( data ) 
{
	m_currentBlock = -1;
	m_pSamples = NULL;
	m_sampleCount = 0;
	m_samplePosition = 0;
	m_offset = 0;

	m_pFormat = (const ADPCMWAVEFORMAT *)m_pData->Source().GetHeader();
	if ( m_pFormat )
	{
		m_pCoefficients = (ADPCMCOEFSET *)((char *)m_pFormat + sizeof(WAVEFORMATEX) + 4);

		// create the decode buffer
		m_pSamples = new short[m_pFormat->wSamplesPerBlock * m_pFormat->wfx.nChannels];

		// number of bytes for samples
		m_blockSize = ((m_pFormat->wSamplesPerBlock - 2) * m_pFormat->wfx.nChannels ) / 2;
		// size of channel header
		m_blockSize += 7 * m_pFormat->wfx.nChannels;
//		Assert(m_blockSize < MAX_BLOCK_SIZE);
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

void CAudioMixerWaveADPCM::Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward /*= true*/ )
{
	if ( NumChannels() == 1 )
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
	else
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
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

	int error = 0, sample = 0;

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

	int error, sample = 0;

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


bool CAudioMixerWaveADPCM::DecodeBlock( void )
{
	char tmpBlock[MAX_BLOCK_SIZE];
	char *pData;

	int available = m_pData->ReadSourceData( (void **) (&pData), m_offset, m_blockSize );
	if ( available < m_blockSize )
	{
		int total = 0;
		while ( available && total < m_blockSize )
		{
			memcpy( tmpBlock + total, pData, available );
			total += available;
			available = m_pData->ReadSourceData( (void **) (&pData), m_offset + total, m_blockSize - total );
		}
		pData = tmpBlock;
		available = total;
	}

	Assert( m_blockSize > 0 );

	// Current block number is based on starting offset
	int blockNumber = m_offset / m_blockSize;
	SetCurrentBlock( blockNumber );

	if ( !available )
	{
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
	m_samplePosition = 0;

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
// Purpose: 
// Input  : block - 
//-----------------------------------------------------------------------------
void CAudioMixerWaveADPCM::SetCurrentBlock( int block )
{
	m_currentBlock = block;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAudioMixerWaveADPCM::GetCurrentBlock( void ) const
{
	return m_currentBlock;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : samplePosition - 
// Output : int
//-----------------------------------------------------------------------------
int CAudioMixerWaveADPCM::GetBlockNumberForSample( int samplePosition )
{
	int blockNum = samplePosition / m_pFormat->wSamplesPerBlock;
	return blockNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : samplePosition - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioMixerWaveADPCM::IsSampleInCurrentBlock( int samplePosition )
{
	int currentBlock = GetCurrentBlock();

	int startSample = currentBlock * m_pFormat->wSamplesPerBlock;
	int endSample = startSample + m_pFormat->wSamplesPerBlock - 1;

	if ( samplePosition >= startSample &&
		 samplePosition <= endSample )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : blocknum - 
// Output : int
//-----------------------------------------------------------------------------
int CAudioMixerWaveADPCM::GetFirstSampleForBlock( int blocknum ) const
{
	return m_pFormat->wSamplesPerBlock * blocknum;
}

//-----------------------------------------------------------------------------
// Purpose: Read existing buffer or decompress a new block when necessary
// Input  : **pData - output data pointer
//			sampleCount - number of samples (or pairs)
// Output : int - available samples (zero to stop decoding)
//-----------------------------------------------------------------------------
int CAudioMixerWaveADPCM::GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward /*= true*/ )
{
	int requestedBlock = GetBlockNumberForSample( samplePosition );
	if ( requestedBlock != GetCurrentBlock() )
	{
		// Ran out of data!!!
		if ( !SetSamplePosition( samplePosition ) )
			return 0;
	}

	Assert( requestedBlock == GetCurrentBlock() );

	if ( m_samplePosition >= m_sampleCount )
	{
		if ( !DecodeBlock() )
			return 0;
	}

	if ( m_samplePosition < m_sampleCount )
	{
		*pData = (void *)(m_pSamples + m_samplePosition * NumChannels());
		int available = m_sampleCount - m_samplePosition;
		if ( available > sampleCount )
			available = sampleCount;

		m_samplePosition += available;
		return available;
	}

	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : position - 
//-----------------------------------------------------------------------------
bool CAudioMixerWaveADPCM::SetSamplePosition( int position, bool scrubbing )
{
	position = max( 0, position );

	CAudioMixerWave::SetSamplePosition( position, scrubbing );

	int requestedBlock	= GetBlockNumberForSample( position );
	int firstSample		= GetFirstSampleForBlock( requestedBlock );

	if ( firstSample >= m_pData->Source().SampleCount() )
	{
		// Read past end of file!!!
		return false;
	}

	int currentSample = ( position - firstSample );

	if ( requestedBlock != GetCurrentBlock() )
	{
		// Rewind file to beginning of block
		m_offset = requestedBlock * m_blockSize;
		if ( !DecodeBlock() )
		{
			return false;
		}
	}
	
	m_samplePosition = currentSample;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Abstract factory function for ADPCM mixers
// Input  : *data - wave data access object
//			channels - 
// Output : CAudioMixer
//-----------------------------------------------------------------------------
CAudioMixer *CreateADPCMMixer( CWaveData *data )
{
	return new CAudioMixerWaveADPCM( data );
}
