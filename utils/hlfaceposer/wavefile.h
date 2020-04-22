//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WAVEFILE_H
#define WAVEFILE_H
#ifdef _WIN32
#pragma once
#endif

#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "sentence.h"

class CAudioSource;

class CWaveFile
{
public:
	// One or both may be valid
	CWaveFile( char const *filename );

	~CWaveFile();

	static int GetLanguageId();

	char const	*GetName() const;
	char const	*GetFileName() const;

	char const	*GetSentenceText();

	int			GetPhonemeCount();
	int			GetWordCount();

	bool		IsAsyncLoading() const { return m_bAsyncLoading; }
	void		SetAsyncLoading( bool async ) { m_bAsyncLoading = async; }

	bool		HasLoadedSentenceInfo() const;
	void				EnsureSentence();

	void		Play();

	
	bool		GetVoiceDuck();
	/*
	void		SetVoiceDuck( bool duck );
	void		ToggleVoiceDucking();

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	*/

	int			GetIconIndex() const;

	void				SetThreadLoadedSentence( CSentence& sentence );

//	void				ExportValveDataChunk( char const *tempfile );
//	void				ImportValveDataChunk( char const *tempfile );

//	void				GetPhonemeExportFile( char *path, int maxlen );

private:

	CSentence			m_Sentence;

	enum
	{
		MAX_SOUND_NAME = 256,
		MAX_SCRIPT_FILE = 64,
		MAX_SOUND_FILENAME = 128,
	};

	char				m_szName[ MAX_SOUND_FILENAME ];
	char				m_szFileName[ MAX_SOUND_FILENAME ];

//	CVCDFile			*m_pOwner;
//	CSoundEntry			*m_pOwnerSE;

	bool				m_bSentenceLoaded;
	bool				m_bAsyncLoading;
};

#endif // WAVEFILE_H
