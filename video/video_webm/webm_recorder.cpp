//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include "webm_video.h"
#include "webm_recorder.h"
#include "filesystem.h"

#ifdef _WIN32
#include "windows.h"
#endif


// Bitrate table for various resolutions, used to compute estimated size of files as well
// as to set the WebM codec.

struct WebMEncodingDataRateInfo_t
{
	int	m_XResolution;
	int m_YResolution;		
	float m_MinDataRate;			// in KBits / second
	float m_MaxDataRate;			// in KBits / second
	float m_MinAudioDataRate;			// in KBits / second
	float m_MaxAudioDataRate;			// in KBits / second            
};

// Quality is passed into us as a number between 0 and 100, we use that to scale between the min and max bitrates.
static WebMEncodingDataRateInfo_t s_WebMEncodeRates[] =
{
	{ 320,	240, 1000, 1000, 128, 128},
	{ 640,	960, 2000, 2000, 128, 128},
	{ 960,	640, 2000, 2000, 128, 128},
	{ 720,	480, 2500, 2500, 128, 128},
	{1280,	720, 5000, 5000, 384, 384},
	{1920, 1080, 8000, 8000, 384, 384},
};





// ===========================================================================
// CWebMVideoRecorder class - implements IVideoRecorder interface for
//	   WebM and buffers commands to the actual encoder object
// ===========================================================================
CWebMVideoRecorder::CWebMVideoRecorder() :
	m_LastResult( VideoResult::SUCCESS ),
	m_bHasAudio( false ),
	m_bMovieFinished( false ),
	m_SrcImageYV12Buffer( NULL ),
	m_nFramesAdded( 0 ),
	m_nAudioFramesAdded( 0 ),
	m_nSamplesAdded( 0 ),
	m_audioChannels( 2 ),
	m_audioSampleRate( 44100 ),
	m_audioBitDepth( 16 ),
	m_audioSampleGroupSize( 0 )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::CWebMVideoRecorder() \n");
#endif
}


CWebMVideoRecorder::~CWebMVideoRecorder()
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::~CWebMVideoRecorder() \n");
#endif
	if (m_SrcImageYV12Buffer)
	{
		vpx_img_free(m_SrcImageYV12Buffer);
		m_SrcImageYV12Buffer = NULL;
	}
}


// All webm movies use 1000000 as a scale
#define WEBM_TIMECODE_SCALE 1000000
// All webm movies using 128kbps as audio bitrate
#define WEBM_AUDIO_BITRATE 128000


bool CWebMVideoRecorder::CreateNewMovieFile( const char *pFilename, bool hasAudio )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::CreateNewMovieFile() \n");
#endif

	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_NOT_EMPTY( pFilename ) );

	SetResult( VideoResult::OPERATION_ALREADY_PERFORMED );

	// crete the webm file
	if (!m_mkvWriter.Open(pFilename))
	{
		SetResult( VideoResult::OPERATION_ALREADY_PERFORMED );	    
		return false;
	}

	// init the webm file and write out the header
	m_mkvMuxerSegment.Init(&m_mkvWriter);
	m_mkvMuxerSegment.set_mode(mkvmuxer::Segment::kFile);
	m_mkvMuxerSegment.OutputCues(true);


	mkvmuxer::SegmentInfo* const mkvInfo = m_mkvMuxerSegment.GetSegmentInfo();
	mkvInfo->set_timecode_scale(WEBM_TIMECODE_SCALE);

	// get the app name
	char appname[ 256 ];
	KeyValues *modinfo = new KeyValues( "ModInfo" );

	if ( modinfo->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
		Q_strncpy( appname, modinfo->GetString( "game" ), sizeof( appname ) );
	else
		Q_strncpy( appname, "Source1 Game", sizeof( appname ) );

	modinfo->deleteThis();
	modinfo = NULL;

	mkvInfo->set_writing_app(appname);

	m_bHasAudio = hasAudio;

	SetResult( VideoResult::SUCCESS );

	return true;
}


bool CWebMVideoRecorder::SetMovieVideoParameters( VideoEncodeCodec_t theCodec, int videoQuality, int movieFrameWidth, int movieFrameHeight, VideoFrameRate_t movieFPS, VideoEncodeGamma_t gamma )
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::SetMovieVideoParameters()\n");
#endif
    
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( theCodec, VideoEncodeCodec::DEFAULT_CODEC, VideoEncodeCodec::CODEC_COUNT ) );
	AssertExitF( IS_IN_RANGE( videoQuality, VideoEncodeQuality::MIN_QUALITY, VideoEncodeQuality::MAX_QUALITY ) );
	AssertExitF( IS_IN_RANGE( movieFrameWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( movieFrameHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );
	AssertExitF( IS_IN_RANGE( movieFPS.GetFPS(), cMinFPS, cMaxFPS ) );
	AssertExitF( IS_IN_RANGECOUNT( gamma, VideoEncodeGamma::NO_GAMMA_ADJUST, VideoEncodeGamma::GAMMA_COUNT ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );

	// Get the defaults for the WebM encoder
	if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &m_vpxConfig, 0) != VPX_CODEC_OK)
	{
	    SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED );
	    return false;
	}

	m_MovieFrameWidth = movieFrameWidth;
	m_MovieFrameHeight = movieFrameHeight;

	m_MovieGamma = gamma;
	
	m_vpxConfig.g_h = movieFrameHeight;
	m_vpxConfig.g_w = movieFrameWidth;

	// How many threads should we use? How about 1 per logical processor
	const CPUInformation *pi = GetCPUInformation();
	m_vpxConfig.g_threads = pi->m_nLogicalProcessors;

	// FPS
	m_vpxConfig.g_timebase.den = movieFPS.GetUnitsPerSecond();
	m_vpxConfig.g_timebase.num = movieFPS.GetUnitsPerFrame();
	
	m_MovieRecordFPS = movieFPS;

	m_DurationPerFrame = m_MovieRecordFPS.GetUnitsPerFrame();
	m_MovieTimeScale   = m_MovieRecordFPS.GetUnitsPerSecond();

	// Compute how long each frame is, in nanoseconds
	m_FrameDuration    = (unsigned long)(((double)m_DurationPerFrame*(double)1000000000)/(double)m_MovieTimeScale);

	// Set the bitrate for this size and level of quality
	m_vpxConfig.rc_target_bitrate = GetVideoDataRate(videoQuality, movieFrameWidth, movieFrameHeight);
	

#ifdef LOG_ENCODER_OPERATIONS
	Msg( "Video Frame Rate = %f FPS\n   %d time units per second\n   %d time units per frame\n", m_MovieRecordFPS.GetFPS(), m_MovieRecordFPS.GetUnitsPerSecond(), m_MovieRecordFPS.GetUnitsPerFrame() );
	if ( m_MovieRecordFPS.IsNTSCRate() )
		Msg( "   IS CONSIDERED NTSC RATE\n");
	Msg( "Using %d threads for WebM encoding\n", m_vpxConfig.g_threads);	
	Msg( "MovieTimeScale is being set to  %d\nDuration Per Frame is  %d\n", m_MovieTimeScale, m_DurationPerFrame );
	Msg( "Time per frame in nanoseconds %d\n\n", m_FrameDuration);

#endif

	// Init the codec
	if (vpx_codec_enc_init(&m_vpxContext, vpx_codec_vp8_cx(), &m_vpxConfig, 0) != VPX_CODEC_OK)
	{
	    SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED );
	    return false;
	}

	// add the video track
	m_vid_track = m_mkvMuxerSegment.AddVideoTrack(static_cast<int>(m_vpxConfig.g_w),
						    static_cast<int>(m_vpxConfig.g_h),
						    1);

	mkvmuxer::VideoTrack* const video =
	    static_cast<mkvmuxer::VideoTrack*>(
		m_mkvMuxerSegment.GetTrackByNumber(m_vid_track));

	video->set_display_width(m_vpxConfig.g_w);
	video->set_display_height(m_vpxConfig.g_h);
	video->set_frame_rate(movieFPS.GetFPS());

	return true;
}


bool CWebMVideoRecorder::SetMovieSourceImageParameters( VideoEncodeSourceFormat_t srcImageFormat, int imgWidth, int imgHeight )
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::SetMovieSourceImageParameters()\n");
#endif
    
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( srcImageFormat, VideoEncodeSourceFormat::VIDEO_FORMAT_FIRST, VideoEncodeSourceFormat::VIDEO_FORMAT_COUNT ) );
	// WebM Recorder only supports BGRA_32BIT and BGR_24BIT
	AssertExitF( (srcImageFormat == VideoEncodeSourceFormat::BGRA_32BIT) ||
	             (srcImageFormat == VideoEncodeSourceFormat::BGR_24BIT) );
	
	AssertExitF( IS_IN_RANGE( imgWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( imgHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );

	m_SrcImageWidth = imgWidth;
	m_SrcImageHeight = imgHeight;
	m_SrcImageFormat = srcImageFormat;

	// Setup the buffers for encoding the frame.  WebM requires a frame in YV12 format
	m_SrcImageYV12Buffer = vpx_img_alloc(NULL, VPX_IMG_FMT_YV12, m_SrcImageWidth, m_SrcImageHeight, 1);

	// Set Cues element attributes
	mkvmuxer::Cues* const cues = m_mkvMuxerSegment.GetCues();
	cues->set_output_block_number(1);
	m_mkvMuxerSegment.CuesTrack(m_vid_track);

	SetResult( VideoResult::SUCCESS );
	
	return true;
}


bool CWebMVideoRecorder::SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat, int audioSampleRate, AudioEncodeOptions_t audioOptions, int audioSampleGroupSize )
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::SetMovieSourceAudioParameters()\n");
#endif
    
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudio );

	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( srcAudioFormat, AudioEncodeSourceFormat::AUDIO_NONE, AudioEncodeSourceFormat::AUDIO_FORMAT_COUNT ) );
	AssertExitF( audioSampleRate == 0 || IS_IN_RANGE( audioSampleRate, cMinSampleRate, cMaxSampleRate ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );

	m_audioSampleRate = audioSampleRate;
	m_audioSampleGroupSize = audioSampleGroupSize;

	// now setup the audio
	vorbis_info_init(&m_vi);

	// We are always using 128kbps for the audio
	int ret=vorbis_encode_init(&m_vi,m_audioChannels,m_audioSampleRate,-1,WEBM_AUDIO_BITRATE,-1);
	if (ret)
	{
	    SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	    return false;
	}
	/* set up the analysis state and auxiliary encoding storage */
	vorbis_comment_init(&m_vc);

	// get the app name
	char appname[ 256 ];
	KeyValues *modinfo = new KeyValues( "ModInfo" );

	if ( modinfo->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
		Q_strncpy( appname, modinfo->GetString( "game" ), sizeof( appname ) );
	else
		Q_strncpy( appname, "Source1 Game", sizeof( appname ) );

	modinfo->deleteThis();
	modinfo = NULL;

	vorbis_comment_add_tag(&m_vc,"ENCODER",appname);

	vorbis_analysis_init(&m_vd,&m_vi);
	vorbis_block_init(&m_vd,&m_vb);

	// setup the audio track
	m_aud_track = m_mkvMuxerSegment.AddAudioTrack(static_cast<int>(m_audioSampleRate),
						      static_cast<int>(m_audioChannels),
						      0);
	if (!m_aud_track)
	{
	    printf("\n Could not add audio track.\n");
	    return false;
	}

	mkvmuxer::AudioTrack* const audio =
	    static_cast<mkvmuxer::AudioTrack*>(
		m_mkvMuxerSegment.GetTrackByNumber(m_aud_track));
	if (!audio)
	{
	    SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	    return false;
	}

	/* Vorbis streams begin with three headers; the initial header (with
	   most of the codec setup parameters) which is mandated by the Ogg
	   bitstream spec.  The second header holds any comment fields.  The
	   third header holds the bitstream codebook.  We merely need to
	   make the headers, then pass them to libvorbis one at a time;
	   libvorbis handles the additional Ogg bitstream constraints */

	{
	    ogg_packet ident_packet;
	    ogg_packet comments_packet;
	    ogg_packet setup_packet;
	    int iHeaderLength;
		uint8 *privateHeader=NULL;
		uint8 *pbPrivateHeader=NULL;

	    vorbis_analysis_headerout(&m_vd,&m_vc,&ident_packet,&comments_packet,&setup_packet);
	    iHeaderLength = 3 + ident_packet.bytes + comments_packet.bytes + setup_packet.bytes;
	    privateHeader = new uint8[iHeaderLength];
	    pbPrivateHeader = privateHeader;

	    *pbPrivateHeader++ = 2; // number of headers - 1
	    *pbPrivateHeader++ = (uint8)ident_packet.bytes;
	    *pbPrivateHeader++ = (uint8)comments_packet.bytes;
	    
	    memcpy(pbPrivateHeader, ident_packet.packet, ident_packet.bytes);
	    pbPrivateHeader+= ident_packet.bytes;

	    memcpy(pbPrivateHeader, comments_packet.packet, comments_packet.bytes);
	    pbPrivateHeader+= comments_packet.bytes;

	    memcpy(pbPrivateHeader, setup_packet.packet, setup_packet.bytes);
	    pbPrivateHeader+= setup_packet.bytes;

	    audio->SetCodecPrivate(privateHeader,iHeaderLength);
		delete [] privateHeader;
	}
	
	SetResult( VideoResult::SUCCESS );
	return true;
}


bool CWebMVideoRecorder::IsReadyToRecord()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::IsReadyToRecord()\n");
#endif

    return ( m_SrcImageYV12Buffer != NULL && !m_bMovieFinished);
}
 
 
VideoResult_t CWebMVideoRecorder::GetLastResult()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::GetLastResult()\n");
#endif

    return m_LastResult;
}


void CWebMVideoRecorder::SetResult( VideoResult_t resultCode )
{
    m_LastResult = resultCode;
}

void CWebMVideoRecorder::ConvertBGRAToYV12( void *pFrameBuffer, int nStrideAdjustBytes, vpx_image_t *m_SrcImageYV12Buffer, bool fIncludesAlpha )
{
	int iSrcBytesPerPixel;

	// Handle 32-bit with alpha and 24-bit
	if (fIncludesAlpha)
		iSrcBytesPerPixel = 4;
	else
		iSrcBytesPerPixel = 3;
	
    int srcStride = m_SrcImageWidth * iSrcBytesPerPixel + nStrideAdjustBytes;
    int iX,iY;
    byte *pSrc;
    byte *pDstY,*pDstU,*pDstV;
    byte r,g,b,a;
    byte y,u,v;

    // This isn't fast or good, but it works and that's good enough for a first pass
    pSrc = (byte *)pFrameBuffer;

    // YV12 has a complete frame of Y, followed by half sized U and V 
    pDstY = m_SrcImageYV12Buffer->planes[0];
    pDstU = m_SrcImageYV12Buffer->planes[1];
    pDstV = m_SrcImageYV12Buffer->planes[2];

    
    for (iY=0;iY<m_MovieFrameHeight;iY++)
    {
	    for(iX=0;iX<m_MovieFrameWidth;iX++)
	    {
		    b = pSrc[iSrcBytesPerPixel*iX+0];
		    g = pSrc[iSrcBytesPerPixel*iX+1];	    
		    r = pSrc[iSrcBytesPerPixel*iX+2];
		    if (fIncludesAlpha)
			    a = pSrc[iSrcBytesPerPixel*iX+3];

		    y = (byte)((66*r + 129*g + 25*b + 128) >> 8) + 16;

		    pDstY[iX] = y;
		    if ((iY%2 == 0) && (iX%2 == 0))
		    {
			    u = (byte)((-38*r - 74*g + 112*b + 128) >> 8) + 128;
			    v = (byte)((112*r - 94*g - 18*b + 128) >> 8) + 128;
		
			    pDstU[iX/2] = u;
			    pDstV[iX/2] = v;
		    }
	    }
	    // next row, using strides
	    pDstY += m_SrcImageYV12Buffer->stride[0];
	    if ((iY%2) == 0)
	    {
		    pDstU += m_SrcImageYV12Buffer->stride[1];
		    pDstV += m_SrcImageYV12Buffer->stride[2];
	    }
	    pSrc += srcStride;
    }
    
}

bool CWebMVideoRecorder::AppendVideoFrame( void *pFrameBuffer, int nStrideAdjustBytes )
{
	uint64 time_ns;    
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::AppendVideoFrame()\n");
#endif

// $$$DEBUG Dump the Frame $$$DEBUG
// If you are trying to debug how the webm libraries work it's very difficult to use the full TF2 replay system as a testbed
// so this section of code here and in the AppendAudioSamples writes out the data TF2 would send to the video recorder to
// plain files on the disc.  You can then use those files with an extracted framework to work with the webm libraries
#ifdef NEVER
	{
		FILE *fp=NULL;
		char rgch[256];
		int i,j;
		byte *pByte;
	
		sprintf(rgch, "./frames/vid_%d", m_nFramesAdded);
		fp = fopen(rgch, "wb");

		pByte = (byte *)pFrameBuffer;
		for (i=0;i<m_MovieFrameHeight;i++)
		{
			fwrite(pByte, 4, m_MovieFrameWidth, fp);
			pByte += (m_MovieFrameWidth*4 + nStrideAdjustBytes);
		}
		fclose(fp);
	
	}
#endif

	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( pFrameBuffer != nullptr );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( IsReadyToRecord() );

	// Convert the frame in pFrameBuffer into YV12 and add it to the stream
	// only convert BGRA_32BIT and BGR_24BIT right now
	switch(m_SrcImageFormat)
	{
		case VideoEncodeSourceFormat::BGR_24BIT:
			ConvertBGRAToYV12(pFrameBuffer, nStrideAdjustBytes, m_SrcImageYV12Buffer, false);
			break;
		case VideoEncodeSourceFormat::BGRA_32BIT:
			ConvertBGRAToYV12(pFrameBuffer, nStrideAdjustBytes, m_SrcImageYV12Buffer, true);
			break;
		default:
			SetResult( VideoResult::BAD_INPUT_PARAMETERS );
			return false;
	}
	

	// Compress it with the webm codec

	time_ns = ((uint64)m_FrameDuration*(uint64)(m_nFramesAdded+1));
    
	vpx_codec_err_t vpxError = vpx_codec_encode(&m_vpxContext, m_SrcImageYV12Buffer, time_ns, m_FrameDuration, 0, 0);   

	if (vpxError != VPX_CODEC_OK)
	{
		SetResult( VideoResult::VIDEO_ERROR_OCCURED );
		return false;
	}

	// Add this frame to the stream
	vpx_codec_iter_t vpxIter = NULL;
	const vpx_codec_cx_pkt_t *vpxPacket;
	bool bKeyframe=false;

	while ( ( vpxPacket = vpx_codec_get_cx_data(&m_vpxContext, &vpxIter)) != NULL )
	{
		if (vpxPacket->kind == VPX_CODEC_CX_FRAME_PKT)
		{
			// Extract if this is a keyframe from the first packet of data for each frame
			bKeyframe = vpxPacket->data.frame.flags & VPX_FRAME_IS_KEY;
	    
			m_mkvMuxerSegment.AddFrame((const uint8 *)vpxPacket->data.frame.buf, vpxPacket->data.frame.sz, m_vid_track,
			                           time_ns, bKeyframe);
		}
	
	}
	m_nFramesAdded++;

	SetResult( VideoResult::SUCCESS );

	return true;
}


bool CWebMVideoRecorder::FlushAudioSamples()
{
	ogg_packet op;

	// See if there are any samples available

	while (vorbis_analysis_blockout(&m_vd, &m_vb) == 1)
	{
		int status_analysis = vorbis_analysis(&m_vb, NULL);
		if (status_analysis)
		{
			return false;
		}
		status_analysis = vorbis_bitrate_addblock(&m_vb);
		if (status_analysis)
		{
			return false;
		}
		while ((status_analysis = vorbis_bitrate_flushpacket(&m_vd, &op)) > 0)
		{
			// Add this packet to the webm stream
			uint64  time_ns = ((uint64)m_FrameDuration*(uint64)(m_nFramesAdded+1));

			if (!m_mkvMuxerSegment.AddFrame(op.packet,
			                                op.bytes,
			                                m_aud_track,
			                                time_ns,
			                                true))
			{
				return false;
			}
		}
	}
	return true;
}

bool CWebMVideoRecorder::AppendAudioSamples( void *pSampleBuffer, size_t sampleSize )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::AppendAudioSamples()\n");
#endif
    
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudio );
	
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( pSampleBuffer != nullptr );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( IsReadyToRecord() );

// $$$DEBUG Dump the Audio $$$DEBUG
// The audio version of the dumping of video/audio data to files for debugging
// You may need to change the path to put these files where you want them.
#ifdef NEVER
	{
		FILE *fp=NULL;
		char rgch[256];
		int i,j;
		byte *pByte;
		static int i_AudSample_batch=0;
		static int i_AudSample_frame=-1;	

		if (m_nFramesAdded != i_AudSample_frame)
			i_AudSample_batch = 0;

		i_AudSample_frame = m_nFramesAdded;
	
		sprintf(rgch, "./frames/aud_%d_%d", i_AudSample_frame, i_AudSample_batch);
		fp = fopen(rgch, "wb");

		// Size
		fwrite(&sampleSize, sizeof(sampleSize), 1, fp);

		// Data
		pByte = (byte *)pSampleBuffer;
		fwrite(pByte, sampleSize, 1, fp);

		fclose(fp);
	
		i_AudSample_batch++;	
	
	}
#endif
	int num_blocks = sampleSize / ((m_audioBitDepth/8)*m_audioChannels);
	float **buffer=vorbis_analysis_buffer(&m_vd,num_blocks);

	// Deinterleave input samples, convert them to float, and store them in
	// buffer
	const int16* pPCMsamples = (int16*)pSampleBuffer;
    
	for (int i = 0; i < num_blocks; ++i)
	{
		for (int c = 0; c < m_audioChannels; ++c)
		{
			buffer[c][i] = pPCMsamples[i * m_audioChannels + c] / 32768.f;
		}
	}

	vorbis_analysis_wrote(&m_vd, num_blocks);

	FlushAudioSamples();

	return true;
}


int CWebMVideoRecorder::GetFrameCount()
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::GetFrameCount()\n");
#endif

	return m_nFramesAdded;
}


int CWebMVideoRecorder::GetSampleCount()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::GetSampleCount()\n");
#endif
    
//	return ( m_pEncoder == nullptr ) ? 0 : m_pEncoder->GetSampleCount();
    return true;
}


VideoFrameRate_t CWebMVideoRecorder::GetFPS()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::GetFPS()\n");
#endif

    return m_MovieRecordFPS;
}


int CWebMVideoRecorder::GetSampleRate()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::GetSampleRate()\n");
#endif
//	return ( m_pEncoder == nullptr ) ? 0 : m_pEncoder->GetSampleRate();
    return true;
}


bool CWebMVideoRecorder::AbortMovie()
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::AbortMovie()\n");
#endif
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
//	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	m_bMovieFinished = true;
//	return m_pEncoder->AbortMovie();
	return true;
 }


bool CWebMVideoRecorder::FinishMovie( bool SaveMovieToDisk )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::FinishMovie()\n");
#endif
	// Have to finish out the compress with a NULL frame
	// Compress it with the webm codec
	m_nFramesAdded++;
	uint64 time_ns = ((uint64)m_FrameDuration*(uint64)(m_nFramesAdded+1));
	    
	vpx_codec_err_t vpxError = vpx_codec_encode(&m_vpxContext, NULL, time_ns, m_FrameDuration, 0, 0);

	if (vpxError != VPX_CODEC_OK)
	{
		return false;
	}

	// Add this frame to the stream
	vpx_codec_iter_t vpxIter = NULL;
	const vpx_codec_cx_pkt_t *vpxPacket;

	while ( ( vpxPacket = vpx_codec_get_cx_data(&m_vpxContext, &vpxIter) ) != NULL )
	{
		if (vpxPacket->kind == VPX_CODEC_CX_FRAME_PKT)
		{
			uint64 time_ns;
			bool bKeyframe=false;
	    
			// Extract if this is a keyframe from the first packet of data for each frame
			bKeyframe = vpxPacket->data.frame.flags & VPX_FRAME_IS_KEY;
			time_ns = ((uint64)m_FrameDuration*(uint64)m_nFramesAdded);
	
			m_mkvMuxerSegment.AddFrame((const uint8 *)vpxPacket->data.frame.buf, vpxPacket->data.frame.sz, m_vid_track, time_ns, bKeyframe);
		}
	}

	m_mkvMuxerSegment.Finalize();
	m_mkvWriter.Close();

	vorbis_dsp_clear(&m_vd);
	vorbis_block_clear(&m_vb);
	vorbis_info_clear(&m_vi);
    
	m_bMovieFinished = true;

#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::FinishMovie() movie encoded.\n");
	Msg("Total frames written: %d Time for movie in nanoseconds: %lu\n", m_nFramesAdded, ((unsigned long)m_nFramesAdded*m_FrameDuration));
#endif

	SetResult( VideoResult::SUCCESS );

	return true;
}

bool CWebMVideoRecorder::EstimateMovieFileSize( size_t *pEstSize, int movieWidth, int movieHeight, VideoFrameRate_t movieFps, float movieDuration, VideoEncodeCodec_t theCodec, int videoQuality,  AudioEncodeSourceFormat_t srcAudioFormat, int audioSampleRate )
{
#ifdef LOG_ENCODER_OPERATIONS
    Msg("CWebMVideoRecorder::EstimateMovieFileSize()\n");    
#endif
    float fVidRate;
    float fAudRate;
    float movieDurationInSeconds;

    fVidRate = GetVideoDataRate(videoQuality, movieWidth, movieHeight);
    fAudRate = GetAudioDataRate(videoQuality, movieWidth, movieHeight);
	movieDurationInSeconds = movieDuration;

    // data rates is in killobits/second so convert to bytes/second
    *pEstSize = (size_t)((fVidRate*1000*movieDurationInSeconds/8) + (fAudRate*1000*movieDuration*movieDurationInSeconds/8));

    SetResult( VideoResult::SUCCESS );
    return true;
}


float CWebMVideoRecorder::GetVideoDataRate( int quality, int width, int height )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::GetDataRate()\n");
#endif
	for(int i = 0; i< ARRAYSIZE( s_WebMEncodeRates ); i++ )
	{
		if (s_WebMEncodeRates[i].m_XResolution == width && s_WebMEncodeRates[i].m_YResolution == height)
		{
			return s_WebMEncodeRates[i].m_MinDataRate + (((s_WebMEncodeRates[i].m_MaxDataRate - s_WebMEncodeRates[i].m_MinDataRate)*(float)quality)/100.0);
		}
	}
	// Didn't find the resolution, odd
	Msg("Unable to find WebM resolution (%d, %d) at quality %d\n", width, height, quality);
	// Default to 2kb/s
	return 2000.0f;	
}

float CWebMVideoRecorder::GetAudioDataRate( int quality, int width, int height )
{
#ifdef LOG_ENCODER_OPERATIONS
	Msg("CWebMVideoRecorder::GetDataRate()\n");
#endif
	for(int i = 0; i< ARRAYSIZE( s_WebMEncodeRates ); i++ )
	{
		if (s_WebMEncodeRates[i].m_XResolution == width && s_WebMEncodeRates[i].m_YResolution == height)
		{
			return s_WebMEncodeRates[i].m_MinAudioDataRate + (((s_WebMEncodeRates[i].m_MaxAudioDataRate - s_WebMEncodeRates[i].m_MinAudioDataRate)*(float)quality)/100.0);
		}
	}
	// Didn't find the resolution, odd
	Msg("Unable to find WebM resolution (%d, %d) at quality %d\n", width, height, quality);
    
	// Default to 128kb/s for audio
	return 128.0f;	
}
