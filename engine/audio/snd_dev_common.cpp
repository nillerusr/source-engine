//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Device Common Base Class.
//
//=====================================================================================//

#include "audio_pch.h"

#define ISPEAKER_RIGHT_FRONT	0
#define ISPEAKER_LEFT_FRONT		1
#define ISPEAKER_RIGHT_REAR		2
#define ISPEAKER_LEFT_REAR		3
#define ISPEAKER_CENTER_FRONT	4

extern Vector		listener_right;

extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );
extern bool MIX_ScaleChannelVolume( paintbuffer_t *pPaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );

inline bool FVolumeFrontNonZero( int *pvol )
{
	return (pvol[IFRONT_RIGHT] || pvol[IFRONT_LEFT]);
}

inline bool FVolumeRearNonZero( int *pvol )
{
	return (pvol[IREAR_RIGHT] || pvol[IREAR_LEFT]);
}

inline bool FVolumeCenterNonZero( int *pvol )
{
	return (pvol[IFRONT_CENTER] != 0);
}

// fade speaker volumes to mono, based on xfade value.
// ie: xfade 1.0 is full mono.
// ispeaker is speaker index, cspeaker is total # of speakers
// fmix2channels causes mono mix for 4 channel mix to mix down to 2 channels
//    this is used for the 2 speaker outpu case, which uses recombined 4 channel front/rear mixing

static float XfadeSpeakerVolToMono( float scale, float xfade, float ispeaker, float cspeaker, bool fmix2channels )
{
	float scale_out;
	float scale_target;

	if (cspeaker == 4 )
	{
		// mono sound distribution:
		float scale_targets[]    = {0.9, 0.9, 0.9, 0.9};	// RF, LF, RR, LR
		float scale_targets2ch[] = {0.9, 0.9, 0.0, 0.0};	// RF, LF, RR, LR

		if ( fmix2channels )
			scale_target = scale_targets2ch[clamp(FastFloatToSmallInt(ispeaker), 0, 3)];
		else
			scale_target = scale_targets[clamp(FastFloatToSmallInt(ispeaker), 0, 3)];

		goto XfadeExit;
	}

	if (cspeaker == 5 )
	{
		// mono sound distribution:
		float scale_targets[] = {0.9, 0.9, 0.5, 0.5, 0.9};	// RF, LF, RR, LR, FC
		scale_target = scale_targets[(int)clamp(FastFloatToSmallInt(ispeaker), 0, 4)];
		goto XfadeExit;
	}

	// if (cspeaker == 2 )
	scale_target = 0.9; // front 2 speakers in stereo each get 50% of total volume in mono case
	
XfadeExit:
	scale_out = scale + (scale_target - scale) * xfade;
	return scale_out;
}

// given:
//  2d yaw angle to sound source (0-360), where 0 is listener_right
//  pitch angle to source
//  angle to speaker position (0-360), where 0 is listener_right
//  speaker index
//  speaker total count,
// return: scale from 0-1.0 for speaker volume.
// NOTE: as pitch angle goes to +/- 90, sound goes to mono, all speakers.

#define PITCH_ANGLE_THRESHOLD	45.0
#define REAR_VOL_DROP			0.5
#define VOLCURVEPOWER			1.5		// 1.0 is a linear crossfade of volume between speakers.
										// 1.5 provides a smoother, nonlinear volume transition - this is done
										// because a volume of 255 played in a single speaker is
										// percieved as louder than 128 + 128 in two speakers
										// separated by at least 45 degrees.  The nonlinear curve
										// gives the volume boost needed.

static float GetSpeakerVol( float yaw_source, float pitch_source, float mono, float yaw_speaker, int ispeaker, int cspeaker, bool fmix2channels )
{
	float adif = fabs(yaw_source - yaw_speaker);
	float pitch_angle = pitch_source;
	float scale = 0.0; 
	float xfade = 0.0;

	if ( adif > 180 )
		adif = 360 - adif;

	// mono goes from 0.0 to 1.0 as listener moves into 'mono' radius of sound source.
	// Also, as pitch_angle to sound source approaches 90 (sound above/below listener), sounds become mono.
	
	// convert pitch angle to 0-90 absolute pitch
	if ( pitch_angle < 0)
		pitch_angle += 360;

	if ( pitch_angle > 180)
		pitch_angle = 360 - pitch_angle;
	
	if ( pitch_angle > 90)
		pitch_angle = 90 - (pitch_angle - 90);
		
	// calculate additional mono crossfade due to pitch angle
	if ( pitch_angle > PITCH_ANGLE_THRESHOLD )	
	{
		xfade  = ( pitch_angle - PITCH_ANGLE_THRESHOLD ) / ( 90.0 - PITCH_ANGLE_THRESHOLD );	// 0.0 -> 1.0 as angle 45->90	

		mono += xfade;
		mono = clamp(mono, 0.0f, 1.0f);
	}
	
	if ( cspeaker == 2 )
	{
		// 2 speaker (headphone) mix: speakers opposing, at 0 & 180 degrees

		scale = (1.0 - powf(adif/180.0, VOLCURVEPOWER));

		goto GetVolExit;
	}
	
	if ( adif >= 90.0 )
		goto GetVolExit;	// 0.0 scale
	
	if ( cspeaker == 4 )
	{
		// 4 ch surround: all speakers on 90 degree angles, 
		// scale ranges from 0.0 (at 90 degree difference between source and speaker)
		// to 1.0 (0 degree difference between source and speaker)

		scale = (1.0 - powf(adif/90.0, VOLCURVEPOWER));
	
		goto GetVolExit;
	}

	// 5 ch surround: 

	// rear speakers are on 90 degree angles and return 0.0->1.0 range over +/- 90 degrees each
	// center speaker is on 45 degree angle to left/right front speaker
	// center speaker has 0.0->1.0 range over 45 degrees

	switch (ispeaker)
	{
	default:
 	case ISPEAKER_RIGHT_REAR:
	case ISPEAKER_LEFT_REAR:
		{
			// rear speakers get +/- 90 degrees of linear scaling...
			scale = (1.0 - powf(adif/90.0, VOLCURVEPOWER));
			break;
		}

	case ISPEAKER_CENTER_FRONT:
		{
			// center speaker gets +/- 45 degrees of linear scaling...
			if (adif > 45.0)
				goto GetVolExit;	// 0.0 scale

			scale = (1.0 - powf(adif/45.0, VOLCURVEPOWER));
			break;
		}
	case ISPEAKER_RIGHT_FRONT:
		{
			if (yaw_source > yaw_speaker)
			{
				// if sound source is between right front speaker and center speaker, 
				// apply scaling over 75 degrees...

					if (adif > 75.0)
						goto GetVolExit;	// 0.0 scale

					scale = (1.0 - powf(adif/75.0, VOLCURVEPOWER));
			}
/*
			if (yaw_source > yaw_speaker && yaw_source < (yaw_speaker + 90.0))
			{
				// if sound source is between right front speaker and center speaker, 
				// apply scaling over 45 degrees...
				if (adif > 45.0)
					goto GetVolExit;	// 0.0 scale

					scale = (1.0 - powf(adif/45.0, VOLCURVEPOWER));
			}
*/
			else
			{
				// sound source is CW from right speaker, apply scaling over 90 degrees...
				scale = (1.0 - powf(adif/90.0, VOLCURVEPOWER));
			}

			break;
		}

	case ISPEAKER_LEFT_FRONT:
		{
			if (yaw_source < yaw_speaker )
			{
				// if sound source is between left front speaker and center speaker, 
				// apply scaling over 75 degrees...

				if (adif > 75.0)
					goto GetVolExit;	// 0.0 scale

				scale = (1.0 - powf(adif/75.0, VOLCURVEPOWER));

			}
/*
			if (yaw_source < yaw_speaker && yaw_source > (yaw_speaker - 90.0))
			{
				// if sound source is between left front speaker and center speaker, 
				// apply scaling over 45 degrees...
				if (adif > 45.0)
					goto GetVolExit;	// 0.0 scale

				scale = (1.0 - powf(adif/45.0, VOLCURVEPOWER));

			}
*/
			else
			{
				// sound source is CW from right speaker, apply scaling over 90 degrees...
				scale = (1.0 - powf(adif/90.0, VOLCURVEPOWER));
			}
			break;
		}
	}

GetVolExit:
	Assert(mono <= 1.0 && mono >= 0.0);
	Assert(scale <= 1.0 && scale >= 0.0);
	
	// crossfade speaker volumes towards mono with increased pitch angle of sound source

	scale = XfadeSpeakerVolToMono( scale, mono, ispeaker, cspeaker, fmix2channels ); 

	Assert(scale <= 1.0 && scale >= 0.0);

	return scale;
}

// given unit vector from listener to sound source,
// determine proportion of volume for sound in FL, FC, FR, RL, RR quadrants
// Scale this proportion by the distance scalar 'gain'
// If sound has 'mono' radius, blend sound to mono over 50% of radius.
void CAudioDeviceBase::SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono )
{
	VPROF("CAudioDeviceBase::SpatializeChannel");
	float rfscale, rrscale, lfscale, lrscale, fcscale;

	fcscale = rfscale = lfscale = rrscale = lrscale = 0.0;	

	// clear volumes

	for (int i = 0; i < CCHANVOLUMES/2; i++)
		volume[i] = 0;

	// linear crossfader for 2, 4 or 5 speakers, using polar coord. separation angle as linear basis

	// get pitch & yaw angle from listener origin to sound source

	QAngle angles;
	float pitch;
	float source_yaw;
	float yaw;

	VectorAngles(sourceDir, angles);

	pitch		= angles[PITCH];
	source_yaw	= angles[YAW];

	// get 2d listener yaw angle from listener right

	QAngle angles2d;
	Vector source2d;
	float listener_yaw;

	source2d.x = listener_right.x;
	source2d.y = listener_right.y;
	source2d.z = 0.0;

	VectorNormalize(source2d);

	// convert right vector to euler angles (yaw & pitch)

	VectorAngles(source2d, angles2d);

	listener_yaw = angles2d[YAW];
	
	// get yaw of sound source, with listener_yaw as reference 0.

	yaw = source_yaw - listener_yaw;

	if (yaw < 0)
		yaw += 360;

	if ( !m_bSurround )
	{
		// 2 ch stereo mixing

		if ( m_bHeadphone )
		{
			// headphone mix: (NO HRTF)

			rfscale = GetSpeakerVol( yaw, pitch, mono, 0.0,  ISPEAKER_RIGHT_FRONT, 2, false);
			lfscale = GetSpeakerVol( yaw, pitch, mono, 180.0, ISPEAKER_LEFT_FRONT, 2, false );
		}
		else
		{
			// stereo speakers at 45 & 135 degrees: (mono sounds mix down to 2 channels)
		
			rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0,  ISPEAKER_RIGHT_FRONT, 4, true );
			lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 4, true );
			rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 4, true );
			lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 4, true );

			// add sounds coming from rear (quieter)

			rfscale = clamp((rfscale + rrscale * 0.75), 0.0, 1.0); 
			lfscale = clamp((lfscale + lrscale * 0.75), 0.0, 1.0);		
			
			rrscale = 0;
			lrscale = 0;

			//DevMsg("lfscale=%f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,rfscale,lrscale,rrscale);
			//DevMsg("pitch=%f yaw=%f \n",pitch, yaw);
		}
		goto SpatialExit;
	}

	if ( m_bSurround && !m_bSurroundCenter )
	{
		// 4 ch surround

		// linearly scale with radial distance from asource to FR, FL, RR, RL
		// where FR = 45 degrees, FL = 135, RR = 315 (-45), RL = 225 (-135)

		rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0,  ISPEAKER_RIGHT_FRONT, 4, false );
		lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 4, false );
		rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 4, false );
		lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 4, false );

		// DevMsg("lfscale=%f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,rfscale,lrscale,rrscale);
		// DevMsg("pitch=%f yaw=%f \n",pitch, yaw);

		goto SpatialExit;
	}

	if ( m_bSurround && m_bSurroundCenter )
	{
		// 5 ch surround

		// linearly scale with radial distance from asource to FR, FC, FL, RR, RL
		// where FR = 45 degrees, FC = 90, FL = 135, RR = 315 (-45), RL = 225 (-135)

		rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0, ISPEAKER_RIGHT_FRONT, 5, false );
		fcscale = GetSpeakerVol( yaw, pitch, mono, 90.0, ISPEAKER_CENTER_FRONT, 5, false );
		lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 5, false );
		rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 5, false );
		lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 5, false );
		
		//DevMsg("lfscale=%f center= %f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,fcscale, rfscale,lrscale,rrscale);
		//DevMsg("pitch=%f yaw=%f \n",pitch, yaw);

		goto SpatialExit;
	}

SpatialExit:

	// scale volumes in each quadrant by distance attenuation.

	// volumes are 0-255:
	// gain is 0.0->1.0, rscale is 0.0->1.0, so scale is 0.0->1.0
	// master_vol is 0->255, so rightvol is 0->255

	volume[IFRONT_RIGHT] = (int) (master_vol * gain * rfscale);
	volume[IFRONT_LEFT] =  (int) (master_vol * gain * lfscale);
	
	volume[IFRONT_RIGHT] = clamp( volume[IFRONT_RIGHT], 0, 255 );
	volume[IFRONT_LEFT]  = clamp( volume[IFRONT_LEFT], 0, 255 );

	if ( m_bSurround )
	{
		volume[IREAR_RIGHT] = (int) (master_vol * gain * rrscale);
		volume[IREAR_LEFT] =  (int) (master_vol * gain * lrscale);

		volume[IREAR_RIGHT] = clamp( volume[IREAR_RIGHT], 0, 255 );
		volume[IREAR_LEFT] = clamp( volume[IREAR_LEFT], 0, 255 );

		if ( m_bSurroundCenter )
		{
			volume[IFRONT_CENTER] = (int) (master_vol * gain * fcscale);
			volume[IFRONT_CENTER0] = 0.0;

			volume[IFRONT_CENTER] = clamp( volume[IFRONT_CENTER], 0, 255);
		}
	}
}

void CAudioDeviceBase::ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount)
{
	VPROF("CAudioDeviceBase::ApplyDSPEffects");
	DEBUG_StartSoundMeasure( 1, samplecount );

	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );

	DEBUG_StopSoundMeasure( 1, samplecount );
}

void CAudioDeviceBase::MixBegin( int sampleCount )
{
	MIX_ClearAllPaintBuffers( sampleCount, false );
}

void CAudioDeviceBase::MixUpsample( int sampleCount, int filtertype )
{
	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = pPaint->ifilter;

	Assert (ifilter < CPAINTFILTERS);

	S_MixBufferUpsample2x( sampleCount, pPaint->pbuf, &(pPaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	if ( pPaint->fsurround )
	{
		Assert( pPaint->pbufrear );
		S_MixBufferUpsample2x( sampleCount, pPaint->pbufrear, &(pPaint->fltmemrear[ifilter][0]), CPAINTFILTERMEM, filtertype );

		if ( pPaint->fsurround_center )
		{
			Assert( pPaint->pbufcenter );
			S_MixBufferUpsample2x( sampleCount, pPaint->pbufcenter, &(pPaint->fltmemcenter[ifilter][0]), CPAINTFILTERMEM, filtertype );
		}
	}

	// make sure on next upsample pass for this paintbuffer, new filter memory is used
	pPaint->ifilter++;
}

void CAudioDeviceBase::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 1) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix8MonoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount);
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix8MonoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount  );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix8MonoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount  );
		}
	}
}

void CAudioDeviceBase::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 2 ) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix8StereoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix8StereoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix8StereoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

void CAudioDeviceBase::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 1 ) )
		return;
	
	if ( FVolumeFrontNonZero(volume) )
	{
		Mix16MonoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{		
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix16MonoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix16MonoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

void CAudioDeviceBase::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 2 ) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix16StereoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix16StereoWavtype( pChannel, pPaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix16StereoWavtype( pChannel, pPaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

// Null Audio Device
class CAudioDeviceNull : public CAudioDeviceBase
{
public:
	bool		IsActive( void ) { return false; }
	bool		Init( void ) { return true; }
	void		Shutdown( void ) {}
	void		Pause( void ) {} 
	void		UnPause( void ) {}
	float		MixDryVolume( void ) { return 0; }
	bool		Should3DMix( void ) { return false; }
	void		StopAllSounds( void ) {}
	
	int			PaintBegin( float, int, int ) { return 0; }
	void		PaintEnd( void ) {}

	void		SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono ) {}
	void		ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount ) {}
	int			GetOutputPosition( void ) { return 0; }
	void		ClearBuffer( void ) {}
	void		UpdateListener( const Vector&, const Vector&, const Vector&, const Vector& ) {}

	void		MixBegin( int ) {}
	void		MixUpsample( int sampleCount, int filtertype ) {}

	void		Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress ) {}
	void		Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress ) {}
	void		Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress ) {}
	void		Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress ) {}

	void		ChannelReset( int, int, float ) {}
	void		TransferSamples( int end ) {}
	
	const char *DeviceName( void )			{ return "Audio Disabled"; }
	int			DeviceChannels( void )		{ return 2; }
	int			DeviceSampleBits( void )	{ return 16; }
	int			DeviceSampleBytes( void )	{ return 2; }
	int			DeviceDmaSpeed( void )		{ return SOUND_DMA_SPEED; }
	int			DeviceSampleCount( void )	{ return 0; }

	bool		IsSurround( void )			{ return false; }
	bool		IsSurroundCenter( void )	{ return false; }
	bool		IsHeadphone( void )			{ return false; }
};

IAudioDevice *Audio_GetNullDevice( void )
{
	// singeton device here
	static CAudioDeviceNull nullDevice;
	return &nullDevice;
}