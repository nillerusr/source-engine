//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include "vgui_bitmapbutton.h"
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include "C_BaseTFPlayer.h"
#include "clientmode_tfbase.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Slider.h>
#include "vgui_rotation_slider.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define DISMANTLE_WAIT_TIME 5.0


//-----------------------------------------------------------------------------
// Standard VGUI panel for objects 
//-----------------------------------------------------------------------------
DECLARE_VGUI_SCREEN_FACTORY( CObjectControlPanel, "object_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CObjectControlPanel::CObjectControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName, g_hVGuiObjectScheme ) 
{
	// Make some high-level panels to group stuff we want to activate/deactivate
	m_pActivePanel = new CCommandChainingPanel( this, "ActivePanel" );
	m_pDeterioratingPanel = new CCommandChainingPanel( this, "DeterioratingPanel" );
	m_pDismantlingPanel = new CCommandChainingPanel( this, "DismantlingPanel" );

	SetCursor( vgui::dc_none ); // don't draw a VGUI cursor for this panel, and for its children

	// Make sure these are behind everything
	m_pActivePanel->SetZPos( -1 );
	m_pDeterioratingPanel->SetZPos( -1 );
	m_pDismantlingPanel->SetZPos( -1 );
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CObjectControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Grab ahold of certain well-known controls
	m_pHealthLabel = new vgui::Label( this, "HealthReadout", "" );
	m_pOwnerLabel = new vgui::Label( this, "OwnerReadout", "" );
	m_pDismantleButton = new CBitmapButton( this, "DismantleButton", "Dismantle" );
	m_pAssumeControlButton = new CBitmapButton( GetDeterioratingPanel(), "AssumeControl", "" );
	m_pDismantleTimeLabel = new vgui::Label( GetDismantlingPanel(), "DismantleTime", "" );

	m_flDismantleTime = -1;

	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	SetCursor( vgui::dc_none ); // don't draw a VGUI cursor for this panel, and for its children

	// Make the bounds of the sub-panels match
	int x, y, w, h;
	GetBounds( x, y, w, h );
	m_pActivePanel->SetBounds( x, y, w, h );
	m_pDeterioratingPanel->SetBounds( x, y, w, h );
	m_pDismantlingPanel->SetBounds( x, y, w, h );

	// Make em all invisible
	m_pActivePanel->SetVisible( false );
	m_pDeterioratingPanel->SetVisible( false );
	m_pDismantlingPanel->SetVisible( false );
	m_pCurrentPanel = m_pActivePanel;

	return true;
}


//-----------------------------------------------------------------------------
// Returns the object it's attached to 
//-----------------------------------------------------------------------------
C_BaseObject *CObjectControlPanel::GetOwningObject() const
{
	C_BaseEntity *pScreenEnt = GetEntity();
	if (!pScreenEnt)
		return NULL;

	C_BaseEntity *pObj = pScreenEnt->GetOwnerEntity();
	if (!pObj)
		return NULL;

	Assert( dynamic_cast<C_BaseObject*>(pObj) );
	return static_cast<C_BaseObject*>(pObj);
}


//-----------------------------------------------------------------------------
// Ticks the panel when its in its various states
//-----------------------------------------------------------------------------

void CObjectControlPanel::OnTickDeteriorating( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer )
{
	char buf[256];
	if ( pLocalPlayer && ClassCanBuild( pLocalPlayer->PlayerClass(), pObj->GetType() ) )
	{
		int nCost = CalculateObjectCost( pObj->GetType(), pLocalPlayer->GetNumObjects( pObj->GetType() ), pLocalPlayer->GetTeamNumber() );
		Q_snprintf( buf, sizeof( buf ), "Buy for %d", nCost );
		m_pAssumeControlButton->SetText( buf );
		m_pAssumeControlButton->SetVisible( true );

		bool bHasEnoughResources = pLocalPlayer->GetBankResources() >= nCost;
		m_pAssumeControlButton->SetEnabled( bHasEnoughResources ); 
	}
	else
	{
		m_pAssumeControlButton->SetVisible( false );
	}

	ShowDismantleButton( false );
}

void CObjectControlPanel::OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer )
{
	ShowDismantleButton( !(pObj->GetFlags() & OF_CANNOT_BE_DISMANTLED) && pObj->GetOwner() == pLocalPlayer );
}

void CObjectControlPanel::OnTickDismantling( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer )
{
	ShowDismantleButton( false );
	if ( !m_bDismantled && (gpGlobals->curtime >= m_flDismantleTime))
	{
		Dismantle();
		m_bDismantled = true;
	}

	int nSec = (int)(m_flDismantleTime - gpGlobals->curtime + 0.5f);
	if (nSec < 0)
		nSec = 0;

	char buf[256];
	int nLen = Q_snprintf( buf, sizeof( buf ), "%d second", nSec );
	if (nSec != 1)
	{
		buf[nLen] = 's';
		++nLen;
		buf[nLen] = 0;
	}

	m_pDismantleTimeLabel->SetText( buf );
}


vgui::Panel* CObjectControlPanel::TickCurrentPanel()
{
	C_BaseTFPlayer *pLocalPlayer = C_BaseTFPlayer::GetLocalPlayer();
	C_BaseObject *pObj = GetOwningObject();

	if (IsDismantling())
	{
		m_pCurrentPanel = GetDismantlingPanel();

		OnTickDismantling(pObj, pLocalPlayer);
	}
	else if (pObj->IsDeteriorating())
	{
		m_pCurrentPanel = GetDeterioratingPanel();

		OnTickDeteriorating(pObj, pLocalPlayer);
	}
	else
	{
		m_pCurrentPanel = GetActivePanel();

		OnTickActive(pObj, pLocalPlayer);
	}
	
	return m_pCurrentPanel;
}


void CObjectControlPanel::ShowDismantleButton( bool bShow )
{
	m_pDismantleButton->SetVisible( bShow );
}


void CObjectControlPanel::ShowOwnerLabel( bool bShow )
{
	m_pOwnerLabel->SetVisible( bShow );
}


void CObjectControlPanel::ShowHealthLabel( bool bShow )
{
	m_pHealthLabel->SetVisible( bShow );
}


void CObjectControlPanel::SendToServerObject( const char *pMsg )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		pObj->SendClientCommand( pMsg );
	}
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CObjectControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	char buf[256];
	Q_snprintf( buf, sizeof( buf ), "Health: %d%%", (int)(pObj->HealthFraction() * 100.0f) );
	m_pHealthLabel->SetText( buf );

	C_BaseTFPlayer *pPlayer = pObj->GetOwner();
	if (pPlayer)
	{
		Q_snprintf( buf, sizeof( buf ), "Owner: %s", pPlayer->GetPlayerName() );
	}
	else
	{
		Q_snprintf( buf, sizeof( buf ), "No Owner" );
	}

	m_pOwnerLabel->SetText( buf );

	// Update the current subpanel
	m_pCurrentPanel->SetVisible( false );
	
	m_pCurrentPanel = TickCurrentPanel();

	m_pCurrentPanel->SetVisible( true );
}


//-----------------------------------------------------------------------------
// Dismantles the object 
//-----------------------------------------------------------------------------
void CObjectControlPanel::Dismantle()
{
	SendToServerObject( "dismantle" );
}


//-----------------------------------------------------------------------------
// Starts/stops dismantling
//-----------------------------------------------------------------------------
void CObjectControlPanel::StartDismantling()
{
	m_flDismantleTime = gpGlobals->curtime + DISMANTLE_WAIT_TIME;
	m_bDismantled = false;
}

void CObjectControlPanel::StopDismantling()
{
	m_flDismantleTime = -1.0f;
}

bool CObjectControlPanel::IsDismantling() const
{
	return m_flDismantleTime >= 0.0f;
}


//-----------------------------------------------------------------------------
// Assumes control of the object 
//-----------------------------------------------------------------------------
void CObjectControlPanel::AssumeControl()
{
	SendToServerObject( "takecontrol" );
}


//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CObjectControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Dismantle", 10))
	{
		StartDismantling();
		return;
	}

	if (!Q_strnicmp(command, "CancelDismantle", 20))
	{
		StopDismantling();
		return;
	}

	if (!Q_strnicmp(command, "AssumeControl", 15))
	{
		AssumeControl();
		return;
	}

	BaseClass::OnCommand(command);
}


DECLARE_VGUI_SCREEN_FACTORY( CRotatingObjectControlPanel, "rotating_object_control_panel" );


//-----------------------------------------------------------------------------
// This is a panel for an object that has rotational controls 
//-----------------------------------------------------------------------------
CRotatingObjectControlPanel::CRotatingObjectControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName ) 
{
}

bool CRotatingObjectControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Grab ahold of certain well-known controls
	m_pRotationSlider = new CRotationSlider( GetActivePanel(), "RotationSlider" );
	m_pRotationLabel = new vgui::Label( GetActivePanel(), "RotationLabel", "Rotation Control" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	m_pRotationSlider->SetControlledObject( GetOwningObject() );

	return true;
}

void CRotatingObjectControlPanel::OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer )
{
	BaseClass::OnTickActive( pObj, pLocalPlayer );
	bool bEnable = (pObj->GetOwner() == pLocalPlayer);
	m_pRotationSlider->SetVisible( bEnable );
	m_pRotationLabel->SetVisible( bEnable );
}

