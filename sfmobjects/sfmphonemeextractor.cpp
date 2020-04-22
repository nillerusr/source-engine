//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "sfmobjects/SFMPhonemeExtractor.h"
#include "tier2/riff.h"
#include "PhonemeConverter.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "sentence.h"
#include "movieobjects/dmesound.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmebookmark.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "soundchars.h"
#include "tier2/p4helpers.h"
#include "tier2/soundutils.h"
#include "tier1/utldict.h"

#include <windows.h>  // WAVEFORMATEX, WAVEFORMAT and ADPCM WAVEFORMAT!!!
#include <mmreg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static const char *s_pAttributeValueNames[LOG_PREVIEW_FLEX_CHANNEL_COUNT] = 
{
	"value",
	"balance",
	"multilevel"
};

static const char *s_pDefaultAttributeValueNames[LOG_PREVIEW_FLEX_CHANNEL_COUNT] = 
{
	"defaultValue",
	"defaultBalance",
	"defaultMultilevel"
};


struct Extractor
{
	PE_APITYPE			apitype;
	CSysModule			*module;
	IPhonemeExtractor	*extractor;
};


//-----------------------------------------------------------------------------
// Implementations of the phoneme extractor
//-----------------------------------------------------------------------------
class CSFMPhonemeExtractor : public ISFMPhonemeExtractor
{
public:
	CSFMPhonemeExtractor();

	// Inherited from ISFMPhonemeExtractor
	virtual bool Init();
	virtual void Shutdown();
	virtual int GetAPICount();
	virtual void GetAPIInfo( int index, CUtlString* pPrintName, PE_APITYPE *pAPIType );
	virtual void Extract( const PE_APITYPE& apiType, ExtractDesc_t& info, bool bWritePhonemesToWavFiles );
	virtual void ReApply( ExtractDesc_t& info );
	virtual bool GetSentence( CDmeGameSound *gameSound, CSentence& sentence );

private:
	int FindExtractor( PE_APITYPE type );
	bool GetWaveFormat( const char *filename, CUtlBuffer* pFormat, int *pDataSize, CSentence& sentence, bool &bGotSentence );
	void LogPhonemes( int nItemIndex, ExtractDesc_t& info ); 
	void ClearInterstitialSpaces( CDmeChannelsClip *pChannelsClip, CUtlDict< LogPreview_t *, int >& controlLookup, ExtractDesc_t& info );

	void StampControlValueLogs( CDmePreset *preset, DmeTime_t tHeadPosition, float flIntensity, CUtlDict< LogPreview_t *, int > &controlLookup );
	void WriteCurrentValuesIntoLogLayers( DmeTime_t tHeadPosition, const CUtlDict< LogPreview_t *, int > &controlLookup );
	void WriteDefaultValuesIntoLogLayers( DmeTime_t tHeadPosition, const CUtlDict< LogPreview_t *, int > &controlLookup );
	void BuildPhonemeLogList( CUtlVector< LogPreview_t > &list, CUtlVector< CDmeLog * > &logs );
	CDmeChannelsClip* FindFacialChannelsClip( const CUtlVector< LogPreview_t > &list );
	void BuildPhonemeToPresetMapping( const CUtlVector< CBasePhonemeTag * > &stream, CDmeAnimationSet *pSet, CDmePresetGroup * pPresetGroup, CUtlDict< CDmePreset *, unsigned short > &phonemeToPresetDict );

	CUtlVector< Extractor >	m_Extractors;
	int m_nCurrentExtractor;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CSFMPhonemeExtractor g_ExtractorSingleton;
ISFMPhonemeExtractor *sfm_phonemeextractor = &g_ExtractorSingleton;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSFMPhonemeExtractor::CSFMPhonemeExtractor() : m_nCurrentExtractor( -1 )
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CSFMPhonemeExtractor::Init()
{
	// Enumerate modules under bin folder of exe
	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx( "phonemeextractors/*.dll", "EXECUTABLE_PATH", &findHandle );
	while( pFilename )
	{	
		char fullpath[ 512 ];
		Q_snprintf( fullpath, sizeof( fullpath ), "phonemeextractors/%s", pFilename );

		// Msg( "Loading extractor from %s\n", fullpath );

		Extractor e;
		e.module = g_pFullFileSystem->LoadModule( fullpath );
		if ( !e.module )
		{
			pFilename = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		CreateInterfaceFn factory = Sys_GetFactory( e.module );
		if ( !factory )
		{
			pFilename = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		e.extractor = ( IPhonemeExtractor * )factory( VPHONEME_EXTRACTOR_INTERFACE, NULL );
		if ( !e.extractor )
		{
			Warning( "Unable to get IPhonemeExtractor interface version %s from %s\n", VPHONEME_EXTRACTOR_INTERFACE, fullpath );
			pFilename = g_pFullFileSystem->FindNext( findHandle );
			continue;
		}

		e.apitype = e.extractor->GetAPIType();

		m_Extractors.AddToTail( e );
		pFilename = g_pFullFileSystem->FindNext( findHandle );
	}

	g_pFullFileSystem->FindClose( findHandle );
	return true;
}

void CSFMPhonemeExtractor::Shutdown()
{
	int c = m_Extractors.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		Extractor *e = &m_Extractors[ i ];
		g_pFullFileSystem->UnloadModule( e->module );
	}

	m_Extractors.RemoveAll();
}


//-----------------------------------------------------------------------------
// Finds an extractor of a particular type
//-----------------------------------------------------------------------------
int CSFMPhonemeExtractor::FindExtractor( PE_APITYPE type )
{
	for ( int i=0; i < m_Extractors.Count(); i++ )
	{
		if ( m_Extractors[i].apitype == type )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Iterates over extractors
//-----------------------------------------------------------------------------
int CSFMPhonemeExtractor::GetAPICount()
{
	return m_Extractors.Count();
}

void CSFMPhonemeExtractor::GetAPIInfo( int index, CUtlString* pPrintName, PE_APITYPE *pAPIType )
{
	Assert( pPrintName );
	Assert( pAPIType );
	pPrintName->Set( m_Extractors[ index ].extractor->GetName() );
	*pAPIType = m_Extractors[ index ].apitype;
}

static void ParseSentence( CSentence& sentence, IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

bool CSFMPhonemeExtractor::GetWaveFormat( const char *filename, CUtlBuffer *pBuf, int *pDataSize, CSentence& sentence, bool &bGotSentence )
{
	InFileRIFF riff( filename, *g_pFSIOReadBinary );
	Assert( riff.RIFFName() == RIFF_WAVE );

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	bool gotFmt = false;
	bool gotData = false;
	bGotSentence = false;

	// Walk input chunks and copy to output
	while ( walk.ChunkAvailable() )
	{
		switch ( walk.ChunkName() )
		{
		case WAVE_FMT:
			{
				pBuf->SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );
				walk.ChunkRead( pBuf->Base() );
				gotFmt = true;
			}
			break;
		case WAVE_DATA:
			{
				*pDataSize = walk.ChunkSize();
				gotData = true;
			}
			break;
		case WAVE_VALVEDATA:
			{
				bGotSentence = true;
				ParseSentence( sentence, walk );
			}
			break;
		default:
			break;
		}

		// Done
		if ( gotFmt && gotData && bGotSentence )
			return true;

		walk.ChunkNext();
	}
	return ( gotFmt && gotData );
}

bool CSFMPhonemeExtractor::GetSentence( CDmeGameSound *gameSound, CSentence& sentence )
{
	const char *filename = gameSound->m_SoundName.Get();
	Assert( filename && filename [ 0 ] );

	char soundname[ 512 ];
	// Note, calling PSkipSoundChars to remove any decorator characters used by the engine!!!
	Q_snprintf( soundname, sizeof( soundname ), "sound/%s", PSkipSoundChars( filename ) );
	Q_FixSlashes( soundname );

	char fullpath[ 512 ];
	g_pFullFileSystem->RelativePathToFullPath( soundname, "GAME", fullpath, sizeof( fullpath ) );

	// Get sound file metrics of interest
	CUtlBuffer buf;
	int nDataSize;
	bool bValidSentence = false;
	if ( !GetWaveFormat( soundname, &buf, &nDataSize, sentence, bValidSentence ) )
		return false;

	return bValidSentence;
}

static void BuildPhonemeStream( CSentence& in, CUtlVector< CBasePhonemeTag * >& list )
{
	for ( int i = 0; i < in.m_Words.Count(); ++i )
	{
		CWordTag *w = in.m_Words[ i ];
		if ( !w )
			continue;

		for ( int j = 0; j < w->m_Phonemes.Count(); ++j )
		{
			CPhonemeTag *ph = w->m_Phonemes[ j ];
			if ( !ph )
				continue;

			CBasePhonemeTag *newTag = new CBasePhonemeTag( *ph );
			list.AddToTail( newTag );
		}
	}

	if ( !in.m_Words.Count() && in.m_RunTimePhonemes.Count() )
	{
		for ( int i = 0 ; i < in.m_RunTimePhonemes.Count(); ++i )
		{
			CBasePhonemeTag *newTag = new CBasePhonemeTag( *in.m_RunTimePhonemes[ i ] );
			list.AddToTail( newTag );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Same the phoneme data into the sound files
//-----------------------------------------------------------------------------
static void StoreValveDataChunk( CSentence& sentence, IterateOutputRIFF& store )
{
	// Buffer and dump data
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	sentence.SaveToBuffer( buf );

	// Copy into store
	store.ChunkWriteData( buf.Base(), buf.TellPut() );
}

static bool SaveSentenceToWavFile( const char *pWavFile, CSentence& sentence )
{
	char pTempFile[ 512 ];

	Q_StripExtension( pWavFile, pTempFile, sizeof( pTempFile ) );
	Q_DefaultExtension( pTempFile, ".tmp", sizeof( pTempFile ) );

	if ( g_pFullFileSystem->FileExists( pTempFile, "GAME" ) )
	{
		g_pFullFileSystem->RemoveFile( pTempFile, "GAME" );
	}

	CP4AutoEditAddFile p4Checkout( pWavFile );
	if ( !g_pFullFileSystem->IsFileWritable( pWavFile ) )
	{
		Warning( "%s is not writable, can't save sentence data to file\n", pWavFile );
		return false;
	}

	// Rename original pWavFile to temp
	g_pFullFileSystem->RenameFile( pWavFile, pTempFile, "GAME" );

	// NOTE:  Put this in it's own scope so that the destructor for outfileRFF actually closes the file!!!!
	{
		// Read from Temp
		InFileRIFF riff( pTempFile, *g_pFSIOReadBinary );
		Assert( riff.RIFFName() == RIFF_WAVE );

		// set up the iterator for the whole file (root RIFF is a chunk)
		IterateRIFF walk( riff, riff.RIFFSize() );

		// And put data back into original pWavFile by name
		OutFileRIFF riffout( pWavFile, *g_pFSIOWriteBinary );

		IterateOutputRIFF store( riffout );

		bool bWordTrackWritten = false;

		// Walk input chunks and copy to output
		while ( walk.ChunkAvailable() )
		{
			store.ChunkStart( walk.ChunkName() );

			switch ( walk.ChunkName() )
			{
			case WAVE_VALVEDATA:
				{
					// Overwrite data
					StoreValveDataChunk( sentence, store );
					bWordTrackWritten = true;
				}
				break;
			default:
				store.CopyChunkData( walk );
				break;
			}

			store.ChunkFinish();

			walk.ChunkNext();
		}

		// If we didn't write it above, write it now
		if ( !bWordTrackWritten )
		{
			store.ChunkStart( WAVE_VALVEDATA );
			StoreValveDataChunk( sentence, store );
			store.ChunkFinish();
		}
	}

	// Remove temp file
	g_pFullFileSystem->RemoveFile( pTempFile, NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for phoneme extraction
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::Extract( const PE_APITYPE& apiType, ExtractDesc_t& info, bool bWritePhonemesToWavFiles )
{
	if ( !info.m_pSet )
		return;

	int iExtractor = FindExtractor( apiType );
	if ( iExtractor == -1 )
		return;

	Extractor& extractor = m_Extractors[ iExtractor ];

	int nWorkItem;
	for ( nWorkItem = 0; nWorkItem < info.m_WorkList.Count(); ++nWorkItem )
	{
		CExtractInfo& workItem = info.m_WorkList[ nWorkItem ];

		workItem.m_flDuration = 0.0f;

		CSentence in;
		CSentence out;
		in.SetText( workItem.m_sHintText.String() );
		out.SetText( workItem.m_sHintText.String() );

		const char *pFileName = workItem.m_pSound->m_SoundName.Get();
		Assert( pFileName && pFileName [ 0 ] );

		char pSoundName[ 512 ];
		// Note, calling PSkipSoundChars to remove any decorator characters used by the engine!!!
		Q_snprintf( pSoundName, sizeof( pSoundName ), "sound/%s", PSkipSoundChars( pFileName ) );
		Q_FixSlashes( pSoundName );

		char pFullPath[ 512 ];
		g_pFullFileSystem->RelativePathToFullPath( pSoundName, "GAME", pFullPath, sizeof( pFullPath ) );

		// Get sound file metrics of interest
		CUtlBuffer buf;
		WAVEFORMATEX *format;
		int nDataSize;
		if ( !GetWaveFormat( pSoundName, &buf, &nDataSize, workItem.m_Sentence, workItem.m_bSentenceValid ) )
			continue;

		format = ( WAVEFORMATEX * )buf.Base();

		if ( !( format->wBitsPerSample > ( 1 << 3 ) ) )
		{
			// Have to warn and early-out here to avoid crashing with "integer divide by zero" below
			Warning( "Cannot extract phonemes from '%s', %u bits per sample.\n", pSoundName, format->wBitsPerSample );
			continue;
		}

		int nBitsPerSample = format->wBitsPerSample;
		float flSampleRate = (float)format->nSamplesPerSec;
		int nChannels = format->nChannels;
		int nSampleCount = nDataSize / ( nBitsPerSample >> 3 );

		float flTrueSampleSize = ( nBitsPerSample * nChannels ) >> 3;
		if ( format->wFormatTag == WAVE_FORMAT_ADPCM )
		{
			nBitsPerSample = 16;
			flTrueSampleSize = 0.5f;

			ADPCMWAVEFORMAT *pFormat = (ADPCMWAVEFORMAT *)buf.Base();
			int blockSize = ((pFormat->wSamplesPerBlock - 2) * pFormat->wfx.nChannels ) / 2;
			blockSize += 7 * pFormat->wfx.nChannels;

			int blockCount = nDataSize / blockSize;
			int blockRem = nDataSize % blockSize;

			// total samples in complete blocks
			nSampleCount = blockCount * pFormat->wSamplesPerBlock;

			// add remaining in a short block
			if ( blockRem )
			{
				nSampleCount += pFormat->wSamplesPerBlock - (((blockSize - blockRem) * 2) / nChannels);
			}
		}

		if ( flSampleRate > 0.0f )
		{
			workItem.m_flDuration = (float)nSampleCount / flSampleRate;
		}
		in.CreateEventWordDistribution( workItem.m_sHintText.String(), workItem.m_flDuration );
		if ( !workItem.m_bUseSentence || !workItem.m_bSentenceValid )
		{
			extractor.extractor->Extract( pFullPath,
				(int)( workItem.m_flDuration * flSampleRate * flTrueSampleSize ),
				Msg, in, out );

			// Tracker 57389:
			// Total hack to fix a bug where the Lipsinc extractor is messing up the # channels on 16 bit stereo waves
			if ( apiType == SPEECH_API_LIPSINC && nChannels == 2 && nBitsPerSample == 16 )
			{
				flTrueSampleSize *= 2.0f;
			}

			float bytespersecond = flSampleRate * flTrueSampleSize;

			int i;
			// Now convert byte offsets to times
			for ( i = 0; i < out.m_Words.Size(); i++ )
			{
				CWordTag *tag = out.m_Words[ i ];
				Assert( tag );
				if ( !tag )
					continue;

				tag->m_flStartTime = ( float )(tag->m_uiStartByte ) / bytespersecond;
				tag->m_flEndTime = ( float )(tag->m_uiEndByte ) / bytespersecond;

				for ( int j = 0; j < tag->m_Phonemes.Size(); j++ )
				{
					CPhonemeTag *ptag = tag->m_Phonemes[ j ];
					Assert( ptag );
					if ( !ptag )
						continue;

					ptag->SetStartTime( ( float )(ptag->m_uiStartByte ) / bytespersecond );
					ptag->SetEndTime( ( float )(ptag->m_uiEndByte ) / bytespersecond );
				}
			}

			if ( bWritePhonemesToWavFiles )
			{
				SaveSentenceToWavFile( pFullPath, out );
			}
		}
		else
		{
			Msg( "Using .wav file phonemes for (%s)\n", pSoundName );
			out = workItem.m_Sentence;
		}

		// Now create channel data
		workItem.ClearTags();
		BuildPhonemeStream( out, workItem.m_ApplyTags );
	}

	if ( info.m_bCreateBookmarks )
	{
		info.m_pSet->GetBookmarks().RemoveAll();
	}

	for ( nWorkItem = 0; nWorkItem < info.m_WorkList.Count(); ++nWorkItem )
	{
		LogPhonemes( nWorkItem, info );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static bool UniquePhonemeLessFunc( CBasePhonemeTag * const & lhs, CBasePhonemeTag * const & rhs )
{
	return lhs->GetPhonemeCode() < rhs->GetPhonemeCode();
}

void CSFMPhonemeExtractor::BuildPhonemeToPresetMapping( const CUtlVector< CBasePhonemeTag * > &stream, 
													   CDmeAnimationSet *pSet, CDmePresetGroup *pPresetGroup, CUtlDict< CDmePreset *, unsigned short > &phonemeToPresetDict )
{
	int i;
	CUtlRBTree< CBasePhonemeTag * > uniquePhonemes( 0, 0, UniquePhonemeLessFunc );
	for ( i = 0; i < stream.Count(); ++i )
	{
		CBasePhonemeTag *tag = stream[ i ];
		if ( uniquePhonemes.Find( tag ) == uniquePhonemes.InvalidIndex() )
		{
			uniquePhonemes.Insert( tag );
		}
	}

	for ( i = uniquePhonemes.FirstInorder(); i != uniquePhonemes.InvalidIndex(); i = uniquePhonemes.NextInorder( i ) )
	{
		CBasePhonemeTag *tag = uniquePhonemes[ i ];
		// Convert phoneme code to text
		char ph[ 32 ];
		Q_strncpy( ph, ConvertPhoneme( tag->GetPhonemeCode() ), sizeof( ph ) );

		char remappedph[ 32 ];
		// By default we search for a preset name p_xxx where xxx is the phoneme string
		Q_snprintf( remappedph, sizeof( remappedph ), "p_%s", ph );
		// Now find the preset in the animation set converter
		CDmePhonemeMapping *mapping = pSet->FindMapping( ph );
		if ( mapping )
		{
			Q_strncpy( remappedph, mapping->GetValueString( "preset" ), sizeof( remappedph ) );
		}

		// Now look up the preset, if it exists
		CDmePreset *preset = pPresetGroup->FindPreset( remappedph );
		if ( !preset )
		{
			Warning( "Animation set '%s' missing phoneme preset for '%s' -> '%s'\n", 
				pSet->GetName(), ph, remappedph );
			continue;
		}

		// Add to dictionary if it's not already there
		if ( phonemeToPresetDict.Find( ph ) == phonemeToPresetDict.InvalidIndex() )
		{
			phonemeToPresetDict.Insert( ph, preset );
		}
	}
}



//-----------------------------------------------------------------------------
// Finds the channels clip which refers to facial control values 
//-----------------------------------------------------------------------------
CDmeChannelsClip* CSFMPhonemeExtractor::FindFacialChannelsClip( const CUtlVector< LogPreview_t > &list )
{
	CDmeChannelsClip *pChannelsClip = NULL;

	int i;
	for ( i = list.Count() - 1; i >= 0; --i )
	{
		const LogPreview_t &lp = list[i];
		CDmeChannelsClip *check = FindAncestorReferencingElement< CDmeChannelsClip >( (CDmElement *)lp.m_hChannels[ 0 ].Get() );

		if ( !pChannelsClip && check )
		{
			pChannelsClip = check;
		}
		else
		{
			if ( pChannelsClip != check )
			{
				Warning( "Selected controls overlap multiple channels clips!!!\n" );
			}
		}
	}

	if ( !pChannelsClip )
	{
		Warning( "Unable to determine destination channels clip!!!\n" );
	}

	return pChannelsClip;
}


//-----------------------------------------------------------------------------
// Builds the list of logs which target facial control values 
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::BuildPhonemeLogList( CUtlVector< LogPreview_t > &list, CUtlVector< CDmeLog * > &logs )
{
	for ( int i = 0; i < list.Count(); ++i )
	{
		LogPreview_t& p = list[ i ];

		for ( int channel = 0; channel < LOG_PREVIEW_FLEX_CHANNEL_COUNT; ++channel )
		{
			CDmeChannel *ch = p.m_hChannels[ channel ];
			if ( !ch )
				continue;

			CDmeLog *log = p.m_hChannels[ channel ]->GetLog();
			if ( !log )
				continue;

			logs.AddToTail( log );
		}
	}
}


//-----------------------------------------------------------------------------
// Writes default values into all log layers targetting facial control values
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::WriteDefaultValuesIntoLogLayers( DmeTime_t tHeadPosition, const CUtlDict< LogPreview_t *, int > &controlLookup )
{
	// Write a zero into all relevant log layers
	for ( int j = controlLookup.First(); j != controlLookup.InvalidIndex(); j = controlLookup.Next( j ) )
	{
		LogPreview_t* lp = controlLookup[ j ];

		CDmElement *pControl = lp->m_hControl;

		for ( int chIndex = 0; chIndex < LOG_PREVIEW_FLEX_CHANNEL_COUNT; ++chIndex )
		{
			CDmeChannel *pChannel = lp->m_hChannels[ chIndex ];
			if ( !pChannel )
				continue;

			// Now get the log for the channel
			CDmeFloatLog *pFloatLog = CastElement< CDmeFloatLog >( pChannel->GetLog() );
			if ( !pFloatLog )
				continue;

			CDmeFloatLogLayer *pLayer = pFloatLog->GetLayer( pFloatLog->GetTopmostLayer() );
			if ( !pLayer )
				continue;

			float flDefaultValue = pControl->GetValue< float >( s_pDefaultAttributeValueNames[chIndex] );
			pLayer->InsertKey( tHeadPosition, flDefaultValue );
		}
	}
}


//-----------------------------------------------------------------------------
// Creates a new log key based on the interpolated value at that time
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::WriteCurrentValuesIntoLogLayers( DmeTime_t tHeadPosition, const CUtlDict< LogPreview_t *, int > &controlLookup )
{
	// Write a zero into all relevant log layers
	for ( int j = controlLookup.First(); j != controlLookup.InvalidIndex(); j = controlLookup.Next( j ) )
	{
		LogPreview_t* lp = controlLookup[ j ];

		for ( int chIndex = 0; chIndex < LOG_PREVIEW_FLEX_CHANNEL_COUNT; ++chIndex )
		{
			CDmeChannel *pChannel = lp->m_hChannels[ chIndex ];
			if ( !pChannel )
				continue;

			// Now get the log for the channel
			CDmeFloatLog *pFloatLog = CastElement< CDmeFloatLog >( pChannel->GetLog() );
			if ( !pFloatLog )
				continue;

			CDmeFloatLogLayer *pLayer = pFloatLog->GetLayer( pFloatLog->GetTopmostLayer() );
			if ( !pLayer )
				continue;

			float flCurrentValue = pLayer->GetValue( tHeadPosition ); 
			pLayer->InsertKey( tHeadPosition, flCurrentValue );
		}
	}
}


//-----------------------------------------------------------------------------
// Samples extracted phoneme data and stamps that values into control value logs 
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::StampControlValueLogs( CDmePreset *preset, DmeTime_t tHeadPosition, float flIntensity, CUtlDict< LogPreview_t *, int > &controlLookup )
{
	// Now walk the logs required by the preset
	const CDmrElementArray< CDmElement > &controlValues = preset->GetControlValues( );
	for ( int j = 0; j < controlValues.Count(); ++j )
	{
		// This control contains the preset value
		CDmElement *presetControl = controlValues[ j ];
		if ( !presetControl )
			continue;

		int visIndex = controlLookup.Find( presetControl->GetName() );
		if ( visIndex == controlLookup.InvalidIndex() )
			continue;

		LogPreview_t* lp = controlLookup[ visIndex ];

		for ( int chIndex = 0; chIndex < LOG_PREVIEW_FLEX_CHANNEL_COUNT; ++chIndex )
		{
			CDmeChannel *ch = lp->m_hChannels[ chIndex ];
			if ( !ch )
				continue;

			// Whereas this control contains the "default" value for the slider (since the presetControl won't have that value)
			CDmElement *defaultValueControl = lp->m_hControl.Get();
			if ( !defaultValueControl )
				continue;

			// Now get the log for the channel
			CDmeLog *log = ch->GetLog();
			if ( !log )
			{
				Assert( 0 );
				continue;
			}

			CDmeFloatLog *floatLog = CastElement< CDmeFloatLog >( log );
			if ( !floatLog )
				continue;

			CDmeFloatLogLayer *pLayer = floatLog->GetLayer( floatLog->GetTopmostLayer() );
			if ( !pLayer )
				continue;

			float flDefault = defaultValueControl->GetValue< float >( s_pDefaultAttributeValueNames[chIndex] );
			float flControlValue = presetControl->GetValue< float >( s_pAttributeValueNames[ chIndex ] );
			float flNewValue = flIntensity * ( flControlValue - flDefault );
			float flCurrent = pLayer->GetValue( tHeadPosition ) - flDefault;
			// Accumulate new value into topmost layer
			pLayer->InsertKey( tHeadPosition, flCurrent + flNewValue + flDefault );
		}
	}
}

void CSFMPhonemeExtractor::ClearInterstitialSpaces( CDmeChannelsClip *pChannelsClip, CUtlDict< LogPreview_t *, int >& controlLookup, ExtractDesc_t& info )
{
	Assert( info.m_pShot );
	Assert( pChannelsClip );

	if ( info.m_WorkList.Count() == 0 )
		return;

	// This is handled by the main layering code...
	if ( info.m_nExtractType == EXTRACT_WIPE_SOUNDS )
		return;

	// Now walk through all relevant logs
	CUtlVector< CDmeLog * > logs;
	BuildPhonemeLogList( info.m_ControlList, logs );

	DmeTime_t tMinTime( DMETIME_MAXTIME );
	DmeTime_t tMaxTime( DMETIME_MINTIME );

	int i;
	// Walk work items and figure out time bounds
	for ( i = 0; i < info.m_WorkList.Count(); ++i )
	{
		CExtractInfo &item = info.m_WorkList[ i ];

		CUtlVector< CDmeHandle< CDmeClip > > srcStack;
		CUtlVector< CDmeHandle< CDmeClip > > dstStack;

		// Convert original .wav start to animation set channels clip relative time
		item.m_pClip->BuildClipStack( &srcStack, info.m_pMovie, info.m_pShot );

		// NOTE: Time bounds measured in sound media time goes from 0 -> flWaveDuration
		DmeTime_t tSoundMediaStartTime = CDmeClip::FromChildMediaTime( srcStack, DMETIME_ZERO, false );
		DmeTime_t tSoundMediaEndTime   = CDmeClip::FromChildMediaTime( srcStack, DmeTime_t( item.m_flDuration ), false );

		// NOTE: Start and end time are measured in sound media time
		DmeTime_t tStartTime = item.m_pClip->GetStartInChildMediaTime();
		DmeTime_t tEndTime   = item.m_pClip->GetEndInChildMediaTime();

		// And convert back down into channels clip relative time
		pChannelsClip->BuildClipStack( &dstStack, info.m_pMovie, info.m_pShot );

		// Now convert back down to channels clip relative time
		DmeTime_t tChannelMediaStartTime = CDmeClip::ToChildMediaTime( dstStack, tSoundMediaStartTime, false );
		DmeTime_t tChannelMediaEndTime = CDmeClip::ToChildMediaTime( dstStack, tSoundMediaEndTime, false );

		// Find a scale + offset which transforms data in media space of the sound [namely, the phonemes]
		// into the media space of the channels [the logs that drive the facial animation]
		DmeTime_t tEndDuration = tChannelMediaEndTime - tChannelMediaStartTime;
		double flScale = ( item.m_flDuration != 0.0f ) ? tEndDuration.GetSeconds() / item.m_flDuration : 0.0f;
		DmeTime_t tOffset = tChannelMediaStartTime;

		DmeTime_t tChannelRelativeStartTime( tStartTime * flScale );
		tChannelRelativeStartTime += tOffset;
		DmeTime_t tChannelRelativeEndTime( tEndTime * flScale );
		tChannelRelativeEndTime += tOffset;

		if ( tChannelRelativeStartTime < tMinTime )
		{
			tMinTime = tChannelRelativeStartTime;
		}
		if ( tChannelRelativeEndTime > tMaxTime )
		{
			tMaxTime = tChannelRelativeEndTime;
		}
	}

	// Bloat by one quantum
	tMinTime -= DMETIME_MINDELTA;
	tMaxTime += DMETIME_MINDELTA;

	for ( i = 0; i < logs.Count(); ++i )
	{
		CDmeLog *log = logs[ i ];

		Assert( log->GetNumLayers() == 1 );
		CDmeLogLayer *layer = log->GetLayer( log->GetTopmostLayer() );

		if ( info.m_nExtractType == EXTRACT_WIPE_RANGE )
		{
			// Write default value keys into log
			// Write a default value at that time
			WriteDefaultValuesIntoLogLayers( tMinTime, controlLookup );

			// Write a default value at that time
			WriteDefaultValuesIntoLogLayers( tMaxTime, controlLookup );

			// Now discard all keys > tMinTime and < tMaxTime
			for ( int j = layer->GetKeyCount() - 1; j >= 0; --j )
			{
				DmeTime_t &t = layer->GetKeyTime( j );
				if ( t <= tMinTime )
					continue;
				if ( t >= tMaxTime )
					continue;

				layer->RemoveKey( j );
			}
		}
		else
		{
			Assert( info.m_nExtractType == EXTRACT_WIPE_CLIP );
			layer->ClearKeys();
		}
	}
}

void AddAnimSetBookmarkAtSoundMediaTime( const char *pName, DmeTime_t tStart, DmeTime_t tEnd, const CUtlVector< CDmeHandle< CDmeClip > > &srcStack, ExtractDesc_t& info )
{
	tStart = CDmeClip::FromChildMediaTime( srcStack, tStart, false );
	tEnd   = CDmeClip::FromChildMediaTime( srcStack, tEnd, false );

	tStart = info.m_pShot->ToChildMediaTime( tStart, false );
	tEnd   = info.m_pShot->ToChildMediaTime( tEnd, false );

	CDmeBookmark *pBookmark = CreateElement< CDmeBookmark >( pName );
	pBookmark->SetNote( pName );
	pBookmark->SetTime( tStart );
	pBookmark->SetDuration( tEnd - tStart );
	info.m_pSet->GetBookmarks().AddToTail( pBookmark );
}

//-----------------------------------------------------------------------------
// Main entry point for generating phoneme logs 
//-----------------------------------------------------------------------------
void CSFMPhonemeExtractor::LogPhonemes( int nItemIndex,	ExtractDesc_t& info )
{
	CExtractInfo &item = info.m_WorkList[ nItemIndex ];

	// Validate input parameters
	Assert( info.m_pSet && item.m_pClip && item.m_pSound );
	if ( !info.m_pSet || !item.m_pClip || !item.m_pSound )
		return;

	CDmePresetGroup *pPresetGroup = info.m_pSet->FindPresetGroup( "phoneme" );
	if ( !pPresetGroup )
	{
		Warning( "Animation set '%s' missing preset group 'phoneme'\n", info.m_pSet->GetName() );
		return;
	}

	if ( !info.m_pSet->GetPhonemeMap().Count() )
	{
		info.m_pSet->RestoreDefaultPhonemeMap();
	}

	// Walk through phoneme stack and build list of unique presets
	CUtlDict< CDmePreset *, unsigned short > phonemeToPresetDict;
	BuildPhonemeToPresetMapping( item.m_ApplyTags, info.m_pSet, pPresetGroup, phonemeToPresetDict );

	CDmeChannelsClip *pChannelsClip = FindFacialChannelsClip( info.m_ControlList );
	if ( !pChannelsClip )
		return;

	// Build a fast lookup of the visible sliders
	int i;
	CUtlDict< LogPreview_t *, int > controlLookup;
	for ( i = 0; i < info.m_ControlList.Count(); ++i )
	{
		controlLookup.Insert( info.m_ControlList[ i ].m_hControl->GetName(), &info.m_ControlList[ i ] );
	}

	// Only need to do this on the first item and we have multiple .wavs selected
	if ( nItemIndex == 0 && info.m_WorkList.Count() > 1 )
	{
		ClearInterstitialSpaces( pChannelsClip, controlLookup, info );
	}

	// Set up time selection, put channels into record and stamp out keyframes

	// Convert original .wav start to animation set channels clip relative time
	CUtlVector< CDmeHandle< CDmeClip > > srcStack;
	item.m_pClip->BuildClipStack( &srcStack, info.m_pMovie, info.m_pShot );
	if ( srcStack.Count() == 0 )
	{
		item.m_pClip->BuildClipStack( &srcStack, info.m_pMovie, NULL );
		if ( srcStack.Count() == 0 )
		{
			Msg( "Couldn't build stack sound clip to current shot\n" );
			return;
		}
	}

	// NOTE: Time bounds measured in sound media time goes from 0 -> flWaveDuration
	DmeTime_t tSoundMediaStartTime = CDmeClip::FromChildMediaTime( srcStack, DMETIME_ZERO, false );
	DmeTime_t tSoundMediaEndTime   = CDmeClip::FromChildMediaTime( srcStack, DmeTime_t( item.m_flDuration ), false );

	// NOTE: Start and end time are measured in sound media time
	DmeTime_t tStartTime = item.m_pClip->GetStartInChildMediaTime();
	DmeTime_t tEndTime   = item.m_pClip->GetEndInChildMediaTime();

	// And convert back down into channels clip relative time
	CUtlVector< CDmeHandle< CDmeClip > > dstStack;
	pChannelsClip->BuildClipStack( &dstStack, info.m_pMovie, info.m_pShot );

	// Now convert back down to channels clip relative time
	DmeTime_t tChannelMediaStartTime = CDmeClip::ToChildMediaTime( dstStack, tSoundMediaStartTime, false );
	DmeTime_t tChannelMediaEndTime   = CDmeClip::ToChildMediaTime( dstStack, tSoundMediaEndTime, false );

	// Find a scale + offset which transforms data in media space of the sound [namely, the phonemes]
	// into the media space of the channels [the logs that drive the facial animation]
	DmeTime_t tEndDuration = tChannelMediaEndTime - tChannelMediaStartTime;
	double flScale = ( item.m_flDuration != 0.0f ) ? tEndDuration.GetSeconds() / item.m_flDuration : 0.0f;
	DmeTime_t tOffset = tChannelMediaStartTime;

	CUtlVector< CDmeLog * > logs;
	BuildPhonemeLogList( info.m_ControlList, logs );

	// Add new write layer to each recording log
	for ( i = 0; i < logs.Count(); ++i )
	{
		logs[ i ]->AddNewLayer();
	}

	// Iterate over the entire range of the sound
	double flStartSoundTime = max( 0, tStartTime.GetSeconds() );
	double flEndSoundTime = min( item.m_flDuration, tEndTime.GetSeconds() );

	// Stamp keys right before and after the sound so as to 
	// not generate new values outside the import time range
	DmeTime_t tPrePhonemeTime( flStartSoundTime * flScale );
	tPrePhonemeTime += tOffset - DMETIME_MINDELTA;
	WriteCurrentValuesIntoLogLayers( tPrePhonemeTime, controlLookup );

	DmeTime_t tPostPhonemeTime( flEndSoundTime * flScale );
	tPostPhonemeTime += tOffset + DMETIME_MINDELTA;
	WriteCurrentValuesIntoLogLayers( tPostPhonemeTime, controlLookup );

	// add bookmarks
	if ( info.m_bCreateBookmarks )
	{
		AddAnimSetBookmarkAtSoundMediaTime( "start", tPrePhonemeTime, tPrePhonemeTime, srcStack, info );

		for ( i = 0; i < item.m_ApplyTags.Count() ; ++i )
		{
			CBasePhonemeTag *p = item.m_ApplyTags[ i ];
			const char *pPhonemeName = ConvertPhoneme( p->GetPhonemeCode() );
			DmeTime_t tStart = DmeTime_t( p->GetStartTime() );
			DmeTime_t tEnd   = DmeTime_t( p->GetEndTime() );
			AddAnimSetBookmarkAtSoundMediaTime( pPhonemeName, tStart, tEnd, srcStack, info );
		}

		AddAnimSetBookmarkAtSoundMediaTime( "end", tPostPhonemeTime, tPostPhonemeTime, srcStack, info );
	}

	if ( info.m_nFilterType == EXTRACT_FILTER_HOLD || info.m_nFilterType == EXTRACT_FILTER_LINEAR )
	{
		CDmePreset *pLastPreset = NULL;

		for ( i = 0; i < item.m_ApplyTags.Count() ; ++i )
		{
			CBasePhonemeTag *p = item.m_ApplyTags[ i ];

			DmeTime_t tStart = DmeTime_t( p->GetStartTime() );
			DmeTime_t tEnd   = DmeTime_t( p->GetEndTime() );

			int idx = phonemeToPresetDict.Find( ConvertPhoneme( p->GetPhonemeCode() ) );
			if ( idx == phonemeToPresetDict.InvalidIndex() )
				continue;

			CDmePreset *preset = phonemeToPresetDict[ idx ];
			if ( !preset )
				continue;

			DmeTime_t tKeyTime = tStart * flScale + tOffset;

			if ( info.m_nFilterType == EXTRACT_FILTER_HOLD )
			{
				// stamp value at end of phoneme (or default prior to first phoneme)
				// NOTE - this ignores phoneme length, but since all phonemes directly abut one another, this doesn't matter
				DmeTime_t tLastEnd = tKeyTime - DMETIME_MINDELTA;
				if ( tLastEnd > tPrePhonemeTime )
				{
					WriteDefaultValuesIntoLogLayers( tKeyTime - DMETIME_MINDELTA, controlLookup );
					if ( pLastPreset )
					{
						StampControlValueLogs( pLastPreset, tKeyTime - DMETIME_MINDELTA, 1.0f, controlLookup );
					}
				}
				pLastPreset = preset;
			}

			WriteDefaultValuesIntoLogLayers( tKeyTime, controlLookup );
			StampControlValueLogs( preset, tKeyTime, 1.0f, controlLookup );

			if ( info.m_nFilterType == EXTRACT_FILTER_HOLD && i == item.m_ApplyTags.Count() - 1 )
			{
				// stamp value at end of last phoneme
				tKeyTime = tEnd * flScale + tOffset;
				tKeyTime = min( tKeyTime, tPostPhonemeTime );
				WriteDefaultValuesIntoLogLayers( tKeyTime - DMETIME_MINDELTA, controlLookup );
				StampControlValueLogs( preset, tKeyTime - DMETIME_MINDELTA, 1.0f, controlLookup );

				// stamp default just after end of last phoneme to hold silence until tPostPhonemeTime
				WriteDefaultValuesIntoLogLayers( tKeyTime, controlLookup );
			}
		}
	}
	else
	{
		Assert( info.m_nFilterType == EXTRACT_FILTER_FIXED_WIDTH );

		double tStep = 1.0 / (double)clamp( info.m_flSampleRateHz, 1.0f, 1000.0f );

		float flFilter = max( info.m_flSampleFilterSize, 0.001f );
		float flOOFilter = 1.0f / flFilter;

		for ( double t = flStartSoundTime; t < flEndSoundTime; t += tStep )
		{
			DmeTime_t tPhonemeTime( t );

			// Determine the location of the sample in the channels clip
			DmeTime_t tKeyTime( t * flScale );
			tKeyTime += tOffset;

			// Write a default value at that time
			WriteDefaultValuesIntoLogLayers( tKeyTime, controlLookup );

			// Walk phonemes...
			for ( i = 0; i < item.m_ApplyTags.Count() ; ++i )
			{
				CBasePhonemeTag *p = item.m_ApplyTags[ i ];

				DmeTime_t tStart = DmeTime_t( p->GetStartTime() );
				DmeTime_t tEnd = DmeTime_t( p->GetEndTime() );

				bool bContinue = false;
				float flI = 0.0f;
				{
					DmeTime_t tFilter( flFilter );
					if ( tStart >= tPhonemeTime + tFilter || tEnd <= tPhonemeTime )
						bContinue = true;

					tStart = max( tStart, tPhonemeTime );
					tEnd = min( tEnd, tPhonemeTime + tFilter );

					flI = ( tEnd - tStart ).GetSeconds() * flOOFilter;
				}

				DmeTime_t dStart = tStart - tPhonemeTime;
				DmeTime_t dEnd = tEnd - tPhonemeTime;

				float t1 = dStart.GetSeconds() * flOOFilter;
				float t2 = dEnd.GetSeconds() * flOOFilter;

				Assert( bContinue == !( t1 < 1.0f && t2 > 0.0f ) );
				if ( !( t1 < 1.0f && t2 > 0.0f ) )
					continue;

				if ( t2 > 1 )
				{
					t2 = 1;
				}
				if ( t1 < 0 )
				{
					t1 = 0;
				}

				float flIntensity = ( t2 - t1 );
				Assert( fabs( flI - flIntensity ) < 0.000001f );

				int idx = phonemeToPresetDict.Find( ConvertPhoneme( p->GetPhonemeCode() ) );
				if ( idx == phonemeToPresetDict.InvalidIndex() )
					continue;

				CDmePreset *preset = phonemeToPresetDict[ idx ];
				if ( !preset )
					continue;

				StampControlValueLogs( preset, tKeyTime, flIntensity, controlLookup );
			}
		}
	}

	// Flatten write layers
	for ( i = 0; i < logs.Count(); ++i )
	{
		logs[ i ]->FlattenLayers( DMELOG_DEFAULT_THRESHHOLD, CDmeLog::FLATTEN_NODISCONTINUITY_FIXUP );
	}
}

void CSFMPhonemeExtractor::ReApply( ExtractDesc_t& info )
{
	if ( info.m_bCreateBookmarks )
	{
		info.m_pSet->GetBookmarks().RemoveAll();
	}

	for ( int nWorkItem = 0; nWorkItem < info.m_WorkList.Count(); ++nWorkItem )
	{
		LogPhonemes( nWorkItem, info );
	}
}

