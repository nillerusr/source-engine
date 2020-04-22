//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/toolmenubar.h"
#include "vgui_controls/Label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Version that only has tool name and info
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolMenuBar::CToolMenuBar( CBaseToolSystem *pParent, const char *pPanelName ) :
	BaseClass( (Panel *)pParent, pPanelName ),
	m_pToolSystem( pParent )
{
	m_pInfo = new Label( this, "Info", "" );
	m_pToolName = new Label( this, "ToolName", "" );
}

CBaseToolSystem *CToolMenuBar::GetToolSystem()
{
	return m_pToolSystem;
}

//-----------------------------------------------------------------------------
// Sets the tool bar's name
//-----------------------------------------------------------------------------
void CToolMenuBar::SetToolName( const char *pName )
{
	m_pToolName->SetText( pName );
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Sets the tool bar info
//-----------------------------------------------------------------------------
void CToolMenuBar::SetInfo( const char *pInfo )
{
	m_pInfo->SetText( pInfo );
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Lays out the menu bar 
//-----------------------------------------------------------------------------
void CToolMenuBar::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int cw, ch;
	m_pInfo->GetContentSize( cw, ch );

	int right = w - cw - 20;
	m_pInfo->SetBounds( right, 0, cw, h );

	m_pToolName->GetContentSize( cw, ch );
	m_pToolName->SetBounds( right - cw - 5, 0, cw, h );
}


//-----------------------------------------------------------------------------
//
// Version that only has tool name, info, and file name
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolFileMenuBar::CToolFileMenuBar( CBaseToolSystem *parent, const char *panelName ) :
	BaseClass( parent, panelName )
{
	m_pFileName = new Label( this, "FileName", "" );
}


void CToolFileMenuBar::SetFileName( char const *name )
{
	m_pFileName->SetText( name );
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Performs layout
//-----------------------------------------------------------------------------
void CToolFileMenuBar::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int cw, ch;
	m_pInfo->GetContentSize( cw, ch );

	int right = w - cw - 20;

	m_pToolName->GetContentSize( cw, ch );

	int barx, bary;
	GetContentSize( barx, bary );

	int faredge = right - cw - 5- 2;
	int nearedge = barx + 2;

	int mid = ( nearedge + faredge ) * 0.5f;

	m_pFileName->GetContentSize( cw, ch );
	m_pFileName->SetBounds( mid - cw * 0.5f, 0, cw, h );
}
