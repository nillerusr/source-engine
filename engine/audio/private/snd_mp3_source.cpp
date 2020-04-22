//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "audio_pch.h"
#include "snd_mp3_source.h"
#include "snd_dma.h"
#include "snd_wave_mixer_mp3.h"
#include "filesystem_engine.h"
#include "utldict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef DEDICATED  // have to test this because VPC is forcing us to compile this file.

// How many bytes initial data bytes of the mp3 should be saved in the soundcache, in addition to the small amount of
// metadata (playbackrate, etc). This will increase memory usage by
// ( N * number-of-precached-mp3-sounds-in-the-whole-game ) at all times, as the soundcache is held in memory.
//
// Right now we're setting this to zero. The IsReadyToMix() logic at the data layer will delay mixing of the sound until
// it arrives. Setting this to anything above zero, however, will allow the sound to start, so it needs to either be
// enough to cover SND_ASYNC_LOOKAHEAD_SECONDS or none at all.
#define MP3_STARTUP_DATA_SIZE_BYTES 0

CUtlDict< CSentence *, int> g_PhonemeFileSentences;
bool g_bAllPhonemesLoaded;

void PhonemeMP3Shutdown( void )
{
	g_PhonemeFileSentences.PurgeAndDeleteElements();
	g_bAllPhonemesLoaded = false;
}

void AddPhonemesFromFile( const char *pszFileName )
{
	// If all Phonemes are loaded, do not load anymore
	if ( g_bAllPhonemesLoaded && g_PhonemeFileSentences.Count() != 0 )
		return;

	// Empty file name implies stop loading more phonemes
	if ( pszFileName == NULL )
	{
		g_bAllPhonemesLoaded = true;
		return;
	}

	// Load this file
	g_bAllPhonemesLoaded = false;

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( g_pFileSystem->ReadFile( pszFileName, "MOD", buf ) )
	{
		while ( 1 )
		{
			char token[4096];
			buf.GetString( token );

			V_FixSlashes( token );

			int iIndex = g_PhonemeFileSentences.Find( token );

			if ( iIndex != g_PhonemeFileSentences.InvalidIndex() )
			{
				delete g_PhonemeFileSentences.Element( iIndex );
				g_PhonemeFileSentences.Remove( token );
			}

			CSentence *pSentence = new CSentence;
			g_PhonemeFileSentences.Insert( token, pSentence );

			buf.GetString( token );

			if ( strlen( token ) <= 0 )
				break;

			if ( !stricmp( token, "{" ) )
			{
				pSentence->InitFromBuffer( buf );
			}
		}
	}
}

CAudioSourceMP3::CAudioSourceMP3( CSfxTable *pSfx )
{
	m_sampleRate = 0;
	m_pSfx = pSfx;
	m_refCount = 0;

	m_dataStart = 0;

	int file = g_pSndIO->open( pSfx->GetFileName() );
	if ( file != -1 )
	{
		m_dataSize = g_pSndIO->size( file );
		g_pSndIO->close( file );
	}
	else
	{
		// No sound cache, the file isn't here, print this so that the relatively deep failure points that are about to
		// spew make a little more sense
		Warning( "MP3 is completely missing, sound system will be upset to learn of this [ %s ]\n", pSfx->GetFileName() );
		m_dataSize = 0;
	}


	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
	m_bIsSentenceWord = false;
	m_bCheckedForPendingSentence = false;
}

CAudioSourceMP3::CAudioSourceMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info )
{
	m_pSfx = pSfx;
	m_refCount = 0;

	m_sampleRate = info->SampleRate();
	m_dataSize = info->DataSize();
	m_dataStart = info->DataStart();

	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
	m_bCheckedForPendingSentence = false;

	CheckAudioSourceCache();
}

CAudioSourceMP3::~CAudioSourceMP3()
{
}

// mixer's references
void CAudioSourceMP3::ReferenceAdd( CAudioMixer * )
{
	m_refCount++;
}

void CAudioSourceMP3::ReferenceRemove( CAudioMixer * )
{
	m_refCount--;
	if ( m_refCount == 0 && IsPlayOnce() )
	{
		SetPlayOnce( false ); // in case it gets used again
		CacheUnload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceMP3::IsAsyncLoad()
{
	// If there's a bit of "cached data" then we don't have to lazy/async load (we still async load the remaining data,
	//  but we run from the cache initially)
	return ( m_nCachedDataSize > 0 ) ? false : true;
}

// check reference count, return true if nothing is referencing this
bool CAudioSourceMP3::CanDelete( void )
{
	return m_refCount > 0 ? false : true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAudioSourceMP3::GetType()
{
	return AUDIO_SOURCE_MP3;
}

//-----------------------------------------------------------------------------
void CAudioSourceMP3::SetSentence( CSentence *pSentence )
{
	CAudioSourceCachedInfo *info = m_AudioCacheHandle.FastGet();

	if ( !info )
		return;

	if ( info && info->Sentence() )
		return;

	CSentence *pNewSentence = new CSentence;

	pNewSentence->Append( 0.0f, *pSentence );
	pNewSentence->MakeRuntimeOnly();

	info->SetSentence( pNewSentence );
}

int CAudioSourceMP3::SampleRate()
{
	if ( !m_sampleRate )
	{
		// This should've come from the sound cache. We can avoid sync I/O jank if and only if we've started streaming
		// data already for some other reason. (Despite the name, CreateWaveDataMemory is just creating a wrapper class
		// that manages access to the wave data cache)
		IWaveData *pData = CreateWaveDataMemory( *this );
		if ( !pData->IsReadyToMix() && SND_IsInGame() )
		{
			// If you hit this, you're creating a sound source that isn't in the sound cache, and asking for its sample
			// rate before it has streamed enough data in to read it from the underlying file. Your options are:
			// - Rebuild sound cache or figure out why this sound wasn't included.
			// - Precache this sound at level load so this doesn't happen during gameplay.
			// - Somehow call CacheLoad() on this source earlier so it has time to get data into memory so the data
			//   shows up as IsReadyToMix here, and this crutch won't jank.
			Warning( "MP3 initialized with no sound cache, this may cause janking. [ %s ]\n", GetFileName() );
			// The code below will still go fine, but the mixer will emit a jank warning that the data wasn't ready and
			// do sync I/O
		}
		CAudioMixerWaveMP3 *pMixer = new CAudioMixerWaveMP3( pData );
		m_sampleRate = pMixer->GetStreamOutputRate();
		// pData ownership is passed to, and free'd by, pMixer
		delete pMixer;
	}
	return m_sampleRate;
}

void CAudioSourceMP3::GetCacheData( CAudioSourceCachedInfo *info )
{
	// Don't want to replicate our cached sample rate back into the new cache, ensure we recompute it.
	CAudioMixerWaveMP3 *pTempMixer = new CAudioMixerWaveMP3( CreateWaveDataMemory(*this) );
	m_sampleRate = pTempMixer->GetStreamOutputRate();
	delete pTempMixer;

	AssertMsg( m_sampleRate, "Creating cache with invalid sample rate data" );
	if ( !m_sampleRate )
	{
		Warning( "Failed to find sample rate creating cache data for MP3, cache will be invalid [ %s ]\n", GetFileName() );
	}

	info->SetSampleRate( m_sampleRate );
	info->SetDataStart( 0 );

	int file = g_pSndIO->open( m_pSfx->GetFileName() );
	if ( !file )
	{
		Warning( "Failed to find file for building soundcache [ %s ]\n", m_pSfx->GetFileName() );
		// Don't re-use old cached value
		m_dataSize = 0;
	}
	else
	{
		m_dataSize = (int)g_pSndIO->size( file );
	}

	Assert( m_dataSize > 0 );

	// Do we need to actually load any audio data?
#if MP3_STARTUP_DATA_SIZE_BYTES > 0 // We may have defined the startup data to nothingness
	if ( info->s_bIsPrecacheSound && m_dataSize > 0 )
	{
		// Ideally this would mimic the wave startup data code and figure out this calculation:
		//   int bytesNeeded = m_channels * ( m_bits >> 3 ) * m_rate * SND_ASYNC_LOOKAHEAD_SECONDS;
		// (plus header)
		int dataSize = min( MP3_STARTUP_DATA_SIZE_BYTES, m_dataSize );
		byte *data = new byte[ dataSize ]();
		int readSize = g_pSndIO->read( data, dataSize, file );
		if ( readSize != dataSize )
		{
			Warning( "Building soundcache, expected %i bytes of data but got %i [ %s ]\n", dataSize, readSize, m_pSfx->GetFileName() );
			dataSize = readSize;
		}
		info->SetCachedDataSize( dataSize );
		info->SetCachedData( data );
	}
#endif // MP3_STARTUP_DATA_SIZE_BYTES > 0

	g_pSndIO->close( file );

	// Data size gets computed in GetStartupData!!!
	info->SetDataSize( m_dataSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CAudioSourceMP3::GetFileName()
{
	return m_pSfx ? m_pSfx->GetFileName() : "NULL m_pSfx";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceMP3::CheckAudioSourceCache()
{
	Assert( m_pSfx );

	if ( !m_pSfx->IsPrecachedSound() )
	{
		return;
	}

	// This will "re-cache" this if it's not in this level's cache already
	m_AudioCacheHandle.Get( GetType(), true, m_pSfx, &m_nCachedDataSize );
}

//-----------------------------------------------------------------------------
// Purpose: NULL the wave data pointer (we haven't loaded yet)
//-----------------------------------------------------------------------------
CAudioSourceMP3Cache::CAudioSourceMP3Cache( CSfxTable *pSfx ) : 
	CAudioSourceMP3( pSfx )
{
	m_hCache = 0;
}

CAudioSourceMP3Cache::CAudioSourceMP3Cache( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) :
	CAudioSourceMP3( pSfx, info )
{
	m_hCache = 0;

	m_dataSize = info->DataSize();
	m_dataStart = info->DataStart();

	m_bNoSentence = false;
}

//-----------------------------------------------------------------------------
// Purpose: Free any wave data we've allocated
//-----------------------------------------------------------------------------
CAudioSourceMP3Cache::~CAudioSourceMP3Cache( void )
{
	CacheUnload();
}

int CAudioSourceMP3Cache::GetCacheStatus( void )
{
	bool bCacheValid;
	int loaded = wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid ) ? AUDIO_IS_LOADED : AUDIO_NOT_LOADED;
	if ( !bCacheValid )
	{
		wavedatacache->RestartDataLoad( &m_hCache, m_pSfx->GetFileName(), m_dataSize, m_dataStart );
	}
	return loaded;
}


void CAudioSourceMP3Cache::CacheLoad( void )
{
	// Commence lazy load?
	if ( m_hCache != 0 )
	{
		GetCacheStatus();
		return;
	}

	m_hCache = wavedatacache->AsyncLoadCache( m_pSfx->GetFileName(), m_dataSize, m_dataStart );
}

void CAudioSourceMP3Cache::CacheUnload( void )
{
	if ( m_hCache != 0 )
	{
		wavedatacache->Unload( m_hCache );
	}
}

char *CAudioSourceMP3Cache::GetDataPointer( void )
{
	char *pMP3Data = NULL;
	bool dummy = false;

	if ( m_hCache == 0 )
	{
		CacheLoad();
	}

	wavedatacache->GetDataPointer( 
		m_hCache, 
		m_pSfx->GetFileName(), 
		m_dataSize, 
		m_dataStart, 
		(void **)&pMP3Data, 
		0, 
		&dummy );

	return pMP3Data;
}

int CAudioSourceMP3Cache::GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	// how many bytes are available ?
	int totalSampleCount = m_dataSize - samplePosition;

	// may be asking for a sample out of range, clip at zero
	if ( totalSampleCount < 0 )
		totalSampleCount = 0;

	// clip max output samples to max available
	if ( sampleCount > totalSampleCount )
		sampleCount = totalSampleCount;

	// if we are returning some samples, store the pointer
	if ( sampleCount )
	{
		// Starting past end of "preloaded" data, just use regular cache
		if ( samplePosition >= m_nCachedDataSize )
		{
			*pData = GetDataPointer();
		}
		else
		{
			// Start async loader if we haven't already done so
			CacheLoad();

			// Return less data if we are about to run out of uncached data
			if ( samplePosition + sampleCount >= m_nCachedDataSize )
			{
				sampleCount = m_nCachedDataSize - samplePosition;
			}

			// Point at preloaded/cached data from .cache file for now
			*pData = GetCachedDataPointer();
		}

		if ( *pData )
		{
			*pData = (char *)*pData + samplePosition;
		}
		else
		{
			// Out of data or file i/o problem
			sampleCount = 0;
		}
	}

	return sampleCount;
}

CAudioMixer	*CAudioSourceMP3Cache::CreateMixer( int initialStreamPosition )
{
	CAudioMixer *pMixer = new CAudioMixerWaveMP3( CreateWaveDataMemory(*this) );

	return pMixer;
}

CSentence *CAudioSourceMP3Cache::GetSentence( void )
{
	// Already checked and this wav doesn't have sentence data...
	if ( m_bNoSentence == true )
	{
		return NULL;
	}

	// Look up sentence from cache
	CAudioSourceCachedInfo *info = m_AudioCacheHandle.FastGet();
	if ( !info )
	{
		info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}
	Assert( info );
	if ( !info )
	{
		m_bNoSentence = true;
		return NULL;
	}

	CSentence *sentence = info->Sentence();
	if ( !sentence )
	{
		if ( !m_bCheckedForPendingSentence )
		{
			int iSentence = g_PhonemeFileSentences.Find( m_pSfx->GetFileName() );
			if ( iSentence != g_PhonemeFileSentences.InvalidIndex() )
			{
				sentence = g_PhonemeFileSentences.Element( iSentence );
				SetSentence( sentence );
			}
			m_bCheckedForPendingSentence = true;
		}
	}

	if ( !sentence )
	{
		m_bNoSentence = true;
		return NULL;
	}

	if ( sentence->m_bIsValid )
	{
		return sentence;
	}

	m_bNoSentence = true;

	return NULL;
}

//-----------------------------------------------------------------------------
// CAudioSourceStreamMP3
//-----------------------------------------------------------------------------
CAudioSourceStreamMP3::CAudioSourceStreamMP3( CSfxTable *pSfx ) :
	CAudioSourceMP3( pSfx )
{
}

CAudioSourceStreamMP3::CAudioSourceStreamMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) :
	CAudioSourceMP3( pSfx, info )
{
	m_dataSize = info->DataSize();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceStreamMP3::Prefetch()
{
	PrefetchDataStream( m_pSfx->GetFileName(), 0, m_dataSize );
}

CAudioMixer	*CAudioSourceStreamMP3::CreateMixer( int intialStreamPosition )
{
	// BUGBUG: Source constructs the IWaveData, mixer frees it, fix this?
	IWaveData *pWaveData = CreateWaveDataStream( *this, static_cast<IWaveStreamSource *>( this ), m_pSfx->GetFileName(), 0, m_dataSize, m_pSfx, 0 );
	if ( pWaveData )
	{
		CAudioMixer *pMixer = new CAudioMixerWaveMP3( pWaveData );
		if ( pMixer )
		{
			if ( !m_bCheckedForPendingSentence )
			{
				int iSentence = g_PhonemeFileSentences.Find( m_pSfx->GetFileName() );

				if ( iSentence != g_PhonemeFileSentences.InvalidIndex() )
				{
					SetSentence( g_PhonemeFileSentences.Element( iSentence ) );
				}

				m_bCheckedForPendingSentence = true;
			}

			return pMixer;
		}

		// no mixer but pWaveData was deleted in mixer's destructor
		// so no need to delete
	}

	return NULL;
}

int	CAudioSourceStreamMP3::GetOutputData( void **pData, int samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	return 0;
}

bool Audio_IsMP3( const char *pName )
{
	int len = strlen(pName);
	if ( len > 4 )
	{
		if ( !Q_strnicmp( &pName[len - 4], ".mp3", 4 ) )
		{
			return true;
		}
	}
	return false;
}


CAudioSource *Audio_CreateStreamedMP3( CSfxTable *pSfx )
{
	CAudioSourceStreamMP3 *pMP3 = NULL; 	
	CAudioSourceCachedInfo *info = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_MP3, pSfx->IsPrecachedSound(), pSfx );
	if ( info )
	{
		pMP3 = new CAudioSourceStreamMP3( pSfx, info );
	}
	else
	{
		pMP3 = new CAudioSourceStreamMP3( pSfx );
	}
	return pMP3;
}


CAudioSource *Audio_CreateMemoryMP3( CSfxTable *pSfx )
{
	CAudioSourceMP3Cache *pMP3 = NULL;
	CAudioSourceCachedInfo *info = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_MP3, pSfx->IsPrecachedSound(), pSfx );
	if ( info )
	{
		pMP3 = new CAudioSourceMP3Cache( pSfx, info );
	}
	else
	{
		pMP3 = new CAudioSourceMP3Cache( pSfx );
	}
	return pMP3;
}

#endif

