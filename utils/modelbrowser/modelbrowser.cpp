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
#include <windows.h>
#include "vstdlib/cvar.h"
#include "appframework/vguimatsysapp.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "tier0/dbg.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/AnimationController.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "filesystem_init.h"
#include "vstdlib/iprocessutils.h"
#include "matsys_controls/matsyscontrols.h"
#include "matsys_controls/mdlpicker.h"
#include "IStudioRender.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vgui_controls/frame.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier3/tier3.h"
#include "vgui_controls/consoledialog.h"
#include "icvar.h"
#include "vgui/keycode.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "ivtex.h"
#include "modelbrowsermaterialproxies.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
const MaterialSystem_Config_t *g_pMaterialSystemConfig;

static CModelBrowserMaterialProxyFactory g_materialProxyFactory;

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
// FIXME: This should be moved into studiorender
//-----------------------------------------------------------------------------
static ConVar	r_showenvcubemap( "r_showenvcubemap", "0", FCVAR_CHEAT );
static ConVar	r_eyegloss		( "r_eyegloss", "1", FCVAR_ARCHIVE ); // wet eyes
static ConVar	r_eyemove		( "r_eyemove", "1", FCVAR_ARCHIVE ); // look around
static ConVar	r_eyeshift_x	( "r_eyeshift_x", "0", FCVAR_ARCHIVE ); // eye X position
static ConVar	r_eyeshift_y	( "r_eyeshift_y", "0", FCVAR_ARCHIVE ); // eye Y position
static ConVar	r_eyeshift_z	( "r_eyeshift_z", "0", FCVAR_ARCHIVE ); // eye Z position
static ConVar	r_eyesize		( "r_eyesize", "0", FCVAR_ARCHIVE ); // adjustment to iris textures
static ConVar	mat_softwareskin( "mat_softwareskin", "0", FCVAR_CHEAT );
static ConVar	r_nohw			( "r_nohw", "0", FCVAR_CHEAT );
static ConVar	r_nosw			( "r_nosw", "0", FCVAR_CHEAT );
static ConVar	r_teeth			( "r_teeth", "1" );
static ConVar	r_drawentities	( "r_drawentities", "1", FCVAR_CHEAT );
static ConVar	r_flex			( "r_flex", "1" );
static ConVar	r_eyes			( "r_eyes", "1" );
static ConVar	r_skin			( "r_skin","0", FCVAR_CHEAT );
static ConVar	r_maxmodeldecal ( "r_maxmodeldecal", "50" );
static ConVar	r_modelwireframedecal ( "r_modelwireframedecal", "0", FCVAR_CHEAT );
static ConVar	mat_wireframe	( "mat_wireframe", "0", FCVAR_CHEAT );
static ConVar	mat_normals		( "mat_normals", "0", FCVAR_CHEAT );
static ConVar	r_eyeglintlodpixels ( "r_eyeglintlodpixels", "0", FCVAR_CHEAT );
static ConVar	r_rootlod		( "r_rootlod", "0" );

static StudioRenderConfig_t s_StudioRenderConfig;

void UpdateStudioRenderConfig( void )
{
	memset( &s_StudioRenderConfig, 0, sizeof(s_StudioRenderConfig) );

	s_StudioRenderConfig.bEyeMove = !!r_eyemove.GetInt();
	s_StudioRenderConfig.fEyeShiftX = r_eyeshift_x.GetFloat();
	s_StudioRenderConfig.fEyeShiftY = r_eyeshift_y.GetFloat();
	s_StudioRenderConfig.fEyeShiftZ = r_eyeshift_z.GetFloat();
	s_StudioRenderConfig.fEyeSize = r_eyesize.GetFloat();	
	if( mat_softwareskin.GetInt() || mat_wireframe.GetInt() )
	{
		s_StudioRenderConfig.bSoftwareSkin = true;
	}
	else
	{
		s_StudioRenderConfig.bSoftwareSkin = false;
	}
	s_StudioRenderConfig.bNoHardware = !!r_nohw.GetInt();
	s_StudioRenderConfig.bNoSoftware = !!r_nosw.GetInt();
	s_StudioRenderConfig.bTeeth = !!r_teeth.GetInt();
	s_StudioRenderConfig.drawEntities = r_drawentities.GetInt();
	s_StudioRenderConfig.bFlex = !!r_flex.GetInt();
	s_StudioRenderConfig.bEyes = !!r_eyes.GetInt();
	s_StudioRenderConfig.bWireframe = !!mat_wireframe.GetInt();
	s_StudioRenderConfig.bDrawNormals = mat_normals.GetBool();
	s_StudioRenderConfig.skin = r_skin.GetInt();
	s_StudioRenderConfig.maxDecalsPerModel = r_maxmodeldecal.GetInt();
	s_StudioRenderConfig.bWireframeDecals = r_modelwireframedecal.GetInt() != 0;
	
	s_StudioRenderConfig.fullbright = g_pMaterialSystemConfig->nFullbright;
	s_StudioRenderConfig.bSoftwareLighting = g_pMaterialSystemConfig->bSoftwareLighting;

	s_StudioRenderConfig.bShowEnvCubemapOnly = r_showenvcubemap.GetInt() ? true : false;
	s_StudioRenderConfig.fEyeGlintPixelWidthLODThreshold = r_eyeglintlodpixels.GetFloat();

	g_pStudioRender->UpdateConfig( s_StudioRenderConfig );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CModelBrowserApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual bool AppUsesReadPixels() { return true; }

private:
	virtual const char *GetAppName() { return "ModelBrowser"; }

	// Sets a default env_cubemap for rendering materials w/ specularity
	void InitDefaultEnvCubemap( );
	void ShutdownDefaultEnvCubemap( );
	CTextureReference m_DefaultEnvCubemap;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CModelBrowserApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CModelBrowserApp::Create()
{
	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "vtex_dll",				IVTEX_VERSION_STRING },

		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) )
		return false;

	if ( !CommandLine()->CheckParm( "-nop4" ))
	{
		AppModule_t hModule = LoadModule( "p4lib" );
		AddSystem( hModule, P4_INTERFACE_VERSION );
	}
	else
	{
		g_p4factory->SetDummyMode( true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CModelBrowserApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	// initialize interfaces
	CreateInterfaceFn appFactory = GetFactory(); 
	if (!vgui::VGui_InitMatSysInterfacesList( "ModelBrowser", &appFactory, 1 ))
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pVGui || !g_pVGuiSurface || !g_pMatSystemSurface || !g_pVTex )
	{
		Warning( "Model browser is missing a required interface!\n" );
		return false;
	}

	g_p4factory->SetOpenFileChangeList( "ModelBrowser Auto Checkout" );

	return true;
}


//-----------------------------------------------------------------------------
// Sets a default env_cubemap for rendering materials w/ specularity
//-----------------------------------------------------------------------------
void CModelBrowserApp::InitDefaultEnvCubemap( )
{
	// Deal with the default cubemap
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ITexture *pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap", NULL, true );
	m_DefaultEnvCubemap.Init( pCubemapTexture );
	pRenderContext->BindLocalCubemap( pCubemapTexture );
}

void CModelBrowserApp::ShutdownDefaultEnvCubemap( )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->BindLocalCubemap( NULL );
	m_DefaultEnvCubemap.Shutdown( );
}


//-----------------------------------------------------------------------------
// Sequence picker frame
//-----------------------------------------------------------------------------
class CMDLBrowserFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CMDLBrowserFrame, vgui::Frame );
public:
	CMDLBrowserFrame() : BaseClass( NULL, "MDLPickerFrame" ) 
	{
		m_pMDLPicker = new CMDLPicker( this );
		SetTitle( "Model Browser", true );
		SetSizeable( false );
		SetCloseButtonVisible( false );
		SetMoveable( false );
		Activate();
		m_pMDLPicker->Activate();
		m_bPositioned = false;

		SetKeyBoardInputEnabled( true );

		m_pConsole = new vgui::CConsoleDialog( this, "ConsoleDialog", false );
		m_pConsole->AddActionSignalTarget( this );
	}

	virtual ~CMDLBrowserFrame() 
	{
		delete m_pMDLPicker;
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int x, y, w, h;
		GetClientArea( x, y, w, h );
		m_pMDLPicker->SetBounds( x, y, w, h );

		if ( !m_bPositioned )
		{
			m_pConsole->SetSize( w/2, h/2 );
			m_pConsole->MoveToCenterOfScreen();
			m_bPositioned = true;
		}
	}

	virtual void OnKeyCodePressed( vgui::KeyCode code )
	{
		BaseClass::OnKeyCodePressed( code );
		if ( code == KEY_BACKQUOTE )
		{
			if ( !m_pConsole->IsVisible() )
			{
				m_pConsole->Activate();
			}
			else
			{
				m_pConsole->Hide();
			}
		}
	}

	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command )
	{
		CCommand args;
		args.Tokenize( command );

		ConCommandBase *pCommandBase = g_pCVar->FindCommandBase( args[0] );
		if ( !pCommandBase )
		{
			ConWarning( "Unknown command or convar '%s'!\n", args[0] );
			return;
		}

		if ( pCommandBase->IsCommand() )
		{
			ConCommand *pCommand = static_cast<ConCommand*>( pCommandBase );
			pCommand->Dispatch( args );
			return;
		}

		ConVar *pConVar = static_cast< ConVar* >( pCommandBase );
		if ( args.ArgC() == 1)
		{
			if ( pConVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
			{
				ConMsg( "%s = %f\n", args[0], pConVar->GetFloat() );
			}
			else
			{
				ConMsg( "%s = %s\n", args[0], pConVar->GetString() );
			}
			return;
		}

		if ( pConVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
		{
			pConVar->SetValue( (float)atof( args[1] ) );
		}
		else
		{
			pConVar->SetValue( args.ArgS() );
		}
	}

private:
	CMDLPicker *m_pMDLPicker;
	vgui::CConsoleDialog *m_pConsole;
	bool m_bPositioned;
};


//-----------------------------------------------------------------------------
// Creates the picker frame
//-----------------------------------------------------------------------------
vgui::PHandle CreatePickerFrame()
{
	vgui::Frame *pMainPanel = new CMDLBrowserFrame( ); 
	pMainPanel->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );

	vgui::PHandle hMainPanel;
	hMainPanel = pMainPanel;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int x, y, w, h;
	pRenderContext->GetViewport( x, y, w, h );
	pMainPanel->SetBounds( x+2, y+2, w-4, h-4 );

	return hMainPanel;
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CModelBrowserApp::Main()
{
	SpewOutputFunc( ModelBrowserSpewFunc );
	SpewActivate( "console", 1 );

	g_pMaterialSystem->ModInit();
	g_pMaterialSystem->SetMaterialProxyFactory( &g_materialProxyFactory );
	if (!SetVideoMode())
		return 0;

	g_pDataCache->SetSize( 64 * 1024 * 1024 );

	InitDefaultEnvCubemap();

	g_pMaterialSystemConfig = &g_pMaterialSystem->GetCurrentConfigForVideoCard();

	// configuration settings
	vgui::system()->SetUserConfigFile( "modelbrowser.vdf", "EXECUTABLE_PATH");

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile( "resource/BoxRocket.res", "ModelBrowser" ))
	{
		Assert( 0 );
	}

	// load the boxrocket localization file
	g_pVGuiLocalize->AddFile( "resource/boxrocket_%language%.txt" );

	// start vgui
	g_pVGui->Start();

	// add our main window
	vgui::PHandle hMainPanel;

	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );
	g_pFullFileSystem->AddSearchPath("platform", "PLATFORM");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");

	// run app frame loop
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	vgui::VPANEL root = g_pVGuiSurface->GetEmbeddedPanel();
	g_pVGuiSurface->Invalidate( root );
	while (g_pVGui->IsRunning())
	{
		UpdateStudioRenderConfig();

		AppPumpMessages();
	
		vgui::GetAnimationController()->UpdateAnimations( Sys_FloatTime() );

		g_pMaterialSystem->BeginFrame( 0 );
		g_pStudioRender->BeginFrame();

		pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
		pRenderContext->ClearBuffers( true, true );

		g_pVGui->RunFrame();

		g_pVGuiSurface->PaintTraverseEx( root, true );

		g_pStudioRender->EndFrame();
		g_pMaterialSystem->EndFrame( );

		g_pMaterialSystem->SwapBuffers();

		// Disallow closing the dialog
		if ( !hMainPanel.Get() )
		{
			hMainPanel = CreatePickerFrame();
		}
	}

	if ( hMainPanel.Get() )
	{
		delete hMainPanel.Get();
	}

 	ShutdownDefaultEnvCubemap();

	g_pMaterialSystem->ModShutdown();

	SpewOutputFunc( NULL );
	return 1;
}



