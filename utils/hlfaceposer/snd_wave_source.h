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
#include "sentence.h"

class IterateRIFF;

class CAudioSourceWave : public CAudioSource
{
public:
	CAudioSourceWave( void );
	~CAudioSourceWave( void );

	void Setup( const char *pFormat, int formatSize, IterateRIFF &walk );

	virtual int				SampleRate( void ) { return m_rate; }
	inline int				SampleSize( void ) { return m_sampleSize; }
	virtual float			TrueSampleSize( void );

	void					*GetHeader( void );

	// Legacy
	virtual	void			ParseChunk( IterateRIFF &walk, int chunkName );

	virtual void			ParseSentence( IterateRIFF &walk );
	
	void					ConvertSamples( char *pData, int sampleCount );
	bool					IsLooped( void ) { return (m_loopStart >= 0) ? true : false; }
	bool					IsStreaming( void ) { return false; }
	bool					IsStereoWav( void );
	int						ConvertLoopedPosition( int samplePosition );

	int						SampleCount( void );

	virtual float			GetRunningLength( void )
							{ 
								if ( m_rate > 0.0 ) 
								{
									return (float)SampleCount() / m_rate;
								}
								return 0.0f; }
		
	CSentence				*GetSentence( void );

protected:
	// returns the loop start from a cue chunk
	int						ParseCueChunk( IterateRIFF &walk );
	void					Init( const char *pHeaderBuffer, int headerSize );

	int				m_bits;
	int				m_rate;
	int				m_channels;
	int				m_format;
	int				m_sampleSize;
	int				m_loopStart;
	int				m_sampleCount;

private:
	char			*m_pHeader;
	CSentence		m_Sentence;
};

#endif // SND_WAVE_SOURCE_H
