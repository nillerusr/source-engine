//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Device Common Routines
//
//=====================================================================================//

#ifndef SND_DEV_COMMON_H
#define SND_DEV_COMMON_H
#pragma once

class CAudioDeviceBase : public IAudioDevice
{
public:	
	virtual bool		IsActive( void ) { return false; }
	virtual bool		Init( void ) { return false; }
	virtual void		Shutdown( void ) {}
	virtual void		Pause( void ) {}
	virtual void		UnPause( void ) {}
	virtual float		MixDryVolume( void ) { return 0; }
	virtual bool		Should3DMix( void ) { return m_bSurround; }
	virtual void		StopAllSounds( void ) {}

	virtual int			PaintBegin( float, int soundtime, int paintedtime ) { return 0; }
	virtual void		PaintEnd( void ) {}

	virtual void		SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono );
	virtual void		ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );
	virtual int			GetOutputPosition( void ) { return 0; }
	virtual void		ClearBuffer( void ) {}
	virtual void		UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up ) {}

	virtual void		MixBegin( int sampleCount );
	virtual void		MixUpsample( int sampleCount, int filtertype );

	virtual void		Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	virtual void		Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	virtual void		Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	virtual void		Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );

	virtual void		ChannelReset( int entnum, int channelIndex, float distanceMod ) {}
	virtual void		TransferSamples( int end ) {}
	
	virtual const char *DeviceName( void )			{ return NULL; }
	virtual int			DeviceChannels( void )		{ return 0; }
	virtual int			DeviceSampleBits( void )	{ return 0; }
	virtual int			DeviceSampleBytes( void )	{ return 0; }
	virtual int			DeviceDmaSpeed( void )		{ return 1; }
	virtual int			DeviceSampleCount( void )	{ return 0; }

	virtual bool		IsSurround( void )			{ return m_bSurround; }
	virtual bool		IsSurroundCenter( void )	{ return m_bSurroundCenter; }
	virtual bool		IsHeadphone( void )			{ return m_bHeadphone; }

	bool				m_bSurround;
	bool				m_bSurroundCenter;
	bool				m_bHeadphone;
};

#endif // SND_DEV_COMMON_H
