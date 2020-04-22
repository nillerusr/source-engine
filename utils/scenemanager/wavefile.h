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

#include "itreeitem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "sentence.h"

class CSoundEntry;
class CAudioSource;
class CVCDFile;

class CWaveFile : public ITreeItem
{
public:
	// One or both may be valid
	CWaveFile( CVCDFile *vcd, CSoundEntry *se, char const *filename );

	~CWaveFile();

	static int GetLanguageId();

	CVCDFile	*GetOwnerVCDFile();
	CSoundEntry	*GetOwnerSoundEntry();

	void		SetName( char const *filename );

	char const	*GetName() const;
	char const	*GetFileName() const;

	char const	*GetSentenceText();
	void		SetSentenceText( char const *newText );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem* parent );

	bool		HasLoadedSentenceInfo() const;
	void				EnsureSentence();


	virtual CWorkspace	*GetWorkspace() { return NULL; }
	virtual CProject	*GetProject() { return NULL; }
	virtual CScene		*GetScene() { return NULL; }
	virtual CVCDFile	*GetVCDFile() { return NULL; }
	virtual CSoundEntry	*GetSoundEntry() { return NULL; }
	virtual CWaveFile	*GetWaveFile() { return this; }

	void		Play();

	bool		GetVoiceDuck();
	void		SetVoiceDuck( bool duck );
	void		ToggleVoiceDucking();

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;

	virtual void MoveChildUp( ITreeItem *child );
	virtual void MoveChildDown( ITreeItem *child );

	virtual void		SetDirty( bool dirty );

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

	void				SetThreadLoadedSentence( CSentence& sentence );

	void				ExportValveDataChunk( char const *tempfile );
	void				ImportValveDataChunk( char const *tempfile );

	void				GetPhonemeExportFile( char *path, int maxlen );

private:

	CAudioSource		*m_pWaveFile;
	CSentence			m_Sentence;

	enum
	{
		MAX_SOUND_NAME = 256,
		MAX_SCRIPT_FILE = 64,
		MAX_SOUND_FILENAME = 128,
	};

	char				m_szName[ MAX_SOUND_FILENAME ];
	char				m_szFileName[ MAX_SOUND_FILENAME ];

	CVCDFile			*m_pOwner;
	CSoundEntry			*m_pOwnerSE;

	bool				m_bSentenceLoaded;
};

#endif // WAVEFILE_H
