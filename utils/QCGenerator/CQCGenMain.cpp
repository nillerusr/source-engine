//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog for selecting game configurations
//
//=====================================================================================//

#include <windows.h>

#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui_controls/MessageBox.h>

#include "matsys_controls/QCGenerator.h"
#include "CQCGenMain.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CQCGenMain *g_pCQCGenMain = NULL;

class CModalPreserveMessageBox : public vgui::MessageBox
{
public:
	CModalPreserveMessageBox(const char *title, const char *text, vgui::Panel *parent)
		: vgui::MessageBox( title, text, parent )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalPreserveMessageBox()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}

public:
	vgui::VPANEL	m_PrevAppFocusPanel;
};
		
//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CQCGenMain::CQCGenMain( Panel *parent, const char *pszPath, const char *pszScene , const char *name ) : BaseClass( parent, name ), m_bChanged( false )
{
	Assert( !g_pCQCGenMain );
	g_pCQCGenMain = this;
	
	SetMinimizeButtonVisible( true );	

	SetSize( 846, 770 );
	SetMinimumSize(846, 770);	
	char szTitle[MAX_PATH];
	strcpy( szTitle, pszPath );
	strcat( szTitle, "\\" );
	strcat( szTitle, pszScene );
	SetTitle( szTitle, true );

    m_pQCGenerator = new CQCGenerator( this, pszPath, pszScene );
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CQCGenMain::~CQCGenMain()
{
	g_pCQCGenMain = NULL;
}



//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CQCGenMain::OnClose( void )
{
	BaseClass::OnClose();
	ivgui()->Stop();	
}

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CQCGenMain::OnCommand( const char *command )
{		
	BaseClass::OnCommand( command );
}

void CQCGenMain::OnRefresh()
{
	Repaint();
}

