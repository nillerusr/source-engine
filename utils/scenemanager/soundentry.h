//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDENTRY_H
#define SOUNDENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "itreeitem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "sentence.h"

class CAudioSource;
class CVCDFile;
class CWaveFile;

class CSoundEntry : public ITreeItem
{
public:
	CSoundEntry( CVCDFile *vcd, char const *name );
	~CSoundEntry();

	CVCDFile	*GetOwnerVCDFile();

	void		SetName( char const *name );

	char const	*GetName() const;

	int			GetWaveCount() const;
	CWaveFile	*GetWave( int index );
	void		ClearWaves();
	void		AddWave( CWaveFile *wave );
	void		RemoveWave( CWaveFile *wave );
	int			FindWave( CWaveFile *wave );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem* parent );

	virtual CWorkspace	*GetWorkspace() { return NULL; }
	virtual CProject	*GetProject() { return NULL; }
	virtual CScene		*GetScene() { return NULL; }
	virtual CVCDFile	*GetVCDFile() { return NULL; }
	virtual CSoundEntry	*GetSoundEntry() { return this; }
	virtual CWaveFile	*GetWaveFile() { return NULL; }

	void		Play();

	char const	*GetScriptFile() const;
	void		SetScriptFile( char const *scriptfile );

	char const	*GetSentenceText( int wavindex );
	void		GetCCText( wchar_t *out, int maxchars );

	CSoundParametersInternal	*GetSoundParameters();

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;


	virtual void MoveChildUp( ITreeItem *child );
	virtual void MoveChildDown( ITreeItem *child );

	virtual void		SetDirty( bool dirty );

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

private:

	CSoundParameters		m_Params;

	enum
	{
		MAX_SOUND_NAME = 256,
		MAX_SCRIPT_FILE = 64,
		MAX_SOUND_FILENAME = 128,
	};

	char				m_szName[ MAX_SOUND_NAME ];
	char				m_szScriptFile[ MAX_SCRIPT_FILE ];

	CVCDFile			*m_pOwner;

	CUtlVector< CWaveFile * >	m_Waves;

	int					m_nLastPlay;
};

#endif // SOUNDENTRY_H
