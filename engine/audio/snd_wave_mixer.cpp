//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "audio_pch.h"
#include "fmtstr.h"
#include "sysexternal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool FUseHighQualityPitch( channel_t *pChannel );

//-----------------------------------------------------------------------------
// These mixers provide an abstraction layer between the audio device and 
// mixing/decoding code.  They allow data to be decoded and mixed using 
// optimized, format sensitive code by calling back into the device that
// controls them.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 8-bit mono mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave8Mono : public CAudioMixerWave
{
public:
	CAudioMixerWave8Mono( IWaveData *data ) : CAudioMixerWave( data ) {}
	virtual int GetMixSampleSize() { return CalcSampleSize(8, 1); }
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
	{
		pDevice->Mix8Mono( pChannel, (char *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
};

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 8-bit stereo mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave8Stereo : public CAudioMixerWave
{
public:
	CAudioMixerWave8Stereo( IWaveData *data ) : CAudioMixerWave( data ) {}
	virtual int GetMixSampleSize( ) { return CalcSampleSize(8, 2); }
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
	{
		pDevice->Mix8Stereo( pChannel, (char *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
};

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 16-bit mono mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave16Mono : public CAudioMixerWave
{
public:
	CAudioMixerWave16Mono( IWaveData *data ) : CAudioMixerWave( data ) {}
	virtual int GetMixSampleSize() { return CalcSampleSize(16, 1); }
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
	{
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
};


//-----------------------------------------------------------------------------
// Purpose: maps mixing to 16-bit stereo mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave16Stereo : public CAudioMixerWave
{
public:
	CAudioMixerWave16Stereo( IWaveData *data ) : CAudioMixerWave( data ) {}
	virtual int GetMixSampleSize() { return CalcSampleSize(16, 2); }
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
	{
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
};


//-----------------------------------------------------------------------------
// Purpose: Create an appropriate mixer type given the data format
// Input  : *data - data access abstraction
//			format - pcm or adpcm (1 or 2 -- RIFF format)
//			channels - number of audio channels (1 = mono, 2 = stereo)
//			bits - bits per sample
// Output : CAudioMixer * abstract mixer type that maps mixing to appropriate code
//-----------------------------------------------------------------------------
CAudioMixer *CreateWaveMixer( IWaveData *data, int format, int nChannels, int bits, int initialStreamPosition )
{
	CAudioMixer *pMixer = NULL;

	if ( format == WAVE_FORMAT_PCM )
	{
		if ( nChannels > 1 )
		{
			if ( bits == 8 )
				pMixer = new CAudioMixerWave8Stereo( data );
			else
				pMixer = new CAudioMixerWave16Stereo( data );
		}
		else
		{
			if ( bits == 8 )
				pMixer = new CAudioMixerWave8Mono( data );
			else
				pMixer = new CAudioMixerWave16Mono( data );
		}
	}
	else if ( format == WAVE_FORMAT_ADPCM )
	{
		return CreateADPCMMixer( data );
	}
#if defined( _X360 )
	else if ( format == WAVE_FORMAT_XMA )
	{
		return CreateXMAMixer( data, initialStreamPosition );
	}
#endif
	else
	{
		// unsupported format or wav file missing!!!
		return NULL;
	}

	if ( pMixer )
	{
		Assert( CalcSampleSize(bits, nChannels ) == pMixer->GetMixSampleSize() );
	}
	else
	{
		Assert( 0 );
	}
	return pMixer;
}


//-----------------------------------------------------------------------------
// Purpose: Init the base WAVE mixer.
// Input  : *data - data access object
//-----------------------------------------------------------------------------
CAudioMixerWave::CAudioMixerWave( IWaveData *data ) : m_pData(data)
{
	CAudioSource *pSource = GetSource();
	if ( pSource )
	{
		pSource->ReferenceAdd( this );
	}

	m_fsample_index = 0;
	m_sample_max_loaded = 0;
	m_sample_loaded_index = -1;
	m_finished = false;
	m_forcedEndSample = 0;
	m_delaySamples = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Frees the data access object (we own it after construction)
//-----------------------------------------------------------------------------
CAudioMixerWave::~CAudioMixerWave( void )
{
	CAudioSource *pSource = GetSource();
	if ( pSource )
	{
		pSource->ReferenceRemove( this );
	}
	delete m_pData;
}

bool CAudioMixerWave::IsReadyToMix()
{
	return m_pData->IsReadyToMix();
}

//-----------------------------------------------------------------------------
// Purpose: Decode and read the data
//			by default we just pass the request on to the data access object
//			other mixers may need to buffer or decode the data for some reason
//
// Input  : **pData - dest pointer
//			sampleCount - number of samples needed
// Output : number of samples available in this batch
//-----------------------------------------------------------------------------
int	CAudioMixerWave::GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	int samples_loaded;
	// clear this out in case the underlying code leaves it unmodified
	*pData = NULL;
	samples_loaded = m_pData->ReadSourceData( pData, m_sample_max_loaded, sampleCount, copyBuf );

	// keep track of total samples loaded
	m_sample_max_loaded += samples_loaded;

	// keep track of index of last sample loaded
	m_sample_loaded_index += samples_loaded;

	return samples_loaded;
}


//-----------------------------------------------------------------------------
// Purpose: calls through the wavedata to get the audio source
// Output : CAudioSource
//-----------------------------------------------------------------------------
CAudioSource *CAudioMixerWave::GetSource( void )
{
	if ( m_pData )
		return &m_pData->Source();

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the current sample location in playback (index of next sample
//			to be loaded).
// Output : int (samples from start of wave)
//-----------------------------------------------------------------------------
int CAudioMixerWave::GetSamplePosition( void )
{
	return m_sample_max_loaded;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : delaySamples - 
//-----------------------------------------------------------------------------
void CAudioMixerWave::SetStartupDelaySamples( int delaySamples )
{
	m_delaySamples = delaySamples;
}

// Move the current position to newPosition
void CAudioMixerWave::SetSampleStart( int newPosition )
{
	CAudioSource *pSource = GetSource();
	if ( pSource )
		newPosition = pSource->ZeroCrossingAfter( newPosition );

	m_fsample_index = newPosition;

	// index of last sample loaded - set to sample at new position
	m_sample_loaded_index = newPosition;
	m_sample_max_loaded = m_sample_loaded_index + 1;
}

// End playback at newEndPosition
void CAudioMixerWave::SetSampleEnd( int newEndPosition )
{
	// forced end of zero means play the whole sample
	if ( !newEndPosition )
		newEndPosition = 1;

	CAudioSource *pSource = GetSource();
	if ( pSource )
		newEndPosition = pSource->ZeroCrossingBefore( newEndPosition );

	// past current position?  limit.
	if ( newEndPosition < m_fsample_index )
		newEndPosition = m_fsample_index;

	m_forcedEndSample = newEndPosition;
}

//-----------------------------------------------------------------------------
// Purpose: Skip source data (read but don't mix).  The mixer must provide the
//			full amount of samples or have silence in its output stream.
//-----------------------------------------------------------------------------
int CAudioMixerWave::SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	float flTempPitch = pChannel->pitch;
	pChannel->pitch = 1.0f;
	int nRetVal = MixDataToDevice_( NULL, pChannel, sampleCount, outputRate, outputOffset, true );
	pChannel->pitch = flTempPitch;
	return nRetVal;
}

// wrapper routine to append without overflowing the temp buffer
static uint AppendToBuffer( char *pBuffer, const char *pSampleData, size_t nBytes, const char *pBufferEnd )
{
#if defined(_WIN32) && !defined(_X360)
	// FIXME: Some clients are crashing here. Let's try to detect why.
	if ( nBytes > 0 && ( (size_t)pBuffer <= 0xFFF || (size_t)pSampleData <= 0xFFF ) )
	{
		Warning( "AppendToBuffer received potentially bad values (%p, %p, %u, %p)\n", pBuffer, pSampleData, (int)nBytes, pBufferEnd );
	}
#endif

	if ( pBufferEnd > pBuffer )
	{
		size_t nAvail = pBufferEnd - pBuffer;
		size_t nCopy = MIN( nBytes, nAvail );
		Q_memcpy( pBuffer, pSampleData, nCopy );
		return nCopy;
	}
	else
	{
		return 0;
	}
}

// Load a static copy buffer (g_temppaintbuffer) with the requested number of samples, 
// with the first sample(s) in the buffer always set up as the last sample(s) of the previous load.
// Return a pointer to the head of the copy buffer.
// This ensures that interpolating pitch shifters always have the previous sample to reference.
//		pChannel:				sound's channel data
//		sample_load_request:	number of samples to load from source data
//		pSamplesLoaded:			returns the actual number of samples loaded (should always = sample_load_request)
//		copyBuf:				req'd by GetOutputData, used by some Mixers
// Returns: NULL ptr to data if no samples available, otherwise always fills remainder of copy buffer with
// 0 to pad remainder.
// NOTE: DO NOT MODIFY THIS ROUTINE (KELLYB)
char *CAudioMixerWave::LoadMixBuffer( channel_t *pChannel, int sample_load_request, int *pSamplesLoaded, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	int samples_loaded;
	char *pSample = NULL;
	char *pData = NULL;
	int cCopySamps = 0;

	// save index of last sample loaded (updated in GetOutputData)
	int sample_loaded_index = m_sample_loaded_index;

	// get data from source (copyBuf is expected to be available for use)
	samples_loaded = GetOutputData( (void **)&pData, sample_load_request, copyBuf );
	if ( !samples_loaded && sample_load_request )
	{
		// none available, bail out
		// 360 might not be able to get samples due to latency of loop seek
		// could also be the valid EOF for non-loops (caller keeps polling for data, until no more)
		AssertOnce( IsX360() || !m_pData->Source().IsLooped() );
		*pSamplesLoaded = 0;
		return NULL;
	}

	int samplesize = GetMixSampleSize();
	const int nTempCopyBufferSize = ( TEMP_COPY_BUFFER_SIZE * sizeof( portable_samplepair_t ) );
	char *pCopy = (char *)g_temppaintbuffer;
	const char *pCopyBufferEnd = pCopy + nTempCopyBufferSize;



	if ( IsX360() || IsDebug() )
	{	
		// for safety, 360 always validates sample request, due to new xma audio code and possible logic flaws
		// PC can expect number of requested samples to be within tolerances due to exisiting aged code
		// otherwise buffer overruns cause hard to track random crashes
		if ( ( ( sample_load_request + 1 ) * samplesize ) > nTempCopyBufferSize )
		{
			// make sure requested samples will fit in temp buffer.
			// if this assert fails, then pitch is too high (ie: > 2.0) or the sample counters have diverged.
			// NOTE: to prevent this, pitch should always be capped in MixDataToDevice (but isn't nor are the sample counters).
			DevWarning( "LoadMixBuffer: sample load request %d exceeds buffer sizes\n", sample_load_request );
			Assert( 0 );
			*pSamplesLoaded = 0;
			return NULL;
		}
	}

	// copy all samples from pData to copy buffer, set 0th sample to saved previous sample - this ensures
	// interpolation pitch shift routines always have a previous sample to reference.

	// copy previous sample(s) to head of copy buffer pCopy
	// In some cases, we'll need the previous 2 samples.  This occurs when
	// Rate < 1.0 - in example below, sample 4.86 - 6.48 requires samples 4-7 (previous samples saved are 4 & 5)

	/*
	Example:
		rate = 0.81, sampleCount = 3 (ie: # of samples to return )

		_____load 3______       ____load 3_______       __load 2__

		0		1		 2		 3		 4		 5		6		7		sample_index     (whole samples)

		^     ^      ^      ^      ^     ^     ^     ^     ^        
		|     |      |      |      |     |     |     |     |   
		0    0.81   1.68   2.43   3.24  4.05  4.86  5.67  6.48          m_fsample_index  (rate*sample)
		_______________    ________________   ________________
						^   ^                  ^ ^        	
						|   |                  | |                                    
	m_sample_loaded_index   |                  | m_sample_loaded_index
							|                  |
		m_fsample_index----   		       ----m_fsample_index

		[return 3 samp]     [return 3 samp]    [return 3 samp]
	*/
	pSample = &(pChannel->sample_prev[0]);

	// determine how many saved samples we need to copy to head of copy buffer (0,1 or 2)
	// so that pitch interpolation will correctly reference samples.
	// NOTE: pitch interpolators always reference the sample before and after the indexed sample.

	// cCopySamps = sample_max_loaded - floor(m_fsample_index);

	if ( sample_loaded_index < 0 || (floor(m_fsample_index) > sample_loaded_index))
	{
		// no samples previously loaded, or
		// next sample index is entirely within the next block of samples to be loaded,
		// so we won't need any samples from the previous block. (can occur when rate > 2.0)
		cCopySamps = 0;
	}
	else if ( m_fsample_index < sample_loaded_index )
	{
		// next sample index is entirely within the previous block of samples loaded,
		// so we'll need the last 2 samples loaded.  (can occur when rate < 1.0)		
		Assert ( ceil(m_fsample_index + 0.00000001) == sample_loaded_index );
		cCopySamps = 2;
	}
	else
	{
		// next sample index is between the next block and the previously loaded block,
		// so we'll need the last sample loaded.  (can occur when 1.0 < rate < 2.0)
		Assert( floor(m_fsample_index) == sample_loaded_index );
		cCopySamps = 1;
	}
	Assert( cCopySamps >= 0 && cCopySamps <= 2 );

	// point to the sample(s) we are to copy
	if ( cCopySamps )
	{
		pSample = cCopySamps == 1 ? pSample + samplesize : pSample;
		pCopy += AppendToBuffer( pCopy, pSample, samplesize * cCopySamps, pCopyBufferEnd );
	}

	// copy loaded samples from pData into pCopy
	// and update pointer to free space in copy buffer
	if ( ( samples_loaded * samplesize ) != 0 && !pData )
	{
		char const *pWavName = "";
		CSfxTable *source = pChannel->sfx;
		if ( source )
		{
			pWavName = source->getname();
		}
		
		Warning( "CAudioMixerWave::LoadMixBuffer: '%s' samples_loaded * samplesize = %i but pData == NULL\n", pWavName, ( samples_loaded * samplesize ) );
		*pSamplesLoaded = 0;
		return NULL;
	}

	pCopy += AppendToBuffer( pCopy, pData, samples_loaded * samplesize, pCopyBufferEnd );

	// if we loaded fewer samples than we wanted to, and we're not
	// delaying, load more samples or, if we run out of samples from non-looping source, 
	// pad copy buffer.
	if ( samples_loaded < sample_load_request )
	{
		// retry loading source data until 0 bytes returned, or we've loaded enough data.
		// if we hit 0 bytes, fill remaining space in copy buffer with 0 and exit
		int samples_load_extra;
		int samples_loaded_retry = -1;
			
		for ( int k = 0; (k < 10000 && samples_loaded_retry && samples_loaded < sample_load_request); k++ )
		{
			// how many more samples do we need to satisfy load request
			samples_load_extra = sample_load_request - samples_loaded;
			samples_loaded_retry = GetOutputData( (void**)&pData, samples_load_extra, copyBuf );

			// copy loaded samples from pData into pCopy
			if ( samples_loaded_retry )
			{
				if ( ( samples_loaded_retry * samplesize ) != 0 && !pData )
				{
					Warning( "CAudioMixerWave::LoadMixBuffer:  samples_loaded_retry * samplesize = %i but pData == NULL\n", ( samples_loaded_retry * samplesize ) );
					*pSamplesLoaded = 0;
					return NULL;
				}

				pCopy += AppendToBuffer( pCopy, pData, samples_loaded_retry * samplesize, pCopyBufferEnd );
				samples_loaded += samples_loaded_retry;
			}
		}
	}

	// if we still couldn't load the requested samples, fill rest of copy buffer with 0
	if ( samples_loaded < sample_load_request )
	{
		// should always be able to get as many samples as we request from looping sound sources
		AssertOnce ( IsX360() || !m_pData->Source().IsLooped() );

		// these samples are filled with 0, not loaded.
		// non-looping source hit end of data, fill rest of g_temppaintbuffer with 0
		int samples_zero_fill = sample_load_request - samples_loaded;

		int nAvail = pCopyBufferEnd - pCopy;
		int nFill = samples_zero_fill * samplesize;
		nFill = MIN( nAvail, nFill );
		Q_memset( pCopy, 0, nFill );
		pCopy += nFill;
		samples_loaded += samples_zero_fill;
	}

	if ( samples_loaded >= 2 )
	{
		// always save last 2 samples from copy buffer to channel 
		// (we'll need 0,1 or 2 samples as start of next buffer for interpolation)
		Assert( sizeof( pChannel->sample_prev ) >= samplesize*2 );
		pSample = pCopy - samplesize*2;
		Q_memcpy( &(pChannel->sample_prev[0]), pSample, samplesize*2 );
	}

	// this routine must always return as many samples loaded (or zeros) as requested.
	Assert( samples_loaded == sample_load_request );

	*pSamplesLoaded = samples_loaded;

	return (char *)g_temppaintbuffer;
}

// Helper routine to round (rate * samples) down to fixed point precision

double RoundToFixedPoint( double rate, int samples, bool bInterpolated_pitch )
{
	fixedint fixp_rate;
	int64 d64_newSamps;		// need to use double precision int to avoid overflow

	double newSamps;

	// get rate, in fixed point, determine new samples at rate

	if ( bInterpolated_pitch )
		fixp_rate = FIX_FLOAT14(rate);		// 14 bit iterator
	else
		fixp_rate = FIX_FLOAT(rate);		// 28 bit iterator
	
	// get number of new samples, convert back to float

	d64_newSamps = (int64)fixp_rate * (int64)samples;

	if ( bInterpolated_pitch )
		newSamps = FIX_14TODOUBLE(d64_newSamps);
	else
		newSamps = FIX_TODOUBLE(d64_newSamps);

	return newSamps;
}

extern double MIX_GetMaxRate( double rate, int sampleCount );

// Helper routine for MixDataToDevice:
// Compute number of new samples to load at 'rate' so we can 
// output 'sampleCount' samples, from m_fsample_index to fsample_index_end (inclusive)
// rate:				sample rate
// sampleCountOut:		number of samples calling routine needs to output
// bInterpolated_pitch: true if mixers use interpolating pitch shifters
int CAudioMixerWave::GetSampleLoadRequest( double rate, int sampleCountOut, bool bInterpolated_pitch )
{
	double fsample_index_end;		// index of last sample we'll need
	int sample_index_high;			// rounded up last sample index
	int sample_load_request;		// number of samples to load

	// NOTE: we must use fixed point math here, identical to math in mixers, to make sure
	// we predict iteration results exactly.
	// get floating point sample index of last sample we'll need
	fsample_index_end = m_fsample_index + RoundToFixedPoint( rate, sampleCountOut-1, bInterpolated_pitch );				

	// always round up to ensure we'll have that n+1 sample for interpolation	
	sample_index_high = (int)( ceil( fsample_index_end ) );															
	
	// make sure we always round the floating point index up by at least 1 sample,
	// ie: make sure integer sample_index_high is greater than floating point sample index
	if ( (double)sample_index_high <= fsample_index_end )
	{
		sample_index_high++;
	}
	Assert ( sample_index_high > fsample_index_end );

	// attempt to load enough samples so we can reach sample_index_high sample.
	sample_load_request = sample_index_high - m_sample_loaded_index;
	Assert( sample_index_high >= m_sample_loaded_index );

	// NOTE: we can actually return 0 samples to load if rate < 1.0
	// and sampleCountOut == 1.  In this case, the output sample
	// is computed from the previously saved buffer data.
	return sample_load_request;
}

int CAudioMixerWave::MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	return MixDataToDevice_(pDevice, pChannel, sampleCount, outputRate, outputOffset, false );
}

//-----------------------------------------------------------------------------
// Purpose: The device calls this to request data.  The mixer must provide the
//			full amount of samples or have silence in its output stream.
//			Mix channel to all active paintbuffers.
//			NOTE: cannot be called consecutively to mix into multiple paintbuffers!
// Input  : *pDevice - requesting device
//			sampleCount - number of samples at the output rate - should never be more than size of paintbuffer.
//			outputRate - sampling rate of the request
//			outputOffset - starting offset to mix to in paintbuffer
//			bskipallmixing - true if we just want to skip ahead in source data

// Output : Returns true to keep mixing, false to delete this mixer

// NOTE:	DO NOT MODIFY THIS ROUTINE (KELLYB)

//-----------------------------------------------------------------------------
int CAudioMixerWave::MixDataToDevice_( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset, bool bSkipAllMixing )
{
	// shouldn't be playing this if finished, but return if we are
	if ( m_finished )
		return 0;

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// save this to compute total output
	int startingOffset = outputOffset;

	double inputRate = (pChannel->pitch * m_pData->Source().SampleRate());
	double rate_max = inputRate / outputRate;
	
	// If we are terminating this wave prematurely, then make sure we detect the limit
	if ( m_forcedEndSample )
	{
		// How many total input samples will we need?
		int samplesRequired = (int)(sampleCount * rate_max);
		// will this hit the end?
		if ( m_fsample_index + samplesRequired >= m_forcedEndSample )
		{
			// yes, mark finished and truncate the sample request
			m_finished = true;
			sampleCount = (int)( (m_forcedEndSample - m_fsample_index) / rate_max );
		}
	}

	/*
	Example:
	rate = 1.2, sampleCount = 3 (ie: # of samples to return )

	______load 4 samples_____       ________load 4 samples____     ___load 3 samples__

	0		1		2		3		4		5		6		7		8		9		10		sample_index     (whole samples)

	^         ^         ^        ^        ^         ^         ^         ^         ^
	|         |         |        |        |         |         |         |         |     
	0		 1.2	   2.4      3.6      4.8       6.0       7.2	    8.4		  9.6		m_fsample_index  (rate*sample)
	_______return 3_______      _______return 3_______       _______return 3__________
	 						 ^   ^
							 |   |
	m_sample_loaded_index-----   |     		(after first load 4 samples, this is where pointers are)
		  m_fsample_index---------
	*/
	while ( sampleCount > 0 )
	{	
		bool advanceSample = true;
		int samples_loaded, outputSampleCount;
		char *pData = NULL;
		double fsample_index_prev = m_fsample_index;		// save so we can modify in LoadMixBuffer
		bool bInterpolated_pitch = FUseHighQualityPitch( pChannel );
		double rate;

		VPROF_( bInterpolated_pitch ? "CAudioMixerWave::MixData innerloop interpolated" : "CAudioMixerWave::MixData innerloop not interpolated", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );

		// process samples in paintbuffer-sized batches
		int sampleCountOut = min( sampleCount, PAINTBUFFER_SIZE );
	
		// cap rate so that we never overflow the input copy buffer.
		rate = MIX_GetMaxRate( rate_max, sampleCountOut );

		if ( m_delaySamples > 0 )
		{
			// If we are preceding sample playback with a delay, 
			// just fill data buffer with 0 value samples.
			// Because there is no pitch shift applied, outputSampleCount == sampleCountOut.
			int num_zero_samples = min( m_delaySamples, sampleCountOut );

			// Decrement delay counter
			m_delaySamples -= num_zero_samples;

			int sampleSize = GetMixSampleSize(); 
			int readBytes = sampleSize * num_zero_samples;

			// make sure we don't overflow temp copy buffer (g_temppaintbuffer)
			Assert ( (TEMP_COPY_BUFFER_SIZE * sizeof(portable_samplepair_t)) > readBytes );
			pData = (char *)g_temppaintbuffer;

			// Now copy in some zeroes
			memset( pData, 0, readBytes );
						
			// we don't pitch shift these samples, so outputSampleCount == samples_loaded
			samples_loaded	  = num_zero_samples;
			outputSampleCount = num_zero_samples;		

			advanceSample = false;

			// the zero samples are at the output rate, so set the input/output ratio to 1.0
			rate = 1.0f;
		}
		else
		{	
			// ask the source for the data...
			// temp buffer req'd by some data loaders
			char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];

			// compute number of new samples to load at 'rate' so we can 
			// output 'sampleCount' samples, from m_fsample_index to fsample_index_end (inclusive)
			int sample_load_request = GetSampleLoadRequest( rate, sampleCountOut, bInterpolated_pitch );

			// return pointer to a new copy buffer (g_temppaintbuffer) loaded with sample_load_request samples +
			// first sample(s), which are always the last sample(s) from the previous load.
			// Always returns sample_load_request samples. Updates m_sample_max_loaded, m_sample_loaded_index.
			pData = LoadMixBuffer( pChannel, sample_load_request, &samples_loaded, copyBuf );

			// LoadMixBuffer should always return requested samples.
			Assert ( !pData || ( samples_loaded == sample_load_request ) );

			outputSampleCount = sampleCountOut;
		}	
		
		// no samples available
		if ( !pData )
		{
			break;
		}
		
		// get sample fraction from 0th sample in copy buffer
		double sampleFraction = m_fsample_index - floor( m_fsample_index );
		
		// if just skipping samples in source, don't mix, just keep reading
		if ( !bSkipAllMixing )
		{
			// mix this data to all active paintbuffers
			// Verify that we won't get a buffer overrun.
			Assert( floor( sampleFraction + RoundToFixedPoint(rate, (outputSampleCount-1), bInterpolated_pitch) ) <= samples_loaded );

			int saveIndex = MIX_GetCurrentPaintbufferIndex();
			for ( int i = 0 ; i < g_paintBuffers.Count(); i++ )
			{
				if ( g_paintBuffers[i].factive )
				{
					// mix channel into all active paintbuffers
					MIX_SetCurrentPaintbuffer( i );

					Mix( 
						pDevice,						// Device.
						pChannel,						// Channel.
						pData,							// Input buffer.
						outputOffset,					// Output position.
						FIX_FLOAT( sampleFraction ),	// Iterators.
						FIX_FLOAT( rate ), 
						outputSampleCount,	
						0 );
				}
			}
			MIX_SetCurrentPaintbuffer( saveIndex );
		}

		if ( advanceSample )
		{
			// update sample index to point to the next sample to output
			// if we're not delaying
			// Use fixed point math to make sure we exactly match results of mix
			// iterators.			
			m_fsample_index = fsample_index_prev + RoundToFixedPoint( rate, outputSampleCount, bInterpolated_pitch );
		}

		outputOffset += outputSampleCount;
		sampleCount -= outputSampleCount;
	}

	// Did we run out of samples? if so, mark finished
	if ( sampleCount > 0 )
	{
		m_finished = true;
	}

	// total number of samples mixed !!! at the output clock rate !!!
	return outputOffset - startingOffset;
}


bool CAudioMixerWave::ShouldContinueMixing( void )
{
	return !m_finished;
}

float CAudioMixerWave::ModifyPitch( float pitch )
{
	return pitch;
}

float CAudioMixerWave::GetVolumeScale( void )
{
	return 1.0f;
}

