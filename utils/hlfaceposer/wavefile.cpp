//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "wavefile.h"
#include "wavebrowser.h"
#include "sentence.h"
#include "ifaceposersound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "snd_wave_source.h"
#include "filesystem.h"
#include "UtlBuffer.h"
#include "phonemeeditor.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CWaveFile::CWaveFile( char const *filename ) 
{
	m_bAsyncLoading = false;

	m_bSentenceLoaded = false;

	m_Sentence.Reset();

	Q_strncpy( m_szName, filename, sizeof( m_szName ) );

	Q_snprintf( m_szFileName, sizeof( m_szFileName ), "sound/%s", filename );
}

CWaveFile::~CWaveFile()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CWaveFile::GetLanguageId()
{
	return GetCloseCaptionLanguageId();
}

bool SceneManager_LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence );

void CWaveFile::EnsureSentence()
{
	if ( m_bSentenceLoaded )
		return;

	m_bSentenceLoaded = true;

	if ( m_szFileName[ 0 ] )
	{
		SceneManager_LoadSentenceFromWavFile( m_szFileName, m_Sentence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveFile::HasLoadedSentenceInfo() const
{
	return m_bSentenceLoaded;
}

char const	*CWaveFile::GetName() const
{
	return m_szName;
}

char const *CWaveFile::GetFileName() const
{
	return m_szFileName;
}

char const	*CWaveFile::GetSentenceText()
{
	EnsureSentence();
	return m_Sentence.GetText();
}

int	CWaveFile::GetPhonemeCount()
{
	EnsureSentence();
	return m_Sentence.CountPhonemes();
}

int	CWaveFile::GetWordCount()
{
	EnsureSentence();
	return m_Sentence.m_Words.Count();
}


void CWaveFile::Play()
{
	Con_Printf( "Playing '%s' : '%s'\n", GetFileName(), GetSentenceText() );

	g_pPhonemeEditor->SetCurrentWaveFile( GetFileName() );
	g_pPhonemeEditor->Play();
}


bool CWaveFile::GetVoiceDuck()
{
	EnsureSentence();
	return m_Sentence.GetVoiceDuck();
}

int CWaveFile::GetIconIndex() const
{
	return 0; // IMAGE_WAV;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sentence - 
//-----------------------------------------------------------------------------
void CWaveFile::SetThreadLoadedSentence( CSentence& sentence )
{
	if ( m_bSentenceLoaded )
		return;

	m_bSentenceLoaded = true;
	m_Sentence = sentence;
}