//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "toolutils/vgui_tools.h"
#include "ienginevgui.h"
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "tier0/vprof.h"
#include <vgui_controls/Panel.h>
#include <KeyValues.h>
#include <dme_controls/dmeControls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : appSystemFactory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_Startup( CreateInterfaceFn appSystemFactory )
{
	// All of the various tools .dlls expose GetVGuiControlsModuleName() to us to make sure we don't have communication across .dlls
	if ( !vgui::VGui_InitDmeInterfacesList( GetVGuiControlsModuleName(), &appSystemFactory, 1 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_PostInit()
{
	// Create any root panels for .dll
	VGUI_CreateToolRootPanel();

	// Make sure we have a panel
	VPANEL root = VGui_GetToolRootPanel();
	if ( !root )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void VGui_Shutdown()
{
	VGUI_DestroyToolRootPanel();

	// Make sure anything "marked for deletion"
	//  actually gets deleted before this dll goes away
	vgui::ivgui()->RunFrame();
}
