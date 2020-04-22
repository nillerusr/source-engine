//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDBROWSER_H
#define SOUNDBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mxListView.h"
#include "commctrl.h"
#include "utlsymbol.h"

class CWorkspace;
class CProject;
class CScene;
class CVCDFile;
class CSoundEntry;

class CSoundList;
class CWorkspaceManager;
class CSoundFilterTab;
class COptionsWindow;


class CSoundBrowser : public mxWindow
{
	typedef mxWindow BaseClass;
public:

	CSoundBrowser( mxWindow *parent, CWorkspaceManager *manager, int id );

	virtual		int handleEvent( mxEvent *event );
	virtual		void OnDelete();

	CWorkspaceManager *GetManager();

	void		RepopulateTree();

	void		BuildSelectionList( CUtlVector< CSoundEntry * >& selected );

	void		OnPlay();

	void		JumpToItem( CSoundEntry *se );

	void		OnSearch();

private:

	char const	*GetSearchString();

	void		OnShowInWaveBrowser();
	void		OnSoundProperties();
	void		OnAddSound();
	void		OnRemoveSound();
	void		OnGetSentence();
	void		PopulateTree( bool voiceonly, char const *scriptonly );

	void		ShowContextMenu( void );

	void		LoadAllSounds();
	void		RemoveAllSounds();


	CSoundList *m_pListView;

	enum
	{
		NUM_BITMAPS = 8,
	};

	CWorkspaceManager *m_pManager;

	CUtlVector< CSoundEntry * > m_AllSounds;
	CUtlSymbolTable			m_ScriptTable;

	CUtlVector< CUtlSymbol >	m_Scripts;

	CSoundFilterTab		*m_pFilter;
	COptionsWindow		*m_pOptions;

	CUtlVector< CSoundEntry * > m_CurrentSelection;
};


#endif // SOUNDBROWSER_H
