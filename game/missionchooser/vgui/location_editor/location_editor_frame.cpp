#include "kv_editor.h"
#include "location_editor_frame.h"
#include "kv_editor_base_panel.h"
#include "kv_fit_children_panel.h"
#include "ScrollingWindow.h"
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include "vgui/ISurface.h"
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TreeView.h>
#include "location_layout_panel.h"
#include "asw_mission_chooser.h"
#include "asw_location_grid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

extern char	g_gamedir[1024];

namespace
{
	bool GroupTreeSortFunc( KeyValues *pItem1, KeyValues *pItem2 )
	{
		return Q_stricmp( pItem1->GetString( "Text" ), pItem2->GetString( "Text" ) ) < 0;
	}
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CLocation_Editor_Frame::CLocation_Editor_Frame( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	//vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "tilegen_scheme.res", NULL );
	//SetScheme( scheme );
		
	m_pGroupPage = new CGroup_Edit_Page( this, "GroupPage" );
	m_pGroupPage->m_pEditor->AddActionSignalTarget( this );
	m_pGroupPage->AddActionSignalTarget( this );

	m_pSaveButton = new vgui::Button( this, "SaveButton", "Save", this, "Save" );
	m_pScrollingWindow = new CScrollingWindow( this, "ScrollingWindow" );
	m_pLocationLayoutPanel = new CLocation_Layout_Panel( this, this, "LocationLayoutPanel" );

	m_pScrollingWindow->SetChildPanel( m_pLocationLayoutPanel );

	m_pTree = new vgui::TreeView( this, "Tree" );
	m_pTree->MakeReadyForUse();
	IScheme *pscheme = vgui::scheme()->GetIScheme( GetScheme() );
	HFont treeFont = pscheme->GetFont( "DefaultVerySmall" );
	m_pTree->SetFont( treeFont );
	m_pTree->SetSortFunc( GroupTreeSortFunc );	
	m_iRootIndex = -1;
	m_iCurrentLocationID = -1;

	UpdateTree();

	LoadControlSettings( "tilegen/LocationEditor.res", "GAME" );
	
	//SetSize( 384, 420 );
	SetMinimumSize( 384, 420 );
		
	SetSizeable( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetMenuButtonVisible( false );
	m_bFirstPerformLayout = true;
	
	SetTitle( "Location Grid Editor", true );
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CLocation_Editor_Frame::~CLocation_Editor_Frame()
{
	
}

//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CLocation_Editor_Frame::OnClose( void )
{
	BaseClass::OnClose();
}

void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... );

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CLocation_Editor_Frame::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if ( !Q_strnicmp( command, "Location", 8 ) )
	{
		int iLocationID = atoi( command + 8 );
		SetLocationID( iLocationID );
	}
	else if ( !Q_stricmp( command, "Save" ) )
	{
		if ( !LocationGrid()->SaveLocationGrid() )
		{
			VGUIMessageBox( this, "Save Error", "Failed to save %s.  Make sure file is checked out from Perforce.", "resource/mission_grid.txt" );
		}
	}
}

void CLocation_Editor_Frame::KeyValuesChanged( vgui::Panel *panel )
{
	if ( panel == m_pGroupPage->m_pEditor )
	{
		CASW_Location_Group *pGroup = LocationGrid()->GetCGroup( m_pGroupPage->m_iCurrentGroupIndex );
		if ( pGroup )
		{
			pGroup->LoadFromKeyValues( m_pGroupPage->m_pEditor->GetKeys() );
		}
	}
}

void CLocation_Editor_Frame::NewGroup( vgui::Panel *panel )
{
	LocationGrid()->CreateNewGroup();
	UpdateTree();
}

void CLocation_Editor_Frame::DeleteGroup( vgui::Panel *panel )
{
	LocationGrid()->DeleteGroup( m_pGroupPage->m_iCurrentGroupIndex );
	UpdateTree();
}

void CLocation_Editor_Frame::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_bFirstPerformLayout )
	{
		int screenWide, screenTall;
		surface()->GetScreenSize( screenWide, screenTall );

		m_bFirstPerformLayout = false;
		//int inset_x = screenWide * 0.15f;
		//int inset_y = screenWide * 0.04f;
		//SetBounds( inset_x, inset_y, screenWide - inset_x * 3, screenTall - inset_y * 2 );
	}
}

void CLocation_Editor_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CLocation_Editor_Frame::UpdateTree()
{
	m_pTree->RemoveAll();

	if ( !LocationGrid() )
		return;

	KeyValues *kv = new KeyValues( "TVI" );
	kv->SetString( "Text", "Groups" );
	m_iRootIndex = m_pTree->AddItem( kv, -1 );

	for ( int i = 0; i < LocationGrid()->GetNumGroups(); i++ )
	{
		CASW_Location_Group *pGroup = LocationGrid()->GetCGroup( i );
		if ( !pGroup )
			continue;
		KeyValues *pGroupEntry = new KeyValues("Group");
		pGroupEntry->SetString( "Text", pGroup->GetGroupName() );
		pGroupEntry->SetInt( "Index", i );
		pGroupEntry->SetString( "Type", "Group" );
		int iItem = m_pTree->AddItem( pGroupEntry, m_iRootIndex );
		m_pTree->SetItemFgColor( iItem, Color(255, 255, 255, 255) );
	}
	m_pTree->ExpandItem( m_iRootIndex, true );
}

void CLocation_Editor_Frame::OnTreeViewItemSelected( int itemIndex )
{
	KeyValues *pKV = m_pTree->GetItemData( itemIndex );
	if ( pKV && pKV->FindKey( "Text" ) )
	{
		//const char *pGroupName = pKV->GetString( "Text" );
		//CASW_Location_Group *pGroup = LocationGrid()->GetGroupByName( pGroupName );
		//if ( !pGroup )
		//return;

		if ( m_pTree->GetItemParent( itemIndex ) == m_iRootIndex && Q_stricmp( pKV->GetString( "Text" ), "Ungrouped" ) )
		{
			int index = pKV->GetInt( "Index", -1 );
			m_pGroupPage->m_iCurrentGroupIndex = index;

			SetGroup( LocationGrid()->GetCGroup( index ) );
		}
		else
		{
			int id = pKV->GetInt( "LocationID", -1 );
			SetLocationID( id );
		}
	}
}


void CLocation_Editor_Frame::SetLocationID( int iLocationID )
{
	if ( !LocationGrid() )
		return;

	m_iCurrentLocationID = iLocationID;

	// TODO: Find the group this location is in and show it, if we're not showing it already
	IASW_Location *pLoc = LocationGrid()->GetLocationByID( iLocationID );
	if ( !pLoc )
		return;

	IASW_Location_Group *pSelectedGroup = pLoc->GetGroup();
	if ( !pSelectedGroup )
		return;

	for ( int i = 0; i < LocationGrid()->GetNumGroups(); i++ )
	{
		if ( pSelectedGroup == LocationGrid()->GetCGroup( i ) )
		{
			m_pGroupPage->m_iCurrentGroupIndex = i;
			SetGroup( LocationGrid()->GetCGroup( i ) );
			break;
		}
	}
}

void CLocation_Editor_Frame::SetGroup( IASW_Location_Group *pGroup )
{
	if ( !pGroup )
		return;

	CASW_Location_Group *pCGroup = dynamic_cast<CASW_Location_Group*>( pGroup );
	if ( pCGroup )
	{
		m_pGroupPage->m_pEditor->SetKeys( pCGroup->GetKeyValuesForEditor() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shows kv editor for manipulating a specific group
//-----------------------------------------------------------------------------

CGroup_Edit_Page::CGroup_Edit_Page( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pEditor = new CKV_Editor( this, "KVEditor" );
	m_pEditor->SetFileSpec( "tilegen/location_group_editor_spec.txt", "GAME" );
	m_pNewGroupButton = new vgui::Button( this, "NewGroupButton", "New Group", this, "NewGroup" );	
	m_pDeleteGroupButton = new vgui::Button( this, "DeleteGroupButton", "Delete Group", this, "DeleteGroup" );	
	m_iCurrentGroupIndex = -1;
}

CGroup_Edit_Page::~CGroup_Edit_Page()
{

}

void CGroup_Edit_Page::PerformLayout()
{	
	m_pEditor->SetBounds( 5, 45, GetWide() - 10, GetTall() - 55 );
	m_pNewGroupButton->SetBounds( GetWide() - 110, 16, 100, 22 );
	m_pDeleteGroupButton->SetBounds( GetWide() - 215, 16, 100, 22 );
}

void CGroup_Edit_Page::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}

void CGroup_Edit_Page::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);
}