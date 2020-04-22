//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SFMPHONEMEEXTRACTOR_H
#define SFMPHONEMEEXTRACTOR_H
#ifdef _WIN32
#pragma once
#endif

#include "phonemeextractor/PhonemeExtractor.h"
#include "tier1/UtlString.h"
#include "sentence.h"
#include "dme_controls/logpreview.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeSoundClip;
class CDmeGameSound;
class CDmeAnimationSet;


//-----------------------------------------------------------------------------
// Info about a particular phoneme to extract
//-----------------------------------------------------------------------------
class CExtractInfo
{
public:
	CExtractInfo();
	CExtractInfo( const CExtractInfo& src );
	~CExtractInfo();

	void ClearTags();

	// Filled in by caller
	CDmeSoundClip	*m_pClip;
	CDmeGameSound	*m_pSound; 
	CUtlString		m_sHintText;
	bool			m_bUseSentence;

	// Filled in by Extract()
	CSentence		m_Sentence;
	float			m_flDuration;
	bool			m_bSentenceValid;

	// Must be passed in when calling for Apply, will be created and passed back for Extract
	CUtlVector< CBasePhonemeTag * > m_ApplyTags;
};


//-----------------------------------------------------------------------------
// Extraction type
//-----------------------------------------------------------------------------
enum SFMPhonemeExtractType_t
{
	EXTRACT_WIPE_RANGE = 0,		// Wipe logs from start of first selected clip to end of last selected clip
	EXTRACT_WIPE_CLIP,			// Wipe all log entries (for facial controls) over entire clip
	EXTRACT_WIPE_SOUNDS,		// Leave logs untouched, except underneath each selected .wav file

	NUM_EXTRACT_WIPE_TYPES,
};

//-----------------------------------------------------------------------------
// Filter type
//-----------------------------------------------------------------------------
enum SFMPhonemeFilterType_t
{
	EXTRACT_FILTER_HOLD,		// hold for phoneme duration
	EXTRACT_FILTER_LINEAR,		// linearly blend from phoneme start to next phoneme
	EXTRACT_FILTER_FIXED_WIDTH,	// hold and linearly falloff before and after

	NUM_EXTRACT_FILTER_TYPES,
};

//-----------------------------------------------------------------------------
// Extraction information
//-----------------------------------------------------------------------------
struct ExtractDesc_t
{
	SFMPhonemeExtractType_t m_nExtractType;
	SFMPhonemeFilterType_t m_nFilterType;
	bool m_bCreateBookmarks;
	CUtlVector< CExtractInfo > m_WorkList;		// One or more .wavs to extract from
	CUtlVector< LogPreview_t > m_ControlList;	// List of facial controls
	CDmeFilmClip *m_pMovie;
	CDmeFilmClip *m_pShot;
	CDmeAnimationSet *m_pSet; 
	float m_flSampleRateHz; 
	float m_flSampleFilterSize;
};


//-----------------------------------------------------------------------------
// Main interface for phoneme extraction
//-----------------------------------------------------------------------------
class ISFMPhonemeExtractor
{
public:
	virtual ~ISFMPhonemeExtractor() {};

	virtual bool Init() = 0;
	virtual void Shutdown() = 0;

	virtual int GetAPICount() = 0;
	virtual void GetAPIInfo( int nIndex, CUtlString* pPrintName, PE_APITYPE *pAPIType ) = 0;

	virtual void Extract( const PE_APITYPE& apiType, ExtractDesc_t& info, bool bWritePhonemesToWavFiles = false ) = 0;
	virtual void ReApply( ExtractDesc_t& info ) = 0;
	virtual bool GetSentence( CDmeGameSound *pGameSound, CSentence& sentence ) = 0;
};

extern ISFMPhonemeExtractor *sfm_phonemeextractor;


//-----------------------------------------------------------------------------
// inline methods of CExtractInfo
//-----------------------------------------------------------------------------
inline CExtractInfo::CExtractInfo() : m_pClip( 0 ), m_pSound( 0 ),
	m_bSentenceValid( false ), m_bUseSentence( false ), m_flDuration( 0.0f )
{
}

inline CExtractInfo::CExtractInfo( const CExtractInfo& src )
{
	m_pClip = src.m_pClip;
	m_pSound = src.m_pSound;
	m_sHintText = src.m_sHintText;
	m_Sentence = src.m_Sentence;
	m_bSentenceValid = src.m_bSentenceValid;
	m_bUseSentence = src.m_bUseSentence;
	m_flDuration = src.m_flDuration;

	ClearTags();

	for ( int i = 0; i < src.m_ApplyTags.Count(); ++i )
	{
		CBasePhonemeTag *newTag = new CBasePhonemeTag( *src.m_ApplyTags[ i ] );
		m_ApplyTags.AddToTail( newTag );
	}
}

inline CExtractInfo::~CExtractInfo()
{
	ClearTags();
}

inline void CExtractInfo::ClearTags()
{
	for ( int i = 0; i < m_ApplyTags.Count(); ++i )
	{
		delete m_ApplyTags[ i ];
	}
	m_ApplyTags.RemoveAll();
}


#endif // PHONEMEEXTRACTOR_H
