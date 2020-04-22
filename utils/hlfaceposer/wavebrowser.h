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
#include "faceposertoolwindow.h"

class CWaveFile;
class CWaveList;
class CWaveFileTree;
class CWaveOptionsWindow;
class CChoreoEvent;

struct _IMAGELIST;
typedef struct _IMAGELIST NEAR* HIMAGELIST;

enum
{
///	IMAGE_WORKSPACE = 0,
//	IMAGE_WORKSPACE_CHECKEDOUT,
//	IMAGE_PROJECT,
//	IMAGE_PROJECT_CHECKEDOUT,
//	IMAGE_SCENE,
//	IMAGE_SCENE_CHECKEDOUT,
//	IMAGE_VCD,
//	IMAGE_VCD_CHECKEDOUT,
//	IMAGE_WAV,
//	IMAGE_WAV_CHECKEDOUT,
//	IMAGE_SPEAK,
//	IMAGE_SPEAK_CHECKEDOUT,

	NUM_IMAGES,
};

class CWaveBrowser : public mxWindow, public IFacePoserToolWindow
{
	typedef mxWindow BaseClass;
public:

	CWaveBrowser( mxWindow *parent );

	virtual void		Think( float dt );

	virtual		int handleEvent( mxEvent *event );
	virtual		void OnDelete();

	void		RepopulateTree();

	void		BuildSelectionList( CUtlVector< CWaveFile * >& selected );

	void		OnPlay();

	void		JumpToItem( CWaveFile *wav );

	CWaveFile	*FindEntry( char const *wavname, bool jump = false );


	int			GetSoundCount() const;
	CWaveFile	*GetSound( int index );

	void		OnSearch();
	void		OnCancelSearch();

	HIMAGELIST		CreateImageList();

	void		SetEvent( CChoreoEvent *event );
	void		SetCurrent( char const *fn );

private:

	char const	*GetSearchString();

	bool		LoadWaveFilesInDirectory( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName, int nDirectoryNameLen );
	bool		InitDirectoryRecursive( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName );

	void		OnWaveProperties();
	void		OnEnableVoiceDucking();
	void		OnDisableVoiceDucking();
//	void		OnCheckout();
//	void		OnCheckin();

	void		OnImportSentence();
	void		OnExportSentence();

	void		PopulateTree( char const *subdirectory );

	void		ShowContextMenu( void );

	void		LoadAllSounds();
	void		RemoveAllSounds();

	CWaveList *m_pListView;

	enum
	{
		NUM_BITMAPS = 6,
	};

	CUtlDict< CWaveFile *, int > m_AllSounds;
	CUtlSymbolTable			m_ScriptTable;

	CUtlVector< CUtlSymbol >	m_Scripts;

	CWaveOptionsWindow		*m_pOptions;
	CWaveFileTree		*m_pFileTree;

	CUtlVector< CWaveFile * > m_CurrentSelection;

	int				m_nPrevProcessed;
	bool			m_bTextSearch;
};

extern CWaveBrowser	*g_pWaveBrowser;

#endif // WAVEBROWSER_H
