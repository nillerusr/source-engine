//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portable code to mix sounds for snd_dma.cpp.
//
//=============================================================================//

#include "audio_pch.h"

#include "mouthinfo.h"
#include "../../cl_main.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "../../sys_dll.h"
#include "video/ivideoservices.h"
#include "engine/IEngineSound.h"

#if defined( REPLAY_ENABLED )
#include "demo.h"
#include "replay_internal.h"
#endif
#ifdef GNUC
// we don't suport the ASM in this file right now under GCC, fallback to C libs
#undef id386
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( REPLAY_ENABLED )
extern IReplayMovieManager *g_pReplayMovieManager;
#endif

#if defined(_WIN32) && id386
// warning C4731: frame pointer register 'ebp' modified by inline assembly code
#pragma warning(disable : 4731)
#endif

// NOTE: !!!!!! YOU MUST UPDATE SND_MIXA.S IF THIS VALUE IS CHANGED !!!!!
#define SND_SCALE_BITS		7
#define SND_SCALE_SHIFT		(8-SND_SCALE_BITS)
#define SND_SCALE_LEVELS	(1<<SND_SCALE_BITS)

#define SND_SCALE_BITS16	8
#define SND_SCALE_SHIFT16	(8-SND_SCALE_BITS16)
#define SND_SCALE_LEVELS16	(1<<SND_SCALE_BITS16)

void Snd_WriteLinearBlastStereo16(void);
void SND_PaintChannelFrom8( portable_samplepair_t *pOutput, int *volume, byte *pData8, int count );
bool Con_IsVisible( void );
void SND_RecordBuffer( void );
bool DSP_RoomDSPIsOff( void );
bool BChannelLowVolume( channel_t *pch, int vol_min );
void ChannelCopyVolumes( channel_t *pch, int *pvolume_dest, int ivol_start, int cvol );
float ChannelLoudestCurVolume( const channel_t * RESTRICT pch );

extern int g_soundtime;
extern float host_frametime;
extern float host_frametime_unbounded;


#if !defined( NO_VOICE )
extern int g_SND_VoiceOverdriveInt;
#endif

extern ConVar dsp_room;
extern ConVar dsp_water;
extern ConVar dsp_player;
extern ConVar dsp_facingaway;
extern ConVar snd_showstart;
extern ConVar dsp_automatic;
extern ConVar snd_pitchquality;

extern float DSP_ROOM_MIX;
extern float DSP_NOROOM_MIX;

portable_samplepair_t *g_paintbuffer;

// temp paintbuffer - not included in main list of paintbuffers
// NOTE: this paintbuffer is also used as a copy buffer by interpolating pitch
// shift routines.  Decreasing TEMP_COPY_BUFFER_SIZE (or PAINTBUFFER_MEM_SIZE)
// will decrease the maximum pitch level (current 4.0)!
portable_samplepair_t *g_temppaintbuffer = NULL;

CUtlVector< paintbuffer_t > g_paintBuffers;


// pointer to current paintbuffer (front and reare), used by all mixing, upsampling and dsp routines
portable_samplepair_t *g_curpaintbuffer = NULL;
portable_samplepair_t *g_currearpaintbuffer = NULL;	
portable_samplepair_t *g_curcenterpaintbuffer = NULL;

bool g_bdirectionalfx;
bool g_bDspOff;
float g_dsp_volume;

// dsp performance timing
unsigned g_snd_call_time_debug = 0;
unsigned g_snd_time_debug = 0;
unsigned g_snd_count_debug = 0;
unsigned g_snd_samplecount = 0;
unsigned g_snd_frametime = 0;
unsigned g_snd_frametime_total = 0;
int	g_snd_profile_type = 0;		// type 1 dsp, type 2 mixer, type 3 load sound, type 4 all sound

#define FILTERTYPE_NONE		0
#define FILTERTYPE_LINEAR	1
#define FILTERTYPE_CUBIC	2

// filter memory for upsampling
portable_samplepair_t cubicfilter1[3] = {{0,0},{0,0},{0,0}};
portable_samplepair_t cubicfilter2[3] = {{0,0},{0,0},{0,0}};

portable_samplepair_t linearfilter1[1] = {{0,0}};
portable_samplepair_t linearfilter2[1] = {{0,0}};
portable_samplepair_t linearfilter3[1] = {{0,0}};
portable_samplepair_t linearfilter4[1] = {{0,0}};
portable_samplepair_t linearfilter5[1] = {{0,0}};
portable_samplepair_t linearfilter6[1] = {{0,0}};
portable_samplepair_t linearfilter7[1] = {{0,0}};
portable_samplepair_t linearfilter8[1] = {{0,0}};

int		snd_scaletable[SND_SCALE_LEVELS][256];	// 32k*4 = 128K

int 	*snd_p, snd_linear_count, snd_vol;
short	*snd_out;

extern int DSP_Alloc( int ipset, float xfade, int cchan );

bool DSP_CheckDspAutoEnabled( void );
int Get_idsp_room ( void );
int dsp_room_GetInt ( void );
void DSP_SetDspAuto( int dsp_preset );
bool DSP_CheckDspAutoEnabled( void );

void MIX_ScalePaintBuffer( int bufferIndex, int count, float fgain );

bool IsReplayRendering()
{
#if defined( REPLAY_ENABLED )
	return g_pReplayMovieManager && g_pReplayMovieManager->IsRendering();
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Free allocated memory buffers
//-----------------------------------------------------------------------------
void MIX_FreeAllPaintbuffers(void)
{		
	if ( g_paintBuffers.Count() )
	{
		if ( g_temppaintbuffer )
		{
			_aligned_free( g_temppaintbuffer );
			g_temppaintbuffer = NULL;
		}

		for ( int i = 0; i < g_paintBuffers.Count(); i++ )
		{
			if ( g_paintBuffers[i].pbuf )
			{
				_aligned_free( g_paintBuffers[i].pbuf );
			}
			if ( g_paintBuffers[i].pbufrear )
			{
				_aligned_free( g_paintBuffers[i].pbufrear );
			}
			if ( g_paintBuffers[i].pbufcenter )
			{
				_aligned_free( g_paintBuffers[i].pbufcenter );
			}
		}

		g_paintBuffers.RemoveAll();
	}
}

void MIX_InitializePaintbuffer( paintbuffer_t *pPaintBuffer, bool bSurround, bool bSurroundCenter )
{
	V_memset( pPaintBuffer, 0, sizeof( *pPaintBuffer ) );

	pPaintBuffer->pbuf = (portable_samplepair_t *)_aligned_malloc( PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t), 16 );
	V_memset( pPaintBuffer->pbuf, 0, PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t) );

	if ( bSurround )
	{
		pPaintBuffer->pbufrear = (portable_samplepair_t *)_aligned_malloc( PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t), 16 );
		V_memset( pPaintBuffer->pbufrear, 0, PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t) );
	}
	if ( bSurroundCenter )
	{
		pPaintBuffer->pbufcenter = (portable_samplepair_t *)_aligned_malloc( PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t), 16 );
		V_memset( pPaintBuffer->pbufcenter, 0, PAINTBUFFER_MEM_SIZE*sizeof(portable_samplepair_t) );
	}
}

//-----------------------------------------------------------------------------
// Allocate memory buffers
// Initialize paintbuffers array, set current paint buffer to main output buffer SOUND_BUFFER_PAINT
//-----------------------------------------------------------------------------
bool MIX_InitAllPaintbuffers(void)
{
	bool	bSurround;
	bool	bSurroundCenter;

	bSurroundCenter = g_AudioDevice->IsSurroundCenter();
	bSurround = g_AudioDevice->IsSurround() || bSurroundCenter;

	g_temppaintbuffer = (portable_samplepair_t*)_aligned_malloc( TEMP_COPY_BUFFER_SIZE*sizeof(portable_samplepair_t), 16 );
	V_memset( g_temppaintbuffer, 0, TEMP_COPY_BUFFER_SIZE*sizeof(portable_samplepair_t) );

	while ( g_paintBuffers.Count() < SOUND_BUFFER_BASETOTAL )
	{
		int nIndex = g_paintBuffers.AddToTail();
		MIX_InitializePaintbuffer( &(g_paintBuffers[ nIndex ]), bSurround, bSurroundCenter );
	}

	g_paintbuffer = g_paintBuffers[SOUND_BUFFER_PAINT].pbuf;

	// buffer flags
	g_paintBuffers[SOUND_BUFFER_ROOM].flags = SOUND_BUSS_ROOM;
	g_paintBuffers[SOUND_BUFFER_FACING].flags = SOUND_BUSS_FACING;
	g_paintBuffers[SOUND_BUFFER_FACINGAWAY].flags = SOUND_BUSS_FACINGAWAY;
	g_paintBuffers[SOUND_BUFFER_SPEAKER].flags = SOUND_BUSS_SPEAKER;
	g_paintBuffers[SOUND_BUFFER_DRY].flags = SOUND_BUSS_DRY;

	// buffer surround sound flag
	g_paintBuffers[SOUND_BUFFER_PAINT].fsurround = bSurround;
	g_paintBuffers[SOUND_BUFFER_FACING].fsurround = bSurround;
	g_paintBuffers[SOUND_BUFFER_FACINGAWAY].fsurround	= bSurround;
	g_paintBuffers[SOUND_BUFFER_DRY].fsurround = bSurround;

	// buffer 5 channel surround sound flag
	g_paintBuffers[SOUND_BUFFER_PAINT].fsurround_center = bSurroundCenter;
	g_paintBuffers[SOUND_BUFFER_FACING].fsurround_center = bSurroundCenter;
	g_paintBuffers[SOUND_BUFFER_FACINGAWAY].fsurround_center = bSurroundCenter;
	g_paintBuffers[SOUND_BUFFER_DRY].fsurround_center = bSurroundCenter;
	
	// room buffer mixes down to mono or stereo, never to 4 or 5 ch
	g_paintBuffers[SOUND_BUFFER_ROOM].fsurround = false;
	g_paintBuffers[SOUND_BUFFER_ROOM].fsurround_center = false;

	// speaker buffer mixes to mono
	g_paintBuffers[SOUND_BUFFER_SPEAKER].fsurround = false;
	g_paintBuffers[SOUND_BUFFER_SPEAKER].fsurround_center	= false;	

	MIX_SetCurrentPaintbuffer( SOUND_BUFFER_PAINT );

	return true;
}

// called before loading samples to mix  - cap the mix rate (ie: pitch) so that
// we never overflow the mix copy buffer.

double MIX_GetMaxRate( double rate, int sampleCount )
{
	if (rate <= 2.0)
		return rate;

	// copybuf_bytes = rate_max * samples_max * samplesize_max
	// so: 
	// rate_max = copybuf_bytes /  (samples_max * samplesize_max )

	double samplesize_max  = 4.0; // stereo 16bit samples
	double copybuf_bytes   = (double)(TEMP_COPY_BUFFER_SIZE * sizeof(portable_samplepair_t));
	double samples_max     = (double)(PAINTBUFFER_SIZE);

	double rate_max = copybuf_bytes / (samples_max * samplesize_max);

	// make sure sampleCount is never greater than paintbuffer samples
	// (this should have been set up in MIX_PaintChannels)

	Assert (sampleCount <= PAINTBUFFER_SIZE);

	return fpmin( rate, rate_max );
}


// Transfer (endtime - lpaintedtime) stereo samples in pfront out to hardware
// pfront - pointer to stereo paintbuffer - 32 bit samples, interleaved stereo
// lpaintedtime - total number of 32 bit stereo samples previously output to hardware
// endtime - total number of 32 bit stereo samples currently mixed in paintbuffer
void S_TransferStereo16( void *pOutput, const portable_samplepair_t *pfront, int lpaintedtime, int endtime )
{
	int		lpos;
	
	if ( IsX360() )
	{
		// not the right path for 360
		Assert( 0 );
		return;
	}

	Assert( pOutput );

	snd_vol = S_GetMasterVolume()*256;
	snd_p = (int *)pfront;
	
	// get size of output buffer in full samples (LR pairs)
	int samplePairCount = g_AudioDevice->DeviceSampleCount() >> 1;
	int sampleMask = samplePairCount  - 1;

	bool bShouldPlaySound = !cl_movieinfo.IsRecording() && !IsReplayRendering();

	while ( lpaintedtime < endtime )
	{														
		// pbuf can hold 16384, 16 bit L/R samplepairs.
		// lpaintedtime - where to start painting into dma buffer. 
		// (modulo size of dma buffer for current position).
		// handle recirculating buffer issues
		// lpos - samplepair index into dma buffer. First samplepair from paintbuffer to be xfered here.															
		lpos = lpaintedtime & sampleMask;

		// snd_out is L/R sample index into dma buffer.  First L sample from paintbuffer goes here.
		snd_out = (short *)pOutput + (lpos<<1);

		// snd_linear_count is number of samplepairs between end of dma buffer and xfer start index.
		snd_linear_count = samplePairCount - lpos;

		// clamp snd_linear_count to be only as many samplepairs premixed
		if ( snd_linear_count > endtime - lpaintedtime )
		{
			// endtime - lpaintedtime = number of premixed sample pairs ready for xfer.
			snd_linear_count = endtime - lpaintedtime;
		}

		// snd_linear_count is now number of mono 16 bit samples (L and R) to xfer.
		snd_linear_count <<= 1;

		// write a linear blast of samples
		SND_RecordBuffer();
		if ( bShouldPlaySound )
		{
			// transfer 16bit samples from snd_p into snd_out, multiplying each sample by volume.
			Snd_WriteLinearBlastStereo16();
		}

		// advance paintbuffer pointer
		snd_p += snd_linear_count;

		// advance lpaintedtime by number of samplepairs just xfered.
		lpaintedtime += (snd_linear_count>>1);				
	}
}

// Transfer contents of main paintbuffer pfront out to 
// device.  Perform volume multiply on each sample.
void S_TransferPaintBuffer(void *pOutput, const portable_samplepair_t *pfront, int lpaintedtime, int endtime)
{
	int 		out_idx;		// mono sample index
	int 		count;			// number of mono samples to output
	int 		out_mask;
	int 		step;
	int			val;
	int			nSoundVol;
	const int 	*p;
 
	if ( IsX360() )
	{
		// not the right path for 360
		Assert( 0 );
		return;
	}

	Assert( pOutput );

	p = (const int *) pfront;

	count = ((endtime - lpaintedtime) * g_AudioDevice->DeviceChannels()); 
	
	out_mask = g_AudioDevice->DeviceSampleCount() - 1; 

	// 44k: remove old 22k sound support << HISPEED_DMA
	// out_idx = ((paintedtime << HISPEED_DMA) * g_AudioDevice->DeviceChannels()) & out_mask;

	out_idx = (lpaintedtime * g_AudioDevice->DeviceChannels()) & out_mask;
	
	step = 3 - g_AudioDevice->DeviceChannels();	// mono output buffer - step 2, stereo - step 1
	nSoundVol = S_GetMasterVolume()*256;

	if (g_AudioDevice->DeviceSampleBits() == 16)
	{
		short *out = (short *) pOutput;
		while (count--)
		{
			val = (*p * nSoundVol) >> 8;
			p+= step;
			val = CLIP(val);
			
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
	else if (g_AudioDevice->DeviceSampleBits() == 8)
	{
		unsigned char *out = (unsigned char *) pOutput;
		while (count--)
		{
			val = (*p * nSoundVol) >> 8;
			p+= step;
			val = CLIP(val);
			
			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/


// free channel so that it may be allocated by the
// next request to play a sound.  If sound is a 
// word in a sentence, release the sentence.
// Works for static, dynamic, sentence and stream sounds

void S_FreeChannel(channel_t *ch)
{
	// Don't reenter in here (can happen inside voice code).
	if ( ch->flags.m_bIsFreeingChannel )
		return;
	ch->flags.m_bIsFreeingChannel = true;

	SND_CloseMouth(ch);

	g_pSoundServices->OnSoundStopped( ch->guid, ch->soundsource, ch->entchannel, ch->sfx->getname() );

	ch->flags.isSentence = false;
//	Msg("End sound %s\n", ch->sfx->getname() );
	
	delete ch->pMixer;
	ch->pMixer = NULL;
	ch->sfx = NULL;

	// zero all data in channel
	g_ActiveChannels.Remove( ch );
	Q_memset(ch, 0, sizeof(channel_t));
}


// Mix all channels into active paintbuffers until paintbuffer is full or 'endtime' is reached.
// endtime: time in 44khz samples to mix
// rate: ignore samples which are not natively at this rate (for multipass mixing/filtering)
//		 if rate == SOUND_ALL_RATES then mix all samples this pass
// flags: if SOUND_MIX_DRY, then mix only samples with channel flagged as 'dry'
// outputRate: target mix rate for all samples.  Note, if outputRate = SOUND_DMA_SPEED, then
//		 this routine will fill the paintbuffer to endtime.  Otherwise, fewer samples are mixed.
//		 if (endtime - paintedtime) is not aligned on boundaries of 4, 
//		 we'll miss data if outputRate < SOUND_DMA_SPEED!
void MIX_MixChannelsToPaintbuffer( CChannelList &list, int endtime, int flags, int rate, int outputRate )
{
	VPROF( "MixChannelsToPaintbuffer" );
	int		i;
	int		sampleCount;

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s c:%d %d/%d", __FUNCTION__, list.Count(), rate, outputRate );

	// mix each channel into paintbuffer
	// validate parameters
	Assert( outputRate <= SOUND_DMA_SPEED );
	Assert( !((endtime - g_paintedtime) & 0x3) || (outputRate == SOUND_DMA_SPEED) ); // make sure we're not discarding data
											  
	// 44k: try to mix this many samples at outputRate
	sampleCount = ( endtime - g_paintedtime ) / ( SOUND_DMA_SPEED / outputRate );
	if ( sampleCount <= 0 )
		return;

	// Apply a global pitch shift if we're playing back a time-scaled replay
	float flGlobalPitchScale = 1.0f;

#if defined( REPLAY_ENABLED )
	extern IDemoPlayer *g_pReplayDemoPlayer;
	if ( demoplayer->IsPlayingBack() && demoplayer == g_pReplayDemoPlayer )
	{
		// adjust time scale if playing back demo
		flGlobalPitchScale = demoplayer->GetPlaybackTimeScale();
	}
#endif

	for ( i = list.Count(); --i >= 0; )
	{
		channel_t *ch = list.GetChannel( i );
		Assert( ch->sfx );
		// must never have a 'dry' and 'speaker' set - causes double mixing & double data reading
		Assert ( !( ( ch->flags.bdry && ch->flags.bSpeaker ) || ( ch->flags.bdry && ch->special_dsp != 0 ) ) );	

		// if mixing with SOUND_MIX_DRY flag, ignore (don't even load) all channels not flagged as 'dry'
		if ( flags == SOUND_MIX_DRY )
		{
			if ( !ch->flags.bdry )
				continue;
		}
		
		// if mixing with SOUND_MIX_WET flag, ignore (don't even load) all channels flagged as 'dry' or 'speaker'
		if ( flags == SOUND_MIX_WET )
		{
			if ( ch->flags.bdry || ch->flags.bSpeaker || ch->special_dsp != 0 )
				continue;
		}

		// if mixing with SOUND_MIX_SPEAKER flag, ignore (don't even load) all channels not flagged as 'speaker'
		if ( flags == SOUND_MIX_SPEAKER )
		{
			if ( !ch->flags.bSpeaker )
				continue;
		}

		// if mixing with SOUND_MIX_SPEAKER flag, ignore (don't even load) all channels not flagged as 'speaker'
		if ( flags == SOUND_MIX_SPECIAL_DSP )
		{
			if ( ch->special_dsp == 0 )
				continue;
		}

		// multipass mixing - only mix samples of specified sample rate
		switch ( rate )
		{
		case SOUND_11k:
		case SOUND_22k:
		case SOUND_44k:
			if ( rate != ch->sfx->pSource->SampleRate() )
				continue;
			break;
		default:
		case SOUND_ALL_RATES:
			break;
		}

		// Tracker 20771, if breen is speaking through the monitor, the client doesn't have an entity
		//  for the "soundsource" but we still need the lipsync to pause if the game is paused.  Therefore
		//  I changed SND_IsMouth to look for any .wav on any channels which has sentence data
		bool bIsMouth = SND_IsMouth(ch);
		bool bShouldPause = IsX360() ? !ch->sfx->m_bIsUISound : bIsMouth; 

		// Tracker 14637:  Pausing the game pauses voice sounds, but not other sounds...
		if ( bShouldPause && g_pSoundServices->IsGamePaused() )
		{
			continue;
		}

		if ( bIsMouth )
		{
			if ( ( ch->soundsource == SOUND_FROM_UI_PANEL ) || entitylist->GetClientEntity(ch->soundsource) || 
				( ch->flags.bSpeaker && entitylist->GetClientEntity( ch->speakerentity ) ) )
			{
				// UNDONE: recode this as a member function of CAudioMixer
				SND_MoveMouth8(ch, ch->sfx->pSource, sampleCount);
			}
		}

		// mix channel to all active paintbuffers:
		// mix 'dry' sounds only to dry paintbuffer.
		// mix 'speaker' sounds only to speaker paintbuffer.
		// mix all other sounds between room, facing & facingaway paintbuffers
		// NOTE: must be called once per channel only - consecutive calls retrieve additional data.
		float flPitch = ch->pitch;
		ch->pitch *= flGlobalPitchScale;

		if (list.IsQuashed(i))
		{
			// If the sound has been silenced as a performance heuristic, quash it.
			ch->pMixer->SkipSamples( ch, sampleCount, outputRate, 0 );
			// DevMsg("Quashed channel %d (%s)\n", i, ch->sfx->GetFileName());
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "MixDataToDevice" );
			ch->pMixer->MixDataToDevice( g_AudioDevice, ch, sampleCount, outputRate, 0 );
		}

		// restore to original pitch settings
		ch->pitch = flPitch;

		if ( !ch->pMixer->ShouldContinueMixing() )
		{
			S_FreeChannel( ch );
			list.RemoveChannelFromList(i);
		}
		if ( (ch->nFreeChannelAtSampleTime > 0 && (int)ch->nFreeChannelAtSampleTime <= endtime) )
		{
			S_FreeChannel( ch );
			list.RemoveChannelFromList(i);
		}
	}
}

// pass in index -1...count+2, return pointer to source sample in either paintbuffer or delay buffer
inline portable_samplepair_t * S_GetNextpFilter(int i, portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem)
{
	// The delay buffer is assumed to precede the paintbuffer by 6 duplicated samples
	if (i == -1)
		return (&(pfiltermem[0]));
	if (i == 0)
		return (&(pfiltermem[1]));
	if (i == 1)
		return (&(pfiltermem[2]));

	// return from paintbuffer, where samples are doubled.  
	// even samples are to be replaced with interpolated value.

	return (&(pbuffer[(i-2)*2 + 1]));
}

// pass forward over passed in buffer and cubic interpolate all odd samples
// pbuffer: buffer to filter (in place)
// prevfilter:  filter memory. NOTE: this must match the filtertype ie: filtercubic[] for FILTERTYPE_CUBIC
//				if NULL then perform no filtering. UNDONE: should have a filter memory array type
// count: how many samples to upsample. will become count*2 samples in buffer, in place.

void S_Interpolate2xCubic( portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem, int cfltmem, int count )
{

// implement cubic interpolation on 2x upsampled buffer.   Effectively delays buffer contents by 2 samples.
// pbuffer: contains samples at 0, 2, 4, 6...
// temppaintbuffer is temp buffer, of same or larger size than a paintbuffer, used to store processed values
// count: number of samples to process in buffer ie: how many samples at 0, 2, 4, 6...

// finpos is the fractional, inpos the integer part.
//		finpos = 0.5 for upsampling by 2x
//		inpos is the position of the sample

//		xm1 = x [inpos - 1];
//		x0 = x [inpos + 0];
//		x1 = x [inpos + 1];
//		x2 = x [inpos + 2];
//		a = (3 * (x0-x1) - xm1 + x2) / 2;
//		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
//		c = (x1 - xm1) / 2;
//		y [outpos] = (((a * finpos) + b) * finpos + c) * finpos + x0;

	int i, upCount = count << 1;
	int a, b, c;
	int xm1, x0, x1, x2;
	portable_samplepair_t *psamp0;
	portable_samplepair_t *psamp1;
	portable_samplepair_t *psamp2;
	portable_samplepair_t *psamp3;
	int outpos = 0;

	Assert (upCount <= PAINTBUFFER_SIZE);

	// pfiltermem holds 6 samples from previous buffer pass

	// process 'count' samples

	for ( i = 0; i < count; i++)
	{
		
		// get source sample pointer

		psamp0 = S_GetNextpFilter(i-1, pbuffer, pfiltermem);
		psamp1 = S_GetNextpFilter(i,   pbuffer, pfiltermem);
		psamp2 = S_GetNextpFilter(i+1, pbuffer, pfiltermem);
		psamp3 = S_GetNextpFilter(i+2, pbuffer, pfiltermem);

		// write out original sample to interpolation buffer

		g_temppaintbuffer[outpos++] = *psamp1;

		// get all left samples for interpolation window

		xm1 = psamp0->left;
		x0 = psamp1->left;
		x1 = psamp2->left;
		x2 = psamp3->left;
		
		// interpolate

		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;
		
		// write out interpolated sample

		g_temppaintbuffer[outpos].left = a/8 + b/4 + c/2 + x0;
		
		// get all right samples for window

		xm1 = psamp0->right;
		x0 = psamp1->right;
		x1 = psamp2->right;
		x2 = psamp3->right;
		
		// interpolate

		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;
		
		// write out interpolated sample, increment output counter
		g_temppaintbuffer[outpos++].right = a/8 + b/4 + c/2 + x0;

		Assert( outpos <= TEMP_COPY_BUFFER_SIZE );
	}
	
	Assert(cfltmem >= 3);

	// save last 3 samples from paintbuffer
	
	pfiltermem[0] = pbuffer[upCount - 5];
	pfiltermem[1] = pbuffer[upCount - 3];
	pfiltermem[2] = pbuffer[upCount - 1];

	// copy temppaintbuffer back into paintbuffer

	for (i = 0; i < upCount; i++)
		pbuffer[i] = g_temppaintbuffer[i];
}

// pass forward over passed in buffer and linearly interpolate all odd samples
// pbuffer: buffer to filter (in place)
// prevfilter:  filter memory. NOTE: this must match the filtertype ie: filterlinear[] for FILTERTYPE_LINEAR
//				if NULL then perform no filtering.
// count: how many samples to upsample. will become count*2 samples in buffer, in place.

void S_Interpolate2xLinear( portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem, int cfltmem, int count )
{
	int i, upCount = count<<1;

	Assert (upCount <= PAINTBUFFER_SIZE);
	Assert (cfltmem >= 1);

	// use interpolation value from previous mix

	pbuffer[0].left = (pfiltermem->left + pbuffer[0].left) >> 1;
	pbuffer[0].right = (pfiltermem->right + pbuffer[0].right) >> 1;

	for ( i = 2; i < upCount; i+=2)
	{
		// use linear interpolation for upsampling

		pbuffer[i].left = (pbuffer[i].left + pbuffer[i-1].left) >> 1;
		pbuffer[i].right = (pbuffer[i].right + pbuffer[i-1].right) >> 1;
	}

	// save last value to be played out in buffer

	*pfiltermem = pbuffer[upCount - 1]; 
}


// Optimized routine.  2.27X faster than the above routine
void S_Interpolate2xLinear_2( int count, portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem, int cfltmem )
{
	Assert (cfltmem >= 1);

	int sample = count-1;
	int end = (count*2)-1;
	portable_samplepair_t *pwrite = &pbuffer[end];
	portable_samplepair_t *pread = &pbuffer[sample];
	portable_samplepair_t last = pread[0];
	pread--;

	// PERFORMANCE: Unroll the loop 8 times.  This improves speed quite a bit
	for ( ;sample >= 8; sample -= 8 )
	{
		pwrite[0] = last;
		pwrite[-1].left = (pread[0].left + last.left)>>1;
		pwrite[-1].right = (pread[0].right + last.right)>>1;
		last = pread[0];
		
		pwrite[-2] = last;
		pwrite[-3].left = (pread[-1].left + last.left)>>1;
		pwrite[-3].right = (pread[-1].right + last.right)>>1;
		last = pread[-1];
		
		pwrite[-4] = last;
		pwrite[-5].left = (pread[-2].left + last.left)>>1;
		pwrite[-5].right = (pread[-2].right + last.right)>>1;
		last = pread[-2];
		
		pwrite[-6] = last;
		pwrite[-7].left = (pread[-3].left + last.left)>>1;
		pwrite[-7].right = (pread[-3].right + last.right)>>1;
		last = pread[-3];
		
		pwrite[-8] = last;
		pwrite[-9].left = (pread[-4].left + last.left)>>1;
		pwrite[-9].right = (pread[-4].right + last.right)>>1;
		last = pread[-4];
		
		pwrite[-10] = last;
		pwrite[-11].left = (pread[-5].left + last.left)>>1;
		pwrite[-11].right = (pread[-5].right + last.right)>>1;
		last = pread[-5];
		
		pwrite[-12] = last;
		pwrite[-13].left = (pread[-6].left + last.left)>>1;
		pwrite[-13].right = (pread[-6].right + last.right)>>1;
		last = pread[-6];
		
		pwrite[-14] = last;
		pwrite[-15].left = (pread[-7].left + last.left)>>1;
		pwrite[-15].right = (pread[-7].right + last.right)>>1;
		last = pread[-7];
		
		pread -= 8;
		pwrite -= 16;
	}
	while ( pread >= pbuffer )
	{
		pwrite[0] = last;
		pwrite[-1].left = (pread[0].left + last.left)>>1;
		pwrite[-1].right = (pread[0].right + last.right)>>1;
		last = pread[0];
		pread--;
		pwrite-=2;
	}
	pbuffer[1] = last;
	pbuffer[0].left = (pfiltermem->left + last.left) >> 1;
	pbuffer[0].right = (pfiltermem->right + last.right) >> 1;
	*pfiltermem = pbuffer[end];
}
 

// upsample by 2x, optionally using interpolation
// count: how many samples to upsample. will become count*2 samples in buffer, in place.
// pbuffer: buffer to upsample into (in place)
// pfiltermem:  filter memory. NOTE: this must match the filtertype ie: filterlinear[] for FILTERTYPE_LINEAR
//				if NULL then perform no filtering.
// cfltmem: max number of sample pairs filter can use
// filtertype: FILTERTYPE_NONE, _LINEAR, _CUBIC etc.  Must match prevfilter.
void S_MixBufferUpsample2x( int count, portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem, int cfltmem, int filtertype )
{
	// JAY: Optimized this routine.  Test then remove old routine.  
	// NOTE: Has been proven equivalent by comparing output.
	if ( filtertype == FILTERTYPE_LINEAR )
	{
		S_Interpolate2xLinear_2( count, pbuffer, pfiltermem, cfltmem );
		return;
	}
	int i, j, upCount = count<<1;
	
	// reverse through buffer, duplicating contents for 'count' samples

	for (i = upCount - 1, j = count - 1; j >= 0; i-=2, j--)
	{	
		pbuffer[i] = pbuffer[j];
		pbuffer[i-1] = pbuffer[j];
	}
	
	// pass forward through buffer, interpolate all even slots

	switch (filtertype)
	{
	default:
		break;
	case FILTERTYPE_LINEAR:
		S_Interpolate2xLinear(pbuffer, pfiltermem, cfltmem, count);
		break;
	case FILTERTYPE_CUBIC:
		S_Interpolate2xCubic(pbuffer, pfiltermem, cfltmem, count);
		break;
	}
}

//===============================================================================
// PAINTBUFFER ROUTINES
//===============================================================================


// Set current paintbuffer to pbuf.  
// The set paintbuffer is used by all subsequent mixing, upsampling and dsp routines.
// Also sets the rear paintbuffer if paintbuffer has fsurround true.
// (otherwise, rearpaintbuffer is NULL)

void MIX_SetCurrentPaintbuffer(int ipaintbuffer)
{
	// set front and rear paintbuffer

	Assert(ipaintbuffer < g_paintBuffers.Count());
	
	g_curpaintbuffer = g_paintBuffers[ipaintbuffer].pbuf;

	if ( g_paintBuffers[ipaintbuffer].fsurround )
	{
		g_currearpaintbuffer = g_paintBuffers[ipaintbuffer].pbufrear;
		
		g_curcenterpaintbuffer = NULL;

		if ( g_paintBuffers[ipaintbuffer].fsurround_center )
			g_curcenterpaintbuffer = g_paintBuffers[ipaintbuffer].pbufcenter;
	}
	else
	{
		g_currearpaintbuffer = NULL;
		g_curcenterpaintbuffer = NULL;
	}

	Assert(g_curpaintbuffer != NULL);
}

// return index to current paintbuffer

int MIX_GetCurrentPaintbufferIndex( void )
{
	int i;

	for ( i = 0; i < g_paintBuffers.Count(); i++ )
	{
		if (g_curpaintbuffer == g_paintBuffers[i].pbuf)
			return i;
	}

	return 0;
}


// return pointer to current paintbuffer struct

paintbuffer_t *MIX_GetCurrentPaintbufferPtr( void )
{
	int ipaint = MIX_GetCurrentPaintbufferIndex();
	
	Assert( ipaint < g_paintBuffers.Count() );

	return &g_paintBuffers[ipaint];
}


// return pointer to front paintbuffer pbuf, given index

inline portable_samplepair_t *MIX_GetPFrontFromIPaint(int ipaintbuffer)
{
	return g_paintBuffers[ipaintbuffer].pbuf;
}

paintbuffer_t *MIX_GetPPaintFromIPaint( int ipaintbuffer )
{	
	Assert( ipaintbuffer < g_paintBuffers.Count() );

	return &g_paintBuffers[ipaintbuffer];
}


// return pointer to rear buffer, given index.
// returns null if fsurround is false;

inline portable_samplepair_t *MIX_GetPRearFromIPaint(int ipaintbuffer)
{
	if ( g_paintBuffers[ipaintbuffer].fsurround )
		return g_paintBuffers[ipaintbuffer].pbufrear;

	return NULL;
}

// return pointer to center buffer, given index.
// returns null if fsurround_center is false;

inline portable_samplepair_t *MIX_GetPCenterFromIPaint(int ipaintbuffer)
{
	if ( g_paintBuffers[ipaintbuffer].fsurround_center )
		return g_paintBuffers[ipaintbuffer].pbufcenter;

	return NULL;
}

// return index to paintbuffer, given buffer pointer

inline int MIX_GetIPaintFromPFront( portable_samplepair_t *pbuf )
{
	int i;

	for ( i = 0; i < g_paintBuffers.Count(); i++ )
	{
		if ( pbuf == g_paintBuffers[i].pbuf )
			return i;
	}

	return 0;
}

// return pointer to paintbuffer struct, given ptr to buffer data

inline paintbuffer_t *MIX_GetPPaintFromPFront( portable_samplepair_t *pbuf )
{
	int i;
	i = MIX_GetIPaintFromPFront( pbuf );

	return &g_paintBuffers[i];
}


// up convert mono buffer to full surround

inline void MIX_ConvertBufferToSurround( int ipaintbuffer )
{
	paintbuffer_t *ppaint = &g_paintBuffers[ipaintbuffer];

	// duplicate channel data as needed

	if ( g_AudioDevice->IsSurround() )
	{
		// set buffer flags

		ppaint->fsurround = g_AudioDevice->IsSurround();
		ppaint->fsurround_center = g_AudioDevice->IsSurroundCenter();

		portable_samplepair_t *pfront  = MIX_GetPFrontFromIPaint( ipaintbuffer );
		portable_samplepair_t *prear   = MIX_GetPRearFromIPaint( ipaintbuffer );
		portable_samplepair_t *pcenter = MIX_GetPCenterFromIPaint( ipaintbuffer );
		
		// copy front to rear
		Q_memcpy(prear, pfront, sizeof(portable_samplepair_t) * PAINTBUFFER_SIZE);

		// copy front to center
		if ( g_AudioDevice->IsSurroundCenter() )
			Q_memcpy(pcenter, pfront, sizeof(portable_samplepair_t) * PAINTBUFFER_SIZE);
	}
}

// Activate a paintbuffer.  All active paintbuffers are mixed in parallel within 
// MIX_MixChannelsToPaintbuffer, according to flags

inline void MIX_ActivatePaintbuffer(int ipaintbuffer)
{
	Assert( ipaintbuffer < g_paintBuffers.Count() );
	g_paintBuffers[ipaintbuffer].factive = true;
}

// Don't mix into this paintbuffer

inline void MIX_DeactivatePaintbuffer(int ipaintbuffer)
{
	Assert( ipaintbuffer < g_paintBuffers.Count() );
	g_paintBuffers[ipaintbuffer].factive = false;
}

// Don't mix into any paintbuffers

inline void MIX_DeactivateAllPaintbuffers(void)
{
	int i;
	for ( i = 0; i < g_paintBuffers.Count(); i++ )
		g_paintBuffers[i].factive = false;
}

// set upsampling filter indexes back to 0

inline void MIX_ResetPaintbufferFilterCounters( void )

{
	int i;
	for ( i = 0; i < g_paintBuffers.Count(); i++ )
		g_paintBuffers[i].ifilter = 0;
}

inline void MIX_ResetPaintbufferFilterCounter( int ipaintbuffer )
{
	Assert ( ipaintbuffer < g_paintBuffers.Count() );
	g_paintBuffers[ipaintbuffer].ifilter = 0;
}

// Change paintbuffer's flags

inline void MIX_SetPaintbufferFlags(int ipaintbuffer, int flags)
{
	Assert( ipaintbuffer < g_paintBuffers.Count() );
	g_paintBuffers[ipaintbuffer].flags = flags;
}


// zero out all paintbuffers

void MIX_ClearAllPaintBuffers( int SampleCount, bool clearFilters )
{
	// g_paintBuffers can be NULL with -nosound
	if ( g_paintBuffers.Count() <= 0 )
	{
		return;
	}

	int i;
	int count = min(SampleCount, PAINTBUFFER_SIZE);

	// zero out all paintbuffer data (ignore sampleCount)

	for ( i = 0; i < g_paintBuffers.Count(); i++ )
	{
		if (g_paintBuffers[i].pbuf != NULL)
			Q_memset(g_paintBuffers[i].pbuf, 0, (count+1) * sizeof(portable_samplepair_t));

		if (g_paintBuffers[i].pbufrear != NULL)
			Q_memset(g_paintBuffers[i].pbufrear, 0, (count+1) * sizeof(portable_samplepair_t));

		if (g_paintBuffers[i].pbufcenter != NULL)
			Q_memset(g_paintBuffers[i].pbufcenter, 0, (count+1) * sizeof(portable_samplepair_t));

		if ( clearFilters )
		{
			Q_memset( g_paintBuffers[i].fltmem, 0, sizeof(g_paintBuffers[i].fltmem) );
			Q_memset( g_paintBuffers[i].fltmemrear, 0, sizeof(g_paintBuffers[i].fltmemrear) );
			Q_memset( g_paintBuffers[i].fltmemcenter, 0, sizeof(g_paintBuffers[i].fltmemcenter) );
		}
	}

	if ( clearFilters )
	{
		MIX_ResetPaintbufferFilterCounters();
	}
}

#define SWAP(a,b,t)		{(t) = (a); (a) = (b); (b) = (t);}
#define AVG(a,b)		(((a) + (b)) >> 1 )
#define AVG4(a,b,c,d)	(((a) + (b) + (c) + (d)) >> 2 )

// Synthesize center channel from left/right values (average).
// Currently just averages, but could actually remove
// the center signal from the l/r channels...

inline void MIX_CenterFromLeftRight( int *pl, int *pr, int *pc )
{
	int l = *pl;
	int r = *pr;
	int c = 0;
	

	c = (l + r) / 2;

/*
	l = l - c/2;
	r = r - c/2;
	
	if (l < 0)
	{
		l = 0;
		r += (-l);
		c += (-l);
	}
	else if (r < 0)
	{
		r = 0;
		l += (-r);
		c += (-r);
	}
*/
	*pc = c;
//	*pl = l;
//	*pr = r;
}

// mixes pbuf1 + pbuf2 into pbuf3, count samples
// fgain is output gain 0-1.0
// NOTE: pbuf3 may equal pbuf1 or pbuf2!

// mixing algorithms:

// destination 2ch:
// pb1 2ch		  + pb2 2ch			-> pb3 2ch
// pb1 (4ch->2ch) + pb2 2ch			-> pb3 2ch
// pb1 2ch		  + pb2 (4ch->2ch)	-> pb3 2ch
// pb1 (4ch->2ch) + pb2 (4ch->2ch)	-> pb3 2ch

// destination 4ch:
// pb1 4ch		  + pb2 4ch			-> pb3 4ch
// pb1 (2ch->4ch) + pb2 4ch			-> pb3 4ch
// pb1 4ch		  + pb2 (2ch->4ch)	-> pb3 4ch
// pb1 (2ch->4ch) + pb2 (2ch->4ch)	-> pb3 4ch

// if all buffers are 4 or 5 ch surround, mix rear & center channels into ibuf3 as well.

// NOTE: for performance, conversion and mixing are done in a single pass instead of 
// a two pass channel convert + mix scheme.

void MIX_MixPaintbuffers(int ibuf1, int ibuf2, int ibuf3, int count, float fgain_out)
{
	VPROF("Mixpaintbuffers");
	int i;
	portable_samplepair_t *pbuf1, *pbuf2, *pbuf3, *pbuft;
	portable_samplepair_t *pbufrear1, *pbufrear2, *pbufrear3, *pbufreart;
	portable_samplepair_t *pbufcenter1, *pbufcenter2, *pbufcenter3, *pbufcentert;
	int cchan1, cchan2, cchan3, cchant;
	int xl,xr;
	int l,r,l2,r2,c, c2;
	int gain_out;

	gain_out = 256 * fgain_out;
	
	Assert (count <= PAINTBUFFER_SIZE);
	Assert (ibuf1 < g_paintBuffers.Count());
	Assert (ibuf2 < g_paintBuffers.Count());
	Assert (ibuf3 < g_paintBuffers.Count());

	pbuf1 = g_paintBuffers[ibuf1].pbuf;
	pbuf2 = g_paintBuffers[ibuf2].pbuf;
	pbuf3 = g_paintBuffers[ibuf3].pbuf;
	
	pbufrear1 = g_paintBuffers[ibuf1].pbufrear;
	pbufrear2 = g_paintBuffers[ibuf2].pbufrear;
	pbufrear3 = g_paintBuffers[ibuf3].pbufrear;

	pbufcenter1 = g_paintBuffers[ibuf1].pbufcenter;
	pbufcenter2 = g_paintBuffers[ibuf2].pbufcenter;
	pbufcenter3 = g_paintBuffers[ibuf3].pbufcenter;
	
	cchan1 = 2 + (g_paintBuffers[ibuf1].fsurround ? 2 : 0) + (g_paintBuffers[ibuf1].fsurround_center ? 1 : 0);
	cchan2 = 2 + (g_paintBuffers[ibuf2].fsurround ? 2 : 0) + (g_paintBuffers[ibuf2].fsurround_center ? 1 : 0);
	cchan3 = 2 + (g_paintBuffers[ibuf3].fsurround ? 2 : 0) + (g_paintBuffers[ibuf3].fsurround_center ? 1 : 0);

	// make sure pbuf1 always has fewer or equal channels than pbuf2
	// NOTE: pbuf3 may equal pbuf1 or pbuf2!

	if ( cchan2 < cchan1 )
	{
		SWAP( cchan1, cchan2, cchant );
		SWAP( pbuf1, pbuf2, pbuft );
		SWAP( pbufrear1, pbufrear2, pbufreart );
		SWAP( pbufcenter1, pbufcenter2, pbufcentert);
	}


	// UNDONE: implement fast mixing routines for each of the following sections

	// destination buffer stereo - average n chans down to stereo 

	if ( cchan3 == 2 )
	{
		// destination 2ch:
		// pb1 2ch		  + pb2 2ch			-> pb3 2ch
		// pb1 2ch		  + pb2 (4ch->2ch)	-> pb3 2ch
		// pb1 (4ch->2ch) + pb2 (4ch->2ch)	-> pb3 2ch

		if ( cchan1 == 2 && cchan2 == 2 )
		{
			// mix front channels

			for (i = 0; i < count; i++)
			{
				pbuf3[i].left  = pbuf1[i].left  + pbuf2[i].left;
				pbuf3[i].right = pbuf1[i].right + pbuf2[i].right;
			}
			goto gain2ch;
		}

		if ( cchan1 == 2 && cchan2 == 4 )
		{
			// avg rear chan l/r

			for (i = 0; i < count; i++)
			{
				pbuf3[i].left  = pbuf1[i].left  + AVG( pbuf2[i].left,  pbufrear2[i].left );
				pbuf3[i].right = pbuf1[i].right + AVG( pbuf2[i].right, pbufrear2[i].right );
			}
			goto gain2ch;
		}

		if ( cchan1 == 4 && cchan2 == 4 )
		{
			// avg rear chan l/r

			for (i = 0; i < count; i++)
			{
				pbuf3[i].left  = AVG( pbuf1[i].left, pbufrear1[i].left)  + AVG( pbuf2[i].left,  pbufrear2[i].left );
				pbuf3[i].right = AVG( pbuf1[i].right, pbufrear1[i].right) + AVG( pbuf2[i].right, pbufrear2[i].right );
			}
			goto gain2ch;
		}

		if ( cchan1 == 2 && cchan2 == 5 )
		{
			// avg rear chan l/r + center split into left/right

			for (i = 0; i < count; i++)
			{
				l = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);

				pbuf3[i].left  = pbuf1[i].left  + AVG( l,  pbufrear2[i].left );
				pbuf3[i].right = pbuf1[i].right + AVG( r, pbufrear2[i].right );
			}
			goto gain2ch;
		}

		if ( cchan1 == 4 && cchan2 == 5)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);

				pbuf3[i].left  = AVG( pbuf1[i].left, pbufrear1[i].left)  + AVG( l,  pbufrear2[i].left );
				pbuf3[i].right = AVG( pbuf1[i].right, pbufrear1[i].right) + AVG( r, pbufrear2[i].right );
			}
			goto gain2ch;
		}

		if ( cchan1 == 5 && cchan2 == 5)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left + ((pbufcenter1[i].left) >> 1);
				r = pbuf1[i].right + ((pbufcenter1[i].left) >> 1);
				
				l2 = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r2 = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);
				
				pbuf3[i].left  = AVG( l, pbufrear1[i].left)  + AVG( l2,  pbufrear2[i].left );
				pbuf3[i].right = AVG( r, pbufrear1[i].right) + AVG( r2, pbufrear2[i].right );
			}	goto gain2ch;
		}

	}
	
	// destination buffer quad - duplicate n chans up to quad

	if ( cchan3 == 4 )
	{
		
		// pb1 4ch		  + pb2 4ch			-> pb3 4ch
		// pb1 (2ch->4ch) + pb2 4ch			-> pb3 4ch
		// pb1 (2ch->4ch) + pb2 (2ch->4ch)	-> pb3 4ch
		
		if ( cchan1 == 4 && cchan2 == 4)
		{
			// mix front -> front, rear -> rear

			for (i = 0; i < count; i++)
			{
				pbuf3[i].left  = pbuf1[i].left  + pbuf2[i].left;
				pbuf3[i].right = pbuf1[i].right + pbuf2[i].right;

				pbufrear3[i].left  = pbufrear1[i].left  + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;
			}
			goto gain4ch;
		}

		if ( cchan1 == 2 && cchan2 == 4)
		{
				
			for (i = 0; i < count; i++)
			{
				// split 2 ch left ->  front left, rear left
				// split 2 ch right -> front right, rear right

				xl = pbuf1[i].left;
				xr = pbuf1[i].right;

				pbuf3[i].left  = xl + pbuf2[i].left;
				pbuf3[i].right = xr + pbuf2[i].right;

				pbufrear3[i].left  = xl + pbufrear2[i].left;
				pbufrear3[i].right = xr + pbufrear2[i].right;
			}
			goto gain4ch;
		}

		if ( cchan1 == 2 && cchan2 == 2)
		{
			// mix l,r, split into front l, front r

			for (i = 0; i < count; i++)
			{
				xl = pbuf1[i].left  + pbuf2[i].left;
				xr = pbuf1[i].right + pbuf2[i].right;

				pbufrear3[i].left  = pbuf3[i].left  = xl;
				pbufrear3[i].right = pbuf3[i].right = xr;
			}
			goto gain4ch;
		}

		
		if ( cchan1 == 2 && cchan2 == 5 )
		{
			for (i = 0; i < count; i++)
			{
				// split center of chan2 into left/right

				l2 = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r2 = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);

				xl = pbuf1[i].left;
				xr = pbuf1[i].right;

				pbuf3[i].left  = xl + l2;
				pbuf3[i].right = xr + r2;

				pbufrear3[i].left  = xl + pbufrear2[i].left;
				pbufrear3[i].right = xr + pbufrear2[i].right;
			}
			goto gain4ch;
		}

		if ( cchan1 == 4 && cchan2 == 5)
		{
			
			for (i = 0; i < count; i++)
			{
				l2 = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r2 = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);

				pbuf3[i].left  = pbuf1[i].left  + l2;
				pbuf3[i].right = pbuf1[i].right + r2;

				pbufrear3[i].left  = pbufrear1[i].left  + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;
			}
			goto gain4ch;
		}

		if ( cchan1 == 5 && cchan2 == 5 )
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left + ((pbufcenter1[i].left) >> 1);
				r = pbuf1[i].right + ((pbufcenter1[i].left) >> 1);
				
				l2 = pbuf2[i].left + ((pbufcenter2[i].left) >> 1);
				r2 = pbuf2[i].right + ((pbufcenter2[i].left) >> 1);

				pbuf3[i].left  = l  + l2;
				pbuf3[i].right = r + r2;

				pbufrear3[i].left  = pbufrear1[i].left  + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;
			}
			goto gain4ch;
		}
	}

	// 5 channel destination

	if (cchan3 == 5)
	{
		// up convert from 2 or 4 ch buffer to 5 ch buffer: 
		// center channel is synthesized from front left, front right

		if (cchan1 == 2 && cchan2 == 2)
		{
			for (i = 0; i < count; i++)
			{
				// split 2 ch left ->  front left, center, rear left
				// split 2 ch right -> front right, center, rear right

				l = pbuf1[i].left;
				r = pbuf1[i].right;

				MIX_CenterFromLeftRight(&l, &r, &c);

				l2 = pbuf2[i].left;
				r2 = pbuf2[i].right;

				MIX_CenterFromLeftRight(&l2, &r2, &c2);

				pbuf3[i].left  = l + l2;
				pbuf3[i].right = r + r2;

				pbufrear3[i].left  = pbuf1[i].left + pbuf2[i].left;
				pbufrear3[i].right = pbuf1[i].right + pbuf2[i].right;

				pbufcenter3[i].left = c + c2;
			}
			goto gain5ch;
		}

		if (cchan1 == 2 && cchan2 == 4)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left;
				r = pbuf1[i].right;

				MIX_CenterFromLeftRight(&l, &r, &c);

				l2 = pbuf2[i].left;
				r2 = pbuf2[i].right;

				MIX_CenterFromLeftRight(&l2, &r2, &c2);

				pbuf3[i].left  = l + l2;
				pbuf3[i].right = r + r2;

				pbufrear3[i].left  = pbuf1[i].left + pbufrear2[i].left;
				pbufrear3[i].right = pbuf1[i].right + pbufrear2[i].right;

				pbufcenter3[i].left = c + c2;
			}
			goto gain5ch;
		}

		if (cchan1 == 2 && cchan2 == 5)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left;
				r = pbuf1[i].right;

				MIX_CenterFromLeftRight(&l, &r, &c);

				pbuf3[i].left  = l + pbuf2[i].left;
				pbuf3[i].right = r + pbuf2[i].right;

				pbufrear3[i].left  = pbuf1[i].left + pbufrear2[i].left;
				pbufrear3[i].right = pbuf1[i].right + pbufrear2[i].right;

				pbufcenter3[i].left = c + pbufcenter2[i].left;
			}
			goto gain5ch;
		}

		if (cchan1 == 4 && cchan2 == 4)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left;
				r = pbuf1[i].right;

				MIX_CenterFromLeftRight(&l, &r, &c);

				l2 = pbuf2[i].left;
				r2 = pbuf2[i].right;

				MIX_CenterFromLeftRight(&l2, &r2, &c2);

				pbuf3[i].left  = l + l2;
				pbuf3[i].right = r + r2;

				pbufrear3[i].left  = pbufrear1[i].left + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;

				pbufcenter3[i].left = c + c2;
			}
			goto gain5ch;
		}

	
		if (cchan1 == 4 && cchan2 == 5)
		{
			for (i = 0; i < count; i++)
			{
				l = pbuf1[i].left;
				r = pbuf1[i].right;

				MIX_CenterFromLeftRight(&l, &r, &c);

				pbuf3[i].left  = l + pbuf2[i].left;
				pbuf3[i].right = r + pbuf2[i].right;

				pbufrear3[i].left  = pbufrear1[i].left + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;

				pbufcenter3[i].left = c + pbufcenter2[i].left;
			}
			goto gain5ch;
		}

		if ( cchan2 == 5 && cchan1 == 5 )
		{
			for (i = 0; i < count; i++)
			{
				pbuf3[i].left  = pbuf1[i].left  + pbuf2[i].left;
				pbuf3[i].right = pbuf1[i].right + pbuf2[i].right;
				pbufrear3[i].left  = pbufrear1[i].left  + pbufrear2[i].left;
				pbufrear3[i].right = pbufrear1[i].right + pbufrear2[i].right;
				pbufcenter3[i].left = pbufcenter1[i].left + pbufcenter2[i].left;
			}
			goto gain5ch;
		}
	}

gain2ch:
    if ( gain_out == 256)		// KDB: perf
		return;

	for (i = 0; i < count; i++)
	{
		pbuf3[i].left  = (pbuf3[i].left * gain_out) >> 8;
		pbuf3[i].right = (pbuf3[i].right * gain_out) >> 8;
	}
	return;

gain4ch:
	if ( gain_out == 256)		// KDB: perf
		return;

	for (i = 0; i < count; i++)
	{
		pbuf3[i].left  = (pbuf3[i].left * gain_out) >> 8;
		pbuf3[i].right = (pbuf3[i].right * gain_out) >> 8;
		pbufrear3[i].left  = (pbufrear3[i].left * gain_out) >> 8;
		pbufrear3[i].right = (pbufrear3[i].right * gain_out) >> 8;
	}
	return;

gain5ch:
	if ( gain_out == 256)		// KDB: perf
		return;

	for (i = 0; i < count; i++)
	{
		pbuf3[i].left  = (pbuf3[i].left * gain_out) >> 8;
		pbuf3[i].right = (pbuf3[i].right * gain_out) >> 8;
		pbufrear3[i].left  = (pbufrear3[i].left * gain_out) >> 8;
		pbufrear3[i].right = (pbufrear3[i].right * gain_out) >> 8;
		pbufcenter3[i].left  = (pbufcenter3[i].left * gain_out) >> 8;
	}
	return;
}

// multiply all values in paintbuffer by fgain

void MIX_ScalePaintBuffer( int bufferIndex, int count, float fgain )
{
	portable_samplepair_t *pbuf = g_paintBuffers[bufferIndex].pbuf;
	portable_samplepair_t *pbufrear = g_paintBuffers[bufferIndex].pbufrear;
	portable_samplepair_t *pbufcenter = g_paintBuffers[bufferIndex].pbufcenter;

	int gain = 256 * fgain;
	int i;

	if (gain == 256)
		return;

	if ( !g_paintBuffers[bufferIndex].fsurround )
	{
		for (i = 0; i < count; i++)
		{
			pbuf[i].left  = (pbuf[i].left * gain) >> 8;
			pbuf[i].right = (pbuf[i].right * gain) >> 8;
		}
	}
	else
	{
		for (i = 0; i < count; i++)
		{
			pbuf[i].left  = (pbuf[i].left * gain) >> 8;
			pbuf[i].right = (pbuf[i].right * gain) >> 8;
			pbufrear[i].left  = (pbufrear[i].left * gain) >> 8;
			pbufrear[i].right = (pbufrear[i].right * gain) >> 8;
		}

		if (g_paintBuffers[bufferIndex].fsurround_center)
		{
			for (i = 0; i < count; i++)
			{
				pbufcenter[i].left  = (pbufcenter[i].left * gain) >> 8;
				// pbufcenter[i].right = (pbufcenter[i].right * gain) >> 8; mono center channel
			}
		}
	}
}

// DEBUG peak detection values
#define _SDEBUG 1

#ifdef _SDEBUG
float sdebug_avg_in = 0.0;
float sdebug_in_count = 0.0;
float sdebug_avg_out = 0.0;
float sdebug_out_count = 0.0;
#define SDEBUG_TOTAL_COUNT	(3*44100)
#endif // DEBUG

// DEBUG code - get and show peak value of specified paintbuffer
// DEBUG code - ibuf is buffer index, count is # samples to test, pppeakprev stores peak


void SDEBUG_GetAvgValue( int ibuf, int count, float *pav )
{
#ifdef _SDEBUG
	if (snd_showstart.GetInt() != 4 )
		return;

	float av = 0.0;
	
	for (int i = 0; i < count; i++)
		av += (float)(abs(g_paintBuffers[ibuf].pbuf->left) + abs(g_paintBuffers[ibuf].pbuf->right))/2.0;
	
	*pav = av / count;
#endif // DEBUG
}


void SDEBUG_GetAvgIn( int ibuf, int count)
{
	float av = 0.0;
	SDEBUG_GetAvgValue( ibuf, count, &av );

	sdebug_avg_in = ((av * count ) + (sdebug_avg_in * sdebug_in_count)) / (count + sdebug_in_count);
	sdebug_in_count += count;
}

void SDEBUG_GetAvgOut( int ibuf, int count)
{
	float av = 0.0;
	SDEBUG_GetAvgValue( ibuf, count, &av );

	sdebug_avg_out = ((av * count ) + (sdebug_avg_out * sdebug_out_count)) / (count + sdebug_out_count);
	sdebug_out_count += count;
}


void SDEBUG_ShowAvgValue()
{
#ifdef _SDEBUG
	if (sdebug_in_count > SDEBUG_TOTAL_COUNT)
	{
		if ((int)sdebug_avg_in > 20.0 && (int)sdebug_avg_out > 20.0)
			DevMsg("dsp avg gain:%1.2f in:%1.2f out:%1.2f 1/gain:%1.2f\n", sdebug_avg_out/sdebug_avg_in, sdebug_avg_in, sdebug_avg_out, sdebug_avg_in/sdebug_avg_out);

		sdebug_avg_in = 0.0;
		sdebug_avg_out = 0.0;
		sdebug_in_count = 0.0;
		sdebug_out_count = 0.0;
	}
#endif // DEBUG
}

// clip all values in paintbuffer to 16bit.
// if fsurround is set for paintbuffer, also process rear buffer samples

void MIX_CompressPaintbuffer(int ipaint, int count)
{
	VPROF("CompressPaintbuffer");
	int i;
	paintbuffer_t *ppaint = MIX_GetPPaintFromIPaint(ipaint);
	portable_samplepair_t *pbf;
	portable_samplepair_t *pbr;
	portable_samplepair_t *pbc;

	pbf = ppaint->pbuf;
	pbr = ppaint->pbufrear;
	pbc = ppaint->pbufcenter;

	for (i = 0; i < count; i++)
	{
		pbf->left = CLIP(pbf->left);
		pbf->right = CLIP(pbf->right);
		pbf++;
	}
	
	if ( ppaint->fsurround )
	{
		Assert (pbr);

		for (i = 0; i < count; i++)
		{
			pbr->left = CLIP(pbr->left);
			pbr->right = CLIP(pbr->right);
			pbr++;
		}
	}

	if ( ppaint->fsurround_center )
	{
		Assert (pbc);

		for (i = 0; i < count; i++)
		{
			pbc->left = CLIP(pbc->left);
			//pbc->right = CLIP(pbc->right); mono center channel
			pbc++;
		}
	}
}


// mix and upsample channels to 44khz 'ipaintbuffer'
// mix channels matching 'flags' (SOUND_MIX_DRY, SOUND_MIX_WET, SOUND_MIX_SPEAKER) into specified paintbuffer
// upsamples 11khz, 22khz channels to 44khz.

// NOTE: only call this on channels that will be mixed into only 1 paintbuffer
// and that will not be mixed until the next mix pass! otherwise, MIX_MixChannelsToPaintbuffer
// will advance any internal pointers on mixed channels; subsequent calls will be at 
// incorrect offset.

void MIX_MixUpsampleBuffer( CChannelList &list, int ipaintbuffer, int end, int count, int flags )
{
	VPROF("MixUpsampleBuffer");
	int ipaintcur = MIX_GetCurrentPaintbufferIndex(); // save current paintbuffer

	// reset paintbuffer upsampling filter index
	MIX_ResetPaintbufferFilterCounter( ipaintbuffer );

	// prevent other paintbuffers from being mixed
	MIX_DeactivateAllPaintbuffers();
	
	MIX_ActivatePaintbuffer( ipaintbuffer );			// operates on MIX_MixChannelsToPaintbuffer	
	MIX_SetCurrentPaintbuffer( ipaintbuffer );			// operates on MixUpSample

	// mix 11khz channels to buffer
	if ( list.m_has11kChannels )
	{
		MIX_MixChannelsToPaintbuffer( list, end, flags, SOUND_11k, SOUND_11k );

		// upsample 11khz buffer by 2x
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_11k), FILTERTYPE_LINEAR ); 
	}

	if ( list.m_has22kChannels || list.m_has11kChannels )
	{
		// mix 22khz channels to buffer
		MIX_MixChannelsToPaintbuffer( list, end, flags, SOUND_22k, SOUND_22k );

#if (SOUND_DMA_SPEED > SOUND_22k)
		// upsample 22khz buffer by 2x
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_22k), FILTERTYPE_LINEAR );
#endif
	}

	// mix 44khz channels to buffer
	MIX_MixChannelsToPaintbuffer( list, end, flags, SOUND_44k, SOUND_DMA_SPEED);

	MIX_DeactivateAllPaintbuffers();
	
	// restore previous paintbuffer
	MIX_SetCurrentPaintbuffer( ipaintcur );
}

// upsample and mix sounds into final 44khz versions of the following paintbuffers:
// SOUND_BUFFER_ROOM, SOUND_BUFFER_FACING, IFACINGAWAY, SOUND_BUFFER_DRY, SOUND_BUFFER_SPEAKER, SOUND_BUFFER_SPECIALs
// dsp fx are then applied to these buffers by the caller.
// caller also remixes all into final SOUND_BUFFER_PAINT output.

void MIX_UpsampleAllPaintbuffers( CChannelList &list, int end, int count )
{
	VPROF( "MixUpsampleAll" );

	// 'dry' and 'speaker' channel sounds mix 100% into their corresponding buffers

	// mix and upsample all 'dry' sounds (channels) to 44khz SOUND_BUFFER_DRY paintbuffer

	if ( list.m_hasDryChannels )
		MIX_MixUpsampleBuffer( list, SOUND_BUFFER_DRY, end, count, SOUND_MIX_DRY );

	// mix and upsample all 'speaker' sounds (channels) to 44khz SOUND_BUFFER_SPEAKER paintbuffer
	
	if ( list.m_hasSpeakerChannels )
		MIX_MixUpsampleBuffer( list, SOUND_BUFFER_SPEAKER, end, count, SOUND_MIX_SPEAKER );

	// mix and upsample all 'special dsp' sounds (channels) to 44khz SOUND_BUFFER_SPECIALs paintbuffer

	for ( int iDSP = 0; iDSP < list.m_nSpecialDSPs.Count(); ++iDSP )
	{
		for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
		{
			paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );
			if ( pSpecialBuffer->nSpecialDSP == list.m_nSpecialDSPs[ iDSP ] && pSpecialBuffer->idsp_specialdsp != -1 )
			{
				MIX_MixUpsampleBuffer( list, i, end, count, SOUND_MIX_SPECIAL_DSP );
				break;
			}
		}
	}

	// 'room', 'facing' 'facingaway' sounds are mixed into up to 3 buffers:

	// 11khz sounds are mixed into 3 buffers based on distance from listener, and facing direction
	// These buffers are room, facing, facingaway
	// These 3 mixed buffers are then each upsampled to 22khz.

	// 22khz sounds are mixed into the 3 buffers based on distance from listener, and facing direction
	// These 3 mixed buffers are then each upsampled to 44khz.

	// 44khz sounds are mixed into the 3 buffers based on distance from listener, and facing direction
	MIX_DeactivateAllPaintbuffers();

	// set paintbuffer upsample filter indices to 0
	MIX_ResetPaintbufferFilterCounters();

	if ( !g_bDspOff )		
	{
		// only mix to roombuffer if dsp fx are on KDB: perf
		MIX_ActivatePaintbuffer(SOUND_BUFFER_ROOM);					// operates on MIX_MixChannelsToPaintbuffer
	}

	MIX_ActivatePaintbuffer(SOUND_BUFFER_FACING);					

	if ( g_bdirectionalfx )
	{
		// mix to facing away buffer only if directional presets are set

		MIX_ActivatePaintbuffer(SOUND_BUFFER_FACINGAWAY);		
	}
	
	// mix 11khz sounds: 
	// pan sounds between 3 busses: facing, facingaway and room buffers
	
	MIX_MixChannelsToPaintbuffer( list, end, SOUND_MIX_WET, SOUND_11k, SOUND_11k);

	// upsample all 11khz buffers by 2x
	if ( !g_bDspOff )
	{
		// only upsample roombuffer if dsp fx are on KDB: perf
		MIX_SetCurrentPaintbuffer(SOUND_BUFFER_ROOM);			// operates on MixUpSample
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_11k), FILTERTYPE_LINEAR ); 
	}

	MIX_SetCurrentPaintbuffer(SOUND_BUFFER_FACING);			
	g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_11k), FILTERTYPE_LINEAR ); 

	if ( g_bdirectionalfx )
	{
		MIX_SetCurrentPaintbuffer(SOUND_BUFFER_FACINGAWAY);	
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_11k), FILTERTYPE_LINEAR ); 
	}

	// mix 22khz sounds: 
	// pan sounds between 3 busses: facing, facingaway and room buffers
	MIX_MixChannelsToPaintbuffer( list, end, SOUND_MIX_WET, SOUND_22k, SOUND_22k);

	// upsample all 22khz buffers by 2x
#if ( SOUND_DMA_SPEED > SOUND_22k )
	if ( !g_bDspOff )
	{
		// only upsample roombuffer if dsp fx are on KDB: perf

		MIX_SetCurrentPaintbuffer(SOUND_BUFFER_ROOM);
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_22k), FILTERTYPE_LINEAR );
	}

	MIX_SetCurrentPaintbuffer(SOUND_BUFFER_FACING);
	g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_22k), FILTERTYPE_LINEAR );

	if ( g_bdirectionalfx )
	{
		MIX_SetCurrentPaintbuffer(SOUND_BUFFER_FACINGAWAY);
		g_AudioDevice->MixUpsample( count / (SOUND_DMA_SPEED / SOUND_22k), FILTERTYPE_LINEAR );
	}
#endif

	// mix all 44khz sounds to all active paintbuffers
	MIX_MixChannelsToPaintbuffer( list, end, SOUND_MIX_WET, SOUND_44k, SOUND_DMA_SPEED);

	MIX_DeactivateAllPaintbuffers();

	MIX_SetCurrentPaintbuffer(SOUND_BUFFER_PAINT);
}

ConVar snd_cull_duplicates("snd_cull_duplicates","0",FCVAR_ALLOWED_IN_COMPETITIVE,"If nonzero, aggressively cull duplicate sounds during mixing. The number specifies the number of duplicates allowed to be played.");


// Helper class for determining whether a given channel number should be culled from
// mixing, if snd_cull_duplicates is enabled (psychoacoustic quashing).
class CChannelCullList
{
public:
	// default constructor
	CChannelCullList() : m_numChans(0) {}; 

	// call if you plan on culling channels - and not otherwise, it's a little expensive
	// (that's why it's not in the constructor)
	void Initialize( CChannelList &list );

	// returns true if a given channel number has been marked for culling
	inline bool ShouldCull( int channelNum )
	{
		return (m_numChans > channelNum) ? m_bShouldCull[channelNum] : false;
	}

	// an array of sound names and their volumes
	// TODO: there may be a way to do this faster on 360 (eg, pad to 128bit, use SIMD)
	struct sChannelVolData
	{
		int m_channelNum;
		int m_vol; // max volume of sound. -1 means "do not cull, ever, do not even do the math"
		unsigned int m_nameHash; // a unique id for a sound file
	};
protected:
	sChannelVolData m_channelInfo[MAX_CHANNELS];

	bool m_bShouldCull[MAX_CHANNELS]; // in ChannelList order, not sorted order
	int m_numChans;
};

// comparator for qsort as used below (eg a lambda)
// returns < 0 if a should come before b, > 0 if a should come after, 0 otherwise
static int __cdecl ChannelVolComparator ( const void * a, const void * b ) 
{
	// greater numbers come first.
	return static_cast<const CChannelCullList::sChannelVolData *>(b)->m_vol - static_cast<const CChannelCullList::sChannelVolData *>(a)->m_vol;
}


void CChannelCullList::Initialize( CChannelList &list )
{
	VPROF("CChannelCullList::Initialize");
	// First, build a sorted list of channels by decreasing volume, and by a hash of their wavname.
	m_numChans = list.Count();

	for ( int i = m_numChans - 1 ; i >= 0 ; --i )
	{
		channel_t *ch = list.GetChannel(i);
		m_channelInfo[i].m_channelNum = i; 
		if ( ch && ch->pMixer->IsReadyToMix() )
		{
			m_channelInfo[i].m_vol = ChannelLoudestCurVolume(ch);
			AssertMsg(m_channelInfo[i].m_vol >= 0, "Sound channel has a negative volume?");
			m_channelInfo[i].m_nameHash = (unsigned int) ch->sfx;
		}
		else
		{
			m_channelInfo[i].m_vol = -1;
			m_channelInfo[i].m_nameHash = NULL; // doesn't matter
		}
	}

	// set the unused channels to invalid data
	for ( int i = m_numChans ; i < MAX_CHANNELS ; ++i )
	{
		m_channelInfo[i].m_channelNum = -1;
		m_channelInfo[i].m_vol = -1;
	}

	// Sort the list.
	qsort( m_channelInfo, MAX_CHANNELS, sizeof(sChannelVolData), ChannelVolComparator );

	// Then, determine if the given sound is less than the nth loudest of its hash. If so, mark its flag
	// for removal.
	// TODO: use an actual algorithm rather than this bogus quadratic technique.
	// (I'm using it for now because we don't have convenient/fast hash table 
	// classes, which would be the linear-time way to deal with this).
	const int cutoff = snd_cull_duplicates.GetInt();
	for ( int i = 0 ; i < m_numChans ; ++i ) // i is index in original channel list
	{
		channel_t *ch = list.GetChannel(i);
		// for each sound, determine where it ranks in loudness
		int howManyLouder = 0;
		for ( int j = 0 ;
			 m_channelInfo[j].m_channelNum != i && m_channelInfo[j].m_vol >= 0 && j < MAX_CHANNELS ;
			 ++j )
		{
			// j steps through the sorted list until we find ourselves:
			if (m_channelInfo[j].m_nameHash == (unsigned int)(ch->sfx))
			{
				// that's another channel playing this sound but louder than me
				++howManyLouder;
			}
		}
		if (howManyLouder >= cutoff)
		{
			// this sound should be culled
			m_bShouldCull[i] = true;
		}
		else
		{	
			// this sound should not be culled
			m_bShouldCull[i] = false;
		}
	}
}

ConVar snd_mute_losefocus("snd_mute_losefocus", "1", FCVAR_ARCHIVE);

// build a list of channels that will actually do mixing in this update
// remove all active channels that won't mix for some reason
void MIX_BuildChannelList( CChannelList &list )
{
	VPROF("MIX_BuildChannelList");
	g_ActiveChannels.GetActiveChannels( list );
	list.m_nSpecialDSPs.RemoveAll();
	list.m_hasDryChannels = false;
	list.m_hasSpeakerChannels = false;
	list.m_has11kChannels = false;
	list.m_has22kChannels = false;
	list.m_has44kChannels = false;
	bool delayStartServer = false;
	bool delayStartClient = false;
	bool bPaused = g_pSoundServices->IsGamePaused();
#ifdef POSIX
	bool bActive = g_pSoundServices->IsGameActive();
	bool bStopOnFocusLoss = !bActive && snd_mute_losefocus.GetBool();
#endif

	CChannelCullList cullList;
	if (snd_cull_duplicates.GetInt() > 0)
	{
		cullList.Initialize(list);
	}

	// int numQuashed = 0;
	for ( int i = list.Count(); --i >= 0; )
	{
		channel_t *ch = list.GetChannel(i);
		bool bRemove = false;
		// Certain async loaded sounds lazily load into memory in the background, use this to determine
		//  if the sound is ready for mixing
		CAudioSource *pSource = NULL;
		if ( ch->pMixer->IsReadyToMix() )
		{
			pSource = S_LoadSound( ch->sfx, ch );

			// Don't mix sound data for sounds with 'zero' volume. If it's a non-looping sound, 
			// just remove the sound when its volume goes to zero. If it's a 'dry' channel sound (ie: music)
			// then assume bZeroVolume is fade in - don't restart

			// To be 'zero' volume, all target volume and current volume values must all be less than 5

			bool bZeroVolume = BChannelLowVolume( ch, 1 );

			if ( !pSource || ( bZeroVolume && !pSource->IsLooped() && !ch->flags.bdry ) )
			{
				// NOTE: Since we've loaded the sound, check to see if it's a sentence.  Play them at zero anyway
				// to keep the character's lips moving and the captions happening.
				if ( !pSource || pSource->GetSentence() == NULL )
				{
					S_FreeChannel( ch );
					bRemove = true;
				}
			}
			else if ( bZeroVolume )
			{
				bRemove = true;
			}
			// If the sound wants to stop when the game pauses, do so
			if ( bPaused && SND_ShouldPause(ch) )
			{
				bRemove = true;
			}
#ifdef POSIX
			// If we aren't the active app and the option for background audio isn't on, mute the audio
			// Windows has it's own system for background muting
			if ( !bRemove && bStopOnFocusLoss )
			{
				bRemove = true;

				// Free up the sound channels otherwise they start filling up
				if ( pSource && ( !pSource->IsLooped() && !pSource->IsStreaming() ) )
				{
					S_FreeChannel( ch );
				}

			}
#endif
			// On lowend, aggressively cull duplicate sounds.
			if ( !bRemove && snd_cull_duplicates.GetInt() > 0 )
			{
				// We can't simply remove them, because then sounds will pile up waiting to finish later.
				// We need to flag them for not mixing.
				list.m_quashed[i] = cullList.ShouldCull(i);
				/*
				if (list.m_quashed[i])
				{
					numQuashed++;
					// Msg("removed %i\n", i);
				}
				*/
			}
			else
			{
				list.m_quashed[i] = false;
			}
		}
		else
		{
			bRemove = true;
		}

		if ( bRemove )
		{
			list.RemoveChannelFromList(i);
			continue;
		}
		if ( ch->flags.bSpeaker )
		{
			list.m_hasSpeakerChannels = true;
		}
		if ( ch->special_dsp != 0 )
		{
			if ( list.m_nSpecialDSPs.Find( ch->special_dsp ) == -1 )
			{
				list.m_nSpecialDSPs.AddToTail( ch->special_dsp );
			}
		}
		if ( ch->flags.bdry )
		{
			list.m_hasDryChannels = true;
		}
		int rate = pSource->SampleRate();
		if ( rate == SOUND_11k )
		{
			list.m_has11kChannels = true;
		}
		else if ( rate == SOUND_22k )
		{
			list.m_has22kChannels = true;
		}
		else if ( rate == SOUND_44k )
		{
			list.m_has44kChannels = true;
		}
		if ( ch->flags.delayed_start && !SND_IsMouth(ch) )
		{
			if ( ch->flags.fromserver )
			{
				delayStartServer = true;
			}
			else
			{
				delayStartClient = true;
			}
		}

		// get playback pitch
		ch->pitch = ch->pMixer->ModifyPitch( ch->basePitch * 0.01f );
	}
	// DevMsg( "%d channels quashed.\n", numQuashed );

	// This code will resync the delay calculation clock really often
	// any time there are no scheduled waves or the game is paused
	// we go ahead and reset the clock
	// That way the clock is only used for short periods of time
	// and we need no solution for drift
	if ( bPaused || (host_frametime_unbounded > host_frametime) )
	{
		delayStartClient = false;
		delayStartServer = false;
	}
	if (!delayStartServer)
	{
		S_SyncClockAdjust(CLOCK_SYNC_SERVER);
	}
	if (!delayStartClient)
	{
		S_SyncClockAdjust(CLOCK_SYNC_CLIENT);
	}
}

// main mixing rountine - mix up to 'endtime' samples.
// All channels are mixed in a paintbuffer and then sent to 
// hardware.

// A mix pass is performed, resulting in mixed sounds in SOUND_BUFFER_ROOM, SOUND_BUFFER_FACING, SOUND_BUFFER_FACINGAWAY, SOUND_BUFFER_DRY, SOUND_BUFFER_SPEAKER, SOUND_BUFFER_SPECIALs
                                  
	// directional sounds are panned and mixed between SOUND_BUFFER_FACING and SOUND_BUFFER_FACINGAWAY
	// omnidirectional sounds are panned 100% into SOUND_BUFFER_FACING
	// sound sources far from player (ie: near back of room ) are mixed in proportion to this distance
	// into SOUND_BUFFER_ROOM
	// sounds with ch->bSpeaker set are mixed in mono into SOUND_BUFFER_SPEAKER
	// sounds with ch->bSpecialDSP set are mixed in mono into SOUND_BUFFER_SPECIALs

// dsp_facingaway fx (2 or 4ch filtering) are then applied to the SOUND_BUFFER_FACINGAWAY
// dsp_speaker fx (1ch) are then applied to the SOUND_BUFFER_SPEAKER
// dsp_specialdsp fx (1ch) are then applied to the SOUND_BUFFER_SPECIALs
// dsp_room fx (1ch reverb) are then applied to the SOUND_BUFFER_ROOM

// All buffers are recombined into the SOUND_BUFFER_PAINT

// The dsp_water and dsp_player fx are applied in series to the SOUND_BUFFER_PAINT

// Finally, the SOUND_BUFFER_DRY buffer is mixed into the SOUND_BUFFER_PAINT

extern ConVar dsp_off;
extern ConVar snd_profile;
extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );
extern ConVar dsp_enhance_stereo;

extern ConVar dsp_volume;
extern ConVar dsp_vol_5ch;
extern ConVar dsp_vol_4ch;
extern ConVar dsp_vol_2ch;

extern void MXR_SetCurrentSoundMixer( const char *szsoundmixer );
extern ConVar snd_soundmixer;

void MIX_PaintChannels( int endtime, bool bIsUnderwater )
{
	VPROF("MIX_PaintChannels");
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	int 	end;
	int		count;
	bool	b_spatial_delays = dsp_enhance_stereo.GetInt() != 0 ? true : false;
	bool room_fsurround_sav;
	bool room_fsurround_center_sav;
	paintbuffer_t	*proom = MIX_GetPPaintFromIPaint(SOUND_BUFFER_ROOM);

	CheckNewDspPresets();

	MXR_SetCurrentSoundMixer( snd_soundmixer.GetString() );

	// dsp performance tuning

	g_snd_profile_type = snd_profile.GetInt();

	// dsp_off is true if no dsp processing is to run
	// directional dsp processing is enabled if dsp_facingaway is non-zero

	g_bDspOff = dsp_off.GetInt() ? 1 : 0;
	CChannelList list;

	MIX_BuildChannelList(list);

	// get master dsp volume 
	g_dsp_volume = dsp_volume.GetFloat();

	// attenuate master dsp volume by 2,4 or 5 ch settings
	if ( g_AudioDevice->IsSurround() )
	{	
		g_dsp_volume *= ( g_AudioDevice->IsSurroundCenter() ? dsp_vol_5ch.GetFloat() : dsp_vol_4ch.GetFloat() );
	}
	else
	{
		g_dsp_volume *= dsp_vol_2ch.GetFloat();
	}

	if ( !g_bDspOff )
	{
		g_bdirectionalfx = dsp_facingaway.GetInt() ? 1 : 0;
	}
	else
	{
		g_bdirectionalfx = 0;
	}
	
	// get dsp preset gain values, update gain crossfaders, used when mixing dsp processed buffers into paintbuffer
	SDEBUG_ShowAvgValue();
	
	// the cache needs to hold the audio in memory during mixing, so tell it that mixing is starting
	wavedatacache->OnMixBegin();

	while ( g_paintedtime < endtime )
	{
		VPROF("MIX_PaintChannels inner loop");
		// mix a full 'paintbuffer' of sound
		
		// clamp at paintbuffer size
		end = endtime;
		if (endtime - g_paintedtime > PAINTBUFFER_SIZE)
		{
			end = g_paintedtime + PAINTBUFFER_SIZE;
		}

		// number of 44khz samples to mix into paintbuffer, up to paintbuffer size
		count = end - g_paintedtime;

		// clear all mix buffers
		g_AudioDevice->MixBegin( count );
		
		// upsample all mix buffers.
		// results in 44khz versions of:
		// SOUND_BUFFER_ROOM, SOUND_BUFFER_FACING, SOUND_BUFFER_FACINGAWAY, SOUND_BUFFER_DRY, SOUND_BUFFER_SPEAKER, SOUND_BUFFER_SPECIALs
		MIX_UpsampleAllPaintbuffers( list, end, count );

		// apply appropriate dsp fx to each buffer, remix buffers into single quad output buffer
		// apply 2 or 4ch filtering to IFACINGAWAY buffer
		if ( g_bdirectionalfx )
		{
			g_AudioDevice->ApplyDSPEffects( idsp_facingaway, MIX_GetPFrontFromIPaint(SOUND_BUFFER_FACINGAWAY), MIX_GetPRearFromIPaint(SOUND_BUFFER_FACINGAWAY), MIX_GetPCenterFromIPaint(SOUND_BUFFER_FACINGAWAY), count );
		}

		if ( !g_bDspOff && list.m_hasSpeakerChannels )
		{
			// apply 1ch filtering to SOUND_BUFFER_SPEAKER
			g_AudioDevice->ApplyDSPEffects( idsp_speaker, MIX_GetPFrontFromIPaint(SOUND_BUFFER_SPEAKER), MIX_GetPRearFromIPaint(SOUND_BUFFER_SPEAKER), MIX_GetPCenterFromIPaint(SOUND_BUFFER_SPEAKER), count );
			
			// mix SOUND_BUFFER_SPEAKER with SOUND_BUFFER_ROOM and SOUND_BUFFER_FACING
			MIX_ScalePaintBuffer( SOUND_BUFFER_SPEAKER, count, 0.7 );

			MIX_MixPaintbuffers( SOUND_BUFFER_SPEAKER, SOUND_BUFFER_FACING, SOUND_BUFFER_FACING, count, 1.0 );	// +70% dry speaker

			MIX_ScalePaintBuffer( SOUND_BUFFER_SPEAKER, count, 0.43 );

			MIX_MixPaintbuffers( SOUND_BUFFER_SPEAKER, SOUND_BUFFER_ROOM, SOUND_BUFFER_ROOM, count, 1.0 );		// +30% wet speaker
		}

		if ( !g_bDspOff )
		{
			// apply 1ch filtering to SOUND_BUFFER_SPECIALs
			for ( int iDSP = 0; iDSP < list.m_nSpecialDSPs.Count(); ++iDSP )
			{
				bool bFoundMixer = false;
				
				for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
				{
					paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );
					if ( pSpecialBuffer->nSpecialDSP == list.m_nSpecialDSPs[ iDSP ] && pSpecialBuffer->idsp_specialdsp != -1 )
					{
						g_AudioDevice->ApplyDSPEffects( pSpecialBuffer->idsp_specialdsp, MIX_GetPFrontFromIPaint( i ), MIX_GetPRearFromIPaint( i ), MIX_GetPCenterFromIPaint( i ), count );

						// mix SOUND_BUFFER_SPECIALs with SOUND_BUFFER_ROOM and SOUND_BUFFER_FACING
						MIX_ScalePaintBuffer( i, count, 0.7 );

						MIX_MixPaintbuffers( i, SOUND_BUFFER_FACING, SOUND_BUFFER_FACING, count, 1.0 );	// +70% dry speaker

						MIX_ScalePaintBuffer( i, count, 0.43 );

						MIX_MixPaintbuffers( i, SOUND_BUFFER_ROOM, SOUND_BUFFER_ROOM, count, 1.0 );		// +30% wet speaker
						
						bFoundMixer = true;

						break;
					}
				}

				// Couldn't find a mixer with the correct DSP, so make a new one!
				if ( !bFoundMixer )
				{
					bool bSurroundCenter = g_AudioDevice->IsSurroundCenter();
					bool bSurround = g_AudioDevice->IsSurround() || bSurroundCenter;

					int nIndex = g_paintBuffers.AddToTail();
					MIX_InitializePaintbuffer( &(g_paintBuffers[ nIndex ]), bSurround, bSurroundCenter );

					g_paintBuffers[ nIndex ].flags = SOUND_BUSS_SPECIAL_DSP;

					// special dsp buffer mixes to mono
					g_paintBuffers[ nIndex ].fsurround = false;
					g_paintBuffers[ nIndex ].fsurround_center = false;

					g_paintBuffers[ nIndex ].idsp_specialdsp = -1;
					g_paintBuffers[ nIndex ].nSpecialDSP = list.m_nSpecialDSPs[ iDSP ];

					g_paintBuffers[ nIndex ].nPrevSpecialDSP = g_paintBuffers[ nIndex ].nSpecialDSP;
					g_paintBuffers[ nIndex ].idsp_specialdsp = DSP_Alloc( g_paintBuffers[ nIndex ].nSpecialDSP, 300, 1 );
				}
			}
		}

		// apply dsp_room effects to room buffer
		g_AudioDevice->ApplyDSPEffects( Get_idsp_room(), MIX_GetPFrontFromIPaint(SOUND_BUFFER_ROOM), MIX_GetPRearFromIPaint(SOUND_BUFFER_ROOM), MIX_GetPCenterFromIPaint(SOUND_BUFFER_ROOM), count );
		
		// save room buffer surround status, in case we upconvert it
		room_fsurround_sav = proom->fsurround;
		room_fsurround_center_sav = proom->fsurround_center;
		
		// apply left/center/right/lrear/rrear spatial delays to room buffer
		if ( b_spatial_delays && !g_bDspOff && !DSP_RoomDSPIsOff() )
		{
			// upgrade mono room buffer to surround status so we can apply spatial delays to all channels
			MIX_ConvertBufferToSurround( SOUND_BUFFER_ROOM );
			g_AudioDevice->ApplyDSPEffects( idsp_spatial, MIX_GetPFrontFromIPaint(SOUND_BUFFER_ROOM),  MIX_GetPRearFromIPaint(SOUND_BUFFER_ROOM), MIX_GetPCenterFromIPaint(SOUND_BUFFER_ROOM), count );
		}

		if ( g_bdirectionalfx )		// KDB: perf
		{
			// Recombine IFACING and IFACINGAWAY buffers into SOUND_BUFFER_PAINT
			MIX_MixPaintbuffers( SOUND_BUFFER_FACING, SOUND_BUFFER_FACINGAWAY, SOUND_BUFFER_PAINT, count, DSP_NOROOM_MIX );
			
			// Add in dsp room fx to paintbuffer, mix at 75%
			MIX_MixPaintbuffers( SOUND_BUFFER_ROOM, SOUND_BUFFER_PAINT, SOUND_BUFFER_PAINT, count, DSP_ROOM_MIX );
		} 
		else
		{
			// Mix IFACING buffer with SOUND_BUFFER_ROOM
			// (SOUND_BUFFER_FACINGAWAY contains no data, IFACINGBBUFFER has full dry mix based on distance from listener)
			// if dsp disabled, mix 100% facingbuffer, otherwise, mix 75% facingbuffer + roombuffer
			float mix = g_bDspOff ? 1.0 : DSP_ROOM_MIX;
			MIX_MixPaintbuffers( SOUND_BUFFER_ROOM, SOUND_BUFFER_FACING, SOUND_BUFFER_PAINT, count, mix );	
		}

		// restore room buffer surround status, in case we upconverted it 
		proom->fsurround = room_fsurround_sav;
		proom->fsurround_center = room_fsurround_center_sav;
		
		// Apply underwater fx dsp_water (serial in-line)
		if ( bIsUnderwater )
		{
			// BUG: if out of water, previous delays will be heard. must clear dly buffers.
			g_AudioDevice->ApplyDSPEffects( idsp_water, MIX_GetPFrontFromIPaint(SOUND_BUFFER_PAINT), MIX_GetPRearFromIPaint(SOUND_BUFFER_PAINT), MIX_GetPCenterFromIPaint(SOUND_BUFFER_PAINT), count );
		}

		// find dsp gain
		SDEBUG_GetAvgIn(SOUND_BUFFER_PAINT, count);

		// Apply player fx dsp_player (serial in-line) - does nothing if dsp fx are disabled
		g_AudioDevice->ApplyDSPEffects( idsp_player, MIX_GetPFrontFromIPaint(SOUND_BUFFER_PAINT),  MIX_GetPRearFromIPaint(SOUND_BUFFER_PAINT), MIX_GetPCenterFromIPaint(SOUND_BUFFER_PAINT), count );

		// display dsp gain
		SDEBUG_GetAvgOut(SOUND_BUFFER_PAINT, count);

/*
		// apply left/center/right/lrear/rrear spatial delays to paint buffer

		if ( b_spatial_delays )
			g_AudioDevice->ApplyDSPEffects( idsp_spatial, MIX_GetPFrontFromIPaint(SOUND_BUFFER_PAINT),  MIX_GetPRearFromIPaint(SOUND_BUFFER_PAINT), MIX_GetPCenterFromIPaint(SOUND_BUFFER_PAINT), count );
*/
		// Add dry buffer, set output gain to water * player dsp gain (both 1.0 if not active)

		MIX_MixPaintbuffers( SOUND_BUFFER_PAINT, SOUND_BUFFER_DRY, SOUND_BUFFER_PAINT, count, 1.0);

		// clip all values > 16 bit down to 16 bit
		// NOTE: This is required - the hardware buffer transfer routines no longer perform clipping.
		MIX_CompressPaintbuffer( SOUND_BUFFER_PAINT, count );

		// transfer SOUND_BUFFER_PAINT paintbuffer out to DMA buffer
		MIX_SetCurrentPaintbuffer( SOUND_BUFFER_PAINT );

		g_AudioDevice->TransferSamples( end );

		g_paintedtime = end;
	}

	// the cache needs to hold the audio in memory during mixing, so tell it that mixing is complete
	wavedatacache->OnMixEnd();
}

// Applies volume scaling (evenly) to all fl,fr,rl,rr volumes
// used for voice ducking and panning between various mix busses
// Ensures if mixing to speaker buffer, only speaker sounds pass through

// Called just before mixing wav data to current paintbuffer.
// a) if another player in a multiplayer game is speaking, scale all volumes down.
// b) if mixing to SOUND_BUFFER_ROOM, scale all volumes by ch.dspmix and dsp_room gain
// c) if mixing to SOUND_BUFFER_FACINGAWAY, scale all volumes by ch.dspface and dsp_facingaway gain
// d) If SURROUND_ON, but buffer is not surround, recombined front/rear volumes

// returns false if channel is to be entirely skipped. 

bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans )
{
	int i;
	int	mixflag = ppaint->flags;
	float scale;
	char wavtype = pChannel->wavtype;
	float dspmix;

	// copy current channel volumes into output array

	ChannelCopyVolumes( pChannel, volume, 0, CCHANVOLUMES );

	dspmix = pChannel->dspmix;
	
	// if dsp is off, or room dsp is off, mix 0% to mono room buffer, 100% to facing buffer

	if ( g_bDspOff || DSP_RoomDSPIsOff() )
		dspmix = 0.0;
	
	// duck all sound volumes except speaker's voice
#if !defined( NO_VOICE )
	int duckScale = min((int)(g_DuckScale * 256), g_SND_VoiceOverdriveInt);
#else
	int duckScale = (int)(g_DuckScale * 256);
#endif
	if( duckScale < 256 )
	{
		if( pChannel->pMixer )
		{
			CAudioSource *pSource = pChannel->pMixer->GetSource();
			if( !pSource->IsVoiceSource() )
			{
				// Apply voice overdrive..
				for (i = 0; i < CCHANVOLUMES; i++)
					volume[i] = (volume[i] * duckScale) >> 8;
			}
		}
	}

	// If mixing to the room buss, adjust volume based on channel's dspmix setting.
	// dspmix is DSP_MIX_MAX (~0.78) if sound is far from player, DSP_MIX_MIN (~0.24) if sound is near player
	
	if ( mixflag & SOUND_BUSS_ROOM )
	{
		// set dsp mix volume, scaled by global dsp_volume

		float dspmixvol = fpmin(dspmix * g_dsp_volume, 1.0f);

		// if dspmix is 1.0, 100% of sound goes to SOUND_BUFFER_ROOM and 0% to SOUND_BUFFER_FACING

		for (i = 0; i < CCHANVOLUMES; i++)
			volume[i] = (int)((float)(volume[i]) * dspmixvol);
	}

	// If global dsp volume is less than 1, reduce dspmix (ie: increase dry volume)
	// If gloabl dsp volume is greater than 1,  do not reduce dspmix
	
	if (g_dsp_volume < 1.0)
		dspmix *= g_dsp_volume;

	// If mixing to facing/facingaway buss, adjust volume based on sound entity's facing direction.

	// If sound directly faces player, ch->dspface = 1.0.  If facing directly away, ch->dspface = -1.0.
	// mix to lowpass buffer if facing away, to allpass if facing
	
	// scale 1.0 - facing player, scale 0, facing away

	scale = (pChannel->dspface + 1.0) / 2.0;

	// UNDONE: get front cone % from channel to set this.

	// bias scale such that 1.0 to 'cone' is considered facing.  Facing cone narrows as cone -> 1.0
	// and 'cone' -> 0.0 becomes 1.0 -> 0.0

	float cone = 0.6f;

	scale = scale * (1/cone);

	scale = clamp( scale, 0.0f, 1.0f );

	// pan between facing and facing away buffers

	// if ( !g_bdirectionalfx || wavtype == CHAR_DOPPLER || wavtype == CHAR_OMNI || (wavtype == CHAR_DIRECTIONAL && mixchans == 2) )
	if ( !g_bdirectionalfx || wavtype != CHAR_DIRECTIONAL )
	{
		// if no directional fx mix 0% to facingaway buffer
		// if wavtype is DOPPLER, mix 0% to facingaway buffer - DOPPLER wavs have a custom mixer
		// if wavtype is OMNI, mix 0% to facingaway buffer - OMNI wavs have no directionality
		// if wavtype is DIRECTIONAL and stereo encoded, mix 0% to facingaway buffer - DIRECTIONAL STEREO wavs have a custom mixer
		
		scale = 1.0;
	}

	if ( mixflag & SOUND_BUSS_FACING )
	{
		// facing player
		// if dspface is 1.0, 100% of sound goes to SOUND_BUFFER_FACING

		for (i = 0; i < CCHANVOLUMES; i++)
			volume[i] = (int)((float)(volume[i]) * scale * (1.0 - dspmix));
	}
	else if ( mixflag & SOUND_BUSS_FACINGAWAY )
	{
		// facing away from player
		// if dspface is 0.0, 100% of sound goes to SOUND_BUFFER_FACINGAWAY

		for (i = 0; i < CCHANVOLUMES; i++)
			volume[i] = (int)((float)(volume[i]) * (1.0 - scale) * (1.0 - dspmix));
	}

	// NOTE: this must occur last in this routine: 

	if ( g_AudioDevice->IsSurround() && !ppaint->fsurround )
	{
		// if 4ch or 5ch spatialization on, but current mix buffer is 2ch, 
		// recombine front + rear volumes (revert to 2ch spatialization)

		volume[IFRONT_RIGHT] += volume[IREAR_RIGHT];
		volume[IFRONT_LEFT]  += volume[IREAR_LEFT];

		volume[IFRONT_RIGHTD] += volume[IREAR_RIGHTD];
		volume[IFRONT_LEFTD]  += volume[IREAR_LEFTD];

		// if 5 ch, recombine center channel vol

		if ( g_AudioDevice->IsSurroundCenter() )
		{
			volume[IFRONT_RIGHT] += volume[IFRONT_CENTER] / 2;
			volume[IFRONT_LEFT]  += volume[IFRONT_CENTER] / 2;

			volume[IFRONT_RIGHTD] += volume[IFRONT_CENTERD] / 2;
			volume[IFRONT_LEFTD]  += volume[IFRONT_CENTERD] / 2;
		}

		// clear rear & center volumes

		volume[IREAR_RIGHT] = 0;
		volume[IREAR_LEFT] = 0;
		volume[IFRONT_CENTER] = 0;

		volume[IREAR_RIGHTD] = 0;
		volume[IREAR_LEFTD] = 0;
		volume[IFRONT_CENTERD] = 0;

	}

	bool fzerovolume = true;

	for (i = 0; i < CCHANVOLUMES; i++)
	{
		volume[i] = clamp(volume[i], 0, 255);

		if (volume[i])
			fzerovolume = false;
	}

	
	if ( fzerovolume )
	{
		// DevMsg ("Skipping mix of 0 volume sound! \n");
		return false;
	}

	return true;
}


//===============================================================================
// Low level mixing routines
//===============================================================================
void Snd_WriteLinearBlastStereo16( void )
{
#if	!id386
	int		i;
	int		val;

	for ( i=0; i<snd_linear_count; i+=2 )
	{
		// scale and clamp left 16bit signed: [0x8000, 0x7FFF]
		val = ( snd_p[i] * snd_vol )>>8;
		if ( val > 32767 )
			snd_out[i] = 32767;
		else if ( val < -32768 )
			snd_out[i] = -32768;
		else
			snd_out[i] = val;

		// scale and clamp right 16bit signed: [0x8000, 0x7FFF]
		val = ( snd_p[i+1] * snd_vol )>>8;
		if ( val > 32767 )
			snd_out[i+1] = 32767;
		else if ( val < -32768 )
			snd_out[i+1] = -32768;
		else
			snd_out[i+1] = val;
	}
#else
	__asm 
	{
		// input data
		mov	ebx,snd_p

		// output data
		mov	edi,snd_out

		// iterate from end to beginning
		mov	ecx,snd_linear_count

		// scale table
		mov	esi,snd_vol

		// scale and clamp 16bit signed lsw: [0x8000, 0x7FFF]
WLBS16_LoopTop:
		mov		eax,[ebx+ecx*4-8]
		imul	eax,esi
		sar		eax,0x08
		cmp		eax,0x7FFF
		jg		WLBS16_ClampHigh
		cmp		eax,0xFFFF8000
		jnl		WLBS16_ClampDone
		mov		eax,0xFFFF8000
		jmp		WLBS16_ClampDone
WLBS16_ClampHigh:
		mov		eax,0x7FFF
WLBS16_ClampDone:

		// scale and clamp 16bit signed msw: [0x8000, 0x7FFF]
		mov		edx,[ebx+ecx*4-4]
		imul	edx,esi
		sar		edx,0x08
		cmp		edx,0x7FFF
		jg		WLBS16_ClampHigh2
		cmp		edx,0xFFFF8000
		jnl		WLBS16_ClampDone2
		mov		edx,0xFFFF8000
		jmp		WLBS16_ClampDone2
WLBS16_ClampHigh2:
		mov		edx,0x7FFF
WLBS16_ClampDone2:
		shl		edx,0x10
		and		eax,0xFFFF
		or		edx,eax
		mov		[edi+ecx*2-4],edx

		// two shorts per iteration
		sub		ecx,0x02
		jnz		WLBS16_LoopTop
	}
#endif
}

void SND_InitScaletable (void)
{
	int		i, j;
	
	for (i=0 ; i<SND_SCALE_LEVELS; i++)
		for (j=0 ; j<256 ; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * (1<<SND_SCALE_SHIFT);
}

void SND_PaintChannelFrom8(portable_samplepair_t *pOutput, int *volume, byte *pData8, int count)
{
#if	!id386
	int 	data;
	int		*lscale, *rscale;
	int		i;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for (i=0 ; i<count ; i++)
	{
		data = pData8[i];

		pOutput[i].left += lscale[data];
		pOutput[i].right += rscale[data];
	}
#else
	// portable_samplepair_t structure
#define psp_left		0
#define psp_right		4
#define psp_size		8
	static int			tempStore;
	
	__asm
	{
		// prologue
		push	ebp

		// esp = pOutput
		mov		eax, pOutput
		mov		tempStore, eax
		xchg	esp,tempStore
		// ebx = volume
		mov		ebx,volume
		// esi = pData8
		mov		esi,pData8
		// ecx = count
		mov		ecx,count

		// These values depend on the setting of SND_SCALE_BITS
		// The mask must mask off all the lower bits you aren't using in the multiply
		// so for 7 bits, the mask is 0xFE, 6 bits 0xFC, etc.
		// The shift must multiply by the table size.  There are 256 4-byte values in the table at each level.
		// So each index must be shifted left by 10, but since the bits we use are in the MSB rather than LSB
		// they must be shifted right by 8 - SND_SCALE_BITS.  e.g., for a 7 bit number the left shift is:
		// 10 - (8-7) = 9.  For a 5 bit number it's 10 - (8-5) = 7.
		mov		eax,[ebx]
		mov		edx,[ebx + 4]
		and		eax,0xFE
		and		edx,0xFE

		// shift up by 10 to index table, down by 1 to make the 7 MSB of the bytes an index
		// eax = lscale
		// edx = rscale
		shl		eax,0x09
		shl		edx,0x09
		add		eax,OFFSET snd_scaletable
		add		edx,OFFSET snd_scaletable

		// ebx = data byte
		sub		ebx,ebx
		mov		bl,[esi+ecx-1]

		// odd or even number of L/R samples
		test	ecx,0x01
		jz		PCF8_Loop

		// process odd L/R sample
		mov		edi,[eax+ebx*4]
		mov		ebp,[edx+ebx*4]
		add		edi,[esp+ecx*psp_size-psp_size+psp_left]
		add		ebp,[esp+ecx*psp_size-psp_size+psp_right]
		mov		[esp+ecx*psp_size-psp_size+psp_left],edi
		mov		[esp+ecx*psp_size-psp_size+psp_right],ebp
		mov		bl,[esi+ecx-1-1]

		dec		ecx
		jz		PCF8_Done

PCF8_Loop:
		// process L/R sample N
		mov		edi,[eax+ebx*4]
		mov		ebp,[edx+ebx*4]
		add		edi,[esp+ecx*psp_size-psp_size+psp_left]
		add		ebp,[esp+ecx*psp_size-psp_size+psp_right]
		mov		[esp+ecx*psp_size-psp_size+psp_left],edi
		mov		[esp+ecx*psp_size-psp_size+psp_right],ebp
		mov		bl,[esi+ecx-1-1]

		// process L/R sample N-1
		mov		edi,[eax+ebx*4]
		mov		ebp,[edx+ebx*4]
		add		edi,[esp+ecx*psp_size-psp_size*2+psp_left]
		add		ebp,[esp+ecx*psp_size-psp_size*2+psp_right]
		mov		[esp+ecx*psp_size-psp_size*2+psp_left],edi
		mov		[esp+ecx*psp_size-psp_size*2+psp_right],ebp
		mov		bl,[esi+ecx-1-2]

		// two L/R samples per iteration
		sub		ecx,0x02
		jnz		PCF8_Loop

PCF8_Done:
		// epilogue
		xchg	esp,tempStore
		pop		ebp
	}
#endif
}

//===============================================================================
// SOFTWARE MIXING ROUTINES
//===============================================================================

// UNDONE: optimize these

// grab samples from left source channel only and mix as if mono. 
// volume array contains appropriate spatialization volumes for doppler left (incoming sound)
void SW_Mix8StereoDopplerLeft( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// grab samples from right source channel only and mix as if mono.
// volume array contains appropriate spatialization volumes for doppler right (outgoing sound)
void SW_Mix8StereoDopplerRight( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex+1]];
		pOutput[i].right += rscale[pData[sampleIndex+1]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}

}


// grab samples from left source channel only and mix as if mono. 
// volume array contains appropriate spatialization volumes for doppler left (incoming sound)

void SW_Mix16StereoDopplerLeft( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


// grab samples from right source channel only and mix as if mono.
// volume array contains appropriate spatialization volumes for doppler right (outgoing sound)

void SW_Mix16StereoDopplerRight( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	
	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex+1]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex+1]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
void SW_Mix8StereoDirectional( float soundfacing, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int	x;
	int l,r;
	signed char lb,rb;
	int	*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256
	
	for ( int i = 0; i < outCount; i++ )
	{
		lb = (pData[sampleIndex]);		// get left byte
		rb = (pData[sampleIndex+1]);	// get right byte

		l = ((int)lb);	
		r = ((int)rb);
		
		x = ( r + ((( l - r ) * frontmix) >> 8) );

		pOutput[i].left  += lscale[x & 0xFF];			// multiply by volume and convert to 16 bit
		pOutput[i].right += rscale[x & 0xFF];

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.
void SW_Mix8StereoDirectional_Interp( float soundfacing, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interpl, interpr;
	int	*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	int	x;
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256
	
	for ( int i = 0; i < outCount; i++ )
	{
		// interpolate between first & second sample (the samples bordering sampleFrac12 fraction)
		
		first  = (int)((signed char)(pData[sampleIndex]));		// left byte
		second = (int)((signed char)(pData[sampleIndex+2]));

		interpl = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		first  = (int)((signed char)(pData[sampleIndex+1]));	// right byte
		second = (int)((signed char)(pData[sampleIndex+3]));

		interpr = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		// crossfade between right/left based on directional mix

		x = ( interpr + ((( interpl - interpr ) * frontmix) >> 8) );

		pOutput[i].left  += lscale[x & 0xFF];				// scale and convert to 16 bit
		pOutput[i].right += rscale[x & 0xFF];

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}


// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction

void SW_Mix16StereoDirectional( float soundfacing, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	fixedint sampleIndex = 0;
	fixedint sampleFrac  = inputOffset;

	int	x;
	int l, r;
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256

	for ( int i = 0; i < outCount; i++ )
	{	
		// get left, right samples

		l = (int)(pData[sampleIndex]);
		r = (int)(pData[sampleIndex+1]);

		// crossfade between left & right based on front/rear facing

		x = ( r + ((( l - r ) * frontmix) >> 8) );

		pOutput[i].left  += (volume[0] * x) >> 8;
		pOutput[i].right += (volume[1] * x) >> 8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.

void SW_Mix16StereoDirectional_Interp( float soundfacing, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int	x;
	int first, second, interpl, interpr;
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256

	for ( int i = 0; i < outCount; i++ )
	{	
		// get interpolated left, right samples

		first   = (int)(pData[sampleIndex]);
		second  = (int)(pData[sampleIndex+2]);	
		
		interpl = first + (((second - first) * (int)sampleFrac14) >> 14);

		first   = (int)(pData[sampleIndex+1]);
		second  = (int)(pData[sampleIndex+3]);	
		
		interpr = first + (((second - first) * (int)sampleFrac14) >> 14);

		// crossfade between left & right based on front/rear facing

		x = ( interpr + ((( interpl - interpr ) * frontmix) >> 8) );

		pOutput[i].left  += (volume[0] * x) >> 8;
		pOutput[i].right += (volume[1] * x) >> 8;

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}


// distance variant wav (left is close, right is far)
void SW_Mix8StereoDistVar( float distmix, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int	x;
	int l,r;
	signed char lb, rb;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)

	int nearmix  = (int)(256.0f * (1.0f - distmix));
	int	farmix = (int)(256.0f * distmix);
	
	// if mixing at max or min range, skip crossfade (KDB: perf)

	if (!nearmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			rb = (pData[sampleIndex+1]);	// get right byte
			x = (int) rb;

			pOutput[i].left  += lscale[x & 0xFF];	// multiply by volume and convert to 16 bit
			pOutput[i].right += rscale[x & 0xFF];

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	if (!farmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{

			lb = (pData[sampleIndex]);				// get left byte
			x = (int) lb;

			pOutput[i].left  += lscale[x & 0xFF];	// multiply by volume and convert to 16 bit
			pOutput[i].right += rscale[x & 0xFF];

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	// crossfade left/right

	for ( int i = 0; i < outCount; i++ )
	{

		lb = (pData[sampleIndex]);		// get left byte
		rb = (pData[sampleIndex+1]);	// get right byte

		l = (int)lb;
		r = (int)rb;

		x = ( l + (((r - l) * farmix ) >> 8) );

		pOutput[i].left  += lscale[x & 0xFF];	// multiply by volume and convert to 16 bit
		pOutput[i].right += rscale[x & 0xFF];

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


// distance variant wav (left is close, right is far)
// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.
void SW_Mix8StereoDistVar_Interp( float distmix, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int	x;

	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)

	int nearmix  = (int)(256.0f * (1.0f - distmix));
	int	farmix = (int)(256.0f * distmix);
	
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interpl, interpr;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	// if mixing at max or min range, skip crossfade (KDB: perf)

	if (!nearmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			first  = (int)((signed char)(pData[sampleIndex+1]));	// right sample
			second = (int)((signed char)(pData[sampleIndex+3]));

			interpr = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

			pOutput[i].left  += lscale[interpr & 0xFF];					// scale and convert to 16 bit
			pOutput[i].right += rscale[interpr & 0xFF];

			sampleFrac14 += rateScaleFix14;
			sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
			sampleFrac14 = FIX_FRACPART14(sampleFrac14);

		}
		return;
	}

	if (!farmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			first  = (int)((signed char)(pData[sampleIndex]));		// left sample
			second = (int)((signed char)(pData[sampleIndex+2]));

			interpl = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

			pOutput[i].left  += lscale[interpl & 0xFF];					// scale and convert to 16 bit
			pOutput[i].right += rscale[interpl & 0xFF];

			sampleFrac14 += rateScaleFix14;
			sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
			sampleFrac14 = FIX_FRACPART14(sampleFrac14);
		}
		return;
	}

	// crossfade left/right

	for ( int i = 0; i < outCount; i++ )
	{
		// interpolate between first & second sample (the samples bordering sampleFrac14 fraction)
		
		first  = (int)((signed char)(pData[sampleIndex]));
		second = (int)((signed char)(pData[sampleIndex+2]));

		interpl = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		first  = (int)((signed char)(pData[sampleIndex+1]));
		second = (int)((signed char)(pData[sampleIndex+3]));

		interpr = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		// crossfade between left and right based on distance mix

		x = ( interpl + (((interpr - interpl) * farmix ) >> 8) );

		pOutput[i].left  += lscale[x & 0xFF];				// scale and convert to 16 bit
		pOutput[i].right += rscale[x & 0xFF];

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}


// distance variant wav (left is close, right is far)

void SW_Mix16StereoDistVar( float distmix, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset; 
	int	x;
	int l,r;

	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)

	int nearmix  = Float2Int(256.0f * (1.f - distmix));
	int	farmix =  Float2Int(256.0f * distmix);

	// if mixing at max or min range, skip crossfade (KDB: perf)

	if (!nearmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			x = pData[sampleIndex+1];	// right sample

			pOutput[i].left  += (volume[0] * x)>>8;
			pOutput[i].right += (volume[1] * x)>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	if (!farmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			x = pData[sampleIndex];		// left sample
		
			pOutput[i].left  += (volume[0] * x)>>8;
			pOutput[i].right += (volume[1] * x)>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	// crossfade left/right

	for ( int i = 0; i < outCount; i++ )
	{
		l = pData[sampleIndex];
		r = pData[sampleIndex+1];

		x = ( l + (((r - l) * farmix) >> 8) );

		pOutput[i].left  += (volume[0] * x)>>8;
		pOutput[i].right += (volume[1] * x)>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// distance variant wav (left is close, right is far)
// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.

void SW_Mix16StereoDistVar_Interp( float distmix, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount ) 
{
	int	x;

	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interpl, interpr;

	
	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)

	int nearmix  = Float2Int(256.0f * (1.f - distmix));
	int	farmix =  Float2Int(256.0f * distmix);

	// if mixing at max or min range, skip crossfade (KDB: perf)

	if (!nearmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			first   = (int)(pData[sampleIndex+1]);		// right sample
			second  = (int)(pData[sampleIndex+3]);	
			interpr = first + (((second - first) * (int)sampleFrac14) >> 14);

			pOutput[i].left += (volume[0] * interpr)>>8;
			pOutput[i].right += (volume[1] * interpr)>>8;

			sampleFrac14 += rateScaleFix14;
			sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
			sampleFrac14 = FIX_FRACPART14(sampleFrac14);
		}
		return;
	}

	if (!farmix)
	{
		for ( int i = 0; i < outCount; i++ )
		{
			first   = (int)(pData[sampleIndex]);		// left sample
			second  = (int)(pData[sampleIndex+2]);		
			interpl = first + (((second - first) * (int)sampleFrac14) >> 14);
	
			pOutput[i].left += (volume[0] * interpl)>>8;
			pOutput[i].right += (volume[1] * interpl)>>8;

			sampleFrac14 += rateScaleFix14;
			sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
			sampleFrac14 = FIX_FRACPART14(sampleFrac14);
		}
		return;
	}

	// crossfade left/right

	for ( int i = 0; i < outCount; i++ )
	{
		first   = (int)(pData[sampleIndex]);
		second  = (int)(pData[sampleIndex+2]);		
		interpl = first + (((second - first) * (int)sampleFrac14) >> 14);

		first   = (int)(pData[sampleIndex+1]);
		second  = (int)(pData[sampleIndex+3]);	
		interpr = first + (((second - first) * (int)sampleFrac14) >> 14);

		// crossfade between left & right samples

		x = ( interpl + (((interpr - interpl) * farmix) >> 8) );

		pOutput[i].left += (volume[0]  * x) >> 8;
		pOutput[i].right += (volume[1] * x) >> 8;

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}

void SW_Mix8Mono( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	// Not using pitch shift?
	if ( rateScaleFix == FIX(1) )
	{
		// native code
		SND_PaintChannelFrom8( pOutput, volume, (byte *)pData, outCount );
		return;
	}

	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left  += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.
void SW_Mix8Mono_Interp( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount)
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interp;
	int	*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];
	
	// iterate 0th sample to outCount-1 sample

	for (int i = 0; i < outCount; i++ )
	{
		// interpolate between first & second sample (the samples bordering sampleFrac12 fraction)
		
		first  = (int)((signed char)(pData[sampleIndex]));
		second = (int)((signed char)(pData[sampleIndex+1]));

		interp = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		pOutput[i].left  += lscale[interp & 0xFF];				// multiply by volume and convert to 16 bit
		pOutput[i].right += rscale[interp & 0xFF];

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14);
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}

void SW_Mix8Stereo( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left  += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex+1]];

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.
void SW_Mix8Stereo_Interp( portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount)
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interpl, interpr;
	int		*lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];
	
	// iterate 0th sample to outCount-1 sample

	for (int i = 0; i < outCount; i++ )
	{
		// interpolate between first & second sample (the samples bordering sampleFrac12 fraction)
		
		first  = (int)((signed char)(pData[sampleIndex]));		// left
		second = (int)((signed char)(pData[sampleIndex+2]));

		interpl = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		first  = (int)((signed char)(pData[sampleIndex+1]));	// right
		second = (int)((signed char)(pData[sampleIndex+3]));

		interpr = first + ( ((second - first) * (int)sampleFrac14) >> 14 );

		pOutput[i].left  += lscale[interpl & 0xFF];				// multiply by volume and convert to 16 bit
		pOutput[i].right += rscale[interpr & 0xFF];

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}

void SW_Mix16Mono_Shift( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	int vol0 = volume[0];
	int vol1 = volume[1];

#if !id386
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left  += (vol0 * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (vol1 * (int)(pData[sampleIndex]))>>8;
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
#else
	// in assembly, you can make this 32.32 instead of 4.28 and use the carry flag instead of masking
	int rateScaleInt = FIX_INTPART(rateScaleFix);
	unsigned int rateScaleFrac = FIX_FRACPART(rateScaleFix) << (32-FIX_BITS);

	__asm
	{
		mov eax, volume					;
		movq mm0, DWORD PTR [eax]		; vol1, vol0 (32-bits each)
		packssdw mm0, mm0				; pack and replicate... vol1, vol0, vol1, vol0 (16-bits each)
		//pxor mm7, mm7					; mm7 is my zero register...

		xor esi, esi
		mov	eax, DWORD PTR [pOutput]	; store initial output ptr
		mov edx, DWORD PTR [pData]		; store initial input ptr
		mov ebx, inputOffset;
		mov ecx, outCount;
		
BEGINLOAD:
		movd mm2, WORD PTR [edx+2*esi]	; load first piece of data from pData
		punpcklwd mm2, mm2				; 0, 0, pData_1st, pData_1st

		add ebx, rateScaleFrac			; do the crazy fixed integer math
		adc esi, rateScaleInt

		movd mm3, WORD PTR [edx+2*esi]	; load second piece of data from pData
		punpcklwd mm3, mm3				; 0, 0, pData_2nd, pData_2nd
		punpckldq mm2, mm3				; pData_2nd, pData_2nd, pData_2nd, pData_2nd

		add ebx, rateScaleFrac			; do the crazy fixed integer math
		adc esi, rateScaleInt
	
        movq mm3, mm2					; copy the goods
		pmullw mm2, mm0					; pData_2nd*vol1, pData_2nd*vol0, pData_1st*vol1, pData_1st*vol0 (bits 0-15)
		pmulhw mm3, mm0					; pData_2nd*vol1, pData_2nd*vol0, pData_1st*vol1, pData_1st*vol0 (bits 16-31)

		movq mm4, mm2					; copy
		movq mm5, mm3					; copy

		punpcklwd mm2, mm3				; pData_1st*vol1, pData_1st*vol0 (bits 0-31)
		punpckhwd mm4, mm5				; pData_2nd*vol1, pData_2nd*vol0 (bits 0-31)
		psrad mm2, 8					; shift right by 8
		psrad mm4, 8					; shift right by 8

		add ecx, -2                     ; decrement i-value
		paddd mm2, QWORD PTR [eax]		; add to existing vals
		paddd mm4, QWORD PTR [eax+8]	;

		movq QWORD PTR [eax], mm2		; store back
		movq QWORD PTR [eax+8], mm4		;

		add eax, 10h					;
		cmp ecx, 01h                    ; see if we can quit
		jg BEGINLOAD                    ; Kipp Owens is a doof...
		jl END							; Nick Shaffner is killing me...

		movsx edi, WORD PTR [edx+2*esi] ; load first 16 bit val and zero-extend
		imul  edi, vol0					; multiply pData[sampleIndex] by volume[0]
		sar   edi, 08h                  ; divide by 256
		add DWORD PTR [eax], edi        ; add to pOutput[i].left
		
		movsx edi, WORD PTR [edx+2*esi] ; load same 16 bit val and zero-extend (cuz I thrashed the reg)
		imul  edi, vol1					; multiply pData[sampleIndex] by volume[1]
		sar   edi, 08h                  ; divide by 256
		add DWORD PTR [eax+04h], edi    ; add to pOutput[i].right
END:
		emms;
	}
#endif
}

void SW_Mix16Mono_NoShift( portable_samplepair_t *pOutput, int *volume, short *pData, int outCount )
{
	int vol0 = volume[0];
	int vol1 = volume[1];
#if !id386
	for ( int i = 0; i < outCount; i++ )
	{
		int x = *pData++;
		pOutput[i].left += (x * vol0) >> 8;
		pOutput[i].right += (x * vol1) >> 8;
	}
#else
	__asm
	{
		mov eax, volume					;
		movq mm0, DWORD PTR [eax]		; vol1, vol0 (32-bits each)
		packssdw mm0, mm0				; pack and replicate... vol1, vol0, vol1, vol0 (16-bits each)
		//pxor mm7, mm7					; mm7 is my zero register...

		mov	eax, DWORD PTR [pOutput]	; store initial output ptr
		mov edx, DWORD PTR [pData]		; store initial input ptr
		mov ecx, outCount;
		
BEGINLOAD:
		movd mm2, WORD PTR [edx]	; load first piece o data from pData
		punpcklwd mm2, mm2				; 0, 0, pData_1st, pData_1st
		add edx,2						; move to the next sample

		movd mm3, WORD PTR [edx]	; load second piece o data from pData
		punpcklwd mm3, mm3				; 0, 0, pData_2nd, pData_2nd
		punpckldq mm2, mm3				; pData_2nd, pData_2nd, pData_2nd, pData_2nd

		add edx,2						; move to the next sample
	
        movq mm3, mm2					; copy the goods
		pmullw mm2, mm0					; pData_2nd*vol1, pData_2nd*vol0, pData_1st*vol1, pData_1st*vol0 (bits 0-15)
		pmulhw mm3, mm0					; pData_2nd*vol1, pData_2nd*vol0, pData_1st*vol1, pData_1st*vol0 (bits 16-31)

		movq mm4, mm2					; copy
		movq mm5, mm3					; copy

		punpcklwd mm2, mm3				; pData_1st*vol1, pData_1st*vol0 (bits 0-31)
		punpckhwd mm4, mm5				; pData_2nd*vol1, pData_2nd*vol0 (bits 0-31)
		psrad mm2, 8					; shift right by 8
		psrad mm4, 8					; shift right by 8

		add ecx, -2                     ; decrement i-value
		paddd mm2, QWORD PTR [eax]		; add to existing vals
		paddd mm4, QWORD PTR [eax+8]	;

		movq QWORD PTR [eax], mm2		; store back
		movq QWORD PTR [eax+8], mm4		;

		add eax, 10h					;
		cmp ecx, 01h                    ; see if we can quit
		jg BEGINLOAD                    ; I can cut and paste code!
		jl END							; 

		movsx edi, WORD PTR [edx]		; load first 16 bit val and zero-extend
		mov esi,edi						; save a copy for the other channel
		imul  edi, vol0					; multiply pData[sampleIndex] by volume[0]
		sar   edi, 08h                  ; divide by 256
		add DWORD PTR [eax], edi        ; add to pOutput[i].left
		
										; esi has a copy, use it now
		imul  esi, vol1					; multiply pData[sampleIndex] by volume[1]
		sar   esi, 08h                  ; divide by 256
		add DWORD PTR [eax+04h], esi    ; add to pOutput[i].right
END:
		emms;
	}
#endif
}

void SW_Mix16Mono( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	if ( rateScaleFix == FIX(1) )
	{
		SW_Mix16Mono_NoShift( pOutput, volume, pData, outCount );
	}
	else
	{
		SW_Mix16Mono_Shift( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
	}
}

// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.

void SW_Mix16Mono_Interp( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount  )
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interp;

	for ( int i = 0; i < outCount; i++ )
	{	
		first   = (int)(pData[sampleIndex]);
		second  = (int)(pData[sampleIndex+1]);	
		
		interp = first + (((second - first) * (int)sampleFrac14) >> 14);

		pOutput[i].left  += (volume[0] * interp) >> 8;
		pOutput[i].right += (volume[1] * interp) >> 8;

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14);
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}

void SW_Mix16Stereo( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	for ( int i = 0; i < outCount; i++ )
	{
		pOutput[i].left  += (volume[0] * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex+1]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// interpolating pitch shifter - sample(s) from preceding buffer are preloaded in
// pData buffer, ensuring we can always provide 'outCount' samples.

void SW_Mix16Stereo_Interp( portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount  )
{
	fixedint sampleIndex = 0;
	fixedint rateScaleFix14 = FIX_28TO14(rateScaleFix);		// convert 28 bit fixed point to 14 bit fixed point
	fixedint sampleFrac14   = FIX_28TO14(inputOffset);

	int first, second, interpl, interpr;

	for ( int i = 0; i < outCount; i++ )
	{	
		first   = (int)(pData[sampleIndex]);
		second  = (int)(pData[sampleIndex+2]);	
		
		interpl = first + (((second - first) * (int)sampleFrac14) >> 14);

		first   = (int)(pData[sampleIndex+1]);
		second  = (int)(pData[sampleIndex+3]);	
		
		interpr = first + (((second - first) * (int)sampleFrac14) >> 14);

		pOutput[i].left  += (volume[0] * interpl) >> 8;
		pOutput[i].right += (volume[1] * interpr) >> 8;

		sampleFrac14 += rateScaleFix14;
		sampleIndex += FIX_INTPART14(sampleFrac14)<<1;
		sampleFrac14 = FIX_FRACPART14(sampleFrac14);
	}
}
// return true if mixer should use high quality pitch interpolation for this sound

bool FUseHighQualityPitch( channel_t *pChannel )
{
	// do not use interpolating pitch shifter if:
	// low quality flag set on sound (ie: wave name is prepended with CHAR_FAST_PITCH)
	// or pitch has no fractional part
	// or snd_pitchquality is 0
	if ( !snd_pitchquality.GetInt() || pChannel->flags.bfast_pitch )
		return false;

	return ( (pChannel->pitch != floor(pChannel->pitch)) );
}

//===============================================================================
// DISPATCHERS FOR MIXING ROUTINES
//===============================================================================
void Mix8MonoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	if ( FUseHighQualityPitch( pChannel ) )
		SW_Mix8Mono_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
	else
		SW_Mix8Mono( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
}

void Mix16MonoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	if ( FUseHighQualityPitch( pChannel ) )
		SW_Mix16Mono_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
	else
		// fast native coded mixers with lower quality pitch shift
		SW_Mix16Mono( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
}

void Mix8StereoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	switch ( pChannel->wavtype )
	{
	case CHAR_DOPPLER:
		SW_Mix8StereoDopplerLeft( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		SW_Mix8StereoDopplerRight( pOutput, &volume[IFRONT_LEFTD], pData, inputOffset, rateScaleFix, outCount );
		break;

	case CHAR_DIRECTIONAL:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix8StereoDirectional_Interp( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix8StereoDirectional( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;

	case CHAR_DISTVARIANT:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix8StereoDistVar_Interp( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		else
			SW_Mix8StereoDistVar( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		break;

	case CHAR_OMNI:
		// non directional stereo - all channel volumes are the same
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix8Stereo_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix8Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;

	default:
	case CHAR_SPATIALSTEREO:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix8Stereo_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix8Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	}
}


void Mix16StereoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount )
{
	switch ( pChannel->wavtype )
	{
	case CHAR_DOPPLER:
		SW_Mix16StereoDopplerLeft( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		SW_Mix16StereoDopplerRight( pOutput, &volume[IFRONT_LEFTD], pData, inputOffset, rateScaleFix, outCount );
		break;

	case CHAR_DIRECTIONAL:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix16StereoDirectional_Interp( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix16StereoDirectional( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;

	case CHAR_DISTVARIANT:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix16StereoDistVar_Interp( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		else
			SW_Mix16StereoDistVar( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		break;

	case CHAR_OMNI:
		// non directional stereo - all channel volumes are same
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix16Stereo_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix16Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;

	default:
	case CHAR_SPATIALSTEREO:
		if ( FUseHighQualityPitch( pChannel ) )
			SW_Mix16Stereo_Interp( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		else
			SW_Mix16Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	}
}


//===============================================================================
// Client entity mouth movement code.  Set entity mouthopen variable, based
// on the sound envelope of the voice channel playing.
// KellyB 10/22/97
//===============================================================================


extern IBaseClientDLL *g_ClientDLL;

// called when voice channel is first opened on this entity
static CMouthInfo *GetMouthInfoForChannel( channel_t *pChannel )
{
#ifndef DEDICATED
	// If it's a sound inside the client UI, ask the client for the mouthinfo
	if ( pChannel->soundsource == SOUND_FROM_UI_PANEL )
		return g_ClientDLL ? g_ClientDLL->GetClientUIMouthInfo() : NULL;
#endif

	int mouthentity = pChannel->speakerentity == -1 ? pChannel->soundsource : pChannel->speakerentity;

	IClientEntity *pClientEntity = entitylist->GetClientEntity( mouthentity );
	
	if( !pClientEntity )
		return NULL;

	return pClientEntity->GetMouth();
}

void SND_InitMouth( channel_t *pChannel )
{
	if ( SND_IsMouth( pChannel ) )
	{
		CMouthInfo *pMouth = GetMouthInfoForChannel(pChannel);
		// init mouth movement vars
		if ( pMouth )
		{
			pMouth->mouthopen = 0;
			pMouth->sndavg = 0;
			pMouth->sndcount = 0;
			if ( pChannel->sfx->pSource && pChannel->sfx->pSource->GetSentence() )
			{
				pMouth->AddSource( pChannel->sfx->pSource, pChannel->flags.m_bIgnorePhonemes );
			}
		}
	}
}

// called when channel stops

void SND_CloseMouth(channel_t *pChannel) 
{
	if ( SND_IsMouth( pChannel ) )
	{
		CMouthInfo *pMouth = GetMouthInfoForChannel(pChannel);
		if ( pMouth )
		{
			// shut mouth
			int idx = pMouth->GetIndexForSource( pChannel->sfx->pSource );

			if ( idx != UNKNOWN_VOICE_SOURCE )
			{
				pMouth->RemoveSourceByIndex(idx);
			}
			else
			{
				pMouth->ClearVoiceSources();
			}
			pMouth->mouthopen = 0;
		}
	}
}

#define CAVGSAMPLES 10
// need this to make the debug code below work.
//#include "snd_wave_source.h"
void SND_MoveMouth8( channel_t *ch, CAudioSource *pSource, int count ) 
{
	int 	data;
	char	*pdata = NULL;
	int		i;
	int		savg;
	int		scount;

	CMouthInfo *pMouth = GetMouthInfoForChannel( ch );
	
	if ( !pMouth )
		return;

	if ( pSource->GetSentence() )
	{
		int idx = pMouth->GetIndexForSource( pSource );
		
		if ( idx == UNKNOWN_VOICE_SOURCE )
		{
			if ( pMouth->AddSource( pSource, ch->flags.m_bIgnorePhonemes ) == NULL )
			{
				DevMsg( 1, "out of voice sources, won't lipsync %s\n", ch->sfx->getname() );
#if 0
				for ( int i = 0; i < pMouth->GetNumVoiceSources(); i++ )
				{
					CVoiceData *pVoice  = pMouth->GetVoiceSource(i);
					CAudioSourceWave *pWave = dynamic_cast<CAudioSourceWave *>(pVoice->GetSource());
					const char *pName = "unknown";
					if ( pWave && pWave->GetName() )
						pName = pWave->GetName();
					Msg("Playing %s...\n", pName );
				}
#endif
			 }
		}
		else
		{
			// Update elapsed time from mixer
			CVoiceData *vd = pMouth->GetVoiceSource( idx );
			Assert( vd );
			if ( vd )
			{
				Assert( pSource->SampleRate() > 0 );

				float elapsed = ( float )ch->pMixer->GetSamplePosition() / ( float )pSource->SampleRate();

				vd->SetElapsedTime( elapsed );
			}
		}
	}

	if ( IsX360() )
	{
		// not supporting because data is assumed to be 8 bit and bypasses mixer (decoding)
		return;
	}

	if ( pMouth->NeedsEnvelope() )
	{
		int availableSamples = pSource->GetOutputData((void**)&pdata, ch->pMixer->GetSamplePosition(), count, NULL );

		if( pdata == NULL )
			return;
		
		i = 0;
		scount = pMouth->sndcount;
		savg = 0;

		while ( i < availableSamples && scount < CAVGSAMPLES )
		{
			data = pdata[i];
			savg += abs(data);	

			i += 80 + ((byte)data & 0x1F);
			scount++;
		}

		pMouth->sndavg += savg;
		pMouth->sndcount = (byte) scount;

		if ( pMouth->sndcount >= CAVGSAMPLES ) 
		{
			pMouth->mouthopen = pMouth->sndavg / CAVGSAMPLES;
			pMouth->sndavg = 0;
			pMouth->sndcount = 0;
		}
	}
	else
	{
		pMouth->mouthopen = 0;
	}
}


void SND_UpdateMouth( channel_t *pChannel )
{
	CMouthInfo *m = GetMouthInfoForChannel( pChannel );
	if ( !m )
		return;

	if ( pChannel->sfx )
	{
		m->AddSource( pChannel->sfx->pSource, pChannel->flags.m_bIgnorePhonemes );
	}
}


void SND_ClearMouth( channel_t *pChannel )
{
	CMouthInfo *m = GetMouthInfoForChannel( pChannel );
	if ( !m )
		return;

	if ( pChannel->sfx )
	{
		m->RemoveSource( pChannel->sfx->pSource );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pChannel - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool SND_IsMouth( channel_t *pChannel )
{
#ifndef DEDICATED
	if ( pChannel->soundsource == SOUND_FROM_UI_PANEL )
		return true;
#endif

	if ( !entitylist )
	{
		return false;
	}

	if ( pChannel->entchannel == CHAN_VOICE || pChannel->entchannel == CHAN_VOICE2 )
	{
		return true;
	}

	if ( pChannel->sfx && 
		 pChannel->sfx->pSource && 
		 pChannel->sfx->pSource->GetSentence() )
	{
		return true;	
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pChannel - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool SND_ShouldPause( channel_t *pChannel )
{
	return pChannel->flags.m_bShouldPause;
}

//===============================================================================
// Movie recording support
//===============================================================================

void SND_RecordInit()
{
	g_paintedtime = 0;
	g_soundtime = 0;

	// TMP Wave file supports stereo only, so force stereo
	if ( snd_surround.GetInt() != 2 )
	{
		snd_surround.SetValue( 2 );
	}
}

void SND_MovieStart( void )
{
	if ( IsX360() )
		return;

	if ( !cl_movieinfo.IsRecording() )
		return;

	SND_RecordInit();

	// 44k: engine playback rate is now 44100...changed from 22050
	if ( cl_movieinfo.DoWav() )
	{
		WaveCreateTmpFile( cl_movieinfo.moviename, SOUND_DMA_SPEED, 16, 2 );
	}
}

void SND_MovieEnd( void )
{
	if ( IsX360() )
		return;

	if ( !cl_movieinfo.IsRecording() )
	{
		return;
	}

	if ( cl_movieinfo.DoWav() )
	{
		WaveFixupTmpFile( cl_movieinfo.moviename );
	}
}

bool SND_IsRecording()
{
	return ( ( IsReplayRendering() || cl_movieinfo.IsRecording() ) && !Con_IsVisible() );
}



extern IVideoRecorder *g_pVideoRecorder;
void SND_RecordBuffer( void )
{
	if ( IsX360() )
		return;

	if ( !SND_IsRecording() )
		return;

	int		i;
	int		val;
	int		bufferSize = snd_linear_count * sizeof(short);
	short	*tmp = (short *)_alloca( bufferSize );
	
	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = (snd_p[i]*snd_vol)>>8;
		tmp[i] = CLIP(val);
		
		val = (snd_p[i+1]*snd_vol)>>8;
		tmp[i+1] = CLIP(val);
	}

	if ( IsReplayRendering() )
	{
#if defined( REPLAY_ENABLED )
		extern IClientReplayContext *g_pClientReplayContext;
		IReplayMovieRenderer *pMovieRenderer = g_pClientReplayContext->GetMovieRenderer();
		if ( IsReplayRendering() && pMovieRenderer && pMovieRenderer->IsAudioSyncFrame() )
		{
			pMovieRenderer->RenderAudio( (unsigned char *)tmp, bufferSize, snd_linear_count );
		}
#endif
	}
	else
	{
		if ( cl_movieinfo.DoWav() )
		{
			WaveAppendTmpFile( cl_movieinfo.moviename, tmp, 16, snd_linear_count );
		}

		if ( cl_movieinfo.DoVideoSound() )
		{
			g_pVideoRecorder->AppendAudioSamples( tmp, bufferSize );
		}
	}
}
