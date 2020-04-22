//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The tool UI has 4 purposes:
//		1) Create the menu bar and client area (lies under the menu bar)
//		2) Forward all mouse messages to the tool workspace so action menus work
//		3) Forward all commands to the tool system so all smarts can reside there
//		4) Control the size of the menu bar + the working area
//=============================================================================

#include "ToolUI.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/basetoolsystem.h"
#include "vgui/Cursor.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "toolutils/toolmenubar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MENU_HEIGHT 28
// Height of the status bar, if the tool installs one
#define STATUS_HEIGHT 24
//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolUI::CToolUI( vgui::Panel *pParent, const char *panelName, CBaseToolSystem *pBaseToolSystem ) :
	BaseClass( pParent, panelName ), m_pClientArea( 0 ), m_pBaseToolSystem( pBaseToolSystem )
{
	SetPaintEnabled(false);
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);

	int w, h;
	pParent->GetSize( w, h );
	SetBounds( 0, 0, w, h );

	m_pMenuBar = m_pBaseToolSystem->CreateMenuBar( m_pBaseToolSystem );
	m_pMenuBar->SetParent( this );
	m_pMenuBar->SetSize( w, MENU_HEIGHT );
	// This can be NULL if no status bar should be included
	m_pStatusBar = m_pBaseToolSystem->CreateStatusBar( this );
	m_pStatusBar->SetParent( this );

	m_pClientArea = new vgui::Panel( this, "ClientArea" );
	m_pClientArea->SetMouseInputEnabled( false );
	m_pClientArea->SetCursor( vgui::dc_none );
	m_pClientArea->SetBounds( 0, MENU_HEIGHT, w, h - MENU_HEIGHT );
}

vgui::Panel *CToolUI::GetClientArea()
{
	return m_pClientArea;
}


//-----------------------------------------------------------------------------
// The tool UI panel should always fill the space...
//-----------------------------------------------------------------------------
void CToolUI::PerformLayout()
{
	BaseClass::PerformLayout();

	// Make the editor panel fill the space
	int iWidth, iHeight;
	vgui::VPANEL parent = GetParent() ? GetParent()->GetVPanel() : vgui::surface()->GetEmbeddedPanel(); 
	vgui::ipanel()->GetSize( parent, iWidth, iHeight );
	SetSize( iWidth, iHeight );
	int insettop = MENU_HEIGHT;
	int insetbottom = 0;
	m_pMenuBar->SetSize( iWidth, insettop );
	if ( m_pStatusBar )
	{
		insetbottom = STATUS_HEIGHT;
		m_pStatusBar->SetBounds( 0, iHeight - insetbottom, iWidth, insetbottom );
	}
	m_pClientArea->SetBounds( 0, insettop, iWidth, iHeight - insettop - insetbottom );
}


//-----------------------------------------------------------------------------
// Returns the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CToolUI::GetMenuBar()
{
	return m_pMenuBar;
}

vgui::Panel *CToolUI::GetStatusBar()
{
	return m_pStatusBar;
}
	
//-----------------------------------------------------------------------------
// Forward commands to systems that have more smarts than I do!
//-----------------------------------------------------------------------------
void CToolUI::OnMousePressed( vgui::MouseCode code )
{
	// Chain mouse pressed calls to the parent tool workspace
	CallParentFunction( new KeyValues( "MousePressed", "code", code ) );
}

void CToolUI::OnCommand( const char *cmd )
{
	m_pBaseToolSystem->OnCommand( cmd );
}

void CToolUI::UpdateMenuBarTitle()
{
	CToolFileMenuBar *mb = dynamic_cast< CToolFileMenuBar * >( GetMenuBar() );
	if ( mb )
	{
		char title[ 64 ];
		m_pBaseToolSystem->ComputeMenuBarTitle( title, sizeof( title ) );
		mb->SetInfo( title );
	}
}
