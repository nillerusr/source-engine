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

#ifndef SND_WAVE_SOURCE_H
#define SND_WAVE_SOURCE_H
#pragma once

#include "snd_audio_source.h"
class IterateRIFF;
#include "sentence.h"
#include "snd_sfx.h"

//=============================================================================
// Functions to create audio sources from wave files or from wave data.
//=============================================================================
extern CAudioSource* Audio_CreateMemoryWave( CSfxTable *pSfx );
extern CAudioSource* Audio_CreateStreamedWave( CSfxTable *pSfx );

class CAudioSourceWave : public CAudioSource
{
public:
	CAudioSourceWave( CSfxTable *pSfx );
	CAudioSourceWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	~CAudioSourceWave( void );

	virtual int				GetType( void );
	virtual void			GetCacheData( CAudioSourceCachedInfo *info );

	void					Setup( const char *pFormat, int formatSize, IterateRIFF &walk );
	virtual int				SampleRate( void );
	virtual int				SampleSize( void );
	virtual int				SampleCount( void );

	virtual int				Format( void );
	virtual int				DataSize( void );

	void					*GetHeader( void );
	virtual bool			IsVoiceSource();

	virtual	void			ParseChunk( IterateRIFF &walk, int chunkName );
	virtual void			ParseSentence( IterateRIFF &walk );

	void					ConvertSamples( char *pData, int sampleCount );
	bool					IsLooped( void );
	bool					IsStereoWav( void );
	bool					IsStreaming( void );
	int						GetCacheStatus( void );
	int						ConvertLoopedPosition( int samplePosition );
	void					CacheLoad( void );
	void					CacheUnload( void );
	virtual int				ZeroCrossingBefore( int sample );
	virtual int				ZeroCrossingAfter( int sample );
	virtual void			ReferenceAdd( CAudioMixer *pMixer );
	virtual void			ReferenceRemove( CAudioMixer *pMixer );
	virtual bool			CanDelete( void );
	virtual CSentence		*GetSentence( void );
	const char				*GetName();

	virtual bool			IsAsyncLoad();

	virtual void			CheckAudioSourceCache();

	virtual char const		*GetFileName();

	// 360 uses alternate play once semantics
	virtual void			SetPlayOnce( bool bIsPlayOnce ) { m_bIsPlayOnce = IsPC() ? bIsPlayOnce : false; }
	virtual bool			IsPlayOnce() { return IsPC() ? m_bIsPlayOnce : false; }

	virtual void			SetSentenceWord( bool bIsWord ) { m_bIsSentenceWord = bIsWord; }
	virtual bool			IsSentenceWord() { return m_bIsSentenceWord; }

	int						GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples );

	virtual int				SampleToStreamPosition( int samplePosition ) { return 0; }
	virtual int				StreamToSamplePosition( int streamPosition ) { return 0; }

protected:
	void					ParseCueChunk( IterateRIFF &walk );
	void					ParseSamplerChunk( IterateRIFF &walk );

	void					Init( const char *pHeaderBuffer, int headerSize );
	bool					GetStartupData( void *dest, int destsize, int& bytesCopied );
	bool					GetXboxAudioStartupData();

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		VPROF("CAudioSourceWave::GetCachedDataPointer");

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			Assert( !"CAudioSourceWave::GetCachedDataPointer info == NULL" );
			return NULL;
		}

		return (byte *)info->CachedData();
	}

	int				m_bits;
	int				m_rate;
	int				m_channels;
	int				m_format;
	int				m_sampleSize;
	int				m_loopStart;
	int				m_sampleCount;			// can be "samples" or "bytes", depends on format

	CSfxTable		*m_pSfx;
	CSentence		*m_pTempSentence;

	int				m_dataStart;			// offset of sample data
	int				m_dataSize;				// size of sample data

	char			*m_pHeader;
	int				m_nHeaderSize;

	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;

	int				m_nCachedDataSize;

	// number of actual samples (regardless of format)
	// compressed formats alter definition of m_sampleCount 
	// used to spare expensive calcs by decoders
	int				m_numDecodedSamples;

	// additional data needed by xma decoder to for looping
	unsigned short	m_loopBlock;			// the block the loop occurs in
	unsigned short	m_numLeadingSamples;	// number of leader samples in the loop block to discard
	unsigned short	m_numTrailingSamples;	// number of trailing samples in the final block to discard
	unsigned short	unused;

	unsigned int	m_bNoSentence : 1;
	unsigned int	m_bIsPlayOnce : 1;
	unsigned int	m_bIsSentenceWord : 1;

private:
	CAudioSourceWave( const CAudioSourceWave & ); // not implemented, not allowed
	int				m_refCount;
	
#ifdef _DEBUG
	// Only set in debug mode so you can see the name.
	const char		*m_pDebugName;
#endif
};


#endif // SND_WAVE_SOURCE_H
