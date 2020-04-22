//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CWorkspaceBrowser;
class CWorkspaceWorkArea;
class CWorkspace;
class CProject;
class CScene;
class CVCDFile;
class CSoundEntry;
class ITreeItem;
class CSoundBrowser;
class CWaveBrowser;
class CWaveFile;

struct _IMAGELIST;
typedef struct _IMAGELIST NEAR* HIMAGELIST;

enum
{
	IMAGE_WORKSPACE = 0,
	IMAGE_WORKSPACE_CHECKEDOUT,
	IMAGE_PROJECT,
	IMAGE_PROJECT_CHECKEDOUT,
	IMAGE_SCENE,
//	IMAGE_SCENE_CHECKEDOUT,
	IMAGE_VCD,
	IMAGE_VCD_CHECKEDOUT,
	IMAGE_WAV,
	IMAGE_WAV_CHECKEDOUT,
	IMAGE_SPEAK,
	IMAGE_SPEAK_CHECKEDOUT,

	NUM_IMAGES,
};

class CWorkspaceManager : public mxWindow
{
public:
	CWorkspaceManager();
	~CWorkspaceManager();

	virtual int handleEvent( mxEvent *event );

	CWorkspaceBrowser	*GetBrowser();
	CSoundBrowser	*GetSoundBrowser();
	CWaveBrowser	*GetWaveBrowser();
	void			LoadWorkspace( char const *filename );

	void			AutoLoad( char const *workspace );

	void			ShowContextMenu( int x, int y, ITreeItem *item );
	void			OnDoubleClicked( ITreeItem *item );

	void			UpdateMenus();

	virtual bool	Closing();

	HIMAGELIST		CreateImageList();

	void			RefreshBrowsers();

	void			OnSoundShowInBrowsers();

	void			SetWorkspaceDirty();

	int				GetLanguageId() const;

private:

	void			PerformLayout( bool movebrowsers );

	void			Think( float dt );
	void			Frame( void );

	virtual void	OnDelete();

	void			SetWorkspace( CWorkspace *ws );
	void			OnUpdateTitle( void );

	void			CreateFileMenu( mxMenu *m );
	void			CreateProjectMenu( mxMenu *m );

	int				GetMaxRecentFiles( void ) const;

// Workspace message handlers
	void			OnNewWorkspace();
	void			OnOpenWorkspace();
	void			OnCloseWorkspace();
	void			OnSaveWorkspace();

	void			OnChangeVSSProperites();

	void			OnCheckoutWorkspace();
	void			OnCheckinWorkspace();

// Project message handlers
	void			OnNewProject();
	void			OnInsertProject();
	void			OnRemoveProject();
	void			OnModifyProjectComments();

// Scene message handlers
	void			OnNewScene();
	void			OnModifySceneComments();
	void			OnRemoveScene();

// Sound entry handlers
	void			OnSoundPlay();
	void			OnSoundToggleVoiceDuck();
	void			OnSoundEditText();

	void			OnSoundProperties();
	void			OnWaveProperties();

	void			OnCheckout();
	void			OnCheckin();

	void			OnMoveUp();
	void			OnMoveDown();

	//void			OnSoundCheckOut();
	//void			OnSoundCheckIn();

// Scene entries
	void			OnSceneAddVCD();
	void			OnSceneRemoveVCD();
	void			OnModifyVCDComments();

	void			OnRecentWorkspace( int index );
	void			OnChangeLanguage( int lang_index, bool force = false );
	void			AddFileToRecentWorkspaceList( char const *filename );
	void			UpdateRecentFilesMenu();

	void			LoadRecentFilesMenuFromDisk();
	void			SaveRecentFilesMenuToDisk();


	bool			CloseWorkspace();

	void		ShowContextMenu_Workspace( int x, int y, CWorkspace *ws );
	void		ShowContextMenu_Project( int x, int y, CProject *project );
	void		ShowContextMenu_Scene( int x, int y, CScene *scene );
	void		ShowContextMenu_VCD( int x, int y, CVCDFile *vcd );
	void		ShowContextMenu_SoundEntry( int x, int y, CSoundEntry *entry );
	void		ShowContextMenu_WaveFile( int x, int y, CWaveFile *entry );

	mxMenuBar		*m_pMenuBar;

	mxMenu			*m_pFileMenu;
	mxMenu			*m_pRecentFileMenu;
	int				m_nRecentMenuItems;
	mxMenu			*m_pProjectMenu;
	mxMenu			*m_pOptionsMenu;
	mxMenu			*m_pMenuCloseCaptionLanguages;

	CWorkspaceWorkArea	*m_pWorkArea;

	CWorkspaceBrowser	*m_pBrowser;
	CSoundBrowser		*m_pSoundBrowser;
	CWaveBrowser		*m_pWaveBrowser;

	struct RecentFile
	{
		char filename[ 256 ];
	};

	CUtlVector< RecentFile > m_RecentFiles;
	int				m_nLanguageId;
	long			m_lEnglishCaptionsFileChangeTime;
};

CWorkspaceManager *GetWorkspaceManager();

#endif // WORKSPACEMANAGER_H
