//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WAVEBROWSER_H
#define WAVEBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mxListView.h"
#include "commctrl.h"
#include "utldict.h"

class CWorkspace;
class CProject;
class CScene;
class CVCDFile;
class CWaveFile;

class CWaveList;
class CWorkspaceManager;
class CWaveFilterTab;
class CWaveOptionsWindow;
class CWaveFileTree;

class CWaveBrowser : public mxWindow
{
	typedef mxWindow BaseClass;
public:

	CWaveBrowser( mxWindow *parent, CWorkspaceManager *manager, int id );

	virtual		int handleEvent( mxEvent *event );
	virtual		void OnDelete();

	CWorkspaceManager *GetManager();

	void		RepopulateTree();

	void		BuildSelectionList( CUtlVector< CWaveFile * >& selected );

	void		OnPlay();

	void		JumpToItem( CWaveFile *wav );

	CWaveFile	*FindEntry( char const *wavname, bool jump = false );


	int			GetSoundCount() const;
	CWaveFile	*GetSound( int index );

	void		OnSearch();

private:

	char const	*GetSearchString();

	bool		LoadWaveFilesInDirectory( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName, int nDirectoryNameLen );
	bool		InitDirectoryRecursive( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName );

	void		OnWaveProperties();
	void		OnEnableVoiceDucking();
	void		OnDisableVoiceDucking();
	void		OnCheckout();
	void		OnCheckin();

	void		OnImportSentence();
	void		OnExportSentence();

	void		OnGetSentence();

	void		PopulateTree( char const *subdirectory, bool textsearch = false );

	void		ShowContextMenu( void );

	void		LoadAllSounds();
	void		RemoveAllSounds();

	CWaveList *m_pListView;

	enum
	{
		NUM_BITMAPS = 6,
	};

	CWorkspaceManager *m_pManager;

	CUtlDict< CWaveFile *, int > m_AllSounds;
	CUtlSymbolTable			m_ScriptTable;

	CUtlVector< CUtlSymbol >	m_Scripts;

	CWaveOptionsWindow		*m_pOptions;
	CWaveFileTree		*m_pFileTree;

	CUtlVector< CWaveFile * > m_CurrentSelection;
};

#endif // WAVEBROWSER_H
