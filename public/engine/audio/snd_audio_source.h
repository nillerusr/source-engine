//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_AUDIO_SOURCE_H
#define SND_AUDIO_SOURCE_H
#pragma once

#if !defined( _X360 )
#define MP3_SUPPORT	1
#endif

#define AUDIOSOURCE_COPYBUF_SIZE	4096

struct channel_t;
class CSentence;
class CSfxTable;

class CAudioSource;
class IAudioDevice;
class CUtlBuffer;

#include "tier0/vprof.h"

//-----------------------------------------------------------------------------
// Purpose: This is an instance of an audio source.
//			Mixers are attached to channels and reference an audio source.
//			Mixers are specific to the sample format and source format.
//			Mixers are never re-used, so they can track instance data like
//			sample position, fractional sample, stream cache, faders, etc.
//-----------------------------------------------------------------------------
abstract_class CAudioMixer
{
public:
	virtual ~CAudioMixer( void ) {}

	// return number of samples mixed
	virtual int MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset ) = 0;
	virtual int SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset ) = 0;
	virtual bool ShouldContinueMixing( void ) = 0;

	virtual CAudioSource *GetSource( void ) = 0;
	
	// get the current position (next sample to be mixed)
	virtual int GetSamplePosition( void ) = 0;

	// Allow the mixer to modulate pitch and volume. 
	// returns a floating point modulator
	virtual float ModifyPitch( float pitch ) = 0;
	virtual float GetVolumeScale( void ) = 0;

	// NOTE: Playback is optimized for linear streaming.  These calls will usually cost performance
	// It is currently optimal to call them before any playback starts, but some audio sources may not
	// guarantee this.  Also, some mixers may choose to ignore these calls for internal reasons (none do currently).

	// Move the current position to newPosition 
	// BUGBUG: THIS CALL DOES NOT SUPPORT MOVING BACKWARD, ONLY FORWARD!!!
	virtual void SetSampleStart( int newPosition ) = 0;

	// End playback at newEndPosition
	virtual void SetSampleEnd( int newEndPosition ) = 0;

	// How many samples to skip before commencing actual data reading ( to allow sub-frametime sound
	//  offsets and avoid synchronizing sounds to various 100 msec clock intervals throughout the
	//  engine and game code)
	virtual void SetStartupDelaySamples( int delaySamples ) = 0;
	virtual int GetMixSampleSize() = 0;

	// Certain async loaded sounds lazilly load into memory in the background, use this to determine
	//  if the sound is ready for mixing
	virtual bool IsReadyToMix() = 0;
	
	// NOTE: The "saved" position can be different than the "sample" position
	// NOTE: Allows mixer to save file offsets, loop info, etc
	virtual int GetPositionForSave() = 0;
	virtual void SetPositionFromSaved( int savedPosition ) = 0;
};

inline int CalcSampleSize( int bitsPerSample, int _channels ) 
{
	return (bitsPerSample >> 3) * _channels;
}

#include "UtlCachedFileData.h"

class CSentence;
class CSfxTable;
class CAudioSourceCachedInfo : public IBaseCacheInfo
{
public:
	CAudioSourceCachedInfo();
	CAudioSourceCachedInfo( const CAudioSourceCachedInfo& src );

	virtual ~CAudioSourceCachedInfo();

	CAudioSourceCachedInfo& operator =( const CAudioSourceCachedInfo& src );

	void	Clear();
	void	RemoveData();

	virtual void Save( CUtlBuffer& buf );
	virtual void Restore( CUtlBuffer& buf );
	virtual void Rebuild( char const *filename );

	// A hack, but will work okay
	static int s_CurrentType;
	static CSfxTable *s_pSfx;
	static bool s_bIsPrecacheSound;

	inline int		Type() const
	{
		return info.m_Type;
	}
	void	SetType( int type )
	{
		info.m_Type = type;
	}

	inline int		Bits() const
	{
		return info.m_bits;
	}
	void	SetBits( int bits )
	{
		info.m_bits = bits;
	}

	inline int		Channels() const
	{
		return info.m_channels;
	}
	void	SetChannels( int _channels )
	{
		info.m_channels = _channels;
	}

	inline int		SampleSize() const
	{
		return info.m_sampleSize;
	}
	void	SetSampleSize( int size )
	{
		info.m_sampleSize = size;
	}

	inline int		Format() const
	{
		return info.m_format;
	}
	void	SetFormat( int format )
	{
		info.m_format = format;
	}

	inline int		SampleRate() const
	{
		return info.m_rate;
	}
	void	SetSampleRate( int rate )
	{
		info.m_rate = rate;
	}

	inline int		CachedDataSize() const
	{
		return (int)m_usCachedDataSize;
	}

	void	SetCachedDataSize( int size )
	{
		m_usCachedDataSize = (unsigned short)size;
	}

	inline const byte	*CachedData() const
	{
		return m_pCachedData;
	}

	void	SetCachedData( const byte *data )
	{
		m_pCachedData = ( byte * )data;
		flags.m_bCachedData = ( data != NULL ) ? true : false;
	}

	inline int		HeaderSize() const
	{
		return (int)m_usHeaderSize;
	}

	void	SetHeaderSize( int size )
	{
		m_usHeaderSize = (unsigned short)size;
	}

	inline const byte	*HeaderData() const
	{
		return m_pHeader;
	}

	void	SetHeaderData( const byte *data )
	{
		m_pHeader = ( byte * )data;
		flags.m_bHeader = ( data != NULL ) ? true : false;
	}

	inline int		LoopStart() const
	{
		return m_loopStart;
	}
	void	SetLoopStart( int start )
	{
		m_loopStart = start;
	}

	inline int		SampleCount() const
	{
		return m_sampleCount;
	}

	void	SetSampleCount( int count )
	{
		m_sampleCount = count;
	}
	inline int		DataStart() const
	{
		return m_dataStart;
	}
	void	SetDataStart( int start )
	{
		m_dataStart = start;
	}
	inline int		DataSize() const
	{
		return m_dataSize;
	}
	void	SetDataSize( int size )
	{
		m_dataSize = size;
	}
	inline CSentence	*Sentence() const
	{
		return m_pSentence;
	}
	void	SetSentence( CSentence *sentence )
	{
		m_pSentence = sentence;
		flags.m_bSentence = ( sentence != NULL ) ? true : false;
	}

private:

	union
	{
		unsigned int infolong;
		struct
		{
			unsigned int				m_Type : 2;  // 0 1 2 or 3
			unsigned int				m_bits : 5;  // 0 to 31
			unsigned int				m_channels : 2; // 1 or 2
			unsigned int				m_sampleSize : 3; // 1 2 or 4
			unsigned int				m_format : 2; // 1 == PCM, 2 == ADPCM
			unsigned int				m_rate : 17; // 0 to 64 K
		} info;
	};

	union
	{
		byte	flagsbyte;
		struct
		{
			bool			m_bSentence : 1;
			bool			m_bCachedData : 1;
			bool			m_bHeader : 1;
		} flags;
	};

	int				m_loopStart;
	int				m_sampleCount;
	int				m_dataStart;	// offset of wave data chunk
	int				m_dataSize;		// size of wave data chunk

	unsigned short	m_usCachedDataSize;
	unsigned short	m_usHeaderSize;

	CSentence		*m_pSentence;
	byte			*m_pCachedData;
	byte			*m_pHeader;
};

class IAudioSourceCache
{
public:
	virtual bool Init( unsigned int memSize ) = 0;
	virtual void Shutdown() = 0;
	virtual void LevelInit( char const *mapname ) = 0;
	virtual void LevelShutdown() = 0;

	// This invalidates the cached size/date info for sounds so it'll regenerate that next time it's accessed.
	// Used when you connect to a pure server.
	virtual void ForceRecheckDiskInfo() = 0;

	virtual CAudioSourceCachedInfo	*GetInfo( int audiosourcetype, bool soundisprecached, CSfxTable *sfx ) = 0;
	virtual void RebuildCacheEntry( int audiosourcetype, bool soundisprecached, CSfxTable *sfx ) = 0;
};

extern IAudioSourceCache *audiosourcecache;

FORWARD_DECLARE_HANDLE( memhandle_t );

typedef int StreamHandle_t;
enum
{
	INVALID_STREAM_HANDLE = (StreamHandle_t)~0 
};

typedef int BufferHandle_t;
enum
{
	INVALID_BUFFER_HANDLE = (BufferHandle_t)~0
};

typedef unsigned int streamFlags_t;
enum
{
	STREAMED_FROMDVD    = 0x00000001,		// stream buffers are compliant to dvd sectors
	STREAMED_SINGLEPLAY = 0x00000002,		// non recurring data, buffers don't need to persist and can be recycled
	STREAMED_QUEUEDLOAD = 0x00000004,		// hint the streamer to load using the queued loader system
};

abstract_class IAsyncWavDataCache
{
public:
	virtual bool			Init( unsigned int memSize ) = 0;
	virtual void			Shutdown() = 0;

	// implementation that treats file as monolithic
	virtual memhandle_t		AsyncLoadCache( char const *filename, int datasize, int startpos, bool bIsPrefetch = false ) = 0;
	virtual void			PrefetchCache( char const *filename, int datasize, int startpos ) = 0;
	virtual bool			CopyDataIntoMemory( char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed ) = 0;
	virtual bool			CopyDataIntoMemory( memhandle_t& handle, char const *filename, int datasize, int startpos, void *buffer, int bufsize, int copystartpos, int bytestocopy, bool *pbPostProcessed ) = 0;
	virtual bool			IsDataLoadCompleted( memhandle_t handle, bool *pIsValid ) = 0;
	virtual void			RestartDataLoad( memhandle_t *pHandle, const char *pFilename, int dataSize, int startpos ) = 0;
	virtual bool			GetDataPointer( memhandle_t& handle, char const *filename, int datasize, int startpos, void **pData, int copystartpos, bool *pbPostProcessed ) = 0;
	virtual void			SetPostProcessed( memhandle_t handle, bool proc ) = 0;
	virtual void			Unload( memhandle_t handle ) = 0;

	// alternate multi-buffer streaming implementation
	virtual StreamHandle_t	OpenStreamedLoad( char const *pFileName, int dataSize, int dataStart, int startPos, int loopPos, int bufferSize, int numBuffers, streamFlags_t flags ) = 0;
	virtual void			CloseStreamedLoad( StreamHandle_t hStream ) = 0;
	virtual int				CopyStreamedDataIntoMemory( StreamHandle_t hStream, void *pBuffer, int buffSize, int copyStartPos, int bytesToCopy ) = 0;
	virtual bool			IsStreamedDataReady( StreamHandle_t hStream ) = 0;
	virtual void			MarkBufferDiscarded( BufferHandle_t hBuffer ) = 0;
	virtual void			*GetStreamedDataPointer( StreamHandle_t hStream, bool bSync ) = 0;
	virtual bool			IsDataLoadInProgress( memhandle_t handle ) = 0;
	virtual void			Flush() = 0;
	virtual void			OnMixBegin() = 0;
	virtual void			OnMixEnd() = 0;
};

extern IAsyncWavDataCache *wavedatacache;

struct CAudioSourceCachedInfoHandle_t
{
	CAudioSourceCachedInfoHandle_t() :
		info( NULL ),
		m_FlushCount( 0 )
	{
	}

	CAudioSourceCachedInfo	*info;
	unsigned int			m_FlushCount;

	inline CAudioSourceCachedInfo *Get( int audiosourcetype, bool soundisprecached, CSfxTable *sfx, int *pcacheddatasize )
	{
		VPROF("CAudioSourceCachedInfoHandle_t::Get");

		if ( m_FlushCount != s_nCurrentFlushCount )
		{
			// Reacquire
			info = audiosourcecache->GetInfo( audiosourcetype, soundisprecached, sfx );

			if ( pcacheddatasize )
			{
				*pcacheddatasize = info ? info->CachedDataSize() : 0;
			}

			// Tag as current
			m_FlushCount = s_nCurrentFlushCount;
		}
		return info;
	}

	inline bool IsValid()
	{
		return !!( m_FlushCount == s_nCurrentFlushCount );
	}

	inline CAudioSourceCachedInfo *FastGet()
	{
		VPROF("CAudioSourceCachedInfoHandle_t::FastGet");

		if ( m_FlushCount != s_nCurrentFlushCount )
		{
			return NULL;
		}
		return info;
	}

	static void				InvalidateCache();
	static unsigned int		s_nCurrentFlushCount;
};


//-----------------------------------------------------------------------------
// Purpose: A source is an abstraction for a stream, cached file, or procedural
//			source of audio.
//-----------------------------------------------------------------------------
abstract_class CAudioSource
{
public:
	enum
	{
		AUDIO_SOURCE_UNK = 0,
		AUDIO_SOURCE_WAV,
		AUDIO_SOURCE_MP3,
		AUDIO_SOURCE_VOICE,

		AUDIO_SOURCE_MAXTYPE,
	};

	enum
	{
		AUDIO_NOT_LOADED = 0,
		AUDIO_IS_LOADED = 1,
		AUDIO_LOADING = 2,
	};

	virtual ~CAudioSource( void ) {}

	// Create an instance (mixer) of this audio source
	virtual CAudioMixer			*CreateMixer( int initialStreamPosition = 0 ) = 0;

	// Serialization for caching
	virtual int					GetType( void ) = 0;
	virtual void				GetCacheData( CAudioSourceCachedInfo *info ) = 0;

	// Provide samples for the mixer. You can point pData at your own data, or if you prefer to copy the data,
	// you can copy it into copyBuf and set pData to copyBuf.
	virtual int					GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) = 0;
	
	virtual int					SampleRate( void ) = 0;

	// Returns true if the source is a voice source.
	// This affects the voice_overdrive behavior (all sounds get quieter when
	// someone is speaking).
	virtual bool				IsVoiceSource() = 0;
	
	// Sample size is in bytes.  It will not be accurate for compressed audio.  This is a best estimate.
	// The compressed audio mixers understand this, but in general do not assume that SampleSize() * SampleCount() = filesize
	// or even that SampleSize() is 100% accurate due to compression.
	virtual int					SampleSize( void ) = 0;

	// Total number of samples in this source.  NOTE: Some sources are infinite (mic input), they should return
	// a count equal to one second of audio at their current rate.
	virtual int					SampleCount( void ) = 0;

	virtual int					Format( void ) = 0;
	virtual int					DataSize( void ) = 0;

	virtual bool				IsLooped( void ) = 0;
	virtual bool				IsStereoWav( void ) = 0;
	virtual bool				IsStreaming( void ) = 0;
	virtual int					GetCacheStatus( void ) = 0;
	int 						IsCached( void ) { return GetCacheStatus() == AUDIO_IS_LOADED ? true : false; }
	virtual void				CacheLoad( void ) = 0;
	virtual void				CacheUnload( void ) = 0;
	virtual CSentence			*GetSentence( void ) = 0;

	// these are used to find good splice/loop points.
	// If not implementing these, simply return sample
	virtual int					ZeroCrossingBefore( int sample ) = 0;
	virtual int					ZeroCrossingAfter( int sample ) = 0;
	
	// mixer's references
	virtual void				ReferenceAdd( CAudioMixer *pMixer ) = 0;
	virtual void				ReferenceRemove( CAudioMixer *pMixer ) = 0;

	// check reference count, return true if nothing is referencing this
	virtual bool				CanDelete( void ) = 0;

	virtual void				Prefetch() = 0;

	virtual bool				IsAsyncLoad() = 0;

	// Make sure our data is rebuilt into the per-level cache
	virtual void				CheckAudioSourceCache() = 0;

	virtual char const			*GetFileName() = 0;

	virtual void				SetPlayOnce( bool ) = 0;
	virtual bool				IsPlayOnce() = 0;

	// Used to identify a word that is part of a sentence mixing operation
	virtual void				SetSentenceWord( bool bIsWord ) = 0;
	virtual bool				IsSentenceWord() = 0;

	virtual int					SampleToStreamPosition( int samplePosition ) = 0;
	virtual int					StreamToSamplePosition( int streamPosition ) = 0;
};

// Fast method for determining duration of .wav/.mp3, exposed to server as well
extern float AudioSource_GetSoundDuration( char const *pName );

// uses wave file cached in memory already
extern float AudioSource_GetSoundDuration( CSfxTable *pSfx );

#endif // SND_AUDIO_SOURCE_H
