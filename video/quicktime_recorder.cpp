//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include "quicktime_recorder.h"
#include "filesystem.h"

#ifdef _WIN32
	#include "windows.h"
#endif

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

#define SAFE_DISPOSE_HANDLE( _handle )		if ( _handle != nullptr ) { DisposeHandle( (Handle) _handle ); _handle = nullptr; }
#define SAFE_DISPOSE_GWORLD( _gworld )		if ( _gworld != nullptr ) { DisposeGWorld( _gworld ); _gworld = nullptr; }
#define SAFE_DISPOSE_MOVIE( _movie )		if ( _movie != nullptr ) { DisposeMovie( _movie ); ThreadSleep(10); Assert( GetMoviesError() == noErr ); _movie = nullptr; }


// Platform check
#if defined ( OSX ) || defined ( WIN32 )
	// platform is supported
#else	
	#error "Unsupported Platform for QuickTime"
#endif



//-----------------------------------------------------------------------------
// Helper functions for copying and converting bitmaps
//-----------------------------------------------------------------------------
enum PixelComponent_t
{
	RED = 0,
	GREEN,
	BLUE,
	ALPHA
};


int GetBytesPerPixel( OSType pixelFormat )
{
	int bpp = ( pixelFormat == k24BGRPixelFormat || pixelFormat == k24RGBPixelFormat ) ? 3 : 
			  ( pixelFormat == k32BGRAPixelFormat || pixelFormat == k32RGBAPixelFormat ) ? 4 : 0;
			  
	Assert( bpp > 0 );
	return bpp;
}


int GetPixelCompnentByteOffset( OSType format, PixelComponent_t component )
{
	if ( component == RED )
	{
		return ( format == k24RGBPixelFormat || format == k32RGBAPixelFormat ) ? 0 :
			   ( format == k24BGRPixelFormat || format == k32BGRAPixelFormat ) ? 2 : -1;
	}
	if ( component == GREEN )
	{
		return ( format == k24RGBPixelFormat || format == k32RGBAPixelFormat ) ? 1 :
			   ( format == k24BGRPixelFormat || format == k32BGRAPixelFormat ) ? 1 : -1;
	}
	if ( component == BLUE )
	{
		return ( format == k24RGBPixelFormat || format == k32RGBAPixelFormat ) ? 2 :
			   ( format == k24BGRPixelFormat || format == k32BGRAPixelFormat ) ? 0 : -1;
	}
	if ( component == ALPHA )
	{
		return ( format == k32BGRAPixelFormat || format == k32RGBAPixelFormat ) ? 3 : -1;
	}

	Assert( false );
	return -1;
}


bool CopyBitMapPixels( int width, int height, OSType srcFmt, byte *srcBase, int srcStride, OSType dstFmt, byte *dstBase, int dstStride )
{
	AssertExitF( width > 0 && height > 0 && srcBase != nullptr && srcStride > 0 && dstBase != nullptr && dstStride > 0 );
	
	// copy the bitmap pixels into our GWorld
	if ( srcFmt == dstFmt )		// identical formats, memcopy each line
	{
		int srcLineSize = width * GetBytesPerPixel( srcFmt );
		AssertExitF( srcLineSize <= dstStride && srcLineSize <= srcStride );
		
		for ( int y = 0; y < height; y++ )
		{
			byte *src = srcBase + srcStride * y;
			byte *dst = dstBase + dstStride * y;
			memcpy( dst, src, srcLineSize );
		}
		
		return true;
	}

	// ok, we got some byte swizzling to do.. get the info we need	
	int srcBPP = GetBytesPerPixel( srcFmt );
	int dstBPP = GetBytesPerPixel( dstFmt );

	int rSrcIndex = GetPixelCompnentByteOffset( srcFmt, RED );
	int gSrcIndex = GetPixelCompnentByteOffset( srcFmt, GREEN );
	int bSrcIndex = GetPixelCompnentByteOffset( srcFmt, BLUE );
	int aSrcIndex = GetPixelCompnentByteOffset( srcFmt, ALPHA );
	
	int rDstIndex = GetPixelCompnentByteOffset( dstFmt, RED );
	int gDstIndex = GetPixelCompnentByteOffset( dstFmt, GREEN );
	int bDstIndex = GetPixelCompnentByteOffset( dstFmt, BLUE );
	int aDstIndex = GetPixelCompnentByteOffset( dstFmt, ALPHA );

	Assert( rSrcIndex >= 0 && gSrcIndex >= 0 && bSrcIndex >= 0 );
	
	// 3 byte format to 3 byte format or a 4 byte format to a 3 byte format?
	if ( dstBPP == 3 )
	{
		for ( int y = 0; y < height; y++ )
		{
			byte *src = srcBase + srcStride * y;
			byte *dst = dstBase + dstStride * y;
			
			for ( int x = 0; x < width; x++, dst+=dstBPP, src+=srcBPP )
			{
				dst[rDstIndex] = src[rSrcIndex];
				dst[gDstIndex] = src[gSrcIndex];
				dst[bDstIndex] = src[bSrcIndex];
			}
		}
		return true;
	}

	AssertExitF( aDstIndex >= 0 );
	
	// 3 byte format to 4 byte format?
	if ( srcBPP == 3 && dstBPP == 4 )
	{
		for ( int y = 0; y < height; y++ )
		{
			byte *src = srcBase + srcStride * y;
			byte *dst = dstBase + dstStride * y;
			
			for ( int x = 0; x < width; x++, dst+=dstBPP, src+=srcBPP )
			{
				dst[rDstIndex] = src[rSrcIndex];
				dst[gDstIndex] = src[gSrcIndex];
				dst[bDstIndex] = src[bSrcIndex];
				dst[aDstIndex] = 0xFF;
			}
		}
		return true;
	}
	
	// 4 byte format to 4 byte format?
	if ( srcBPP == 4 && dstBPP == 4 )
	{
		for ( int y = 0; y < height; y++ )
		{
			byte *src = srcBase + srcStride * y;
			byte *dst = dstBase + dstStride * y;
			
			for ( int x = 0; x < width; x++, dst+=dstBPP, src+=srcBPP )
			{
				dst[rDstIndex] = src[rSrcIndex];
				dst[gDstIndex] = src[gSrcIndex];
				dst[bDstIndex] = src[bSrcIndex];
				dst[aDstIndex] = src[aSrcIndex];
			}
		}
		return true;
	}

	// didn't find the format?
	Assert( false );
	return false;
}



//-----------------------------------------------------------------------------
// Utility functions to save targa images
//-----------------------------------------------------------------------------
#pragma pack( push, 1 )
struct TGA_Header
{
   public:
	byte  identsize;          // size of ID field that follows 18 byte header (0 usually)
	byte  colourmaptype;      // type of colour map 0=none, 1=has palette
	byte  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	short colourmapstart;     // first colour map entry in palette
	short colourmaplength;    // number of colours in palette
	byte  colourmapbits;      // number of bits per palette entry 15,16,24,32

	short xstart;             // image x origin
	short ystart;             // image y origin
	short width;              // image width in pixels
	short height;             // image height in pixels
	byte  bits;               // image bits per pixel 8,16,24,32
	byte  descriptor;         // image descriptor bits (vh flip bits)

	// pixel data follows header
};
#pragma pack(pop)



void SaveToTargaFile( int frameNum, const char* pBaseFileName, int width, int height, void *pPixels, OSType PixelFormat, int strideAdjust )	
{
	if ( pBaseFileName == nullptr || pPixels== nullptr ) return;

	Assert( sizeof( TGA_Header ) == 18 );
	
	TGA_Header  theHeader;
	ZeroVar( theHeader );

	int BytesPerPixel = GetBytesPerPixel( PixelFormat );
	Assert( BytesPerPixel > 0 );

	theHeader.imagetype = 2;
	theHeader.width = (short) width;
	theHeader.height = (short) height;
	theHeader.colourmapbits = BytesPerPixel * 8;
	theHeader.bits = BytesPerPixel * 8;
	theHeader.descriptor = ( BytesPerPixel == 4) ? ( 8 | 32 ) : 32;		// Targa32, Upper Left Origin, attribute (alpha) bits in bits 0-3

	char TGAFileName[MAX_PATH];
	
	V_snprintf( TGAFileName, MAX_PATH, "%s%.4d.tga", pBaseFileName, frameNum );

	FileHandle_t TGAFile = g_pFullFileSystem->Open( TGAFileName, "wb" );

	g_pFullFileSystem->Write( &theHeader, sizeof( theHeader ), TGAFile );
	
	// is the buffer in BGR format?
	if ( PixelFormat == k24BGRPixelFormat || PixelFormat == k32BGRAPixelFormat )
	{
		if ( strideAdjust == 0 )
		{
			g_pFullFileSystem->Write( pPixels, width * height * BytesPerPixel, TGAFile );
		}
		else
		{
			int lineWidth = width * BytesPerPixel;
			int lineOffset = lineWidth + strideAdjust;
			for ( int y = 0; y < height; y++ )
			{
				byte *pData = (byte*) pPixels + ( y * lineOffset );
				g_pFullFileSystem->Write( pData, lineWidth, TGAFile );
			}
		}
	
	}
	else	// we need to convert the bits from RGB to BGR
	{
		byte *pData = new byte[width * height * BytesPerPixel];

		OSType tgaFormat = ( PixelFormat == k24RGBPixelFormat ) ? k24BGRPixelFormat :
						   ( PixelFormat == k32RGBAPixelFormat ) ? k32BGRAPixelFormat : 0;
						   
		CopyBitMapPixels( width, height, PixelFormat, (byte*) pPixels, width * BytesPerPixel + strideAdjust, tgaFormat, pData, width * BytesPerPixel );
		
		g_pFullFileSystem->Write( pData, width * height * BytesPerPixel, TGAFile );
		
		delete [] pData;	
	}
	
	g_pFullFileSystem->Close( TGAFile );
	
}




// ===========================================================================
// Data tables used to estimate file size
// ===========================================================================
enum EstVideoEncodeQuality_t
{
	cVEQuality_Min = 0,
	cVEQuality_Low = 25,
	cVEQuality_Normal = 50,
	cVEQuality_High = 75,
	cVEQuality_Max = 100
};


struct EncodingDataRateInfo_t
{
		EstVideoEncodeQuality_t		m_QualitySetting;
		int							m_XResolution;
		int							m_YResolution;		
		float						m_DataRate;			// in MBits / second
};


struct VideoRes_t
{
	int		X, Y;
};


static EstVideoEncodeQuality_t s_QualityPresets[] =
{
	cVEQuality_Min,
	cVEQuality_Low,
	cVEQuality_Normal,
	cVEQuality_High,
	cVEQuality_Max
};


static VideoRes_t s_ResolutionPresets[] =
{
	{  16, 16  },
	{ 720, 480 },
	{ 640, 960 },
	{ 960, 640 },
	{ 1280, 720 },
	{ 1920, 1080 },
	{ 2048, 2048 },
};


static EncodingDataRateInfo_t  s_H264EncodeRates[] = 
{
	{ cVEQuality_Min,   16,  160, 2.00f },
	{ cVEQuality_Min,  720,  480, 2.26f },
	{ cVEQuality_Min,  640,  960, 2.73f },
	{ cVEQuality_Min,  960,  640, 2.91f },
	{ cVEQuality_Min, 1280,  720, 3.56f },
	{ cVEQuality_Min, 1920, 1080, 5.6f },
	{ cVEQuality_Min, 2048, 2048, 6.6f },
	
	{ cVEQuality_Low,   16,  160, 3.00f },
	{ cVEQuality_Low,  720,  480, 3.65f },
	{ cVEQuality_Low,  640,  960, 4.57f },
	{ cVEQuality_Low,  960,  640, 5.03f },
	{ cVEQuality_Low, 1280,  720, 6.41f },
	{ cVEQuality_Low, 1920, 1080, 10.57f },
	{ cVEQuality_Low, 2048, 2048, 13.0f },

	{ cVEQuality_Normal,   16,  160, 5.00f },
	{ cVEQuality_Normal,  720,  480, 6.4f },
	{ cVEQuality_Normal,  640,  960, 8.25f },
	{ cVEQuality_Normal,  960,  640, 9.24f },
	{ cVEQuality_Normal, 1280,  720, 12.1f },
	{ cVEQuality_Normal, 1920, 1080, 20.64f },
	{ cVEQuality_Normal, 2048, 2048, 25.0f },

	{ cVEQuality_High,   16,  160, 9.50f },
	{ cVEQuality_High,  720,  480, 11.3f },
	{ cVEQuality_High,  640,  960, 15.06f },
	{ cVEQuality_High,  960,  640, 16.9f },
	{ cVEQuality_High, 1280,  720, 22.72 },
	{ cVEQuality_High, 1920, 1080, 40.06f },
	{ cVEQuality_High, 2048, 2048, 52.5f },

	{ cVEQuality_Max,   16,  160, 15.50f },
	{ cVEQuality_Max,  720,  480, 19.33f },
	{ cVEQuality_Max,  640,  960, 29.89f },
	{ cVEQuality_Max,  960,  640, 26.82f },
	{ cVEQuality_Max, 1280,  720, 41.08f },
	{ cVEQuality_Max, 1920, 1080, 75.14f },
	{ cVEQuality_Max, 2048, 2048, 90.0f },

};




// ===========================================================================
// CQuickTimeVideoRecorder class - implements IVideoRecorder interface for
//     QuickTime, and buffers commands to the actual encoder object
// ===========================================================================
CQuickTimeVideoRecorder::CQuickTimeVideoRecorder() :
	m_pEncoder( nullptr ),
	m_LastResult( VideoResult::SUCCESS ),
	m_bHasAudio( false ),
	m_bMovieFinished( false )
{

}


CQuickTimeVideoRecorder::~CQuickTimeVideoRecorder()
{
	if ( m_pEncoder != nullptr )
	{
		// Abort any encoding in progress
		if ( !m_bMovieFinished )
		{
			AbortMovie();
		}
		SAFE_DELETE( m_pEncoder );
	}
}


bool CQuickTimeVideoRecorder::CreateNewMovieFile( const char *pFilename, bool hasAudio )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_NOT_EMPTY( pFilename ) );

	SetResult( VideoResult::OPERATION_ALREADY_PERFORMED );
	AssertExitF( m_pEncoder == nullptr  && !m_bMovieFinished );
	
	// Create new video recorder
	m_pEncoder = new CQTVideoFileComposer();
	if ( !m_pEncoder->CreateNewMovie( pFilename, hasAudio ) )
	{
		SetResult( m_pEncoder->GetResult() );	// save the error result for after the encoder goes poof
		SAFE_DELETE( m_pEncoder );
		return false;
	}

	m_bHasAudio = hasAudio;

	SetResult( VideoResult::SUCCESS );
	return true;
}


bool CQuickTimeVideoRecorder::SetMovieVideoParameters( VideoEncodeCodec_t theCodec, int videoQuality, int movieFrameWidth, int movieFrameHeight, VideoFrameRate_t movieFPS, VideoEncodeGamma_t gamma )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( theCodec, VideoEncodeCodec::DEFAULT_CODEC, VideoEncodeCodec::CODEC_COUNT ) );
	AssertExitF( IS_IN_RANGE( videoQuality, VideoEncodeQuality::MIN_QUALITY, VideoEncodeQuality::MAX_QUALITY ) );
	AssertExitF( IS_IN_RANGE( movieFrameWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( movieFrameHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );
	AssertExitF( IS_IN_RANGE( movieFPS.GetFPS(), cMinFPS, cMaxFPS ) );
	AssertExitF( IS_IN_RANGECOUNT( gamma, VideoEncodeGamma::NO_GAMMA_ADJUST, VideoEncodeGamma::GAMMA_COUNT ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	return m_pEncoder->SetMovieVideoParameters( movieFrameWidth, movieFrameHeight, movieFPS, theCodec, videoQuality, gamma );
}


bool CQuickTimeVideoRecorder::SetMovieSourceImageParameters( VideoEncodeSourceFormat_t srcImageFormat, int imgWidth, int imgHeight )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( srcImageFormat, VideoEncodeSourceFormat::VIDEO_FORMAT_FIRST, VideoEncodeSourceFormat::VIDEO_FORMAT_COUNT ) );
	AssertExitF( IS_IN_RANGE( imgWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( imgHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	return m_pEncoder->SetMovieSourceImageParameters( imgWidth, imgHeight, srcImageFormat );
}


bool CQuickTimeVideoRecorder::SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat, int audioSampleRate, AudioEncodeOptions_t audioOptions, int audioSampleGroupSize )
{
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudio );

	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( srcAudioFormat, AudioEncodeSourceFormat::AUDIO_NONE, AudioEncodeSourceFormat::AUDIO_FORMAT_COUNT ) );
	AssertExitF( audioSampleRate == 0 || IS_IN_RANGE( audioSampleRate, cMinSampleRate, cMaxSampleRate ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	bool result = m_pEncoder->SetMovieSourceAudioParameters( srcAudioFormat, audioSampleRate, audioOptions, audioSampleGroupSize );
	
	m_bHasAudio = m_pEncoder->HasAudio();	// Audio can be turned off after specifying, so reload status
	return result;
}


bool CQuickTimeVideoRecorder::IsReadyToRecord()
{
	return ( m_pEncoder == nullptr || m_bMovieFinished ) ? false : m_pEncoder->IsReadyToRecord();
}
 
 
VideoResult_t CQuickTimeVideoRecorder::GetLastResult()
{
	return  ( m_pEncoder == nullptr ) ? m_LastResult : m_pEncoder->GetResult();
}


void CQuickTimeVideoRecorder::SetResult( VideoResult_t resultCode )
{
	m_LastResult = resultCode;
	if ( m_pEncoder != nullptr )
	{
		m_pEncoder->SetResult( resultCode );
	}
}


bool CQuickTimeVideoRecorder::AppendVideoFrame( void *pFrameBuffer, int nStrideAdjustBytes )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( pFrameBuffer != nullptr );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( IsReadyToRecord() );
	
	return m_pEncoder->AppendVideoFrameToMedia( pFrameBuffer, nStrideAdjustBytes );
}


bool CQuickTimeVideoRecorder::AppendAudioSamples( void *pSampleBuffer, size_t sampleSize )
{
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudio );
	
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( pSampleBuffer != nullptr );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( IsReadyToRecord() );

	return m_pEncoder->AppendAudioSamplesToMedia( pSampleBuffer, sampleSize );
}


int CQuickTimeVideoRecorder::GetFrameCount()
{
	return ( m_pEncoder == nullptr ) ? 0 : m_pEncoder->GetFrameCount();
}


int CQuickTimeVideoRecorder::GetSampleCount()
{
	return ( m_pEncoder == nullptr ) ? 0 : m_pEncoder->GetSampleCount();
}


VideoFrameRate_t CQuickTimeVideoRecorder::GetFPS()
{
	return ( m_pEncoder == nullptr ) ? VideoFrameRate_t( 0 ) : m_pEncoder->GetFPS();
}


int CQuickTimeVideoRecorder::GetSampleRate()
{
	return ( m_pEncoder == nullptr ) ? 0 : m_pEncoder->GetSampleRate();
}


bool CQuickTimeVideoRecorder::AbortMovie()
{
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	m_bMovieFinished = true;
	return m_pEncoder->AbortMovie();
}


bool CQuickTimeVideoRecorder::FinishMovie( bool SaveMovieToDisk )
{
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_pEncoder != nullptr && !m_bMovieFinished );

	m_bMovieFinished = true;
	return m_pEncoder->FinishMovie( SaveMovieToDisk );
}


#ifdef ENABLE_EXTERNAL_ENCODER_LOGGING
bool CQuickTimeVideoRecorder::LogMessage( const char *pMsg )
{
	if ( m_pEncoder != nullptr )
	{
		m_pEncoder->LogMessage( pMsg );
	}

	return true;
}
#endif

bool CQuickTimeVideoRecorder::EstimateMovieFileSize( size_t *pEstSize, int movieWidth, int movieHeight, VideoFrameRate_t movieFps, float movieDuration, VideoEncodeCodec_t theCodec, int videoQuality,  AudioEncodeSourceFormat_t srcAudioFormat, int audioSampleRate )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertPtrExitF( pEstSize );
	*pEstSize = 0;

	AssertExitF( IS_IN_RANGE( movieWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( movieHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );
	AssertExitF( IS_IN_RANGE( movieFps.GetFPS(), cMinFPS, cMaxFPS ) && movieDuration > 0.0f );
	AssertExitF( IS_IN_RANGECOUNT( theCodec, VideoEncodeCodec::DEFAULT_CODEC, VideoEncodeCodec::CODEC_COUNT ) );
	AssertExitF( IS_IN_RANGE( videoQuality, VideoEncodeQuality::MIN_QUALITY, VideoEncodeQuality::MAX_QUALITY ) );
	AssertExitF( IS_IN_RANGECOUNT( srcAudioFormat, AudioEncodeSourceFormat::AUDIO_NONE, AudioEncodeSourceFormat::AUDIO_FORMAT_COUNT ) );
	AssertExitF( audioSampleRate == 0 || IS_IN_RANGE( audioSampleRate, cMinSampleRate, cMaxSampleRate ) );

	// Determine the Quality LERP
	int  Q1 = VideoEncodeQuality::MIN_QUALITY, Q2 = VideoEncodeQuality::MAX_QUALITY;
	float  Qlerp = 0.0f;
	bool   bQLerp = true;
	
	for ( int i = 0; i < ARRAYSIZE( s_QualityPresets ); i++ )
	{
		if ( s_QualityPresets[i] == videoQuality )
		{
			Q1 = videoQuality;
			Q2 = videoQuality;
			Qlerp = 0.0f;
			bQLerp = false;
			break;
		}
		else if ( s_QualityPresets[i] < videoQuality && s_QualityPresets[i] > Q1 )
		{
			Q1 = s_QualityPresets[i];
		}
		else if ( s_QualityPresets[i] > videoQuality && s_QualityPresets[i] < Q2 )
		{
			Q2 = s_QualityPresets[i];
		}
	}
	
	if ( bQLerp )
	{
		Qlerp =  ( (float) videoQuality - (float) Q1 ) / ( (float) Q2 - (float) Q1 ) ;
	}
	
	// determine the resolution lerp
	
	VideoRes_t		RES1 = { cMinVideoFrameWidth, cMinVideoFrameHeight }, RES2 = { cMaxVideoFrameWidth, cMaxVideoFrameHeight };
	float			RLerp = 0.0f;
	bool			bRLerp = true;
	int				nPixels = movieHeight * movieWidth;
	int				R1pixels = RES1.X * RES1.Y;
	int				R2pixels = RES2.X * RES2.Y;

	for ( int i = 0; i < ARRAYSIZE( s_ResolutionPresets ); i++ )
	{
		if ( s_ResolutionPresets[i].X == movieWidth && s_ResolutionPresets[i].Y == movieHeight )
		{
			RES1 = s_ResolutionPresets[i]; 
			RES2 = s_ResolutionPresets[i]; 
			RLerp = 0.0f;
			bRLerp = false;
			break;
		}
	
		int rPixels = s_ResolutionPresets[i].X * s_ResolutionPresets[i].Y;
		
		if ( rPixels <= nPixels && rPixels > R1pixels )
		{
			RES1 = s_ResolutionPresets[i];
			R1pixels = rPixels;
		}
		else if ( rPixels > nPixels && rPixels < R2pixels )
		{
			RES2 = s_ResolutionPresets[i];
			R2pixels = rPixels;
		}
	}
	
	if ( bRLerp )
	{
		RLerp = (float) (nPixels - R1pixels) / (float) ( R2pixels - R1pixels );
	}
	

	// Now we see what we need to do
	// We determine the estimated Data Rate
	
	float DR = 0.0f;
	
	if ( bQLerp == false && bRLerp == false )
	{
		DR = GetDataRate( videoQuality, movieWidth, movieHeight );
	}
	else if ( bQLerp == true && bRLerp == false )
	{
		float D1 = GetDataRate( Q1, movieWidth, movieHeight );
		float D2 = GetDataRate( Q2, movieWidth, movieHeight );
		
		DR = D1 + Qlerp * ( D2 - D1 );
	}
	else if ( bQLerp == false && bRLerp == true )
	{
		float D1 = GetDataRate( videoQuality, RES1.X, RES1.Y );
		float D2 = GetDataRate( videoQuality, RES2.X, RES2.Y );
		
		DR = D1 + RLerp * ( D2 - D1 );
	
	}
	else  // need the full filter
	{
		float D1 = GetDataRate( Q1, RES1.X, RES1.Y );
		float D2 = GetDataRate( Q1, RES2.X, RES2.Y );
		float D3 = GetDataRate( Q2, RES1.X, RES1.Y );
		float D4 = GetDataRate( Q2, RES2.X, RES2.Y );
	
		float I1 = D1 + Qlerp * ( D3 - D1 );
		float I2 = D2 + Qlerp * ( D4 - D2 );
		
		DR = I1 + RLerp * ( I2 - I1 );
	}

	// Now do the big computation
	// should this be 1024 * 1024?

	double  VideoData = DR * 1000000 / 8 * movieDuration ;

	// Quick hack to guess at audio data size	
	double audioData = 0;
	
	if ( srcAudioFormat == AudioEncodeSourceFormat::AUDIO_16BIT_PCMStereo )
	{
		audioData = ( audioSampleRate * 2 ) * ( 0.05 * DR );
	}
	
	*pEstSize = (size_t) VideoData + (size_t) audioData;

	SetResult( VideoResult::SUCCESS );
	return true;
}


float CQuickTimeVideoRecorder::GetDataRate( int quality, int width, int height )
{
	for (int i = 0; i < ARRAYSIZE( s_H264EncodeRates ); i++ )
	{
		if ( s_H264EncodeRates[i].m_QualitySetting == quality && s_H264EncodeRates[i].m_XResolution == width && s_H264EncodeRates[i].m_YResolution == height )
		{
			return s_H264EncodeRates[i].m_DataRate;
		}
	}

	Assert( false );	
	return 0.0f;	
}





// ------------------------------------------------------------------------
// CQTVideoFileComposer - Class to encapsulate the creation of a QuickTime
//   Movie from a sequence of uncompressed images and (future) audio samples
// ------------------------------------------------------------------------
CQTVideoFileComposer::CQTVideoFileComposer() :
	m_LastResult( VideoResult::SUCCESS ),

	m_bMovieCreated( false ),
	m_bHasAudioTrack( false ),
	
	m_bMovieConfigured( false ),
	m_bSourceImagesConfigured( false ),
	m_bSourceAudioConfigured( false ),
	
	m_bComposingMovie( false ),
	m_bMovieCompleted( false ),
	
	m_nFramesAdded( 0 ),
	m_nAudioFramesAdded( 0 ),
	m_nSamplesAdded( 0 ),
	m_nSamplesAddedToMedia( 0 ),
	
	m_MovieFrameWidth( 0 ),
	m_MovieFrameHeight( 0 ),
	
	m_MovieTimeScale( 0 ),
	m_DurationPerFrame( 0 ),
	
	m_AudioOptions( AudioEncodeOptions::NO_AUDIO_OPTIONS ),
	m_SampleGrouping( AG_NONE ),
	m_nAudioSampleGroupSize( 0 ),
	
	m_AudioSourceFrequency( 0 ),
	m_AudioBytesPerSample( 0 ),
	
	m_bBufferSourceAudio( false ),
	m_bLimitAudioDurationToVideo( false ),
	
	m_srcAudioBuffer( nullptr ),
	m_srcAudioBufferSize( 0 ),
	m_srcAudioBufferCurrentSize( 0 ),
	
	m_AudioSampleFrameCounter( 0 ),
	
	m_FileName( nullptr ),
	
	m_SrcImageWidth( 0 ),
	m_SrcImageHeight( 0 ),
	m_ScrImageMaxCompressedSize( 0 ),
	m_SrcImageBuffer( nullptr ),
	m_SrcImageCompressedBuffer( nullptr ),
	m_SrcPixelFormat( 0 ),
	m_SrcBytesPerPixel( 0 ),
	
	m_GWorldPixelFormat( 0 ),
	m_GWorldBytesPerPixel( 0 ),
	m_GWorldImageWidth( 0 ),
	m_GWorldImageHeight( 0 ),
	
	m_srcSoundDescription( nullptr ),
	
	m_EncodeQuality( CQTVideoFileComposer::DEFAULT_ENCODE_QUALITY ),
	m_VideoCodecToUse( CQTVideoFileComposer::DEFAULT_CODEC ),
	m_EncodeGamma( CQTVideoFileComposer::DEFAULT_GAMMA ),
	
	m_theSrcGWorld( nullptr ),

	m_MovieFileDataRef( nullptr ),
	m_MovieFileDataRefType( 0 ),
	m_MovieFileDataHandler( nullptr ),

	m_theMovie( nullptr ),
	m_theVideoTrack( nullptr),
	m_theAudioTrack( nullptr ),
	m_theVideoMedia( nullptr ),
	m_theAudioMedia( nullptr )
	
#ifdef LOG_ENCODER_OPERATIONS
	,m_LogFile( FILESYSTEM_INVALID_HANDLE )
#endif	
{
	m_MovieRecordFPS.SetFPS( 0, false );
	m_GWorldRect.top = m_GWorldRect.left = m_GWorldRect.bottom = m_GWorldRect.right = 0;
	
#ifdef LOG_FRAMES_TO_TGA
	ZeroVar( m_TGAFileBase );
#endif		
	
}


CQTVideoFileComposer::~CQTVideoFileComposer()
{
	if ( m_bComposingMovie )
	{
		AbortMovie();
	}

#ifdef LOG_ENCODER_OPERATIONS
	if ( m_LogFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Close( m_LogFile );
		m_LogFile = FILESYSTEM_INVALID_HANDLE;
	}

#endif


	SAFE_DELETE_ARRAY( m_FileName );
	SAFE_DELETE_ARRAY( m_SrcImageBuffer );
	SAFE_DELETE_ARRAY( m_srcAudioBuffer );

	SAFE_DISPOSE_HANDLE( m_MovieFileDataRef );
	SAFE_DISPOSE_HANDLE( m_srcSoundDescription );
	SAFE_DISPOSE_HANDLE( m_SrcImageCompressedBuffer );
	SAFE_DISPOSE_GWORLD( m_theSrcGWorld );
	
}


#ifdef LOG_ENCODER_OPERATIONS
void CQTVideoFileComposer::LogMsg( const char* pMsg, ... )
{
	const int MAX_TEXT = 8192;
	static char messageBuf[MAX_TEXT];

	if ( m_LogFile == FILESYSTEM_INVALID_HANDLE || pMsg == nullptr )
	{
		return;
	}

	va_list marker;

	va_start( marker, pMsg );

#ifdef _WIN32
	int len = _vsnprintf( messageBuf, MAX_TEXT, pMsg, marker );
#elif POSIX
	int len = vsnprintf( messageBuf, MAX_TEXT, pMsg, marker );
#else
	#error "define vsnprintf type."
#endif

	// Len < 0 represents an overflow
	if( len < 0 )
	{
		((char*) pMsg)[MAX_TEXT-1] = nullchar;
	}
	va_end( marker );

	g_pFullFileSystem->Write( messageBuf, V_strlen( messageBuf ), m_LogFile );

}
#endif 


bool CQTVideoFileComposer::CreateNewMovie( const char *fileName, bool hasAudio )
{
	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_NOT_EMPTY( fileName ) );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( !m_bMovieCreated && !m_bMovieCompleted );

#ifdef LOG_ENCODER_OPERATIONS
	char logFileName[MAX_PATH];
	V_strncpy( logFileName, fileName, MAX_PATH );
	V_SetExtension( logFileName, ".log", MAX_PATH );
	m_LogFile = g_pFullFileSystem->Open( logFileName, "wb" );
	const char* aMsg = (hasAudio) ? "HAS" : "DOES NOT HAVE";
	LogMsg( "Creating Video File: '%s'  - %s AUDIO TRACK\n", fileName, aMsg );
#endif

	// now create the movie file

	OSErr	status = noErr;
	OSType movieType = FOUR_CHAR_CODE('TVOD');			// todo - change movie type??

	m_MovieFileDataRef = nullptr;
	m_MovieFileDataRefType = 0;
	m_MovieFileDataHandler = nullptr;
	
	CFStringRef	imageStrRef = CFStringCreateWithCString ( NULL,  fileName, 0 ); 
	
	status = QTNewDataReferenceFromFullPathCFString( imageStrRef, (QTPathStyle) kQTNativeDefaultPathStyle, 0, &m_MovieFileDataRef, &m_MovieFileDataRefType );
	AssertExitF( status == noErr );
	
	
	status = CreateMovieStorage( m_MovieFileDataRef, m_MovieFileDataRefType, movieType, smCurrentScript,  createMovieFileDeleteCurFile | createMovieFileDontCreateResFile, &m_MovieFileDataHandler, &m_theMovie );
	AssertExitF( status == noErr );

	CFRelease( imageStrRef );
    m_FileName = COPY_STRING( fileName );

#ifdef LOG_FRAMES_TO_TGA
	V_strncpy( m_TGAFileBase, m_FileName, sizeof( m_TGAFileBase ) );
	V_StripExtension( m_TGAFileBase, m_TGAFileBase, sizeof( m_TGAFileBase ) );
#endif

	// we did it! party on...	
	SetResult( VideoResult::SUCCESS );
	m_bMovieCreated = true;
	m_bHasAudioTrack = hasAudio;

	return m_bMovieCreated;
}


bool CQTVideoFileComposer::SetMovieVideoParameters( int width, int height, VideoFrameRate_t movieFPS, VideoEncodeCodec_t desiredCodec, int encodeQuality, VideoEncodeGamma_t gamma )
{
	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGE( width, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( height, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );
	AssertExitF( IS_IN_RANGE( movieFPS.GetFPS(), cMinFPS, cMaxFPS ) );
	AssertExitF( IS_IN_RANGECOUNT( desiredCodec, VideoEncodeCodec::DEFAULT_CODEC, VideoEncodeCodec::CODEC_COUNT ) );
	AssertExitF( IS_IN_RANGE( encodeQuality, VideoEncodeQuality::MIN_QUALITY, VideoEncodeQuality::MAX_QUALITY ) );
	AssertExitF( IS_IN_RANGECOUNT( gamma, VideoEncodeGamma::NO_GAMMA_ADJUST, VideoEncodeGamma::GAMMA_COUNT ) );
	
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bMovieCreated && !m_bMovieConfigured );

	// Configure video parameters
	m_MovieFrameWidth = width;
	m_MovieFrameHeight = height;

	// map the requested codec in 
	switch( desiredCodec )
	{
		case VideoEncodeCodec::MPEG2_CODEC:
		{
			m_VideoCodecToUse = kMpegYUV420CodecType;
			break;
		}
		case VideoEncodeCodec::MPEG4_CODEC:
		{
			m_VideoCodecToUse = kMPEG4VisualCodecType;
			break;
		}
		case VideoEncodeCodec::H261_CODEC:
		{
			m_VideoCodecToUse = kH261CodecType;
			break;
		}
		case VideoEncodeCodec::H263_CODEC:
		{
			m_VideoCodecToUse = kH263CodecType;
			break;
		}
		case VideoEncodeCodec::H264_CODEC:
		{
			m_VideoCodecToUse = kH264CodecType;
			break;
		}
		case VideoEncodeCodec::MJPEG_A_CODEC:
		{
			m_VideoCodecToUse = kMotionJPEGACodecType;
			break;
		}
		case VideoEncodeCodec::MJPEG_B_CODEC:
		{
			m_VideoCodecToUse = kMotionJPEGBCodecType;
			break;
		}
		case VideoEncodeCodec::SORENSON3_CODEC:
		{
			m_VideoCodecToUse = kSorenson3CodecType;
			break;
		}
		case VideoEncodeCodec::CINEPACK_CODEC:
		{
			m_VideoCodecToUse = kCinepakCodecType;
			break;
		}
		default:										// should never hit this because we are already range checked
		{
			m_VideoCodecToUse = CQTVideoFileComposer::DEFAULT_CODEC;
			break;
		}
	}

	// Determine if codec is available...
	CodecInfo	theInfo;
	
	OSErr status = GetCodecInfo( &theInfo, m_VideoCodecToUse, 0 );
	if ( status == noCodecErr )
	{
		SetResult( VideoResult::CODEC_NOT_AVAILABLE );
		return false;
	}
	AssertExitF( status == noErr );

#ifdef LOG_ENCODER_OPERATIONS
	char codecName[64];
	ZeroVar( codecName );
	V_memcpy( codecName, &theInfo.typeName[1], (int) theInfo.typeName[0] );

	LogMsg( "Video Image Size is (%d x %d)\n", m_MovieFrameWidth, m_MovieFrameHeight );
	LogMsg( "Codec selected is %s\n", codecName );
	LogMsg( "Encoding Quality = %d\n", (int) encodeQuality );
	LogMsg( "Encode Gamma = %d\n", (int) gamma );
#endif

	// convert encoding quality into quicktime specific value
	int Q = (int) encodeQuality; - (int) VideoEncodeQuality::MIN_QUALITY;
	int MaxQ = (int) VideoEncodeQuality::MAX_QUALITY - (int) VideoEncodeQuality::MIN_QUALITY;
	
	m_EncodeQuality = codecLosslessQuality * ( (float) Q  / (float) MaxQ ) ;
	clamp( m_EncodeQuality, codecMinQuality, codecMaxQuality );

	// convert the gamma correction value into quicktime specific values
	switch( gamma )
	{
		case VideoEncodeGamma::NO_GAMMA_ADJUST:
		{
			m_EncodeGamma = kQTUseSourceGammaLevel;
			break;
		}
		case VideoEncodeGamma::PLATFORM_STANDARD_GAMMA:
		{
			m_EncodeGamma = kQTUsePlatformDefaultGammaLevel;
			break;
		}
		case VideoEncodeGamma::GAMMA_1_8:
		{
			m_EncodeGamma = 0x0001CCCC;		// (Fixed) Gamma 1.8
			break;
		}
		case VideoEncodeGamma::GAMMA_2_2:
		{
			m_EncodeGamma = kQTCCIR601VideoGammaLevel;
			break;
		}
		case VideoEncodeGamma::GAMMA_2_5:
		{
			m_EncodeGamma = 0x00028000;		// (Fixed) Gamma 2.5
			break;
		}
		default:
		{
			m_EncodeGamma = CQTVideoFileComposer::DEFAULT_GAMMA;
			break;
		}
	}
	
	// Process the framerate into usable values

	m_MovieRecordFPS = movieFPS;
	
	m_DurationPerFrame = m_MovieRecordFPS.GetUnitsPerFrame();
	m_MovieTimeScale   = m_MovieRecordFPS.GetUnitsPerSecond();
	
	AssertExitF( m_DurationPerFrame > 0 && m_MovieTimeScale > 0 );
	
/*	if ( movieFPS.IsNTSCRate() )
	{
		m_MovieTimeScale	= movieFPS.GetIntFPS() * 1000;
		m_DurationPerFrame	= 1001;
	}
	else if ( movieFPS.GetUnitsPerSecond() % movieFPS.GetUnitsPerFrame() == 0 )		// integer frame rate?
	{
		m_MovieTimeScale	= movieFPS.GetIntFPS() * 1000;
		m_DurationPerFrame  = 1000;
	}
	else	// round to nearest .001 second
	{
		m_MovieTimeScale  = (int) ( movieFPS.GetFPS() * 1000 );
		m_DurationPerFrame  = 1000;
	}
*/

#ifdef LOG_ENCODER_OPERATIONS
	LogMsg( "Video Frame Rate = %f FPS\n   %d time units per second\n   %d time units per frame\n", m_MovieRecordFPS.GetFPS(), m_MovieRecordFPS.GetUnitsPerSecond(), m_MovieRecordFPS.GetUnitsPerFrame() );
	if ( m_MovieRecordFPS.IsNTSCRate() )
		LogMsg( "   IS CONSIDERED NTSC RATE\n");
	LogMsg( "MovieTimeScale is being set to  %d\nDuration Per Frame is  %d\n\n", m_MovieTimeScale, m_DurationPerFrame );
#endif

	// Create the video track and media
	SetResult( VideoResult::VIDEO_ERROR_OCCURED );

	m_theVideoTrack = NewMovieTrack( m_theMovie, FixRatio( width, 1 ), FixRatio( height, 1 ), kNoVolume );
	AssertExitF( GetMoviesError() == noErr );
	
	m_theVideoMedia = NewTrackMedia( m_theVideoTrack, VideoMediaType, m_MovieTimeScale, NULL, 0 );
	AssertExitF( GetMoviesError() == noErr );

	// we have successfully configured the output movie
	SetResult( VideoResult::SUCCESS );
	m_bMovieConfigured = true;
	
	return true;
}


bool CQTVideoFileComposer::SetMovieSourceImageParameters( int srcWidth, int srcHeight, VideoEncodeSourceFormat_t srcImageFormat )
{
	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGE( srcWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth ) && IS_IN_RANGE( srcHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight ) );
	AssertExitF( IS_IN_RANGECOUNT( srcImageFormat, VideoEncodeSourceFormat::VIDEO_FORMAT_FIRST, VideoEncodeSourceFormat::VIDEO_FORMAT_COUNT ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bMovieCreated && !m_bMovieCompleted && m_bMovieConfigured && !m_bSourceImagesConfigured );

	// Setup source image format related stuff
	m_SrcPixelFormat = ( srcImageFormat == VideoEncodeSourceFormat::BGRA_32BIT ) ? k32BGRAPixelFormat :
					   ( srcImageFormat == VideoEncodeSourceFormat::BGR_24BIT  ) ? k24BGRPixelFormat :
					   ( srcImageFormat == VideoEncodeSourceFormat::RGB_24BIT ) ? k24RGBPixelFormat :
					   ( srcImageFormat == VideoEncodeSourceFormat::RGBA_32BIT ) ? k32RGBAPixelFormat : 0;

	m_SrcBytesPerPixel = GetBytesPerPixel( m_SrcPixelFormat );
	
	// Setup source image size related stuff
	m_SrcImageWidth = srcWidth;
	m_SrcImageHeight = srcHeight;
	m_SrcImageSize = srcWidth * srcHeight * m_SrcBytesPerPixel;

	// Setup the GWorld to hold the frame to video compress	

	m_GWorldPixelFormat = k32BGRAPixelFormat;			// can use k24BGRPixelFormat on Win32.. but it compresses wrong on OSX?
	m_GWorldBytesPerPixel = 4;
	m_GWorldImageWidth  = CONTAINING_MULTIPLE_OF( srcWidth, 4 );			// make sure the encoded surface is a multiple of 4 in each dimensions
	m_GWorldImageHeight = CONTAINING_MULTIPLE_OF( srcHeight, 4 );
	
	m_GWorldRect.top = m_GWorldRect.left = 0;
	m_GWorldRect.bottom = m_GWorldImageHeight;
	m_GWorldRect.right = m_GWorldImageWidth;
	
	// Setup the QuiuckTime Graphics World for incoming frames of video
	// Always use a 32-bit GWORD to avoid encoding bugs
	SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	
	OSErr status = QTNewGWorld( &m_theSrcGWorld, m_GWorldPixelFormat, &m_GWorldRect, nil, nil, 0 );
	AssertExitF( status == noErr );

	PixMapHandle  thePixMap = GetGWorldPixMap( m_theSrcGWorld );
	AssertPtrExitF( thePixMap );

	status = QTSetPixMapHandleRequestedGammaLevel( thePixMap, m_EncodeGamma );
	AssertExitF( status == noErr );

	// Set encoding buffer to max size at max quality
	// Should we try it with the actual quality setting?
	
	status = GetMaxCompressionSize(	thePixMap, &m_GWorldRect, 0, m_EncodeQuality, m_VideoCodecToUse, 
									(CompressorComponent)anyCodec, (long*) &m_ScrImageMaxCompressedSize );
									
	AssertExitF( status == noErr && m_ScrImageMaxCompressedSize > 0 );

	// allocated buffers for the uncompressed and compressed images
	m_SrcImageBuffer = new byte[ m_SrcImageSize ];
	m_SrcImageCompressedBuffer = NewHandle( m_ScrImageMaxCompressedSize );

	// we have successfully configured the video input images
	SetResult( VideoResult::SUCCESS );
	m_bSourceImagesConfigured = true;
	
	return CheckForReadyness();		// We are ready to go if audio is...
}


bool CQTVideoFileComposer::SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat, int audioSampleRate, AudioEncodeOptions_t audioOptions, int audioSampleGroupSize )
{
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudioTrack );

	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_IN_RANGECOUNT( srcAudioFormat, AudioEncodeSourceFormat::AUDIO_NONE, AudioEncodeSourceFormat::AUDIO_FORMAT_COUNT ) );
	AssertExitF( audioSampleRate == 0 || IS_IN_RANGE( audioSampleRate, cMinSampleRate, cMaxSampleRate ) );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bMovieCreated && !m_bMovieCompleted && m_bMovieConfigured && !m_bSourceAudioConfigured );

	// it is possible to disable audio here by passing in AudioEncodeSourceFormat::AUDIO_NONE even
	// if the movie was created with the hasHadio flag set to true, or by setting the sample rate to 0
	
	if ( srcAudioFormat == AudioEncodeSourceFormat::AUDIO_NONE || audioSampleRate == 0 )
	{
		m_bHasAudioTrack = false;
	}
	else
	{
		m_AudioOptions = audioOptions;
	
		// Setup the audio frequency
		m_AudioSourceFrequency = audioSampleRate;

		// Create the audio track and media
		SetResult( VideoResult::AUDIO_ERROR_OCCURED );

		m_theAudioTrack = NewMovieTrack( m_theMovie, 0, 0, kFullVolume );
		AssertExitF( GetMoviesError() == noErr );
		
		m_theAudioMedia = NewTrackMedia( m_theAudioTrack, SoundMediaType, (TimeScale) audioSampleRate, NULL, 0 );
		AssertExitF( GetMoviesError() == noErr );

		// Setup the Audio Sound description
		AudioStreamBasicDescription  inASBD;
		
		switch( srcAudioFormat )
		{
			case AudioEncodeSourceFormat::AUDIO_16BIT_PCMStereo:
			{
				inASBD.mSampleRate			= Float64( audioSampleRate );
				inASBD.mFormatID			= kAudioFormatLinearPCM;
				inASBD.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
				inASBD.mBytesPerPacket		= 4;
				inASBD.mFramesPerPacket		= 1;
				inASBD.mBytesPerPacket		= 4;
				inASBD.mChannelsPerFrame	= 2;
				inASBD.mBitsPerChannel		= 16;
				inASBD.mReserved			= 0;
				break;
			}
			default:
			{
				Assert( false );			// Impossible.. we hope
				return false;
			}
		}

		m_AudioBytesPerSample = inASBD.mBytesPerPacket;
	
		OSStatus result = QTSoundDescriptionCreate( &inASBD, NULL, 0, NULL, 0, kQTSoundDescriptionKind_Movie_LowestPossibleVersion, (SoundDescriptionHandle*) &m_srcSoundDescription );
		AssertExitF( result == noErr );

		// Setup audio sample buffering if needed
		
		m_bLimitAudioDurationToVideo = BITFLAGS_SET( audioOptions, AudioEncodeOptions::LIMIT_AUDIO_TRACK_TO_VIDEO_DURATION );
		
		m_SampleGrouping = ( BITFLAGS_SET( audioOptions, AudioEncodeOptions::GROUP_SIZE_IS_VIDEO_FRAME ) ) ? CQTVideoFileComposer::AG_PER_FRAME : 
						   ( BITFLAGS_SET( audioOptions, AudioEncodeOptions::USE_AUDIO_ENCODE_GROUP_SIZE ) ) ? CQTVideoFileComposer::AG_FIXED_SIZE : AG_NONE;
		
		// check for invalid sample grouping duration
		if ( m_SampleGrouping ==  AG_FIXED_SIZE && ( audioSampleGroupSize < MIN_AUDIO_SAMPLE_GROUP_SIZE || audioSampleGroupSize > MAX_AUDIO_GROUP_SIZE_IN_SEC * m_AudioSourceFrequency ) ) 
		{
			SetResult( VideoResult::BAD_INPUT_PARAMETERS );
			Assert( false );
			return false;	
		}
		
		m_bBufferSourceAudio = ( m_SampleGrouping != AG_NONE ) || m_bLimitAudioDurationToVideo;

		// Set up an audio buffer than can hold the maxium specified duration
		if ( m_bBufferSourceAudio )
		{
			m_srcAudioBufferSize = m_AudioSourceFrequency * m_AudioBytesPerSample * MAX_AUDIO_GROUP_SIZE_IN_SEC;
			m_srcAudioBuffer = new byte[m_srcAudioBufferSize];
			m_srcAudioBufferCurrentSize = 0;
		}

		if ( m_SampleGrouping == AG_FIXED_SIZE )		
		{
			// Set up to emit audio after fixed number of samples
			m_nAudioSampleGroupSize = audioSampleGroupSize;
			
		}

		if ( m_SampleGrouping == AG_PER_FRAME )
		{
			m_AudioSampleFrameCounter = 0;
		}
	
		m_bSourceAudioConfigured = true;
	}

#ifdef LOG_ENCODER_AUDIO_OPERATIONS
	LogMsg( "Audio Sample Grouping Mode = %d\nSample Group Size = %d \n", (int) m_SampleGrouping, m_nAudioSampleGroupSize );
	LogMsg( "Audio Track Sample Rate is %d samples per second\n", m_AudioSourceFrequency );
	LogMsg( "Estimated Samples per frame = %d\n\n",  (int) ( m_AudioSourceFrequency / m_MovieRecordFPS.GetFPS() ) );
#endif

	// finish up
	SetResult( VideoResult::SUCCESS );
	
	return CheckForReadyness();				// We are ready to go if video is...
}


// Returns true if we are not ready, or if we began movie creation successfully
// This ONLY returns false if it tried to begin the movie creation process and failed
bool CQTVideoFileComposer::CheckForReadyness()
{
	return ( m_bMovieCreated && !m_bMovieCompleted && !m_bComposingMovie &&  m_bMovieConfigured && m_bSourceImagesConfigured && 
	         m_bSourceAudioConfigured == m_bHasAudioTrack ) ? BeginMovieCreation() : true;
}


bool CQTVideoFileComposer::BeginMovieCreation()
{
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bMovieCreated && !m_bMovieCompleted && !m_bComposingMovie && 
	             m_bMovieConfigured && m_bSourceImagesConfigured && m_bSourceAudioConfigured == m_bHasAudioTrack );

	// Open the tracks up for editing
	SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	OSErr status = BeginMediaEdits( m_theVideoMedia );
	AssertExitF( status == noErr );

	if ( m_bHasAudioTrack )
	{
		OSErr status = BeginMediaEdits( m_theAudioMedia );
		AssertExitF( status == noErr );
	}


#ifdef LOG_ENCODER_OPERATIONS
	LogMsg( "Media Tracks opened for editing\n\n" );
#endif


	// We are now ready to take in data to make a movie with
	SetResult( VideoResult::SUCCESS );
	m_bComposingMovie = true;
	
	return true;
}


bool CQTVideoFileComposer::AppendVideoFrameToMedia( void *ImageBuffer, int strideAdjustBytes )
{
#ifdef LOG_ENCODER_OPERATIONS
	LogMsg( "AppendVideoFrameToMedia( %8.8x ) called for %d --- ", ImageBuffer, m_nFramesAdded+1 );
#endif

	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( ImageBuffer != nullptr );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bComposingMovie && !m_bMovieCompleted );

	SetResult( VideoResult::VIDEO_ERROR_OCCURED );

	// Get the pixmap
	PixMapHandle  thePixMap = GetGWorldPixMap( m_theSrcGWorld );
	AssertPtrExitF( thePixMap );

	// copy the raw image into our bitmap
	AssertExitF( LockPixels( thePixMap ) );

	byte *srcBase = (byte*) ImageBuffer;
	int  srcStride = m_SrcImageWidth * m_SrcBytesPerPixel + strideAdjustBytes;
	
	byte *dstBase = nullptr;
	int  dstStride = 0;	

#if defined ( WIN32 )
	// Get the HBITMAP of our GWorld
	HBITMAP theHBITMAP = (HBITMAP) GetPortHBITMAP( (GrafPtr) m_theSrcGWorld );

	// retrieve the bitmap info header information
	BITMAP bmp; 
	AssertExitF( GetObject( theHBITMAP, sizeof(BITMAP), (LPSTR) &bmp) );

	// validate the BMP we just got
	AssertExitF( bmp.bmWidth == m_SrcImageWidth && bmp.bmHeight == m_SrcImageHeight && bmp.bmBitsPixel == 8 * m_GWorldBytesPerPixel );

	// setup the pixel copy info
	dstBase = (byte*)bmp.bmBits;
	dstStride = bmp.bmWidthBytes;

#elif defined ( OSX )	
	// setup the pixel copy info
	dstBase = (byte*) GetPixBaseAddr( thePixMap );
	dstStride = GetPixRowBytes( thePixMap );	

#endif

	AssertExitF( dstBase != nullptr && dstStride > 0 );

	// save a TGA if we are running diagnostics
#if defined ( LOG_FRAMES_TO_TGA )

	if ( ( m_nFramesAdded % LOG_FRAMES_TO_TGA_INTERVAL ) == 0 ) 
	{
		SaveToTargaFile( m_nFramesAdded, m_TGAFileBase, m_SrcImageWidth, m_SrcImageHeight, srcBase, m_SrcPixelFormat, strideAdjustBytes );
	}

#endif 

	// copy the supplied pixel buffer into our GWORLD data
	if ( !CopyBitMapPixels( m_SrcImageWidth, m_SrcImageHeight, 
							m_SrcPixelFormat, srcBase, srcStride, 
	                        m_GWorldPixelFormat, dstBase, dstStride ) )
	{
		Assert( false );
		return false;
	}

	// You are now free to move about the cabin...
	UnlockPixels( thePixMap );

	// allocate a handle which CompressImage will resize...
	ImageDescriptionHandle theImageDescHandle = (ImageDescriptionHandle) NewHandle( sizeof(ImageDescriptionHandle) );
	AssertExitF( theImageDescHandle != nullptr );

	// compress the single image
	OSErr status = CompressImage( thePixMap, &m_GWorldRect, m_EncodeQuality,
		   	   					  m_VideoCodecToUse, theImageDescHandle, *m_SrcImageCompressedBuffer );
	if ( status != noErr )
	{
		Assert( false );		// tell the user
		SAFE_DISPOSE_HANDLE( theImageDescHandle );
		return false;
	}

 	TimeValue	addedTime = 0;

	// Lets add gamma info the image description 	
	if ( m_EncodeGamma != kQTUseSourceGammaLevel )
	{
	 	Fixed newGamma = m_EncodeGamma;
	 	status = ICMImageDescriptionSetProperty( theImageDescHandle, kQTPropertyClass_ImageDescription, kICMImageDescriptionPropertyID_GammaLevel, sizeof( newGamma ), &newGamma );
		AssertExitF( status == noErr );
	}
	
	// add the compressed image to the movie stream
	status = AddMediaSample( m_theVideoMedia, m_SrcImageCompressedBuffer, 0, (**theImageDescHandle).dataSize, m_DurationPerFrame,	
							(SampleDescriptionHandle) theImageDescHandle, 1, 0, &addedTime );
								
	if ( status != noErr )
	{
		Assert( false );		// tell the user
		SAFE_DISPOSE_HANDLE( theImageDescHandle );
		return false;
	}


#ifdef LOG_ENCODER_OPERATIONS
	LogMsg( "Video Frame %d added to Video Media: Duration = %d, Inserted at Time %d\n", m_nFramesAdded+1, m_DurationPerFrame, addedTime );
#endif

	// free up dynamic resources
	SAFE_DISPOSE_HANDLE( theImageDescHandle );

	// Report success
	SetResult( VideoResult::SUCCESS );
	m_nFramesAdded++;
	
	return true;
}



int CQTVideoFileComposer::GetAudioSampleCountThruFrame( int frameNo )
{
	if ( frameNo < 1 )
	{
		return 0;
	}

	double  secondsSoFar  = (double) ( frameNo * m_DurationPerFrame ) / (double) m_MovieTimeScale;
	int		nAudioSamples = (int) floor( secondsSoFar * (double) m_AudioSourceFrequency );
	
	return nAudioSamples;
}



bool CQTVideoFileComposer::AppendAudioSamplesToMedia( void *soundBuffer, size_t bufferSize )
{
	SetResult( VideoResult::ILLEGAL_OPERATION );
	AssertExitF( m_bHasAudioTrack );

	// Validate input and state
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( soundBuffer != nullptr && bufferSize % m_AudioBytesPerSample == 0 );

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bComposingMovie && !m_bMovieCompleted );

	int nSamples = bufferSize / m_AudioBytesPerSample;	
	Assert( bufferSize % m_AudioBytesPerSample == 0 );

	TimeValue64		insertTime64 = 0;
	OSErr			status = noErr;

	bool	retest;
	int		samplesToEmit, MaxCanAdd, nSamplesAvailable;

	m_nSamplesAdded+= nSamples;									// track samples given to encoder

	#ifdef LOG_ENCODER_AUDIO_OPERATIONS
		LogMsg( "%d Audio Samples Submitted (%d total) -- ", nSamples, m_nSamplesAdded );
	#endif

	// We can pass in 0 bytes to trigger a flush...
	if ( nSamples == 0 )
	{
		if ( !m_bBufferSourceAudio  || m_srcAudioBufferCurrentSize == 0 )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "NO SAMPLES TO PROCESS.  EXIT\n" );
			#endif
			goto finish_up;
		}
	}
	
	// Are we not buffering audio?
	if ( !m_bBufferSourceAudio && nSamples > 0 )
	{
		SetResult( VideoResult::AUDIO_ERROR_OCCURED );
		status = AddMediaSample2( m_theAudioMedia, (const UInt8 *) soundBuffer, (ByteCount) bufferSize,
								  (TimeValue64) 1, (TimeValue64) 0, (SampleDescriptionHandle) m_srcSoundDescription,
								  (ItemCount) nSamples, 0, &insertTime64 );             
										
		AssertExitF( status == noErr );
		
		m_nSamplesAddedToMedia+= nSamples;
		
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "%d samples (%d total) inserted into Video at time %ld\n", nSamples, m_nSamplesAddedToMedia, insertTime64 );
		#endif
		
		goto finish_up;
	}

	// Buffering audio ....
	if ( nSamples > 0 )
	{
		memaddr_t pSrc = (memaddr_t) soundBuffer;
		size_t bytesToCopy = nSamples * m_AudioBytesPerSample;
		
		// Is buffer big enough to hold it all?
		if ( m_srcAudioBufferCurrentSize + bytesToCopy > m_srcAudioBufferSize )
		{
			// get a bigger buffer
			size_t  newBufferSize = m_srcAudioBufferSize * 2 + bytesToCopy;
			byte   *newBuffer = new byte[newBufferSize];
			
			// copy buffered sound and swap out buffers
			V_memcpy( newBuffer, m_srcAudioBuffer, m_srcAudioBufferCurrentSize );
			
			delete[] m_srcAudioBuffer;
			m_srcAudioBuffer = newBuffer;
			m_srcAudioBufferSize = newBufferSize;
		}
		
		// Append samples to buffer
		V_memcpy( m_srcAudioBuffer + m_srcAudioBufferCurrentSize, pSrc, bytesToCopy );
		m_srcAudioBufferCurrentSize += bytesToCopy;
	}
	
#ifdef LOG_ENCODER_AUDIO_OPERATIONS
	LogMsg( "%d Samples buffered.  Buffer=%d Samples  -- ", nSamples, ( m_srcAudioBufferCurrentSize / m_AudioBytesPerSample ) );
#endif

retest_here:
	nSamplesAvailable = m_srcAudioBufferCurrentSize / m_AudioBytesPerSample;
	samplesToEmit = 0;
	retest = false;
	MaxCanAdd = ( m_bLimitAudioDurationToVideo ) ? ( GetAudioSampleCountThruFrame( m_nFramesAdded ) - m_nSamplesAddedToMedia ) : INT32_MAX;

	if ( MaxCanAdd <= 0 )
	{
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "Can't Add Audio Now.\n" );
		#endif
		goto finish_up;
	}

	// Now.. we determine if we are ready to insert the audio samples into the media..and if so, how much...	
	
	if ( m_SampleGrouping == AG_NONE )
	{
		// are we keeping audio from getting ahead of video?
		Assert( m_bLimitAudioDurationToVideo );
		
		samplesToEmit = MIN( MaxCanAdd, nSamplesAvailable );
	}
	else if ( m_SampleGrouping == AG_FIXED_SIZE )
	{
		// do we have enough to emit a sample?
		if ( ( nSamplesAvailable < m_nAudioSampleGroupSize ) || ( m_bLimitAudioDurationToVideo && ( MaxCanAdd < m_nAudioSampleGroupSize ) ) )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				if ( nSamplesAvailable < m_nAudioSampleGroupSize ) 
					LogMsg( "Need %d Samples to emit sample group\n", m_nAudioSampleGroupSize );
				else
					LogMsg( "Audio is caught up to Video (can add %d) \n", MaxCanAdd );
			#endif
			goto finish_up;			
		}
		
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "emitting 1 group of audio (%d samples)\n", m_nAudioSampleGroupSize );
		#endif		
		
		samplesToEmit = m_nAudioSampleGroupSize;
		retest = true;
	}
	else if ( m_SampleGrouping == AG_PER_FRAME )
	{
		// is the audio already caught up with the current video frame?
		if ( m_bLimitAudioDurationToVideo && m_AudioSampleFrameCounter >= m_nFramesAdded )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Audio is caught up to Video\n" );
			#endif		
			goto finish_up;
		}
	
		int curSampleCount = GetAudioSampleCountThruFrame( m_AudioSampleFrameCounter );
		int nextSampleCount = GetAudioSampleCountThruFrame( m_AudioSampleFrameCounter+1 );
		int thisGroupSize = nextSampleCount - curSampleCount;
		
		Assert( m_nSamplesAddedToMedia == curSampleCount );

		if ( nSamplesAvailable < thisGroupSize )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Not enough samples to fill video frame (need %d)\n", thisGroupSize );
			#endif		
			goto finish_up;
		}

		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "emitting 1 video frame of audio (%d samples)\n", thisGroupSize );
		#endif		
		
		samplesToEmit = thisGroupSize;
		m_AudioSampleFrameCounter++;
		retest = true;
	}
	else
	{
		Assert( false );
	}
	
	if ( samplesToEmit > 0 )	
	{
		SetResult( VideoResult::AUDIO_ERROR_OCCURED );
	
		status = AddMediaSample2( m_theAudioMedia, (const UInt8 *) m_srcAudioBuffer, (ByteCount) samplesToEmit * m_AudioBytesPerSample,
								  (TimeValue64) 1, (TimeValue64) 0, (SampleDescriptionHandle) m_srcSoundDescription,
								  (ItemCount) samplesToEmit, 0, &insertTime64 );             
										
		AssertExitF( status == noErr );
		
		m_nSamplesAddedToMedia+= samplesToEmit;
		
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "%d samples inserted into Video (%d total) at time %ld  -- ", samplesToEmit, m_nSamplesAddedToMedia, insertTime64 );
		#endif

		// remove added samples from sound buffer
		// (this really should be a circular buffer.. but that has its own problems)		
		
		m_srcAudioBufferCurrentSize -= samplesToEmit * m_AudioBytesPerSample;
		nSamplesAvailable -= samplesToEmit;
		
		if ( nSamplesAvailable > 0 )
		{
			V_memcpy( m_srcAudioBuffer, m_srcAudioBuffer + (samplesToEmit * m_AudioBytesPerSample), nSamplesAvailable * m_AudioBytesPerSample );
		}
		
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "Buffer now =%d samples", nSamplesAvailable );
		#endif
		
		if ( retest )	
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( " -- rechecking -- "  );
			#endif
			goto retest_here;
		}
	}
	#ifdef LOG_ENCODER_AUDIO_OPERATIONS
		LogMsg( "\n" );
	#endif


finish_up:	
	// Report success
	SetResult( VideoResult::SUCCESS );
	
	return true;
}



bool CQTVideoFileComposer::SyncAndFlushAudio()
{
	if ( !m_bHasAudioTrack )
	{
		return false;
	}

	#ifdef LOG_ENCODER_AUDIO_OPERATIONS
		LogMsg( "Resolving Audio Track...\n" );
	#endif

restart_sync:
	bool	bPadWithSilence = BITFLAGS_SET( m_AudioOptions, AudioEncodeOptions::PAD_AUDIO_WITH_SILENCE );
	int		VideoDurationInSamples = GetAudioSampleCountThruFrame( m_nFramesAdded );
	int		CurShortfall = VideoDurationInSamples - m_nSamplesAddedToMedia;
	
	int		SilenceToEmit = 0;
	bool	forceFlush = false;
	bool    forcePartialGroupFlush = false;
	int		nSamplesInBuffer = ( m_bBufferSourceAudio ) ? m_srcAudioBufferCurrentSize / m_AudioBytesPerSample : 0;
	
	#ifdef LOG_ENCODER_AUDIO_OPERATIONS
		LogMsg( "Video duration is %d frames, which is %d Audio Samples\n", m_nFramesAdded, VideoDurationInSamples );
		LogMsg( "%d Samples emitted to Audio track so far.  %d samples remain in audio buffer\n", m_nSamplesAddedToMedia, nSamplesInBuffer );
		LogMsg( "Delta to sync end of audio to end of video is %d Samples\n", CurShortfall );
		LogMsg( "Pad With Silence Mode = %d, Align End of Audio With Video Mode = %d\n", (int) bPadWithSilence, (int) m_bLimitAudioDurationToVideo );
	#endif

	// not grouping samples mode
	if ( m_SampleGrouping == AG_NONE )			
	{
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "No sample grouping Mode\n" );
		#endif
		
		Assert( m_bLimitAudioDurationToVideo == m_bBufferSourceAudio );	// if we're not limiting, we're not buffering
		
		if ( m_bLimitAudioDurationToVideo && CurShortfall > 0 && bPadWithSilence )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Padding with %d samples to match video duration", CurShortfall );
			#endif
			
			SilenceToEmit = CurShortfall;			// pad with silence
		}
		
		if ( nSamplesInBuffer > 0 || SilenceToEmit > 0 )		// force if we have something to add
		{
			forceFlush = true;
		}
	}
	
	// Fixed sized grouping (and buffering)
	if ( m_SampleGrouping == AG_FIXED_SIZE )	
	{ 
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "Fixed sample grouping mode.  Group size of %d\n", m_nAudioSampleGroupSize );
		#endif
		
		// No matter what, if we have a partially filled buffer, we add silence to it to make a complete group
		// do we have a partially full buffer? if so pad with silence to make a full group
		if ( nSamplesInBuffer > 0 && nSamplesInBuffer % m_nAudioSampleGroupSize != 0 )
		{
			SilenceToEmit = m_nAudioSampleGroupSize - ( nSamplesInBuffer % m_nAudioSampleGroupSize );
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Adding %d silence samples to complete group\n", SilenceToEmit );
			#endif
			
		}
	
		int bufferedSamples = nSamplesInBuffer + SilenceToEmit;
		int newShortFall = VideoDurationInSamples - m_nSamplesAddedToMedia - bufferedSamples;
		
		if ( bPadWithSilence && newShortFall > 0 )
		{
			SilenceToEmit += newShortFall;		// pad with silence until audio matches video duration
			forceFlush = true;
			forcePartialGroupFlush = true;
			
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Adding %d silence samples to pad to match audio to video duration\n", newShortFall );
			#endif
		}
	}
	
	if ( m_SampleGrouping == AG_PER_FRAME )
	{
		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "Video Frame duraiton Audio grouping mode\n" );
		#endif

		// Have we already enough audio to match the video
		if ( m_bLimitAudioDurationToVideo && m_AudioSampleFrameCounter >= m_nFramesAdded )
		{
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Audio is caught up to Video\n" );
			#endif		
			goto audio_complete;
		}
		
		// if we have anything in the buffer... pad it out with zeros
		if ( nSamplesInBuffer > 0 )
		{
			// get the group size for the video frame the audio is currently on
			int thisGroupSize = GetAudioSampleCountThruFrame( m_AudioSampleFrameCounter+1 ) - GetAudioSampleCountThruFrame( m_AudioSampleFrameCounter );
			Assert( m_nSamplesAddedToMedia == GetAudioSampleCountThruFrame( m_AudioSampleFrameCounter ) );

			// if we already have 1 (or more) groups in the buffer.. emit them, and restart
			if ( nSamplesInBuffer >= thisGroupSize )
			{
				char  n = nullchar;
				AppendAudioSamplesToMedia( &n, 0 );
				goto restart_sync;
			}
		
			SilenceToEmit = thisGroupSize - nSamplesInBuffer;
			forceFlush = true;
			
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Adding %d silence samples to pad current group to match video frame duration\n", SilenceToEmit );
			#endif
			
		}
			
		// with the output being aligned to a video frame, do we need to add more to pad to end of the video
		
		int bufferedSamples = nSamplesInBuffer + SilenceToEmit;
		int newShortFall = VideoDurationInSamples - m_nSamplesAddedToMedia - bufferedSamples;
		if ( bPadWithSilence && newShortFall > 0 )
		{
			SilenceToEmit += newShortFall;		// pad with silence until audio matches video duration
			forceFlush = true;
			forcePartialGroupFlush = true;
			
			#ifdef LOG_ENCODER_AUDIO_OPERATIONS
				LogMsg( "Adding %d silence samples to pad audio to match video duration", newShortFall );
			#endif
		}
	
	}
		
	#ifdef LOG_ENCODER_AUDIO_OPERATIONS
		LogMsg( "\n" );
	#endif
	
	// now we append any needed silence to the audio stream...
	if ( SilenceToEmit > 0 )
	{
		int bufferSize = SilenceToEmit * m_AudioBytesPerSample;
		byte *pSilenceBuf = new byte[ bufferSize ];
		V_memset( pSilenceBuf, nullchar, bufferSize );

		#ifdef LOG_ENCODER_AUDIO_OPERATIONS
			LogMsg( "Appending %d Silence samples\n", SilenceToEmit );
		#endif
		
		AppendAudioSamplesToMedia( pSilenceBuf, bufferSize );
	}
	else
	{
		if ( forceFlush )
		{
			char  n = nullchar;
			AppendAudioSamplesToMedia( &n, 0 );
		}
	}
		
	if ( forcePartialGroupFlush &&  m_srcAudioBufferCurrentSize >0 )
	{
		int nSamplesThisAdd	 = m_srcAudioBufferCurrentSize / m_AudioBytesPerSample;
		TimeValue64	 insertTime64 = 0;
	
		OSErr status = AddMediaSample2( m_theAudioMedia, (const UInt8 *) m_srcAudioBuffer, (ByteCount) m_srcAudioBufferCurrentSize,
									   (TimeValue64) 1, (TimeValue64) 0, (SampleDescriptionHandle) m_srcSoundDescription,
									   (ItemCount) nSamplesThisAdd, 0, &insertTime64 );             
										
		AssertExitF( status == noErr );
		
		m_srcAudioBufferCurrentSize = 0;
		m_nSamplesAddedToMedia+= nSamplesThisAdd;
		
		#ifdef LOG_ENCODER_OPERATIONS
			LogMsg( "FORCED FLUSH - Audio Samples added to media.  %d added, %d total samples, inserted at time %ld\n", nSamplesThisAdd, m_nSamplesAddedToMedia, insertTime64 );
		#endif
	}


audio_complete:

	return true;
}


bool CQTVideoFileComposer::EndMovieCreation( bool saveMovieData )
{
	#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "\nEndMovieCreation Called Composing=%d  Completed=%d\n\n", (int) m_bComposingMovie, (int) m_bMovieCompleted );
	#endif

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bComposingMovie && !m_bMovieCompleted );

	#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "\nEndMovieCreation Called\n\n" );
	#endif

	// Stop adding to and (optionally) save the video media into the file
	SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	if ( m_nFramesAdded > 0 )
	{
		TimeValue VideoDuration = GetMediaDuration( m_theVideoMedia );
		AssertExitF( VideoDuration == m_nFramesAdded * m_DurationPerFrame );

		OSErr status = EndMediaEdits( m_theVideoMedia );
		AssertExitF( status == noErr );

		if ( saveMovieData )
		{
			status = InsertMediaIntoTrack( m_theVideoTrack, 0, 0, VideoDuration, fixed1 );
			Assert( status == noErr );
			
			#ifdef LOG_ENCODER_OPERATIONS
				LogMsg( "\nVideo Media inserted into Track\n" );
			#endif
			
		}
	}
	
	// Stop adding to and (optionally) save the audio media into the file
	SetResult( VideoResult::AUDIO_ERROR_OCCURED );
	if ( m_bHasAudioTrack && m_nSamplesAdded > 0 )	
	{
		// flush any remaining samples in the buffer to the media
		
	#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "Calling SyncAndFlushAudio()\n" );
	#endif
		
		SyncAndFlushAudio();
	
		TimeValue AudioDuration = GetMediaDuration( m_theAudioMedia );
		#ifdef LOG_ENCODER_OPERATIONS
			LogMsg( "Audio Duration = %d  nSamples Added = %d\n", AudioDuration, m_nSamplesAdded );
		#endif
		// AssertExitF( AudioDuration == m_nSamplesAdded );
		
		OSErr status = EndMediaEdits( m_theAudioMedia );
		AssertExitF( status == noErr );

		if ( saveMovieData )
		{
			status = InsertMediaIntoTrack( m_theAudioTrack, 0, 0, AudioDuration, fixed1 );
			AssertExitF( status == noErr );
			
			#ifdef LOG_ENCODER_OPERATIONS
				LogMsg( "\nAudio Media inserted into Track\n" );
			#endif
		}
	}

	if ( saveMovieData )
	{

#ifdef LOG_ENCODER_OPERATIONS	
		LogMsg( "Saving Movie Data...\n" );
#endif

		SetResult( VideoResult::FILE_ERROR_OCCURED );
		OSErr status = AddMovieToStorage( m_theMovie, m_MovieFileDataHandler );
		AssertExitF( status == noErr );
		if ( status != noErr )
		{
			DataHDeleteFile( m_MovieFileDataHandler );
		}
		
		#ifdef LOG_ENCODER_OPERATIONS
			LogMsg( "\nMovie Resource added to file.   Returned Status = %d\n", (int) status );
		#endif
	}

	// free our resources
	if ( m_MovieFileDataHandler != nullptr )
	{
		OSErr status = CloseMovieStorage( m_MovieFileDataHandler );
		m_MovieFileDataHandler = nullptr;
		AssertExitF( status == noErr );
	}
	
	SAFE_DISPOSE_HANDLE( m_MovieFileDataRef );
	SAFE_DISPOSE_HANDLE( m_SrcImageCompressedBuffer );
	SAFE_DISPOSE_GWORLD( m_theSrcGWorld );
	SAFE_DISPOSE_HANDLE( m_srcSoundDescription );

	SetResult( VideoResult::SUCCESS );
	m_bComposingMovie = false;	
	return true;
}


// The movie can be aborted at any time before completion
bool CQTVideoFileComposer::AbortMovie()
{
	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( !m_bMovieCompleted );

	// Shut down the movie if we are recording	
	if ( m_bComposingMovie )
	{
		if ( !EndMovieCreation( false ) )
		{
			return false;
		}
	}

	if ( m_bMovieCreated )
	{
		if (  m_MovieFileDataHandler != nullptr )
		{
			SetResult( VideoResult::FILE_ERROR_OCCURED );
			OSErr status = CloseMovieStorage( m_MovieFileDataHandler );
            AssertExitF( status == noErr );
			m_MovieFileDataHandler = nullptr;
		}
	
		SAFE_DISPOSE_HANDLE( m_MovieFileDataRef );
		SAFE_DISPOSE_MOVIE( m_theMovie );
	}

	if ( m_FileName )
	{
		g_pFullFileSystem->RemoveFile( m_FileName );
	}

	SetResult( VideoResult::SUCCESS );
	m_bMovieCompleted = true;
	
	return true;
}



bool CQTVideoFileComposer::FinishMovie( bool SaveMovieToDisk ) 
{
	#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "\nFinish Movie Called\n" );
	#endif

	SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
	AssertExitF( m_bComposingMovie && !m_bMovieCompleted );

	// Shutdown movie creation
	if ( !EndMovieCreation( SaveMovieToDisk )  )
	{
	#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "\nEndMovieCreation Aborted\n" );
	#endif
		return false;
	}

	// todo: check on Disposing of theMovie and theMedia
	if (  m_MovieFileDataHandler != nullptr )
	{
		SetResult( VideoResult::FILE_ERROR_OCCURED );
		OSErr status = CloseMovieStorage( m_MovieFileDataHandler );
		AssertExitF( status == noErr );
		m_MovieFileDataHandler = nullptr;
		
#ifdef LOG_ENCODER_OPERATIONS
		LogMsg( "Movie File Closed\n" );
#endif
		
	}
	
	SAFE_DISPOSE_HANDLE( m_MovieFileDataRef );
	SAFE_DISPOSE_MOVIE( m_theMovie );
	
	// if no frames have been added.. delete files
	if ( SaveMovieToDisk == false || ( m_nFramesAdded <= 0 && m_nSamplesAdded <= 0) )
	{
		g_pFullFileSystem->RemoveFile( m_FileName );
	}
	
	SetResult( VideoResult::SUCCESS );
	m_bMovieCompleted = true;
	
#ifdef LOG_ENCODER_OPERATIONS
	g_pFullFileSystem->Close( m_LogFile );
	m_LogFile = FILESYSTEM_INVALID_HANDLE;
#endif
	
	
	return true;
}



void CQTVideoFileComposer::SetResult( VideoResult_t status )
{
	m_LastResult = status;
}

VideoResult_t CQTVideoFileComposer::GetResult()
{
	return m_LastResult;
}

bool CQTVideoFileComposer::IsReadyToRecord()
{
	return ( m_bComposingMovie && !m_bMovieCompleted );
}


#ifdef ENABLE_EXTERNAL_ENCODER_LOGGING
bool CQTVideoFileComposer::LogMessage(  const char *msg )
{

#ifdef LOG_ENCODER_OPERATIONS
	if ( IS_NOT_EMPTY(msg) && m_LogFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write( msg, V_strlen( msg ), m_LogFile );
	}
#endif
	return true;
}
#endif
