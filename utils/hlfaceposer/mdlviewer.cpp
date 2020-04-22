//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#include "cbase.h"
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <mxtk/mx.h>
#include <mxtk/mxTga.h>
#include <mxtk/mxEvent.h>
#include "mdlviewer.h"
#include "ViewerSettings.h"
#include "MatSysWin.h"
#include "ControlPanel.h"
#include "FlexPanel.h"
#include "StudioModel.h"
#include "mxExpressionTray.h"
#include "mxStatusWindow.h"
#include "ChoreoView.h"
#include "ifaceposersound.h"
#include "ifaceposerworkspace.h"
#include "expclass.h"
#include "PhonemeEditor.h"
#include "filesystem.h"
#include "ExpressionTool.h"
#include "ControlPanel.h"
#include "choreowidgetdrawhelper.h"
#include "choreoviewcolors.h"
#include "tabwindow.h"
#include "faceposer_models.h"
#include "choiceproperties.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "tier1/strtools.h"
#include "InputProperties.h"
#include "GestureTool.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "inputsystem/iinputsystem.h"
#include "RampTool.h"
#include "SceneRampTool.h"
#include "tier0/icommandline.h"
#include "phonemeextractor/PhonemeExtractor.h"
#include "animationbrowser.h"
#include "CloseCaptionTool.h"
#include "wavebrowser.h"
#include "vcdbrowser.h"
#include "ifilesystemopendialog.h"
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "appframework/appframework.h"
#include "icvar.h"
#include "vstdlib/cvar.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "vphysics_interface.h"
#include "Datacache/imdlcache.h"
#include "datacache/idatacache.h"
#include "filesystem_init.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier1/strtools.h"
#include "appframework/tier3app.h"
#include "faceposer_vgui.h"
#include "vguiwnd.h"
#include "vgui_controls/Frame.h"
#include "vgui/ISurface.h"
#include "p4lib/ip4.h"
#include "tier2/p4helpers.h"
#include "ProgressDialog.h"
#include "scriplib.h"

#define WINDOW_TAB_OFFSET 24

MDLViewer *g_MDLViewer = 0;
char g_appTitle[] = "Half-Life Face Poser";
static char recentFiles[8][256] = { "", "", "", "", "", "", "", "" };

using namespace vgui;

//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
IPhysicsSurfaceProps *physprop;
IPhysicsCollision *physcollision;
IStudioDataCache *g_pStudioDataCache;
vgui::ILocalize *g_pLocalize = NULL;
ISoundEmitterSystemBase *soundemitter = NULL;
CreateInterfaceFn g_Factory;
IFileSystem *g_pFileSystem = NULL;

bool g_bInError = false;

static char gamedir[MAX_PATH];  // full path to gamedir U:\main\game\ep2
static char gamedirsimple[MAX_PATH];  // just short name:  ep2

// Filesystem dialog module wrappers.
CSysModule *g_pFSDialogModule = 0;
CreateInterfaceFn g_FSDialogFactory = 0;

#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ComboBox.h"
#include "tier1/fmtstr.h"

class CFacePoserVguiFrame : public Frame
{
	DECLARE_CLASS_SIMPLE( CFacePoserVguiFrame, Frame );

public:
	CFacePoserVguiFrame( Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName )
	{
		SetTitle( panelName, true );

		SetTitleBarVisible( false );

		SetSizeable( false );
		SetMoveable( false );

		SetPaintBackgroundEnabled( true );
		SetCloseButtonVisible( false );
		m_pEntry = new TextEntry( this, "textentry" );
		m_pEntry->AddActionSignalTarget( this );
		m_pButton = new Button( this, "button", "Button1", this );
		m_pButton->SetCommand( new KeyValues( "OnButtonPressed" ) );
		m_pLabel = new Label( this, "label", "..." );

		m_pCombo = new ComboBox( this, "combo", 5, true );
		for ( int i = 0; i < 10; ++i )
		{
			m_pCombo->AddItem( CFmtStr( "item%02d", i + 1 ), NULL );
		}
	}

	MESSAGE_FUNC( OnButtonPressed, "OnButtonPressed" )
	{
		Msg( "OnButtonPressed\n" );
	}

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", str )
	{
		char sz[ 256 ];
		m_pEntry->GetText( sz, sizeof( sz ) );
		m_pLabel->SetText( sz );

		m_pCombo->GetText( sz, sizeof( sz ) );
		Msg( "Combo %s\n", sz );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int w, h;
		GetSize( w, h );

		int y = 30;
		int skip = 20;
		m_pEntry->SetBounds( 5, y, w, skip - 2 );
		y += skip;
		m_pButton->SetBounds( 5, y, w, skip - 2 );
		y += skip;
		m_pCombo->SetBounds( 5, y, w, skip - 2 );
		y += skip;
		m_pLabel->SetBounds( 5, y, w, skip - 2 );
		y += skip;

	}

private:

	TextEntry		*m_pEntry;
	Button			*m_pButton;
	Label			*m_pLabel;
	ComboBox		*m_pCombo;
};

class TestWindow : public CVGuiPanelWnd, public IFacePoserToolWindow
{
	typedef CVGuiPanelWnd BaseClass;

public:

	TestWindow( mxWindow *parent, int x, int y, int w, int h) : 
		BaseClass(parent, x, y, w, h ), 
		IFacePoserToolWindow( "FacePoser Frame", "FacePoser Frame" )
	{
		CFacePoserVguiFrame *f = new CFacePoserVguiFrame( NULL, "FacePoser Frame" );
		
		SetParentWindow( this );
		SetMainPanel( f );
		f->SetVisible( true );
		f->SetPaintBackgroundEnabled( true );

		FacePoser_MakeToolWindow( this, true );
	}

	virtual int handleEvent( mxEvent *event )
	{
		if ( HandleToolEvent( event ) )
			return 1;
		return BaseClass::handleEvent( event );
	}
};


//-----------------------------------------------------------------------------
// FIXME: Remove this crap (from cmdlib.cpp)
// We can't include cmdlib owing to appframework incompatibilities
//-----------------------------------------------------------------------------
void Q_mkdir( const char *path )
{
#if defined( _WIN32 ) || defined( WIN32 )
	if (_mkdir (path) != -1)
		return;
#else
	if (mkdir (path, 0777) != -1)
		return;
#endif
	if (errno != EEXIST)
	{
		Error ("mkdir %s: %s",path, strerror(errno));
	}
}

void CreatePath( const char *relative )
{
	char fullpath[ 512 ];
	Q_snprintf( fullpath, sizeof( fullpath ), "%s%s", GetGameDirectory(), relative );
 
	char *path = fullpath;

	char *ofs, c;

	if (path[1] == ':')
	{
		path += 2;
	}

	for (ofs = const_cast<char*>(path+1); *ofs ; ofs++)
	{
		c = *ofs;
		if (c == '/' || c == '\\')
		{	
			// create the directory, but not if it's actually a filename with a dot in it!!!
			*ofs = 0;
			if ( !Q_stristr( path, "." ) )
			{
				Q_mkdir (path);
			}
			*ofs = c;
		}
	}
}

//-----------------------------------------------------------------------------
// LoadFile
//-----------------------------------------------------------------------------
int LoadFile (const char *filename, void **bufferptr)
{
	FileHandle_t f = filesystem->Open( filename, "rb" );
	int length = filesystem->Size( f );
	void *buffer = malloc (length+1);
	((char *)buffer)[length] = 0;
	if ( filesystem->Read (buffer, length, f) != (int)length )
	{
		Error ("File read failure");
	}
	filesystem->Close (f);

	*bufferptr = buffer;
	return length;
}

char *ExpandPath(char *path)
{
	static char full[1024];
	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return path;

	V_sprintf_safe( full, "%s%s", gamedir, path );
	return full;
}


//-----------------------------------------------------------------------------
// This is here because scriplib.cpp is included in this project but cmdlib.cpp
// is not, but scriplib.cpp uses some stuff from cmdlib.cpp, same with
// LoadFile and ExpandPath above.  The only thing that currently uses this
// is $include in scriptlib, if this function returns 0, $include will
// behave the way it did before this change
//-----------------------------------------------------------------------------
int CmdLib_ExpandWithBasePaths( CUtlVector< CUtlString > &expandedPathList, const char *pszPath )
{
	return 0;
}


//-----------------------------------------------------------------------------
// FIXME: Move into appsystem framework
//-----------------------------------------------------------------------------
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
			mb->modify (IDC_FILE_RECENTFILES1 + i, IDC_FILE_RECENTFILES1 + i, recentFiles[i]);
		}
		else
		{
			mb->modify (IDC_FILE_RECENTFILES1 + i, IDC_FILE_RECENTFILES1 + i, "(empty)");
			mb->setEnabled (IDC_FILE_RECENTFILES1 + i, false);
		}
	}
}


#define RECENTFILESPATH "/hlfaceposer.rf"
void
MDLViewer::loadRecentFiles ()
{
	char path[256];
	strcpy (path, mx::getApplicationPath ());
	strcat (path, RECENTFILESPATH);
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
	strcat (path, RECENTFILESPATH);

	FILE *file = fopen (path, "wb");
	if (file)
	{
		fwrite (recentFiles, sizeof recentFiles, 1, file);
		fclose (file);
	}
}

bool MDLViewer::AreSoundScriptsDirty()
{
	// Save any changed sound script files
	int c = soundemitter->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		if ( soundemitter->IsSoundScriptDirty( i ) )
		{
			return true;
		}
	}
	return false;
}

bool MDLViewer::CanClose()
{
	Con_Printf( "Checking for vcd changes...\n" );

	if ( m_bVCDSaved )
	{
		int retval = mxMessageBox( NULL, "Rebuild scenes.image?", g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval == 2 )
		{
			return false;
		}

		m_bVCDSaved = false;
		if ( retval == 0 ) // YES
		{
			OnRebuildScenesImage();	
		}
	}

	Con_Printf( "Checking for sound script changes...\n" );

	// Save any changed sound script files
	int c = soundemitter->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		if ( !soundemitter->IsSoundScriptDirty( i ) )
			continue;

		char const *scriptname = soundemitter->GetSoundScriptName( i );
		if ( !scriptname )
			continue;

		if ( !filesystem->FileExists( scriptname ) ||
			 !filesystem->IsFileWritable( scriptname ) )
		{
			continue;
		}

		int retval = mxMessageBox( NULL, va( "Save changes to sound script '%s'?", scriptname ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval == 2 )
		{
			return false;
		}

		if ( retval == 0 )
		{
			soundemitter->SaveChangesToSoundScript( i );
		}
	}

	SaveWindowPositions();

	models->SaveModelList();
	models->CloseAllModels();

	return true;
}

bool MDLViewer::Closing( void )
{
	return true;
}

#define IDC_GRIDSETTINGS_FPS	1001
#define IDC_GRIDSETTINGS_SNAP	1002

class CFlatButton : public mxButton
{
public:
	CFlatButton( mxWindow *parent, int id )
		: mxButton( parent, 0, 0, 0, 0, "", id )
	{
		HWND wnd = (HWND)getHandle();
		DWORD exstyle = GetWindowLong( wnd, GWL_EXSTYLE );
		exstyle |= WS_EX_CLIENTEDGE;
		SetWindowLong( wnd, GWL_EXSTYLE, exstyle );

		DWORD style = GetWindowLong( wnd, GWL_STYLE );
		style &= ~WS_BORDER;
		SetWindowLong( wnd, GWL_STYLE, style );

	}
};

class CMDLViewerGridSettings : public mxWindow
{
public:
	typedef mxWindow BaseClass;

	CMDLViewerGridSettings( mxWindow *parent, int x, int y, int w, int h ) :
		mxWindow( parent, x, y, w, h )
	{
		FacePoser_AddWindowStyle( this, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
		m_btnFPS = new CFlatButton( this, IDC_GRIDSETTINGS_FPS );
		m_btnGridSnap = new CFlatButton( this, IDC_GRIDSETTINGS_SNAP );

	}

	void Init( void )
	{
		if ( g_pChoreoView )
		{
			CChoreoScene *scene = g_pChoreoView->GetScene();
			if ( scene )
			{
				char sz[ 256 ];
				Q_snprintf( sz, sizeof( sz ), "%i fps", scene->GetSceneFPS() );
				m_btnFPS->setLabel( sz );

				Q_snprintf( sz, sizeof( sz ), "snap: %s", scene->IsUsingFrameSnap() ? "on" : "off" );
				m_btnGridSnap->setLabel( sz );

				m_btnFPS->setVisible( true );
				m_btnGridSnap->setVisible( true );
				return;
			}
		}
		
		m_btnFPS->setVisible( false );
		m_btnGridSnap->setVisible( false );
	}

	virtual int handleEvent( mxEvent *event )
	{
		int iret = 0;
		switch ( event->event )
		{
		default:
			break;
		case mxEvent::Size:
			{
				int leftedge = w2() * 0.45f;
				m_btnFPS->setBounds( 0, 0, leftedge, h2() );
				m_btnGridSnap->setBounds( leftedge, 0, w2() - leftedge, h2() );
				iret = 1;
			}
			break;
		case mxEvent::Action:
			{
				iret = 1;
				switch ( event->action )
				{
				default:
					iret = 0;
					break;
				case IDC_GRIDSETTINGS_FPS:
					{
						if ( g_pChoreoView )
						{
							CChoreoScene *scene = g_pChoreoView->GetScene();
							if ( scene )
							{
								int currentFPS = scene->GetSceneFPS();
								
								CInputParams params;
								memset( &params, 0, sizeof( params ) );
								
								strcpy( params.m_szDialogTitle, "Change FPS" );
								
								Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ),
									"%i", currentFPS );
								
								strcpy( params.m_szPrompt, "Current FPS:" );
								
								if ( InputProperties( &params ) )
								{
									int newFPS = atoi( params.m_szInputText );
									
									if ( ( newFPS > 0 ) && ( newFPS != currentFPS ) )
									{
										g_pChoreoView->SetDirty( true );
										g_pChoreoView->PushUndo( "Change Scene FPS" );
										scene->SetSceneFPS( newFPS );
										g_pChoreoView->PushRedo( "Change Scene FPS" );
										Init();

										Con_Printf( "FPS changed to %i\n", newFPS );
									}
								}
								
							}
						}
					}
					break;
				case IDC_GRIDSETTINGS_SNAP:
					{
						if ( g_pChoreoView )
						{
							CChoreoScene *scene = g_pChoreoView->GetScene();
							if ( scene )
							{
								g_pChoreoView->SetDirty( true );
								g_pChoreoView->PushUndo( "Change Snap Frame" );
								
								scene->SetUsingFrameSnap( !scene->IsUsingFrameSnap() );
								
								g_pChoreoView->PushRedo( "Change Snap Frame" );
								
								Init();

								Con_Printf( "Time frame snapping: %s\n",
									scene->IsUsingFrameSnap() ? "on" : "off" );
							}
						}


					}
					break;
				}
			}
		}
		return iret;
	}

	bool PaintBackground( void )
	{
		CChoreoWidgetDrawHelper drawHelper( this );
		RECT rc;
		drawHelper.GetClientRect( rc );
		drawHelper.DrawFilledRect( GetSysColor( COLOR_BTNFACE ), rc );
		return false;
	}
	
private:

	CFlatButton	*m_btnFPS;
	CFlatButton	*m_btnGridSnap;
};


#define IDC_MODELTAB_LOAD			1000
#define IDC_MODELTAB_CLOSE			1001
#define IDC_MODELTAB_CLOSEALL		1002
#define IDC_MODELTAB_CENTERONFACE	1003
#define IDC_MODELTAB_ASSOCIATEACTOR 1004
#define IDC_MODELTAB_TOGGLE3DVIEW	1005
#define IDC_MODELTAB_SHOWALL		1006
#define IDC_MODELTAB_HIDEALL		1007
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMDLViewerModelTab : public CTabWindow
{
public:
	typedef CTabWindow BaseClass;

	CMDLViewerModelTab( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 ) :
		CTabWindow( parent, x, y, w, h, id, style )
	{
		SetInverted( true );
	}

	virtual void ShowRightClickMenu( int mx, int my )
	{
		mxPopupMenu *pop = new mxPopupMenu();
		Assert( pop );

		char const *current = "";
		char const *filename = "";
		int idx = getSelectedIndex();
		if ( idx >= 0 )
		{
			current = models->GetModelName( idx );
			filename = models->GetModelFileName( idx );
		}

		if ( models->Count() < MAX_FP_MODELS )
		{
			pop->add( "Load Model...", IDC_MODELTAB_LOAD );
		}
		if ( idx >= 0 )
		{
			pop->add( va( "Close '%s'", current ), IDC_MODELTAB_CLOSE );
		}
		if ( models->Count() > 0 )
		{
			pop->add( "Close All", IDC_MODELTAB_CLOSEALL );
		}
		if ( idx >= 0 )
		{
			pop->addSeparator();
			pop->add( va( "Center %s's face", current ), IDC_MODELTAB_CENTERONFACE );

			CChoreoScene *scene = g_pChoreoView->GetScene();
			if ( scene )
			{
				// See if there is already an actor with this model associated
				int c = scene->GetNumActors();
				bool hasassoc = false;
				for ( int i = 0; i < c; i++ )
				{
					CChoreoActor *a = scene->GetActor( i );
					Assert( a );
				
					if ( stricmp( a->GetFacePoserModelName(), filename ) )
						continue;
					hasassoc = true;
					break;
				}

				if ( hasassoc )
				{
					pop->add( va( "Change associated actor for %s", current ), IDC_MODELTAB_ASSOCIATEACTOR );
				}
				else
				{
					pop->add( va( "Associate actor to %s", current ), IDC_MODELTAB_ASSOCIATEACTOR );
				}
			}

			pop->addSeparator();

			bool visible = models->IsModelShownIn3DView( idx );
			if ( visible )
			{
				pop->add( va( "Remove %s from 3D View", current ), IDC_MODELTAB_TOGGLE3DVIEW );
			}
			else
			{
				pop->add( va( "Show %s in 3D View", current ), IDC_MODELTAB_TOGGLE3DVIEW );
			}
		}
		if ( models->Count() > 0 )
		{
			pop->addSeparator();
			pop->add( "Show All", IDC_MODELTAB_SHOWALL );
			pop->add( "Hide All", IDC_MODELTAB_HIDEALL );
		}

		// Convert click position
		POINT pt;
		pt.x = mx;
		pt.y = my;

		// Convert coordinate space
		pop->popup( this, pt.x, pt.y );
	}

	virtual int handleEvent( mxEvent *event )
	{
		int iret = 0;
		switch ( event->event )
		{
		default:
			break;
		case mxEvent::Action:
			{
				iret = 1;
				switch ( event->action )
				{
				default:
					iret = 0;
					break;
				case IDC_MODELTAB_SHOWALL:
				case IDC_MODELTAB_HIDEALL:
					{
						bool show = ( event->action == IDC_MODELTAB_SHOWALL ) ? true : false;
						int c = models->Count();
						for ( int i = 0; i < c ; i++ )
						{
							models->ShowModelIn3DView( i, show );
						}
					}
					break;
				case IDC_MODELTAB_LOAD:
					{
						if ( ! CommandLine()->FindParm( "-NoSteamDialog" ) )
						{
							g_MDLViewer->LoadModel_Steam();
						}
						else
						{
							char modelfile[ 512 ];
							if ( FacePoser_ShowOpenFileNameDialog( modelfile, sizeof( modelfile ), "models", "*.mdl" ) )
							{
								g_MDLViewer->LoadModelFile( modelfile );
							}
						}
					}
					break;
				case IDC_MODELTAB_CLOSE:
					{
						int idx = getSelectedIndex();
						if ( idx >= 0 )
						{
							models->FreeModel( idx );
						}
					}
					break;
				case IDC_MODELTAB_CLOSEALL:
					{
						models->CloseAllModels();
					}
					break;
				case IDC_MODELTAB_CENTERONFACE:
					{
						g_pControlPanel->CenterOnFace();
					}
					break;
				case IDC_MODELTAB_TOGGLE3DVIEW:
					{
						int idx = getSelectedIndex();
						if ( idx >= 0 )
						{
							bool visible = models->IsModelShownIn3DView( idx );
							models->ShowModelIn3DView( idx, !visible );
						}
					}
					break;
				case IDC_MODELTAB_ASSOCIATEACTOR:
					{
						int idx = getSelectedIndex();
						if ( idx >= 0 )
						{
							char const *modelname = models->GetModelFileName( idx );

							CChoreoScene *scene = g_pChoreoView->GetScene();
							if ( scene )
							{
								CChoiceParams params;
								strcpy( params.m_szDialogTitle, "Associate Actor" );

								params.m_bPositionDialog = false;
								params.m_nLeft = 0;
								params.m_nTop = 0;
								strcpy( params.m_szPrompt, "Choose actor:" );

								params.m_Choices.RemoveAll();

								params.m_nSelected = -1;
								int oldsel = -1;

								int c = scene->GetNumActors();
								ChoiceText text;
								for ( int i = 0; i < c; i++ )
								{
									CChoreoActor *a = scene->GetActor( i );
									Assert( a );

									
									strcpy( text.choice, a->GetName() );

									if ( !stricmp( a->GetFacePoserModelName(), modelname ) )
									{
										params.m_nSelected = i;
										oldsel = -1;
									}

									params.m_Choices.AddToTail( text );
								}
		
								if ( ChoiceProperties( &params ) && 
									params.m_nSelected != oldsel )
								{
									
									// Chose something new...
									CChoreoActor *a = scene->GetActor( params.m_nSelected );
									
									g_pChoreoView->AssociateModelToActor( a, idx );
								}
							}
						}

					}
				}
			}
			break;
		}
		if ( iret )
			return iret;
		return BaseClass::handleEvent( event );
	}


	void HandleModelSelect( void )
	{
		int idx = getSelectedIndex();
		if ( idx < 0 )
			return;

		// FIXME: Do any necessary window resetting here!!!
		g_pControlPanel->ChangeModel( models->GetModelFileName( idx ) );
	}

	void Init( void )
	{
		removeAll();
		
		int c = models->Count();
		int i;
		for ( i = 0; i < c ; i++ )
		{
			char const *name = models->GetModelName( i );

			// Strip it down to the base name
			char cleanname[ 256 ];
			Q_FileBase( name, cleanname, sizeof( cleanname ) );

			add( cleanname );
		}
	}
};

#define IDC_TOOL_TOGGLEVISIBILITY	1000
#define IDC_TOOL_TOGGLELOCK			1001
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMDLViewerWindowTab : public CTabWindow
{
public:
	typedef CTabWindow BaseClass;

	CMDLViewerWindowTab( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 ) :
		CTabWindow( parent, x, y, w, h, id, style )
	{
		SetInverted( true );

		m_nLastSelected = -1;
		m_flLastSelectedTime = -1;
	}

	virtual void ShowRightClickMenu( int mx, int my )
	{
		IFacePoserToolWindow *tool = GetSelectedTool();
		if ( !tool )
			return;

		mxWindow *toolw = tool->GetMxWindow();
		if ( !toolw )
			return;

		mxPopupMenu *pop = new mxPopupMenu();
		Assert( pop );

		bool isVisible = toolw->isVisible();
		bool isLocked = tool->IsLocked();

		pop->add( isVisible ? "Hide" : "Show", IDC_TOOL_TOGGLEVISIBILITY );
		pop->add( isLocked ? "Unlock" : "Lock", IDC_TOOL_TOGGLELOCK );

		// Convert click position
		POINT pt;
		pt.x = mx;
		pt.y = my;

		/*
		ClientToScreen( (HWND)getHandle(), &pt );
		ScreenToClient( (HWND)g_MDLViewer->getHandle(), &pt );
		*/

		// Convert coordinate space
		pop->popup( this, pt.x, pt.y );
	}

	virtual int	handleEvent( mxEvent *event )
	{
		int iret = 0;
		switch ( event->event )
		{
		case mxEvent::Action:
			{
				iret = 1;
				switch ( event->action )
				{
				default:
					iret = 0;
					break;
				case IDC_TOOL_TOGGLEVISIBILITY:
					{
						IFacePoserToolWindow *tool = GetSelectedTool();
						if ( tool )
						{
							mxWindow *toolw = tool->GetMxWindow();
							if ( toolw )
							{
								toolw->setVisible( !toolw->isVisible() );
								g_MDLViewer->UpdateWindowMenu();
							}
						}
					}
					break;
				case IDC_TOOL_TOGGLELOCK:
					{
						IFacePoserToolWindow *tool = GetSelectedTool();
						if ( tool )
						{
							tool->ToggleLockedState();
						}
					}
					break;
				}
			}
			break;
		default:
			break;
		}
		if ( iret )
			return iret;
		return BaseClass::handleEvent( event );
	}

	void	Init( void )
	{
		int c = IFacePoserToolWindow::GetToolCount();
		int i;
		for ( i = 0; i < c ; i++ )
		{
			IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
			add( tool->GetDisplayNameRoot() );
		}
	}

#define WINDOW_DOUBLECLICK_TIME 0.4

	void	HandleWindowSelect( void )
	{
		extern double realtime;
		IFacePoserToolWindow *tool = GetSelectedTool();
		if ( !tool )
			return;

		bool doubleclicked = false;

		double curtime = realtime;
		int clickedItem = getSelectedIndex();

		if ( clickedItem == m_nLastSelected )
		{
			if ( curtime < m_flLastSelectedTime + WINDOW_DOUBLECLICK_TIME )
			{
				doubleclicked = true;
			}
		}

		m_flLastSelectedTime = curtime;
		m_nLastSelected = clickedItem;

		mxWindow *toolw = tool->GetMxWindow();
		if ( !toolw )
			return;

		if ( doubleclicked )
		{
			toolw->setVisible( !toolw->isVisible() );
			m_flLastSelectedTime = -1;
		}

		if ( !toolw->isVisible() )
		{
			return;
		}

		// Move window to front
		HWND wnd = (HWND)tool->GetMxWindow()->getHandle();
		SetFocus( wnd );
		SetWindowPos( wnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	}

private:

	IFacePoserToolWindow *GetSelectedTool()
	{
		int idx = getSelectedIndex();
		int c = IFacePoserToolWindow::GetToolCount();
	
		if ( idx < 0 || idx >= c )
			return NULL;

		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( idx );
		return tool;
	}

	// HACKY double click handler
	int		m_nLastSelected;
	double	m_flLastSelectedTime;
};

//-----------------------------------------------------------------------------
// Purpose: The workspace is the parent of all of the tool windows
//-----------------------------------------------------------------------------
class CMDLViewerWorkspace : public mxWindow
{
public:
	CMDLViewerWorkspace( mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int style = 0)
		: mxWindow( parent, x, y, w, h, label, style )
	{
		FacePoser_AddWindowStyle( this, WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool PaintBackground( void )
	{
		CChoreoWidgetDrawHelper drawHelper( this );
		RECT rc;
		drawHelper.GetClientRect( rc );
		drawHelper.DrawFilledRect( GetSysColor( COLOR_APPWORKSPACE ), rc );
		return false;
	}
};

void MDLViewer::LoadPosition( void )
{
	bool visible;
	bool locked;
	bool zoomed;
	int x, y, w, h;

	FacePoser_LoadWindowPositions( "MDLViewer", visible, x, y, w, h, locked, zoomed );

	if ( w == 0 || h == 0 )
	{
		zoomed = true;
		visible = true;
	}

	setBounds( x, y, w, h );
	if ( zoomed )
	{
		ShowWindow( (HWND)getHandle(), SW_SHOWMAXIMIZED );
	}
	else
	{
		setVisible( visible );
	}
}

void MDLViewer::SavePosition( void )
{
	bool visible;
	int xpos, ypos, width, height;

	visible = isVisible();
	xpos = x();
	ypos = y();
	width = w();
	height = h();

	// xpos and ypos are screen space
	POINT pt;
	pt.x = xpos;
	pt.y = ypos;

	// Convert from screen space to relative to client area of parent window so
	//  the setBounds == MoveWindow call will offset to the same location
	if ( getParent() )
	{
		ScreenToClient( (HWND)getParent()->getHandle(), &pt );
		xpos = (short)pt.x;
		ypos = (short)pt.y;
	}

	bool zoomed = IsZoomed( (HWND)getHandle() ) ? true : false;

	bool iconic = IsIconic( (HWND)getHandle() ) ? true : false;

	// Don't reset values if it's minimized during shutdown
	if ( iconic )
		return;

	FacePoser_SaveWindowPositions( "MDLViewer", visible, xpos, ypos, width, height, false, zoomed );
}

MDLViewer::MDLViewer () : 
	mxWindow (0, 0, 0, 0, 0, g_appTitle, mxWindow::Normal),
	menuCloseCaptionLanguages(0),
	m_bOldSoundScriptsDirty( -1 ),
	m_bVCDSaved( false )
{
	int i;

	g_MDLViewer = this;

	FacePoser_MakeToolWindow( this, false );

	workspace = new CMDLViewerWorkspace( this, 0, 0, 500, 500, "" );
	windowtab = new CMDLViewerWindowTab( this, 0, 500, 500, 20, IDC_WINDOW_TAB );
	modeltab = new CMDLViewerModelTab( this, 500, 500, 200, 20, IDC_MODEL_TAB );
	gridsettings = new CMDLViewerGridSettings( this, 0, 500, 500, 20 );
	modeltab->SetRightJustify( true );

	g_pStatusWindow = new mxStatusWindow( workspace, 0, 0, 1024, 150, "" );
	g_pStatusWindow->setVisible( true );

	InitViewerSettings( "faceposer" );
	g_viewerSettings.speechapiindex = SPEECH_API_LIPSINC;
	g_viewerSettings.m_iEditAttachment = -1;

	LoadViewerRootSettings( );

	LoadPosition();
	// ShowWindow( (HWND)getHandle(), SW_SHOWMAXIMIZED );

	g_pStatusWindow->setBounds(  0, h2() - 150, w2(), 150 );

	Con_Printf( "MDLViewer started\n" );

	Con_Printf( "Creating menu bar\n" );

	// create menu stuff
	mb = new mxMenuBar (this);
	menuFile = new mxMenu ();
	menuOptions = new mxMenu ();
	menuWindow = new mxMenu ();
	menuHelp = new mxMenu ();
	menuEdit = new mxMenu ();
	menuExpressions = new mxMenu();
	menuChoreography = new mxMenu();

	mb->addMenu ("File", menuFile);
	//mb->addMenu( "Edit", menuEdit );
	mb->addMenu ("Options", menuOptions);
	mb->addMenu ( "Expression", menuExpressions );
	mb->addMenu ( "Choreography", menuChoreography );
	mb->addMenu ("Window", menuWindow);
	mb->addMenu ("Help", menuHelp);

	mxMenu *menuRecentFiles = new mxMenu ();
	menuRecentFiles->add ("(empty)", IDC_FILE_RECENTFILES1);
	menuRecentFiles->add ("(empty)", IDC_FILE_RECENTFILES2);
	menuRecentFiles->add ("(empty)", IDC_FILE_RECENTFILES3);
	menuRecentFiles->add ("(empty)", IDC_FILE_RECENTFILES4);

	menuFile->add ("Load Model...", IDC_FILE_LOADMODEL);
	menuFile->add( "Refresh\tF5", IDC_FILE_REFRESH );

	menuFile->addSeparator();
	menuFile->add ("Save Sound Changes...", IDC_FILE_SAVESOUNDSCRIPTCHANGES );
	menuFile->add( "Rebuild scenes.image...", IDC_FILE_REBUILDSCENESIMAGE );

	menuFile->addSeparator();

	menuFile->add ("Load Background Texture...", IDC_FILE_LOADBACKGROUNDTEX);
	menuFile->add ("Load Ground Texture...", IDC_FILE_LOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->add ("Unload Ground Texture", IDC_FILE_UNLOADGROUNDTEX);
	menuFile->addSeparator ();
	menuFile->addMenu ("Recent Files", menuRecentFiles);
	menuFile->addSeparator ();
	menuFile->add ("Exit", IDC_FILE_EXIT);

	menuFile->setEnabled(IDC_FILE_LOADBACKGROUNDTEX, false);
	menuFile->setEnabled(IDC_FILE_LOADGROUNDTEX, false);
	menuFile->setEnabled(IDC_FILE_UNLOADGROUNDTEX, false);
	menuFile->setEnabled(IDC_FILE_SAVESOUNDSCRIPTCHANGES, false);

	menuOptions->add ("Background Color...", IDC_OPTIONS_COLORBACKGROUND);
	menuOptions->add ("Ground Color...", IDC_OPTIONS_COLORGROUND);
	menuOptions->add ("Light Color...", IDC_OPTIONS_COLORLIGHT);

	{
		menuCloseCaptionLanguages = new mxMenu();

		for ( int i = 0; i < CC_NUM_LANGUAGES; i++ )
		{
			int id = IDC_OPTIONS_LANGUAGESTART + i;
			menuCloseCaptionLanguages->add( CSentence::NameForLanguage( i ), id );
		}

		menuOptions->addSeparator();
		menuOptions->addMenu( "CC Language", menuCloseCaptionLanguages );
	}

	menuOptions->addSeparator ();
	menuOptions->add ("Center View", IDC_OPTIONS_CENTERVIEW);
	menuOptions->add ("Center on Face", IDC_OPTIONS_CENTERONFACE );
#ifdef WIN32
	menuOptions->addSeparator ();
	menuOptions->add ("Make Screenshot...", IDC_OPTIONS_MAKESCREENSHOT);
	//menuOptions->add ("Dump Model Info", IDC_OPTIONS_DUMP);
	menuOptions->addSeparator ();
	menuOptions->add ("Clear model sounds.", IDC_OPTIONS_CLEARMODELSOUNDS );

#endif

	menuExpressions->add( "New...", IDC_EXPRESSIONS_NEW );
	menuExpressions->addSeparator ();
	menuExpressions->add( "Load...", IDC_EXPRESSIONS_LOAD );
	menuExpressions->add( "Save", IDC_EXPRESSIONS_SAVE );
	menuExpressions->addSeparator ();
	menuExpressions->add( "Export to VFE", IDC_EXPRESSIONS_EXPORT );
	menuExpressions->addSeparator ();
	menuExpressions->add( "Close class", IDC_EXPRESSIONS_CLOSE );
	menuExpressions->add( "Close all classes", IDC_EXPRESSIONS_CLOSEALL );
	menuExpressions->addSeparator();
	menuExpressions->add( "Recreate all bitmaps", IDC_EXPRESSIONS_REDOBITMAPS );

	menuChoreography->add( "New...", IDC_CHOREOSCENE_NEW );
	menuChoreography->addSeparator();
	menuChoreography->add( "Load...", IDC_CHOREOSCENE_LOAD );
	menuChoreography->add( "Save", IDC_CHOREOSCENE_SAVE );
	menuChoreography->add( "Save As...", IDC_CHOREOSCENE_SAVEAS );
	menuChoreography->addSeparator();
	menuChoreography->add( "Close", IDC_CHOREOSCENE_CLOSE );
	menuChoreography->addSeparator();
	menuChoreography->add( "Add Actor...", IDC_CHOREOSCENE_ADDACTOR );
	menuChoreography->addSeparator();
	menuChoreography->add( "Load Next", IDC_CHOREOSCENE_LOADNEXT );

#ifdef WIN32
	menuHelp->add ("Goto Homepage...", IDC_HELP_GOTOHOMEPAGE);
	menuHelp->addSeparator ();
#endif
	menuHelp->add ("About...", IDC_HELP_ABOUT);

	// create the Material System window
	Con_Printf( "Creating 3D View\n" );
	g_pMatSysWindow = new MatSysWindow (workspace, 0, 0, 100, 100, "", mxWindow::Normal);

	Con_Printf( "Creating Close Caption tool" );
	g_pCloseCaptionTool = new CloseCaptionTool( workspace );

	Con_Printf( "Creating control panel\n" );
	g_pControlPanel = new ControlPanel (workspace);

	Con_Printf( "Creating phoneme editor\n" );
	g_pPhonemeEditor = new PhonemeEditor( workspace );

	Con_Printf( "Creating expression tool\n" );
	g_pExpressionTool = new ExpressionTool( workspace );

	Con_Printf( "Creating gesture tool\n" );
	g_pGestureTool = new GestureTool( workspace );

	Con_Printf( "Creating ramp tool\n" );
	g_pRampTool = new RampTool( workspace );

	Con_Printf( "Creating scene ramp tool\n" );
	g_pSceneRampTool = new SceneRampTool( workspace );

	Con_Printf( "Creating expression tray\n" );
	g_pExpressionTrayTool = new mxExpressionTray( workspace, IDC_EXPRESSIONTRAY );

	Con_Printf( "Creating animation browser\n" );
	g_pAnimationBrowserTool = new AnimationBrowser( workspace, IDC_ANIMATIONBROWSER );

	Con_Printf( "Creating flex slider window\n" );
	g_pFlexPanel = new FlexPanel( workspace );

	Con_Printf( "Creating wave browser\n" );
	g_pWaveBrowser = new CWaveBrowser( workspace );

	Con_Printf( "Creating VCD browser\n" );
	g_pVCDBrowser = new CVCDBrowser( workspace );

	Con_Printf( "Creating choreography view\n" );
	g_pChoreoView = new CChoreoView( workspace, 200, 200, 400, 300, 0 );
	// Choreo scene file drives main window title name
	g_pChoreoView->SetUseForMainWindowTitle( true );
#if 0
	new TestWindow( workspace, 100, 100, 256, 256 );
#endif

	Con_Printf( "IFacePoserToolWindow::Init\n" );

	IFacePoserToolWindow::InitTools();

	Con_Printf( "windowtab->Init\n" );
	
	windowtab->Init();

	Con_Printf( "loadRecentFiles\n" );

	loadRecentFiles ();
	initRecentFiles ();

	Con_Printf( "RestoreThumbnailSize\n" );

	g_pExpressionTrayTool->RestoreThumbnailSize();
	g_pAnimationBrowserTool->RestoreThumbnailSize();

	Con_Printf( "Add Tool Windows\n" );

	int c = IFacePoserToolWindow::GetToolCount();
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		menuWindow->add( tool->GetToolName(), IDC_WINDOW_FIRSTTOOL + i );
	}

	menuWindow->addSeparator();
	menuWindow->add( "Cascade", IDC_WINDOW_CASCADE );
	menuWindow->addSeparator();
	menuWindow->add( "Tile", IDC_WINDOW_TILE );
	menuWindow->add( "Tile Horizontally", IDC_WINDOW_TILE_HORIZ );
	menuWindow->add( "Tile Vertically", IDC_WINDOW_TILE_VERT );
	menuWindow->addSeparator();
	menuWindow->add( "Hide All", IDC_WINDOW_HIDEALL );
	menuWindow->add( "Show All", IDC_WINDOW_SHOWALL );

	Con_Printf( "UpdateWindowMenu\n" );

	UpdateWindowMenu();
	// Check the default item
	UpdateLanguageMenu( g_viewerSettings.cclanguageid );

	m_nCurrentFrame = 0;

	Con_Printf( "gridsettings->Init()\n" );

	gridsettings->Init();

	Con_Printf( "LoadWindowPositions\n" );

	LoadWindowPositions();

	Con_Printf( "Model viewer created\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MDLViewer::UpdateWindowMenu( void )
{
	int c = IFacePoserToolWindow::GetToolCount();
	for ( int i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		menuWindow->setChecked( IDC_WINDOW_FIRSTTOOL + i, tool->GetMxWindow()->isVisible() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : currentLanguageId - 
//-----------------------------------------------------------------------------
void MDLViewer::UpdateLanguageMenu( int currentLanguageId )
{
	if ( !menuCloseCaptionLanguages )
		return;

	for ( int i = 0; i < CC_NUM_LANGUAGES; i++ )
	{
		int id = IDC_OPTIONS_LANGUAGESTART + i;
		menuCloseCaptionLanguages->setChecked( id, i == currentLanguageId ? true : false );
	}
}

void MDLViewer::OnDelete()
{
	saveRecentFiles ();
	SaveViewerRootSettings( );

#ifdef WIN32
	DeleteFile ("hlmv.cfg");
	DeleteFile ("midump.txt");
#endif

	IFacePoserToolWindow::ShutdownTools();

	g_MDLViewer = NULL;
}

MDLViewer::~MDLViewer ()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MDLViewer::InitModelTab( void )
{
	modeltab->Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MDLViewer::InitGridSettings( void )
{
	gridsettings->Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int MDLViewer::GetActiveModelTab( void )
{
	return modeltab->getSelectedIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : modelindex - 
//-----------------------------------------------------------------------------
void MDLViewer::SetActiveModelTab( int modelindex )
{
	modeltab->select( modelindex );
	modeltab->HandleModelSelect();
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the currently loaded model file.
//-----------------------------------------------------------------------------
void MDLViewer::Refresh( void )
{
	Con_ColorPrintf( RGB( 0, 125, 255 ), "Refreshing...\n" );

	bool reinit_soundemitter = true;

	// Save any changed sound script files
	int c = soundemitter->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		if ( !soundemitter->IsSoundScriptDirty( i ) )
			continue;

		char const *scriptname = soundemitter->GetSoundScriptName( i );
		if ( !scriptname )
			continue;

		if ( !filesystem->FileExists( scriptname ) ||
			 !filesystem->IsFileWritable( scriptname ) )
		{
			continue;
		}

		int retval = mxMessageBox( NULL, va( "Save changes to sound script '%s'?", scriptname ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval != 0 )
		{
			reinit_soundemitter = false;
			continue;
		}

		if ( retval == 0 )
		{
			soundemitter->SaveChangesToSoundScript( i );
			Con_ColorPrintf( RGB( 50, 255, 100 ), "  saving changes to script file '%s'\n", scriptname );
		}
	}

	// kill the soundemitter system
	if ( reinit_soundemitter )
	{
		soundemitter->Shutdown();
	}


	Con_ColorPrintf( RGB( 50, 255, 100 ), "  reloading textures\n" );
	g_pMaterialSystem->ReloadTextures();

	models->ReleaseModels();

	Con_ColorPrintf( RGB( 50, 255, 100 ), "  reloading models\n" );
	models->RestoreModels();

	// restart the soundemitter system
	if ( reinit_soundemitter )
	{
		Con_ColorPrintf( RGB( 50, 255, 100 ), "  reloading sound emitter system\n" );
		soundemitter->Init();
	}
	else
	{
		Con_ColorPrintf( RGB( 250, 50, 50 ), "  NOT reloading sound emitter system\n" );
	}

	Con_ColorPrintf( RGB( 0, 125, 255 ), "done.\n" );
}

void MDLViewer::OnFileLoaded( char const *pszFile )
{
	int i;
	for (i = 0; i < 8; i++)
	{
		if (!Q_stricmp( recentFiles[i], pszFile ))
			break;
	}

	// swap existing recent file
	if (i < 8)
	{
		char tmp[256];
		strcpy (tmp, recentFiles[0]);
		strcpy (recentFiles[0], recentFiles[i]);
		strcpy (recentFiles[i], tmp);
	}

	// insert recent file
	else
	{
		for (i = 7; i > 0; i--)
			strcpy (recentFiles[i], recentFiles[i - 1]);

		strcpy( recentFiles[0], pszFile );
	}

	initRecentFiles ();

	if ( g_pVCDBrowser )
	{
		g_pVCDBrowser->SetCurrent( pszFile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads the file and updates the MRU list.
// Input  : pszFile - File to load.
//-----------------------------------------------------------------------------
void MDLViewer::LoadModelFile( const char *pszFile )
{
	models->LoadModel( pszFile );

	OnFileLoaded( pszFile );

	g_pControlPanel->CenterOnFace();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *wnd - 
//			x - 
//			y - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool WindowContainsPoint( mxWindow *wnd, int x, int y )
{
	POINT pt;
	pt.x = (short)x;
	pt.y = (short)y;

	HWND window = (HWND)wnd->getHandle();
	if ( !window )
		return false;

	ScreenToClient( window, &pt );

	if ( pt.x < 0 )
		return false;
	if ( pt.y < 0 )
		return false;
	if ( pt.x > wnd->w() )
		return false;
	if ( pt.y > wnd->h() )
		return false;

	return true;
}


void MDLViewer::LoadModel_Steam()
{
	if ( !g_FSDialogFactory )
		return;

	IFileSystemOpenDialog *pDlg;
	pDlg = (IFileSystemOpenDialog*)g_FSDialogFactory( FILESYSTEMOPENDIALOG_VERSION, NULL );
	if ( !pDlg )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "Can't create %s interface.", FILESYSTEMOPENDIALOG_VERSION );
		::MessageBox( NULL, str, "Error", MB_OK );
		return;
	}
	pDlg->Init( g_Factory, NULL );
	pDlg->AddFileMask( "*.jpg" );
	pDlg->AddFileMask( "*.mdl" );
	pDlg->SetInitialDir( "models", "game" );
	pDlg->SetFilterMdlAndJpgFiles( true );

	if (pDlg->DoModal() == IDOK)
	{
		char filename[MAX_PATH];
		pDlg->GetFilename( filename, sizeof( filename ) );
		LoadModelFile( filename );
	}

	pDlg->Release();
}



int MDLViewer::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	switch (event->event)
	{
	case mxEvent::Size:
		{
			int width = w2();
			int height = h2();

			windowtab->SetRowHeight( WINDOW_TAB_OFFSET - 2 );
			modeltab->SetRowHeight( WINDOW_TAB_OFFSET - 2 );

			int gridsettingswide = 100;
			int gridstart = width - gridsettingswide - 5;

			int modelwide = gridstart / 3;
			int windowwide = gridstart - modelwide;

			int rowheight = max( windowtab->GetBestHeight( windowwide ), modeltab->GetBestHeight( modelwide ) );

			workspace->setBounds( 0, 0, width, height - rowheight );

			gridsettings->setBounds( gridstart, height - rowheight + 1, gridsettingswide, WINDOW_TAB_OFFSET - 2 );

			windowtab->setBounds( 0, height - rowheight, windowwide, rowheight );
			modeltab->setBounds( windowwide, height - rowheight, modelwide, rowheight );

			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch (event->action)
			{
			case IDC_WINDOW_TAB:
				{
					windowtab->HandleWindowSelect();
				}
				break;
			case IDC_MODEL_TAB:
				{
					modeltab->HandleModelSelect();
				}
				break;
			
			case IDC_FILE_LOADMODEL:
				{
					if ( ! CommandLine()->FindParm( "-NoSteamDialog" ) )
					{
						g_MDLViewer->LoadModel_Steam();
					}
					else
					{
						char modelfile[ 512 ];
						if ( FacePoser_ShowOpenFileNameDialog( modelfile, sizeof( modelfile ), "models", "*.mdl" ) )
						{
							LoadModelFile( modelfile );
						}
					}
				}
				break;

			case IDC_FILE_REFRESH:
				{
					Refresh();
					break;
				}

			case IDC_FILE_SAVESOUNDSCRIPTCHANGES:
				{
					OnSaveSoundScriptChanges();
				}
				break;
			case IDC_FILE_REBUILDSCENESIMAGE:
				{
					OnRebuildScenesImage();
				}
				break;

			case IDC_FILE_LOADBACKGROUNDTEX:
			case IDC_FILE_LOADGROUNDTEX:
				{
					const char *ptr = mxGetOpenFileName (this, 0, "*.*");
					if (ptr)
					{
						if (0 /* g_pMatSysWindow->loadTexture (ptr, event->action - IDC_FILE_LOADBACKGROUNDTEX) */)
						{
							if (event->action == IDC_FILE_LOADBACKGROUNDTEX)
								g_pControlPanel->setShowBackground (true);
							else
								g_pControlPanel->setShowGround (true);
							
						}
						else
							mxMessageBox (this, "Error loading texture.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
					}
				}
				break;
				
			case IDC_FILE_UNLOADGROUNDTEX:
				{
					// g_pMatSysWindow->loadTexture (0, 1);
					g_pControlPanel->setShowGround (false);
				}
				break;
				
			case IDC_FILE_RECENTFILES1:
			case IDC_FILE_RECENTFILES2:
			case IDC_FILE_RECENTFILES3:
			case IDC_FILE_RECENTFILES4:
			case IDC_FILE_RECENTFILES5:
			case IDC_FILE_RECENTFILES6:
			case IDC_FILE_RECENTFILES7:
			case IDC_FILE_RECENTFILES8:
				{
					int i = event->action - IDC_FILE_RECENTFILES1;
					
					if ( recentFiles[ i ] && recentFiles[ i ][ 0 ] )
					{
						char ext[ 4 ];
						Q_ExtractFileExtension( recentFiles[ i ], ext, sizeof( ext ) );
						bool valid = false;
						if ( !Q_stricmp( ext, "mdl" ) )
						{
							// Check extension
							LoadModelFile( recentFiles[ i ] );
							valid = true;
						}
						else if ( !Q_stricmp( ext, "vcd" ) )
						{
							g_pChoreoView->LoadSceneFromFile( recentFiles[ i ] );
							valid = true;
						}

						if ( valid )
						{
							char tmp[256];			
							strcpy (tmp, recentFiles[0]);
							strcpy (recentFiles[0], recentFiles[i]);
							strcpy (recentFiles[i], tmp);
						
							initRecentFiles ();
						}
					}
					
					redraw ();
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
				{
					float *cols[3] = { g_viewerSettings.bgColor, g_viewerSettings.gColor, g_viewerSettings.lColor };
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
				g_pControlPanel->centerView ();
				break;
				
			case IDC_OPTIONS_CENTERONFACE:
				g_pControlPanel->CenterOnFace();
				break;
				
			case IDC_OPTIONS_CLEARMODELSOUNDS:
				{
					sound->StopAll();
					Con_ColorPrintf( RGB( 0, 100, 255 ), "Resetting model sound channels\n" );
				}
				break;

			case IDC_OPTIONS_MAKESCREENSHOT:
				{
					char *ptr = (char *) mxGetSaveFileName (this, "", "*.tga");
					if (ptr)
					{
						char fn[ 512 ];
						Q_strncpy( fn, ptr, sizeof( fn ) );
						Q_SetExtension( fn, ".tga", sizeof( fn ) );
						g_pMatSysWindow->TakeScreenShot( fn );
					}
				}
				break;
				
			case IDC_OPTIONS_DUMP:
				g_pControlPanel->dumpModelInfo ();
				break;
				
#ifdef WIN32
			case IDC_HELP_GOTOHOMEPAGE:
				ShellExecute (0, "open", "http://developer.valvesoftware.com/wiki/Category:Choreography", 0, 0, SW_SHOW);
				break;
#endif
				
			case IDC_HELP_ABOUT:
				mxMessageBox (this,
					"v1.0  Copyright © 1996-2007, Valve Corporation. All rights reserved.\r\nBuild Date: " __DATE__ "",
					"Valve Face Poser", 
					MX_MB_OK | MX_MB_INFORMATION);
				break;
				
			case IDC_EXPRESSIONS_REDOBITMAPS:
				{
					CExpClass *active = expressions->GetActiveClass();
					if ( active )
					{
						g_pProgressDialog->Start( "Rebuild Bitmaps", "", true );

						g_pMatSysWindow->EnableStickySnapshotMode( );
						for ( int i = 0; i < active->GetNumExpressions() ; i++ )
						{
							CExpression *exp = active->GetExpression( i );
							if ( !exp )
								continue;
							
							g_pProgressDialog->UpdateText( exp->name );
							g_pProgressDialog->Update( (float)i / (float)active->GetNumExpressions() );
							if ( g_pProgressDialog->IsCancelled() )
							{
								Msg( "Cancelled\n" );
								break;
							}

							exp->CreateNewBitmap( models->GetActiveModelIndex() );

							if ( ! ( i % 5 ) )
							{
								g_pExpressionTrayTool->redraw();
							}
						}
						g_pMatSysWindow->DisableStickySnapshotMode( );
						
						g_pProgressDialog->Finish();

						active->SelectExpression( 0 );
					}
				}
				break;
			case IDC_EXPRESSIONS_NEW:
				{
					char classfile[ 512 ];
					if ( FacePoser_ShowSaveFileNameDialog( classfile, sizeof( classfile ), "expressions", "*.txt" ) )
					{
	                    Q_DefaultExtension( classfile, ".txt", sizeof( classfile ) );
						expressions->CreateNewClass( classfile );
					}
				}
				break;
			case IDC_EXPRESSIONS_LOAD:
				{
					char classfile[ 512 ];
					if ( FacePoser_ShowOpenFileNameDialog( classfile, sizeof( classfile ), "expressions", "*.txt" ) )
					{
						expressions->LoadClass( classfile );
					}
				}
				break;
				
			case IDC_EXPRESSIONS_SAVE:
				{
					CExpClass *active = expressions->GetActiveClass();
					if ( active )
					{
						active->Save();
						active->Export();
					}
				}
				break;
			case IDC_EXPRESSIONS_EXPORT:
				{
					CExpClass *active = expressions->GetActiveClass();
					if ( active )
					{
						active->Export();
					}
				}
				break;
			case IDC_EXPRESSIONS_CLOSE:
				g_pControlPanel->Close();
				break;
			case IDC_EXPRESSIONS_CLOSEALL:
				g_pControlPanel->Closeall();
				break;
			case IDC_CHOREOSCENE_NEW:
				g_pChoreoView->New();
				break;
			case IDC_CHOREOSCENE_LOAD:
				g_pChoreoView->Load();
				break;
			case IDC_CHOREOSCENE_LOADNEXT:
				g_pChoreoView->LoadNext();
				break;
			case IDC_CHOREOSCENE_SAVE:
				g_pChoreoView->Save();
				break;
			case IDC_CHOREOSCENE_SAVEAS:
				g_pChoreoView->SaveAs();
				break;
			case IDC_CHOREOSCENE_CLOSE:
				g_pChoreoView->Close();
				break;
			case IDC_CHOREOSCENE_ADDACTOR:
				g_pChoreoView->NewActor();
				break;
			case IDC_WINDOW_TILE:
				{
					OnTile();
				}
				break;
			case IDC_WINDOW_TILE_HORIZ:
				{
					OnTileHorizontally();
				}
				break;
			case IDC_WINDOW_TILE_VERT:
				{
					OnTileVertically();
				}
				break;
			case IDC_WINDOW_CASCADE:
				{
					OnCascade();
				}
				break;
			case IDC_WINDOW_HIDEALL:
				{
					OnHideAll();
				}
				break;
			case IDC_WINDOW_SHOWALL:
				{
					OnShowAll();
				}
				break;
			default:
				{
					iret = 0;
					int tool_number = event->action - IDC_WINDOW_FIRSTTOOL;
					int max_tools = IDC_WINDOW_LASTTOOL - IDC_WINDOW_FIRSTTOOL;
					
					if ( tool_number >= 0 && 
						tool_number <= max_tools && 
						tool_number < IFacePoserToolWindow::GetToolCount() )
					{
						iret = 1;
						IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( tool_number );
						if ( tool )
						{
							mxWindow *toolw = tool->GetMxWindow();
							
							bool wasvisible = toolw->isVisible();
							toolw->setVisible( !wasvisible );
							
							g_MDLViewer->UpdateWindowMenu();

						}
					}

					int lang_number = event->action - IDC_OPTIONS_LANGUAGESTART;
					if ( lang_number >= 0 &&
						 lang_number < CC_NUM_LANGUAGES )
					{
						iret = 1;
						SetCloseCaptionLanguageId( lang_number );
					}
				}
				break;
			} //switch (event->action)
		} // mxEvent::Action
		break;
	case KeyDown:
		{
			//g_pMatSysWindow->handleEvent(event);
			// Send it to the active tool
			IFacePoserToolWindow *active = IFacePoserToolWindow::GetActiveTool();
			if ( active )
			{
				mxWindow *w = active->GetMxWindow();
				if ( w )
				{
					w->handleEvent( event );
				}
			}
			else
			{
				g_pMatSysWindow->handleEvent(event);
			}
			iret = 1;
		}
		break;
	case mxEvent::Activate:
		{
			if (event->action)
			{
				mx::setIdleWindow( g_pMatSysWindow );
				// Force reload of localization data
				SetCloseCaptionLanguageId( GetCloseCaptionLanguageId(), true );
			}
			else
			{
				mx::setIdleWindow( 0 );
			}
			iret = 1;
		}
		break;
	} // event->event
	
	return iret;
}

void MDLViewer::SaveWindowPositions( void )
{
	// Save the model viewer position
	SavePosition();

	int c = IFacePoserToolWindow::GetToolCount();
	for ( int i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *w = IFacePoserToolWindow::GetTool( i );
		w->SavePosition();
	}
}

void MDLViewer::LoadWindowPositions( void )
{
	// NOTE: Don't do this here, we do the mdlviewer position earlier in startup
	// LoadPosition();

	int w = this->w();
	int h = this->h();

	g_viewerSettings.width = w;
	g_viewerSettings.height = h;

	int c = IFacePoserToolWindow::GetToolCount();
	for ( int i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *w = IFacePoserToolWindow::GetTool( i );
		w->LoadPosition();
	}
}

void
MDLViewer::redraw ()
{
}

int MDLViewer::GetCurrentFrame( void )
{
	return m_nCurrentFrame;
}

void MDLViewer::Think( float dt )
{
	++m_nCurrentFrame;

	// Iterate across tools
	IFacePoserToolWindow::ToolThink( dt );

	sound->Update( dt );

	bool soundscriptsdirty = AreSoundScriptsDirty();
	if ( soundscriptsdirty != m_bOldSoundScriptsDirty )
	{
		// Update the menu item when this changes
		menuFile->setEnabled(IDC_FILE_SAVESOUNDSCRIPTCHANGES, soundscriptsdirty );
	}

	m_bOldSoundScriptsDirty = soundscriptsdirty;
}

static int CountVisibleTools( void )
{
	int i;
	int c = IFacePoserToolWindow::GetToolCount();
	int viscount = 0;

	for ( i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		mxWindow *w = tool->GetMxWindow();
		if ( !w->isVisible() )
			continue;

		viscount++;
	}

	return viscount;
}

void MDLViewer::OnCascade()
{
	int i;
	int c = IFacePoserToolWindow::GetToolCount();
	int viscount = CountVisibleTools();

	int x = 0, y = 0;

	int offset = 20;

	int wide = workspace->w2() - viscount * offset;
	int tall = ( workspace->h2() - viscount * offset ) / 2;

	for ( i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		mxWindow *w = tool->GetMxWindow();
		if ( !w->isVisible() )
			continue;

		w->setBounds( x, y, wide, tall );
		x += offset;
		y += offset;
	}
}

void MDLViewer::OnTile()
{
	int c = CountVisibleTools();

	int rows = (int)sqrt( ( float )c );
	rows = clamp( rows, 1, rows );

	int cols  = 1;
	while ( rows * cols < c )
	{
		cols++;
	}

	DoTile( rows, cols );
}

void MDLViewer::OnTileHorizontally()
{
	int c = CountVisibleTools();

	DoTile( c, 1 );
}

void MDLViewer::OnTileVertically()
{
	int c = CountVisibleTools();

	DoTile( 1, c );
}

void MDLViewer::OnHideAll()
{
	int c = IFacePoserToolWindow::GetToolCount();
	for ( int i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		mxWindow *w = tool->GetMxWindow();

		w->setVisible( false );
	}

	UpdateWindowMenu();
}

void MDLViewer::OnShowAll()
{
	int c = IFacePoserToolWindow::GetToolCount();
	for ( int i = 0; i < c; i++ )
	{
		IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( i );
		mxWindow *w = tool->GetMxWindow();

		w->setVisible( true );
	}

	UpdateWindowMenu();
}

void MDLViewer::DoTile( int x, int y )
{
	int c = IFacePoserToolWindow::GetToolCount();

	if ( x < 1 )
		x = 1;
	if ( y < 1 )
		y = 1;

	int wide = workspace->w2() / y;
	int tall = workspace->h2() / x;

	int obj = 0;

	for ( int row = 0 ; row < x ; row++ )
	{
		for  ( int col = 0; col < y; col++ )
		{
			bool found = false;
			while ( 1 )
			{
				if ( obj >= c )
					break;

				IFacePoserToolWindow *tool = IFacePoserToolWindow::GetTool( obj++ );
				mxWindow *w = tool->GetMxWindow();
				if ( w->isVisible() )
				{
					w->setBounds( col * wide, row * tall, wide, tall );

					found = true;
					break;
				}
			}

			if ( !found )
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Not used by faceposer
// Output : int
//-----------------------------------------------------------------------------
int MDLViewer::GetCurrentHitboxSet(void)
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool MDLViewer::PaintBackground( void )
{
	CChoreoWidgetDrawHelper drawHelper( this );

	RECT rc;
	drawHelper.GetClientRect( rc );

	drawHelper.DrawFilledRect( COLOR_CHOREO_BACKGROUND, rc );
	return false;
}

void MDLViewer::OnRebuildScenesImage()
{
	g_pProgressDialog->Start( "Rebuilding scenes.image", "", false );

	CUtlBuffer	targetBuffer;

	bool bLittleEndian = true;

	const char *pFilename = bLittleEndian ? "scenes/scenes.image" : "scenes/scenes.360.image";

	CP4AutoEditAddFile checkout( CFmtStr( "%s%s", gamedir, pFilename ) );

	bool bSuccess = g_pSceneImage->CreateSceneImageFile( targetBuffer, gamedir, bLittleEndian, false, this );
	if ( bSuccess )
	{
		scriptlib->WriteBufferToFile( pFilename, targetBuffer, WRITE_TO_DISK_ALWAYS );
	}

	g_pProgressDialog->Finish();
	m_bVCDSaved = false;
}

void MDLViewer::UpdateStatus( char const *pchSceneName, bool bQuiet, int nIndex, int nCount )
{
	g_pProgressDialog->UpdateText( pchSceneName );
	g_pProgressDialog->Update( (float)nIndex / (float)nCount );
}

void MDLViewer::OnVCDSaved()
{
	m_bVCDSaved = true;
}

SpewRetval_t HLFacePoserSpewFunc( SpewType_t spewType, char const *pMsg )
{
	g_bInError = true;

	switch (spewType)
	{
	case SPEW_ERROR:
		::MessageBox(NULL, pMsg, "FATAL ERROR", MB_OK);
		g_bInError = false;
		return SPEW_ABORT;

	case SPEW_LOG:
		g_bInError = false;
		return SPEW_CONTINUE;

	case SPEW_WARNING:
		Con_ErrorPrintf( pMsg );
		g_bInError = false;
		return SPEW_CONTINUE;

	default:
		Con_Printf(pMsg);
		g_bInError = false;
#ifdef _DEBUG
		return spewType == SPEW_ASSERT ? SPEW_DEBUGGER : SPEW_CONTINUE;
#else
		return SPEW_CONTINUE;
#endif
	}
}

void MDLViewer::OnSaveSoundScriptChanges()
{
	if ( !AreSoundScriptsDirty() )
	{
		return;
	}

// Save any changed sound script files
	int c = soundemitter->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		if ( !soundemitter->IsSoundScriptDirty( i ) )
			continue;

		char const *scriptname = soundemitter->GetSoundScriptName( i );
		if ( !scriptname )
			continue;

		if ( !filesystem->FileExists( scriptname ) )
		{
			continue;
		}

		if ( !filesystem->IsFileWritable( scriptname ) )
		{
			mxMessageBox( NULL, va( "Can't save changes to sound script '%s', file is READ-ONLY?", scriptname ), g_appTitle, MX_MB_OK );
			continue;
		}

		int retval = mxMessageBox( NULL, va( "Save changes to sound script '%s'?", scriptname ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval == 2 )
		{
			return;
		}

		if ( retval == 0 )
		{
			soundemitter->SaveChangesToSoundScript( i );
		}
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CHLFacePoserApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	// Sets up the search paths
	bool SetupSearchPaths();
};


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CHLFacePoserApp::Create()
{
	// Save some memory so engine/hammer isn't so painful
	CommandLine()->AppendParm( "-disallowhwmorph", NULL );

	SpewOutputFunc( HLFacePoserSpewFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	// Add the P4 module separately so that if it is absent (say in the SDK) then the other system will initialize properly
	AppModule_t p4Module = LoadModule( "p4lib.dll" );
	if ( p4Module != APP_MODULE_INVALID )
	{
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}

	g_Factory = GetFactory();

	IMaterialSystem* pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
	{
		Warning( "Material System interface could not be found!\n" );
		return false;
	}

	const char *pShaderDLL = CommandLine()->ParmValue("-shaderdll");
	if(!pShaderDLL)
	{
		pShaderDLL = "shaderapidx9.dll";
	}
	pMaterialSystem->SetShaderAPI( pShaderDLL );

	return true;
}


void CHLFacePoserApp::Destroy()
{
}


const char *GetGameDirectory()
{
	// TODO: get rid of this and ONLY use the filesystem, so hlfaceposer works nicely for
	// mods that get the base game resources from the Steam filesystem.
	return gamedir;
}

char const *GetGameDirectorySimple()
{
	return gamedirsimple;
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CHLFacePoserApp::SetupSearchPaths()
{
	// Add paths...
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	// Set gamedir.
	Q_MakeAbsolutePath( gamedir, sizeof( gamedir ), GetGameInfoPath() );

	Q_FileBase( gamedir, gamedirsimple, sizeof( gamedirsimple ) );

	Q_AppendSlash( gamedir, sizeof( gamedir ) );

	workspacefiles->Init( GetGameDirectorySimple() );

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CHLFacePoserApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	g_pFileSystem = filesystem = g_pFullFileSystem;
	g_pStudioDataCache = (IStudioDataCache*)FindSystem( STUDIO_DATA_CACHE_INTERFACE_VERSION ); 
	physcollision = (IPhysicsCollision *)FindSystem( VPHYSICS_COLLISION_INTERFACE_VERSION );
	physprop = (IPhysicsSurfaceProps *)FindSystem( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION );
	g_pLocalize = (vgui::ILocalize *)FindSystem(VGUI_LOCALIZE_INTERFACE_VERSION );
	soundemitter = (ISoundEmitterSystemBase*)FindSystem(SOUNDEMITTERSYSTEM_INTERFACE_VERSION);

	if ( !soundemitter || !g_pLocalize || !filesystem || !physprop || !physcollision || 
		!g_pMaterialSystem || !g_pStudioRender || !g_pMDLCache || !g_pDataCache )
	{
		Error("Unable to load required library interface!\n");
	}

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	filesystem->SetWarningFunc( Warning );

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	// Get the adapter from the command line....
	const char *pAdapterString;
	int nAdapter = 0;
	if (CommandLine()->CheckParm( "-adapter", &pAdapterString ))
	{
		nAdapter = atoi( pAdapterString );
	}

	int adapterFlags = MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	if ( CommandLine()->CheckParm( "-ref" ) )
	{
		adapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}

	g_pMaterialSystem->SetAdapter( nAdapter, adapterFlags );

	LoadFileSystemDialogModule();

	return true; 
}

void CHLFacePoserApp::PostShutdown()
{
	UnloadFileSystemDialogModule();

	g_pFileSystem = filesystem = NULL;
	g_pStudioDataCache = NULL;
	physcollision = NULL;
	physprop = NULL;

	BaseClass::PostShutdown();

	g_Factory = NULL;
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CHLFacePoserApp::Main()
{
	// Do Perforce Stuff
	g_p4factory->SetDummyMode( false );
	if ( CommandLine()->FindParm( "-nop4" ) || !p4 )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "FacePoser Auto Checkout" );

	soundemitter->ModInit();
	g_pMaterialSystem->ModInit();

	g_pDataCache->SetSize( 64 * 1024 * 1024 );

	// Always start with english
	g_pLocalize->AddFile( "resource/closecaption_english.txt", "GAME", true );

	sound->Init();

	IFacePoserToolWindow::EnableToolRedraw( false );

	g_MDLViewer = new MDLViewer ();
	g_MDLViewer->setMenuBar (g_MDLViewer->getMenuBar ());

	FaceposerVGui()->Init( (HWND)g_MDLViewer->getHandle() );

	// Force reload of close captioning data file!!!
	SetCloseCaptionLanguageId( g_viewerSettings.cclanguageid, true );

	g_pStudioModel->Init();

	int i;
	bool modelloaded = false;
	for ( i = 1; i < CommandLine()->ParmCount(); i++ )
	{
		if ( Q_stristr (CommandLine()->GetParm( i ), ".mdl") )
		{
			modelloaded = true;
			g_MDLViewer->LoadModelFile( CommandLine()->GetParm( i ) );
			break;
		}
	}

	models->LoadModelList();
	g_pPhonemeEditor->ValidateSpeechAPIIndex();

	if ( models->Count() == 0 )
	{
		g_pFlexPanel->initFlexes( );
	}

	// Load expressions from last time
	int files = workspacefiles->GetNumStoredFiles( IWorkspaceFiles::EXPRESSION );
	for ( i = 0; i < files; i++ )
	{
		expressions->LoadClass( workspacefiles->GetStoredFile( IWorkspaceFiles::EXPRESSION, i ) );
	}

	IFacePoserToolWindow::EnableToolRedraw( true );

	int nRetVal = mx::run ();

	if (g_pStudioModel)
	{
		g_pStudioModel->Shutdown();
		g_pStudioModel = NULL;
	}

	g_pMaterialSystem->ModShutdown();
	soundemitter->ModShutdown();
 	g_pMaterialSystem->ModShutdown();

	FaceposerVGui()->Shutdown();

	return nRetVal;
}

static bool CHLFacePoserApp_SuggestGameInfoDirFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	if ( pbBubbleDirectories )
		*pbBubbleDirectories = true;

	for ( int i = 1; i < CommandLine()->ParmCount(); i++ )
	{
		if ( Q_stristr( CommandLine()->GetParm( i ), ".mdl" ) )
		{
			Q_MakeAbsolutePath( pchPathBuffer, nBufferLength, CommandLine()->GetParm( i ) );
			return true;
		}
	}

	return false;
}

int main (int argc, char *argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );
	CoInitialize(NULL);

	// make sure, we start in the right directory
	char szName[256];
	strcpy (szName, mx::getApplicationPath() );
	mx::init (argc, argv);

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// Set game info directory suggestion callback
	SetSuggestGameInfoDirFn( CHLFacePoserApp_SuggestGameInfoDirFn );

 	CHLFacePoserApp hlFacePoserApp;
	CSteamApplication steamApplication( &hlFacePoserApp );
	int nRetVal = steamApplication.Run();

	CoUninitialize();

	return nRetVal;
}

