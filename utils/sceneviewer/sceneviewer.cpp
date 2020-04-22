//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#include "vstdlib/cvar.h"
#include "appframework/AppFramework.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "tier0/dbg.h"
#include "vgui_controls/Frame.h"
#include "appframework/vguimatsysapp.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/iinput.h"
#include "vgui/isystem.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "datamodel/dmelement.h"
#include "movieobjects/idmemakefileutils.h"
#include "dme_controls/dmecontrols.h"
#include "IStudioRender.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "datamodel/idatamodel.h"
#include "vphysics_interface.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "filesystem_init.h"
#include "dmserializers/idmserializers.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "p4lib/ip4.h"
#include "vstdlib/iprocessutils.h"
#include "tier3/tier3.h"
#include "vgui_controls/perforcefileexplorer.h"
#include "dme_controls/assetbuilder.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

// temporary HACK
class _Window_t { int unused; };
_Window_t *g_pWindow = NULL;


const MaterialSystem_Config_t *g_pMaterialSystemConfig;
vgui::Panel *CreateSceneViewerPanel();

//-----------------------------------------------------------------------------
// Spew func
//-----------------------------------------------------------------------------
SpewRetval_t ModelBrowserSpewFunc( SpewType_t spewType, const tchar *pMsg )
{
	OutputDebugString( pMsg );
	switch( spewType )
	{
	case SPEW_ASSERT:
		g_pCVar->ConsoleColorPrintf( Color( 255, 192, 0, 255 ), pMsg );
#ifdef _DEBUG
		return SPEW_DEBUGGER;
#else
		return SPEW_CONTINUE;
#endif

	case SPEW_ERROR:
		g_pCVar->ConsoleColorPrintf( Color( 255, 0, 0, 255 ), pMsg );
		return SPEW_ABORT;

	case SPEW_WARNING:
		g_pCVar->ConsoleColorPrintf( Color( 192, 192, 0, 255 ), pMsg );
		break;

	case SPEW_MESSAGE:
		{
			Color c = *GetSpewOutputColor();
			if ( !Q_stricmp( GetSpewOutputGroup(), "developer" ) )
				g_pCVar->ConsoleDPrintf( pMsg );
			else
				g_pCVar->ConsoleColorPrintf( c, pMsg );
		}
		break;
	}

	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// redirect spew to debug output window
//-----------------------------------------------------------------------------
SpewRetval_t SpewToODS( SpewType_t spewType, char const *pMsg )
{
	OutputDebugString( pMsg );

	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_WARNING:
	case SPEW_LOG:
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
	case SPEW_ERROR:
	default:
		return SPEW_DEBUGGER;
	}
}


//-----------------------------------------------------------------------------
// redirect spew to stdout
//-----------------------------------------------------------------------------
SpewRetval_t SpewStdout( SpewType_t spewType, char const *pMsg )
{
	_tprintf( "%s", pMsg );
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CSceneViewerApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();
	virtual const char *GetAppName() { return "SceneViewer"; }
	virtual bool AppUsesReadPixels() { return true; }

private:
	// Sets a default env_cubemap for rendering materials w/ specularity
	void InitDefaultEnvCubemap( );
	void ShutdownDefaultEnvCubemap( );
	CTextureReference m_DefaultEnvCubemap;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool CHLSceneViewerApp_SuggestGameInfoDirFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	const char *pFilename = NULL;
	const int nParmCount = CommandLine()->ParmCount();
	char pchTmpBuf[ MAX_PATH ];
	for ( int nPi = 0; nPi < nParmCount; ++nPi )
	{
		pFilename = CommandLine()->GetParm( nParmCount - 1 );
		Q_MakeAbsolutePath( pchTmpBuf, sizeof( pchTmpBuf ), pFilename );
		if ( _access( pchTmpBuf, 04 ) == 0 )
		{
			Q_strncpy( pchPathBuffer, pchTmpBuf, nBufferLength );

			if ( pbBubbleDirectories )
				*pbBubbleDirectories = true;

			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int __stdcall WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	static CSceneViewerApp sceneViewerApp;				\
	static CSteamApplication steamApp( &sceneViewerApp );	\
	return AppMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow, &steamApp );
}


// DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CSceneViewerApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CSceneViewerApp::Create()
{
	if ( CommandLine()->FindParm( "-help" ) )
	{
		const bool newConsole( SetupWin32ConsoleIO() );

		Msg( "\n\n Sceneviewer - Loads and views Valve DMX Assets\n\n" );
		Msg( " Synopsis: sceneviewer [ vgui opts ] [ -help ] [ filename.[dmx|obj] ]\n" );
		Msg(
			"\n"
			" Where:\n"
			"\n"
			" Sceneviewer Options:\n"
			"\n"
			" -help . . . . . . Prints this information\n"
			" -nozoom . . . . . Stop sceneviewer zooming model viewer to occupy all client\n"
			"                   space when a dmx is specified on the command line\n"
			" -showasset  . . . Stop sceneviewer from hiding the asset builder when a dmx\n"
			"                   file is specified on the command line.\n"
			"  filename.dmx . . The name of a dmx file to load on start\n"
			"\n"
			" VGUI Options:\n"
			"\n"
			" -vproject <$> . . Override VPROJECT environment variable\n"
			" -game <$> . . . . Override VPROJECT environment variable\n"
			" -remote <$> . . . Add the remote share name\n"
			" -host <$> . . . . Set the host name\n"
			" -norfs  . . . . . Do not use remote filesystem\n"
			" -fullscreen . . . Run application fullscreen rather than in a window\n"
			" -width <#>  . . . Set the window width when running windowed\n"
			" -height <#> . . . Set the window height when running windowed\n"
			" -adapter <$>  . . Set the adapter??\n"
			" -ref  . . . . . . Set MATERIAL_INIT_REFERENCE_RASTERIZER on adapter??\n"
			" -resizing . . . . Allow the window to be resized\n"
			" -mat_vsync  . . . Wait for VSYNC\n"
			" -mat_antialias  . Turn on Anti-Aliasing\n"
			" -mat_aaquality  . Antialiasing quality (set to zero unless you know what you're doing)\n"
			"\n"
			);

		if ( newConsole )
		{
			Msg( "\n\nPress Any Key Continue..." );

			char tmpBuf[ 2 ];
			DWORD cRead;
			ReadConsole( GetStdHandle( STD_INPUT_HANDLE ), tmpBuf, 1, &cRead, NULL );
		}

		return false;
	}

	// FIXME: Enable vs30 shaders while NVidia driver bug exists
	CommandLine()->AppendParm( "-box", NULL );

	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) )
	{
		return false;
	}

	// Add the P4 module separately so that if it is absent (say in the SDK) then the other system will initialize properly
	AppModule_t p4Module = LoadModule( "p4lib.dll" );

	if ( APP_MODULE_INVALID != p4Module )
	{
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}

	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmElementFramework, VDMELEMENTFRAMEWORK_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );
	AddSystem( GetDefaultDmeMakefileUtils(), DMEMAKEFILE_UTILS_INTERFACE_VERSION );

	return true; 
}

void CSceneViewerApp::Destroy()
{
	BaseClass::Destroy();
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CSceneViewerApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	SpewOutputFunc( ModelBrowserSpewFunc );
	SpewActivate( "console", 1 );

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pDataModel || !g_pDmElementFramework || !g_pStudioRender || !g_pDataCache || !g_pMDLCache || !g_pVGuiSurface || !g_pVGui )
	{
		Warning( "CSceneViewerApp::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	// initialize interfaces
	CreateInterfaceFn appFactory = GetFactory(); 
	return vgui::VGui_InitDmeInterfacesList( "SceneViewer", &appFactory, 1 );
}

void CSceneViewerApp::PostShutdown()
{
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Sets a default env_cubemap for rendering materials w/ specularity
//-----------------------------------------------------------------------------
void CSceneViewerApp::InitDefaultEnvCubemap( )
{
	// Deal with the default cubemap
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ITexture *pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap", NULL, true );
	m_DefaultEnvCubemap.Init( pCubemapTexture );
	pRenderContext->BindLocalCubemap( pCubemapTexture );
}

void CSceneViewerApp::ShutdownDefaultEnvCubemap( )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->BindLocalCubemap( NULL );
	m_DefaultEnvCubemap.Shutdown( );
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CSceneViewerApp::Main()
{
	g_pMaterialSystem->ModInit();
	if (!SetVideoMode())
		return 0;

	g_pDataCache->SetSize( 64 * 1024 * 1024 );

	InitDefaultEnvCubemap();

	g_pMaterialSystemConfig = &g_pMaterialSystem->GetCurrentConfigForVideoCard();

	// configuration settings
	vgui::system()->SetUserConfigFile("sceneviewer.vdf", "EXECUTABLE_PATH");

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile("resource/BoxRocket.res", "SceneViewer" ))
	{
		Assert( 0 );
	}

	g_pVGuiLocalize->AddFile( "resource/boxrocket_%language%.txt" );

	// start vgui
	g_pVGui->Start();

	//vgui::input()->SetAppModalSurface( mainPanel->GetVPanel() );

	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );
	g_pFullFileSystem->AddSearchPath("platform", "PLATFORM");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/dmecontrols_%language%.txt");

	// add our main window
	vgui::Panel *mainPanel = CreateSceneViewerPanel();

	// run app frame loop
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	vgui::VPANEL root = vgui::surface()->GetEmbeddedPanel();
	vgui::surface()->Invalidate( root );

	// See if there was a DMX file passed on the command line

	for ( int i( CommandLine()->ParmCount() - 1 ); i > 0; --i )
	{
		const char *const arg( CommandLine()->GetParm( i ) );

		if ( arg && *arg != '\0' && _access( arg, 04 ) == 0 )
		{
			if ( !CommandLine()->FindParm( "-nozoom" ) )
			{
				KeyValues *oz( new KeyValues( "PinAndZoomIt" ) );
				mainPanel->PostMessage( mainPanel, oz );
			}

			KeyValues *ofs( new KeyValues( "LoadFile" ) );
			ofs->SetString( "fullpath", arg );
			mainPanel->PostMessage( mainPanel, ofs );

			if ( CommandLine()->FindParm( "-showasset" ) )
			{
				KeyValues *msg( new KeyValues( "ShowAssetBuilder" ) );
				mainPanel->PostMessage( mainPanel, msg );
			}

			if ( CommandLine()->FindParm( "-showcomboeditor" ) )
			{
				KeyValues *msg( new KeyValues( "ShowComboBuilder" ) );
				mainPanel->PostMessage( mainPanel, msg );
			}

			break;
		}
	}

	int nLastTime = Plat_MSTime();
	while (g_pVGui->IsRunning())
	{
		// Give other applications a chance to run
		Sleep( 1 );
		int nTime = Plat_MSTime();
		if ( ( nTime - nLastTime ) < 16 )
			continue;
		nLastTime = nTime;

		AppPumpMessages();
	
		vgui::GetAnimationController()->UpdateAnimations( Sys_FloatTime() );

		g_pMaterialSystem->BeginFrame( 0 );
		pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
		pRenderContext->ClearBuffers( true, true );

		g_pVGui->RunFrame();

		g_pVGuiSurface->PaintTraverseEx( root, true );

		g_pMaterialSystem->EndFrame();
		g_pMaterialSystem->SwapBuffers();
	}

	delete mainPanel;

	ShutdownDefaultEnvCubemap();
 	g_pMaterialSystem->ModShutdown();

	// HACK -  this is a bit of a hack, since in theory, there could be multiple of these panels,
	// or there could be elements allocated outside of these panels, but since the filenames are hardcoded,
	// I don't feel too bad about unloading *all* files to make sure they're caught
	int nFiles = g_pDataModel->NumFileIds();
	for ( int i = 0; i < nFiles; ++i )
	{
		DmFileId_t fileid = g_pDataModel->GetFileId( i );
		g_pDataModel->UnloadFile( fileid );
	}

	return 1;
}