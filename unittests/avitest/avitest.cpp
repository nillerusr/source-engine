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
#include "appframework/vguimatsysapp.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/controls.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CAVITestApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual const char *GetAppName() { return "AVITest"; }
	virtual bool AppUsesReadPixels() { return true; }

private:
	// Draws a quad
	void DrawStuff( AVIMaterial_t hMaterial );
	IMaterial *m_pMaterial;
	float m_flStartTime;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CAVITestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CAVITestApp::Create()
{
	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "valve_avi.dll",				AVI_INTERFACE_VERSION },
		{ "valve_avi.dll",				BIK_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CAVITestApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pVGui || !g_pVGuiSurface || !g_pAVI || !g_pBIK )
		return false;

	g_pAVI->SetMainWindow( GetAppWindow() );
	return true;
}

void CAVITestApp::PostShutdown( )
{
	g_pAVI->SetMainWindow( NULL );
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Draws a quad
//-----------------------------------------------------------------------------
void CAVITestApp::DrawStuff( AVIMaterial_t hMaterial )
{
	int iViewableWidth = GetWindowWidth();
	int iViewableHeight = GetWindowHeight();

	g_pAVI->SetTime( hMaterial, Sys_FloatTime() - m_flStartTime );

	float flMaxU, flMaxV;
	g_pAVI->GetTexCoordRange( hMaterial, &flMaxU, &flMaxV );
	IMaterial *pMaterial = g_pAVI->GetMaterial( hMaterial );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, 0, iViewableWidth, iViewableHeight, 0, 1 );

	pRenderContext->Bind( pMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	// Draw a polygon the size of the panel
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.Position3f( -0.5, iViewableHeight + 0.5, 0 );
	meshBuilder.TexCoord2f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.Position3f( -0.5, 0.5, 0 );
	meshBuilder.TexCoord2f( 0, 0, flMaxV );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.Position3f( iViewableWidth - 0.5, 0.5, 0 );
	meshBuilder.TexCoord2f( 0, flMaxU, flMaxV );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.Position3f( iViewableWidth - 0.5, iViewableHeight + 0.5, 0 );
	meshBuilder.TexCoord2f( 0, flMaxU, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

	
//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CAVITestApp::Main()
{
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
	g_pFullFileSystem->AddSearchPath("platform", "PLATFORM");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");

	// start vgui
	g_pVGui->Start();

	// add our main window
//	vgui::Panel *mainPanel = CreateElementViewerPanel();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	AVIParams_t params;
	Q_strcpy( params.m_pFileName, "c:\\temp\\out.avi" );
	Q_strcpy( params.m_pPathID, "MOD" );
	pRenderContext->GetWindowSize( params.m_nWidth, params.m_nHeight );
	params.m_nFrameRate = 24;
	params.m_nFrameScale = 1;
	params.m_nNumChannels = 0;

	AVIHandle_t h = g_pAVI->StartAVI( params );
	AVIMaterial_t hAVIMaterial = g_pAVI->CreateAVIMaterial( "avitest", "c:\\temp\\test.avi", "MOD" );

	// run app frame loop
	vgui::VPANEL root = g_pVGuiSurface->GetEmbeddedPanel();
	g_pVGuiSurface->Invalidate( root );

	int imagesize = params.m_nWidth * params.m_nHeight;
	BGR888_t *hp = new BGR888_t[ imagesize ];

	m_flStartTime = Sys_FloatTime();
	m_pMaterial = g_pMaterialSystem->FindMaterial( "debug/debugempty", "app" );
	while ( g_pVGui->IsRunning() )
	{
		AppPumpMessages();
	
		pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
		pRenderContext->ClearBuffers( true, true );

		DrawStuff( hAVIMaterial );

		g_pVGui->RunFrame();

		g_pVGuiSurface->PaintTraverse( root );

		// Get Bits from material system
		Rect_t rect;
		rect.x = rect.y = 0;
		rect.width = params.m_nWidth;
		rect.height = params.m_nHeight;

		pRenderContext->ReadPixelsAndStretch( &rect, &rect, (unsigned char*)hp, 
			IMAGE_FORMAT_BGR888, rect.width * sizeof( BGR888_t ) );
		g_pAVI->AppendMovieFrame( h, hp );

		g_pMaterialSystem->SwapBuffers();
	}

	delete[] hp;
	g_pAVI->FinishAVI( h );
	g_pAVI->DestroyAVIMaterial( hAVIMaterial );

//	delete mainPanel;

	return 1;
}



