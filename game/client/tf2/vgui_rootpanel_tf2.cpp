//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "c_tf2rootpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_TF2RootPanel *g_pTF2RootPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	g_pTF2RootPanel = new C_TF2RootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyClientDLLRootPanel( void )
{
	delete g_pTF2RootPanel;
	g_pTF2RootPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	return g_pTF2RootPanel->GetVPanel();
}