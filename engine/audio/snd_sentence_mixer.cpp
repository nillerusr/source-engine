//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sentence Mixing
//
//=============================================================================//

#include "audio_pch.h"
#include "vox_private.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: This replaces the old sentence logic that was integrated with the
//			sound code.  Now it is a hierarchical mixer.
//-----------------------------------------------------------------------------
class CSentenceMixer : public CAudioMixer
{
public:
	CSentenceMixer( voxword_t *pWords );
	~CSentenceMixer( void );

	// return number of samples mixed
	virtual int			MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	virtual int			SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	virtual bool		ShouldContinueMixing( void );

	virtual CAudioSource*	GetSource( void );
	
	// get the current position (next sample to be mixed)
	virtual int			GetSamplePosition( void );
	virtual float		ModifyPitch( float pitch );
	virtual float		GetVolumeScale( void );

	// BUGBUG: These are only applied to the current word, not the whole sentence!!!!
	virtual void		SetSampleStart( int newPosition );
	virtual void		SetSampleEnd( int newEndPosition );

	virtual void		SetStartupDelaySamples( int delaySamples );
	virtual int			GetMixSampleSize() { return m_pCurrentWordMixer ? m_pCurrentWordMixer->GetMixSampleSize() : 0; }

	virtual bool		IsReadyToMix();

	virtual int			GetPositionForSave() { return GetSamplePosition(); }
	virtual void		SetPositionFromSaved( int savedPosition ) { SetSampleStart( savedPosition ); }

private:
	CAudioMixer			*LoadWord( int nWordIndex );
	void				FreeWord( int nWordIndex );

	// identifies the active word
	int					m_currentWordIndex;
	CAudioMixer			*m_pCurrentWordMixer;

	// set when a transition to a new word occurs
	bool				m_bNewWord;

	voxword_t			m_VoxWords[CVOXWORDMAX];
	CAudioMixer			*m_pWordMixers[CVOXWORDMAX];
	int					m_nNumWords;
};

CAudioMixer *CreateSentenceMixer( voxword_t *pWords )
{
	if ( pWords )
	{
		return new CSentenceMixer( pWords );
	}

	return NULL;
}

CSentenceMixer::CSentenceMixer( voxword_t *pWords )
{
	// count the expected number of words
	m_nNumWords = 0;
	while ( pWords[m_nNumWords].sfx != NULL )
	{
		// get a private copy of the words
		m_VoxWords[m_nNumWords] = pWords[m_nNumWords];
		m_nNumWords++;
		if ( m_nNumWords >= ARRAYSIZE( m_VoxWords ) )
		{
			// very long sentence, prevent overflow
			break;
		}
	}	

	// startup all the mixers now, this serves as a hint to the audio streamer
	// actual mixing will commence when they are ALL ready
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		// it is possible to get a null mixer (due to wav error, etc)
		// the sentence will skip these words
		m_pWordMixers[nWord] = LoadWord( nWord );
	}
	Assert( m_nNumWords < ARRAYSIZE( m_pWordMixers ) );

	// find first valid word mixer
	m_currentWordIndex = 0;
	m_pCurrentWordMixer = NULL;
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		if ( m_pWordMixers[nWord] )
		{
			m_currentWordIndex = nWord;
			m_pCurrentWordMixer = m_pWordMixers[nWord];
			break;
		}
	}

	m_bNewWord = ( m_pCurrentWordMixer != NULL );	
}

CSentenceMixer::~CSentenceMixer( void )
{
	// free all words
	for ( int nWord = 0; nWord < m_nNumWords; nWord++ )
	{
		FreeWord( nWord );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if mixing can commence, false otherwise
//-----------------------------------------------------------------------------
bool CSentenceMixer::IsReadyToMix()
{
	if ( !m_pCurrentWordMixer )
	{
		// no word, but mixing has to commence in order to shutdown
		return true;
	}

	// all the words should be available before mixing the sentence
	for ( int nWord = m_currentWordIndex; nWord < m_nNumWords; nWord++ )
	{
		if ( m_pWordMixers[nWord] && !m_pWordMixers[nWord]->IsReadyToMix() )
		{
			// Still waiting for async data to arrive
			return false;
		}
	}

	if ( m_bNewWord )
	{
		m_bNewWord = false;

		int start = m_VoxWords[m_currentWordIndex].start;
		int end = m_VoxWords[m_currentWordIndex].end;
		
		// don't allow overlapped ranges
		if ( end <= start )
		{
			end = 0;
		}

		if ( start || end )
		{
			int sampleCount = m_pCurrentWordMixer->GetSource()->SampleCount();
			if ( start > 0 && start < 100 )
			{
				m_pCurrentWordMixer->SetSampleStart( (int)(sampleCount * 0.01f * start) );
			}
			if ( end > 0 && end < 100 )
			{
				m_pCurrentWordMixer->SetSampleEnd( (int)(sampleCount * 0.01f * end) );
			}
		}
	}

	return true;
}

bool CSentenceMixer::ShouldContinueMixing( void )
{
	if ( m_pCurrentWordMixer )
	{
		// keep mixing until the words run out
		return true;
	}

	return false;
}

CAudioSource *CSentenceMixer::GetSource( void )
{
	if ( m_pCurrentWordMixer )
	{
		return m_pCurrentWordMixer->GetSource();
	}

	return NULL;
}
	
// get the current position (next sample to be mixed)
int CSentenceMixer::GetSamplePosition( void )
{
	if ( m_pCurrentWordMixer )
	{
		return m_pCurrentWordMixer->GetSamplePosition();
	}

	return 0;
}

void CSentenceMixer::SetSampleStart( int newPosition )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetSampleStart( newPosition );
	}
}

// End playback at newEndPosition
void CSentenceMixer::SetSampleEnd( int newEndPosition )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetSampleEnd( newEndPosition );
	}
}

void CSentenceMixer::SetStartupDelaySamples( int delaySamples )
{
	if ( m_pCurrentWordMixer )
	{
		m_pCurrentWordMixer->SetStartupDelaySamples( delaySamples );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Free a word
//-----------------------------------------------------------------------------
void CSentenceMixer::FreeWord( int nWord )
{
	if ( m_pWordMixers[nWord] )
	{
		delete m_pWordMixers[nWord];
		m_pWordMixers[nWord] = NULL;
	}	

	if ( m_VoxWords[nWord].sfx )
	{
		// If this wave wasn't precached by the game code
		if ( !m_VoxWords[nWord].fKeepCached )
		{
			// If this was the last mixer that had a reference
			if ( m_VoxWords[nWord].sfx->pSource->CanDelete() )
			{
				// free the source
				delete m_VoxWords[nWord].sfx->pSource;
				m_VoxWords[nWord].sfx->pSource = NULL;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Load a word
//-----------------------------------------------------------------------------
CAudioMixer *CSentenceMixer::LoadWord( int nWord )
{
	CAudioMixer *pMixer = NULL;
	if ( m_VoxWords[nWord].sfx )
	{
		CAudioSource *pSource = S_LoadSound( m_VoxWords[nWord].sfx, NULL );
		if ( pSource )
		{
			pSource->SetSentenceWord( true );
			pMixer = pSource->CreateMixer();
		}
	}

	return pMixer;
}

float CSentenceMixer::ModifyPitch( float pitch )
{
	if ( m_pCurrentWordMixer )
	{
		if ( m_VoxWords[m_currentWordIndex].pitch > 0 )
		{
			pitch += (m_VoxWords[m_currentWordIndex].pitch - 100) * 0.01f;
		}
	}
	return pitch;
}

float CSentenceMixer::GetVolumeScale( void )
{
	if ( m_pCurrentWordMixer )
	{
		if ( m_VoxWords[m_currentWordIndex].volume )
		{
			float volume = m_VoxWords[m_currentWordIndex].volume * 0.01;
			if ( volume < 1.0f )
				return volume;
		}
	}
	return 1.0f;
}

int  CSentenceMixer::SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	Assert( 0 );
	return 0;
}

// return number of samples mixed
int CSentenceMixer::MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	if ( !m_pCurrentWordMixer )
	{
		return 0;
	}

	// save this to compute total output
	int startingOffset = outputOffset;

	while ( sampleCount > 0 && m_pCurrentWordMixer )
	{
		int outputCount = m_pCurrentWordMixer->MixDataToDevice( pDevice, pChannel, sampleCount, outputRate, outputOffset );

		outputOffset += outputCount;
		sampleCount -= outputCount;

		if ( !m_pCurrentWordMixer->ShouldContinueMixing() )
		{
			bool bMouth = SND_IsMouth( pChannel );
			if ( bMouth )
			{
				SND_ClearMouth( pChannel );
			}

			// advance to next valid word mixer
			do 
			{
				m_currentWordIndex++;
				if ( m_currentWordIndex >= m_nNumWords )
				{
					// end of sentence
					m_pCurrentWordMixer = NULL;
					break;
				}
				m_pCurrentWordMixer = m_pWordMixers[m_currentWordIndex];
			}
			while ( m_pCurrentWordMixer == NULL );

			if ( m_pCurrentWordMixer )
			{
				m_bNewWord = true;

				pChannel->sfx = m_VoxWords[m_currentWordIndex].sfx;
				if ( bMouth )
				{
					SND_UpdateMouth( pChannel );
				}
				if ( !IsReadyToMix() )
				{
					// current word isn't ready, stop mixing
					break;
				}
			}
		}
	}

	return outputOffset - startingOffset;
}
