//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include "hammer.h"
#include "hammervgui.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "vgui/IInput.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include <matsys_controls/matsyscontrols.h>
#include "material.h"
#include "vgui_controls/AnimationController.h"
#include "inputsystem/iinputsystem.h"
#include "VGuiWnd.h"

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CHammerVGui s_HammerVGui;

CHammerVGui *HammerVGui()
{
	return &s_HammerVGui;
}

CHammerVGui::CHammerVGui(void)
{
	m_pActiveWindow = NULL;
	m_hMainWindow = NULL;
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
bool CHammerVGui::Init( HWND hWindow )
{
	// initialize vgui_control interfaces
	if (!vgui::VGui_InitInterfacesList( "HAMMER", &g_Factory, 1 ))
		return false;
	
	if ( !vgui::VGui_InitMatSysInterfacesList( "HAMMER", &g_Factory, 1 ) )
		return false;

	if ( !g_pMatSystemSurface )
		return false;
	
	// configuration settings
	vgui::system()->SetUserConfigFile("hammer.vdf", "EXECUTABLE_PATH");

	// Are we trapping input?
	g_pMatSystemSurface->EnableWindowsMessages( true );

	// Need to be able to play sounds through vgui
	// g_pMatSystemSurface->InstallPlaySoundFunc( VGui_PlaySound );

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile("Resource/SourceScheme.res", "Hammer"))
	{
		return false;
	}

	m_hMainWindow = hWindow;

	// Start the App running
	vgui::ivgui()->Start();
	vgui::ivgui()->SetSleep(false);

	return true;
}

void CHammerVGui::SetFocus( CVGuiWnd *pVGuiWnd )
{
	if ( pVGuiWnd == m_pActiveWindow )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	g_pMatSystemSurface->AttachToWindow( NULL, false );
	g_pInputSystem->DetachFromWindow( );

	if ( pVGuiWnd )
	{
		m_pActiveWindow = pVGuiWnd;
		g_pInputSystem->AttachToWindow( pVGuiWnd->GetParentWnd()->GetSafeHwnd() );
		g_pMatSystemSurface->AttachToWindow( pVGuiWnd->GetParentWnd()->GetSafeHwnd(), false );
		vgui::ivgui()->ActivateContext( pVGuiWnd->GetVGuiContext() );
	}
	else
	{
		m_pActiveWindow = NULL;
		vgui::ivgui()->ActivateContext( vgui::DEFAULT_VGUI_CONTEXT );
	}
}

bool CHammerVGui::HasFocus( CVGuiWnd *pWnd )
{
	return m_pActiveWindow == pWnd;
}

void CHammerVGui::Simulate()
{
// VPROF( "CHammerVGui::Simulate" );

	if ( !IsInitialized() )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( vgui::system()->GetCurrentTime() );
}

void CHammerVGui::Shutdown()
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

CHammerVGui::~CHammerVGui(void)
{
}
