//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"
#include "workspace.h"
#include "statuswindow.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "drawhelper.h"
#include "InputProperties.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "project.h"
#include "scene.h"
#include <KeyValues.h>
#include "utlbuffer.h"
#include "iscenemanagersound.h"
#include "soundentry.h"
#include "vcdfile.h"
#include "soundbrowser.h"
#include "wavebrowser.h"
#include "VSSProperties.h"
#include "resource.h"
#include "soundproperties.h"
#include "waveproperties.h"
#include "wavefile.h"
#include "ifileloader.h"
#include "MultipleRequest.h"
#include <vgui/ILocalize.h>
#include "tier3/tier3.h"

char g_appTitle[] = "Source Engine SceneManager";
static char g_appTitleFmt[] = "%s - SceneManager Workspace (%s)";
static char g_appTitleFmtModified[] = "%s * - SceneManager Workspace (%s)";

#define RECENT_FILES_FILE "scenemanager.rf"

enum
{
	// Menu options
	IDC_WSM_FILE_EXIT = 1000,

	IDC_WSM_FILE_WS_NEW,
	IDC_WSM_FILE_WS_OPEN,
	IDC_WSM_FILE_WS_CLOSE,
	IDC_WSM_FILE_WS_SAVE,

	IDC_WSM_FILE_WS_REFRESH,

	IDC_WSM_FILE_WS_VSSPROPERTIES,
	IDC_WSM_FILE_WS_CHECKOUT,
	IDC_WSM_FILE_WS_CHECKIN,

	IDC_WSM_FILE_SCENE_NEW, // + prompt for add to active project

	IDC_WSM_PROJECT_PRJ_NEW,   // + Prompt for add to workspace

	IDC_WSM_PROJECT_PRJ_INSERT,
	IDC_WSM_PROJECT_PRJ_REMOVE,
	IDC_WSM_PROJECT_PRJ_MODIFYCOMMENTS,

	IDC_WSM_PROJECT_SCENE_NEW,
	IDC_WSM_SCENE_REMOVE,

	IDC_WSM_SOUNDENTRY_PLAY,
	IDC_WSM_SOUNDENTRY_TOGGLEVOICEDUCK,
	IDC_WSM_SOUNDENTRY_EDITTEXT,
	IDC_WSM_SOUNDENTRY_SHOWINBROWSERS,
	IDC_WSM_SOUNDENTRY_PROPERTIES,

	IDC_WSM_SCENE_VCD_ADD,
	IDC_WSM_SCENE_VCD_REMOVE,
	IDC_WSM_SCENE_EDIT_COMMENTS,

	IDC_WSM_VCD_EDIT_COMMENTS,

	IDC_WSM_SELECTION_CHECKOUT,
	IDC_WSM_SELECTION_CHECKIN,

	IDC_WSM_SELECTION_MOVEUP,
	IDC_WSM_SELECTION_MOVEDOWN,

	IDC_WSM_WAVEFILE_PROPERTIES,

	// Controls
	IDC_WS_BROWSER,
	IDC_WS_SOUNDBROWSER,
	IDC_WS_WAVEBROWSER,

	// These should be the final entriex
	IDC_WSM_FILE_RECENT_WORKSPACE_START = 1500,
	IDC_WSM_FILE_RECENT_WORKSPACE_END = 1510,

	IDC_WSM_OPTIONS_LANGUAGESTART = 1600,
	IDC_WSM_OPTIONS_LANGUAGEEND = 1600 + CC_NUM_LANGUAGES,
};

static CWorkspaceManager *g_pManager = NULL;
CWorkspaceManager *GetWorkspaceManager()
{
	Assert( g_pManager );
	return g_pManager;
}

class CWorkspaceWorkArea : public mxWindow
{
public:
	CWorkspaceWorkArea( mxWindow *parent )
		: mxWindow( parent, 0, 0, 0, 0, "" )
	{
		SceneManager_AddWindowStyle( this, WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
	}

	virtual void redraw()
	{
		CDrawHelper helper( this, RGB( 200, 200, 200 ) );
	}
};

CWorkspaceManager::CWorkspaceManager()
	: mxWindow (0, 0, 0, 0, 0, g_appTitle, mxWindow::Normal)
{
	m_lEnglishCaptionsFileChangeTime = -1L;
	m_nLanguageId = CC_ENGLISH;

	g_pManager = this;

	g_pStatusWindow = new CStatusWindow( this, 0, 0, 1024, 150, "" );
	g_pStatusWindow->setVisible( true );

	m_pWorkArea = new CWorkspaceWorkArea( this );

	Con_Printf( "Initializing\n" );

	Con_Printf( "CSoundEmitterSystemBase::Init()\n" );

	m_nRecentMenuItems = 0;

	// create menu stuff
	m_pMenuBar = new mxMenuBar (this);
	m_pFileMenu = new mxMenu ();
	m_pProjectMenu = new mxMenu ();
	m_pOptionsMenu = new mxMenu();

	{
		m_pMenuCloseCaptionLanguages = new mxMenu();

		for ( int i = 0; i < CC_NUM_LANGUAGES; i++ )
		{
			int id = IDC_WSM_OPTIONS_LANGUAGESTART + i;
			m_pMenuCloseCaptionLanguages->add( CSentence::NameForLanguage( i ), id );
		}
		// Check the first item
		m_pMenuCloseCaptionLanguages->setChecked( IDC_WSM_OPTIONS_LANGUAGESTART + CC_ENGLISH, true );
		m_pOptionsMenu->addMenu( "CC Language", m_pMenuCloseCaptionLanguages );

	}

	m_pMenuBar->addMenu ("File", m_pFileMenu );
	m_pMenuBar->addMenu( "Project", m_pProjectMenu );
	m_pMenuBar->addMenu ( "Options", m_pOptionsMenu );
	
	CreateFileMenu( m_pFileMenu );
	CreateProjectMenu( m_pProjectMenu );

	setMenuBar ( m_pMenuBar );

	Con_Printf( "Creating browser\n" );

	m_pBrowser = new CWorkspaceBrowser( m_pWorkArea, this, IDC_WS_BROWSER );

	m_pSoundBrowser = NULL;
	m_pWaveBrowser = NULL;

	int w = 1280;
	setBounds (10, 10, w, 960);

	setVisible (true);

	Con_Printf( "Creating wave browser\n" );

	m_pWaveBrowser = new CWaveBrowser( m_pWorkArea, this, IDC_WS_WAVEBROWSER );

	Con_Printf( "Creating sound browser\n" );

	m_pSoundBrowser = new CSoundBrowser( m_pWorkArea, this, IDC_WS_SOUNDBROWSER );

	GetBrowser()->SetWorkspace( NULL );

	// See if command line requested workspace

	// Default layout
	PerformLayout( true );

	LoadRecentFilesMenuFromDisk();
}

CWorkspaceManager::~CWorkspaceManager()
{
}

bool CWorkspaceManager::Closing()
{
	Con_Printf( "Checking for sound script changes...\n" );

	MultipleRequestChangeContext();

	// Save any changed sound script files
	int c = g_pSoundEmitterSystem->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		if ( !g_pSoundEmitterSystem->IsSoundScriptDirty( i ) )
			continue;

		char const *scriptname = g_pSoundEmitterSystem->GetSoundScriptName( i );
		if ( !scriptname )
			continue;

		Assert( filesystem->FileExists( scriptname ) );

		bool save = true;

		if ( filesystem->FileExists( scriptname ) )
		{
			if ( !filesystem->IsFileWritable( scriptname ) )
			{
				int retval = MultipleRequest( va( "Check out '%s'?", scriptname ));
				// Cancel
				if ( retval == 2 ) 
					return false;

				if ( retval == 0 )
				{
					VSS_Checkout( scriptname );

					if ( !filesystem->IsFileWritable( scriptname ) )
					{
						mxMessageBox( NULL, va( "Aborting shutdown, %s, not writable!!", scriptname ), g_appTitle, MX_MB_OK );
						return false;
					}
				}
				else if ( retval == 1 )
				{
					// Don't save
					save = false;
				}
			}

			if ( filesystem->IsFileWritable( scriptname ) )
			{
				int retval = mxMessageBox( NULL, va( "Save changes to out '%s'?", scriptname ), g_appTitle, MX_MB_YESNOCANCEL );
				// Cancel
				if ( retval == 2 ) 
					return false;

				if ( retval == 0 )
				{
					if ( !filesystem->IsFileWritable( scriptname ) )
					{
						mxMessageBox( NULL, va( "Aborting shutdown, %s, not writable!!", scriptname ), g_appTitle, MX_MB_OK );
						return false;
					}
				}
				else if ( retval == 1 )
				{
					// Skip changes!!!
					save = false;
				}
			}
		}

		// Try and save it
		if ( !save )
			continue;

		g_pSoundEmitterSystem->SaveChangesToSoundScript( i );
	}

	return CloseWorkspace();
}

void CWorkspaceManager::OnDelete()
{
	SaveRecentFilesMenuToDisk();
}

void CWorkspaceManager::CreateFileMenu( mxMenu *m )
{
	m->add ("&New Workspace", IDC_WSM_FILE_WS_NEW );
	m->addSeparator();
	m->add ("Open &Workspace...", IDC_WSM_FILE_WS_OPEN );
	m->add ("Sa&ve Workspace", IDC_WSM_FILE_WS_SAVE );
	m->add ("Close Wor&kspace", IDC_WSM_FILE_WS_CLOSE );
	m->addSeparator();
	m->add ("&Create New Scene...", IDC_WSM_FILE_SCENE_NEW );
	m->addSeparator();

	m->add( "V&SS Properties...", IDC_WSM_FILE_WS_VSSPROPERTIES );
	m->addSeparator();

	m->add( "Refresh\tF5", IDC_WSM_FILE_WS_REFRESH );

	m->addSeparator();

	m_pRecentFileMenu = new mxMenu ();
	m->addMenu ("Recent Files", m_pRecentFileMenu);

	m->addSeparator();
	m->add ("E&xit", IDC_WSM_FILE_EXIT );
}

void CWorkspaceManager::UpdateMenus()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();

	ITreeItem *item = GetBrowser()->GetSelectedItem();

	bool hasworkspace = ws != NULL ? true : false;
	//bool workspacedirty = ws && ws->IsDirty() ? true : false;
	bool hasproject = item && item->GetProject() ? true : false;

	m_pMenuBar->setEnabled( IDC_WSM_FILE_WS_SAVE, true );
	m_pMenuBar->setEnabled( IDC_WSM_FILE_WS_CLOSE, hasworkspace );

	
	if ( hasproject )
	{
		m_pMenuBar->modify( IDC_WSM_FILE_SCENE_NEW, IDC_WSM_FILE_SCENE_NEW, va( "&Create Scene in '%s'", item->GetProject()->GetName() ) );
	}
	else
	{
		m_pMenuBar->modify( IDC_WSM_FILE_SCENE_NEW, IDC_WSM_FILE_SCENE_NEW, "&Create New Scene..." );
	}
	m_pMenuBar->setEnabled( IDC_WSM_FILE_SCENE_NEW, hasproject );

}

void CWorkspaceManager::CreateProjectMenu( mxMenu *m )
{
	m->add ("Create New Project...", IDC_WSM_PROJECT_PRJ_NEW );
	m->addSeparator();
	m->add ("Insert Project...", IDC_WSM_PROJECT_PRJ_INSERT );
	m->add ("Remove Project...", IDC_WSM_PROJECT_PRJ_REMOVE );
	m->addSeparator();
	m->add ("Properties...", IDC_WSM_PROJECT_PRJ_MODIFYCOMMENTS );
}

CWorkspaceBrowser *CWorkspaceManager::GetBrowser()
{
	return m_pBrowser;
}

CSoundBrowser *CWorkspaceManager::GetSoundBrowser()
{
	return m_pSoundBrowser;
}

CWaveBrowser *CWorkspaceManager::GetWaveBrowser()
{
	return m_pWaveBrowser;
}

int CWorkspaceManager::GetMaxRecentFiles() const
{
	return IDC_WSM_FILE_RECENT_WORKSPACE_END - IDC_WSM_FILE_RECENT_WORKSPACE_START;
}

#define MAX_FPS 250.0f
#define MIN_TIMESTEP ( 1.0f / MAX_FPS )

double realtime = 0.0f;

void CWorkspaceManager::Think( float dt )
{
	sound->Update( dt );
	int c = fileloader->ProcessCompleted();
	if ( c > 0 )
	{
		RefreshBrowsers();
		Con_Printf( "Thread loaded %i sounds\n", c );
	}
}

void CWorkspaceManager::Frame( void )
{
	static bool recursion_guard = false;

	static double prev = 0.0;
	double curr = (double) mx::getTickCount () / 1000.0;
	double dt = ( curr - prev );
	
	if ( recursion_guard )
		return;

	recursion_guard = true;

	// clamp to MAX_FPS
	if ( dt >= 0.0 && dt < MIN_TIMESTEP )
	{
		Sleep( max( 0, (int)( ( MIN_TIMESTEP - dt ) * 1000.0f ) ) );

		recursion_guard = false;
		return;
	}

	if ( prev != 0.0 )
	{
		dt = min( 0.1, dt );
		
		Think( dt );

		realtime += dt;
	}
	
	prev = curr;

	recursion_guard = false;
}

int CWorkspaceManager::handleEvent( mxEvent *event )
{
	int iret = 0;
	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Activate:
		{
			if (event->action)
			{
				mx::setIdleWindow( this );
				// Force reload of localization data
				OnChangeLanguage( GetLanguageId(), true );
			}
			else
			{
				mx::setIdleWindow( 0 );
			}
			iret = 1;
		}
		break;
	case mxEvent::Idle:
		{
			iret = 1;
			Frame();
		}
		break;
	case mxEvent::KeyUp:
		{
			if ( event->key == VK_F5 )
			{
				RefreshBrowsers();
			}
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				{
					if ( event->action >= IDC_WSM_FILE_RECENT_WORKSPACE_START && 
						 event->action < IDC_WSM_FILE_RECENT_WORKSPACE_END )
					{
						OnRecentWorkspace( event->action - IDC_WSM_FILE_RECENT_WORKSPACE_START );
					}
					else if ( event->action >= IDC_WSM_OPTIONS_LANGUAGESTART &&
						event->action < IDC_WSM_OPTIONS_LANGUAGEEND	)
					{
						OnChangeLanguage( event->action - IDC_WSM_OPTIONS_LANGUAGESTART );
					}
					else
					{
						iret = 0;
					}
				}
				break;
			case IDC_WSM_FILE_EXIT:
				{
					mx::quit();
				}
				break;
			case IDC_WSM_FILE_WS_REFRESH:
				{
					RefreshBrowsers();
				}
				break;
			case IDC_WSM_FILE_WS_NEW:
				{
					OnNewWorkspace();
				}
				break;
			case IDC_WSM_FILE_WS_OPEN:
				{
					OnOpenWorkspace();
				}
				break;
			case IDC_WSM_FILE_WS_CLOSE:
				{
					OnCloseWorkspace();
				}
				break;
			case IDC_WSM_FILE_WS_SAVE:
				{
					OnSaveWorkspace();
				}
				break;
			case IDC_WSM_FILE_WS_VSSPROPERTIES:
				{
					OnChangeVSSProperites();
				}
				break;
			case IDC_WSM_FILE_WS_CHECKOUT:
				{
					OnCheckoutWorkspace();
				}
				break;
			case IDC_WSM_FILE_WS_CHECKIN:
				{
					OnCheckinWorkspace();
				}
				break;
			case IDC_WSM_PROJECT_PRJ_NEW:
				{
					OnNewProject();
				}
				break;
			case IDC_WSM_PROJECT_PRJ_INSERT:
				{
					OnInsertProject();
				}
				break;
			case IDC_WSM_PROJECT_PRJ_REMOVE:
				{
					OnRemoveProject();
				}
				break;
			case IDC_WSM_PROJECT_PRJ_MODIFYCOMMENTS:
				{
					OnModifyProjectComments();
				}
				break;
			case IDC_WSM_PROJECT_SCENE_NEW:
				{
					OnNewScene();
				}
				break;
			case IDC_WSM_SOUNDENTRY_PLAY:
				{
					OnSoundPlay();
				}
				break;
			case IDC_WSM_SOUNDENTRY_TOGGLEVOICEDUCK:
				{
					OnSoundToggleVoiceDuck();
				}
				break;
			case IDC_WSM_SOUNDENTRY_EDITTEXT:
				{
					OnSoundEditText();
				}
				break;
			case IDC_WSM_SOUNDENTRY_PROPERTIES:
				{
					OnSoundProperties();
				}
				break;
			case  IDC_WSM_SCENE_VCD_ADD:
				{
					OnSceneAddVCD();
				}
				break;
			case IDC_WSM_SCENE_VCD_REMOVE:
				{
					OnSceneRemoveVCD();
				}
				break;
			case IDC_WSM_SCENE_EDIT_COMMENTS:
				{
					OnModifySceneComments();
				}
				break;
			case IDC_WSM_VCD_EDIT_COMMENTS:
				{
					OnModifyVCDComments();
				}
				break;
			case IDC_WSM_SCENE_REMOVE:
				{
					OnRemoveScene();
				}
				break;
			case IDC_WSM_SOUNDENTRY_SHOWINBROWSERS:
				{
					OnSoundShowInBrowsers();
				}
				break;
			case IDC_WSM_SELECTION_CHECKOUT:
				{
					OnCheckout();
				}
				break;
			case IDC_WSM_SELECTION_CHECKIN:
				{
					OnCheckin();
				}
				break;
			case IDC_WSM_SELECTION_MOVEUP:
				{
					OnMoveUp();
				}
				break;
			case IDC_WSM_SELECTION_MOVEDOWN:
				{
					OnMoveDown();
				}
				break;
			case IDC_WSM_WAVEFILE_PROPERTIES:
				{
					OnWaveProperties();
				}
				break;
			}
		}
		break;
	case mxEvent::Size:
		{
			iret = 1;
			PerformLayout( false );
		}
		break;
	}

	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::PerformLayout( bool movebrowsers )
{
	int width = w2();
	int height = h2();

	int statush = 100;
	m_pWorkArea->setBounds( 0, 0, width, height - statush );
	if ( movebrowsers )
	{
		GetBrowser()->setBounds( 0, 0, m_pWorkArea->w2(), m_pWorkArea->h2() / 3 );
		if ( GetSoundBrowser() )
		{
			GetSoundBrowser()->setBounds( 0, m_pWorkArea->h2() / 3, m_pWorkArea->w2(), m_pWorkArea->h2() / 3 );
		}
		if ( GetWaveBrowser() ) 
		{
			GetWaveBrowser()->setBounds( 0, 2 * m_pWorkArea->h2() / 3, m_pWorkArea->w2(), m_pWorkArea->h2() / 3 );
		}
	}

	g_pStatusWindow->setBounds( 0, height - statush, width, statush );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWorkspaceManager::CloseWorkspace()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
		return true;

	if ( !ws->CanClose() )
		return false;

	delete ws;

	SetWorkspace( NULL );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnUpdateTitle( void )
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();

	char szTitle[ 256 ];
	if ( ws )
	{
		char const *fmt = g_appTitleFmt;
		if ( ws->IsDirty() )
		{
			fmt = g_appTitleFmtModified;
		}
		Q_snprintf( szTitle, sizeof( szTitle ), fmt, ws->GetName(), CSentence::NameForLanguage( m_nLanguageId ) );
	}
	else
	{
		Q_snprintf( szTitle, sizeof( szTitle ), g_appTitle );
	}

	setLabel( szTitle );

	UpdateMenus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ws - 
//-----------------------------------------------------------------------------
void CWorkspaceManager::SetWorkspace( CWorkspace *ws )
{
	GetBrowser()->SetWorkspace( ws );

	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnNewWorkspace()
{
	// Show file io
	const char *fullpath = mxGetSaveFileName( 
		0, 
		".", 
		"*.vsw" );

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	// Strip game directory and slash
	char workspace_name[ 512 ];
	filesystem->FullPathToRelativePath( fullpath, workspace_name, sizeof( workspace_name ) );

	Q_StripExtension( workspace_name, workspace_name, sizeof( workspace_name ) );
	Q_DefaultExtension( workspace_name, ".vsw", sizeof( workspace_name ) );

	if ( filesystem->FileExists( workspace_name ) )
	{
		Con_Printf( "%s exists already!\n", workspace_name );
		Assert( 0 );
		return;
	}

	LoadWorkspace( workspace_name );
}

void CWorkspaceManager::AddFileToRecentWorkspaceList( char const *filename )
{
	int i;
	int c = m_RecentFiles.Count();

	for ( i = 0; i < c; i++ )
	{
		if (!Q_stricmp( m_RecentFiles[i].filename, filename ))
			break;
	}

	// swap existing recent file
	if ( i < c )
	{
		RecentFile rf = m_RecentFiles[0];
		m_RecentFiles[ 0 ] = m_RecentFiles[ i ];
		m_RecentFiles[ i ] = rf;
	}
	// insert recent file
	else
	{
		RecentFile rf;
		Q_strcpy( rf.filename, filename );

		m_RecentFiles.AddToHead( rf );
	}

	while( m_RecentFiles.Count() > GetMaxRecentFiles() )
	{
		m_RecentFiles.Remove( m_RecentFiles.Count() - 1 );
	}

	UpdateRecentFilesMenu();
}

void CWorkspaceManager::LoadRecentFilesMenuFromDisk()
{
	KeyValues *kv = new KeyValues( "recentfiles" );
	if ( kv->LoadFromFile( filesystem, RECENT_FILES_FILE ) )
	{
		for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			RecentFile rf;
			Q_strncpy( rf.filename, sub->GetString(), sizeof( rf ) );
			m_RecentFiles.AddToTail( rf );
		}
	}
	kv->deleteThis();

	UpdateRecentFilesMenu();
}

void CWorkspaceManager::AutoLoad( char const *workspace )
{
	if ( workspace )
	{
		LoadWorkspace( workspace );
	}
	else
	{
		if ( m_RecentFiles.Count() > 0 )
		{
			LoadWorkspace( m_RecentFiles[ 0 ].filename );
		}
	}
}

void CWorkspaceManager::SaveRecentFilesMenuToDisk()
{
	int i;
	int c = m_RecentFiles.Count();

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "recentfiles\n{\n" );

	for ( i = 0; i < c; i++ )
	{
		buf.Printf( "\t\"%i\"\t\"%s\"\n", i + 1, m_RecentFiles[ i ].filename );
	}

	buf.Printf( "}\n" );

	char const *recentfiles = RECENT_FILES_FILE;

	// Write it out baby
	FileHandle_t fh = filesystem->Open( recentfiles, "wt" );
	if (fh)
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);
	}
	else
	{
		Con_Printf( "CWorkspace::SaveRecentFilesMenuToDisk:  Unable to write file %s!!!\n", recentfiles );
	}
}

void CWorkspaceManager::UpdateRecentFilesMenu()
{
	int c = m_RecentFiles.Count();

	while ( c > m_nRecentMenuItems )
	{
		m_pRecentFileMenu->add( "(empty)", IDC_WSM_FILE_RECENT_WORKSPACE_START + m_nRecentMenuItems++ );
	}

	for (int i = 0; i < c; i++)
	{
		m_pMenuBar->modify (IDC_WSM_FILE_RECENT_WORKSPACE_START + i, IDC_WSM_FILE_RECENT_WORKSPACE_START + i, m_RecentFiles[i].filename );
	}
}

void CWorkspaceManager::LoadWorkspace( char const *filename )
{
	if ( !CloseWorkspace() )
		return;

	Con_Printf( "Loading workspace %s\n", filename );
	
	CWorkspace *wks = new CWorkspace( filename );
	SetWorkspace( wks );

	AddFileToRecentWorkspaceList( filename );

	OnUpdateTitle();

	UpdateMenus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnOpenWorkspace()
{
	// Show file io
	const char *fullpath = mxGetOpenFileName( 
		0, 
		".", 
		"*.vsw" );

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	// Strip game directory and slash
	char workspace_name[ 512 ];
	filesystem->FullPathToRelativePath( fullpath, workspace_name, sizeof( workspace_name ) );

	LoadWorkspace( workspace_name );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnCloseWorkspace()
{
	if ( !CloseWorkspace() )
		return;

	Con_Printf( "Closed workspace\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnSaveWorkspace()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( ws )
	{
		ws->SaveChanges();

		OnUpdateTitle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnNewProject()
{
	Con_Printf( "OnNewProject()\n" );

	// Show file io
	const char *fullpath = mxGetSaveFileName( 
		0, 
		".", 
		"*.vsp" );

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	// Strip game directory and slash
	char project_name[ 512 ];
	filesystem->FullPathToRelativePath( fullpath, project_name, sizeof( project_name ) );

	Q_StripExtension( project_name, project_name, sizeof( project_name ) );
	Q_DefaultExtension( project_name, ".vsp", sizeof( project_name ) );

	if ( filesystem->FileExists( project_name ) )
	{
		Con_Printf( "%s exists already!\n", project_name );
		Assert( 0 );
		return;
	}

	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		// Create one on the fly
		char workspace_name[ 512 ];
		Q_StripExtension( project_name, workspace_name, sizeof( workspace_name ) );
		Q_DefaultExtension( workspace_name, ".vsw", sizeof( workspace_name ) );

		if ( !filesystem->FileExists( workspace_name ) )
		{
			int retval = mxMessageBox( NULL, va( "Automatically create workspace %s?", workspace_name ), g_appTitle, MX_MB_YESNOCANCEL );
			if ( retval != 0 )
			{
				Con_Printf( "Canceling project creation\n" );
				return;
			}
		}
		else
		{
			Con_Printf( "Found workspace '%s', automatically loading...\n", workspace_name );
		}

		LoadWorkspace( workspace_name );

		ws = GetBrowser()->GetWorkspace();
	}

	if ( ws && ws->FindProjectFile( project_name ) )
	{
		Con_Printf( "Project %s already exists in workspace\n", project_name );
		return;
	}

	// Create a new project and add it into current workspace
	CProject *proj = new CProject( ws, project_name );
	Assert( proj );
	Assert( ws );

	if ( ws )
	{
		GetBrowser()->AddProject( proj );
	}

	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnInsertProject()
{
	Con_Printf( "OnInsertProject()\n" );

	// Show file io
	const char *fullpath = mxGetOpenFileName( 
		0, 
		".", 
		"*.vsp" );

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	// Strip game directory and slash
	char project_name[ 512 ];
	filesystem->FullPathToRelativePath( fullpath, project_name, sizeof( project_name ) );

	Q_StripExtension( project_name, project_name, sizeof( project_name ) );
	Q_DefaultExtension( project_name, ".vsp", sizeof( project_name ) );

	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		// Create one on the fly
		char workspace_name[ 512 ];
		Q_StripExtension( project_name, workspace_name, sizeof( workspace_name ) );
		Q_DefaultExtension( workspace_name, ".vsw", sizeof( workspace_name ) );

		if ( !filesystem->FileExists( workspace_name ) )
		{
			int retval = mxMessageBox( NULL, va( "Automatically create workspace %s?", workspace_name ), g_appTitle, MX_MB_YESNOCANCEL );
			if ( retval != 0 )
			{
				Con_Printf( "Canceling project creation\n" );
				return;
			}
		}
		else
		{
			Con_Printf( "Found workspace '%s', automatically loading...\n", workspace_name );
		}

		LoadWorkspace( workspace_name );

		ws = GetBrowser()->GetWorkspace();
	}

	if ( ws && ws->FindProjectFile( project_name ) )
	{
		Con_Printf( "Project %s already exists in workspace\n", project_name );
		return;
	}

	// Create a new project and add it into current workspace
	CProject *proj = new CProject( ws, project_name );
	Assert( proj );
	Assert( ws );

	if ( ws )
	{
		GetBrowser()->AddProject( proj );
	}

	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnRemoveProject()
{
	Con_Printf( "OnRemoveProject()\n" );

	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CProject *project = item->GetProject();
	if ( !project )
	{
		Con_Printf( "Can't remove project, item is not a project\n" );
		return;
	}

	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		Con_Printf( "Can't remove project '%s', no current workspace?!\n",
			project->GetName() );
		return;
	}

	ws->RemoveProject( project );
	Con_Printf( "Removed project '%s'\n", project->GetName() );
	delete project;

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnRemoveScene()
{
	Con_Printf( "OnRemoveScene()\n" );

	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CScene *scene = item->GetScene();
	if ( !scene )
	{
		Con_Printf( "Can't remove scene, item is not a scene\n" );
		return;
	}

	CProject *project = scene->GetOwnerProject();
	if ( !project )
	{
		Con_Printf( "Can't remove scene '%s', no current owner project?!\n",
			scene->GetName() );
		return;
	}

	project->RemoveScene( scene );
	Con_Printf( "Removed scene '%s'\n", scene->GetName() );
	delete scene;

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnNewScene()
{
	Con_Printf( "OnNewScene()\n" );

	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CProject *project = item->GetProject();
	if ( !project )
	{
		Con_Printf( "Can't add new scene, selected item is not a project\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Create scene in '%s'", project->GetName() );
	Q_strcpy( params.m_szPrompt, "Scene Name:" );
	Q_strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( !params.m_szInputText[ 0 ] )
	{
		Con_Printf( "You must name the scene!\n" );
		return;
	}

	CScene *scene = new CScene( project, params.m_szInputText );
	Assert( scene );

	project->AddScene( scene );

	Con_Printf( "Added scene '%s' to project '%s'\n", scene->GetName(),
		project->GetName() );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnModifyProjectComments()
{
	Con_Printf( "OnModifyProjectComments()\n" );

	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CProject *project = item->GetProject();
	if ( !project )
	{
		Con_Printf( "Can't modify comments, item is not a project\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Edit comments for '%s'", project->GetName() );
	Q_strcpy( params.m_szPrompt, "Comments:" );
	Q_strncpy( params.m_szInputText, project->GetComments(), sizeof( params.m_szInputText ) );

	if ( !InputProperties( &params ) )
		return;

	if ( !Q_strcmp( params.m_szInputText, project->GetComments() ) )
		return;

	project->SetComments( params.m_szInputText );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnRecentWorkspace( int index )
{
	if ( index < 0 || index >= m_RecentFiles.Count() )
		return;

	LoadWorkspace( m_RecentFiles[ index ].filename );
}

int	 CWorkspaceManager::GetLanguageId() const
{
	return m_nLanguageId;
}

void CWorkspaceManager::OnChangeLanguage( int lang_index, bool force /* = false */ )
{
	bool changed = m_nLanguageId != lang_index;
	m_nLanguageId = lang_index;

	if ( changed || force )
	{
		// Update the menu
		for ( int i = 0; i < CC_NUM_LANGUAGES; i++ )
		{
			int id = IDC_WSM_OPTIONS_LANGUAGESTART + i;
			m_pMenuCloseCaptionLanguages->setChecked( id, i == m_nLanguageId );
		}

		bool filechanged = false;

		char const *suffix = CSentence::NameForLanguage( lang_index );
		if ( Q_stricmp( suffix, "unknown_language" ) )
		{
			char fn[ MAX_PATH ];
			Q_snprintf( fn, sizeof( fn ), "resource/closecaption_%s.txt", suffix );

			long filetimestamp = filesystem->GetFileTime( fn );

			
			if ( filesystem->FileExists( fn ) )
			{
				if ( m_lEnglishCaptionsFileChangeTime != filetimestamp )
				{
					filechanged = true;
					m_lEnglishCaptionsFileChangeTime = filetimestamp;
					//Con_Printf( "Reloading close caption data from '%s'\n", fn );
					g_pVGuiLocalize->RemoveAll();
					g_pVGuiLocalize->AddFile( fn );
				}
			}
			else
			{
				Con_Printf( "CWorkspaceManager::OnChangeLanguage  Warning, can't find localization file %s\n", fn );
			}
		}

		if ( !force || filechanged )
		{
			// Update all text for items.
			RefreshBrowsers();

			OnUpdateTitle();
		}
	}
}

void CWorkspaceManager::OnDoubleClicked( ITreeItem *item )
{
	CSoundEntry *s = item->GetSoundEntry();
	if ( s )
	{
		s->Play();
		return;
	}
}

void CWorkspaceManager::OnSoundPlay()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( item->GetSoundEntry() )
	{
		item->GetSoundEntry()->Play();
	}
	else if ( item->GetWaveFile() )
	{
		item->GetWaveFile()->Play();
	}
}

void CWorkspaceManager::OnSoundToggleVoiceDuck()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( item->GetWaveFile() )
	{
		item->GetWaveFile()->ToggleVoiceDucking();
	}
}

void CWorkspaceManager::OnSoundEditText()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CWaveFile *s = item->GetWaveFile();
	if ( !s )
		return;

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Edit text of '%s'", s->GetName() );
	Q_strcpy( params.m_szPrompt, "Sentence text:" );
	V_strcpy_safe( params.m_szInputText, s->GetSentenceText() );

	if ( !InputProperties( &params ) )
		return;

	if ( !Q_stricmp( params.m_szInputText, s->GetSentenceText() ) )
	{
		return;
	}

	s->SetSentenceText( params.m_szInputText );
	
	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

// Scene entries
void CWorkspaceManager::OnSceneAddVCD()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CScene *scene = item->GetScene();
	if ( !scene )
		return;

	// Show file io
	const char *fullpath = mxGetOpenFileName( 
		0, 
		".", 
		"*.vcd" );

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	// Strip game directory and slash
	char vcd_name[ 512 ];
	filesystem->FullPathToRelativePath( fullpath, vcd_name, sizeof( vcd_name ) );

	Q_StripExtension( vcd_name, vcd_name, sizeof( vcd_name ) );
	Q_DefaultExtension( vcd_name, ".vcd", sizeof( vcd_name ) );

	if ( scene->FindVCD( vcd_name ) )
	{
		Con_Printf( "File '%s' is already in scene '%s'\n", vcd_name, scene->GetName() );
		return;
	}

	CVCDFile *vcd = new CVCDFile( scene, vcd_name );

	scene->AddVCD( vcd );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnModifySceneComments()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CScene *scene = item->GetScene();
	if ( !scene )
		return;

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Edit comments for '%s'", scene->GetName() );
	Q_strcpy( params.m_szPrompt, "Comments:" );
	Q_strncpy( params.m_szInputText, scene->GetComments(), sizeof( params.m_szInputText ) );

	if ( !InputProperties( &params ) )
		return;

	if ( !Q_strcmp( params.m_szInputText, scene->GetComments() ) )
		return;

	scene->SetComments( params.m_szInputText );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnModifyVCDComments()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CVCDFile *file = item->GetVCDFile();
	if ( !file )
		return;

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Edit comments for '%s'", file->GetName() );
	Q_strcpy( params.m_szPrompt, "Comments:" );
	Q_strncpy( params.m_szInputText, file->GetComments(), sizeof( params.m_szInputText ) );

	if ( !InputProperties( &params ) )
		return;

	if ( !Q_strcmp( params.m_szInputText, file->GetComments() ) )
		return;

	file->SetComments( params.m_szInputText );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnSceneRemoveVCD()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CVCDFile *file = item->GetVCDFile();
	if ( !file )
		return;

	CScene *scene = file->GetOwnerScene();
	if ( !scene )
		return;

	scene->RemoveVCD( file );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::ShowContextMenu( int x, int y, ITreeItem *p )
{
	CWorkspace *w = p->GetWorkspace();
	if ( w )
	{
		ShowContextMenu_Workspace( x, y, w );
		return;
	}

	CProject *proj = p->GetProject();
	if ( proj )
	{
		ShowContextMenu_Project( x, y, proj );
		return;
	}

	CScene *scene = p->GetScene();
	if ( scene )
	{
		ShowContextMenu_Scene( x, y, scene );
		return;
	}

	CVCDFile *vcd = p->GetVCDFile();
	if ( vcd )
	{
		ShowContextMenu_VCD( x, y, vcd );
		return;
	}

	CSoundEntry *sound = p->GetSoundEntry();
	if ( sound )
	{
		ShowContextMenu_SoundEntry( x, y, sound );
		return;
	}

	CWaveFile *wave = p->GetWaveFile();
	if ( wave )
	{
		ShowContextMenu_WaveFile( x, y, wave );
		return;
	}

	Con_Printf( "unknown tree item type\n" );
	Assert( 0 );
}

void CWorkspaceManager::ShowContextMenu_Workspace( int x, int y, CWorkspace *ws )
{
	if ( !ws )
		return;

	// New project and insert project
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add ("Create New Project...", IDC_WSM_PROJECT_PRJ_NEW );
	pop->add ("Insert Project...", IDC_WSM_PROJECT_PRJ_INSERT );
	pop->addSeparator();
	pop->add( "VSS Properties...", IDC_WSM_FILE_WS_VSSPROPERTIES );

	if ( !filesystem->IsFileWritable( ws->GetFileName() ) )
	{
		pop->add( va( "Checkout '%s'", ws->GetName() ), IDC_WSM_SELECTION_CHECKOUT );
	}
	else
	{
		pop->add( va( "Checkin '%s'", ws->GetName() ), IDC_WSM_SELECTION_CHECKIN );
	}

	pop->popup( this, x, y );
}

void CWorkspaceManager::ShowContextMenu_Project( int x, int y, CProject *project )
{
	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add ("Create New Scene...", IDC_WSM_PROJECT_SCENE_NEW );
	pop->addSeparator();
	pop->add ("Remove Project...", IDC_WSM_PROJECT_PRJ_REMOVE );
	pop->addSeparator();
	pop->add( "Edit comments...", IDC_WSM_PROJECT_PRJ_MODIFYCOMMENTS );
	if ( !project->IsFirstChild() || !project->IsLastChild() )
	{
		pop->addSeparator();
		if( !project->IsFirstChild() )
		{
			pop->add( va( "Move '%s' Up", project->GetName() ), IDC_WSM_SELECTION_MOVEUP );
		}
		if( !project->IsLastChild() ) 
		{
			pop->add( va( "Move '%s' Down", project->GetName() ), IDC_WSM_SELECTION_MOVEDOWN );
		}
	}
	pop->addSeparator();

	if ( !filesystem->IsFileWritable( project->GetFileName() ) )
	{
		pop->add( va( "Checkout '%s'", project->GetName() ), IDC_WSM_SELECTION_CHECKOUT );
	}
	else
	{
		pop->add( va( "Checkin '%s'", project->GetName() ), IDC_WSM_SELECTION_CHECKIN );
	}

	pop->popup( this, x, y );
}

void CWorkspaceManager::ShowContextMenu_Scene( int x, int y, CScene *scene )
{
	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add( va( "Add VCD to '%s'...", scene->GetName() ), IDC_WSM_SCENE_VCD_ADD );
	pop->addSeparator();
	pop->add ("Remove Scene", IDC_WSM_SCENE_REMOVE );
	pop->addSeparator();
	pop->add( va( "Edit comments..." ), IDC_WSM_SCENE_EDIT_COMMENTS );
	if ( !scene->IsFirstChild() || !scene->IsLastChild() )
	{
		pop->addSeparator();
		if( !scene->IsFirstChild() )
		{
			pop->add( va( "Move '%s' Up", scene->GetName() ), IDC_WSM_SELECTION_MOVEUP );
		}
		if( !scene->IsLastChild() ) 
		{
			pop->add( va( "Move '%s' Down", scene->GetName() ), IDC_WSM_SELECTION_MOVEDOWN );
		}
	}

	pop->popup( this, x, y );
}

void CWorkspaceManager::ShowContextMenu_VCD( int x, int y, CVCDFile *vcd )
{
	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add( va( "Remove VCD" ), IDC_WSM_SCENE_VCD_REMOVE );
	pop->addSeparator();
	pop->add( va( "Edit comments..." ), IDC_WSM_VCD_EDIT_COMMENTS );
	if ( !vcd->IsFirstChild() || !vcd->IsLastChild() )
	{
		pop->addSeparator();
		if( !vcd->IsFirstChild() )
		{
			pop->add( va( "Move '%s' Up", vcd->GetName() ), IDC_WSM_SELECTION_MOVEUP );
		}
		if( !vcd->IsLastChild() ) 
		{
			pop->add( va( "Move '%s' Down", vcd->GetName() ), IDC_WSM_SELECTION_MOVEDOWN );
		}
	}
	pop->addSeparator();
	if ( !filesystem->IsFileWritable( vcd->GetName() ) )
	{
		pop->add( va( "Checkout '%s'", vcd->GetName() ), IDC_WSM_SELECTION_CHECKOUT );
	}
	else
	{
		pop->add( va( "Checkin '%s'", vcd->GetName() ), IDC_WSM_SELECTION_CHECKIN );
	}

	pop->popup( this, x, y );
}

void CWorkspaceManager::ShowContextMenu_SoundEntry( int x, int y, CSoundEntry *entry )
{
	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add ("&Play", IDC_WSM_SOUNDENTRY_PLAY );
	// pop->add( "&Jump To", IDC_WSM_SOUNDENTRY_SHOWINBROWSERS );
	pop->addSeparator();

	pop->add( "Properties...", IDC_WSM_SOUNDENTRY_PROPERTIES );

	pop->popup( this, x, y );
}

void CWorkspaceManager::ShowContextMenu_WaveFile( int x, int y, CWaveFile *entry )
{
	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	pop->add ("&Play", IDC_WSM_SOUNDENTRY_PLAY );
	// pop->add( "&Jump To", IDC_WSM_SOUNDENTRY_SHOWINBROWSERS );

	if ( entry->GetVoiceDuck() )
	{
		pop->add ("Disable &voice duck", IDC_WSM_SOUNDENTRY_TOGGLEVOICEDUCK );
	}
	else
	{
		pop->add ("Enable &voice duck", IDC_WSM_SOUNDENTRY_TOGGLEVOICEDUCK );
	}
	pop->add( "&Edit sentence text...", IDC_WSM_SOUNDENTRY_EDITTEXT );
	pop->addSeparator();
	if ( !filesystem->IsFileWritable( entry->GetFileName() ) )
	{
		pop->add( va( "Checkout '%s'", entry->GetName() ), IDC_WSM_SELECTION_CHECKOUT );
	}
	else
	{
		pop->add( va( "Checkin '%s'", entry->GetName() ), IDC_WSM_SELECTION_CHECKIN );
	}
	pop->addSeparator();
	pop->add( "Properties...", IDC_WSM_WAVEFILE_PROPERTIES );

	pop->popup( this, x, y );
}


void CWorkspaceManager::OnChangeVSSProperites()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		return;
	}

	CVSSParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "VSS Properites" );
	V_strcpy_safe( params.m_szUserName, ws->GetVSSUserName() );
	V_strcpy_safe( params.m_szProject, ws->GetVSSProject() );

	if ( !VSSProperties( &params ) )
		return;

	if ( !params.m_szUserName[ 0 ] )
	{
		Con_Printf( "You must enter a user name\n" );
		return;
	}

	if ( !params.m_szProject[ 0 ] )
	{
		Con_Printf( "You must enter a project name\n" );
		return;
	}

	ws->SetVSSUserName( params.m_szUserName );
	ws->SetVSSProject( params.m_szProject );

	ws->SetDirty( true );

	Con_Printf( "VSS user = '%s', project = '%s'\n",
		ws->GetVSSUserName(), ws->GetVSSProject() );
}

void CWorkspaceManager::OnCheckoutWorkspace()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		return;
	}

	ws->Checkout();
}

void CWorkspaceManager::OnCheckinWorkspace()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		return;
	}

	ws->Checkin();
}


#define CX_ICON  16
#define CY_ICON  16 

HIMAGELIST CWorkspaceManager::CreateImageList()
{
	HIMAGELIST list;
	
	list = ImageList_Create( CX_ICON, CY_ICON, 
		FALSE, NUM_IMAGES, 0 );

    // Load the icon resources, and add the icons to the image list. 
    HICON hicon;
	int slot;
#if defined( DBGFLAG_ASSERT )
	int c = 0;
#endif

	hicon = LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WORKSPACE)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WORKSPACE_CHECKEDOUT)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_PROJECT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_PROJECT_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SCENE)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

//	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SCENE_CHECKEDOUT)); 
//	slot = ImageList_AddIcon(list, hicon); 
//	Assert( slot == c++ );
//	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_VCD)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_VCD_CHECKEDOUT )); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WAV)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WAV_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SPEAK)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SPEAK_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	return list;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::RefreshBrowsers()
{
	if ( GetBrowser() )
	{
		GetBrowser()->PopulateTree();
	}

	if ( GetSoundBrowser() )
	{
		GetSoundBrowser()->RepopulateTree();
	}

	if ( GetWaveBrowser() )
	{
		GetWaveBrowser()->RepopulateTree();
	}
}

void CWorkspaceManager::OnSoundShowInBrowsers()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CSoundEntry *se = item->GetSoundEntry();
	if ( se )
	{
		GetSoundBrowser()->JumpToItem( se );
		return;
	}

	CWaveFile *wave = item->GetWaveFile();
	if ( wave )
	{
		GetWaveBrowser()->JumpToItem( wave );
		if ( wave->GetOwnerSoundEntry() )
		{
			GetSoundBrowser()->JumpToItem( wave->GetOwnerSoundEntry()  );
		}
	}
}

void CWorkspaceManager::OnCheckout()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	item->Checkout();

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnCheckin()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	item->Checkin();

	GetBrowser()->PopulateTree();

	OnUpdateTitle();
}

void CWorkspaceManager::OnMoveUp()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	ITreeItem *parent = item->GetParentItem();
	if ( !parent )
		return;

	parent->MoveChildUp( item );

	parent->SetDirty( true );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();

	GetBrowser()->JumpTo( item );
}

void CWorkspaceManager::OnMoveDown()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	ITreeItem *parent = item->GetParentItem();
	if ( !parent )
		return;

	parent->MoveChildDown( item );

	parent->SetDirty( true );

	GetBrowser()->PopulateTree();

	OnUpdateTitle();

	GetBrowser()->JumpTo( item );
}

void CWorkspaceManager::SetWorkspaceDirty()
{
	CWorkspace *ws = GetBrowser()->GetWorkspace();
	if ( ws )
	{
		ws->SetDirty( true );
	}
	OnUpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnSoundProperties()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CSoundEntry *entry = item->GetSoundEntry();
	if ( !entry )
		return;

	CSoundParams params;
	memset( &params, 0, sizeof( params ) );

	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Sound Properties" );

	params.items.AddToTail( entry );

	SoundProperties( &params );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceManager::OnWaveProperties()
{
	ITreeItem *item = GetBrowser()->GetSelectedItem();
	if ( !item )
		return;

	CWaveFile *wave = item->GetWaveFile();
	if ( !wave )
		return;

	CWaveParams params;
	memset( &params, 0, sizeof( params ) );

	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Wave Properties" );

	params.items.AddToTail( wave );

	WaveProperties( &params );
}
