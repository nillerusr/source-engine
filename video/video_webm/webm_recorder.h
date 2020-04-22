//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef WEBM_RECORDER_H
#define WEBM_RECORDER_H

#ifdef _WIN32
#pragma once
#endif

//--------------------------------------------------------------------------------
//#include ""

#include "video/ivideoservices.h"

#include "video_macros.h"
#include "webm_common.h"

// comment out to prevent logging of creation data
//#define LOG_ENCODER_OPERATIONS

#if defined( LOG_ENCODER_OPERATIONS ) || defined( LOG_ENCODER_AUDIO_OPERATIONS ) ||  defined ( LOG_FRAMES_TO_TGA ) || defined ( ENABLE_EXTERNAL_ENCODER_LOGGING )
	#include <filesystem.h>
#endif


class CWebMVideoRecorder : public IVideoRecorder
{
public:
	CWebMVideoRecorder();
	~CWebMVideoRecorder();

	virtual bool EstimateMovieFileSize( size_t *pEstSize, int movieWidth, int movieHeight, VideoFrameRate_t movieFps, float movieDuration, VideoEncodeCodec_t theCodec, int videoQuality,  AudioEncodeSourceFormat_t srcAudioFormat = AudioEncodeSourceFormat::AUDIO_NONE, int audioSampleRate = 0 );

	virtual bool CreateNewMovieFile( const char *pFilename, bool hasAudioTrack = false );
		
	virtual bool SetMovieVideoParameters( VideoEncodeCodec_t theCodec, int videoQuality,  int movieFrameWidth, int movieFrameHeight, VideoFrameRate_t movieFPS, VideoEncodeGamma_t gamma = VideoEncodeGamma::NO_GAMMA_ADJUST );
	virtual bool SetMovieSourceImageParameters( VideoEncodeSourceFormat_t srcImageFormat, int imgWidth, int imgHeight );
	virtual bool SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat = AudioEncodeSourceFormat::AUDIO_NONE, int audioSampleRate = 0, AudioEncodeOptions_t audioOptions = AudioEncodeOptions::NO_AUDIO_OPTIONS, int audioSampleGroupSize = 0 );
		
	virtual bool IsReadyToRecord();
	virtual VideoResult_t GetLastResult();
		
	virtual bool AppendVideoFrame( void *pFrameBuffer, int nStrideAdjustBytes = 0 );
	virtual bool AppendAudioSamples( void *pSampleBuffer, size_t sampleSize );
		
	virtual int GetFrameCount();
	virtual int GetSampleCount();
	virtual int GetSampleRate();
	virtual VideoFrameRate_t GetFPS();
		
	virtual bool AbortMovie();
	virtual bool FinishMovie( bool SaveMovieToDisk = true );

private:
	bool FlushAudioSamples();
	void ConvertBGRAToYV12( void *pFrameBuffer, int nStrideAdjustBytes, vpx_image_t *m_SrcImageYV12Buffer, bool fIncludesAlpha );
	void SetResult( VideoResult_t resultCode );
		
	float GetVideoDataRate( int quality, int width, int height );
	float GetAudioDataRate( int quality, int width, int height );
		
	VideoResult_t m_LastResult;
	bool m_bHasAudio;
	bool m_bMovieFinished;

	int m_nFramesAdded;
	int m_nAudioFramesAdded;
	int m_nSamplesAdded;

	VideoFrameRate_t m_MovieRecordFPS;
	int m_MovieTimeScale;
	int m_DurationPerFrame;

	unsigned long m_FrameDuration;

	int m_MovieFrameWidth;
	int m_MovieFrameHeight;

	vpx_image_t *m_SrcImageYV12Buffer;

	VideoEncodeGamma_t m_MovieGamma;

	VideoEncodeSourceFormat_t m_SrcImageFormat;
	int m_SrcImageWidth;
	int m_SrcImageHeight;

	// WebM VPX
	vpx_codec_ctx_t m_vpxContext;
	vpx_codec_enc_cfg_t m_vpxConfig;
	mkvmuxer::MkvWriter m_mkvWriter;
	mkvmuxer::Segment m_mkvMuxerSegment;
	uint64 m_vid_track;

	// Vorbis audio
	uint64 m_aud_track;
	int m_audioChannels;
	int m_audioSampleRate;
	int m_audioSampleGroupSize;
	int m_audioBitDepth;

	vorbis_info m_vi;
	vorbis_dsp_state m_vd;
	vorbis_block m_vb;
	vorbis_comment m_vc;
};



#endif // WEBM_RECORDER_H
