#include "kv_editor.h"
#include "kv_editor_frame.h"
#include "kv_editor_base_panel.h"
#include "kv_fit_children_panel.h"
#include "ScrollingWindow.h"
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/MenuBar.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include "vgui/ISurface.h"
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

extern char	g_gamedir[1024];

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Editor_Frame::CKV_Editor_Frame( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "tilegen_scheme.res", NULL );
	SetScheme( scheme );
	
	m_pEditor = new CKV_Editor( this, "KVEditor" );
	m_pEditor->SetFileFilter( "*.txt", "KeyValues files (*.txt)" );
	
	m_szLastFileName[0] = '\0';
	
	SetSize( 384, 420 );
	SetMinimumSize( 384, 420 );
	
	// setup the menu
	m_pMenuBar = new MenuBar( this, "KV_Editor_MenuBar" );
	MenuButton *pFileMenuButton = new MenuButton( this, "KV_Editor_FileMenuButton", "&File" );
	m_pFileMenu = new Menu( pFileMenuButton, "KV_Editor_FileMenu" );	
	//m_pFileMenu->AddMenuItem( "&New",  "NewKeys", this );
	m_pFileMenu->AddMenuItem( "&Open...",  "OpenKeys", this );
	m_pFileMenu->AddMenuItem( "&Save",  "SaveKeys", this );
	m_pFileMenu->AddMenuItem( "&Save As...",  "SaveKeysAs", this );
	pFileMenuButton->SetMenu( m_pFileMenu );
	m_pMenuBar->AddButton( pFileMenuButton );

	// disable keyboard input on the menu, otherwise once you click there, this dialog never gets key input again (is there a better way to do this and maintain keyboard shortcuts for the menu?)
	pFileMenuButton->SetKeyBoardInputEnabled( false );
	m_pFileMenu->SetKeyBoardInputEnabled( false );
	
	SetSizeable( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetMenuButtonVisible( false );
	m_bFirstPerformLayout = true;

	
	SetTitle( "KeyValues Editor", true );

	// temp for mission editor
	//Q_snprintf( m_szFileDirectory, sizeof( m_szFileDirectory ), "infested/tilegen/missions" );
	char keys_dir[1024];
	Q_snprintf( keys_dir, sizeof( keys_dir ), "%s\\tilegen\\missions", g_gamedir );
	m_pEditor->SetFileDirectory( keys_dir );
	m_pEditor->SetFileSpec( "tilegen/mission_editor_spec.txt", "GAME" );

}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CKV_Editor_Frame::~CKV_Editor_Frame()
{
	
}

//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CKV_Editor_Frame::OnClose( void )
{
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CKV_Editor_Frame::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "OpenKeys" ) == 0 )
	{
		DoFileOpen();
		return;
	}
	else if ( Q_stricmp( command, "SaveKeys" ) == 0 )
	{
		if ( Q_strlen( m_szLastFileName ) <= 0 )
		{
			DoFileSaveAs();
		}
		else if ( m_pEditor->GetKeys() )
		{
			m_pEditor->GetKeys()->SaveToFile( g_pFullFileSystem, m_szLastFileName, "GAME" );
		}
		return;
	}
	else if ( Q_stricmp( command, "SaveKeysAs" ) == 0 )
	{
		DoFileSaveAs();
		return;
	}
}

void CKV_Editor_Frame::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_bFirstPerformLayout )
	{
		int screenWide, screenTall;
		surface()->GetScreenSize(screenWide, screenTall);

		m_bFirstPerformLayout = false;
		int inset_x = screenWide * 0.15f;
		int inset_y = screenWide * 0.04f;
		SetBounds( inset_x, inset_y, screenWide - inset_x * 3, screenTall - inset_y * 2 );
	}

	m_pMenuBar->SetBounds( 20, 35, GetWide() - 40, 25 );
	m_pEditor->SetBounds( 20, 65, GetWide() - 40, GetTall() - 85 );
}

void CKV_Editor_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CKV_Editor_Frame::DoFileOpen()
{
	m_FileSelectType = KV_FST_LAYOUT_OPEN;
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog( NULL, "Open....", true );
	
	pFileDialog->SetStartDirectory( m_pEditor->GetFileDirectory() );
	pFileDialog->AddFilter( m_pEditor->GetFileFilter(), m_pEditor->GetFileFilterName(), true );
	pFileDialog->AddActionSignalTarget( this );

	pFileDialog->DoModal( false );
}

void CKV_Editor_Frame::DoFileSaveAs()
{
	m_FileSelectType = KV_FST_LAYOUT_SAVE_AS;
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog (NULL, "Save Map Layout", false);
	pFileDialog->SetStartDirectory( m_pEditor->GetFileDirectory() );
	pFileDialog->AddFilter( m_pEditor->GetFileFilter(), m_pEditor->GetFileFilterName(), true );
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(true);
}

void CKV_Editor_Frame::OnFileSelected( const char *fullpath )
{
	if ( m_FileSelectType == KV_FST_LAYOUT_SAVE_AS )
	{
        if ( m_pEditor->GetKeys() )
		{
            Q_snprintf( m_szLastFileName, sizeof( m_szLastFileName ), "%s", fullpath );
            m_pEditor->GetKeys()->SaveToFile( g_pFullFileSystem, fullpath, "GAME" );
        }
	}
	else if ( m_FileSelectType == KV_FST_LAYOUT_OPEN )
	{
        if ( m_pEditor->GetKeys() )
		{
            m_pEditor->GetKeys()->deleteThis();
		}
            
        KeyValues *pKeys = new KeyValues( "Keys" );
        // load in the new one
        if ( !pKeys->LoadFromFile( g_pFullFileSystem, fullpath ) )
        {
            MessageBox *pMessage = new MessageBox ("Error", "Error loading KeyValues", this);
            pMessage->DoModal();
            return;
        }
        m_pEditor->SetKeys( pKeys );
	}
}

MessageMapItem_t CKV_Editor_Frame::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR(CKV_Editor_Frame, "FileSelected", OnFileSelected, "fullpath"), 
};

IMPLEMENT_PANELMAP(CKV_Editor_Frame, Frame);