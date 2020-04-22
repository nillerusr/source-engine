//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           mdlviewer.cpp
// last modified:  Jun 03 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mxtk/mx.h>
#include <mxtk/mxTga.h>
#include <mxtk/mxEvent.h>
#include "mdlviewer.h"
#include "ViewerSettings.h"
#include "MatSysWin.h"
#include "ControlPanel.h"
#include "StudioModel.h"
#include "FileAssociation.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "ifilesystemopendialog.h"
#include "appframework/appframework.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "vphysics_interface.h"
#include "Datacache/imdlcache.h"
#include "datacache/idatacache.h"
#include "filesystem_init.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundsystem/isoundsystem.h"
#include "tier1/tier1.h"
#include "valve_ipc_win32.h"
#include "threadtools.h"
#include "vstdlib/IKeyValuesSystem.h"

bool g_bOldFileDialogs = false;

MDLViewer *g_MDLViewer = 0;
char g_appTitle[] = "Half-Life Model Viewer v1.22";
static char recentFiles[8][256] = { "", "", "", "", "", "", "", "" };
extern int g_dxlevel;
bool g_bInError = false;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
IStudioRender *g_pStudioRender;
IMDLCache *g_pMDLCache;
IPhysicsSurfaceProps *physprop;
IPhysicsCollision *physcollision;
IFileSystem *g_pFileSystem;
IMaterialSystem *g_pMaterialSystem;
IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig;
IStudioDataCache *g_pStudioDataCache;
IDataCache *g_pDataCache;
ISoundEmitterSystemBase *g_pSoundEmitterBase;
ISoundSystem *g_pSoundSystem;
CreateInterfaceFn g_Factory;

// Filesystem dialog module wrappers.
CSysModule *g_pFSDialogModule = 0;
CreateInterfaceFn g_FSDialogFactory = 0;


class CHlmvIpcServer : public CValveIpcServerUtl
{
public:
	CHlmvIpcServer() : CValveIpcServerUtl( "HLMV_IPC_SERVER" ) {}
	~CHlmvIpcServer();

public:
	bool HasCommands();
	void AppendCommand( char *pszCommand );
	char *GetCommand();
	void PopCommand();

protected:
	virtual BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res );

protected:
	CThreadFastMutex m_mtx;
	CUtlVector< char * > m_lstCommands;
}
g_HlmvIpcServer;

CValveIpcClientUtl g_HlmvIpcClient( "HLMV_IPC_SERVER" );
bool g_bHlmvMaster = false;	// This hlmv is controlling a controlled hlmv instance
bool g_bHlmvControlled = false;	// This hlmv is being controlled by a master hlmv instance

void LoadFileSystemDialogModule()
{
	Assert( !g_pFSDialogModule );

	// Load the module with the file system open dialog.
	const char *pDLLName = "FileSystemOpenDialog.dll";
	g_pFSDialogModule = Sys_LoadModule( pDLLName );
	if ( g_pFSDialogModule )
	{
		g_FSDialogFactory = Sys_GetFactory( g_pFSDialogModule );
	}

	if ( !g_pFSDialogModule || !g_FSDialogFactory )
	{
		if ( g_pFSDialogModule )
		{
			Sys_UnloadModule( g_pFSDialogModule );
			g_pFSDialogModule = NULL;
		}
	}
}

void UnloadFileSystemDialogModule()
{
	if ( g_pFSDialogModule )
	{
		Sys_UnloadModule( g_pFSDialogModule );
		g_pFSDialogModule = 0;
	}
}	



void
MDLViewer::initRecentFiles ()
{
	for (int i = 0; i < 8; i++)
	{
		if (strlen (recentFiles[i]))
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, recentFiles[i]);
		}
		else
		{
			mb->modify (IDC_FILE_RECENTMODELS1 + i, IDC_FILE_RECENTMODELS1 + i, "(empty)");
			mb->setEnabled (IDC_FILE_RECENTMODELS1 + i, false);
		}
	}
}



void
MDLViewer::loadRecentFiles ()
{
	char path[256];
	strcpy (path, mx::getApplicationPath ());
	strcat (path, "/hlmv.rf");
	FILE *file = fopen (path, "rb");
	if (file)
	{
		fread (recentFiles, sizeof recentFiles, 1, file);
		fclose (file);
	}
}



void
MDLViewer::saveRecentFiles ()
{
	char path[256];

	strcpy (path, mx::getApplicationPath ());
	strcat (path, "/hlmv.rf");

	FILE *file = fopen (path, "wb");
	if (file)
	{
		fwrite (recentFiles, sizeof recentFiles, 1, file);
		fclose (file);
	}
}

struct AccelTableEntry_t
{
	unsigned short key;
	unsigned short command;
	unsigned char  flags;
};

AccelTableEntry_t accelTable[] =					{{VK_F1, IDC_FLUSH_SHADERS,		mx::ACCEL_VIRTKEY},
													{VK_F5, IDC_FILE_REFRESH,		mx::ACCEL_VIRTKEY},
													{'u', IDC_FILE_UNLOADALLMERGEDMODELS, mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'U', IDC_FILE_UNLOADALLMERGEDMODELS, mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'w', IDC_ACCEL_WIREFRAME,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'W', IDC_ACCEL_WIREFRAME,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'a', IDC_ACCEL_ATTACHMENTS,	mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'A', IDC_ACCEL_ATTACHMENTS,	mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'g', IDC_ACCEL_GROUND,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'G', IDC_ACCEL_GROUND,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'h', IDC_ACCEL_HITBOXES,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'H', IDC_ACCEL_HITBOXES,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'o', IDC_ACCEL_BONES,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'O', IDC_ACCEL_BONES,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'b', IDC_ACCEL_BACKGROUND,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'B', IDC_ACCEL_BACKGROUND,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'m', IDC_ACCEL_MOVEMENT,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'M', IDC_ACCEL_MOVEMENT,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'n', IDC_ACCEL_NORMALS,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'N', IDC_ACCEL_NORMALS,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'t', IDC_ACCEL_TANGENTS,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'T', IDC_ACCEL_TANGENTS,		mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'s', IDC_ACCEL_SHADOW,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY},
													{'S', IDC_ACCEL_SHADOW,			mx::ACCEL_CONTROL | mx::ACCEL_VIRTKEY}};
#define NUM_ACCELERATORS ARRAYSIZE( accelTable )


MDLViewer::MDLViewer ()
: mxWindow (0, 0, 0, 0, 0, g_appTitle, mxWindow::Normal)
{
	d_MatSysWindow = 0;
	d_cpl = 0;

	// create menu stuff
	mb = new mxMenuBar (this);
	mxMenu *menuFile = new mxMenu ();
	menuOptions = new mxMenu ();
	menuView = new mxMenu ();
	mxMenu *menuHelp = new mxMenu ();

	mb->addMenu ("File", menuFile);
	mb->addMenu ("Options", menuOptions);
	mb->addMenu ("View", menuView);
	mb->addMenu ("Help", menuHelp);

	mxMenu *menuRecentModels = new mxMenu ();
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS1);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS2);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS3);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS4);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS5);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS6);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS7);
	menuRecentModels->add ("(empty)", IDC_FILE_RECENTMODELS8);

	if ( g_bOldFileDialogs )
	{
		menuFile->add ("Load Model...", IDC_FILE_LOADMODEL);
		menuFile->add ("(Steam) Load Model...", IDC_FILE_LOADMODEL_STEAM);
	}
	else
	{
		menuFile->add ("Load Model...", IDC_FILE_LOADMODEL_STEAM);
	}

	menuFile->add( "Refresh (F5)", IDC_FILE_REFRESH );
	menuFile->addSeparator ();
	
	if ( g_bOldFileDialogs )
	{
		menuFile->add ("Load Weapon...", IDC_FILE_LOADMERGEDMODEL);
		menuFile->add ("(Steam) Load Weapon...", IDC_FILE_LOADMERGEDMODEL_STEAM);
	}
	else
	{
		menuFile->add ("Load Weapon...", IDC_FILE_LOADMERGEDMODEL_STEAM);
	}

	mxMenu *menuUnloadWeapon = new mxMenu ();
	menuUnloadWeapon->add ("Unload All Merged Models (Ctrl-U)", IDC_FILE_UNLOADALLMERGEDMODELS);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL1);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL2);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL3);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL4);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL5);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL6);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL7);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL8);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL9);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL10);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL11);
	menuUnloadWeapon->add ("(empty)", IDC_FILE_UNLOADMERGEDMODEL12);
	for ( int i = IDC_FILE_UNLOADMERGEDMODEL1; i <= IDC_FILE_UNLOADMERGEDMODEL12; i++ )
	{
		menuUnloadWeapon->setEnabled( i, false );
	}
	menuFile->addMenu ("Unload Weapon", menuUnloadWeapon);
	
	menuFile->addSeparator ();
	menuFile->add ("Load Background Texture...", IDC_FILE_LOADBACKGROUNDTEX);
	menuFile->add ("Load Ground Texture...", IDC_FILE_LOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Unload Ground Texture", IDC_FILE_UNLOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->addMenu ("Recent Models", menuRecentModels);
	menuFile->addSeparator ();
	menuFile->add ("Exit", IDC_FILE_EXIT);

	menuFile->setEnabled(IDC_FILE_LOADBACKGROUNDTEX, false);
	menuFile->setEnabled(IDC_FILE_LOADGROUNDTEX, false);
	menuFile->setEnabled(IDC_FILE_UNLOADGROUNDTEX, false);

	menuOptions->add ("Background Color...", IDC_OPTIONS_COLORBACKGROUND);
	menuOptions->add ("Ground Color...", IDC_OPTIONS_COLORGROUND);
	menuOptions->add ("Light Color...", IDC_OPTIONS_COLORLIGHT);
	menuOptions->add ("Ambient Color...", IDC_OPTIONS_COLORAMBIENT);
	menuOptions->addSeparator ();
	menuOptions->add ("Center View", IDC_OPTIONS_CENTERVIEW);
	menuOptions->add ("Viewmodel Mode", IDC_OPTIONS_VIEWMODEL);
#ifdef WIN32
	menuOptions->addSeparator ();
	menuOptions->add ("Make Screenshot...", IDC_OPTIONS_MAKESCREENSHOT);
	//menuOptions->add ("Dump Model Info", IDC_OPTIONS_DUMP);
#endif

	menuView->add ("File Associations...", IDC_VIEW_FILEASSOCIATIONS);
	menuView->setEnabled( IDC_VIEW_FILEASSOCIATIONS, false );

	menuView->addSeparator ();
	menuView->add ("Show Activities", IDC_VIEW_ACTIVITIES);
	menuView->add ("Show hidden", IDC_VIEW_HIDDEN );

#ifdef WIN32
	menuHelp->add ("Goto Homepage...", IDC_HELP_GOTOHOMEPAGE);
	menuHelp->addSeparator ();
#endif
	menuHelp->add ("About...", IDC_HELP_ABOUT);


	d_MatSysWindow = new MatSysWindow (this, 0, 0, 100, 100, "", mxWindow::Normal);
#ifdef WIN32
	// SetWindowLong ((HWND) d_MatSysWindow->getHandle (), GWL_EXSTYLE, WS_EX_CLIENTEDGE);
#endif

	d_cpl = new ControlPanel (this);
	d_cpl->setMatSysWindow (d_MatSysWindow);
	g_MatSysWindow = d_MatSysWindow;

	g_FileAssociation = new FileAssociation ();

	loadRecentFiles ();
	initRecentFiles ();

	LoadViewerRootSettings( );

	// FIXME: where do I actually find the domain size of the viewport, especially for multi-monitor
	// try to catch weird initialization error
	if (g_viewerSettings.xpos < -16384)
		g_viewerSettings.xpos = 20;
	if (g_viewerSettings.ypos < -16384)
		g_viewerSettings.ypos = 20;
	g_viewerSettings.ypos  = max( 0, g_viewerSettings.ypos );
	g_viewerSettings.width = max( 640, g_viewerSettings.width );
	g_viewerSettings.height = max( 700, g_viewerSettings.height );

	setBounds( g_viewerSettings.xpos, g_viewerSettings.ypos, g_viewerSettings.width, g_viewerSettings.height );
	setVisible (true);
	setTimer( 200 );

	CUtlVector< mx::Accel_t > accelerators;
	mx::Accel_t accel;

	for (int i=0; i < NUM_ACCELERATORS; i++)
	{
		accel.flags	  = accelTable[i].flags ;
		accel.key	  = accelTable[i].key;
		accel.command = accelTable[i].command;
		accelerators.AddToTail( accel );
	}

	mx::createAccleratorTable( accelerators.Count(), accelerators.Base() );

	g_HlmvIpcServer.EnsureRegisteredAndRunning();

	if ( !g_HlmvIpcServer.IsRunning() )
	{
		menuOptions->addSeparator();
		menuOptions->add ( "Link HLMV", IDC_OPTIONS_LINKHLMV );
		menuOptions->add ( "Unlink HLMV", IDC_OPTIONS_UNLINKHLMV );
		menuOptions->setChecked( IDC_OPTIONS_UNLINKHLMV, true );
		menuOptions->setEnabled( IDC_OPTIONS_UNLINKHLMV, false );
	}
}



MDLViewer::~MDLViewer ()
{
	g_HlmvIpcServer.EnsureStoppedAndUnregistered();

	saveRecentFiles ();
	SaveViewerSettings( g_pStudioModel->GetFileName(), g_pStudioModel );
	SaveViewerRootSettings( );

#ifdef WIN32
	DeleteFile ("hlmv.cfg");
	DeleteFile ("midump.txt");
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Reloads the currently loaded model file.
//-----------------------------------------------------------------------------
void MDLViewer::Refresh( void )
{
	KeyValuesSystem()->InvalidateCache();

	g_pStudioModel->ReleaseStudioModel();
	g_pMDLCache->Flush();
	if ( recentFiles[0][0] != '\0' )
	{
		char szFile[MAX_PATH];
		strcpy( szFile, recentFiles[0] );
		g_pMaterialSystem->ReloadMaterials();
		d_cpl->loadModel( szFile );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads the file and updates the MRU list.
// Input  : pszFile - File to load.
//-----------------------------------------------------------------------------
void MDLViewer::LoadModelFile( const char *pszFile, int slot )
{
	// copy off name, pszFile may be point into recentFiles array
	char filename[1024];
	strcpy( filename, pszFile );

	LoadModelResult_t eLoaded = d_cpl->loadModel( filename, slot );

	if ( eLoaded != LoadModel_Success )
	{
		switch (eLoaded)
		{
			case LoadModel_LoadFail:
			{
				mxMessageBox (this, "Error loading model.", g_appTitle, MX_MB_ERROR | MX_MB_OK);
				break;
			}
			
			case LoadModel_PostLoadFail:
			{
				mxMessageBox (this, "Error post-loading model.", g_appTitle, MX_MB_ERROR | MX_MB_OK);
				break;
			}

			case LoadModel_NoModel:
			{
				mxMessageBox (this, "Error loading model. The model has no vertices.", g_appTitle, MX_MB_ERROR | MX_MB_OK);
				break;
			}
		}

		return;
	}

	if (slot == -1)
	{
		int i;
		for (i = 0; i < 8; i++)
		{
			if (!mx_strcasecmp( recentFiles[i], filename ))
				break;
		}

		// shift down existing recent files
		for (i = ((i > 7) ? 7 : i); i > 0; i--)
		{
			strcpy (recentFiles[i], recentFiles[i-1]);
		}

		strcpy( recentFiles[0], filename );

		initRecentFiles ();

		setLabel( "%s", filename );
	}
	else
	{	
		mb->modify (IDC_FILE_UNLOADMERGEDMODEL1 + slot, IDC_FILE_UNLOADMERGEDMODEL1 + slot, pszFile);
		mb->setEnabled (IDC_FILE_UNLOADMERGEDMODEL1 + slot, true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Takes a TGA screenshot of the given filename and exits.
// Input  : pszFile - File to load.
//-----------------------------------------------------------------------------
void MDLViewer::SaveScreenShot( const char *pszFile )
{
	char filename[1024];
	strcpy( filename, pszFile );
	LoadModelResult_t eLoaded = d_cpl->loadModel( filename );

	//
	// Screenshot mode. Write a screenshot file and exit.
	//
	if ( eLoaded == LoadModel_Success )
	{
		g_viewerSettings.bgColor[0] = 117.0f / 255.0f;
		g_viewerSettings.bgColor[1] = 196.0f / 255.0f;
		g_viewerSettings.bgColor[2] = 219.0f / 255.0f;

		// Build the name of the TGA to write.
		char szScreenShot[256];
		strcpy(szScreenShot, filename);
		char *pchDot = strrchr(szScreenShot, '.');
		if (pchDot)
		{
			strcpy(pchDot, ".tga");
		}
		else
		{
			strcat(szScreenShot, ".tga");
		}

		// Center the view and write the TGA.
		d_cpl->centerView();
		d_MatSysWindow->dumpViewport(szScreenShot);
	}

	// Shut down.
	mx::quit();
	return;
}


void MDLViewer::DumpText( const char *pszFile )
{
	char filename[1024];
	strcpy( filename, pszFile );
	LoadModelResult_t eLoaded = d_cpl->loadModel( filename );

	//
	// Screenshot mode. Write a screenshot file and exit.
	//
	if ( eLoaded == LoadModel_Success )
	{
		if ( g_pStudioModel->m_bIsTransparent )
		{
			Msg("%s is transparent\n", filename );
		}
		if ( g_pStudioModel->m_bHasProxy )
		{
			Msg("%s has material proxies\n", filename );
		}
	}

	// Shut down.
	mx::quit();
}


const char* MDLViewer::SteamGetOpenFilename()
{
	if ( !g_FSDialogFactory )
		return NULL;

	static char filename[MAX_PATH];

	IFileSystemOpenDialog *pDlg;
	pDlg = (IFileSystemOpenDialog*)g_FSDialogFactory( FILESYSTEMOPENDIALOG_VERSION, NULL );
	if ( !pDlg )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "Can't create %s interface.", FILESYSTEMOPENDIALOG_VERSION );
		MessageBox( NULL, str, "Error", MB_OK );
		return NULL;
	}
	pDlg->Init( g_Factory, NULL );
	pDlg->AddFileMask( "*.jpg" );
	pDlg->AddFileMask( "*.mdl" );
	pDlg->SetInitialDir( "models", "game" );
	pDlg->SetFilterMdlAndJpgFiles( true );

	if (pDlg->DoModal() == IDOK)
	{
		pDlg->GetFilename( filename, sizeof( filename ) );
		pDlg->Release();
		return filename;
	}
	else
	{
		pDlg->Release();
		return NULL;
	}
}


int
MDLViewer::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	
	switch (event->event)
	{
	case mxEvent::Action:
	{
		switch (event->action)
		{
		case IDC_FILE_LOADMODEL:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "*.mdl");
			if (ptr)
			{
				LoadModelFile( ptr );
			}
		}
		break;

		case IDC_FILE_LOADMODEL_STEAM:
		{
			const char *pFilename = SteamGetOpenFilename();
			if ( pFilename )
			{
				LoadModelFile( pFilename );
			}
		}
		break;

		case IDC_FILE_LOADMERGEDMODEL:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "*.mdl");
			if (ptr)
			{
				// find the first free slot
				int iChosenSlot = 0;
				for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
				{
					if ( g_viewerSettings.mergeModelFile[i][0] == 0 )
					{
						iChosenSlot = i;
						break;
					}
				}
				strcpy( g_viewerSettings.mergeModelFile[iChosenSlot], ptr );
				LoadModelFile( ptr, iChosenSlot );
			}
		}
		break;

		case IDC_FILE_LOADMERGEDMODEL_STEAM:
		{
			const char *pFilename = SteamGetOpenFilename();
			if ( pFilename )
			{
				// find the first free slot
				int iChosenSlot = 0;
				for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
				{
					if ( g_viewerSettings.mergeModelFile[i][0] == 0 )
					{
						iChosenSlot = i;
						break;
					}
				}
				strcpy( g_viewerSettings.mergeModelFile[iChosenSlot], pFilename );
				LoadModelFile( pFilename, iChosenSlot );
			}
		}
		break;


		case IDC_FILE_UNLOADMERGEDMODEL1:
		case IDC_FILE_UNLOADMERGEDMODEL2:
		case IDC_FILE_UNLOADMERGEDMODEL3:
		case IDC_FILE_UNLOADMERGEDMODEL4:
		case IDC_FILE_UNLOADMERGEDMODEL5:
		case IDC_FILE_UNLOADMERGEDMODEL6:
		case IDC_FILE_UNLOADMERGEDMODEL7:
		case IDC_FILE_UNLOADMERGEDMODEL8:
		case IDC_FILE_UNLOADMERGEDMODEL9:
		case IDC_FILE_UNLOADMERGEDMODEL10:
		case IDC_FILE_UNLOADMERGEDMODEL11:
		case IDC_FILE_UNLOADMERGEDMODEL12:
		{
			int i = event->action - IDC_FILE_UNLOADMERGEDMODEL1;
			// FIXME: move to d_cpl
			if (g_pStudioExtraModel[i])
			{
				V_strcpy_safe( g_viewerSettings.mergeModelFile[i], "" );
				g_pStudioExtraModel[i]->FreeModel( false );
				delete g_pStudioExtraModel[i];
				g_pStudioExtraModel[i] = NULL;

				mb->modify (IDC_FILE_UNLOADMERGEDMODEL1 + i, IDC_FILE_UNLOADMERGEDMODEL1 + i, "(empty)");
				mb->setEnabled (IDC_FILE_UNLOADMERGEDMODEL1 + i, false);					
			}
		}
		break;

		case IDC_FILE_UNLOADALLMERGEDMODELS:
			d_cpl->UnloadAllMergedModels();
			break;

		case IDC_FILE_REFRESH:
		{
			Refresh();
			break;
		}

		case IDC_FLUSH_SHADERS:
		{
			CCommand args;
			args.Tokenize( "mat_flushshaders" );

			ConCommandBase *pCommandBase = g_pCVar->FindCommandBase( args[0] );
			if ( !pCommandBase )
			{
				ConWarning( "Unknown command or convar '%s'!\n", args[0] );
				break;
			}

			if ( pCommandBase->IsCommand() )
			{
				ConCommand *pCommand = static_cast<ConCommand*>( pCommandBase );
				pCommand->Dispatch( args );
			}
		}
		break;

		case IDC_FILE_LOADBACKGROUNDTEX:
		case IDC_FILE_LOADGROUNDTEX:
		{
			const char *ptr = mxGetOpenFileName (this, 0, "*.*");
			if (ptr)
			{
				if (0 /* d_MatSysWindow->loadTexture (ptr, event->action - IDC_FILE_LOADBACKGROUNDTEX) */)
				{
					if (event->action == IDC_FILE_LOADBACKGROUNDTEX)
						d_cpl->setShowBackground (true);
					else
						d_cpl->setShowGround (true);

				}
				else
					mxMessageBox (this, "Error loading texture.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
			}
		}
		break;

		case IDC_FILE_UNLOADGROUNDTEX:
		{
			// d_MatSysWindow->loadTexture (0, 1);
			d_cpl->setShowGround (false);
		}
		break;

		case IDC_FILE_RECENTMODELS1:
		case IDC_FILE_RECENTMODELS2:
		case IDC_FILE_RECENTMODELS3:
		case IDC_FILE_RECENTMODELS4:
		case IDC_FILE_RECENTMODELS5:
		case IDC_FILE_RECENTMODELS6:
		case IDC_FILE_RECENTMODELS7:
		case IDC_FILE_RECENTMODELS8:
		{
			int i = event->action - IDC_FILE_RECENTMODELS1;
			LoadModelFile( recentFiles[i] );
		}
		break;

		case IDC_FILE_EXIT:
		{
			redraw ();
			mx::quit ();
		}
		break;

		case IDC_OPTIONS_COLORBACKGROUND:
		case IDC_OPTIONS_COLORGROUND:
		case IDC_OPTIONS_COLORLIGHT:
		case IDC_OPTIONS_COLORAMBIENT:
		{
			float *cols[4] = { g_viewerSettings.bgColor, g_viewerSettings.gColor, g_viewerSettings.lColor, g_viewerSettings.aColor };
			float *col = cols[event->action - IDC_OPTIONS_COLORBACKGROUND];
			int r = (int) (col[0] * 255.0f);
			int g = (int) (col[1] * 255.0f);
			int b = (int) (col[2] * 255.0f);
			if (mxChooseColor (this, &r, &g, &b))
			{
				col[0] = (float) r / 255.0f;
				col[1] = (float) g / 255.0f;
				col[2] = (float) b / 255.0f;
			}
		}
		break;

		case IDC_OPTIONS_CENTERVIEW:
			d_cpl->centerView ();
			if ( g_bHlmvMaster )
			{
				SendModelTransformToLinkedHlmv();
			}
			break;
		case IDC_OPTIONS_CENTERVERTS:
			//d_cpl->centerVerts( );
			if ( g_bHlmvMaster )
			{
				SendModelTransformToLinkedHlmv();
			}
			break;
		case IDC_OPTIONS_VIEWMODEL:
		{
			d_cpl->viewmodelView();
			if ( g_bHlmvMaster )
			{
				SendModelTransformToLinkedHlmv();
			}
		}
		break;

		case IDC_OPTIONS_MAKESCREENSHOT:
		{
			char *ptr = (char *) mxGetSaveFileName (this, "", "*.tga");
			if (ptr)
			{
				if (!strstr (ptr, ".tga"))
					strcat (ptr, ".tga");
				d_MatSysWindow->dumpViewport (ptr);
			}
		}
		break;

		case IDC_OPTIONS_DUMP:
			d_cpl->dumpModelInfo ();
			break;

		case IDC_OPTIONS_SYNCHLMVCAMERA:
			SendModelTransformToLinkedHlmv();

			break;

		case IDC_OPTIONS_LINKHLMV:
			if ( !g_bHlmvMaster && !g_HlmvIpcServer.IsRunning() && g_HlmvIpcClient.Connect() )
			{
				CUtlBuffer cmd;
				CUtlBuffer res;

				// Make connection to other hlmv
				cmd.PutString( "hlmvLink" );
				cmd.PutChar( '\0' );

				if ( g_HlmvIpcClient.ExecuteCommand( cmd, res ) )
				{
					g_bHlmvMaster = true;
				}

				g_HlmvIpcClient.Disconnect();

				SendModelTransformToLinkedHlmv();
				SendLightRotToLinkedHlmv();

				menuOptions->setChecked( IDC_OPTIONS_LINKHLMV, true );
				menuOptions->setEnabled( IDC_OPTIONS_LINKHLMV, false );

				menuOptions->setChecked( IDC_OPTIONS_UNLINKHLMV, false );
				menuOptions->setEnabled( IDC_OPTIONS_UNLINKHLMV, true );
			}
			break;

		case IDC_OPTIONS_UNLINKHLMV:
			if ( g_bHlmvMaster && g_HlmvIpcClient.Connect() )
			{
				CUtlBuffer cmd;
				CUtlBuffer res;

				// Break connection to linked hlmv

				cmd.PutString( "hlmvUnlink" );
				cmd.PutChar( '\0' );

				g_HlmvIpcClient.ExecuteCommand( cmd, res );
				g_bHlmvMaster = false;

				g_HlmvIpcClient.Disconnect();

				menuOptions->setChecked( IDC_OPTIONS_LINKHLMV, false );
				menuOptions->setEnabled( IDC_OPTIONS_LINKHLMV, true );

				menuOptions->setChecked( IDC_OPTIONS_UNLINKHLMV, true );
				menuOptions->setEnabled( IDC_OPTIONS_UNLINKHLMV, false );
			}
			break;

		case IDC_VIEW_FILEASSOCIATIONS:
			g_FileAssociation->setAssociation (0);
			g_FileAssociation->setVisible (true);
			break;

		case IDC_VIEW_ACTIVITIES:
			g_viewerSettings.showActivities = !g_viewerSettings.showActivities;
			menuView->setChecked( event->action, g_viewerSettings.showActivities );
			d_cpl->initSequenceChoices();
			d_cpl->resetControlPanel();
			break;

		case IDC_VIEW_HIDDEN:
			g_viewerSettings.showHidden = !g_viewerSettings.showHidden;
			menuView->setChecked( event->action, g_viewerSettings.showHidden );
			d_cpl->initSequenceChoices();
			d_cpl->resetControlPanel();
			break;

#ifdef WIN32
		case IDC_HELP_GOTOHOMEPAGE:
			ShellExecute (0, "open", "http://www.swissquake.ch/chumbalum-soft/index.html", 0, 0, SW_SHOW);
			break;
#endif

		case IDC_HELP_ABOUT:
			mxMessageBox (this,
				"Half-Life Model Viewer v2.0 (c) 2004 Valve Corp.\n"
				"Portions (c) 1999 by Mete Ciragan\n\n"
				"Left-drag inside circle to spin.\n"
				"Left-drag outside circle to rotate.\n"
				"Right-drag to zoom.\n"
				"Shift-left-drag to x-y-pan.\n"
				"Shift-right-drag to z-pan.\n"
				"Ctrl-left-drag to move light.\n\n"
				"Build:\t" __DATE__ ".\n"
				"Email:\tmete@swissquake.ch\n"
				"Web:\thttp://www.swissquake.ch/chumbalum-soft/", "About Half-Life Model Viewer",
				MX_MB_OK | MX_MB_INFORMATION);
			break;

		case IDC_ACCEL_WIREFRAME:
			d_cpl->setOverlayWireframe( !g_viewerSettings.overlayWireframe );
			break;

		case IDC_ACCEL_ATTACHMENTS:
			d_cpl->setShowAttachments( !g_viewerSettings.showAttachments );
			break;

		case IDC_ACCEL_GROUND:
			d_cpl->setShowGround( !g_viewerSettings.showGround );
			break;

		case IDC_ACCEL_HITBOXES:
			d_cpl->setShowHitBoxes( !g_viewerSettings.showHitBoxes );
			break;

		case IDC_ACCEL_BONES:
			d_cpl->setShowBones( !g_viewerSettings.showBones );
			break;

		case IDC_ACCEL_BACKGROUND:
			d_cpl->setShowBackground( !g_viewerSettings.showBackground );
			break;

		case IDC_ACCEL_MOVEMENT:
			d_cpl->setShowMovement( !g_viewerSettings.showMovement );
			break;

		case IDC_ACCEL_NORMALS:
			d_cpl->setShowNormals( !g_viewerSettings.showNormals );
			break;

		case IDC_ACCEL_TANGENTS:
			d_cpl->setShowTangentFrame( !g_viewerSettings.showTangentFrame );
			break;

		case IDC_ACCEL_SHADOW:
			d_cpl->setShowShadow( !g_viewerSettings.showShadow );
			break;

		} //switch (event->action)

	} // mxEvent::Action
	break;

	case mxEvent::Size:
	{
		g_viewerSettings.xpos = x();
		g_viewerSettings.ypos = y();
		g_viewerSettings.width = w();
		g_viewerSettings.height = h();

		int w = event->width;
		int h = event->height;
		int y = mb->getHeight ();
#ifdef WIN32
#define HEIGHT 240
#else
#define HEIGHT 140
		h -= 40;
#endif

		d_MatSysWindow->setBounds (0, y, w, h - HEIGHT); // !!
		d_cpl->setBounds (0, y + h - HEIGHT, w, HEIGHT);
	}
	break;


	case mxEvent::PosChanged:
	{
		g_viewerSettings.xpos = x();
		g_viewerSettings.ypos = y();
	}
	break;

	case KeyDown:
		d_MatSysWindow->handleEvent(event);
		d_cpl->handleEvent(event);
	break;

	case mxEvent::Activate:
	{
		if (event->action)
		{
			mx::setIdleWindow( getMatSysWindow() );
		}
		else
		{
			mx::setIdleWindow( 0 );
		}
	}
	break;

	case mxEvent::Timer:
	{
		if ( g_HlmvIpcServer.HasCommands() )
		{
			// Execute next command at next msg pump, ~ 1/60th s (60Hz) if controlled, 0.1s if not
			if ( g_bHlmvControlled )
			{
				// Clear up to 10 pending commands if controlled
				for ( int nCmdCount = 0; nCmdCount < 10 && g_HlmvIpcServer.HasCommands(); ++nCmdCount )
				{
					handleIpcCommand( g_HlmvIpcServer.GetCommand() );
					g_HlmvIpcServer.PopCommand();
				}

				setTimer( 17 );
			}
			else
			{
				handleIpcCommand( g_HlmvIpcServer.GetCommand() );
				g_HlmvIpcServer.PopCommand();

				setTimer( 100 );
			}
		}
		else if ( !g_HlmvIpcServer.IsRunning() )
		{
			// Keep trying to establish our server slot if another instance quits
			BOOL bIsRunning = g_HlmvIpcServer.IsRunning();
			g_HlmvIpcServer.EnsureRegisteredAndRunning();
			if ( !bIsRunning && g_HlmvIpcServer.IsRunning() )
			{
				g_bHlmvMaster = false;
				menuOptions->setEnabled( IDC_OPTIONS_LINKHLMV, false );
				menuOptions->setChecked( IDC_OPTIONS_LINKHLMV, false );
				menuOptions->setEnabled( IDC_OPTIONS_UNLINKHLMV, false );
				menuOptions->setChecked( IDC_OPTIONS_LINKHLMV, false );
			}

			// Attempt every 1.0 s
			setTimer( 1000 );
		}
		else
		{
			// Idling, poll command queue @ ~60Hz if controlled, 0.5s ( 2Hz ) otherwise
			if ( g_bHlmvControlled )
			{
				setTimer( 17 );
			}
			else
			{
				setTimer( 500 );
			}
		}
	}
	break;
	} // event->event

	return 1;
}



void TranslateMayaToHLMVCoordinates( const Vector &vMayaPos, const QAngle &vMayaRot, Vector &vHLMVPos, QAngle &vHLMVAngles )
{
	vHLMVPos.Init( vMayaPos.z, vMayaPos.x, vMayaPos.y );

	vHLMVAngles[PITCH] = -vMayaRot[0];
	vHLMVAngles[YAW] = vMayaRot[1] + 180;
	vHLMVAngles[ROLL] = -vMayaRot[2];
}


void MDLViewer::handleIpcCommand( char *szCommand )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( !strcmp( "reload", szCommand ) )
	{
		Refresh();

		if ( HWND hWnd = (HWND) getHandle() )
		{
			if ( ::IsIconic( hWnd ) )
				::ShowWindow( hWnd, SW_RESTORE );

			::BringWindowToTop( hWnd );
			::SetForegroundWindow( hWnd );
			::SetFocus( hWnd );
		}
	}
	else if ( V_strncasecmp( "cameraTo", szCommand, 8 ) == 0 )
	{
		// Read the camera position and angles from Maya.
		Vector vMayaPos;
		QAngle vMayaRot;

		char szFirstPart[32];
		sscanf( szCommand, "%s %f %f %f %f %f %f",
			szFirstPart,
			&vMayaPos.x, &vMayaPos.y, &vMayaPos.z, 
			&vMayaRot.x, &vMayaRot.y, &vMayaRot.z );


		//
		// Obviously, this could all be simplified, but it's nice to make it easy to see what it's doing here.
		//

		
		// Translate the camera position/angles from Maya space to model space.
		// In model space, +X=forward, +Y=left, and +Z=up
		// (i.e. the model faces forward along +X)
		Vector vCameraPos;
		QAngle vCameraAngles;
		TranslateMayaToHLMVCoordinates( vMayaPos, vMayaRot, vCameraPos, vCameraAngles );


		// Now, build a matrix from model space to camera space. The way we're defining camera space,
		// the axes point the same way as in model space (+X=forward, +Y=left, +Z=up).
		// This is a standard put-stuff-in-camera-space matrix (backtranslate and then backrotate).
		matrix3x4_t mModelToCameraRot, mModelToCameraTrans, mModelToCameraFull;

		// Backtranslate..
		SetIdentityMatrix( mModelToCameraTrans );
		MatrixSetColumn( -vCameraPos, 3, mModelToCameraTrans );

		// Backrotate..
		AngleMatrix( vCameraAngles, mModelToCameraRot );
		MatrixTranspose( mModelToCameraRot );

		// Concatenate.
		MatrixMultiply( mModelToCameraRot, mModelToCameraTrans, mModelToCameraFull );

		
		// Now we need to convert the camera space from above to HLMV's specific camera space. This just means negating
		// the X and Y axes because HLMV's camera space is (+X=back, +Y=left, +Z=up).
		matrix3x4_t mCameraToWorld( 
			-1, 0, 0, 0,
			0, -1, 0, 0,
			0, 0, 1, 0 );

		// Blat all these matrices together.
		matrix3x4_t mFinal;
		MatrixMultiply( mCameraToWorld, mModelToCameraFull, mFinal );

		// Tell HLMV our new fancy transform to use, and then we'll see the model from the same place Maya did.
		g_pStudioModel->SetModelTransform( mFinal );

		// Redraw.
		d_MatSysWindow->redraw();
	}
	else if ( StringHasPrefixCaseSensitive( szCommand, "hlmvModelTransform" ) )
	{
		matrix3x4_t m;

		sscanf( szCommand, "%*s %f %f %f %f %f %f %f %f %f %f %f %f",
			&m.m_flMatVal[0][0], &m.m_flMatVal[0][1], &m.m_flMatVal[0][2], &m.m_flMatVal[0][3],
			&m.m_flMatVal[1][0], &m.m_flMatVal[1][1], &m.m_flMatVal[1][2], &m.m_flMatVal[1][3],
			&m.m_flMatVal[2][0], &m.m_flMatVal[2][1], &m.m_flMatVal[2][2], &m.m_flMatVal[2][3] );

		// Tell HLMV our new fancy transform to use, and then we'll see the model from the same place Maya did.
		g_pStudioModel->SetModelTransform( m );

		// Redraw.
		d_MatSysWindow->redraw();
	}
	else if ( StringHasPrefixCaseSensitive( szCommand, "hlmvLightRot" ) )
	{
		sscanf( szCommand, "%*s %f %f %f",
			&g_viewerSettings.lightrot[0], &g_viewerSettings.lightrot[1], &g_viewerSettings.lightrot[2] );

		// Redraw.
		d_MatSysWindow->redraw();
	}
	else if ( StringHasPrefixCaseSensitive( szCommand, "hlmvForceFrame" ) )
	{
		float flFrame = 0.0f;
		sscanf( szCommand, "%*s %f", &flFrame );

		d_cpl->SetFrameSlider( flFrame );
		d_cpl->setFrame( flFrame );
		d_cpl->setSpeedScale( 0 );

		// Redraw.
		d_MatSysWindow->redraw();
	}
	else if ( StringHasPrefixCaseSensitive( szCommand, "hlmvLink" ) )
	{
		if ( !g_bHlmvControlled )
		{
			g_bHlmvControlled = true;

			CUtlString label( "LINKED: " );
			label += getLabel();
			setLabel( label.Get() );
		}
	}
	else if ( StringHasPrefixCaseSensitive( szCommand, "hlmvUnlink" ) )
	{
		g_bHlmvControlled = false;

		const char *pszLabel = getLabel();
		if ( StringHasPrefixCaseSensitive( pszLabel, "LINKED: " ) )
		{
			setLabel( pszLabel + 8 );	// Skip past "LINKED: "
		}
	}
}


void
MDLViewer::redraw ()
{
	/*
	mxEvent event;
	event.event = mxEvent::Size;
	event.width = w2 ();
	event.height = h2 ();
	handleEvent (&event);
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int MDLViewer::GetCurrentHitboxSet( void )
{
	return d_cpl ? d_cpl->GetCurrentHitboxSet() : 0;
}


//-----------------------------------------------------------------------------
// Sends the model transform to the controlled hlmv instance
//-----------------------------------------------------------------------------
void MDLViewer::SendModelTransformToLinkedHlmv()
{
	if ( g_bHlmvMaster && g_HlmvIpcClient.Connect() )
	{
		matrix3x4_t m;
		g_pStudioModel->GetModelTransform( m );

		CUtlBuffer cmd;
		CUtlBuffer res;

		cmd.Printf( "%s %f %f %f %f %f %f %f %f %f %f %f %f",
			"hlmvModelTransform",
			m.m_flMatVal[0][0], m.m_flMatVal[0][1], m.m_flMatVal[0][2], m.m_flMatVal[0][3],
			m.m_flMatVal[1][0], m.m_flMatVal[1][1], m.m_flMatVal[1][2], m.m_flMatVal[1][3],
			m.m_flMatVal[2][0], m.m_flMatVal[2][1], m.m_flMatVal[2][2], m.m_flMatVal[2][3] );

		g_HlmvIpcClient.ExecuteCommand( cmd, res );

		g_HlmvIpcClient.Disconnect();
	}
}


//-----------------------------------------------------------------------------
// Sends the light rotation to the controlled hlmv instance
//-----------------------------------------------------------------------------
void MDLViewer::SendLightRotToLinkedHlmv()
{
	if ( g_bHlmvMaster && g_HlmvIpcClient.Connect() )
	{
		CUtlBuffer cmdLightRot;
		CUtlBuffer resLightRot;

		cmdLightRot.Printf( "%s %f %f %f",
			"hlmvLightRot",
			g_viewerSettings.lightrot[0], g_viewerSettings.lightrot[1], g_viewerSettings.lightrot[2] );

		g_HlmvIpcClient.ExecuteCommand( cmdLightRot, resLightRot );

		g_HlmvIpcClient.Disconnect();
	}
}


SpewRetval_t HLMVSpewFunc( SpewType_t spewType, char const *pMsg )
{
	g_bInError = true;
	switch (spewType)
	{
	case SPEW_ERROR:
		MessageBox(NULL, pMsg, "FATAL ERROR", MB_OK);
		g_bInError = false;
		return SPEW_ABORT;

	default:
		OutputDebugString(pMsg);
		g_bInError = false;
#ifdef _DEBUG
		return spewType == SPEW_ASSERT ? SPEW_DEBUGGER : SPEW_CONTINUE;
#else
		return SPEW_CONTINUE;
#endif
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CHLModelViewerApp : public CSteamAppSystemGroup
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();
};


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CHLModelViewerApp::Create()
{
	SpewOutputFunc( HLMVSpewFunc );

	g_dxlevel = CommandLine()->ParmValue( "-dx", 0 );
	g_bOldFileDialogs = ( CommandLine()->FindParm( "-olddialogs" ) != 0 );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "soundsystem.dll",		SOUNDSYSTEM_INTERFACE_VERSION },

		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	g_pMaterialSystemHardwareConfig = (IMaterialSystemHardwareConfig*)FindSystem( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION );
	g_pStudioRender = (IStudioRender*)FindSystem( STUDIO_RENDER_INTERFACE_VERSION );
	g_pDataCache = (IDataCache*)FindSystem( DATACACHE_INTERFACE_VERSION );
	g_pMDLCache = (IMDLCache*)FindSystem( MDLCACHE_INTERFACE_VERSION );
	g_pStudioDataCache = (IStudioDataCache*)FindSystem( STUDIO_DATA_CACHE_INTERFACE_VERSION ); 
	physcollision = (IPhysicsCollision *)FindSystem( VPHYSICS_COLLISION_INTERFACE_VERSION );
	physprop = (IPhysicsSurfaceProps *)FindSystem( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION );
	g_pSoundEmitterBase = (ISoundEmitterSystemBase *)FindSystem( SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
	g_pSoundSystem = (ISoundSystem *)FindSystem( SOUNDSYSTEM_INTERFACE_VERSION );

	if ( !g_pFileSystem || !physprop || !physcollision || !g_pMaterialSystem || !g_pStudioRender || !g_pMDLCache || !g_pDataCache )
	{
		Error("Unable to load required library interface!\n");
	}

	const char *pShaderDLL = CommandLine()->ParmValue("-shaderdll");
    const char *pArg;
	if ( CommandLine()->CheckParm( "-shaderapi", &pArg ))
	{
		pShaderDLL = pArg;
	}
	
	if(!pShaderDLL)
	{
		pShaderDLL = "shaderapidx9.dll";
	}

	g_pMaterialSystem->SetShaderAPI( pShaderDLL );

	g_Factory = GetFactory();

	return true;
}


void CHLModelViewerApp::Destroy()
{
	g_pFileSystem = NULL;
	g_pMaterialSystem = NULL;
	g_pMaterialSystemHardwareConfig = NULL;
	g_pStudioRender = NULL;
	g_pDataCache = NULL;
	g_pMDLCache = NULL;
	g_pStudioDataCache = NULL;
	physcollision = NULL;
	physprop = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CHLModelViewerApp::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConVar_Register( 0 );

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	// Get the adapter from the command line....
	const char *pAdapterString;
	int nAdapter = 0;
	if (CommandLine()->CheckParm( "-adapter", &pAdapterString ))
	{
		nAdapter = atoi( pAdapterString );
	}

	int nAdapterFlags = 0;
	if ( CommandLine()->CheckParm( "-ref" ) )
	{
		nAdapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}

	g_pMaterialSystem->SetAdapter( nAdapter, nAdapterFlags );

	g_bOldFileDialogs = true;
	if ( CommandLine()->FindParm( "-NoSteamdDialog" ) )
		g_bOldFileDialogs = false;
	
	LoadFileSystemDialogModule();

	return true; 
}

void CHLModelViewerApp::PostShutdown()
{
	UnloadFileSystemDialogModule();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CHLModelViewerApp::Main()
{
	g_pMaterialSystem->ModInit();
	g_pSoundEmitterBase->ModInit();

	g_pDataCache->SetSize( 64 * 1024 * 1024 );

	//mx::setDisplayMode (0, 0, 0);
	g_MDLViewer = new MDLViewer ();
	g_MDLViewer->setMenuBar (g_MDLViewer->getMenuBar ());

	g_pStudioModel->Init();
	g_pStudioModel->ModelInit();
	g_pStudioModel->ClearLookTargets( );

	// Load up the initial model
	const char *pMdlName = NULL;
	int nParmCount = CommandLine()->ParmCount();
	if ( nParmCount > 1 )
	{
		pMdlName = CommandLine()->GetParm( nParmCount - 1 );
	}

	if ( pMdlName && Q_stristr( pMdlName, ".mdl" ) )
	{
		char absPath[MAX_PATH];
		Q_MakeAbsolutePath( absPath, sizeof( absPath ), pMdlName );

		if ( CommandLine()->FindParm( "-screenshot" ) )
		{
			g_MDLViewer->SaveScreenShot( absPath );
		}
		else if ( CommandLine()->FindParm( "-dump" ) )
		{
			g_MDLViewer->DumpText( absPath );
		}
		else
		{
			g_MDLViewer->LoadModelFile( absPath );
		}
	}

	int nRetVal = mx::run ();

	g_pStudioModel->Shutdown();
	g_pMaterialSystem->ModShutdown();

	return nRetVal;
}

static bool CHLModelViewerApp_SuggestGameInfoDirFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	const char *pMdlName = NULL;
	int nParmCount = CommandLine()->ParmCount();
	if ( nParmCount > 1 )
	{
		pMdlName = CommandLine()->GetParm( nParmCount - 1 );
	}

	if ( pMdlName && Q_stristr( pMdlName, ".mdl" ) )
	{
		Q_MakeAbsolutePath( pchPathBuffer, nBufferLength, pMdlName );

		if ( pbBubbleDirectories )
			*pbBubbleDirectories = true;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//   Main entry point
//-----------------------------------------------------------------------------
int main (int argc, char *argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );
	mx::init( argc, argv );

	// make sure we start in the right directory
	char szName[256];
	strcpy( szName, mx::getApplicationPath() );
	// mx_setcwd (szName);

	// Set game info directory suggestion callback
	SetSuggestGameInfoDirFn( CHLModelViewerApp_SuggestGameInfoDirFn );

	CHLModelViewerApp hlmodelviewerApp;
	CSteamApplication steamApplication( &hlmodelviewerApp );
	return steamApplication.Run();
}
//
// Implementation of IPC server
//


CHlmvIpcServer::~CHlmvIpcServer()
{
	for ( int k = 0; k < m_lstCommands.Count(); ++ k )
	{
		delete [] m_lstCommands[k];
	}
	m_lstCommands.Purge();
}

bool CHlmvIpcServer::HasCommands()
{
	AUTO_LOCK( m_mtx );
	return m_lstCommands.Count() > 0;
}

void CHlmvIpcServer::AppendCommand( char *pszCommand )
{
	AUTO_LOCK( m_mtx );
	m_lstCommands.AddToTail( pszCommand );
}

char * CHlmvIpcServer::GetCommand()
{
	AUTO_LOCK( m_mtx );
	return m_lstCommands.Count() ? m_lstCommands[0] : "";
}

void CHlmvIpcServer::PopCommand()
{
	AUTO_LOCK( m_mtx );
	if ( m_lstCommands.Count() )
	{
		delete [] m_lstCommands[0];
		m_lstCommands.Remove( 0 );
	}
}

BOOL CHlmvIpcServer::ExecuteCommand(CUtlBuffer &cmd, CUtlBuffer &res)
{
	char *szCommand = ( char * ) cmd.Base();
	int nLen = strlen( szCommand );
	while ( nLen > 0 && V_isspace( szCommand[ nLen - 1 ] ) )
		-- nLen;

	if ( nLen <= 0 )
		return FALSE;

	char *pchCopy = new char[ nLen + 1 ];
	memcpy( pchCopy, szCommand, nLen );
	pchCopy[ nLen ] = 0;

	AppendCommand( pchCopy );

	res.PutInt( 0 );
	return TRUE;
}

