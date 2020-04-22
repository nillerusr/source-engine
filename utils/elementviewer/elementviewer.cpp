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
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "tier0/dbg.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/AnimationController.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "datamodel/dmelement.h"
#include "filesystem_init.h"
#include "vstdlib/iprocessutils.h"
#include "dmserializers/idmserializers.h"
#include "dme_controls/dmecontrols.h"
#include "p4lib/ip4.h"
#include "tier3/tier3.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
vgui::Panel *CreateElementViewerPanel();


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
// The application object
//-----------------------------------------------------------------------------
class CElementViewerApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy();

private:
	virtual const char *GetAppName() { return "ElementViewer"; }
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CElementViewerApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CElementViewerApp::Create()
{
	SpewOutputFunc( SpewToODS );

	// This is a little cheezy but fast...
	CommandLine()->AppendParm( "-resizing", NULL );

	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmElementFramework, VDMELEMENTFRAMEWORK_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );

	return AddSystems( appSystems );
}

void CElementViewerApp::Destroy()
{
	BaseClass::Destroy();
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CElementViewerApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pVGui || !g_pVGuiSurface || !g_pDataModel || !g_pMatSystemSurface || !p4 )
	{
		Error( "Element viewer is missing required interfaces!\n" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_RecursivePrintTree( int depth, int start, int end, vgui::Panel *current, int& totaldrawn )
{
	// No more room
	if ( totaldrawn >= 128 )
		return;

	if ( !current )
		return;

	int count = current->GetChildCount();
	for ( int i = 0; i < count ; i++ )
	{
		vgui::Panel *panel = current->GetChild( i );
//		Msg( "%i:  %s : %p, %s %s\n", 
//			i + 1, 
//			panel->GetName(), 
//			panel, 
//			panel->IsVisible() ? "visible" : "hidden",
//			panel->IsPopup() ? "popup" : "" );

		int width = panel->GetWide();
		int height = panel->GetTall();
		int x, y;
		
		panel->GetPos( x, y );

		if ( depth >= start && depth <= end )
		{
			totaldrawn++;
			Msg( 
			// Con_NPrintf( totaldrawn++, 
				"%s (%i.%i): %p, %s %s x(%i) y(%i) w(%i) h(%i)\n", 
				panel->GetName(), 
				depth + 1,
				i + 1, 
				panel, 
				panel->IsVisible() ? "visible" : "hidden",
				panel->IsPopup() ? "popup" : "",
				x, y,
				width, height );
		}

		VGui_RecursivePrintTree( depth + 1, start, end, panel, totaldrawn );	
	}
}

#define VGUI_DRAWPOPUPS 1
#define VGUI_DRAWTREE 0
#define VGUI_DRAWPANEL ""
#define VGUI_TREESTART 0
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_DrawPopups( void )
{
	if ( !VGUI_DRAWPOPUPS )
		return;
	
	int c = vgui::surface()->GetPopupCount();
	for ( int i = 0; i < c; i++ )
	{
		vgui::VPANEL popup = vgui::surface()->GetPopup( i );
		if ( !popup )
			continue;

		const char *p = vgui::ipanel()->GetName( popup );
		bool visible = vgui::ipanel()->IsVisible( popup );

		int width, height;
		int x, y;
		
		vgui::ipanel()->GetSize( popup, width, height );
		vgui::ipanel()->GetPos( popup, x, y );

		//Con_NPrintf( i, 
		Msg( 
			"%i:  %s : %x, %s pos(%i,%i) w(%i) h(%i)\n",
			i,
			p,
			popup,
			visible ? "visible" : "hidden",
			x, y,
			width, height );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_DrawHierarchy( void )
{
	if ( VGUI_DRAWTREE <= 0 && Q_strlen( VGUI_DRAWPANEL ) <= 0 )
		return;

	Msg( "\n" );

	int startlevel = 0;
	int endlevel = 1000;

	bool wholetree = VGUI_DRAWTREE > 0 ? true : false;
	
	if ( wholetree )
	{
		startlevel = VGUI_TREESTART;
		endlevel = VGUI_DRAWTREE;
	}

	// Can't start after end
	startlevel = min( endlevel, startlevel );

	int drawn = 0;

	vgui::VPANEL root = vgui::surface()->GetEmbeddedPanel();
	if ( !root )
		return;

	vgui::Panel *p = vgui::ipanel()->GetPanel( root, "ElementViewer" );

	if ( !wholetree )
	{
		// Find named panel
		char const *name = VGUI_DRAWPANEL;
		p = p->FindChildByName( name, true );
	}

	if ( !p )
		return;

	VGui_RecursivePrintTree( 0, startlevel, endlevel, p, drawn );
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CElementViewerApp::Main()
{
	g_pMaterialSystem->ModInit();
	if (!SetVideoMode())
		return 0;

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile("resource/BoxRocket.res", "ElementViewer" ))
	{
		Assert( 0 );
	}

	// load the boxrocket localization file
	g_pVGuiLocalize->AddFile( "resource/boxrocket_%language%.txt" );

	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );
	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/dmecontrols_%language%.txt");

	// start vgui
	g_pVGui->Start();

	// add our main window
	vgui::Panel *mainPanel = CreateElementViewerPanel();

	// run app frame loop
	vgui::VPANEL root = vgui::surface()->GetEmbeddedPanel();
	vgui::surface()->Invalidate( root );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int nLastTime = Plat_MSTime();
	while (g_pVGui->IsRunning())
	{
		// Give other applications a chance to run
		Sleep( 1 );
		int nTime = Plat_MSTime();
		if ( ( nTime - nLastTime ) < 16 )
			continue;
		nLastTime = nTime;

		g_pDmElementFramework->BeginEdit();

		AppPumpMessages();

		pRenderContext->Viewport( 0, 0, GetWindowWidth(), GetWindowHeight() );
	
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

	g_pMaterialSystem->ModShutdown();

	return 1;
}



