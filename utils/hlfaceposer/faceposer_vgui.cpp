//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "faceposer_vgui.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "vgui/IInput.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include <matsys_controls/matsyscontrols.h>
#include <dme_controls/dmecontrols.h>
//#include "material.h"
#include "vgui_controls/AnimationController.h"
#include "inputsystem/iinputsystem.h"
#include "VGuiWnd.h"

extern CreateInterfaceFn g_Factory;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CFacePoserVGui s_FaceposerVGui;

CFacePoserVGui *FaceposerVGui()
{
	return &s_FaceposerVGui;
}

CFacePoserVGui::CFacePoserVGui(void)
{
	m_pActiveWindow = NULL;
	m_hMainWindow = NULL;
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
bool CFacePoserVGui::Init( HWND hWindow )
{
	// initialize vgui_control interfaces
	if (!vgui::VGui_InitInterfacesList( "FACEPOSER", &g_Factory, 1 ))
		return false;

//	if ( !vgui::VGui_InitMatSysInterfacesList( "FACEPOSER", &g_Factory, 1 ) )
//		return false;

	// All of the various tools .dlls expose GetVGuiControlsModuleName() to us to make sure we don't have communication across .dlls
//	if ( !vgui::VGui_InitDmeInterfacesList( "FACEPOSER", &g_Factory, 1 ) )
//		return false;

	if ( !g_pMatSystemSurface )
		return false;

	// configuration settings
	vgui::system()->SetUserConfigFile("faceposer.vdf", "EXECUTABLE_PATH");

	// Are we trapping input?
	g_pMatSystemSurface->EnableWindowsMessages( true );

	// Need to be able to play sounds through vgui
	// g_pMatSystemSurface->InstallPlaySoundFunc( VGui_PlaySound );

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile("Resource/SourceScheme.res", "FacePoser"))
	{
		return false;
	}

	m_hMainWindow = hWindow;

	// Start the App running
	vgui::ivgui()->Start();
	vgui::ivgui()->SetSleep(false);

	return true;
}

void CFacePoserVGui::SetFocus( CVGuiWnd *pVGuiWnd )
{
	if ( pVGuiWnd == m_pActiveWindow )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	g_pMatSystemSurface->AttachToWindow( NULL, false );
	g_pInputSystem->DetachFromWindow( );

	if ( pVGuiWnd )
	{
		HWND hWnd = (HWND)pVGuiWnd->GetParentWnd()->getHandle();

		m_pActiveWindow = pVGuiWnd;
		g_pInputSystem->AttachToWindow( hWnd );
		g_pMatSystemSurface->AttachToWindow( hWnd, false );
		vgui::ivgui()->ActivateContext( pVGuiWnd->GetVGuiContext() );
	}
	else
	{
		m_pActiveWindow = NULL;
		vgui::ivgui()->ActivateContext( vgui::DEFAULT_VGUI_CONTEXT );
	}
}

bool CFacePoserVGui::HasFocus( CVGuiWnd *pWnd )
{
	return m_pActiveWindow == pWnd;
}

void CFacePoserVGui::Simulate()
{
	// VPROF( "CFacePoserVGui::Simulate" );

	if ( !IsInitialized() )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( vgui::system()->GetCurrentTime() );
}

void CFacePoserVGui::Shutdown()
{
	// Give panels a chance to settle so things
	//  Marked for deletion will actually get deleted

	if ( !IsInitialized() )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame();

	// stop the App running
	vgui::ivgui()->Stop();
}

CFacePoserVGui::~CFacePoserVGui(void)
{
}
