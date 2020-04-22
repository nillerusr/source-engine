//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "hud.h"
#include "clientmode_tfc.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "buymenu.h"
#include "filesystem.h"
#include "vgui/IVGui.h"
#include "hud_chat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include <KeyValues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"


ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );

IClientMode *g_pClientMode = NULL;


// --------------------------------------------------------------------------------- //
// CTFCModeManager.
// --------------------------------------------------------------------------------- //

class CTFCModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CTFCModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;


// --------------------------------------------------------------------------------- //
// CTFCModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CTFCModeManager::Init()
{
	g_pClientMode = GetClientModeNormal();
	
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

void CTFCModeManager::LevelInit( const char *newmap )
{
	g_pClientMode->LevelInit( newmap );
}

void CTFCModeManager::LevelShutdown( void )
{
	g_pClientMode->LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeTFCNormal::ClientModeTFCNormal()
{
}

//-----------------------------------------------------------------------------
// Purpose: If you don't know what a destructor is by now, you are probably going to get fired
//-----------------------------------------------------------------------------
ClientModeTFCNormal::~ClientModeTFCNormal()
{
}


void ClientModeTFCNormal::InitViewport()
{
	m_pViewport = new TFCViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

ClientModeTFCNormal g_ClientModeNormal;

IClientMode *GetClientModeNormal()
{
	return &g_ClientModeNormal;
}


ClientModeTFCNormal* GetClientModeTFCNormal()
{
	Assert( dynamic_cast< ClientModeTFCNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModeTFCNormal* >( GetClientModeNormal() );
}

float g_flViewModelFOV = 90;
float ClientModeTFCNormal::GetViewModelFOV( void )
{
	return g_flViewModelFOV;
}

int ClientModeTFCNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

void ClientModeTFCNormal::FireGameEvent( KeyValues * event)
{
	BaseClass::FireGameEvent( event );
}


void ClientModeTFCNormal::PostRenderVGui()
{
}




