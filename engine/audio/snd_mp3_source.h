//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_MP3_SOURCE_H
#define SND_MP3_SOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "snd_audio_source.h"
#include "snd_wave_data.h"
#include "snd_sfx.h"

class IWaveData;
class CAudioMixer;

abstract_class CAudioSourceMP3 : public CAudioSource
{
public:

	CAudioSourceMP3( CSfxTable *pSfx );
	CAudioSourceMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	virtual ~CAudioSourceMP3();

	// Create an instance (mixer) of this audio source
	virtual CAudioMixer			*CreateMixer( int initialStreamPosition = 0 ) = 0;
	
	virtual int					GetType( void );
	virtual void				GetCacheData( CAudioSourceCachedInfo *info );

	// Provide samples for the mixer. You can point pData at your own data, or if you prefer to copy the data,
	// you can copy it into copyBuf and set pData to copyBuf.
	virtual int					GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) = 0;

	virtual int					SampleRate( void );

	// Returns true if the source is a voice source.
	// This affects the voice_overdrive behavior (all sounds get quieter when
	// someone is speaking).
	virtual bool				IsVoiceSource() { return false; }
	virtual int					SampleSize( void ) { return 1; }

	// Total number of samples in this source.  NOTE: Some sources are infinite (mic input), they should return
	// a count equal to one second of audio at their current rate.
	virtual int					SampleCount( void ) { return m_dataSize; }
	
	virtual int					Format() { return 0; }
	virtual int					DataSize( void ) { return 0; }
	
	virtual bool				IsLooped( void ) { return false; }
	virtual bool				IsStereoWav( void ) { return false; }
	virtual bool				IsStreaming( void ) { return false; } 
	virtual int					GetCacheStatus( void ) { return AUDIO_IS_LOADED; }
	virtual void				CacheLoad( void ) {}
	virtual void				CacheUnload( void ) {}
	virtual CSentence			*GetSentence( void ) { return NULL; }

	virtual int					ZeroCrossingBefore( int sample ) { return sample; }
	virtual int					ZeroCrossingAfter( int sample ) { return sample; }
	
	// mixer's references
	virtual void				ReferenceAdd( CAudioMixer *pMixer );
	virtual void				ReferenceRemove( CAudioMixer *pMixer );
	// check reference count, return true if nothing is referencing this
	virtual bool				CanDelete( void );

	virtual bool				IsAsyncLoad();

	virtual void				CheckAudioSourceCache();

	virtual char const			*GetFileName();

	virtual void				SetPlayOnce( bool isPlayOnce ) { m_bIsPlayOnce = isPlayOnce; }
	virtual bool				IsPlayOnce() { return m_bIsPlayOnce; }

	virtual void				SetSentenceWord( bool bIsWord ) { m_bIsSentenceWord = bIsWord; }
	virtual bool				IsSentenceWord() { return m_bIsSentenceWord; }

	virtual int					SampleToStreamPosition( int samplePosition ) { return 0; }
	virtual int					StreamToSamplePosition( int streamPosition ) { return 0; }

	virtual void				SetSentence( CSentence *pSentence );

protected:

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		VPROF("CAudioSourceMP3::GetCachedDataPointer");

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_MP3, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			Assert( !"CAudioSourceMP3::GetCachedDataPointer info == NULL" );
			return NULL;
		}

		return (byte *)info->CachedData();
	}

	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;
	int				m_nCachedDataSize;

protected:
	CSfxTable		*m_pSfx;
	int				m_sampleRate;
	int				m_dataSize;
	int				m_dataStart;
	int				m_refCount;
	bool			m_bIsPlayOnce : 1;
	bool			m_bIsSentenceWord : 1;
	bool			m_bCheckedForPendingSentence;
};

//-----------------------------------------------------------------------------
// Purpose: Streaming MP3 file
//-----------------------------------------------------------------------------
class CAudioSourceStreamMP3 : public CAudioSourceMP3, public IWaveStreamSource
{
public:
	CAudioSourceStreamMP3( CSfxTable *pSfx );
	CAudioSourceStreamMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceStreamMP3() {}

	bool			IsStreaming( void ) OVERRIDE { return true; }
	bool			IsStereoWav( void ) OVERRIDE { return false; }
	CAudioMixer		*CreateMixer( int initialStreamPosition = 0 ) OVERRIDE;
	int				GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) OVERRIDE;

	// IWaveStreamSource
	int UpdateLoopingSamplePosition( int samplePosition ) OVERRIDE
	{
		return samplePosition;
	}
	void UpdateSamples( char *pData, int sampleCount ) OVERRIDE {}

	int	GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples ) OVERRIDE
	{
		return 0;
	}

	void Prefetch() OVERRIDE;

private:
	CAudioSourceStreamMP3( const CAudioSourceStreamMP3 & ); // not implemented, not accessible
};

class CAudioSourceMP3Cache : public CAudioSourceMP3
{
public:
	CAudioSourceMP3Cache( CSfxTable *pSfx );
	CAudioSourceMP3Cache( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceMP3Cache( void );

	int						GetCacheStatus( void ) OVERRIDE;
	void					CacheLoad( void ) OVERRIDE;
	void					CacheUnload( void ) OVERRIDE;
	// NOTE: "samples" are bytes for MP3
	int						GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) OVERRIDE;
	CAudioMixer				*CreateMixer( int initialStreamPosition = 0 ) OVERRIDE;
	CSentence				*GetSentence( void ) OVERRIDE;

	void					Prefetch() OVERRIDE {}

protected:
	virtual char			*GetDataPointer( void );
	memhandle_t				m_hCache;

private:
	CAudioSourceMP3Cache( const CAudioSourceMP3Cache & );

	unsigned int	m_bNoSentence : 1;
};

bool Audio_IsMP3( const char *pName );
CAudioSource *Audio_CreateStreamedMP3( CSfxTable *pSfx );
CAudioSource *Audio_CreateMemoryMP3( CSfxTable *pSfx );

#endif // SND_MP3_SOURCE_H
