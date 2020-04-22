//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "mdmpRipper.h"
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/MenuButton.h"
#include "CMDModulePanel.h"
#include "CMDErrorPanel.h"


using namespace vgui;

//-----------------------------------------------------------------------------
// Test panel
//-----------------------------------------------------------------------------
class CVGuiTestPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CVGuiTestPanel, vgui::Panel );

public:
	CVGuiTestPanel( vgui::Panel *pParent, const char *pName );

	virtual void PerformLayout();

private:
	MESSAGE_FUNC( OnOpen, "Open" );
	MESSAGE_FUNC( OnError, "Error" );
	MESSAGE_FUNC_PARAMS( OnCompare, "compare", data );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );		
	
	void CVGuiTestPanel::MiniDumpCompare( CUtlVector<HANDLE> *pMiniDumpHandles );

	vgui::MenuBar *m_pMenuBar;
	vgui::Panel *m_pClientArea;

//	void OnFileSelected( const char * filename );	
};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
vgui::Panel *CreateVGuiTestPanel( const char *pName )
{
	CVGuiTestPanel *pVGuiTestPanel = new CVGuiTestPanel( NULL, pName );
//	pVGuiTestPanel->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );
	return pVGuiTestPanel;
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVGuiTestPanel::CVGuiTestPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( NULL, pName )
{
	
	// Create the menu bar
	m_pMenuBar = new vgui::MenuBar( this, "Main Menu Bar" );
	m_pMenuBar->SetSize( 10, 28 );

	// Create a test menu
 	Menu *pFileMenu = new Menu(NULL, "File");
	pFileMenu->AddMenuItem( "&Open", new KeyValues( "Open" ), this );
	m_pMenuBar->AddMenu( "&File", pFileMenu );

    Menu *pErrorMenu = new Menu(NULL, "Error");
	pErrorMenu->AddMenuItem( "&Error", new KeyValues("Error"), this);
	m_pMenuBar->AddMenu( "&Error", pErrorMenu );

	MenuButton *pCloseButton = new vgui::MenuButton( this, "Close", "X" );
	m_pMenuBar->AddButton( pCloseButton );

	// Area below the menu bar
	m_pClientArea = new vgui::Panel( this, "VGuiTest Client Area ");
}


//-----------------------------------------------------------------------------
// Test menu button
//-----------------------------------------------------------------------------
void CVGuiTestPanel::OnOpen()
{
	FileOpenDialog *pFileDialog = new FileOpenDialog ( this, "File Open", true);
	pFileDialog->AddActionSignalTarget(this);
	pFileDialog->AddFilter( "*.mdmp", "MiniDumps", true );
	pFileDialog->DoModal( false );
}

void CVGuiTestPanel::OnError()
{
	CMDErrorPanel *pPanel = new CMDErrorPanel( this, "MDError Panel" );
	pPanel->Create();
	pPanel->AddActionSignalTarget( this );
	pPanel->DoModal();	
}


//-----------------------------------------------------------------------------
// The editor panel should always fill the space...
//-----------------------------------------------------------------------------
void CVGuiTestPanel::PerformLayout()
{
	// Make the editor panel fill the space
	int iWidth, iHeight;

	vgui::VPANEL parent = GetParent() ? GetParent()->GetVPanel() : vgui::surface()->GetEmbeddedPanel(); 
	vgui::ipanel()->GetSize( parent, iWidth, iHeight );
	SetSize( iWidth, iHeight );
	m_pMenuBar->SetSize( iWidth, 28 );

	// Make the client area also fill the space not used by the menu bar
	int iTemp, iMenuHeight;
	m_pMenuBar->GetSize( iTemp, iMenuHeight );
	m_pClientArea->SetPos( 0, iMenuHeight );
	m_pClientArea->SetSize( iWidth, iHeight - iMenuHeight );
}

void CVGuiTestPanel::OnCompare( KeyValues *data )
{
	int test = data->GetInt( "handlePointer" );
	CUtlVector<HANDLE> *pMiniDumpHandles = (CUtlVector<HANDLE> *)(void *)test;
	CUtlVector<CMiniDumpObject *> miniDumps;

	for( int i = 0; i < pMiniDumpHandles->Count(); i++ )
	{
		miniDumps.AddToTail( new CMiniDumpObject( pMiniDumpHandles->Element( i ) ) );
	}
	CMDModulePanel *pPanel = new CMDModulePanel( this, "MDModule Panel" );
	pPanel->Create( &miniDumps );
	pPanel->DoModal();
	miniDumps.RemoveAll();
	pMiniDumpHandles->RemoveAll();
}

void CVGuiTestPanel::OnFileSelected( const char *filename ) 
{
	CMDModulePanel *pPanel = new CMDModulePanel( this, "MDModule Panel" );
	pPanel->Create( filename );
	pPanel->DoModal();
}

